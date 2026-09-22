#pragma once

#include "World.h"

#include <cstdint>

namespace Outpost
{

/// ADR-002's tick, in milliseconds, for the shell to hand to a schedule. IT IS A PLAIN INTEGER
/// AND NOT A `std::chrono` TYPE, because this library is the simulation's and R16 keeps wall time
/// out of it -- the seam is `Neuron::TickSchedule`, in the engine, driven by `Server.cpp`.
inline constexpr std::int64_t TICK_PERIOD_MILLISECONDS = 50;

/// One pass over the world, in the fixed order `TechnicalDesign.md` section 2 names: drain
/// incoming commands, then orders, AI, movement, weapons, mining, build queues, deaths, victory.
/// No system reads another's half-updated output, and the order is not negotiable -- it is what
/// makes two hosts running the same inputs reach the same state.
///
/// AT M0 ONLY MOVEMENT EXISTS, and the rest are not written as empty functions. An empty system is
/// a thing a reader has to open to discover does nothing; a system that is absent is obvious.
///
/// The tick is the clock (R16). Nothing in here reads a wall clock, is handed a duration, or knows
/// how long anything took -- wall time meets the tick at exactly one seam, and that seam is the
/// host's loop, which is M0.11's.
void Tick(World& _world) noexcept;

} // namespace Outpost
