#pragma once

namespace Outpost
{
// A displacement on the battlefield plane, in meters: the difference of two PlanePositions. Server-side arithmetic for
// movement and pathing; it never crosses the transport.
struct PlaneVector
{
  float xMeters = 0.0f;
  float zMeters = 0.0f;

  friend constexpr bool operator==(const PlaneVector&, const PlaneVector&) = default;
};

[[nodiscard]] constexpr PlaneVector operator-(PlanePosition _to, PlanePosition _from) noexcept
{
  return {_to.xMeters - _from.xMeters, _to.zMeters - _from.zMeters};
}

[[nodiscard]] constexpr PlanePosition operator+(PlanePosition _position, PlaneVector _offset) noexcept
{
  return {_position.xMeters + _offset.xMeters, _position.zMeters + _offset.zMeters};
}

[[nodiscard]] constexpr PlanePosition operator-(PlanePosition _position, PlaneVector _offset) noexcept
{
  return {_position.xMeters - _offset.xMeters, _position.zMeters - _offset.zMeters};
}

[[nodiscard]] constexpr PlaneVector operator+(PlaneVector _a, PlaneVector _b) noexcept
{
  return {_a.xMeters + _b.xMeters, _a.zMeters + _b.zMeters};
}

[[nodiscard]] constexpr PlaneVector operator*(PlaneVector _vector, float _scale) noexcept
{
  return {_vector.xMeters * _scale, _vector.zMeters * _scale};
}

[[nodiscard]] constexpr float Dot(PlaneVector _a, PlaneVector _b) noexcept
{
  return _a.xMeters * _b.xMeters + _a.zMeters * _b.zMeters;
}

// A quarter turn counterclockwise, seen from above.
[[nodiscard]] constexpr PlaneVector Perpendicular(PlaneVector _vector) noexcept
{
  return {-_vector.zMeters, _vector.xMeters};
}

[[nodiscard]] inline float Length(PlaneVector _vector) noexcept
{
  return std::sqrt(Dot(_vector, _vector));
}

[[nodiscard]] inline float Distance(PlanePosition _a, PlanePosition _b) noexcept
{
  return Length(_a - _b);
}

// The unit vector along _vector, or _fallback when _vector has no length.
[[nodiscard]] inline PlaneVector Normalized(PlaneVector _vector, PlaneVector _fallback) noexcept
{
  const float length = Length(_vector);
  return length > 0.0f ? _vector * (1.0f / length) : _fallback;
}

// How far _point is from the segment from _a to _b.
[[nodiscard]] inline float DistanceToSegment(PlanePosition _point, PlanePosition _a, PlanePosition _b) noexcept
{
  const PlaneVector segment = _b - _a;
  const float lengthSquared = Dot(segment, segment);
  if (lengthSquared == 0.0f)
    return Distance(_point, _a);
  const float along = std::clamp(Dot(_point - _a, segment) / lengthSquared, 0.0f, 1.0f);
  return Distance(_point, _a + segment * along);
}
} // namespace Outpost
