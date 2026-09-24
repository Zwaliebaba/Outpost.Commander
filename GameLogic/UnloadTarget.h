#pragma once

#include "UniformGrid.h"
#include "World.h"

#include <vector>

namespace Outpost
{

/// **WHERE A MINER UNLOADS IS A QUERY, NOT A CONSTANT** (M2.6, `GameDesign.md` section 4): the nearest live
/// entity that _player owns and whose derived stats accept ore, from _from, over the whole map. Today that
/// set holds one station. **It is written as a set** because a mining factory at a contested field is only
/// worth adding if the miner already asks what is nearest -- writing "the station" here is what would make
/// that feature a rewrite instead of a table row.
///
/// **TIES ON IDENTITY** (R16), through `UniformGrid::Nearest`: two acceptors at exactly equal distance
/// resolve to the lower identity on every run. `NO_ENTITY` when the player owns nothing that accepts ore --
/// "an idle miner with a full hold and a dead station" (section 4), which is the economy failing visibly.
///
/// _grid must have been rebuilt from _world this tick. _scratch is the caller's, so nothing allocates once
/// it has grown.
[[nodiscard]] EntityId FindUnloadTarget(const World& _world, const UniformGrid& _grid, PlayerId _player, const Neuron::Vec2& _from,
                                        std::vector<EntityId>& _scratch);

/// **HOW CLOSE IS TOUCHING**: the two hulls' half sizes (Q37's `sizeUnits`), plus `UNLOAD_SLACK_UNITS`, in
/// `Fixed`. A miner's center within this of the acceptor's center unloads (`OpenQuestions.md` Q51). From the
/// catalog's sizes, so a larger acceptor is reached from further out without anybody restating it.
[[nodiscard]] Neuron::Fixed UnloadReach(DesignId _miner, DesignId _acceptor) noexcept;

/// Q51, answered by the owner on 2026-09-23 and provisional: **twenty units of slack past touching**, so a
/// miner that stopped a step short of the exact contact still counts. 110 + 30 + 20 = 160 for a `Scout` at the
/// station.
inline constexpr std::int32_t UNLOAD_SLACK_UNITS = 20;

/// **HOW FAR FROM AN ACCEPTOR A HOSTILE COUNTS AS A THREAT TO ITS UNLOADING** (Q63 as built): the home field's
/// outer radius plus the longest weapon's reach, so a raider anywhere it could be shooting at a miner in the home
/// field is seen, and an enemy base across the map is not. Past it the unload point is the near side, as before
/// Q63, which is what keeps Q62's measured economy true in a match nobody is raiding.
[[nodiscard]] std::int32_t ThreatRadiusUnits() noexcept;

/// **HOW NEAR THE FAR-SIDE POINT A MINER MUST BE TO UNLOAD THERE**: well inside the distance between the two sides,
/// and wide enough that the router's arrival, which stops a little short, counts.
inline constexpr std::int32_t FAR_SIDE_SLACK_UNITS = 60;

/// **Q63: THE FAR SIDE.** When a hostile is within `ThreatRadiusUnits` of _acceptor, the point a _miner unloads at
/// is its unload reach from the acceptor's center, on the ray away from the nearest such hostile -- which at a
/// station is inside point defense's 480 and out of a raider's 600 from anywhere the point defense does not cover.
/// The nearest is the grid's, ties to the lower identity, and the ray comes from the pinned bearing and sine table
/// (R16). False, and _outPoint untouched, when nothing hostile is that close.
[[nodiscard]] bool FarSideUnloadPoint(const World& _world, const UniformGrid& _grid, const Entity& _acceptor, DesignId _miner,
                                      std::vector<EntityId>& _scratch, Neuron::Vec2& _outPoint);

} // namespace Outpost
