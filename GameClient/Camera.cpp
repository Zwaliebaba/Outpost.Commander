#include "pch.h"

#include "Camera.h"

#include <cmath>

namespace Outpost
{

namespace
{
constexpr float PI = 3.14159265358979323846f;
constexpr float QUARTER_TURN_RADIANS = PI / 2.0f;
constexpr float FULL_TURN_RADIANS = PI * 2.0f;

[[nodiscard]] constexpr float ToRadians(float _degrees) noexcept
{
  return _degrees * (PI / 180.0f);
}

[[nodiscard]] constexpr float Dot(const Vector3& _a, const Vector3& _b) noexcept
{
  return (_a.x * _b.x) + (_a.y * _b.y) + (_a.z * _b.z);
}

[[nodiscard]] constexpr float Clamp(float _value, float _low, float _high) noexcept
{
  if (_value < _low)
  {
    return _low;
  }
  if (_value > _high)
  {
    return _high;
  }
  return _value;
}

/// Half the vertical field of view as a tangent, which is the only form the projection uses.
[[nodiscard]] float TangentOfHalfFieldOfView() noexcept
{
  return std::tan(ToRadians(VERTICAL_FIELD_OF_VIEW_DEGREES) * 0.5f);
}

/// The ray direction for a normalized screen point, in world space. NOT normalized to unit
/// length -- nothing below needs it to be, and the two places that use it both divide by a
/// component, where a scale factor cancels.
[[nodiscard]] Vector3 DirectionThroughScreenPoint(const CameraBasis& _basis, float _aspectRatio, float _screenX, float _screenY) noexcept
{
  const float tangent = TangentOfHalfFieldOfView();
  const float alongRight = _screenX * tangent * _aspectRatio;
  const float alongUp = _screenY * tangent;

  return Vector3{_basis.forward.x + (_basis.right.x * alongRight) + (_basis.up.x * alongUp),
                 _basis.forward.y + (_basis.right.y * alongRight) + (_basis.up.y * alongUp),
                 _basis.forward.z + (_basis.right.z * alongRight) + (_basis.up.z * alongUp)};
}
} // namespace

float PitchForDistance(float _distance) noexcept
{
  // Linear between the two ends, which is the simplest coupling that is monotonic -- and monotonic
  // is what `Interface.md` section 5 actually asks for. A curve here would be a feel decision, and
  // M1.8 is where feel decisions about this range are taken.
  const float span = MAXIMUM_CAMERA_DISTANCE - MINIMUM_CAMERA_DISTANCE;
  const float travel = (_distance - MINIMUM_CAMERA_DISTANCE) / span;
  const float degrees = MINIMUM_PITCH_DEGREES + (travel * (MAXIMUM_PITCH_DEGREES - MINIMUM_PITCH_DEGREES));

  // THE OUTPUT IS CLAMPED AND THE INPUT IS NOT. A distance below the near end is legal and simply
  // stops lowering the pitch; clamping the distance instead would make the floor a zoom limit.
  return ToRadians(Clamp(degrees, MINIMUM_PITCH_DEGREES, MAXIMUM_PITCH_DEGREES));
}

CameraBasis BuildBasis(const CameraPose& _pose) noexcept
{
  const float pitch = PitchForDistance(_pose.distance);
  const float cosPitch = std::cos(pitch);
  const float sinPitch = std::sin(pitch);
  const float cosHeading = std::cos(_pose.headingRadians);
  const float sinHeading = std::sin(_pose.headingRadians);

  CameraBasis basis;

  // The eye sits behind the focus along the heading and above it by the pitch.
  basis.eye = Vector3{_pose.focusX - (_pose.distance * cosPitch * cosHeading), _pose.focusY - (_pose.distance * cosPitch * sinHeading),
                      _pose.distance * sinPitch};

  // Unit by construction: the components are a pitch and a heading resolved onto three axes.
  basis.forward = Vector3{cosPitch * cosHeading, cosPitch * sinHeading, -sinPitch};

  // NEVER ROLLS (ADR-001). Right is the heading turned a quarter turn in the plane and has no z
  // term at all, which is what "never rolls" means expressed as a vector.
  //
  // THE SIGN IS THE HANDEDNESS AND IT IS EASY TO GET BACKWARDS. With z up and the camera looking
  // along +x, the right hand points along -y, not +y: this is forward crossed with the world's up.
  // Flip it and every screen x is mirrored, which a round-trip test cannot see -- it inverts twice
  // and agrees with itself -- so the suite pins a point to the camera's right landing at a positive
  // screen x instead.
  basis.right = Vector3{sinHeading, -cosHeading, 0.0f};

  // up = right x forward, expanded. Also unit, since the two it comes from are unit and
  // perpendicular.
  basis.up = Vector3{(basis.right.y * basis.forward.z) - (basis.right.z * basis.forward.y),
                     (basis.right.z * basis.forward.x) - (basis.right.x * basis.forward.z),
                     (basis.right.x * basis.forward.y) - (basis.right.y * basis.forward.x)};

  return basis;
}

bool ScreenToPlane(const CameraPose& _pose, float _aspectRatio, float _screenX, float _screenY, float& _outWorldX,
                   float& _outWorldY) noexcept
{
  const CameraBasis basis = BuildBasis(_pose);
  const Vector3 direction = DirectionThroughScreenPoint(basis, _aspectRatio, _screenX, _screenY);

  // Parallel to the plane, or pointing up and away from it: the horizon and above. The pitch floor
  // is what keeps this off the screen, so reaching it means the floor was bypassed.
  if (direction.z >= 0.0f)
  {
    return false;
  }

  // ADR-018's "a divide". The eye is always above the plane, so this is positive.
  const float travel = -basis.eye.z / direction.z;
  _outWorldX = basis.eye.x + (direction.x * travel);
  _outWorldY = basis.eye.y + (direction.y * travel);
  return true;
}

bool PlaneToScreen(const CameraPose& _pose, float _aspectRatio, float _worldX, float _worldY, float& _outScreenX,
                   float& _outScreenY) noexcept
{
  const CameraBasis basis = BuildBasis(_pose);
  const Vector3 toPoint{_worldX - basis.eye.x, _worldY - basis.eye.y, -basis.eye.z};

  const float depth = Dot(toPoint, basis.forward);
  if (depth <= 0.0f)
  {
    return false;
  }

  const float tangent = TangentOfHalfFieldOfView();
  _outScreenX = (Dot(toPoint, basis.right) / depth) / (tangent * _aspectRatio);
  _outScreenY = (Dot(toPoint, basis.up) / depth) / tangent;
  return true;
}

bool SolveFocusForAnchor(const CameraPose& _pose, float _aspectRatio, float _anchorWorldX, float _anchorWorldY, float _screenX,
                         float _screenY, float& _outFocusX, float& _outFocusY) noexcept
{
  // The basis's three AXES depend only on heading and pitch, and pitch only on distance -- none of
  // them on the focus. That is the whole reason this is closed form: the ray through the screen
  // point is known before the answer is.
  const CameraPose posed{0.0f, 0.0f, _pose.headingRadians, _pose.distance};
  const CameraBasis basis = BuildBasis(posed);
  const Vector3 direction = DirectionThroughScreenPoint(basis, _aspectRatio, _screenX, _screenY);

  if (direction.z >= 0.0f)
  {
    return false;
  }

  const float pitch = PitchForDistance(_pose.distance);
  const float eyeHeight = _pose.distance * std::sin(pitch);

  // The eye must sit where the ray through the screen point, walked backwards, reaches the eye's
  // own height -- the anchor is on the plane, so its height is zero and the whole drop is the
  // eye's. One divide, no iteration.
  const float travel = -eyeHeight / direction.z;
  const Vector3 eye{_anchorWorldX - (direction.x * travel), _anchorWorldY - (direction.y * travel), eyeHeight};

  // And the focus is the eye brought back down the heading. Its height is zero by construction:
  // the eye's height and the offset's are the same number.
  const float cosPitch = std::cos(pitch);
  _outFocusX = eye.x + (_pose.distance * cosPitch * std::cos(_pose.headingRadians));
  _outFocusY = eye.y + (_pose.distance * cosPitch * std::sin(_pose.headingRadians));
  return true;
}

void ClampFocus(CameraPose& _pose) noexcept
{
  const float limit = PLAY_AREA_HALF_EXTENT_UNITS + FOCUS_CLAMP_MARGIN;
  _pose.focusX = Clamp(_pose.focusX, -limit, limit);
  _pose.focusY = Clamp(_pose.focusY, -limit, limit);
}

float ClampDistance(float _distance) noexcept
{
  return Clamp(_distance, MINIMUM_CAMERA_DISTANCE, MAXIMUM_CAMERA_DISTANCE);
}

float SnapHeadingToCardinal(float _headingRadians) noexcept
{
  // Wrapped into one turn first, so that the arithmetic below does not depend on how many turns a
  // long gesture accumulated.
  float wrapped = std::fmod(_headingRadians, FULL_TURN_RADIANS);
  if (wrapped < 0.0f)
  {
    wrapped += FULL_TURN_RADIANS;
  }

  const float quarters = wrapped / QUARTER_TURN_RADIANS;
  const float nearestCardinal = std::round(quarters) * QUARTER_TURN_RADIANS;

  if (std::fabs(wrapped - nearestCardinal) <= ToRadians(CARDINAL_SNAP_DEGREES))
  {
    // Back into one turn: rounding up from just under a full turn lands on the full turn itself.
    return std::fmod(nearestCardinal, FULL_TURN_RADIANS);
  }
  return wrapped;
}

void AuthoredToNormalized(float _authoredX, float _authoredY, float _authoredWidth, float _authoredHeight, float& _outScreenX,
                          float& _outScreenY) noexcept
{
  _outScreenX = ((_authoredX / _authoredWidth) * 2.0f) - 1.0f;
  // Flipped: authored pixels count down from the top edge and the projection counts up.
  _outScreenY = 1.0f - ((_authoredY / _authoredHeight) * 2.0f);
}

float GroundDistanceAtScreenTop(float _pitchRadians) noexcept
{
  // The top edge looks half a field of view above the camera's own pitch, so it meets the plane at
  // the cotangent of what is left. At the floor that is 30 minus 20, which is ten degrees down.
  const float depression = _pitchRadians - (ToRadians(VERTICAL_FIELD_OF_VIEW_DEGREES) * 0.5f);
  return 1.0f / std::tan(depression);
}

float GroundDistanceAtScreenCenter(float _pitchRadians) noexcept
{
  return 1.0f / std::tan(_pitchRadians);
}

} // namespace Outpost
