#include "pch.h"

#include <cstdint>
#include <limits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{

namespace
{
inline constexpr Neuron::Fixed INT32_LOWEST = std::numeric_limits<std::int32_t>::min();
inline constexpr Neuron::Fixed INT32_HIGHEST = std::numeric_limits<std::int32_t>::max();
} // namespace

TEST_CLASS(FixedPointConversion)
{
public:
  TEST_METHOD(AWholeUnitIsTwoHundredAndFiftySix)
  {
    Assert::AreEqual(Neuron::Fixed{256}, Neuron::FIXED_ONE);
    Assert::AreEqual(Neuron::Fixed{256}, Neuron::FixedFromWholeUnits(1));
    Assert::AreEqual(Neuron::Fixed{-256}, Neuron::FixedFromWholeUnits(-1));
    Assert::AreEqual(Neuron::Fixed{0}, Neuron::FixedFromWholeUnits(0));
  }

  TEST_METHOD(ConversionBackFloorsRatherThanTruncating)
  {
    // The distinction that matters: a value just below zero is -1 whole units and not 0. Every
    // conversion in FixedPoint.h floors, and this is where that is pinned.
    Assert::AreEqual(0, Neuron::FixedToWholeUnitsFloor(Neuron::FIXED_ONE - 1));
    Assert::AreEqual(1, Neuron::FixedToWholeUnitsFloor(Neuron::FIXED_ONE));
    Assert::AreEqual(-1, Neuron::FixedToWholeUnitsFloor(Neuron::Fixed{-1}));
    Assert::AreEqual(-1, Neuron::FixedToWholeUnitsFloor(-Neuron::FIXED_ONE));
    Assert::AreEqual(-2, Neuron::FixedToWholeUnitsFloor(-Neuron::FIXED_ONE - 1));
  }

  TEST_METHOD(TheWholeUnitRoundTripSurvivesTheEdges)
  {
    // 8,388,607 whole units is the most a Fixed can carry, which is far outside the 8,192 the
    // play area needs -- ADR-002's three orders of magnitude of headroom, asserted rather than
    // quoted.
    Assert::AreEqual(8388607, Neuron::FixedToWholeUnitsFloor(Neuron::FixedFromWholeUnits(8388607)));
    Assert::AreEqual(-8388608, Neuron::FixedToWholeUnitsFloor(Neuron::FixedFromWholeUnits(-8388608)));
  }
};

TEST_CLASS(FixedPointMultiply)
{
public:
  TEST_METHOD(OneIsTheIdentityIncludingAtBothEdges)
  {
    Assert::AreEqual(Neuron::FIXED_ONE, Neuron::Multiply(Neuron::FIXED_ONE, Neuron::FIXED_ONE));
    Assert::AreEqual(-Neuron::FIXED_ONE, Neuron::Multiply(-Neuron::FIXED_ONE, Neuron::FIXED_ONE));

    // The whole point of the 64-bit intermediate: the product here is 5.5e11, which an int32
    // could not hold for a moment, and the answer still comes back exact.
    Assert::AreEqual(INT32_HIGHEST, Neuron::Multiply(INT32_HIGHEST, Neuron::FIXED_ONE));
    Assert::AreEqual(INT32_LOWEST, Neuron::Multiply(INT32_LOWEST, Neuron::FIXED_ONE));
  }

  TEST_METHOD(TheShiftFloorsAndThatIsVisibleAtOne)
  {
    // 1/256 times 1/256 is 1/65536, which this format cannot hold. It floors to zero going up
    // and to minus one going down -- NOT symmetric, and that asymmetry is the arithmetic shift.
    Assert::AreEqual(Neuron::Fixed{0}, Neuron::Multiply(1, 1));
    Assert::AreEqual(Neuron::Fixed{-1}, Neuron::Multiply(-1, 1));
    Assert::AreEqual(Neuron::Fixed{-1}, Neuron::Multiply(1, -1));
    Assert::AreEqual(Neuron::Fixed{0}, Neuron::Multiply(-1, -1));
  }

  TEST_METHOD(AResultTooLargeWrapsAndTheWrapIsPinned)
  {
    // FixedPoint.h says a result that will not fit wraps modulo 2^32, deterministically on every
    // platform, and that the bound is the caller's to keep. These are what that actually does --
    // asserted rather than described, so that a change to the narrowing shows up here and not as
    // a movement bug months later. None of these three is a value any caller should produce.
    Assert::AreEqual(Neuron::Fixed{-16777216}, Neuron::Multiply(INT32_HIGHEST, INT32_HIGHEST));
    Assert::AreEqual(Neuron::Fixed{0}, Neuron::Multiply(INT32_LOWEST, INT32_LOWEST));
    Assert::AreEqual(Neuron::Fixed{8388608}, Neuron::Multiply(INT32_LOWEST, INT32_HIGHEST));
  }

  TEST_METHOD(APositionSizedMultiplyIsExact)
  {
    // The case the simulation actually runs: a coordinate near the play area's edge scaled by a
    // half. 2,097,152 is ADR-002's half-extent.
    const Neuron::Fixed halfExtent = 2097152;
    Assert::AreEqual(Neuron::Fixed{1048576}, Neuron::Multiply(halfExtent, Neuron::FIXED_ONE / 2));
    Assert::AreEqual(Neuron::Fixed{-1048576}, Neuron::Multiply(-halfExtent, Neuron::FIXED_ONE / 2));
  }
};

