#include "pch.h"

#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr float TARGET_ASPECT = 1440.0f / 960.0f;
constexpr float PI = 3.14159265358979323846f;

[[nodiscard]] constexpr float ToRadians(float _degrees) noexcept
{
  return _degrees * (PI / 180.0f);
}

/// A row-vector times a row-major 4x4, which is the convention `Camera.h` states and the shader is
/// declared `row_major` to match. Written out here rather than shared, so that a test of the matrix
/// does not depend on the same multiply the renderer uses.
struct Clip
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;
};

[[nodiscard]] Clip Transform(const Outpost::Matrix4& _m, float _x, float _y, float _z)
{
  return Clip{(_x * _m.m[0]) + (_y * _m.m[4]) + (_z * _m.m[8]) + _m.m[12], (_x * _m.m[1]) + (_y * _m.m[5]) + (_z * _m.m[9]) + _m.m[13],
              (_x * _m.m[2]) + (_y * _m.m[6]) + (_z * _m.m[10]) + _m.m[14], (_x * _m.m[3]) + (_y * _m.m[7]) + (_z * _m.m[11]) + _m.m[15]};
}

[[nodiscard]] Outpost::Matrix4 Multiply(const Outpost::Matrix4& _a, const Outpost::Matrix4& _b)
{
  Outpost::Matrix4 out;
  for (int row = 0; row < 4; ++row)
  {
    for (int column = 0; column < 4; ++column)
    {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k)
      {
        sum += _a.m[(row * 4) + k] * _b.m[(k * 4) + column];
      }
      out.m[(row * 4) + column] = sum;
    }
  }
  return out;
}

[[nodiscard]] Outpost::CameraPose PoseAt(float _distance, float _headingDegrees = 0.0f)
{
  Outpost::CameraPose pose;
  pose.headingRadians = ToRadians(_headingDegrees);
  pose.distance = _distance;
  return pose;
}
} // namespace

