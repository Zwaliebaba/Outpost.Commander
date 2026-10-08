#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr std::uint64_t DELAY_TICKS = std::uint64_t{Outpost::DEPUTY_DELAY_SECONDS} * TICKS_PER_SECOND;
} // namespace

// Phase 5 design §6, gate H2 (ADR-079): who plays a seat that has a deputy.
TEST_CLASS(SeatControllerTests)
{
public:
  TEST_METHOD(TheSeatIsTheDeputysUntilItsPlayerTakesIt)
  {
    Outpost::SeatController control(TICKS_PER_SECOND);
    Assert::IsTrue(control.DeputyPlays(0, 0, false));
    Assert::IsTrue(control.DeputyPlays(DELAY_TICKS * 3, 0, false));
    Assert::IsFalse(control.DeputyPlays(DELAY_TICKS * 3 + 1, 1, false), L"the player plays once it has taken the seat");
  }

  TEST_METHOD(TheDeputyTakesTheSeatAMinuteAfterTheConnectionHasGone)
  {
    Outpost::SeatController control(TICKS_PER_SECOND);
    Assert::IsFalse(control.DeputyPlays(100, 1, false));
    Assert::IsFalse(control.DeputyPlays(200, 1, true), L"the connection has just gone");
    Assert::IsFalse(control.DeputyPlays(200 + DELAY_TICKS - 1, 1, true), L"a tick short of the delay");
    Assert::IsTrue(control.DeputyPlays(200 + DELAY_TICKS, 1, true), L"the delay is up");
    Assert::IsTrue(control.DeputyPlays(200 + (DELAY_TICKS * 10), 1, true), L"and the seat stays its deputy's");
  }

  TEST_METHOD(ThePlayerTakesItsSeatBack)
  {
    Outpost::SeatController control(TICKS_PER_SECOND);
    (void)control.DeputyPlays(0, 1, true);
    Assert::IsTrue(control.DeputyPlays(DELAY_TICKS, 1, true));
    Assert::IsFalse(control.DeputyPlays(DELAY_TICKS + 1, 2, false), L"a newer connection takes the seat back at once");
    // The newer connection goes too: the delay runs again from when it was first seen gone.
    Assert::IsFalse(control.DeputyPlays(DELAY_TICKS * 2, 2, true));
    Assert::IsFalse(control.DeputyPlays((DELAY_TICKS * 3) - 1, 2, true));
    Assert::IsTrue(control.DeputyPlays(DELAY_TICKS * 3, 2, true));
  }

  TEST_METHOD(AConnectionTakenUpAgainInTimeNeverSeesTheDeputy)
  {
    Outpost::SeatController control(TICKS_PER_SECOND);
    Assert::IsFalse(control.DeputyPlays(10, 1, true));
    Assert::IsFalse(control.DeputyPlays(10 + DELAY_TICKS - 1, 2, false), L"taken again before the delay is up");
    Assert::IsFalse(control.DeputyPlays(10 + (DELAY_TICKS * 2), 2, false));
  }
};
} // namespace GameLogicTests
