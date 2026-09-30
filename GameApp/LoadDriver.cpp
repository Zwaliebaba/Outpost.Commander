#include "pch.h"
#include "LoadDriver.h"

namespace
{
// Opposite corners of the map's open middle; the two players head for opposite ones, so their fleets cross.
constexpr Outpost::PlanePosition FIRST_POINT{.xMeters = -550.0f, .zMeters = 550.0f};
constexpr Outpost::PlanePosition SECOND_POINT{.xMeters = 550.0f, .zMeters = -550.0f};
} // namespace

std::vector<Outpost::Command> Outpost::LoadDriver::Update(const Snapshot& _snapshot)
{
  const std::uint64_t period = _snapshot.tick / PERIOD_TICKS;
  if (m_lastPeriod == period)
    return {};
  m_lastPeriod = period;

  MoveCommand move;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind == EntityKind::Ship && entity.owner == _snapshot.player)
      move.ships.push_back(entity.id);
  }
  if (move.ships.empty())
    return {};
  const bool first = ((period + _snapshot.player.value) % 2) == 0;
  move.destination = first ? FIRST_POINT : SECOND_POINT;
  return {Command{.player = {}, .order = std::move(move)}};
}
