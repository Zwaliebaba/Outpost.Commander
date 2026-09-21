#include "pch.h"

#include <cmath>
#include <cstdint>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{

namespace
{
/// DOUBLES AND `std::sin` ARE FINE HERE AND ONLY HERE. R16 keeps CRT transcendentals out of the
/// simulation because they are not correctly rounded and not specified identical across
/// architectures -- but a test is not the simulation, and the whole job of this file is to check
/// 4,096 checked-in literals against an independent reference. The reference being a shade
/// different on another machine is exactly what the tolerances below are sized for.
[[nodiscard]] double ExactSine(Neuron::Angle _angle) noexcept
{
  return std::sin((2.0 * std::numbers::pi * _angle) / 65536.0);
}

/// ADR-002's angular resolution, 360 / 4,096 degrees, expressed as the sine units it is worth at
/// full scale: 32,767 * 2 * pi / 4,096, which is 50.3. Nothing in the table may be further from
/// the truth than one step of the thing the table is stepped by.
inline constexpr double RESOLUTION_UNITS = (32767.0 * 2.0 * std::numbers::pi) / 4096.0;
} // namespace

TEST_CLASS(SineTableCardinals)
{
public:
  TEST_METHOD(TheFourCardinalsAreTheSaturatedValues)
  {
    // ADR-002's documented property, and the reason it is documented: Q1.15 spans [-1, +0.999969]
    // so a sine of one does not exist. 32,767 is what the table holds instead.
    Assert::AreEqual(std::int16_t{0}, Neuron::Sine(0));
    Assert::AreEqual(Neuron::SINE_ONE, Neuron::Sine(Neuron::ANGLE_QUARTER_TURN));
    Assert::AreEqual(std::int16_t{0}, Neuron::Sine(32768));
    Assert::AreEqual(static_cast<std::int16_t>(-Neuron::SINE_ONE), Neuron::Sine(49152));
  }

  TEST_METHOD(SaturationIsNotQuiteOne)
  {
    // The 0.003% ADR-002 names. A "unit" vector built from this table is 0.99997 long, and a
    // chain of multiplies by it shrinks -- which is a property to know about rather than a bug.
    Assert::AreEqual(std::int16_t{32767}, Neuron::SINE_ONE);
    Assert::IsTrue(Neuron::SINE_ONE < 32768, L"32768 would overflow an int16_t, which is the whole point");
  }

  TEST_METHOD(CosineIsSineAQuarterTurnAlong)
  {
    Assert::AreEqual(Neuron::SINE_ONE, Neuron::Cosine(0));
    Assert::AreEqual(std::int16_t{0}, Neuron::Cosine(Neuron::ANGLE_QUARTER_TURN));
    Assert::AreEqual(static_cast<std::int16_t>(-Neuron::SINE_ONE), Neuron::Cosine(32768));
    Assert::AreEqual(std::int16_t{0}, Neuron::Cosine(49152));

    // And the identity holds everywhere, including across the wrap the addition does for free.
    for (std::uint32_t angle = 0; angle < 65536; angle += 137)
    {
      const Neuron::Angle heading = static_cast<Neuron::Angle>(angle);
      Assert::AreEqual(Neuron::Sine(static_cast<Neuron::Angle>(heading + Neuron::ANGLE_QUARTER_TURN)), Neuron::Cosine(heading));
    }
  }
};

TEST_CLASS(SineTableAccuracy)
{
public:
  TEST_METHOD(EveryEntryIsTheCorrectlyRoundedReference)
  {
    // All 4,096, not a sample. These are checked-in literals, so this is the test that makes them
    // auditable rather than magic -- a mistyped digit anywhere in SineTable.cpp fails here.
    for (std::uint32_t index = 0; index < Neuron::SINE_TABLE_SIZE; ++index)
    {
      const Neuron::Angle angle = static_cast<Neuron::Angle>(index << 4);
      const double exact = 32767.0 * ExactSine(angle);
      const double error = std::abs(static_cast<double>(Neuron::Sine(angle)) - exact);
      Assert::IsTrue(error <= 0.5 + 1e-9, L"a table entry is not the correctly rounded value");
    }
  }

  TEST_METHOD(EveryAngleIsWithinTheStatedResolution)
  {
    // The claim ADR-002 actually makes, over all 65,536 angles rather than the 4,096 sampled
    // ones: an angle between two entries takes the one below it, and the error that introduces
    // is bounded by one step of the table. Measured worst case is 47.6 against a bound of 50.3.
    double worst = 0.0;
    for (std::uint32_t angle = 0; angle < 65536; ++angle)
    {
      const Neuron::Angle heading = static_cast<Neuron::Angle>(angle);
      const double error = std::abs(static_cast<double>(Neuron::Sine(heading)) - (32767.0 * ExactSine(heading)));
      worst = (error > worst) ? error : worst;
    }
    Assert::IsTrue(worst <= RESOLUTION_UNITS, L"the table is further from the truth than its own angular resolution");
  }

  TEST_METHOD(TheTableIsExactlyAntisymmetric)
  {
    // A heading and its opposite scale by the same magnitude, which is why the negative cardinal
    // is -32,767 rather than the -32,768 that Q1.15 could have held. Exact, not within one.
    for (std::uint32_t angle = 0; angle < 65536; angle += 16)
    {
      const Neuron::Angle heading = static_cast<Neuron::Angle>(angle);
      const Neuron::Angle opposite = static_cast<Neuron::Angle>(angle + 32768);
      Assert::AreEqual(static_cast<std::int16_t>(-Neuron::Sine(heading)), Neuron::Sine(opposite));
    }
  }

  TEST_METHOD(EveryAngleInASixteenStepRunSharesOneEntry)
  {
    // `angle >> 4` is the index, so sixteen consecutive angles are one entry. This is what "no
    // interpolation" means in practice, and it is worth asserting because a future change that
    // adds interpolation would have to come through here.
    for (std::uint32_t base = 0; base < 65536; base += 4096)
    {
      const std::int16_t expected = Neuron::Sine(static_cast<Neuron::Angle>(base));
      for (std::uint32_t step = 0; step < 16; ++step)
      {
        Assert::AreEqual(expected, Neuron::Sine(static_cast<Neuron::Angle>(base + step)));
      }
    }
  }

  TEST_METHOD(NothingInTheTableLeavesQOneFifteen)
  {
    for (std::uint32_t index = 0; index < Neuron::SINE_TABLE_SIZE; ++index)
    {
      const std::int16_t value = Neuron::Sine(static_cast<Neuron::Angle>(index << 4));
      Assert::IsTrue(value >= -Neuron::SINE_ONE && value <= Neuron::SINE_ONE, L"an entry escaped the saturated range");
    }
  }
};

} // namespace NeuronCoreTests
