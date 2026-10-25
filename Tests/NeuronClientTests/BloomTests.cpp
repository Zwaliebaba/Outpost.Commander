#include "pch.h"

#include <algorithm>
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
using Extent = Neuron::Bloom::Extent;

void AssertExtent(const Extent& _extent, UINT _widthPixels, UINT _heightPixels, const wchar_t* _what)
{
  Assert::AreEqual(_widthPixels, _extent.widthPixels, _what);
  Assert::AreEqual(_heightPixels, _extent.heightPixels, _what);
}
} // namespace

TEST_CLASS(BloomTests)
{
public:
  // ADR-091: the first level is half the scene, rounded down, and each level after it half the one before.
  TEST_METHOD(HalvesTheSceneLevelByLevel)
  {
    const std::array<Extent, Neuron::Bloom::LEVEL_COUNT> levels = Neuron::Bloom::LevelExtents(1920, 1080);
    AssertExtent(levels[0], 960, 540, L"half the scene");
    AssertExtent(levels[1], 480, 270, L"a quarter");
    AssertExtent(levels[2], 240, 135, L"an eighth");
    AssertExtent(levels[3], 120, 67, L"rounded down");
    AssertExtent(levels[4], 60, 33, L"a thirty-second");
  }

  // Each pixel of the first level covers two by two of the scene's, so an odd row or column of the scene is left out
  // rather than read past the scene's edge.
  TEST_METHOD(RoundsAnOddSizeDown)
  {
    const std::array<Extent, Neuron::Bloom::LEVEL_COUNT> levels = Neuron::Bloom::LevelExtents(2881, 1921);
    AssertExtent(levels[0], 1440, 960, L"rounded down");
    for (UINT level = 1; level < Neuron::Bloom::LEVEL_COUNT; ++level)
    {
      Assert::AreEqual(std::max(1u, levels[level - 1].widthPixels / 2), levels[level].widthPixels, L"half the one before");
      Assert::AreEqual(std::max(1u, levels[level - 1].heightPixels / 2), levels[level].heightPixels, L"half the one before");
    }
  }

  // No level is ever empty, however small the window.
  TEST_METHOD(KeepsEveryLevelAPixelAtLeast)
  {
    for (const Extent& level : Neuron::Bloom::LevelExtents(1, 1))
      AssertExtent(level, 1, 1, L"a pixel");
    const std::array<Extent, Neuron::Bloom::LEVEL_COUNT> thin = Neuron::Bloom::LevelExtents(640, 3);
    AssertExtent(thin[0], 320, 1, L"a row");
    AssertExtent(thin[4], 20, 1, L"still a row");
  }

  // The resolve adds the intensity's share of the bright light, however far it spreads: the intensity over the sum of the
  // levels' weights, 1, spread, spread squared and so on.
  TEST_METHOD(ScalesTheGlowByItsSpread)
  {
    Assert::AreEqual(
      2.0f, Neuron::Bloom::CompositeScale({.thresholdLuminance = 0.35f, .kneeLuminance = 0.15f, .intensity = 2.0f, .spread = 0.0f}), 1e-6f,
      L"the first level alone");
    Assert::AreEqual(
      2.0f / 5.0f, Neuron::Bloom::CompositeScale({.thresholdLuminance = 0.35f, .kneeLuminance = 0.15f, .intensity = 2.0f, .spread = 1.0f}),
      1e-6f, L"every level alike");
    const float spread = 0.6f;
    const float weights = 1.0f + spread + (spread * spread) + std::pow(spread, 3.0f) + std::pow(spread, 4.0f);
    Assert::AreEqual(
      2.0f / weights,
      Neuron::Bloom::CompositeScale({.thresholdLuminance = 0.35f, .kneeLuminance = 0.15f, .intensity = 2.0f, .spread = spread}), 1e-5f,
      L"weighed by the spread");
  }

  // Nothing is added without an intensity, and a spread out of range is held to it.
  TEST_METHOD(AddsNothingWithoutAnIntensity)
  {
    Assert::AreEqual(0.0f, Neuron::Bloom::CompositeScale({}), L"no glow by default");
    Assert::AreEqual(0.0f, Neuron::Bloom::CompositeScale({.intensity = -1.0f, .spread = 0.5f}), L"never a negative glow");
    Assert::AreEqual(Neuron::Bloom::CompositeScale({.intensity = 1.0f, .spread = 1.0f}),
                     Neuron::Bloom::CompositeScale({.intensity = 1.0f, .spread = 3.0f}), L"a spread held at 1");
  }
};
} // namespace NeuronClientTests
