#pragma once

#include "GameCore.h"

#include "NeuronCore.h"

#include <cstdint>

namespace Outpost
{

/// THE CLIENT RENDERS THE PAST, and this file is the arithmetic that decides how far past and
/// where between two snapshots the frame actually sits (`TechnicalDesign.md` section 6).
///
/// **SINCE ADR-024 THE TWO ARE ONE ENTITY'S OWN SAMPLES, NOT TWO SNAPSHOTS.** An update is a bag of
/// records rather than the world, so `ReplicaStore` interpolates each entity between the two samples it
/// holds of it. The arithmetic here did not change and neither did its names; read "snapshot" below as
/// "sample". What did change is that the store never asks for a fraction past the newer sample -- an
/// entity past its newest holds, and the extrapolation states below go unused by it.
///
/// IT TOUCHES NOTHING BUT NUMBERS. The store above it owns which snapshots are held; the renderer
/// below it owns what a position becomes on screen. What is here takes milliseconds and wire
/// values and returns wire values, so a suite can pin it without a socket, a clock or a device --
/// the same split R21 forced on the gesture seam, for the same reason.
///
/// R16 DOES NOT REACH THIS FILE and that is not an oversight. `Scripts/CheckDeterminism.py` sweeps
/// `GameCore` and `GameLogic`; this is presentation, and two clients that interpolate a frame
/// differently disagree about nothing the host will ever ask them. It is written in fixed point
/// anyway, because the values arriving are fixed point and converting to float and back to draw
/// would be work rather than simplicity.

/// How far behind the newest snapshot the frame is drawn -- ADR-003 and `TechnicalDesign.md`
/// section 4, which both call it "one snapshot interval plus a jitter margin": 50 plus 25 at
/// 20 Hz. ONE NAMED CONSTANT, because the plan's step names three places a literal 75 could hide
/// and every one of them would have to move together.
inline constexpr std::uint32_t INTERPOLATION_DELAY_MILLISECONDS = 75;

/// Snapshots go out at 20 Hz (`TechnicalDesign.md` section 4). Here because the delay above is
/// meaningless without it and the buffer depth below is computed from the pair.
inline constexpr std::uint32_t SNAPSHOT_INTERVAL_MILLISECONDS = 50;

/// HOW FAR THE CLIENT WILL GUESS BEFORE IT STOPS, and the design does not state this figure --
/// it says "a short bounded window" and then "holds position rather than sliding a ship somewhere
/// it never was" (`TechnicalDesign.md` section 6). One snapshot interval is the bound this file
/// takes, and the argument is ADR-003's: a single lost snapshot is a 50-millisecond gap that the
/// 75-millisecond buffer covers without extrapolating at all, so extrapolation only begins when a
/// SECOND consecutive snapshot is missing -- which at 2% loss is once every two minutes. Guessing
/// through one more interval covers the third, and past that the guess is worth less than the
/// stillness: a ship drawn where it never was has to be yanked back when the truth arrives.
inline constexpr std::uint32_t EXTRAPOLATION_BOUND_MILLISECONDS = SNAPSHOT_INTERVAL_MILLISECONDS;

/// What the render clock found when it looked.
enum class PlayoutState : std::uint8_t
{
  /// The render time sits between two snapshots, which is the ordinary answer.
  Interpolating,
  /// Past the newest snapshot and inside the bound above -- the fraction runs beyond one and the
  /// caller is extending the last known motion.
  Extrapolating,
  /// Past the bound. The fraction stops at the bound and stays there, so the frame is still
  /// drawn and nothing slides.
  Holding,
  /// Fewer than two snapshots, or two that carry the same timestamp. Nothing to interpolate
  /// between and nothing to guess from; the caller draws the newest as it stands.
  Starved
};

/// Where the frame sits between a pair of snapshots.
///
/// R8: a value with no invariant of its own, so plain fields and brace initialization.
struct Playout
{
  /// Zero at the older snapshot, `Neuron::FIXED_ONE` at the newer, and beyond it when
  /// extrapolating. Never negative: a render time before the older snapshot is the store handing
  /// over the wrong pair, and clamping is the answer that still draws a frame.
  Neuron::Fixed fraction = 0;

  PlayoutState state = PlayoutState::Starved;
};

/// The fraction, from three timestamps on one monotonic millisecond clock.
///
/// _olderMilliseconds and _newerMilliseconds are the two snapshots' times, _renderMilliseconds is
/// where the frame is being drawn. A newer that is not after the older is Starved rather than a
/// division by zero.
[[nodiscard]] Playout ComputePlayout(std::uint64_t _olderMilliseconds, std::uint64_t _newerMilliseconds,
                                     std::uint64_t _renderMilliseconds) noexcept;

/// A position between two, at the fraction above. Linear, and the fraction is allowed past one so
/// that an extrapolating frame extends the same line rather than taking a second code path.
[[nodiscard]] Neuron::Fixed InterpolatePosition(Neuron::Fixed _from, Neuron::Fixed _to, Neuron::Fixed _fraction) noexcept;

/// A heading between two, THE SHORT WAY ROUND -- which the binary angle makes a subtraction
/// rather than a special case, and which is the whole reason ADR-002 chose the format
/// (`NeuronCore/FixedPoint.h`). A heading of 1 degree and one of 359 degrees are two degrees
/// apart, so the ship turns through north rather than sweeping the long way back around.
[[nodiscard]] Neuron::Angle InterpolateHeading(Neuron::Angle _from, Neuron::Angle _to, Neuron::Fixed _fraction) noexcept;

/// `DequantizeWireHeading` WAS HERE AND IS NOW IN `GameCore/EntityRecord.h`, beside
/// `QuantizePosition` and the rest of the wire's rounding rules -- which is where this file said it
/// would go "when the host needs it". M1.3 is when: `BuildSnapshot` was doing the shift by hand.
/// Both halves are in `Outpost`, so nothing that called it moved.

/// One entity, interpolated between the two snapshots that straddle the frame.
///
/// FALSE WHEN THE IDENTITIES ARE NOT THE SAME ENTITY, and this is the guard that makes the
/// generation in ADR-003's identity worth its bits. Two records can share an index and be two
/// different ships: the slot was freed and refilled between the snapshots, which
/// `GameCore/EntityRecord.h` sizes the generation to make visible. Interpolating across that
/// draws one ship sliding into the other's position -- a teleport with a smooth animation on it,
/// which is harder to recognize as a bug than a teleport would be. Matching on the whole packed
/// identity costs one comparison and forecloses it.
///
/// The fields that do not interpolate are taken from the NEWER record: hull, design, flags. A
/// hull percentage between two integers is a number the host never held, and a flag half set is
/// not a state -- the newest truth is the honest answer for all three.
[[nodiscard]] bool InterpolateRecord(const EntityRecord& _older, const EntityRecord& _newer, Neuron::Fixed _fraction,
                                     EntityRecord& _outRecord) noexcept;

} // namespace Outpost
