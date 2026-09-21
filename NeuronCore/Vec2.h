#pragma once

#include "FixedPoint.h"

#include <cstdint>

namespace Neuron
{

/// A position or a velocity on ADR-001's plane, in ADR-002's Fixed.
///
/// NEVER `Vector.h`. `AGENTS.md` section 2 forbids a header spelled like an SDK or CRT one,
/// because the other projects' directories sit ahead of the SDK on the include path and MSVC
/// matches them case-insensitively even for an angled include -- so a file called `Vector.h` here
/// is what `<vector>` would find. `TechnicalDesign.md` section 1 names this trap explicitly.
///
/// R8: a public aggregate, so plain `camelCase` fields and brace initialization that reads
/// naturally -- `Vec2{.x = 0, .y = FIXED_ONE}`.
///
/// THE OPERATIONS THAT ARE NOT HERE ARE NOT HERE ON PURPOSE. There is no rotation, no
/// normalization and no length: ADR-002 settles a representation and M0.6 adds that and nothing
/// beyond it. Rotation needs the sine table and its first caller is movement, which is M0.8's;
/// normalization needs a divide by a length that ADR-002 says is computed rarely and compared
/// squared the rest of the time. Each arrives with the code that needs it, rather than now on
/// speculation about what that code will want.
struct Vec2
{
  Fixed x = 0;
  Fixed y = 0;

  [[nodiscard]] friend constexpr bool operator==(const Vec2&, const Vec2&) noexcept = default;
};

/// Componentwise, and the bound is the play area: two positions at opposite corners sum to
/// +/-4,194,304, which an `int32` holds with room to spare. A sum of many is the caller's bound
/// to keep, as it is for every Fixed expression (see Multiply).
[[nodiscard]] constexpr Vec2 operator+(const Vec2& _a, const Vec2& _b) noexcept
{
  return Vec2{.x = static_cast<Fixed>(static_cast<std::uint32_t>(_a.x) + static_cast<std::uint32_t>(_b.x)),
              .y = static_cast<Fixed>(static_cast<std::uint32_t>(_a.y) + static_cast<std::uint32_t>(_b.y))};
}

[[nodiscard]] constexpr Vec2 operator-(const Vec2& _a, const Vec2& _b) noexcept
{
  return Vec2{.x = static_cast<Fixed>(static_cast<std::uint32_t>(_a.x) - static_cast<std::uint32_t>(_b.x)),
              .y = static_cast<Fixed>(static_cast<std::uint32_t>(_a.y) - static_cast<std::uint32_t>(_b.y))};
}

[[nodiscard]] constexpr Vec2 operator-(const Vec2& _value) noexcept
{
  return Vec2{.x = static_cast<Fixed>(0u - static_cast<std::uint32_t>(_value.x)),
              .y = static_cast<Fixed>(0u - static_cast<std::uint32_t>(_value.y))};
}

/// Each component through ADR-002's multiply, so the scale is itself a Fixed and FIXED_ONE is
/// the identity.
[[nodiscard]] inline Vec2 Scale(const Vec2& _value, Fixed _scale) noexcept
{
  return Vec2{.x = Multiply(_value.x, _scale), .y = Multiply(_value.y, _scale)};
}

/// `std::int64_t` out, because ADR-002 compares distances squared and that is the width it says
/// to compare them in. Each term reaches 4.4e12 at the play area's edge and the sum 8.8e12, which
/// is four million times short of overflowing -- but a Fixed built from something other than a
/// position has no such bound, and then it is the caller's.
[[nodiscard]] constexpr std::int64_t Dot(const Vec2& _a, const Vec2& _b) noexcept
{
  return (static_cast<std::int64_t>(_a.x) * _b.x) + (static_cast<std::int64_t>(_a.y) * _b.y);
}

/// The squared magnitude, which is the form ADR-002 wants every comparison done in. Reach for
/// Sqrt only when a magnitude itself is the answer.
[[nodiscard]] constexpr std::int64_t LengthSquared(const Vec2& _value) noexcept
{
  return Dot(_value, _value);
}

} // namespace Neuron
