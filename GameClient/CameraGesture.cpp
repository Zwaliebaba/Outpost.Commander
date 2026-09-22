#include "pch.h"

#include "CameraGesture.h"

#include <cmath>
#include <limits>

namespace Outpost
{

namespace
{
inline constexpr float AUTHORED_WIDTH = static_cast<float>(Neuron::INTERFACE_AUTHORED_WIDTH);
inline constexpr float AUTHORED_HEIGHT = static_cast<float>(Neuron::INTERFACE_AUTHORED_HEIGHT);

inline constexpr float PI = 3.14159265358979323846f;

[[nodiscard]] float ToRadians(float _degrees) noexcept
{
  return _degrees * (PI / 180.0f);
}
} // namespace

bool CameraGesture::Begin(const CameraPose& _pose, float _aspectRatio, float _centroidXAuthored, float _centroidYAuthored) noexcept
{
  float screenX = 0.0f;
  float screenY = 0.0f;
  AuthoredToNormalized(_centroidXAuthored, _centroidYAuthored, AUTHORED_WIDTH, AUTHORED_HEIGHT, screenX, screenY);

  float anchorX = 0.0f;
  float anchorY = 0.0f;
  if (!ScreenToPlane(_pose, _aspectRatio, screenX, screenY, anchorX, anchorY))
  {
    // The ray missed the plane, which is the horizon and above it. The pitch floor keeps that off
    // screen, so reaching here means the floor was bypassed -- and a gesture that did nothing is a
    // better outcome than one anchored to a point behind the eye.
    m_active = false;
    return false;
  }

  m_active = true;
  m_poseAtStart = _pose;
  m_anchorWorldX = anchorX;
  m_anchorWorldY = anchorY;
  m_centroidXAuthored = _centroidXAuthored;
  m_centroidYAuthored = _centroidYAuthored;
  return true;
}

CameraPose CameraGesture::Update(const Neuron::GatedManipulation& _manipulation, float _aspectRatio) const noexcept
{
  if (!m_active)
  {
    return m_poseAtStart;
  }

  CameraPose pose = m_poseAtStart;

  // **SCALE FIRST, AND IT IS A DIVISION.** Above one the fingers moved apart, which is a pinch out,
  // which is zooming IN -- so the distance shrinks. R21 names this sign as a thing a package can hide
  // and a test cannot, and the test is in `CameraGestureTests`.
  //
  // The gate has already applied the two per cent deadzone, so a pure orbit arrives here at exactly
  // one and does not creep the zoom -- which matters more than it sounds, because pitch is coupled to
  // distance and a creeping zoom is a silently re-pitching camera.
  if (_manipulation.scale > 0.0f)
  {
    pose.distance = ClampDistance(m_poseAtStart.distance / _manipulation.scale);
  }

  // **AND PITCH IS NOT SET HERE BECAUSE IT IS NOT STORED.** `CameraPose` holds four values and pitch
  // is derived from the distance every time it is needed, which is what makes "pitch is coupled to
  // zoom and is not separately controllable" true by construction rather than by discipline.

  // **THE ROTATION TURNS THE GROUND WITH THE FINGERS**, so the camera turns the other way. Positive
  // is clockwise on screen (the recognizer's convention, kept); a clockwise finger rotation should
  // carry the map clockwise under it, and the camera orbiting counter-clockwise is what does that.
  //
  // The gate has already applied the eight-degree deadzone and its latch, and has rebased the value
  // at the crossing (Q38) -- so engaging rotation moves nothing.
  pose.headingRadians = m_poseAtStart.headingRadians - ToRadians(_manipulation.rotationDegrees);

  // **THE TARGET IS THE STARTING CENTROID PLUS THE GATE'S PAN.** The gate measures the pan from the
  // moment the tap slop was crossed, so it is zero at the crossing and the target is exactly where
  // the anchor already is -- which is how a tap becoming a pan does not jump sixteen pixels.
  const float targetXAuthored = m_centroidXAuthored + _manipulation.panXAuthoredPixels;
  const float targetYAuthored = m_centroidYAuthored + _manipulation.panYAuthoredPixels;

  float screenX = 0.0f;
  float screenY = 0.0f;
  AuthoredToNormalized(targetXAuthored, targetYAuthored, AUTHORED_WIDTH, AUTHORED_HEIGHT, screenX, screenY);

  float focusX = 0.0f;
  float focusY = 0.0f;
  if (!SolveFocusForAnchor(pose, _aspectRatio, m_anchorWorldX, m_anchorWorldY, screenX, screenY, focusX, focusY))
  {
    // The finger has reached a point on screen that does not meet the plane. Hold the last good
    // pose rather than teleporting: this is the horizon case again and the floor should prevent it.
    return m_poseAtStart;
  }

  pose.focusX = focusX;
  pose.focusY = focusY;

  // **AFTER THE SOLVE** (ADR-018 decision 6). At the edge the ground stops and the finger slides over
  // it, because the anchor is no longer where the solve asked for. That reads as a defect the first
  // time it is seen and is the correct behavior for a strategy map -- the alternative is a camera
  // that fights the hand.
  ClampFocus(pose);
  return pose;
}

CameraPose CameraGesture::Complete(const CameraPose& _pose) noexcept
{
  CameraPose settled = _pose;
  settled.headingRadians = SnapHeadingToCardinal(_pose.headingRadians);
  ClampFocus(settled);
  return settled;
}

CameraPose Recenter(const CameraPose& _pose, const RecenterRequest& _request) noexcept
{
  CameraPose moved = _pose;

  const std::size_t count =
    (_request.selectionX.size() < _request.selectionY.size()) ? _request.selectionX.size() : _request.selectionY.size();
  if (count == 0)
  {
    moved.focusX = _request.stationX;
    moved.focusY = _request.stationY;
  }
  else
  {
    // The centroid, which is the whole of it. A bounding-box center would jump when one ship at the
    // edge of the selection dies, and an RTS recenter is asked for while things are dying.
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (std::size_t index = 0; index < count; ++index)
    {
      sumX += _request.selectionX[index];
      sumY += _request.selectionY[index];
    }
    moved.focusX = sumX / static_cast<float>(count);
    moved.focusY = sumY / static_cast<float>(count);
  }

  ClampFocus(moved);
  return moved;
}

float TopToCenterPixelStretch(float _pitchRadians) noexcept
{
  // How much plane a pixel covers goes as 1 / sin^2 of the depression, so the ratio is the sines
  // squared the other way up. The center depression is the pitch itself; the top edge's is half a
  // field of view less.
  const float depression = _pitchRadians - (ToRadians(VERTICAL_FIELD_OF_VIEW_DEGREES) * 0.5f);
  if (depression <= 0.0f)
  {
    return std::numeric_limits<float>::infinity();
  }

  const float top = std::sin(depression);
  const float center = std::sin(_pitchRadians);
  if (top <= 0.0f)
  {
    return std::numeric_limits<float>::infinity();
  }
  return (center * center) / (top * top);
}

float TopToCenterStretch(float _pitchRadians) noexcept
{
  const float center = GroundDistanceAtScreenCenter(_pitchRadians);
  if (center <= 0.0f)
  {
    return 0.0f;
  }

  const float top = GroundDistanceAtScreenTop(_pitchRadians);
  if (top <= 0.0f)
  {
    // The top edge is at or above the horizontal, where the ground distance is infinite or behind the
    // camera. Unbounded is the honest answer and the floor is what stops it being reachable.
    return std::numeric_limits<float>::infinity();
  }
  return top / center;
}

} // namespace Outpost
