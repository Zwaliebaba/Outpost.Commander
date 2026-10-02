#include "pch.h"

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

float Dot(const DirectX::XMFLOAT3& _a, const DirectX::XMFLOAT3& _b)
{
  return (_a.x * _b.x) + (_a.y * _b.y) + (_a.z * _b.z);
}

bool SameStars(const std::vector<Star>& _first, const std::vector<Star>& _second)
{
  return _first.size() == _second.size() && std::memcmp(_first.data(), _second.data(), _first.size() * sizeof(Star)) == 0;
}

void CheckDirectionAndColor(const Star& _star)
{
  Assert::AreEqual(1.0f, std::sqrt(Dot(_star.direction, _star.direction)), TOLERANCE);
  for (const float channel : {_star.color.x, _star.color.y, _star.color.z})
    Assert::IsTrue(channel > 0.0f && channel <= 0.75f + TOLERANCE);
}
} // namespace

TEST_CLASS(StarfieldTests)
{
public:
  TEST_METHOD(IsTheSameEveryTime)
  {
    const Outpost::Starfield first = Outpost::BuildStarfield();
    const Outpost::Starfield second = Outpost::BuildStarfield();
    Assert::IsTrue(SameStars(first.points, second.points));
    Assert::IsTrue(SameStars(first.bursts, second.bursts));
  }

  // A few thousand points on the screen at a time, and only the brightest couple of dozen of the whole sky as crosses.
  TEST_METHOD(DrawsTheBrightestFewAsBursts)
  {
    const Outpost::Starfield sky = Outpost::BuildStarfield();
    Assert::AreEqual(size_t{50'000}, sky.points.size() + sky.bursts.size());
    Assert::AreEqual(size_t{25}, sky.bursts.size());
  }

  // Every point is a unit direction, a few pixels across and a pale color no brighter than an exhaust.
  TEST_METHOD(DrawsEveryPointSmallAndDim)
  {
    for (const Star& star : Outpost::BuildStarfield().points)
    {
      CheckDirectionAndColor(star);
      Assert::IsTrue(star.radiusPixels >= 2.1f - TOLERANCE && star.radiusPixels <= 4.8f + TOLERANCE);
    }
  }

  // Every burst is larger than any point, so its cross's arms reach past the points, and small, so that it is a fine
  // mark rather than a flare (ADR-028).
  TEST_METHOD(DrawsEveryBurstLargerThanAPoint)
  {
    const Outpost::Starfield sky = Outpost::BuildStarfield();
    const float largestPoint = std::ranges::max(sky.points, {}, &Star::radiusPixels).radiusPixels;
    for (const Star& star : sky.bursts)
    {
      CheckDirectionAndColor(star);
      Assert::IsTrue(star.radiusPixels > largestPoint);
      Assert::IsTrue(star.radiusPixels <= 16.0f + TOLERANCE);
    }
  }

  // No band and no clump: each of the six caps of 30 degrees round the axes holds the share of the stars its area is,
  // (1 - cos 30°) / 2 of the sky, within 10%.
  TEST_METHOD(SpreadsTheStarsEvenlyOverTheSky)
  {
    const std::vector<Star> stars = Outpost::BuildStarfield().points;
    const float limit = std::cos(std::numbers::pi_v<float> / 6.0f);
    const float expected = static_cast<float>(stars.size()) * (1.0f - limit) / 2.0f;
    const std::array<DirectX::XMFLOAT3, 6> axes{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    for (const DirectX::XMFLOAT3& axis : axes)
    {
      const auto inCap = std::ranges::count_if(stars, [&](const Star& _star) { return Dot(_star.direction, axis) >= limit; });
      Assert::AreEqual(expected, static_cast<float>(inCap), 0.1f * expected);
    }
  }
};
} // namespace GameAppTests
