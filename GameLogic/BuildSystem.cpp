#include "pch.h"

#include "BuildSystem.h"

namespace Outpost
{

namespace
{
/// The player's station, or nullptr. A linear scan over the store, which at the design's 110 entities
/// is nothing -- and the alternative, a per-player cached identity, is a second thing to keep correct
/// across a death that M3 has not written yet.
[[nodiscard]] const Entity* StationOf(const World& _world, PlayerId _player) noexcept
{
  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner == _player) && (entity.design == DesignId::Station))
    {
      return &entity;
    }
  }
  return nullptr;
}
} // namespace

std::uint32_t BuildSystem::TicksToBuild(DesignId _design, std::uint32_t _multiplierPercent) noexcept
{
  const std::uint32_t multiplier = (_multiplierPercent == 0) ? 100 : _multiplierPercent;
  const std::uint64_t cost = Derive(_design).cost;

  // cost / (rate * multiplier / 100) seconds, times the tick rate. Written as one division so there is
  // one rounding rather than two, and both sides of the wire compute it the same way (R16).
  const std::uint64_t numerator = cost * TICKS_PER_SECOND * 100;
  const std::uint64_t denominator = static_cast<std::uint64_t>(BUILD_RATE_CREDITS_PER_SECOND) * multiplier;
  const std::uint64_t ticks = numerator / denominator;

  // AT LEAST ONE. A design with no cost would otherwise complete on the tick it started, and the
  // catalog has rows with no cost in it.
  return (ticks == 0) ? 1 : static_cast<std::uint32_t>(ticks);
}

void BuildSystem::Begin(std::size_t _playerCount) noexcept
{
  m_playerCount = (_playerCount > MAX_PLAYERS) ? MAX_PLAYERS : _playerCount;
  m_credits.fill(0);
  m_items.fill(BuildItem{});
  m_stranded = 0;

  for (std::size_t player = 1; player <= m_playerCount; ++player)
  {
    m_credits[player] = STARTING_CREDITS;
  }
}

bool BuildSystem::Holds(PlayerId _player) const noexcept
{
  return (_player != NO_PLAYER) && (static_cast<std::size_t>(_player) <= m_playerCount);
}

Neuron::Vec2 BuildSystem::SpawnPoint(const Entity& _station, DesignId _design) noexcept
{
  const std::uint32_t stationSize = Hull(Design(_station.design).hull).sizeUnits;
  const std::uint32_t shipSize = Hull(Design(_design).hull).sizeUnits;

  // Half of each, so the two hulls touch rather than overlap. Rounded up, because these are bounds.
  const std::int64_t offsetUnits = static_cast<std::int64_t>((stationSize + shipSize + 1) / 2);
  const std::int64_t offset = offsetUnits * Neuron::FIXED_ONE;

  // The station's heading, through ADR-002's pinned table -- the only trigonometry the simulation is
  // allowed (R16). The station faces the center of the map, so a new ship appears on the side the
  // player is looking toward.
  const std::int64_t alongX = (static_cast<std::int64_t>(Neuron::Cosine(_station.heading)) * offset) / Neuron::SINE_ONE;
  const std::int64_t alongY = (static_cast<std::int64_t>(Neuron::Sine(_station.heading)) * offset) / Neuron::SINE_ONE;

  return ClampToPlayArea(Neuron::Vec2{.x = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(_station.position.x) + alongX),
                                      .y = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(_station.position.y) + alongY)});
}

