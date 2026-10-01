#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Star = Neuron::StarPipeline::Star;

constexpr float TOLERANCE = 1e-4f;
constexpr float WIDE_SCREEN = 16.0f / 9.0f;
constexpr float LAPTOP_SCREEN = 16.0f / 10.0f;

float Dot(const DirectX::XMFLOAT3& _a, const DirectX::XMFLOAT3& _b)
{
  return (_a.x * _b.x) + (_a.y * _b.y) + (_a.z * _b.z);
}

float Radians(float _degrees)
{
  return _degrees * std::numbers::pi_v<float> / 180.0f;
}

// How many stars lie within _radians of a direction.
size_t CountNear(const std::vector<Star>& _stars, const DirectX::XMFLOAT3& _toward, float _radians)
{
  const float limit = std::cos(_radians);
  return static_cast<size_t>(std::ranges::count_if(_stars, [&](const Star& _star) { return Dot(_star.direction, _toward) >= limit; }));
}

// Where a direction at infinity shows on the screen, from -1 to 1 on both axes, as StarVS.hlsl puts it; the test fails
// if it is behind the camera.
DirectX::XMFLOAT2 ScreenPoint(const Outpost::Camera& _camera, float _aspectRatio, const DirectX::XMFLOAT3& _direction)
{
  const DirectX::XMFLOAT4X4 viewProjection = _camera.ViewProjection(_aspectRatio);
  DirectX::XMFLOAT4 clip;
  DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(_direction.x, _direction.y, _direction.z, 0.0f),
                                                            DirectX::XMLoadFloat4x4(&viewProjection)));
  if (clip.w <= 0.0f)
    Assert::Fail(L"The direction is behind the camera.");
  return {clip.x / clip.w, clip.y / clip.w};
}
} // namespace

TEST_CLASS(StarfieldTests)
{
public:
  TEST_METHOD(IsTheSameEveryTime)
  {
    const std::vector<Star> first = Outpost::BuildStarfield();
    const std::vector<Star> second = Outpost::BuildStarfield();
    Assert::AreEqual(first.size(), second.size());
    Assert::AreEqual(0, std::memcmp(first.data(), second.data(), first.size() * sizeof(Star)));
  }

  // Enough stars for the band to read as made of them, and few enough to stay one cheap draw.
  TEST_METHOD(HasStarsEnoughForABand)
  {
    const size_t count = Outpost::BuildStarfield().size();
    Assert::IsTrue(count > 100'000);
    Assert::IsTrue(count < 400'000);
  }

  // Every star is a unit direction, a few pixels across and a pale color no brighter than an exhaust.
  TEST_METHOD(DrawsEveryStarSmallAndDim)
  {
    for (const Star& star : Outpost::BuildStarfield())
    {
      Assert::AreEqual(1.0f, std::sqrt(Dot(star.direction, star.direction)), TOLERANCE);
      Assert::IsTrue(star.spreadPixels >= 0.7f - TOLERANCE && star.spreadPixels <= 1.6f + TOLERANCE);
      for (const float channel : {star.color.x, star.color.y, star.color.z})
        Assert::IsTrue(channel > 0.0f && channel <= 0.75f + TOLERANCE);
    }
  }

  TEST_METHOD(GivesTheGalaxyAFrameAtRightAngles)
  {
    const Outpost::GalacticFrame frame = Outpost::StarfieldGalaxy();
    Assert::AreEqual(1.0f, std::sqrt(Dot(frame.center, frame.center)), TOLERANCE);
    Assert::AreEqual(1.0f, std::sqrt(Dot(frame.pole, frame.pole)), TOLERANCE);
    Assert::AreEqual(0.0f, Dot(frame.center, frame.pole), TOLERANCE);
  }

  // A band, not a scatter: most stars lie close to the galaxy's plane, which is a sixth of the sky within 10 degrees.
  TEST_METHOD(GathersMostStarsIntoTheBand)
  {
    const std::vector<Star> stars = Outpost::BuildStarfield();
    const Outpost::GalacticFrame frame = Outpost::StarfieldGalaxy();
    const float limit = std::sin(Radians(10.0f));
    const auto inBand =
      std::ranges::count_if(stars, [&](const Star& _star) { return std::abs(Dot(_star.direction, frame.pole)) <= limit; });
    Assert::IsTrue(static_cast<float>(inBand) > 0.8f * static_cast<float>(stars.size()));
  }

  // The bulge: stars crowd toward the center far more than toward the opposite side of the band.
  TEST_METHOD(CrowdsTheStarsTowardTheCenter)
  {
    const std::vector<Star> stars = Outpost::BuildStarfield();
    const Outpost::GalacticFrame frame = Outpost::StarfieldGalaxy();
    const DirectX::XMFLOAT3 anticenter{-frame.center.x, -frame.center.y, -frame.center.z};
    Assert::IsTrue(CountNear(stars, frame.center, Radians(15.0f)) > 5 * CountNear(stars, anticenter, Radians(15.0f)));
  }

  // The camera only sees the lower half of the sky, and the galaxy's center is placed where the default view sees it
  // at every zoom, on a wide screen and a laptop's.
  TEST_METHOD(ShowsTheCenterInTheDefaultViewAtEveryZoom)
  {
    const Outpost::GalacticFrame frame = Outpost::StarfieldGalaxy();
    const Outpost::CameraSettings settings = Outpost::LoadCameraSettings(ReadRepositoryAssetText("Camera.json"));
    for (const float notches : {0.0f, 100.0f, -100.0f})
    {
      Outpost::Camera camera(settings);
      camera.Zoom(notches);
      for (const float aspectRatio : {WIDE_SCREEN, LAPTOP_SCREEN})
      {
        const DirectX::XMFLOAT2 point = ScreenPoint(camera, aspectRatio, frame.center);
        Assert::IsTrue(std::abs(point.x) < 0.9f && std::abs(point.y) < 0.9f);
      }
    }
  }
};
} // namespace GameAppTests
