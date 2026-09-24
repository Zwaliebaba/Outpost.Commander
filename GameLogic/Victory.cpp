#include "pch.h"

#include "Victory.h"

#include <array>

namespace Outpost
{

void Victory::Begin(std::size_t _players, std::uint32_t _startTick) noexcept
{
  m_players = (_players > MAX_PLAYERS) ? MAX_PLAYERS : _players;
  m_startTick = _startTick;
  m_outcome = MatchOutcome{};
  m_removed.clear();
}

void Victory::Advance(World& _world, const BuildSystem& _build, std::uint32_t _tick)
{
  m_removed.clear();
  if (m_outcome.over)
  {
    return;
  }

  // WHO STILL HAS A STATION, and how much hull it has left: the station is a design, so this asks the design
  // whether it is a station and nothing names a hull (R24's spirit; the station design is the seat).
  std::array<bool, MAX_PLAYERS + 1> standing{};
  std::array<std::uint32_t, MAX_PLAYERS + 1> stationHull{};
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.design == DesignId::Station) && (entity.owner >= 1) && (entity.owner <= m_players))
    {
      standing[entity.owner] = true;
      stationHull[entity.owner] += entity.hullRemaining;
    }
  }

  // ELIMINATION: EVERYTHING A PLAYER WITHOUT A STATION OWNS GOES THIS TICK. Collected first, then destroyed, so the
  // sweep never reads a slot it has just freed.
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner >= 1) && (entity.owner <= m_players) && !standing[entity.owner])
    {
      m_removed.push_back(entity.id);
    }
  }
  for (const EntityId& id : m_removed)
  {
    static_cast<void>(_world.Destroy(id));
  }

  PlayerId last = NO_PLAYER;
  std::size_t count = 0;
  for (std::size_t player = 1; player <= m_players; ++player)
  {
    if (standing[player])
    {
      last = static_cast<PlayerId>(player);
      ++count;
    }
  }

  // THE LAST STATION STANDING. A match of one seat has nobody to outlast and ends only on the clock.
  if (m_players >= 2)
  {
    if (count == 0)
    {
      m_outcome = MatchOutcome{.over = true, .winner = NO_PLAYER};
      return;
    }
    if (count == 1)
    {
      m_outcome = MatchOutcome{.over = true, .winner = last};
      return;
    }
  }

  // THE CLOCK (Q65), counted in ticks completed since the match began.
  if (((_tick + 1) - m_startTick) < MATCH_CLOCK_TICKS)
  {
    return;
  }

  // Credits plus the catalog cost of everything else still standing, for a tie on hull.
  std::array<std::uint64_t, MAX_PLAYERS + 1> worth{};
  for (std::size_t player = 1; player <= m_players; ++player)
  {
    worth[player] = _build.Credits(static_cast<PlayerId>(player));
  }
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner >= 1) && (entity.owner <= m_players) && (entity.design != DesignId::Station))
    {
      worth[entity.owner] += Derive(entity.design).cost;
    }
  }

  PlayerId best = NO_PLAYER;
  bool tied = false;
  for (std::size_t player = 1; player <= m_players; ++player)
  {
    if (!standing[player])
    {
      continue;
    }
    if (best == NO_PLAYER)
    {
      best = static_cast<PlayerId>(player);
      tied = false;
      continue;
    }
    const auto better = [&](std::size_t _a, std::size_t _b) noexcept
    { return (stationHull[_a] != stationHull[_b]) ? (stationHull[_a] > stationHull[_b]) : (worth[_a] > worth[_b]); };
    const bool equal = (stationHull[player] == stationHull[best]) && (worth[player] == worth[best]);
    if (equal)
    {
      tied = true;
    }
    else if (better(player, best))
    {
      best = static_cast<PlayerId>(player);
      tied = false;
    }
  }
  m_outcome = MatchOutcome{.over = true, .winner = tied ? NO_PLAYER : best, .onClock = true};
}

} // namespace Outpost
