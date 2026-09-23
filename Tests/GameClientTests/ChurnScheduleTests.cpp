#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr std::uint64_t SEED = 0xC0FFEEull;

/// Every action a schedule takes over _ticks harness ticks, seated whenever it is not away.
[[nodiscard]] std::vector<Outpost::ChurnAction> Actions(Outpost::ChurnSchedule& _schedule, std::uint64_t _ticks)
{
  std::vector<Outpost::ChurnAction> actions;
  bool seated = true;
  for (std::uint64_t tick = 0; tick < _ticks; ++tick)
  {
    const Outpost::ChurnAction action = _schedule.Advance(tick, seated);
    seated = (action == Outpost::ChurnAction::Rejoin) || (seated && (action != Outpost::ChurnAction::Drop));
    actions.push_back(action);
  }
  return actions;
}
} // namespace

/// ADR-022's churner: when it drops and when it comes back, by harness tick.
TEST_CLASS(TheChurnSchedule)
{
public:
  TEST_METHOD(TheSameSeedGivesTheSameScheduleTwice)
  {
    Outpost::ChurnSchedule first{SEED, 5};
    Outpost::ChurnSchedule second{SEED, 5};
    const std::vector<Outpost::ChurnAction> a = Actions(first, 5000);
    Assert::IsTrue(a == Actions(second, 5000));
    Assert::IsTrue(first.RejoinCount() > 0, L"five thousand ticks and it never churned");
  }

  /// **IT NEVER DROPS A BOT THAT IS NOT SEATED.** A bot that dropped between its join and the answer would
  /// come back with no token and take a second seat, and seats are never given back.
  TEST_METHOD(ItNeverDropsBeforeTheSeatIsAnswered)
  {
    Outpost::ChurnSchedule schedule{SEED, 0};
    for (std::uint64_t tick = 0; tick < 10000; ++tick)
    {
      Assert::IsTrue(schedule.Advance(tick, false) == Outpost::ChurnAction::None, L"acted while unseated");
    }
  }

  /// Seated and away for the documented spans, and the seated span counted from the seat, not from the rejoin.
  TEST_METHOD(ItDropsAndReturnsWithinItsIntervals)
  {
    Outpost::ChurnSchedule schedule{SEED, 9};
    bool seated = true;
    bool waiting = false;
    std::uint64_t seatedAt = 0;
    std::uint64_t droppedAt = 0;
    std::uint64_t answerAt = 0;
    std::uint32_t cycles = 0;

    static_cast<void>(schedule.Advance(0, true));
    for (std::uint64_t tick = 1; (tick < 20000) && (cycles < 20); ++tick)
    {
      // Seated again three ticks after each rejoin, as a host on loopback would answer.
      if (waiting && (tick >= answerAt))
      {
        waiting = false;
        seated = true;
        seatedAt = tick;
      }

      const Outpost::ChurnAction action = schedule.Advance(tick, seated);
      if (action == Outpost::ChurnAction::Drop)
      {
        const std::uint64_t held = tick - seatedAt;
        Assert::IsTrue(held >= Outpost::ChurnSchedule::SEATED_TICKS_MINIMUM, L"dropped too soon after the seat");
        Assert::IsTrue(held <= Outpost::ChurnSchedule::SEATED_TICKS_MAXIMUM, L"held the seat too long");
        seated = false;
        droppedAt = tick;
      }
      else if (action == Outpost::ChurnAction::Rejoin)
      {
        const std::uint64_t away = tick - droppedAt;
        Assert::IsTrue(away >= Outpost::ChurnSchedule::AWAY_TICKS_MINIMUM, L"came back too soon");
        Assert::IsTrue(away <= Outpost::ChurnSchedule::AWAY_TICKS_MAXIMUM, L"stayed away too long");
        waiting = true;
        answerAt = tick + 3;
        ++cycles;
      }
    }
    Assert::AreEqual(20u, cycles);
  }

  /// **A CHURNER ALWAYS PRESENTS THE TOKEN IT WAS ISSUED**, across ten rejoins -- M1.14b's criterion -- so the
  /// host seats it once and resumes it every time after. The host here is the session table's rule in
  /// miniature: a known token is `Rejoined`, no token is a new seat.
  TEST_METHOD(ARejoinPresentsTheTokenTheSeatWasIssued)
  {
    constexpr Outpost::SessionToken ISSUED = 0xA11CEull;

    Outpost::ChurnSchedule schedule{SEED, 2};
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);

    std::uint32_t seatsIssued = 0;
    std::uint32_t resumes = 0;
    bool connected = true;
    for (std::uint64_t tick = 0; (tick < 50000) && (schedule.RejoinCount() < 10); ++tick)
    {
      const Outpost::ChurnAction action = schedule.Advance(tick, join.IsJoined());
      if (action == Outpost::ChurnAction::Drop)
      {
        connected = false;
      }
      else if (action == Outpost::ChurnAction::Rejoin)
      {
        join.Rejoin();
        connected = true;
      }

      if (connected && join.ShouldSend(tick * Outpost::HARNESS_TICK_MILLISECONDS))
      {
        const Outpost::Join sent = join.Outgoing();
        Outpost::JoinReply reply{.result = Outpost::JoinResult::Rejoined, .player = 1, .token = ISSUED, .matchSeed = 7};
        if (sent.token == Outpost::NO_SESSION_TOKEN)
        {
          Assert::AreEqual(0u, seatsIssued, L"a churner asked for a second seat");
          ++seatsIssued;
          reply.result = Outpost::JoinResult::Accepted;
        }
        else
        {
          Assert::IsTrue(sent.token == ISSUED, L"a churner presented a token it was not issued");
          ++resumes;
        }
        static_cast<void>(join.Accept(reply));
      }
    }

    Assert::AreEqual(10u, schedule.RejoinCount());
    Assert::AreEqual(1u, seatsIssued);
    Assert::AreEqual(10u, resumes, L"the rejoins were not answered as resumes");
  }
};

} // namespace GameClientTests