TEST_CLASS(FixedPointDivide)
{
public:
  TEST_METHOD(OneIsTheIdentityIncludingAtBothEdges)
  {
    Assert::AreEqual(Neuron::FIXED_ONE, Neuron::Divide(Neuron::FIXED_ONE, Neuron::FIXED_ONE));
    Assert::AreEqual(INT32_HIGHEST, Neuron::Divide(INT32_HIGHEST, Neuron::FIXED_ONE));
    Assert::AreEqual(INT32_LOWEST, Neuron::Divide(INT32_LOWEST, Neuron::FIXED_ONE));
  }

  TEST_METHOD(ItTruncatesTowardZeroWhereMultiplyFloors)
  {
    // THE ASYMMETRY, PINNED. A third of 1/256 is zero from both directions here, where Multiply
    // would have floored the negative one to -1. FixedPoint.h documents this rather than papering
    // over it; if it is ever made to floor, this test is the thing that says so out loud.
    Assert::AreEqual(Neuron::Fixed{0}, Neuron::Divide(1, 3 * Neuron::FIXED_ONE));
    Assert::AreEqual(Neuron::Fixed{0}, Neuron::Divide(-1, 3 * Neuron::FIXED_ONE));
    Assert::AreEqual(Neuron::Fixed{-1}, Neuron::Multiply(-1, Neuron::FIXED_ONE / 3));
  }

  TEST_METHOD(AQuotientTooLargeWrapsAndTheWrapIsPinned)
  {
    // Dividing by 1/256 multiplies by 256, so this one leaves the format immediately. Same
    // contract as Multiply's: deterministic, wrong, and the caller's bound to have kept.
    Assert::AreEqual(Neuron::Fixed{-256}, Neuron::Divide(INT32_HIGHEST, 1));
    Assert::AreEqual(INT32_LOWEST, Neuron::Divide(INT32_LOWEST, -Neuron::FIXED_ONE));
  }

  TEST_METHOD(TheFractionSurvivesTheDivide)
  {
    // Three halves, in a format where a half is 128.
    Assert::AreEqual(Neuron::Fixed{384}, Neuron::Divide(3 * Neuron::FIXED_ONE, 2 * Neuron::FIXED_ONE));
    Assert::AreEqual(Neuron::Fixed{1}, Neuron::Divide(1, Neuron::FIXED_ONE));
  }
};

TEST_CLASS(BinaryAngle)
{
public:
  TEST_METHOD(TheDifferenceAcrossTheWrapIsJustASubtraction)
  {
    // The whole reason for the format (ADR-002). A heading just past zero and one just short of
    // it are a few units apart, not almost a full turn -- and there is no branch anywhere in
    // AngleDifference that could have got that wrong.
    Assert::AreEqual(std::int16_t{-136}, Neuron::AngleDifference(100, 65500));
    Assert::AreEqual(std::int16_t{136}, Neuron::AngleDifference(65500, 100));
    Assert::AreEqual(std::int16_t{0}, Neuron::AngleDifference(0, 0));
    Assert::AreEqual(std::int16_t{0}, Neuron::AngleDifference(65535, 65535));
  }

  TEST_METHOD(AQuarterTurnIsSixteenThousandThreeHundredAndEightyFour)
  {
    Assert::AreEqual(Neuron::Angle{16384}, Neuron::ANGLE_QUARTER_TURN);
    Assert::AreEqual(std::int16_t{16384}, Neuron::AngleDifference(0, Neuron::ANGLE_QUARTER_TURN));
  }

  TEST_METHOD(TheHalfTurnIsTheOneAmbiguousAnswer)
  {
    // Exactly opposite headings are 32,768 apart, which int16 cannot hold as a positive. It comes
    // back as -32,768, and that is not a bug to fix: the two directions of travel really are
    // equally far, so either sign is correct and the format has to pick one.
    Assert::AreEqual(std::int16_t{-32768}, Neuron::AngleDifference(0, 32768));
    Assert::AreEqual(std::int16_t{-32768}, Neuron::AngleDifference(32768, 0));
  }

  TEST_METHOD(AdditionWrapsWithoutAModulus)
  {
    const Neuron::Angle nearlyRound = 65500;
    Assert::AreEqual(Neuron::Angle{36}, static_cast<Neuron::Angle>(nearlyRound + 72));
  }
};

