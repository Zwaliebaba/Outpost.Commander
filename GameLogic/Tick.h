#pragma once

#include "World.h"

#include <cstdint>

namespace Outpost
{

/// ADR-002's tick, in milliseconds, for the shell to hand to a schedule. IT IS A PLAIN INTEGER
/// AND NOT A `std::chrono` TYPE, because this library is the simulation's and R16 keeps wall time
/// out of it -- the seam is `Neuron::TickSchedule`, in the engine, driven by `Server.cpp`.
inline constexpr std::int64_t TICK_PERIOD_MILLISECONDS = 50;

/// Twenty. Derived rather than restated, because two statements of it that must agree is a defect
/// waiting for somebody to move one.
inline constexpr std::uint32_t TICKS_PER_SECOND = static_cast<std::uint32_t>(1000 / TICK_PERIOD_MILLISECONDS);

/// **R24's SPEED, IN `Fixed` PER TICK** -- thrust over mass over the tick rate, which is what
/// `GameCore/DerivedStats.h` computes and what M0 could not use because an entity had no design.
///
/// **IT REPLACED A CONSTANT WITH A COMMENT ON IT.** `CommandIntake::MOVE_SPEED_PER_TICK` was
/// `7 * 256` and said "R24 will derive this from thrust over mass; at M0 the intake needs a number
/// and this is it". Seven units a tick is exactly the `Fighter`'s, which is why nothing looked
/// wrong: the Miner was moving at the Fighter's speed and only the Fighter's was right.
///
/// A design with no drive returns zero, which `World::OrderMoveTo` reads as an order that moves
/// nothing -- a station told to move stays where it is rather than being a special case somewhere.
[[nodiscard]] constexpr Neuron::Fixed SpeedPerTick(DesignId _design) noexcept
{
  const std::uint32_t unitsPerSecond = Derive(_design).speedUnitsPerSecond;
  return static_cast<Neuron::Fixed>((unitsPerSecond * static_cast<std::uint32_t>(Neuron::FIXED_ONE)) / TICKS_PER_SECOND);
}

/// Q59's turn rate per tick: the derived rate a second over the tick rate. A Fighter swings 1,638
/// binary-angle units a tick and a Miner 1,170. Zero for a design with no drive, like its speed.
[[nodiscard]] constexpr std::uint16_t TurnAnglePerTick(DesignId _design) noexcept
{
  return static_cast<std::uint16_t>(Derive(_design).turnAnglePerSecond / TICKS_PER_SECOND);
}

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
