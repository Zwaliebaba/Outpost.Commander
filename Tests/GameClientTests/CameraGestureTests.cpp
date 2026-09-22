#include "pch.h"

#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr float ASPECT = 1440.0f / 960.0f;
constexpr float CENTER_X = 720.0f;
constexpr float CENTER_Y = 480.0f;

constexpr float PI = 3.14159265358979323846f;

[[nodiscard]] float Degrees(float _radians) noexcept
{
  return _radians * (180.0f / PI);
}

[[nodiscard]] Outpost::CameraPose PoseAt(float _distance, float _heading = 0.0f) noexcept
{
  return Outpost::CameraPose{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = _heading, .distance = _distance};
}

/// A gate update, as `GestureArithmetic` would have produced it.
[[nodiscard]] Neuron::GatedManipulation Manipulation(float _panX, float _panY, float _scale, float _rotationDegrees) noexcept
{
  return Neuron::GatedManipulation{.contactCount = 2,
                                   .isPanning = true,
                                   .panXAuthoredPixels = _panX,
                                   .panYAuthoredPixels = _panY,
                                   .scale = _scale,
                                   .rotationDegrees = _rotationDegrees};
}
} // namespace

/// `Interface.md` section 5: **pitch is coupled to zoom and is not separately controllable.**
TEST_CLASS(ThePitchCoupling)
{
public:
  /// Monotonic across the range, with both ends pinned. Fully zoomed out is near top-down; fully
  /// zoomed in rakes low.
  TEST_METHOD(PitchTracksZoomMonotonicallyWithBothEndsPinned)
  {
    Assert::AreEqual(85.0f, Degrees(Outpost::PitchForDistance(Outpost::MAXIMUM_CAMERA_DISTANCE)), 0.01f);
    Assert::AreEqual(30.0f, Degrees(Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE)), 0.01f);

    float previous = 0.0f;
    for (float distance = Outpost::MINIMUM_CAMERA_DISTANCE; distance <= Outpost::MAXIMUM_CAMERA_DISTANCE; distance += 100.0f)
    {
      const float pitch = Outpost::PitchForDistance(distance);
      Assert::IsTrue(pitch >= previous, L"pitch went backwards as the camera pulled out");
      previous = pitch;
    }
  }

  /// **THE FLOOR SATURATES THE PITCH; IT DOES NOT END THE ZOOM.** Read the coupling the other way and
  /// the camera quietly loses its close view, which is the opposite of what the floor is for. M1.8
  /// names this as the thing that gets read backwards.
  TEST_METHOD(TheFloorSaturatesAndDoesNotTerminateTheRange)
  {
    // Below the near end is a legal distance, and it simply stops lowering the pitch.
    for (const float distance : {1400.0f, 1000.0f, 400.0f, 1.0f})
    {
      Assert::AreEqual(30.0f, Degrees(Outpost::PitchForDistance(distance)), 0.01f, L"the floor stopped being a floor");
    }

    // And `ClampDistance` is what bounds the range -- the pitch is not involved.
    Assert::AreEqual(Outpost::MINIMUM_CAMERA_DISTANCE, Outpost::ClampDistance(1.0f));
    Assert::AreEqual(Outpost::MAXIMUM_CAMERA_DISTANCE, Outpost::ClampDistance(1000000.0f));
  }

  /// **NEVER BELOW THIRTY DEGREES AT A FORTY-DEGREE FIELD OF VIEW**, which is the number that bounds
  /// both tap precision at the top of the frame and the wedge ADR-010 warns about.
  TEST_METHOD(ThePitchNeverGoesBelowTheFloor)
  {
    for (float distance = 1.0f; distance < 40000.0f; distance += 37.0f)
    {
      Assert::IsTrue(Degrees(Outpost::PitchForDistance(distance)) >= (30.0f - 0.01f));
    }
    Assert::AreEqual(40.0f, Outpost::VERTICAL_FIELD_OF_VIEW_DEGREES);
  }
};

