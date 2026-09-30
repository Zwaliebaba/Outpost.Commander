#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
TEST_CLASS(TickHostTests)
{
public:
  TEST_METHOD(RunsATickEachPeriod)
  {
    Neuron::TickHost host(20, 5);
    Assert::AreEqual(0u, host.Advance(49ms));
    Assert::AreEqual(1u, host.Advance(1ms));
    Assert::AreEqual(0u, host.Advance(49ms));
    Assert::AreEqual(1u, host.Advance(1ms));
    Assert::AreEqual(2u, host.Advance(100ms));
  }

  TEST_METHOD(KeepsTheRemainderExactly)
  {
    // A third of a second is not a whole number of nanoseconds, so a host that rounded the period would drift.
    Neuron::TickHost host(3, 5);
    std::uint32_t ticks = 0;
    for (int frame = 0; frame < 60 * 60; ++frame)
      ticks += host.Advance(std::chrono::nanoseconds(16'666'667));
    // 3,600 frames of 16,666,667 ns are 60.0000012 s: exactly 180 ticks.
    Assert::AreEqual(180u, ticks);
  }

  TEST_METHOD(DropsABacklogBeyondTheLimit)
  {
    Neuron::TickHost host(20, 5);
    Assert::AreEqual(5u, host.Advance(10s));
    // The backlog is gone rather than paid back over the next calls.
    Assert::AreEqual(0u, host.Advance(0ms));
    Assert::AreEqual(1u, host.Advance(50ms));
  }

  TEST_METHOD(IgnoresNegativeTime)
  {
    Neuron::TickHost host(20, 5);
    Assert::AreEqual(0u, host.Advance(-1s));
    Assert::AreEqual(1u, host.Advance(50ms));
  }

  TEST_METHOD(RejectsARateOutOfRange)
  {
    Assert::ExpectException<Neuron::Exception>([] { Neuron::TickHost host(0, 5); });
    Assert::ExpectException<Neuron::Exception>([] { Neuron::TickHost host(20, 0); });
    Assert::ExpectException<Neuron::Exception>([] { Neuron::TickHost host(1'000'000, 5); });
  }
};
} // namespace GameLogicTests
