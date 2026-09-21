#include "pch.h"

#include <chrono>
#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{

namespace
{
using Clock = std::chrono::steady_clock;

/// An arbitrary fixed origin. THE POINT OF THIS SUITE IS THAT IT NEVER READS A CLOCK: the seam
/// takes the time as an argument, so a run of some minutes at twenty ticks a second costs a
/// handful of microseconds here and the answers are exact rather than nearly.
[[nodiscard]] Clock::time_point Origin() noexcept
{
  return Clock::time_point{std::chrono::milliseconds{1'000'000}};
}

[[nodiscard]] Clock::time_point At(std::int64_t _milliseconds) noexcept
{
  return Origin() + std::chrono::milliseconds{_milliseconds};
}

/// ADR-002's tick, which the GAME decides and this schedule is merely handed.
inline constexpr std::int64_t PERIOD_MS = 50;
} // namespace

TEST_CLASS(TheTickSchedule)
{
public:
  TEST_METHOD(NothingIsOwedBeforeTheFirstPeriodElapses)
  {
    Neuron::TickSchedule schedule{Origin(), PERIOD_MS};
    Assert::AreEqual(0u, schedule.TicksDue(Origin()));
    Assert::AreEqual(0u, schedule.TicksDue(At(1)));
    Assert::AreEqual(0u, schedule.TicksDue(At(49)));
    Assert::AreEqual(1u, schedule.TicksDue(At(50)));
    Assert::AreEqual(std::uint64_t{1}, schedule.TicksIssued());
  }

  TEST_METHOD(ATickIsIssuedOnceAndOnlyOnce)
  {
    Neuron::TickSchedule schedule{Origin(), PERIOD_MS};
    Assert::AreEqual(1u, schedule.TicksDue(At(50)));
    Assert::AreEqual(0u, schedule.TicksDue(At(51)));
    Assert::AreEqual(0u, schedule.TicksDue(At(99)));
    Assert::AreEqual(1u, schedule.TicksDue(At(100)));
  }

  TEST_METHOD(ItDoesNotDriftOverSomeMinutes)
  {
    // M0.11'S EXIT CRITERION, and the reason the schedule is a type rather than a `previous + 50`
    // inside the loop. Every wake-up here is one millisecond LATE, which a schedule that added a
    // period to the last wake-up would carry forward forever. Five minutes at twenty ticks a
    // second is 6,000 ticks, and it has to be exactly 6,000.
    Neuron::TickSchedule schedule{Origin(), PERIOD_MS};
    std::uint64_t issued = 0;
    for (std::int64_t millisecond = 1; millisecond <= (5 * 60 * 1000); ++millisecond)
    {
      issued += schedule.TicksDue(At(millisecond));
    }

    Assert::AreEqual(std::uint64_t{6000}, issued, L"the schedule drifted over five minutes");
    Assert::AreEqual(std::uint64_t{6000}, schedule.TicksIssued());
    Assert::AreEqual(std::uint64_t{0}, schedule.TicksAbandoned());
  }

  TEST_METHOD(ALateWakeUpIsSpentRatherThanBanked)
  {
    // The failure the anti-drift argument is about, at its smallest: a wake-up forty milliseconds
    // late still leaves the next tick on the original grid.
    Neuron::TickSchedule schedule{Origin(), PERIOD_MS};
    Assert::AreEqual(1u, schedule.TicksDue(At(90)));
    // IsTrue rather than AreEqual: CppUnitTest cannot stringify a time_point, and a specialization
    // of ToString for one would be more machinery than the assertion is worth.
    Assert::IsTrue(schedule.NextDeadline() == At(100), L"the deadline should be on the grid, not 50 ms after the late wake-up");
  }

  TEST_METHOD(AStallIsCaughtUpButNotChased)
  {
    // A two-second stall owes forty ticks. Running all forty takes longer than the stall did,
    // which owes more still -- so the schedule gives the rest up and says how many.
    Neuron::TickSchedule schedule{Origin(), PERIOD_MS};
    const std::uint32_t due = schedule.TicksDue(At(2000));
    Assert::AreEqual(Neuron::TickSchedule::MAX_CATCH_UP_TICKS, due);
    Assert::AreEqual(std::uint64_t{30}, schedule.TicksAbandoned());

    // And it does not then owe the abandoned ticks again on the next call.
    Assert::AreEqual(0u, schedule.TicksDue(At(2001)));
  }

  TEST_METHOD(TimeGoingBackwardsOwesNothing)
  {
    Neuron::TickSchedule schedule{Origin(), PERIOD_MS};
    Assert::AreEqual(0u, schedule.TicksDue(At(-1000)));
    Assert::AreEqual(std::uint64_t{0}, schedule.TicksIssued());
  }

  TEST_METHOD(APeriodOfZeroDoesNotDivideByZero)
  {
    // Clamped to one rather than refused: this is the steadiest thing in the process and it does
    // not get to throw.
    Neuron::TickSchedule schedule{Origin(), 0};
    Assert::AreEqual(1u, schedule.TicksDue(At(1)));
  }
};

} // namespace NeuronServerTests