BuildRejection BuildSystem::Start(World& _world, PlayerId _player, DesignId _design, std::uint32_t _buildRateMultiplierPercent) noexcept
{
  if (!Holds(_player))
  {
    return BuildRejection::NoPlayer;
  }
  if (static_cast<std::size_t>(_design) >= Designs().size())
  {
    return BuildRejection::UnknownDesign;
  }
  if (!Design(_design).buildable)
  {
    return BuildRejection::NotBuildable;
  }
  if (StationOf(_world, _player) == nullptr)
  {
    return BuildRejection::NoStation;
  }

  // THE REFUND FIRST (Q35). Replacing a Fighter with a Fighter has to work, and it would not if the
  // new cost were checked against a balance the old item was still holding.
  BuildItem& item = m_items[static_cast<std::size_t>(_player)];
  const bool replacing = item.active;
  const std::uint32_t refunded = replacing ? item.creditsSpent : 0;
  m_credits[static_cast<std::size_t>(_player)] += refunded;

  const std::uint32_t cost = Derive(_design).cost;
  if (m_credits[static_cast<std::size_t>(_player)] < cost)
  {
    // Nothing was spent, so the refund above stands and the old item is simply cancelled -- which is
    // what a player who taps something they cannot afford has asked for, and the interface will show
    // it by the item disappearing rather than by a message.
    item = BuildItem{};
    return BuildRejection::Unaffordable;
  }

  m_credits[static_cast<std::size_t>(_player)] -= cost;
  item = BuildItem{.active = true,
                   .design = _design,
                   .ticksElapsed = 0,
                   .ticksRequired = TicksToBuild(_design, _buildRateMultiplierPercent),
                   .creditsSpent = cost};
  return BuildRejection::None;
}

bool BuildSystem::Cancel(PlayerId _player) noexcept
{
  if (!Holds(_player))
  {
    return false;
  }

  BuildItem& item = m_items[static_cast<std::size_t>(_player)];
  if (!item.active)
  {
    return false;
  }

  // FULL, AND FROM WHAT WAS TAKEN RATHER THAN FROM THE CATALOG (Q35).
  m_credits[static_cast<std::size_t>(_player)] += item.creditsSpent;
  item = BuildItem{};
  return true;
}

void BuildSystem::Advance(World& _world) noexcept
{
  // In player order, which is the order everything else in this tree walks players in -- and the
  // order two completions on one tick reach the store in (R16).
  for (std::size_t player = 1; player <= m_playerCount; ++player)
  {
    BuildItem& item = m_items[player];
    if (!item.active)
    {
      continue;
    }

    ++item.ticksElapsed;
    if (item.ticksElapsed < item.ticksRequired)
    {
      continue;
    }

    const Entity* station = StationOf(_world, static_cast<PlayerId>(player));
    if (station == nullptr)
    {
      // NOWHERE TO PUT IT. A station destroyed eliminates its owner (`GameDesign.md` section 2) and
      // M3 is what writes that; until then the honest answer is to give the credits back rather than
      // to drop a ship at the origin.
      m_credits[player] += item.creditsSpent;
      item = BuildItem{};
      ++m_stranded;
      continue;
    }

    // Copied before `Create`, which may reallocate the store out from under the pointer above.
    const Neuron::Vec2 spawn = SpawnPoint(*station, item.design);
    const Neuron::Angle heading = station->heading;

    static_cast<void>(_world.Create(spawn, heading, item.design, static_cast<PlayerId>(player)));
    item = BuildItem{};
  }
}

std::uint32_t BuildSystem::Credits(PlayerId _player) const noexcept
{
  return Holds(_player) ? m_credits[static_cast<std::size_t>(_player)] : 0;
}

void BuildSystem::Grant(PlayerId _player, std::uint32_t _credits) noexcept
{
  if (Holds(_player))
  {
    m_credits[static_cast<std::size_t>(_player)] += _credits;
  }
}

const BuildItem& BuildSystem::Item(PlayerId _player) const noexcept
{
  static constexpr BuildItem NOTHING{};
  return Holds(_player) ? m_items[static_cast<std::size_t>(_player)] : NOTHING;
}

std::uint8_t BuildSystem::WireBuildingDesign(PlayerId _player) const noexcept
{
  const BuildItem& item = Item(_player);
  return item.active ? static_cast<std::uint8_t>(static_cast<std::uint8_t>(item.design) + 1) : 0;
}

std::uint8_t BuildSystem::WireProgressPercent(PlayerId _player) const noexcept
{
  const BuildItem& item = Item(_player);
  if (!item.active || (item.ticksRequired == 0))
  {
    return 0;
  }

  const std::uint64_t percent = (static_cast<std::uint64_t>(item.ticksElapsed) * 100) / item.ticksRequired;
  return (percent > 99) ? 99 : static_cast<std::uint8_t>(percent);
}

} // namespace Outpost
