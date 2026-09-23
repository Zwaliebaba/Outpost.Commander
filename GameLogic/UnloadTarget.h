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

} // namespace Outpost
