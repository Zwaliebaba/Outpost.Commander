#include "pch.h"

#include "Accumulator.h"

#include <algorithm>
#include <limits>

namespace Outpost
{

namespace
{
/// One live entity this tick, as a candidate for this client's updates.
struct Candidate
{
  std::uint16_t slot = 0;

  /// Past the sweep, or never sent. A due candidate goes ahead of every score.
  bool due = false;

  /// Ticks since this client was last sent it; the largest possible when never sent, so among due
  /// candidates the one this client has never heard about goes first.
  std::uint32_t age = 0;

  std::uint64_t score = 0;
  EntityRecord record{};
};

/// **A TOTAL ORDER, AND THE LAST KEY IS WHY.** Due before not due; the longest unsent first among the
/// due; the highest score first among the rest; and the slot last, so no two candidates ever compare
/// equal and `std::sort` has no arrangement to choose between. That is what makes the sent set the same
/// twice from the same state, which is what a suite pins.
[[nodiscard]] bool GoesFirst(const Candidate& _left, const Candidate& _right) noexcept
{
  if (_left.due != _right.due)
  {
    return _left.due;
  }
  if (_left.due && (_left.age != _right.age))
  {
    return _left.age > _right.age;
  }
  if (_left.score != _right.score)
  {
    return _left.score > _right.score;
  }
  return _left.slot < _right.slot;
}

/// Inside the view, in whole world units and integers throughout. Squared, so there is no root; the
/// operands are bounded by the play area, so 64 bits is far more than the squares need.
[[nodiscard]] bool InView(const Neuron::Vec2& _position, const Neuron::Vec2& _center, std::int64_t _radiusUnits) noexcept
{
  if (_radiusUnits <= 0)
  {
    return false;
  }
  const std::int64_t dx = (static_cast<std::int64_t>(_position.x) - static_cast<std::int64_t>(_center.x)) / Neuron::FIXED_ONE;
  const std::int64_t dy = (static_cast<std::int64_t>(_position.y) - static_cast<std::int64_t>(_center.y)) / Neuron::FIXED_ONE;
  return ((dx * dx) + (dy * dy)) <= (_radiusUnits * _radiusUnits);
}
} // namespace

EntityRecord RecordOf(const World& _world, std::size_t _slot) noexcept
{
  const Entity& entity = _world.EntityInSlot(_slot);
  const DerivedStats stats = Derive(entity.design);
  const std::uint8_t chips = CargoChips(_world.MineInSlot(_slot).cargoMilliOre, stats.oreCapacity * MILLI_ORE_PER_ORE);
  return EntityRecord{.identity = PackIdentity(entity.id.index, entity.id.generation),
                      .owner = entity.owner,
                      .positionX = QuantizePosition(entity.position.x),
                      .positionY = QuantizePosition(entity.position.y),
                      .heading = QuantizeWireHeading(entity.heading),
                      .hullPercentRemaining = QuantizeHullPercent(entity.hullRemaining, stats.hullPoints),
                      .designIdentity = static_cast<std::uint8_t>(entity.design),
                      .flags = WithCargoChips(0, chips)};
}

void Accumulator::Begin() noexcept
{
  m_clients.clear();
}

Accumulator::Client& Accumulator::ClientFor(PlayerId _player)
{
  for (Client& client : m_clients)
  {
    if (client.player == _player)
    {
      return client;
    }
  }
  m_clients.push_back(Client{.player = _player});
  return m_clients.back();
}

void Accumulator::Reset(PlayerId _player) noexcept
{
  for (Client& client : m_clients)
  {
    if (client.player == _player)
    {
      // Cleared, not resized: an empty tracking table is "never sent anything", which makes every live
      // entity due on the next fill.
      client.tracked.clear();
      client.removals.clear();
      client.fires.clear();
      return;
    }
  }
}

void Accumulator::SetView(PlayerId _player, std::int16_t _wireX, std::int16_t _wireY, std::uint16_t _radiusUnits) noexcept
{
  Client& client = ClientFor(_player);
  client.viewCenter = Neuron::Vec2{.x = DequantizePosition(_wireX), .y = DequantizePosition(_wireY)};
  client.viewRadiusUnits = static_cast<std::int64_t>(_radiusUnits);
}

void Accumulator::NoteFire(const FireEvent& _fire)
{
  for (Client& client : m_clients)
  {
    client.fires.push_back(PendingFire{.event = _fire, .remaining = FIRE_REPEAT_TICKS});
  }
}

std::vector<Update> Accumulator::Fill(const World& _world, PlayerId _player, const PlayerBlock& _own, std::uint32_t _tick)
{
  Client& client = ClientFor(_player);

  const std::size_t slotCount = _world.SlotCount();
  if (client.tracked.size() < slotCount)
  {
    client.tracked.resize(slotCount);
  }

  // === DEATHS BECOME REMOVALS. ======================================================================
  //
  // Anything this client was sent that is no longer what it was sent as: the slot is dead, or it holds
  // somebody new. Found by looking rather than by being told, so no system that destroys an entity has
  // to remember to report it -- and a slot that died and was refilled in one tick still yields a removal
  // for the old occupant and a first send for the new one.
  for (std::size_t slot = 0; slot < client.tracked.size(); ++slot)
  {
    Tracked& tracked = client.tracked[slot];
    if (!tracked.sent)
    {
      continue;
    }
    const bool stillThere =
      (slot < slotCount) && _world.IsSlotAlive(slot) && (RecordOf(_world, slot).identity == tracked.lastSent.identity);
    if (!stillThere)
    {
      client.removals.push_back(PendingRemoval{.identity = tracked.lastSent.identity, .remaining = REMOVAL_REPEAT_TICKS});
      tracked = Tracked{};
    }
  }

  // === SCORES, AND WHO IS DUE. ======================================================================
  const std::uint32_t sweep = SweepTicks(_world.AliveCount());
  std::vector<Candidate> candidates;
  candidates.reserve(_world.AliveCount());

  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    Tracked& tracked = client.tracked[slot];
    const EntityRecord record = RecordOf(_world, slot);

    std::uint64_t relevance = WEIGHT_BASE;
    if (InView(entity.position, client.viewCenter, client.viewRadiusUnits))
    {
      relevance += WEIGHT_IN_VIEW;
    }
    if (tracked.sent && !(record == tracked.lastSent))
    {
      relevance += WEIGHT_CHANGED;
    }
    if (entity.owner == _player)
    {
      relevance += WEIGHT_OWN;
    }
    tracked.score += relevance;

    const std::uint32_t age = tracked.sent ? (_tick - tracked.lastSentTick) : std::numeric_limits<std::uint32_t>::max();
    candidates.push_back(Candidate{.slot = static_cast<std::uint16_t>(slot),
                                   .due = (!tracked.sent) || (age >= sweep),
                                   .age = age,
                                   .score = tracked.score,
                                   .record = record});
  }

