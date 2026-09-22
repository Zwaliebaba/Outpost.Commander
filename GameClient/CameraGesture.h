#pragma once

#include "Camera.h"

#include "NeuronClient.h"

#include <cstddef>
#include <span>

namespace Outpost
{

/// ADR-018's camera, driven by a manipulation. **M0.20 built the arithmetic and this is the thing that
/// calls it in the right order.**
///
/// **ONE SOLVE DOES ALL OF IT** (`Interface.md` section 5). Scale becomes a distance and therefore a
/// pitch; rotation becomes a heading; then one anchor solve places the focus. Orbit turns about the
/// anchor rather than the focus for free, and **a one-finger drag is the same solve with no scale and
/// no rotation** -- which is why section 3's "two fingers pan identically to one" is true by
/// construction here rather than by care.
///
/// **THE RECOGNIZER'S TRANSLATION IS NOT APPLIED ON TOP.** It is already in the solve, because the
/// solve asks where the focus has to be for the anchor to sit under the current contact point. Applying
/// both double-counts it, and a camera that appears to accelerate under the finger is that defect and
/// almost always that one.
///
/// **NOTHING HERE TOUCHES A `CoreWindow`.** It takes `Neuron::GatedManipulation` -- the deadzones, the
/// latch and the slop already applied by M0.18 -- and authored pixels, so the suite R21 requires can
/// drive a whole gesture (`Plan/README.md` F6).

/// Where the camera is when nothing has been selected yet, and what a recenter falls back to.
///
/// R8: a public aggregate.
struct RecenterRequest
{
  /// The selection's positions, in world units. Empty means nothing is selected.
  std::span<const float> selectionX;
  std::span<const float> selectionY;

  /// Your station, which is where a recenter goes when nothing is selected (ADR-018).
  float stationX = 0.0f;
  float stationY = 0.0f;
};

/// One manipulation in progress.
///
/// R8: a value with no invariant of its own -- `Begin` and `Update` are where the rules live.
class CameraGesture
{
public:
  /// Captures the anchor and the pose the gesture started from.
  ///
  /// _centroidX and _centroidY are the contact centroid in authored pixels, which is what
  /// `Interface.md` section 1's frame reports and what every constant in the gate is stated in.
  ///
  /// **FALSE WHEN THE CENTROID DOES NOT MEET THE PLANE**, which is the horizon and above it. The pitch
  /// floor is what keeps that off screen, so a false here means the floor has been bypassed -- and the
  /// gesture then does nothing rather than moving the camera to a point behind the eye.
  bool Begin(const CameraPose& _pose, float _aspectRatio, float _centroidXAuthored, float _centroidYAuthored) noexcept;

  /// One update. Returns the new pose; returns the starting pose unchanged when the solve fails or when
  /// no gesture is active.
  ///
  /// **THE TARGET SCREEN POINT IS THE STARTING CENTROID PLUS THE GATE'S PAN**, and the gate's pan is
  /// measured from the moment the tap slop was crossed (`Interface.md` section 3). So the camera does
  /// not jump by sixteen pixels when a tap becomes a pan: the pan is zero at the crossing and the
  /// target is exactly where the anchor already is.
  [[nodiscard]] CameraPose Update(const Neuron::GatedManipulation& _manipulation, float _aspectRatio) const noexcept;

  /// What a release settles on: the heading snapped to the nearest cardinal when one is within the
  /// threshold (ADR-018). **There is no minimap and no compass**, so a map that is reliably north-up
  /// whenever the player is not deliberately turning it is what spatial memory is built on.
  ///
  /// The focus is clamped here as well, because the snap turns the camera about the focus and a clamp
  /// that ran before it would be the wrong clamp.
  [[nodiscard]] static CameraPose Complete(const CameraPose& _pose) noexcept;

  [[nodiscard]] bool IsActive() const noexcept
  {
    return m_active;
  }

  [[nodiscard]] const CameraPose& PoseAtStart() const noexcept
  {
    return m_poseAtStart;
  }

  [[nodiscard]] float AnchorWorldX() const noexcept
  {
    return m_anchorWorldX;
  }

  [[nodiscard]] float AnchorWorldY() const noexcept
  {
    return m_anchorWorldY;
  }

  void End() noexcept
  {
    m_active = false;
  }

private:
  bool m_active = false;
  CameraPose m_poseAtStart{};
  float m_anchorWorldX = 0.0f;
  float m_anchorWorldY = 0.0f;
  float m_centroidXAuthored = 0.0f;
  float m_centroidYAuthored = 0.0f;
};

/// **A HOLD ON EMPTY SPACE RECENTERS** (ADR-018, spending one of the two verbs ADR-017 freed).
/// `GameDesign.md` section 7 names the problem: a defender has to be watching the right part of a
/// 16,384-unit map at the right moment, and until this existed the only way back was panning there.
///
/// It goes to the selection's centroid, or to your station when nothing is selected. **It is the cheap
/// half of a minimap** without the second render, the second coordinate space or the second hit test
/// that `Interface.md` section 5 declines.
///
/// The heading and the distance are untouched -- a recenter is a translation, and turning the camera as
/// well would be two things on one verb.
[[nodiscard]] CameraPose Recenter(const CameraPose& _pose, const RecenterRequest& _request) noexcept;

/// **THE STRETCH FROM THE TOP OF THE FRAME TO ITS MIDDLE, AT A GIVEN PITCH** -- how much further a
/// screen pixel reaches on the plane up there than at the center.
///
/// `Interface.md` section 5 states 3.3x at the 30-degree floor and says M1.8 measures it. It is the one
/// number that bounds both tap precision at the top of the frame and the wedge ADR-010's screen circle
/// becomes, and it grows without bound as the top edge approaches the horizontal -- which is what the
/// floor exists to stop.
[[nodiscard]] float TopToCenterStretch(float _pitchRadians) noexcept;

/// **THE SAME COMPARISON IN WORLD UNITS PER SCREEN PIXEL, WHICH IS THE ONE THAT BOUNDS A TAP.**
///
/// M1.8 measured both and they do not behave alike: the ground-distance ratio above is **not monotonic
/// in pitch** -- 3.27 at the 30-degree floor and 5.33 at the 85-degree ceiling -- because near top-down
/// the frame's center is almost directly beneath the camera and a small absolute difference is a large
/// ratio. That is a property of dividing two small numbers, not a precision problem: at maximum
/// zoom-out everything on the plane is far away and equally coarse.
///
/// What actually decides how much world a pixel covers is the local scale of the projection, which goes
/// as `1 / sin^2` of the depression angle. **That ratio IS monotonic**, is worst exactly at the floor,
/// and grows without bound as the top edge approaches the horizontal -- so it is the measure
/// `Interface.md` section 5's claim is true of, and it is stated here because the figure section 5
/// quotes is the other one.
[[nodiscard]] float TopToCenterPixelStretch(float _pitchRadians) noexcept;

/// The zoom range, as a ratio. Arithmetic on the two ends rather than a figure anybody typed.
[[nodiscard]] constexpr float ZoomRange() noexcept
{
  return MAXIMUM_CAMERA_DISTANCE / MINIMUM_CAMERA_DISTANCE;
}

} // namespace Outpost
