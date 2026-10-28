#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
std::vector<std::string> Kept(std::vector<std::string> _records, std::uint64_t _sinceTick)
{
  std::deque<std::string> records(_records.begin(), _records.end());
  Outpost::KeepDayRecords(records, _sinceTick);
  return {records.begin(), records.end()};
}
} // namespace

TEST_CLASS(DayRecordsTests)
{
public:
  // ADR-082 decision 1: a record starts with its kind and its tick.
  TEST_METHOD(ReadsARecordsKindAndTick)
  {
    const std::optional<Outpost::RecordHead> head = Outpost::HeadOf("seat 1200 player 3 deputy");
    Assert::IsTrue(head.has_value());
    Assert::AreEqual(std::string("seat"), std::string(head.value_or(Outpost::RecordHead{}).kind));
    Assert::AreEqual(std::uint64_t{1'200}, head.value_or(Outpost::RecordHead{}).tick);
    Assert::IsFalse(Outpost::HeadOf("seat").has_value(), L"no tick");
    Assert::IsFalse(Outpost::HeadOf("seat twelve").has_value(), L"no number");
    Assert::IsFalse(Outpost::HeadOf("").has_value());
  }

  // ADR-101 decision 7: the day's records are the lanes' kinds, a seat's joins and hours for its window, and the server's long and
  // dropped ticks for its own; a save, of which the log writes one a minute, is not one, nor an order fired.
  TEST_METHOD(KnowsTheDaysKinds)
  {
    for (const std::string_view kind : {"seat", "start", "battle", "join", "hour", "tick", "dropped"})
      Assert::IsTrue(Outpost::IsDayRecord(kind), std::wstring(kind.begin(), kind.end()).c_str());
    for (const std::string_view kind : {"save", "written", "order", "overseer", "lost", "waiting", "restart", ""})
      Assert::IsFalse(Outpost::IsDayRecord(kind), std::wstring(kind.begin(), kind.end()).c_str());
  }

  // ADR-101 decision 7: the day's records from the day's first tick are kept, with each seat's last hand-over before it, which
  // says who played it as the day began; every other record before the day goes, whatever its kind, and so does a save.
  TEST_METHOD(KeepsTheDayAndEachSeatsHandOverBeforeIt)
  {
    const std::vector<std::string> kept =
      Kept({"start 0 new", "seat 10 player 1 player", "seat 20 player 2 deputy", "battle 30 40 sector 1 player 1 present player 2 deputy",
            "join 40 welcomed player 1 from 10.0.0.5", "seat 50 player 1 deputy", "tick 60 us 7200 over 1 Movement 5000",
            "save 110 bytes 10 encode_us 5", "hour 115 player 1 ore 0", "dropped 118 ticks 3", "seat 120 player 2 player",
            "join 125 gone player 2 lost from 10.0.0.6", "battle 130 140 sector 2 player 2 present"},
           100);
    Assert::IsTrue(kept == std::vector<std::string>{"seat 20 player 2 deputy", "seat 50 player 1 deputy", "hour 115 player 1 ore 0",
                                                    "dropped 118 ticks 3", "seat 120 player 2 player",
                                                    "join 125 gone player 2 lost from 10.0.0.6",
                                                    "battle 130 140 sector 2 player 2 present"},
                   L"seat 2's at 20 and seat 1's at 50 start the day; the rest before it goes, and the save");
  }

  // ADR-077 decision 8, ADR-101 decision 7: a recovery comes back to an earlier tick, and what the log wrote past that tick before
  // it belongs to a universe that did not go on.
  TEST_METHOD(DropsWhatARecoveryWentBackPast)
  {
    const std::vector<std::string> kept = Kept({"start 0 new", "seat 100 player 1 deputy", "seat 300 player 1 player",
                                                "start 200 recovered save 180 replayed 4", "seat 250 player 1 player"},
                                               0);
    Assert::IsTrue(kept == std::vector<std::string>{"start 0 new", "seat 100 player 1 deputy", "start 200 recovered save 180 replayed 4",
                                                    "seat 250 player 1 player"});
  }
};
} // namespace GameProtocolTests
