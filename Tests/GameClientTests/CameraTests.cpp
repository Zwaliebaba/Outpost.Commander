#include "pch.h"

#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// The target panel's 3:2 (`Interface.md` section 1, Q6), which is the aspect every case here runs
/// at unless it is specifically about a different one.
constexpr float TARGET_ASPECT = 1440.0f / 960.0f;

constexpr float PI = 3.14159265358979323846f;

[[nodiscard]] constexpr float ToRadians(float _degrees) noexcept
{
  return _degrees * (PI / 180.0f);
}

/// A world-unit tolerance for a round trip. Generous in world units and tight in screen terms: at
/// the far end of the range the frame is 16,000 units tall over 960 authored pixels, so one unit is
/// well under a thousandth of a pixel.
constexpr float WORLD_TOLERANCE = 1.0f;

/// A normalized-screen tolerance. One authored pixel at 1440 across is 0.0014 of the -1..1 range,
/// so this is a fifth of a pixel.
constexpr float SCREEN_TOLERANCE = 0.0003f;

[[nodiscard]] Outpost::CameraPose PoseAt(float _distance, float _headingDegrees = 0.0f) noexcept
{
  Outpost::CameraPose pose;
  pose.focusX = 0.0f;
  pose.focusY = 0.0f;
  pose.headingRadians = ToRadians(_headingDegrees);
  pose.distance = _distance;
  return pose;
}
} // namespace

/// ADR-001's four degrees of freedom, and the coupling that keeps it to four.
TEST_CLASS(CameraCoupling)
{
public:
  TEST_METHOD(PitchIsMonotonicAcrossTheRange)
  {
    float previous = -1.0f;
    for (int step = 0; step <= 20; ++step)
    {
      const float travel = static_cast<float>(step) / 20.0f;
      const float distance =
        Outpost::MINIMUM_CAMERA_DISTANCE + (travel * (Outpost::MAXIMUM_CAMERA_DISTANCE - Outpost::MINIMUM_CAMERA_DISTANCE));
      const float pitch = Outpost::PitchForDistance(distance);
      Assert::IsTrue(pitch >= previous, L"pitch must not fall as the camera pulls back");
      previous = pitch;
    }
  }

  TEST_METHOD(BothEndsOfTheRangeLandOnTheirLimits)
  {
    Assert::AreEqual(ToRadians(Outpost::MINIMUM_PITCH_DEGREES), Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE), 0.0001f);
    Assert::AreEqual(ToRadians(Outpost::MAXIMUM_PITCH_DEGREES), Outpost::PitchForDistance(Outpost::MAXIMUM_CAMERA_DISTANCE), 0.0001f);
  }

  /// `Interface.md` section 5: the floor saturates the pitch and does NOT end the zoom. Read the
  /// coupling the other way and the camera quietly loses its close view.
  TEST_METHOD(ThePitchFloorSaturatesRatherThanEndingTheZoom)
  {
    const float atFloor = Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE);
    const float wellInside = Outpost::PitchForDistance(Outpost::MINIMUM_CAMERA_DISTANCE / 4.0f);
    Assert::AreEqual(atFloor, wellInside, 0.0001f);
    Assert::AreEqual(ToRadians(Outpost::MINIMUM_PITCH_DEGREES), wellInside, 0.0001f);

    // And a closer distance is still a legal distance -- the basis builds and the camera is lower,
    // not refused.
    const Outpost::CameraBasis basis = Outpost::BuildBasis(PoseAt(Outpost::MINIMUM_CAMERA_DISTANCE / 4.0f));
    Assert::IsTrue(basis.eye.z > 0.0f);
    Assert::IsTrue(basis.eye.z < Outpost::MINIMUM_CAMERA_DISTANCE);
  }

  /// THE NUMBER THE FLOOR EXISTS TO BOUND (`Interface.md` section 5). 5.67 camera heights at the
  /// top edge against 1.73 at the center, a 3.3x stretch -- it bounds tap error and ADR-010's
  /// wedge, and it grows without bound if the floor slips.
  TEST_METHOD(TheStretchRatioAtTheFloorIsTheOneTheDesignStates)
  {
    const float floorPitch = ToRadians(Outpost::MINIMUM_PITCH_DEGREES);
    Assert::AreEqual(5.67f, Outpost::GroundDistanceAtScreenTop(floorPitch), 0.01f);
    Assert::AreEqual(1.73f, Outpost::GroundDistanceAtScreenCenter(floorPitch), 0.01f);

    const float ratio = Outpost::GroundDistanceAtScreenTop(floorPitch) / Outpost::GroundDistanceAtScreenCenter(floorPitch);
    Assert::AreEqual(3.3f, ratio, 0.05f);
  }

  /// And the reason the floor is where it is: a few degrees lower and the ratio runs away.
  TEST_METHOD(TheStretchRatioGrowsWithoutBoundBelowTheFloor)
  {
    const float floorRatio = Outpost::GroundDistanceAtScreenTop(ToRadians(30.0f));
    const float lowerRatio = Outpost::GroundDistanceAtScreenTop(ToRadians(25.0f));
    const float lowerStill = Outpost::GroundDistanceAtScreenTop(ToRadians(21.0f));

    Assert::IsTrue(lowerRatio > floorRatio * 1.5f);
    Assert::IsTrue(lowerStill > lowerRatio * 4.0f);
  }

  TEST_METHOD(TheCameraNeverRolls)
  {
    // "Never rolls" as a vector: right stays in the plane at every pitch and heading.
    for (float headingDegrees = 0.0f; headingDegrees < 360.0f; headingDegrees += 37.0f)
    {
      for (float distance = Outpost::MINIMUM_CAMERA_DISTANCE; distance <= Outpost::MAXIMUM_CAMERA_DISTANCE; distance += 4000.0f)
      {
        const Outpost::CameraBasis basis = Outpost::BuildBasis(PoseAt(distance, headingDegrees));
        Assert::AreEqual(0.0f, basis.right.z, 0.0001f);
      }
    }
  }

  /// Sixteen until 2026-09-24, when the owner halved the opening distance and the near end with it.
  TEST_METHOD(TheZoomRangeIsTheThirtyTwoTimesTheDesignQuotes)
  {
    const float range = Outpost::MAXIMUM_CAMERA_DISTANCE / Outpost::MINIMUM_CAMERA_DISTANCE;
    Assert::AreEqual(32.14f, range, 0.1f);
  }
};

