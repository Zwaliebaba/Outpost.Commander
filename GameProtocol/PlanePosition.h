#pragma once

namespace Outpost
{
// A point on the battlefield plane, in meters (design §4). Everything sits on the plane, so there is no height: the
// renderer's y = 0 plane, with x and z as here.
struct PlanePosition
{
  float xMeters = 0.0f;
  float zMeters = 0.0f;

  friend constexpr bool operator==(const PlanePosition&, const PlanePosition&) = default;
};
} // namespace Outpost
