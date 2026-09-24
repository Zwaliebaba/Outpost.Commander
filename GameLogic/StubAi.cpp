#include "pch.h"

#include "StubAi.h"

#include "Accumulator.h"
#include "Host.h"

#include <algorithm>

namespace Outpost
{

namespace
{
[[nodiscard]] Neuron::Vec2 PositionOf(const EntityRecord& _record) noexcept
{
  return Neuron::Vec2{.x = DequantizePosition(_record.positionX), .y = DequantizePosition(_record.positionY)};
}

[[nodiscard]] std::int64_t DistanceSquared(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  const std::int64_t dx = static_cast<std::int64_t>(_a.x) - _b.x;
  const std::int64_t dy = static_cast<std::int64_t>(_a.y) - _b.y;
  return (dx * dx) + (dy * dy);
}

[[nodiscard]] bool IsKnownDesign(const EntityRecord& _record) noexcept
{
  return _record.designIdentity < Designs().size();
}

[[nodiscard]] DesignId DesignOf(const EntityRecord& _record) noexcept
{
  return static_cast<DesignId>(_record.designIdentity);
}

/// A ship is anything with a drive; a station or a module is not one.
[[nodiscard]] bool IsShip(const EntityRecord& _record) noexcept
{
  return IsKnownDesign(_record) && (Derive(DesignOf(_record)).speedUnitsPerSecond > 0);
}

/// The nearest of _candidates to _from, ties to the lower identity (R16). Nothing when there are none.
[[nodiscard]] const EntityRecord* Nearest(std::span<const EntityRecord* const> _candidates, const Neuron::Vec2& _from) noexcept
{
  const EntityRecord* best = nullptr;
  std::int64_t bestSquared = 0;
  for (const EntityRecord* candidate : _candidates)
  {
    const std::int64_t squared = DistanceSquared(PositionOf(*candidate), _from);
    if ((best == nullptr) || (squared < bestSquared) || ((squared == bestSquared) && (candidate->identity < best->identity)))
    {
      best = candidate;
      bestSquared = squared;
    }
  }
  return best;
}
} // namespace

void StubAi::Begin() noexcept
{
  m_orderedMiners.clear();
  m_attackers.clear();
  m_attackTarget = NO_WIRE_IDENTITY;
}

void StubAi::Decide(const AiView& _view, std::vector<Command>& _outCommands)
{
  _outCommands.clear();

  // WHAT IT HAS, from the records, in the order they came -- which is identity order, so every choice below is too.
  const EntityRecord* station = nullptr;
  std::vector<const EntityRecord*> miners;
  std::vector<const EntityRecord*> fighters;
  std::vector<const EntityRecord*> enemyStations;
  std::vector<const EntityRecord*> enemyShips;
  for (const EntityRecord& record : _view.entities)
  {
    if (!IsKnownDesign(record))
    {
      continue;
    }
    const DesignId design = DesignOf(record);
    if (record.owner == _view.player)
    {
      if (design == DesignId::Station)
      {
        station = &record;
      }
      else if (design == DesignId::Miner)
      {
        miners.push_back(&record);
      }
      else if (IsShip(record) && (Derive(design).damagePerSecond > 0))
      {
        fighters.push_back(&record);
      }
    }
    else if (record.owner != NO_PLAYER)
    {
      if (design == DesignId::Station)
      {
        enemyStations.push_back(&record);
      }
      else if (IsShip(record))
      {
        enemyShips.push_back(&record);
      }
    }
  }
  if (station == nullptr)
  {
    return;
  }
  const Neuron::Vec2 home = PositionOf(*station);

  // === BUILD: miners to six, then fighters, one at a time and only what it can pay for. ===================
  const bool idle = (BuildingDesignOf(_view.own.buildingDesign) == 0) && (QueuedOf(_view.own.buildingDesign) == 0);
  if (idle)
  {
    const DesignId next = (miners.size() < AI_MINERS) ? DesignId::Miner : DesignId::Fighter;
    if (_view.own.credits >= Derive(next).cost)
    {
      _outCommands.push_back(Command{
        .sequence = NextSequence(), .type = CommandType::Build, .targetX = static_cast<std::int16_t>(next), .targetY = 0, .selection = {}});
    }
  }

  // === MINE: every new miner to one of the nearest rocks, spread one to a rock. ===============================
  std::vector<std::uint16_t> nearestRocks;
  for (const EntityRecord* miner : miners)
  {
    if (std::find(m_orderedMiners.begin(), m_orderedMiners.end(), miner->identity) != m_orderedMiners.end())
    {
      continue;
    }
    if (nearestRocks.empty())
    {
      for (std::size_t index = 0; index < _view.field.size(); ++index)
      {
        nearestRocks.push_back(static_cast<std::uint16_t>(index));
      }
      std::sort(nearestRocks.begin(), nearestRocks.end(),
                [&](std::uint16_t _a, std::uint16_t _b)
                {
                  const std::int64_t da = DistanceSquared(_view.field[_a].position, home);
                  const std::int64_t db = DistanceSquared(_view.field[_b].position, home);
                  return (da != db) ? (da < db) : (_a < _b);
                });
    }
    if (nearestRocks.empty())
    {
      break;
    }
    const std::uint16_t rock = nearestRocks[m_orderedMiners.size() % std::min(nearestRocks.size(), AI_MINERS)];
    Command mine{.sequence = NextSequence(), .type = CommandType::Mine, .selection = {miner->identity}};
    mine.AimAtRock(rock);
    _outCommands.push_back(std::move(mine));
    m_orderedMiners.push_back(miner->identity);
  }

  // === FIGHT: defend the home field first, else strike with enough fighters. =================================
  if (fighters.empty())
  {
    return;
  }
  std::vector<const EntityRecord*> raiders;
  const std::int64_t defend = static_cast<std::int64_t>(AI_DEFEND_RADIUS_UNITS) * Neuron::FIXED_ONE;
  for (const EntityRecord* enemy : enemyShips)
  {
    if (DistanceSquared(PositionOf(*enemy), home) <= (defend * defend))
    {
      raiders.push_back(enemy);
    }
  }

  const EntityRecord* target = nullptr;
  if (!raiders.empty())
  {
    target = Nearest(raiders, home);
  }
  else if (fighters.size() >= AI_STRIKE_FIGHTERS)
  {
    target = Nearest(enemyStations, home);
  }
  if (target == nullptr)
  {
    return;
  }

  std::vector<WireIdentity> attackers;
  for (const EntityRecord* fighter : fighters)
  {
    attackers.push_back(fighter->identity);
  }
  if ((target->identity == m_attackTarget) && (attackers == m_attackers))
  {
    return;
  }
  Command attack{.sequence = NextSequence(), .type = CommandType::Attack, .selection = attackers};
  attack.AimAt(target->identity);
  _outCommands.push_back(std::move(attack));
  m_attackTarget = target->identity;
  m_attackers = std::move(attackers);
}

void AiSeats::Begin(std::size_t _players, std::size_t _aiSeats) noexcept
{
  m_players = std::min(_players, MAX_PLAYERS);
  m_count = std::min(_aiSeats, m_players);
  for (StubAi& ai : m_ais)
  {
    ai.Begin();
  }
}

bool AiSeats::IsAi(PlayerId _player) const noexcept
{
  return (_player != NO_PLAYER) && (_player <= m_players) && (_player > (m_players - m_count));
}

void AiSeats::Advance(World& _world, BuildSystem& _build, CommandIntake& _intake, std::uint32_t _tick)
{
  if ((m_count == 0) || ((_tick % AI_DECISION_INTERVAL_TICKS) != 0))
  {
    return;
  }

  // THE VIEW A CLIENT IS SENT, built once for every AI seat this tick: every live entity as its wire record.
  m_view.clear();
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (_world.IsSlotAlive(slot))
    {
      m_view.push_back(RecordOf(_world, slot));
    }
  }

  for (std::size_t seat = (m_players - m_count) + 1; seat <= m_players; ++seat)
  {
    const auto player = static_cast<PlayerId>(seat);
    const AiView view{.player = player, .entities = m_view, .own = PlayerBlockFor(_intake, _build, player), .field = _world.Field()};
    m_ais[seat].Decide(view, m_commands);
    for (const Command& command : m_commands)
    {
      static_cast<void>(_intake.Apply(_world, _build, player, command));
    }
  }
}

} // namespace Outpost