/// The two figures `Interface.md` section 5 says M1.8 pins, measured rather than restated.
TEST_CLASS(TheZoomRangeAndTheStretch)
{
public:
  /// **16.07x**, which is the near end of 1,400 against the far end of 22,500. Section 5 says a
  /// comfortable pinch spans about 4x, so the whole range is two gestures and no gain constant is
  /// needed -- and that is the claim this number supports.
  TEST_METHOD(TheZoomRangeIsAboutSixteenTimes)
  {
    Assert::AreEqual(22500.0f, Outpost::MAXIMUM_CAMERA_DISTANCE);
    Assert::AreEqual(1400.0f, Outpost::MINIMUM_CAMERA_DISTANCE);
    Assert::AreEqual(16.07f, Outpost::ZoomRange(), 0.01f);

    // Two pinches of 4x each covers it, which is what "no gain constant is needed" means.
    Assert::IsTrue(Outpost::ZoomRange() <= (4.0f * 4.0f) + 0.1f);
  }

  /// **3.3x, TOP TO MIDDLE, AT THE FLOOR.** Section 5 states the figure and says M1.8 measures it.
  /// At 30 degrees the top edge looks ten degrees down, meeting the plane at 5.67 camera heights
  /// against 1.73 at the center.
  TEST_METHOD(TheStretchAtTheFloorIsThreePointThree)
  {
    const float atFloor = Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE);

    Assert::AreEqual(5.67f, Outpost::GroundDistanceAtScreenTop(atFloor), 0.01f);
    Assert::AreEqual(1.73f, Outpost::GroundDistanceAtScreenCenter(atFloor), 0.01f);
    Assert::AreEqual(3.27f, Outpost::TopToCenterStretch(atFloor), 0.01f);
  }

  /// **AND IT GROWS WITHOUT BOUND AS THE TOP EDGE APPROACHES THE HORIZONTAL**, which is what the floor
  /// exists to stop. Asserted at pitches below the floor, which the camera cannot reach -- the point is
  /// that the cost of lowering the floor is not linear in the degrees given up.
  TEST_METHOD(TheStretchGrowsWithoutBoundBelowTheFloor)
  {
    const float atFloor = Outpost::TopToCenterStretch(Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE));

    // Twenty-five degrees is five below the floor and already half again as bad.
    const float lower = Outpost::TopToCenterStretch(25.0f * (PI / 180.0f));
    Assert::IsTrue(lower > atFloor * 1.4f, L"lowering the floor five degrees should cost far more than five degrees");

    // At twenty degrees the top edge is horizontal and the ground distance is unbounded.
    Assert::IsTrue(std::isinf(Outpost::TopToCenterStretch(20.0f * (PI / 180.0f))));
  }

  /// **AND M1.8 FOUND THAT THE FIGURE SECTION 5 QUOTES IS NOT MONOTONIC IN PITCH.** The
  /// ground-distance ratio is 3.27 at the floor and **5.33 at the 85-degree ceiling**, which reads as
  /// though the camera is worse when zoomed out. It is not: near top-down the frame's center sits
  /// almost directly beneath the camera, so a small absolute difference is a large ratio.
  TEST_METHOD(TheGroundDistanceRatioIsNotMonotonic)
  {
    const float atFloor = Outpost::TopToCenterStretch(Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE));
    const float atCeiling = Outpost::TopToCenterStretch(Outpost::PitchForDistance(Outpost::MAXIMUM_CAMERA_DISTANCE));

    Assert::AreEqual(3.27f, atFloor, 0.01f);
    Assert::AreEqual(5.33f, atCeiling, 0.01f);
    Assert::IsTrue(atCeiling > atFloor, L"the ground-distance ratio has become monotonic; section 5's note is stale");
  }

  /// **THE MEASURE THAT ACTUALLY BOUNDS A TAP IS MONOTONIC, AND IS WORST AT THE FLOOR** -- which is
  /// what section 5's claim is true of. World units per pixel goes as one over the sine squared of the
  /// depression, so the top edge covers 8.3 times as much plane as the center does at 30 degrees and
  /// only 1.21 times as much at 85.
  TEST_METHOD(ThePixelStretchIsMonotonicAndWorstAtTheFloor)
  {
    const float atFloor = Outpost::TopToCenterPixelStretch(Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE));
    const float atCeiling = Outpost::TopToCenterPixelStretch(Outpost::PitchForDistance(Outpost::MAXIMUM_CAMERA_DISTANCE));

    Assert::AreEqual(8.29f, atFloor, 0.01f);
    Assert::AreEqual(1.21f, atCeiling, 0.01f);

    float previous = 1000.0f;
    for (float distance = Outpost::MINIMUM_CAMERA_DISTANCE; distance <= Outpost::MAXIMUM_CAMERA_DISTANCE; distance += 250.0f)
    {
      const float stretch = Outpost::TopToCenterPixelStretch(Outpost::PitchForDistance(distance));
      Assert::IsTrue(stretch <= previous + 0.0001f, L"pulling the camera out made a pixel cover more plane");
      previous = stretch;
    }
  }
};