/// ADR-001 in code: a ray onto the plane, and back.
TEST_CLASS(CameraProjection)
{
public:
  TEST_METHOD(AScreenPointRoundTripsAtSeveralPitches)
  {
    const float distances[] = {Outpost::MINIMUM_CAMERA_DISTANCE, 4000.0f, 11000.0f, Outpost::MAXIMUM_CAMERA_DISTANCE};
    const float screenPoints[][2] = {{0.0f, 0.0f}, {0.5f, 0.5f}, {-0.8f, -0.6f}, {0.9f, -0.9f}, {0.0f, 0.95f}};

    for (const float distance : distances)
    {
      const Outpost::CameraPose pose = PoseAt(distance, 23.0f);
      for (const auto& point : screenPoints)
      {
        float worldX = 0.0f;
        float worldY = 0.0f;
        Assert::IsTrue(Outpost::ScreenToPlane(pose, TARGET_ASPECT, point[0], point[1], worldX, worldY));

        float backX = 0.0f;
        float backY = 0.0f;
        Assert::IsTrue(Outpost::PlaneToScreen(pose, TARGET_ASPECT, worldX, worldY, backX, backY));

        Assert::AreEqual(point[0], backX, SCREEN_TOLERANCE);
        Assert::AreEqual(point[1], backY, SCREEN_TOLERANCE);
      }
    }
  }

  TEST_METHOD(TheCenterOfTheScreenIsTheFocus)
  {
    Outpost::CameraPose pose = PoseAt(9000.0f, 61.0f);
    pose.focusX = 1234.0f;
    pose.focusY = -567.0f;

    float worldX = 0.0f;
    float worldY = 0.0f;
    Assert::IsTrue(Outpost::ScreenToPlane(pose, TARGET_ASPECT, 0.0f, 0.0f, worldX, worldY));

    Assert::AreEqual(pose.focusX, worldX, WORLD_TOLERANCE);
    Assert::AreEqual(pose.focusY, worldY, WORLD_TOLERANCE);
  }

  /// The top of the frame is further away than the bottom -- which is the stretch the pitch floor
  /// bounds, seen from the other side.
  TEST_METHOD(TheTopOfTheFrameIsFurtherThanTheBottom)
  {
    const Outpost::CameraPose pose = PoseAt(Outpost::MINIMUM_CAMERA_DISTANCE);

    float topX = 0.0f;
    float topY = 0.0f;
    float bottomX = 0.0f;
    float bottomY = 0.0f;
    Assert::IsTrue(Outpost::ScreenToPlane(pose, TARGET_ASPECT, 0.0f, 1.0f, topX, topY));
    Assert::IsTrue(Outpost::ScreenToPlane(pose, TARGET_ASPECT, 0.0f, -1.0f, bottomX, bottomY));

    // Heading zero looks along +x, so the top of the frame is the larger x.
    Assert::IsTrue(topX > bottomX);
  }

  /// THE ROUND TRIP CANNOT SEE A MIRRORED BASIS -- it inverts twice and agrees with itself. So the
  /// handedness is pinned directly: with z up and the camera looking along +x, the camera's right is
  /// the world's -y, and a point over there belongs at a POSITIVE screen x.
  TEST_METHOD(TheCameraIsRightHandedAndNotMirrored)
  {
    const Outpost::CameraPose pose = PoseAt(8000.0f);
    const Outpost::CameraBasis basis = Outpost::BuildBasis(pose);

    Assert::AreEqual(0.0f, basis.right.x, 0.0001f);
    Assert::AreEqual(-1.0f, basis.right.y, 0.0001f);

    // Up is genuinely up: the camera is above the plane looking down, so its up vector has a
    // positive z. A flipped cross product puts this below zero and the frame upside down.
    Assert::IsTrue(basis.up.z > 0.0f);

    float screenX = 0.0f;
    float screenY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(pose, TARGET_ASPECT, pose.focusX, pose.focusY - 500.0f, screenX, screenY));
    Assert::IsTrue(screenX > 0.0f, L"the world's -y is the camera's right at heading zero");
  }

  TEST_METHOD(AnAuthoredPixelBecomesANormalizedPoint)
  {
    float screenX = 0.0f;
    float screenY = 0.0f;

    Outpost::AuthoredToNormalized(720.0f, 480.0f, 1440.0f, 960.0f, screenX, screenY);
    Assert::AreEqual(0.0f, screenX, 0.0001f);
    Assert::AreEqual(0.0f, screenY, 0.0001f);

    // Top left of the authored frame is -1, +1: the y axis flips.
    Outpost::AuthoredToNormalized(0.0f, 0.0f, 1440.0f, 960.0f, screenX, screenY);
    Assert::AreEqual(-1.0f, screenX, 0.0001f);
    Assert::AreEqual(1.0f, screenY, 0.0001f);
  }
};

