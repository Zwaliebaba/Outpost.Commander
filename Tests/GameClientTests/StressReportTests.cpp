#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
[[nodiscard]] Outpost::Update TickOnly(std::uint32_t _tick, std::uint16_t _sequence)
{
  Outpost::Update update;
  update.sequence = _sequence;
  update.tick = _tick;
  return update;
}

[[nodiscard]] Outpost::JoinReply Reply(Outpost::JoinResult _result)
{
  return Outpost::JoinReply{.result = _result, .player = 1, .token = 99, .matchSeed = 5};
}

/// A short run of one player and one churner, fed the same way every time.
[[nodiscard]] std::string ARun()
{
  Outpost::StressReport report{{Outpost::BotRole::Player, Outpost::BotRole::Churner}};

  Outpost::JoinState join;
  join.Begin(Outpost::NO_SESSION_TOKEN);
  static_cast<void>(join.Accept(Reply(Outpost::JoinResult::Accepted)));
  report.NoteJoin(0, join);

  Outpost::ReplicaStore store;
  std::uint16_t sequence = 1;
  for (const std::uint32_t tick : {10u, 11u, 13u, 14u})
  {
    static_cast<void>(store.Accept(TickOnly(tick, sequence++), tick * 50ull));
    report.NoteDrain(
      0, Outpost::ClientFrame::DrainResult{.datagrams = 1, .accepted = 1, .refreshed = 2, .refreshTicksTotal = 3, .refreshTicksMax = 2},
      store);
  }
  report.NoteSent(0, 3, 2, 1);
  report.NoteAcknowledged(0, 80);
  report.NoteAcknowledged(0, 95);
  return report.Format(40);
}
} // namespace

/// ADR-022's report: what the harness saw on the wire, per role, as text.
TEST_CLASS(TheStressReport)
{
public:
  TEST_METHOD(TheSameCountsFormatToTheSameTextTwice)
  {
    const std::string first = ARun();
    Assert::IsTrue(first == ARun());
    Assert::IsTrue(first.find("player: 1 bots") != std::string::npos);
    Assert::IsTrue(first.find("churner: 1 bots") != std::string::npos);
    Assert::IsTrue(first.find("flooder") == std::string::npos, L"a role with no bots was reported");
  }

  /// **A SEAT IS COUNTED ON THE CHANGE, NOT ON THE REPLY.** The host answers every retry, so a report that
  /// counted replies would say a host seated more players than it has slots.
  TEST_METHOD(ASeatIsCountedOnceHoweverManyRepliesSayIt)
  {
    Outpost::StressReport report{{Outpost::BotRole::Churner}};
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    for (int reply = 0; reply < 3; ++reply)
    {
      static_cast<void>(join.Accept(Reply(Outpost::JoinResult::Accepted)));
      report.NoteJoin(0, join);
    }
    Assert::AreEqual(static_cast<std::uint64_t>(1), report.Counters(Outpost::BotRole::Churner).seated);

    // A rejoin, answered twice, is one resume.
    join.Rejoin();
    report.NoteRejoin(0);
    for (int reply = 0; reply < 2; ++reply)
    {
      static_cast<void>(join.Accept(Reply(Outpost::JoinResult::Rejoined)));
      report.NoteJoin(0, join);
    }
    const Outpost::RoleCounters& counters = report.Counters(Outpost::BotRole::Churner);
    Assert::AreEqual(static_cast<std::uint64_t>(1), counters.seated);
    Assert::AreEqual(static_cast<std::uint64_t>(1), counters.resumed);
    Assert::AreEqual(static_cast<std::uint64_t>(1), counters.rejoins);
  }

  /// **THE UPDATE TICK GAP**, which is where a host falling behind shows first -- measured between one bot's
  /// consecutive newest ticks, and not across a rejoin, where the gap is the bot's own absence.
  TEST_METHOD(TickGapsAreMeasuredForwardAndNotAcrossARejoin)
  {
    Outpost::StressReport report{{Outpost::BotRole::Player}};
    Outpost::ReplicaStore store;
    std::uint16_t sequence = 1;
    for (const std::uint32_t tick : {10u, 11u, 11u, 14u})
    {
      static_cast<void>(store.Accept(TickOnly(tick, sequence++), tick * 50ull));
      report.NoteDrain(0, Outpost::ClientFrame::DrainResult{}, store);
    }

    const Outpost::RoleCounters& counters = report.Counters(Outpost::BotRole::Player);
    Assert::AreEqual(static_cast<std::uint64_t>(2), counters.tickGaps, L"a repeated tick was counted as a gap");
    Assert::AreEqual(static_cast<std::uint64_t>(4), counters.tickGapTotal);
    Assert::AreEqual(3u, counters.tickGapMax);

    report.NoteRejoin(0);
    static_cast<void>(store.Accept(TickOnly(90, sequence++), 4500));
    report.NoteDrain(0, Outpost::ClientFrame::DrainResult{}, store);
    Assert::AreEqual(static_cast<std::uint64_t>(2), counters.tickGaps, L"the absence across a rejoin was reported as a gap");
  }

  /// Loss is the store's cumulative count, taken as a difference, so it is not counted twice.
  TEST_METHOD(LossIsCountedOnce)
  {
    Outpost::StressReport report{{Outpost::BotRole::Player}};
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(TickOnly(1, 1), 50));
    static_cast<void>(store.Accept(TickOnly(4, 4), 200));
    report.NoteDrain(0, Outpost::ClientFrame::DrainResult{}, store);
    report.NoteDrain(0, Outpost::ClientFrame::DrainResult{}, store);
    Assert::AreEqual(static_cast<std::uint64_t>(2), report.Counters(Outpost::BotRole::Player).updatesLost);
  }

  TEST_METHOD(TheRefreshIntervalAndTheAcknowledgeTimeAreReported)
  {
    const std::string text = ARun();
    Assert::IsTrue(text.find("refresh interval per entity: mean 1.50 ticks, max 2 over 8") != std::string::npos);
    Assert::IsTrue(text.find("acknowledge mean 87.50 ms, max 95 ms") != std::string::npos);
    Assert::IsTrue(text.find("update tick gap: mean 1.33, max 2 over 3") != std::string::npos);
  }

  /// **HUNDREDTHS IN INTEGERS**, rounded to nearest, so a report formats to the same bytes everywhere.
  TEST_METHOD(AMeanIsFormattedInHundredths)
  {
    Assert::IsTrue(Outpost::FormatHundredths(0, 0) == "0.00");
    Assert::IsTrue(Outpost::FormatHundredths(3, 2) == "1.50");
    Assert::IsTrue(Outpost::FormatHundredths(1, 3) == "0.33");
    Assert::IsTrue(Outpost::FormatHundredths(2, 3) == "0.67");
    Assert::IsTrue(Outpost::FormatHundredths(101, 100) == "1.01");
    Assert::IsTrue(Outpost::FormatHundredths(250, 1) == "250.00");
  }
};

} // namespace GameClientTests