/// ADR-018's solve, driven as a manipulation. **The ground sticks to the finger.**
TEST_CLASS(TheCameraGesture)
{
public:
  /// A one-finger drag: the world point under the finger stays under the finger.
  TEST_METHOD(TheGroundSticksToTheFinger)
  {
    const Outpost::CameraPose start = PoseAt(6000.0f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, CENTER_X, CENTER_Y));

    const Outpost::CameraPose moved = gesture.Update(Manipulation(200.0f, -140.0f, 1.0f, 0.0f), ASPECT);

    // Project the anchor at the new pose: it has to land where the finger now is.
    float screenX = 0.0f;
    float screenY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(moved, ASPECT, gesture.AnchorWorldX(), gesture.AnchorWorldY(), screenX, screenY));

    float wantX = 0.0f;
    float wantY = 0.0f;
    Outpost::AuthoredToNormalized(CENTER_X + 200.0f, CENTER_Y - 140.0f, 1440.0f, 960.0f, wantX, wantY);

    Assert::AreEqual(wantX, screenX, 0.002f, L"the ground slid horizontally under the finger");
    Assert::AreEqual(wantY, screenY, 0.002f, L"the ground slid vertically under the finger");
  }

  /// **NO DRIFT OVER A LONG GESTURE**, which `Interface.md` section 7 names as the failure this model
  /// actually has. Sixty updates along a path, and the anchor is still under the finger at the end.
  TEST_METHOD(ThereIsNoDriftOverALongGesture)
  {
    const Outpost::CameraPose start = PoseAt(9000.0f, 0.4f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, CENTER_X, CENTER_Y));

    Outpost::CameraPose pose = start;
    float panX = 0.0f;
    float panY = 0.0f;
    for (int step = 0; step < 60; ++step)
    {
      panX += 4.0f;
      panY += 2.5f;
      pose = gesture.Update(Manipulation(panX, panY, 1.0f, 0.0f), ASPECT);
    }

    float screenX = 0.0f;
    float screenY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(pose, ASPECT, gesture.AnchorWorldX(), gesture.AnchorWorldY(), screenX, screenY));

    float wantX = 0.0f;
    float wantY = 0.0f;
    Outpost::AuthoredToNormalized(CENTER_X + panX, CENTER_Y + panY, 1440.0f, 960.0f, wantX, wantY);

    Assert::AreEqual(wantX, screenX, 0.002f, L"the camera drifted over a long gesture");
    Assert::AreEqual(wantY, screenY, 0.002f);
  }

  /// **TWO FINGERS PAN IDENTICALLY TO ONE**, which section 3 asserts and section 5 says is true by
  /// construction here: a one-finger drag is the same solve with no scale and no rotation.
  TEST_METHOD(TwoFingersPanIdenticallyToOne)
  {
    const Outpost::CameraPose start = PoseAt(7000.0f, 1.1f);

    Outpost::CameraGesture one;
    Assert::IsTrue(one.Begin(start, ASPECT, 500.0f, 600.0f));
    Neuron::GatedManipulation single = Manipulation(-90.0f, 60.0f, 1.0f, 0.0f);
    single.contactCount = 1;
    const Outpost::CameraPose fromOne = one.Update(single, ASPECT);

    Outpost::CameraGesture two;
    Assert::IsTrue(two.Begin(start, ASPECT, 500.0f, 600.0f));
    const Outpost::CameraPose fromTwo = two.Update(Manipulation(-90.0f, 60.0f, 1.0f, 0.0f), ASPECT);

    Assert::AreEqual(fromOne.focusX, fromTwo.focusX, 0.001f);
    Assert::AreEqual(fromOne.focusY, fromTwo.focusY, 0.001f);
    Assert::AreEqual(fromOne.distance, fromTwo.distance, 0.001f);
  }

  /// **FINGERS APART IS ZOOM IN.** R21 names this sign as a thing a package can hide and a test
  /// cannot, so it is pinned rather than reasoned about.
  TEST_METHOD(FingersApartZoomsIn)
  {
    const Outpost::CameraPose start = PoseAt(8000.0f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, CENTER_X, CENTER_Y));

    const Outpost::CameraPose closer = gesture.Update(Manipulation(0.0f, 0.0f, 2.0f, 0.0f), ASPECT);
    Assert::AreEqual(4000.0f, closer.distance, 0.01f);

    const Outpost::CameraPose further = gesture.Update(Manipulation(0.0f, 0.0f, 0.5f, 0.0f), ASPECT);
    Assert::AreEqual(16000.0f, further.distance, 0.01f);
  }

  /// A pinch at the center of the screen zooms about the anchor, so the point under the fingers does
  /// not move. That is the whole of "orbit turns about the anchor rather than the focus for free".
  TEST_METHOD(APinchKeepsTheAnchorUnderTheFingers)
  {
    const Outpost::CameraPose start = PoseAt(9000.0f, 0.7f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, 400.0f, 300.0f));

    const Outpost::CameraPose zoomed = gesture.Update(Manipulation(0.0f, 0.0f, 2.5f, 0.0f), ASPECT);

    float screenX = 0.0f;
    float screenY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(zoomed, ASPECT, gesture.AnchorWorldX(), gesture.AnchorWorldY(), screenX, screenY));

    float wantX = 0.0f;
    float wantY = 0.0f;
    Outpost::AuthoredToNormalized(400.0f, 300.0f, 1440.0f, 960.0f, wantX, wantY);
    Assert::AreEqual(wantX, screenX, 0.002f);
    Assert::AreEqual(wantY, screenY, 0.002f);
  }

  /// A rotation orbits, and the anchor stays put through it.
  TEST_METHOD(ARotationOrbitsAboutTheAnchor)
  {
    const Outpost::CameraPose start = PoseAt(9000.0f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, 900.0f, 400.0f));

    const Outpost::CameraPose turned = gesture.Update(Manipulation(0.0f, 0.0f, 1.0f, 30.0f), ASPECT);

    Assert::AreEqual(-30.0f, Degrees(turned.headingRadians - start.headingRadians), 0.01f,
                     L"a clockwise finger rotation must carry the map clockwise, so the camera turns the other way");

    float screenX = 0.0f;
    float screenY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(turned, ASPECT, gesture.AnchorWorldX(), gesture.AnchorWorldY(), screenX, screenY));
    float wantX = 0.0f;
    float wantY = 0.0f;
    Outpost::AuthoredToNormalized(900.0f, 400.0f, 1440.0f, 960.0f, wantX, wantY);
    Assert::AreEqual(wantX, screenX, 0.002f);
    Assert::AreEqual(wantY, screenY, 0.002f);
  }

  /// **THE GATE'S DEADZONES REACH THE CAMERA.** A pure orbit arrives with a scale of exactly one, so
  /// it does not creep the zoom -- and since pitch is coupled to zoom, that is what stops an orbit
  /// silently re-pitching the camera.
  TEST_METHOD(APureOrbitDoesNotCreepTheZoom)
  {
    const Outpost::CameraPose start = PoseAt(9000.0f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, CENTER_X, CENTER_Y));

    const Outpost::CameraPose turned = gesture.Update(Manipulation(0.0f, 0.0f, 1.0f, 45.0f), ASPECT);
    Assert::AreEqual(start.distance, turned.distance, 0.0001f);
    Assert::AreEqual(Outpost::PitchForDistance(start.distance), Outpost::PitchForDistance(turned.distance), 0.0001f);
  }

  /// **THE PAN IS MEASURED FROM THE CROSSING, SO ENGAGING IT MOVES NOTHING.** A gate that has just
  /// crossed the tap slop reports a pan of zero, and the camera must therefore be exactly where it
  /// started -- not sixteen pixels away.
  TEST_METHOD(EngagingThePanMovesNothing)
  {
    const Outpost::CameraPose start = PoseAt(6000.0f, 0.3f);
    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, CENTER_X, CENTER_Y));

    const Outpost::CameraPose atCrossing = gesture.Update(Manipulation(0.0f, 0.0f, 1.0f, 0.0f), ASPECT);
    Assert::AreEqual(start.focusX, atCrossing.focusX, 0.01f);
    Assert::AreEqual(start.focusY, atCrossing.focusY, 0.01f);
  }

  /// **AT THE CLAMP THE GROUND STOPS AND THE FINGER SLIDES OVER IT.** The focus is clamped after the
  /// solve, so the anchor slips at the edge of the play area -- correct, and stated because it reads
  /// as a defect the first time it is seen.
  TEST_METHOD(ThePanClampsAtTheCornerAndLetsTheAnchorSlip)
  {
    Outpost::CameraPose start = PoseAt(4000.0f);
    start.focusX = Outpost::PLAY_AREA_HALF_EXTENT_UNITS;
    start.focusY = Outpost::PLAY_AREA_HALF_EXTENT_UNITS;

    Outpost::CameraGesture gesture;
    Assert::IsTrue(gesture.Begin(start, ASPECT, CENTER_X, CENTER_Y));

    // Drag hard toward the corner.
    const Outpost::CameraPose pushed = gesture.Update(Manipulation(-700.0f, 450.0f, 1.0f, 0.0f), ASPECT);

    const float limit = Outpost::PLAY_AREA_HALF_EXTENT_UNITS + Outpost::FOCUS_CLAMP_MARGIN;
    Assert::IsTrue(pushed.focusX <= limit + 0.01f);
    Assert::IsTrue(pushed.focusY <= limit + 0.01f);
    Assert::IsTrue((pushed.focusX >= limit - 0.01f) || (pushed.focusY >= limit - 0.01f), L"the clamp never engaged");
  }

  /// A gesture that never began does nothing, rather than moving a camera from a pose it never saw.
  TEST_METHOD(AnInactiveGestureMovesNothing)
  {
    const Outpost::CameraGesture gesture;
    const Outpost::CameraPose unchanged = gesture.Update(Manipulation(500.0f, 500.0f, 3.0f, 90.0f), ASPECT);
    Assert::AreEqual(0.0f, unchanged.focusX);
    Assert::AreEqual(0.0f, unchanged.focusY);
  }
};

