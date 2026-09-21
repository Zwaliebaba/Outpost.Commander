#pragma once

#include "World.h"

#include <cstdint>

namespace Outpost
{

/// The hash ADR-002 puts at the end of every tick, over every entity's identity, position, heading
/// and hull. It is what the determinism test asserts and what a desynchronization report would
/// carry: two hosts that agree here agree about the match.
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

} // namespace Outpost
