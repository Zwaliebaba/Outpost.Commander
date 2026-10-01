#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
// The draws of a seeded generator are pinned: every recorded replay depends on them (ADR-009). The expected values come
// from an independent Python implementation of SplitMix64 and xoshiro256** 1.0, checked against the published vector
// for the state {1, 2, 3, 4}: 11520, 0, 1509978240.
TEST_CLASS(RandomTests)
{
public:
  TEST_METHOD(DrawsThePinnedSequence)
  {
    Neuron::Random zero(0);
    Assert::AreEqual(std::uint64_t{0x99EC5F36CB75F2B4}, zero.NextUInt64());
    Assert::AreEqual(std::uint64_t{0xBF6E1F784956452A}, zero.NextUInt64());
    Assert::AreEqual(std::uint64_t{0x1A5F849D4933E6E0}, zero.NextUInt64());
    Assert::AreEqual(std::uint64_t{0x6AA594F1262D2D2C}, zero.NextUInt64());

    Neuron::Random other(0xDEADBEEF);
    Assert::AreEqual(std::uint64_t{0xC5555444A74D7E83}, other.NextUInt64());
    Assert::AreEqual(std::uint64_t{0x65C30D37B4B16E38}, other.NextUInt64());
  }

  TEST_METHOD(DrawsThePinnedBoundedSequence)
  {
    Neuron::Random random(42);
    constexpr std::array<std::uint32_t, 10> EXPECTED = {0, 2, 4, 5, 5, 4, 4, 5, 4, 3};
    for (const std::uint32_t expected : EXPECTED)
      Assert::AreEqual(expected, random.NextBelow(6));
  }

  TEST_METHOD(DrawsThePinnedUnitSequence)
  {
    Neuron::Random random(42);
    Assert::AreEqual(0.08386297105988216, random.NextUnit());
    Assert::AreEqual(0.3789802506626686, random.NextUnit());
  }

  TEST_METHOD(StaysInRange)
  {
    Neuron::Random random(7);
    for (int i = 0; i < 10'000; ++i)
    {
      Assert::IsTrue(random.NextBelow(3) < 3);
      Assert::AreEqual(0u, random.NextBelow(1));
      const double unit = random.NextUnit();
      Assert::IsTrue(unit >= 0.0 && unit < 1.0);
    }
  }

  TEST_METHOD(ComparesByState)
  {
    Neuron::Random first(5);
    Neuron::Random second(5);
    Assert::IsTrue(first == second);
    (void)first.NextUInt64();
    Assert::IsFalse(first == second);
    (void)second.NextUInt64();
    Assert::IsTrue(first == second);
    Assert::IsFalse(Neuron::Random(5) == Neuron::Random(6));
  }
};
} // namespace GameLogicTests