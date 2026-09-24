#pragma once

#include <cstdint>

namespace Outpost
{

/// ADR-002's tick, in milliseconds, for the shell to hand to a schedule. IT IS A PLAIN INTEGER
/// AND NOT A `std::chrono` TYPE, because this library is the simulation's rules and R16 keeps wall time
/// out of them -- the seam is `Neuron::TickSchedule`, in the engine, driven by `Server.cpp`.
///
/// **HERE SINCE M3.1, AND IN `GameLogic/Tick.h` BEFORE IT.** `GameCore`'s damage table states what a
/// weapon does in one tick (ADR-014), so the rules both sides share have to know how long a tick is.
/// It moved rather than being restated, so there is still one statement of it.
inline constexpr std::int64_t TICK_PERIOD_MILLISECONDS = 50;

/// Twenty. Derived rather than restated, because two statements of it that must agree is a defect
/// waiting for somebody to move one.
inline constexpr std::uint32_t TICKS_PER_SECOND = static_cast<std::uint32_t>(1000 / TICK_PERIOD_MILLISECONDS);

} // namespace Outpost
