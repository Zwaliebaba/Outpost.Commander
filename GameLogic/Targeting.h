#pragma once

#include "UniformGrid.h"
#include "World.h"

#include <cstdint>
#include <vector>

namespace Outpost
{

/// What a design can shoot with, over all its mounts: the longest range and the widest arc any of them has.
/// **Derived from the catalog** (R24), so a station reaches 480 all around because its point defense rows
/// say so, and nothing here names it.
///
/// R8: a public aggregate.
struct Reach
{
  std::uint32_t rangeUnits;
  std::uint16_t arcHalfAngle;
  bool armed;
};

[[nodiscard]] Reach ReachOf(DesignId _design) noexcept;

/// **ANOTHER PLAYER'S, AND NOT NOBODY'S.** An entity owned by nobody is never a target.
[[nodiscard]] bool IsHostile(const Entity& _shooter, const Entity& _other) noexcept;

/// **WITHIN _rangeUnits, INCLUSIVE, CENTER TO CENTER**, compared squared in `Fixed` so a target exactly on the
/// boundary is in range on every run and every machine (M3.2's done-when).
[[nodiscard]] bool InRange(const Neuron::Vec2& _from, const Neuron::Vec2& _to, std::uint32_t _rangeUnits) noexcept;

/// **WITHIN _arcHalfAngle OF THE SHOOTER'S HEADING** (Q68), through the pinned integer bearing. Half a turn or
/// more is all around.
[[nodiscard]] bool InArc(const Entity& _shooter, const Neuron::Vec2& _target, std::uint16_t _arcHalfAngle) noexcept;

/// **THE NEAREST HOSTILE IN REACH, TIES ON IDENTITY** (M3.2, ADR-002): candidates come from the grid sorted by
/// identity, and `UniformGrid::Nearest` keeps the lower of two at equal distance, so the choice never depends
/// on which cell was visited first. When _requireArc, only a hostile inside the arc counts: that is a ship
/// under a move order, which fires at will but never turns to bear (Q78). `NO_ENTITY` when nothing is in reach.
///
/// _grid must have been rebuilt from _world this tick.
[[nodiscard]] EntityId SelectTarget(const World& _world, const UniformGrid& _grid, const Entity& _shooter, bool _requireArc,
                                    std::vector<EntityId>& _scratch);

} // namespace Outpost