  std::sort(candidates.begin(), candidates.end(), GoesFirst);

  // === THE UPDATES, EACH OF THEM WHOLE. =============================================================
  std::vector<Update> updates;
  std::size_t next = 0;
  for (std::size_t index = 0; index < UPDATES_PER_TICK; ++index)
  {
    // AT LEAST ONE, AND A SECOND ONLY FOR RECORDS. The first carries the client's block and the tick
    // whatever else is due; a second with no records in it would be a datagram saying nothing new.
    if ((index > 0) && (next >= candidates.size()))
    {
      break;
    }

    Update update;
    update.sequence = client.sequence++;
    update.tick = _tick;
    update.liveEntityCount =
      static_cast<std::uint16_t>(std::min<std::size_t>(_world.AliveCount(), std::numeric_limits<std::uint16_t>::max()));
    update.own = _own;

    // THE REPEATED FACTS RIDE THE FIRST UPDATE OF A TICK, so a tick counts once toward a repeat
    // however many updates it sent. Past the cap they wait; they are repeated anyway.
    if (index == 0)
    {
      for (const PendingRemoval& removal : client.removals)
      {
        if (update.removals.size() >= MAX_REMOVALS_PER_UPDATE)
        {
          break;
        }
        update.removals.push_back(removal.identity);
      }
      for (const PendingFire& fire : client.fires)
      {
        if (update.fires.size() >= MAX_FIRES_PER_UPDATE)
        {
          break;
        }
        update.fires.push_back(fire.event);
      }
    }

    const std::size_t room = (UPDATE_PAYLOAD_BYTES - EncodedSize(update)) / EntityRecord::SIZE_BYTES;
    const std::size_t take = std::min(room, candidates.size() - next);
    update.records.reserve(take);
    for (std::size_t taken = 0; taken < take; ++taken)
    {
      const Candidate& chosen = candidates[next + taken];
      update.records.push_back(chosen.record);

      Tracked& tracked = client.tracked[chosen.slot];
      tracked.score = 0;
      tracked.lastSentTick = _tick;
      tracked.sent = true;
      tracked.lastSent = chosen.record;
    }
    next += take;

    updates.push_back(std::move(update));
  }

  // === ONE TICK OFF EVERY REPEATED FACT THAT WENT OUT. ==============================================
  const std::size_t removalsSent = updates.front().removals.size();
  for (std::size_t index = 0; index < removalsSent; ++index)
  {
    --client.removals[index].remaining;
  }
  // **ROUND ROBIN PAST THE CAP** (the 2026-09-23 review, m3). Served first-in-first-out, a 55-entity
  // elimination sent the first 48 removals ten times before the last seven went out once, which left seven
  // ghosts on screen for half a second. The ones that went out move behind the ones that did not, so every
  // pending removal's first send is within a tick or two of its death whatever the backlog. Stable in both
  // halves, and nothing downstream depends on the order within one update.
  std::rotate(client.removals.begin(), client.removals.begin() + static_cast<std::ptrdiff_t>(removalsSent), client.removals.end());
  client.removals.erase(std::remove_if(client.removals.begin(), client.removals.end(),
                                       [](const PendingRemoval& _removal) noexcept { return _removal.remaining == 0; }),
                        client.removals.end());

  const std::size_t firesSent = updates.front().fires.size();
  for (std::size_t index = 0; index < firesSent; ++index)
  {
    --client.fires[index].remaining;
  }
  client.fires.erase(
    std::remove_if(client.fires.begin(), client.fires.end(), [](const PendingFire& _fire) noexcept { return _fire.remaining == 0; }),
    client.fires.end());

  return updates;
}

} // namespace Outpost
