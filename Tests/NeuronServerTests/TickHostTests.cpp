#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace NeuronServerTests
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

  // ADR-025: the server's thread sleeps until the next tick is due, which keeps the remainder in view.
  TEST_METHOD(SaysHowLongUntilTheNextTick)
  {
    Neuron::TickHost host(20, 5);
    Assert::IsTrue(host.UntilNextTick() == 50ms);
    Assert::AreEqual(0u, host.Advance(30ms));
    Assert::IsTrue(host.UntilNextTick() == 20ms);
    Assert::AreEqual(1u, host.Advance(host.UntilNextTick()));
    Assert::IsTrue(host.UntilNextTick() == 50ms);
    // Rounded up, so that waiting this long always makes a tick due: a third of a second is 333,333,333.3 ns.
    Neuron::TickHost thirds(3, 5);
    Assert::IsTrue(thirds.UntilNextTick() == std::chrono::nanoseconds(333'333'334));
    Assert::AreEqual(1u, thirds.Advance(thirds.UntilNextTick()));
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
} // namespace NeuronServerTests