#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
// An effect's draws are pinned, so that one destruction always looks the same and a test sees the same draws every run
// (ADR-026). The expected values come from an independent Python implementation of SplitMix64, which gives the published
// sequence for seed 0: 0xE220A8397B1DCDAF, 0x6E789E6AA1B965F4, 0x06C45D188009454F, 0xF88BB8A8724C81EC.
TEST_CLASS(EffectRandomTests)
{
public:
  TEST_METHOD(DrawsThePublishedSplitMix64Sequence)
  {
    Outpost::EffectRandom zero(0);
    Assert::AreEqual(std::uint64_t{0xE220'A839'7B1D'CDAF}, zero.Next());
    Assert::AreEqual(std::uint64_t{0x6E78'9E6A'A1B9'65F4}, zero.Next());
    Assert::AreEqual(std::uint64_t{0x06C4'5D18'8009'454F}, zero.Next());
    Assert::AreEqual(std::uint64_t{0xF88B'B8A8'724C'81EC}, zero.Next());

    Outpost::EffectRandom other(1'234'567);
    Assert::AreEqual(std::uint64_t{0x599E'D017'FB08'FC85}, other.Next());
    Assert::AreEqual(std::uint64_t{0x2C73'F084'5854'0FA5}, other.Next());
  }

  // A unit draw is the top 24 bits of one draw, which a float holds exactly.
  TEST_METHOD(DrawsThePinnedUnitSequence)
  {
    Outpost::EffectRandom random(42);
    Assert::AreEqual(12'441'394.0f / 16'777'216.0f, random.Unit());
    Assert::AreEqual(2'682'851.0f / 16'777'216.0f, random.Unit());
    Assert::AreEqual(4'674'151.0f / 16'777'216.0f, random.Unit());
  }

  // Signed and Between are the unit draw scaled into their range, one draw each.
  TEST_METHOD(ScalesTheUnitDrawIntoItsRange)
  {
    constexpr float TOLERANCE = 1e-5f;
    const float first = 12'441'394.0f / 16'777'216.0f;
    const float second = 2'682'851.0f / 16'777'216.0f;
    Outpost::EffectRandom random(42);
    Assert::AreEqual(((2.0f * first) - 1.0f) * 10.0f, random.Signed(10.0f), TOLERANCE);
    Assert::AreEqual(3.0f + (4.0f * second), random.Between(3.0f, 7.0f), TOLERANCE);
  }

  TEST_METHOD(StaysInRange)
  {
    Outpost::EffectRandom random(7);
    for (int i = 0; i < 10'000; ++i)
    {
      const float unit = random.Unit();
      Assert::IsTrue(unit >= 0.0f && unit < 1.0f);
      const float signedDraw = random.Signed(2.5f);
      Assert::IsTrue(signedDraw >= -2.5f && signedDraw < 2.5f);
      const float between = random.Between(-4.0f, 9.0f);
      Assert::IsTrue(between >= -4.0f && between < 9.0f);
    }
  }

  // Two generators with one seed draw alike, and a different seed draws otherwise.
  TEST_METHOD(RepeatsFromItsSeed)
  {
    Outpost::EffectRandom first(99);
    Outpost::EffectRandom second(99);
    Outpost::EffectRandom third(100);
    bool differs = false;
    for (int i = 0; i < 16; ++i)
    {
      const std::uint64_t draw = first.Next();
      Assert::AreEqual(draw, second.Next());
      differs = differs || draw != third.Next();
    }
    Assert::IsTrue(differs);
  }
};
} // namespace GameAppTests