/// ADR-018, and its one property: project the anchor and it lands on the centroid.
TEST_CLASS(AnchorSolve)
{
public:
  TEST_METHOD(TheAnchorLandsUnderTheContactForOneFinger)
  {
    const Outpost::CameraPose starting = PoseAt(8000.0f, 17.0f);

    // Where the finger went down.
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    Assert::IsTrue(Outpost::ScreenToPlane(starting, TARGET_ASPECT, -0.3f, 0.4f, anchorX, anchorY));

    // And where it has moved to. One finger: no scale, no rotation, so the pose is unchanged but
    // for the focus the solve is about to produce.
    const float movedX = 0.55f;
    const float movedY = -0.2f;

    Outpost::CameraPose solved = starting;
    Assert::IsTrue(Outpost::SolveFocusForAnchor(starting, TARGET_ASPECT, anchorX, anchorY, movedX, movedY, solved.focusX, solved.focusY));

    float projectedX = 0.0f;
    float projectedY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(solved, TARGET_ASPECT, anchorX, anchorY, projectedX, projectedY));
    Assert::AreEqual(movedX, projectedX, SCREEN_TOLERANCE);
    Assert::AreEqual(movedY, projectedY, SCREEN_TOLERANCE);
  }

  /// Two contacts: scale has become a distance and therefore a pitch, rotation a heading, and the
  /// solve runs at the NEW pose. That ordering is ADR-018 decision 3 and it is what removes the
  /// feedback loop between zoom and pitch.
  TEST_METHOD(TheAnchorLandsUnderTheCentroidWithScaleAndRotation)
  {
    const Outpost::CameraPose starting = PoseAt(12000.0f, 40.0f);

    float anchorX = 0.0f;
    float anchorY = 0.0f;
    Assert::IsTrue(Outpost::ScreenToPlane(starting, TARGET_ASPECT, 0.2f, -0.35f, anchorX, anchorY));

    // The pinch and the twist, applied before the solve.
    Outpost::CameraPose posed = starting;
    posed.distance = Outpost::ClampDistance(starting.distance / 1.8f);
    posed.headingRadians = starting.headingRadians + ToRadians(23.0f);

    const float centroidX = -0.4f;
    const float centroidY = 0.6f;

    Assert::IsTrue(Outpost::SolveFocusForAnchor(posed, TARGET_ASPECT, anchorX, anchorY, centroidX, centroidY, posed.focusX, posed.focusY));

    float projectedX = 0.0f;
    float projectedY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(posed, TARGET_ASPECT, anchorX, anchorY, projectedX, projectedY));
    Assert::AreEqual(centroidX, projectedX, SCREEN_TOLERANCE);
    Assert::AreEqual(centroidY, projectedY, SCREEN_TOLERANCE);
  }

  /// THE FAILURE THIS MODEL ACTUALLY HAS is drift over a long gesture, which is why the anchor is
  /// taken once at ManipulationStarted and never recomputed (ADR-018 decision 1). This drives two
  /// hundred updates through the solve and the anchor must still be under the finger at the end.
  TEST_METHOD(ThereIsNoDriftOverALongGesture)
  {
    const Outpost::CameraPose starting = PoseAt(9000.0f, 12.0f);

    float anchorX = 0.0f;
    float anchorY = 0.0f;
    Assert::IsTrue(Outpost::ScreenToPlane(starting, TARGET_ASPECT, 0.0f, 0.0f, anchorX, anchorY));

    Outpost::CameraPose pose = starting;
    for (int step = 1; step <= 200; ++step)
    {
      const float phase = static_cast<float>(step) * 0.05f;
      const float centroidX = 0.7f * std::sin(phase);
      const float centroidY = 0.6f * std::cos(phase * 0.7f);

      pose.distance = Outpost::ClampDistance(9000.0f + (3000.0f * std::sin(phase * 0.3f)));
      pose.headingRadians = starting.headingRadians + (ToRadians(30.0f) * std::sin(phase * 0.2f));

      Assert::IsTrue(Outpost::SolveFocusForAnchor(pose, TARGET_ASPECT, anchorX, anchorY, centroidX, centroidY, pose.focusX, pose.focusY));

      float projectedX = 0.0f;
      float projectedY = 0.0f;
      Assert::IsTrue(Outpost::PlaneToScreen(pose, TARGET_ASPECT, anchorX, anchorY, projectedX, projectedY));
      Assert::AreEqual(centroidX, projectedX, SCREEN_TOLERANCE);
      Assert::AreEqual(centroidY, projectedY, SCREEN_TOLERANCE);
    }
  }

  /// ADR-018 decision 6: the clamp is applied AFTER the solve, and at the edge the anchor slips
  /// while the ground stands still. That reads as a defect the first time it is seen.
  TEST_METHOD(AtTheClampTheGroundStopsAndTheAnchorSlips)
  {
    Outpost::CameraPose pose = PoseAt(6000.0f);
    pose.focusX = Outpost::PLAY_AREA_HALF_EXTENT_UNITS;

    float anchorX = 0.0f;
    float anchorY = 0.0f;
    Assert::IsTrue(Outpost::ScreenToPlane(pose, TARGET_ASPECT, 0.0f, 0.0f, anchorX, anchorY));

    // Drag toward the edge hard enough that the solve wants to go well past the margin. Down the
    // SCREEN, because at heading zero the screen's y axis is the world's x -- the camera looks along
    // +x, so pulling the anchor toward the bottom of the frame walks the focus that way.
    Assert::IsTrue(Outpost::SolveFocusForAnchor(pose, TARGET_ASPECT, anchorX, anchorY, 0.0f, -0.9f, pose.focusX, pose.focusY));
    const float unclampedX = pose.focusX;
    Outpost::ClampFocus(pose);

    const float limit = Outpost::PLAY_AREA_HALF_EXTENT_UNITS + Outpost::FOCUS_CLAMP_MARGIN;
    Assert::IsTrue(unclampedX > limit, L"the solve should have wanted to go past the margin");
    Assert::AreEqual(limit, pose.focusX, 0.001f);

    // And the anchor is no longer under the finger -- it slipped, which is the correct behavior.
    float projectedX = 0.0f;
    float projectedY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(pose, TARGET_ASPECT, anchorX, anchorY, projectedX, projectedY));
    Assert::IsTrue(std::fabs(projectedY - (-0.9f)) > SCREEN_TOLERANCE);
  }

  TEST_METHOD(TheClampHoldsAtEveryCorner)
  {
    const float limit = Outpost::PLAY_AREA_HALF_EXTENT_UNITS + Outpost::FOCUS_CLAMP_MARGIN;
    const float corners[][2] = {{99999.0f, 99999.0f}, {-99999.0f, 99999.0f}, {99999.0f, -99999.0f}, {-99999.0f, -99999.0f}};

    for (const auto& corner : corners)
    {
      Outpost::CameraPose pose = PoseAt(6000.0f);
      pose.focusX = corner[0];
      pose.focusY = corner[1];
      Outpost::ClampFocus(pose);

      Assert::AreEqual(limit, std::fabs(pose.focusX), 0.001f);
      Assert::AreEqual(limit, std::fabs(pose.focusY), 0.001f);
    }
  }

  TEST_METHOD(AFocusInsideTheAreaIsLeftAlone)
  {
    Outpost::CameraPose pose = PoseAt(6000.0f);
    pose.focusX = 1000.0f;
    pose.focusY = -2000.0f;
    Outpost::ClampFocus(pose);

    Assert::AreEqual(1000.0f, pose.focusX, 0.001f);
    Assert::AreEqual(-2000.0f, pose.focusY, 0.001f);
  }
};

