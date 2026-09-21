#pragma once

#include <cstdint>

namespace Neuron
{

/// The simulation's number, and ADR-002 settles what it is: `std::int32_t` with eight fractional
/// bits, so one unit is 1/256 of a world unit -- about four millimeters at the design's nominal
/// meter. The 16,384-unit square spans +/-2,097,152, which leaves three orders of magnitude of
/// headroom in an `int32`.
///
/// AN ALIAS, NOT A STRONG TYPE, AND DELIBERATELY SO. A strong type would catch a class of unit
/// error the compiler otherwise cannot -- adding a velocity to a position, scaling by the wrong
/// thing -- and that is a real argument. It is also an amendment to ADR-002, which says
/// `std::int32_t`, and therefore that ADR's decision to take in its own pull request rather than
/// this file's to take quietly.
using Fixed = std::int32_t;

/// ADR-002: 65,536 to a full turn, so addition wraps without a modulus and the difference of two
/// headings is a subtraction rather than a special case. That is the whole reason for the format,
/// and AngleDifference below is the payoff.
using Angle = std::uint16_t;

/// R3: a compile-time constant is UPPER_CASE. Eight, from ADR-002.
inline constexpr int FIXED_FRACTION_BITS = 8;

/// One whole world unit, 256. The scale factor between Fixed and a plain count of units.
inline constexpr Fixed FIXED_ONE = Fixed{1} << FIXED_FRACTION_BITS;

/// A quarter turn, 16,384. Cosine is sine a quarter turn along, which is the only thing this is
/// for, and it is here rather than spelled 16384 at the two call sites.
inline constexpr Angle ANGLE_QUARTER_TURN = Angle{1} << 14;

[[nodiscard]] constexpr Fixed FixedFromWholeUnits(std::int32_t _wholeUnits) noexcept
{
  return static_cast<Fixed>(static_cast<std::uint32_t>(_wholeUnits) << FIXED_FRACTION_BITS);
}

/// TOWARD NEGATIVE INFINITY, not toward zero. C++20 defines `>>` on a signed value as an
/// arithmetic shift, so this floors, and -1 comes back from anything in [-256, -1] rather than
/// the 0 that a division would give. Every conversion in this file floors for the same reason:
/// one rounding rule is a thing a reader can hold, two is a thing they have to look up.
[[nodiscard]] constexpr std::int32_t FixedToWholeUnitsFloor(Fixed _value) noexcept
{
  return _value >> FIXED_FRACTION_BITS;
}

/// ADR-002's multiply, and the 64-bit intermediate is not optional: two values near the play
/// area's edge produce a product around 4.4e12, which leaves an `int32` far behind.
///
/// THE BOUND IS THE CHECK, NOT THE TYPE. This is correct for ONE multiply of two in-range values.
/// A chain is not: three operands at eight fractional bits each accumulate twenty-four bits of
/// fraction before any shift, and the intermediate runs out sooner than intuition says. Shift
/// between steps, or hold the intermediate yourself.
///
/// A result that will not fit a Fixed wraps, modulo 2^32, which C++20 defines for the narrowing
/// conversion -- so it is deterministic on every platform and wrong on all of them equally. It is
/// the caller's bound to respect; see FixedPointTests, which pins what the edges actually do.
[[nodiscard]] inline Fixed Multiply(Fixed _a, Fixed _b) noexcept
{
  const std::int64_t product = static_cast<std::int64_t>(_a) * _b;
  return static_cast<Fixed>(static_cast<std::uint64_t>(product >> FIXED_FRACTION_BITS));
}

/// The inverse, with the numerator widened and shifted up before the divide so the fraction
/// survives it.
///
/// IT TRUNCATES TOWARD ZERO WHERE Multiply FLOORS, because that is what C++ `/` does and hiding
/// it behind a correction would be this file inventing a rounding rule ADR-002 does not state.
/// The asymmetry is real, it is pinned in the suite, and it is written here so that the next
/// person meets it as a documented property rather than as an off-by-one in a movement bug.
///
/// _divisor MUST NOT be zero. There is no in-band answer to return -- every Fixed is a legal
/// result -- so the precondition is the contract rather than a sentinel nobody would check.
[[nodiscard]] inline Fixed Divide(Fixed _dividend, Fixed _divisor) noexcept
{
  const std::int64_t numerator = static_cast<std::int64_t>(_dividend) << FIXED_FRACTION_BITS;
  return static_cast<Fixed>(static_cast<std::uint64_t>(numerator / _divisor));
}

/// The signed difference between two headings, in [-32768, +32767], with no branch and no
/// modulus. This is what the binary angle buys: the subtraction wraps by construction, so a
/// heading of 1 degree and one of 359 degrees are two degrees apart rather than 358.
[[nodiscard]] constexpr std::int16_t AngleDifference(Angle _from, Angle _to) noexcept
{
  return static_cast<std::int16_t>(static_cast<Angle>(_to - _from));
}

/// Floor of the square root, exact on a perfect square. Digit by digit in integers, because
/// `std::sqrt` is a CRT transcendental and R16 will not have one anywhere near the simulation.
///
/// ADR-002 compares distances squared in `std::int64_t` and reaches for a magnitude rarely; this
/// is that rare case. The result is `std::int64_t` and not `Fixed` because the square root of a
/// full `int64` is a little over 3.0e9, which an `int32` cannot hold.
///
/// A negative _value returns 0: it is the caller's bug, but a squared distance is never negative
/// and there is nothing better to do at this level than refuse to read off the end of the loop.
[[nodiscard]] std::int64_t Sqrt(std::int64_t _value) noexcept;

} // namespace Neuron
