#pragma once

#include "Economy.h"
#include "World.h"

#include <cstdint>

namespace Outpost
{

/// The hash ADR-002 puts at the end of every tick, over every entity's identity, position, heading
/// and hull -- **and, since the 2026-09-23 review (M6), its hull points, its owner and its mine order**
/// (phase, rock, cargo and unload target). It is what the determinism test asserts and what a
/// desynchronization report would carry: two hosts that agree here agree about the world.
///
/// **WHY IT WIDENED.** It folded the hull *type* and not the hull *points*, so M3's damage arithmetic
/// could diverge between two builds -- F3's own worry, one point of rounding a shot -- with the positions,
/// the hash and the four-pair run all agreeing until a death happened to land a tick apart. The same was
/// true of cargo, which is where the economy's arithmetic lives.
///
/// IN INDEX ORDER, AND OVER LIVE SLOTS ONLY. A dead slot still holds its last occupant's record,
/// and two hosts that reused slots in a different sequence would carry different rubbish in
/// them -- so hashing the dead would report a divergence where there is none, which is worse than
/// missing one. The free list keeps the live indices themselves in step (see World).
///
/// FIELD BY FIELD, NEVER THE STRUCT'S BYTES. Hashing an Entity through a `memcpy` would fold in
/// its padding, which no standard says anything about and which two compilers, two architectures
/// or two optimization levels may fill differently. That is a divergence invented by the hash
/// itself, in the one function whose entire job is to detect divergence. Every field below is
/// folded explicitly, little end first, at a fixed width.
[[nodiscard]] std::uint64_t StateHash(const World& _world) noexcept;

/// **THE WHOLE MATCH**: `StateHash` over the world, then every seated player's credits, income owed to the
/// hundredth of a milli-credit, and the item building (the 2026-09-23 review, M6). The world alone does not
/// see a divergence in what a player can spend until it becomes a ship a tick apart; this sees it on the
/// tick it happens. In player order, field by field, for the reasons `StateHash` gives.
[[nodiscard]] std::uint64_t MatchHash(const World& _world, const BuildSystem& _build, const Economy& _economy) noexcept;

} // namespace Outpost