/// ADR-018 decision 5, which stands in for the compass this design declines.
TEST_CLASS(CardinalSnap)
{
public:
  TEST_METHOD(AHeadingNearACardinalSnapsToIt)
  {
    Assert::AreEqual(0.0f, Outpost::SnapHeadingToCardinal(ToRadians(4.0f)), 0.0001f);
    Assert::AreEqual(ToRadians(90.0f), Outpost::SnapHeadingToCardinal(ToRadians(83.0f)), 0.0001f);
    Assert::AreEqual(ToRadians(180.0f), Outpost::SnapHeadingToCardinal(ToRadians(187.0f)), 0.0001f);
    Assert::AreEqual(ToRadians(270.0f), Outpost::SnapHeadingToCardinal(ToRadians(263.0f)), 0.0001f);
  }

  /// Just under a full turn snaps to zero rather than to a full turn, or the heading would creep by
  /// a turn every time a player span the map.
  TEST_METHOD(JustUnderAFullTurnSnapsToZero)
  {
    Assert::AreEqual(0.0f, Outpost::SnapHeadingToCardinal(ToRadians(356.0f)), 0.0001f);
  }

  TEST_METHOD(ADeliberateDiagonalIsLeftAlone)
  {
    // A player holding the map at 45 degrees meant it, and 45 is nowhere near a cardinal.
    Assert::AreEqual(ToRadians(45.0f), Outpost::SnapHeadingToCardinal(ToRadians(45.0f)), 0.0001f);
    Assert::AreEqual(ToRadians(200.0f), Outpost::SnapHeadingToCardinal(ToRadians(200.0f)), 0.0001f);
  }

  /// The threshold has to be wider than the eight-degree rotation deadzone next door, or a rotation
  /// that barely engaged would not be tidied up by the snap.
  TEST_METHOD(TheThresholdClearsTheRotationDeadzone)
  {
    Assert::IsTrue(Outpost::CARDINAL_SNAP_DEGREES > Neuron::ROTATION_DEADZONE_DEGREES);
  }
};

} // namespace GameClientTests