TEST_CLASS(IntegerSquareRoot)
{
public:
  TEST_METHOD(ItIsExactOnEveryPerfectSquareItIsGiven)
  {
    for (std::int64_t root = 0; root <= 4096; ++root)
    {
      Assert::AreEqual(root, Neuron::Sqrt(root * root));
    }
  }

  TEST_METHOD(ItFloorsBetweenTwoSquares)
  {
    Assert::AreEqual(std::int64_t{1}, Neuron::Sqrt(2));
    Assert::AreEqual(std::int64_t{1}, Neuron::Sqrt(3));
    Assert::AreEqual(std::int64_t{2}, Neuron::Sqrt(8));
    Assert::AreEqual(std::int64_t{3}, Neuron::Sqrt(15));

    // One below and one above a square, all the way up, which is where an off-by-one in the
    // digit loop would hide.
    for (std::int64_t root = 2; root <= 1024; ++root)
    {
      Assert::AreEqual(root - 1, Neuron::Sqrt((root * root) - 1));
      Assert::AreEqual(root, Neuron::Sqrt((root * root) + 1));
    }
  }

  TEST_METHOD(ItReachesTheTopOfTheRange)
  {
    // The reason the result is int64 and not Fixed: this is a little over 3.0e9, which an int32
    // cannot hold.
    Assert::AreEqual(std::int64_t{3037000499}, Neuron::Sqrt(std::numeric_limits<std::int64_t>::max()));
    Assert::AreEqual(std::int64_t{2147483648}, Neuron::Sqrt(std::int64_t{1} << 62));
  }

  TEST_METHOD(ANegativeIsZeroRatherThanAnExcursion)
  {
    Assert::AreEqual(std::int64_t{0}, Neuron::Sqrt(-1));
    Assert::AreEqual(std::int64_t{0}, Neuron::Sqrt(std::numeric_limits<std::int64_t>::min()));
    Assert::AreEqual(std::int64_t{0}, Neuron::Sqrt(0));
  }
};

TEST_CLASS(Vector2)
{
public:
  TEST_METHOD(AdditionAndSubtractionAreComponentwise)
  {
    const Neuron::Vec2 a{.x = 3 * Neuron::FIXED_ONE, .y = -4 * Neuron::FIXED_ONE};
    const Neuron::Vec2 b{.x = Neuron::FIXED_ONE, .y = 2 * Neuron::FIXED_ONE};

    Assert::IsTrue(Neuron::Vec2{.x = 4 * Neuron::FIXED_ONE, .y = -2 * Neuron::FIXED_ONE} == (a + b));
    Assert::IsTrue(Neuron::Vec2{.x = 2 * Neuron::FIXED_ONE, .y = -6 * Neuron::FIXED_ONE} == (a - b));
    Assert::IsTrue(Neuron::Vec2{.x = -3 * Neuron::FIXED_ONE, .y = 4 * Neuron::FIXED_ONE} == -a);
  }

  TEST_METHOD(ScaleGoesThroughTheFixedMultiply)
  {
    const Neuron::Vec2 value{.x = 4 * Neuron::FIXED_ONE, .y = -4 * Neuron::FIXED_ONE};
    const Neuron::Vec2 halved = Neuron::Scale(value, Neuron::FIXED_ONE / 2);
    Assert::IsTrue(Neuron::Vec2{.x = 2 * Neuron::FIXED_ONE, .y = -2 * Neuron::FIXED_ONE} == halved);
    Assert::IsTrue(value == Neuron::Scale(value, Neuron::FIXED_ONE));
  }

  TEST_METHOD(TheSquaredLengthIsSixtyFourBitAndStaysExactAtTheEdge)
  {
    // Both components at ADR-002's half-extent, which is the largest a position can be. 8.8e12 --
    // comfortably inside an int64 and hopeless in anything narrower, which is the point.
    const Neuron::Fixed halfExtent = 2097152;
    const Neuron::Vec2 corner{.x = halfExtent, .y = halfExtent};
    Assert::AreEqual(std::int64_t{8796093022208}, Neuron::LengthSquared(corner));
  }

  TEST_METHOD(APerpendicularPairDotsToZero)
  {
    const Neuron::Vec2 east{.x = Neuron::FIXED_ONE, .y = 0};
    const Neuron::Vec2 north{.x = 0, .y = Neuron::FIXED_ONE};
    Assert::AreEqual(std::int64_t{0}, Neuron::Dot(east, north));
    Assert::AreEqual(std::int64_t{65536}, Neuron::Dot(east, east));
  }

  TEST_METHOD(ADefaultVectorIsTheOrigin)
  {
    Assert::IsTrue(Neuron::Vec2{} == (Neuron::Vec2{.x = 0, .y = 0}));
    Assert::AreEqual(std::int64_t{0}, Neuron::LengthSquared(Neuron::Vec2{}));
  }
};

} // namespace NeuronCoreTests