/// ADR-018 decision 5, and what stands in for a compass.
TEST_CLASS(TheCardinalSnap)
{
public:
  TEST_METHOD(AHeadingNearACardinalSnapsToIt)
  {
    const float fiveDegrees = 5.0f * (PI / 180.0f);
    Assert::AreEqual(0.0f, Degrees(Outpost::CameraGesture::Complete(PoseAt(6000.0f, fiveDegrees)).headingRadians), 0.01f);

    const float nearQuarter = (PI / 2.0f) - fiveDegrees;
    Assert::AreEqual(90.0f, Degrees(Outpost::CameraGesture::Complete(PoseAt(6000.0f, nearQuarter)).headingRadians), 0.01f);
  }

  /// **AND ONE THAT IS NOT NEAR ONE IS LEFT ALONE**, because the snap is for a player who was not
  /// deliberately turning the map.
  TEST_METHOD(AHeadingAwayFromACardinalIsLeftAlone)
  {
    const float thirty = 30.0f * (PI / 180.0f);
    Assert::AreEqual(30.0f, Degrees(Outpost::CameraGesture::Complete(PoseAt(6000.0f, thirty)).headingRadians), 0.01f);
  }

  /// Ten degrees is the threshold, so eleven does not snap and nine does.
  TEST_METHOD(TheThresholdIsTenDegrees)
  {
    Assert::AreEqual(10.0f, Outpost::CARDINAL_SNAP_DEGREES);

    const float nine = 9.0f * (PI / 180.0f);
    const float eleven = 11.0f * (PI / 180.0f);
    Assert::AreEqual(0.0f, Degrees(Outpost::CameraGesture::Complete(PoseAt(6000.0f, nine)).headingRadians), 0.01f);
    Assert::AreEqual(11.0f, Degrees(Outpost::CameraGesture::Complete(PoseAt(6000.0f, eleven)).headingRadians), 0.01f);
  }
};

