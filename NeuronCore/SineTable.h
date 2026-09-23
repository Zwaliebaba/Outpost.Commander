#pragma once

#include "FixedPoint.h"

#include <cstddef>
#include <cstdint>

namespace Neuron
{

/// ADR-002: 4,096 entries of `std::int16_t` in Q1.15, indexed by `angle >> 4`. Eight kilobytes,
/// no interpolation, and 0.088 degrees of angular resolution -- 360 divided by 4,096, which is
/// finer than anything a ship does.
inline constexpr std::size_t SINE_TABLE_SIZE = 4096;

/// Q1.15 SPANS [-1, +0.999969], SO ONE IS NOT REPRESENTABLE. `sin(90 degrees) = 1.0` would be
/// 32,768 and overflows an `int16_t`; the table saturates to 32,767 instead, a 0.003% error at
/// the cardinals. ADR-002 states this precisely so the first implementer meets it as a documented
/// property rather than as an off-by-one that compiles -- and it is why a multiply by "one" from
/// this table is quietly not a multiply by one.
///
/// The negative cardinal is -32,767 and not the -32,768 that Q1.15 could hold, so that the table
/// is symmetric about zero and a heading and its opposite scale by the same magnitude.
inline constexpr std::int16_t SINE_ONE = 32767;

/// The table is not exposed. It is an implementation detail of these two, and a caller reaching
/// past them is a caller who has to remember the `>> 4` and the saturation for themselves.
[[nodiscard]] std::int16_t Sine(Angle _angle) noexcept;

/// Sine a quarter turn along, which is the whole of it: the binary angle makes that addition wrap
/// for free, so there is no second table and no special case at the seam.
[[nodiscard]] std::int16_t Cosine(Angle _angle) noexcept;

/// The binary angle a vector points along: zero along +x, a quarter turn along +y, the convention
/// `Sine` and `Cosine` already use. **The inverse of the table, through the table** -- a binary search
/// over its first octant, so a bearing and the sine of that bearing can never disagree by more than the
/// table's own step, and no CRT `atan2` comes near the simulation (R16).
///
/// Resolved to one table step, 16 binary-angle units: exact at the cardinals and the diagonals, and
/// within 0.088 degrees of the true bearing everywhere else. The zero vector has no
/// bearing and returns zero. Each component must be below 2^47 in magnitude, which a position
/// difference always is.
[[nodiscard]] Angle BearingOf(std::int64_t _x, std::int64_t _y) noexcept;

} // namespace Neuron