/// M0.21b: the transform the world pass draws through, and the property that keeps it honest.
TEST_CLASS(WorldTransform)
{
public:
  TEST_METHOD(AnEntityAtTheFocusLandsAtTheCenterOfTheFrame)
  {
    for (const float distance : {Outpost::MINIMUM_CAMERA_DISTANCE, 6000.0f, 14000.0f, Outpost::MAXIMUM_CAMERA_DISTANCE})
    {
      Outpost::CameraPose pose = PoseAt(distance, 33.0f);
      pose.focusX = 900.0f;
      pose.focusY = -400.0f;

      const Outpost::Matrix4 viewProjection = Outpost::ViewProjection(pose, TARGET_ASPECT);
      const Outpost::Matrix4 entity = Outpost::EntityTransform(pose.focusX, pose.focusY, 0);
      const Clip clip = Transform(Multiply(entity, viewProjection), 0.0f, 0.0f, 0.0f);

      Assert::IsTrue(clip.w > 0.0f, L"the focus point must be in front of the camera");
      Assert::AreEqual(0.0f, clip.x / clip.w, 0.001f);
      Assert::AreEqual(0.0f, clip.y / clip.w, 0.001f);
    }
  }

  /// THE ONE THAT STOPS THE RENDERER AND THE TAP DISAGREEING. M0.20's `PlaneToScreen` is what a tap
  /// is resolved against and this matrix is what a ship is drawn with; if they diverge, a player
  /// taps a ship and misses it, and nothing about the picture looks wrong.
  TEST_METHOD(TheMatrixAgreesWithPlaneToScreen)
  {
    // One authored pixel across 1,440 is 0.00139 of the -1..1 range; this is a fifth of that.
    constexpr float TOLERANCE = 0.0003f;

    const float points[][2] = {{0.0f, 0.0f}, {500.0f, 300.0f}, {-1200.0f, 800.0f}, {2000.0f, -1500.0f}};

    for (const float distance : {2000.0f, 7000.0f, 18000.0f})
    {
      for (const float headingDegrees : {0.0f, 47.0f, 180.0f, 291.0f})
      {
        const Outpost::CameraPose pose = PoseAt(distance, headingDegrees);
        const Outpost::Matrix4 viewProjection = Outpost::ViewProjection(pose, TARGET_ASPECT);

        for (const auto& p : points)
        {
          float screenX = 0.0f;
          float screenY = 0.0f;
          Assert::IsTrue(Outpost::PlaneToScreen(pose, TARGET_ASPECT, p[0], p[1], screenX, screenY));

          const Clip clip = Transform(viewProjection, p[0], p[1], 0.0f);
          Assert::IsTrue(clip.w > 0.0f);

          Assert::AreEqual(screenX, clip.x / clip.w, TOLERANCE);
          Assert::AreEqual(screenY, clip.y / clip.w, TOLERANCE);
        }
      }
    }
  }

  /// An arrow at heading zero points along +X, which is where `Camera.h` says heading zero looks.
  TEST_METHOD(HeadingZeroPointsAlongPositiveX)
  {
    const Outpost::Matrix4 entity = Outpost::EntityTransform(100.0f, 200.0f, 0);
    // The shader's nose vertex.
    const Clip nose = Transform(entity, 30.0f, 0.0f, 0.0f);

    Assert::AreEqual(130.0f, nose.x, 0.001f);
    Assert::AreEqual(200.0f, nose.y, 0.001f);
    Assert::AreEqual(0.0f, nose.z, 0.001f, L"the shape must stay in the plane");
  }

  /// A QUARTER TURN OF THE BINARY ANGLE IS A QUARTER TURN ON SCREEN. This is the assertion an arrow
  /// exists for -- a square would pass every test at every heading.
  TEST_METHOD(AQuarterTurnTurnsTheArrowAQuarterTurn)
  {
    const Outpost::Matrix4 entity = Outpost::EntityTransform(0.0f, 0.0f, Neuron::ANGLE_QUARTER_TURN);
    const Clip nose = Transform(entity, 30.0f, 0.0f, 0.0f);

    Assert::AreEqual(0.0f, nose.x, 0.01f);
    Assert::AreEqual(30.0f, nose.y, 0.01f);
  }

  TEST_METHOD(AHalfTurnReversesIt)
  {
    const Outpost::Matrix4 entity = Outpost::EntityTransform(0.0f, 0.0f, static_cast<Neuron::Angle>(32768));
    const Clip nose = Transform(entity, 30.0f, 0.0f, 0.0f);

    Assert::AreEqual(-30.0f, nose.x, 0.01f);
    Assert::AreEqual(0.0f, nose.y, 0.01f);
  }

  /// The shape stays in the plane at every heading -- ADR-001, and the rotation is about the axis
  /// out of it. A rotation about the wrong axis lifts the arrow off the plane and is invisible from
  /// directly above, which is most of the zoom range.
  TEST_METHOD(TheShapeStaysInThePlaneAtEveryHeading)
  {
    for (int step = 0; step < 16; ++step)
    {
      const auto heading = static_cast<Neuron::Angle>(step * 4096);
      const Outpost::Matrix4 entity = Outpost::EntityTransform(500.0f, -700.0f, heading);
      for (const auto& local : {std::pair{30.0f, 0.0f}, std::pair{-18.0f, 14.0f}, std::pair{-18.0f, -14.0f}})
      {
        const Clip corner = Transform(entity, local.first, local.second, 0.0f);
        Assert::AreEqual(0.0f, corner.z, 0.001f);
      }
    }
  }

  /// Behind the camera is a negative or zero w, which the clip stage discards. Asserted because the
  /// alternative -- a point that projects to a plausible screen position from behind -- is the
  /// classic perspective bug and it draws a ship where there is none.
  TEST_METHOD(AnEntityBehindTheCameraHasNoPositiveDepth)
  {
    const Outpost::CameraPose pose = PoseAt(4000.0f);
    const Outpost::CameraBasis basis = Outpost::BuildBasis(pose);
    const Outpost::Matrix4 viewProjection = Outpost::ViewProjection(pose, TARGET_ASPECT);

    // Well behind the eye, along the reverse of the forward direction, and back on the plane.
    const float behindX = basis.eye.x - (basis.forward.x * 5000.0f);
    const float behindY = basis.eye.y - (basis.forward.y * 5000.0f);

    const Clip clip = Transform(viewProjection, behindX, behindY, 0.0f);
    Assert::IsTrue(clip.w <= 0.0f);

    // And PlaneToScreen refuses it too, which is the two halves agreeing about the hard case as
    // well as the easy one.
    float screenX = 0.0f;
    float screenY = 0.0f;
    Assert::IsFalse(Outpost::PlaneToScreen(pose, TARGET_ASPECT, behindX, behindY, screenX, screenY));
  }

  /// Q39's clip planes, asserted where they are used rather than only where they are stated.
  TEST_METHOD(DepthLandsInsideTheClipRange)
  {
    const Outpost::CameraPose pose = PoseAt(Outpost::MAXIMUM_CAMERA_DISTANCE);
    const Outpost::Matrix4 viewProjection = Outpost::ViewProjection(pose, TARGET_ASPECT);

    const Clip atFocus = Transform(viewProjection, pose.focusX, pose.focusY, 0.0f);
    const float depth = atFocus.z / atFocus.w;

    // Direct3D's convention: zero at the near plane, one at the far plane.
    Assert::IsTrue(depth > 0.0f);
    Assert::IsTrue(depth < 1.0f);

    Assert::AreEqual(50.0f, Outpost::NEAR_CLIP_PLANE, 0.001f);
    Assert::AreEqual(50000.0f, Outpost::FAR_CLIP_PLANE, 0.001f);
  }
};

} // namespace GameClientTests