/// **A HOLD ON EMPTY SPACE RECENTERS** (ADR-018), which is the cheap half of the minimap section 5
/// declines.
TEST_CLASS(TheRecenter)
{
public:
  TEST_METHOD(ItGoesToTheSelectionsCentroid)
  {
    const std::vector<float> x{100.0f, 300.0f, 200.0f};
    const std::vector<float> y{0.0f, 0.0f, 600.0f};

    const Outpost::CameraPose moved =
      Outpost::Recenter(PoseAt(6000.0f, 1.2f), Outpost::RecenterRequest{.selectionX = x, .selectionY = y, .stationX = -6000.0f});

    Assert::AreEqual(200.0f, moved.focusX, 0.01f);
    Assert::AreEqual(200.0f, moved.focusY, 0.01f);
  }

  /// **OR TO YOUR STATION WHEN NOTHING IS SELECTED**, which is the case `GameDesign.md` section 7's
  /// defender is in.
  TEST_METHOD(WithNothingSelectedItGoesToYourStation)
  {
    const Outpost::CameraPose moved = Outpost::Recenter(PoseAt(6000.0f), Outpost::RecenterRequest{.stationX = -6000.0f, .stationY = 0.0f});

    Assert::AreEqual(-6000.0f, moved.focusX, 0.01f);
    Assert::AreEqual(0.0f, moved.focusY, 0.01f);
  }

  /// **A RECENTER IS A TRANSLATION.** Turning the camera as well would be two things on one verb, and
  /// the heading is what spatial memory is built on.
  TEST_METHOD(ItLeavesTheHeadingAndTheDistanceAlone)
  {
    const Outpost::CameraPose start = PoseAt(3300.0f, 2.1f);
    const Outpost::CameraPose moved = Outpost::Recenter(start, Outpost::RecenterRequest{.stationX = 1000.0f, .stationY = 2000.0f});

    Assert::AreEqual(start.headingRadians, moved.headingRadians);
    Assert::AreEqual(start.distance, moved.distance);
  }

  /// And it clamps, so a station outside the play area could not park the camera outside it either.
  TEST_METHOD(ItClamps)
  {
    const Outpost::CameraPose moved =
      Outpost::Recenter(PoseAt(6000.0f), Outpost::RecenterRequest{.stationX = 90000.0f, .stationY = -90000.0f});

    const float limit = Outpost::PLAY_AREA_HALF_EXTENT_UNITS + Outpost::FOCUS_CLAMP_MARGIN;
    Assert::AreEqual(limit, moved.focusX, 0.01f);
    Assert::AreEqual(-limit, moved.focusY, 0.01f);
  }
};

} // namespace GameClientTests
