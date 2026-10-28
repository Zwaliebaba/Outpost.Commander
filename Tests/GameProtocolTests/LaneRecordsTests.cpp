#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
std::vector<std::string> Kept(std::vector<std::string> _records, std::uint64_t _sinceTick)
{
  std::deque<std::string> records(_records.begin(), _records.end());
  Outpost::KeepLaneRecords(records, _sinceTick);
  return {records.begin(), records.end()};
}
} // namespace

TEST_CLASS(LaneRecordsTests)
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
    Assert::IsTrue(Outpost::IsLaneRecord("seat") && Outpost::IsLaneRecord("start") && Outpost::IsLaneRecord("battle"));
    Assert::IsFalse(Outpost::IsLaneRecord("hour") || Outpost::IsLaneRecord("save"));
  }

  // ADR-101 decision 7: the lanes keep their records of the day, and each seat's last hand-over before it, which says who played
  // it as the day began; every other record goes, and so does a record of another kind.
  TEST_METHOD(KeepsTheDayAndEachSeatsHandOverBeforeIt)
  {
    const std::vector<std::string> kept =
      Kept({"start 0 new", "seat 10 player 1 player", "seat 20 player 2 deputy", "battle 30 40 sector 1 player 1 present player 2 deputy",
            "seat 50 player 1 deputy", "hour 72000 player 1 ore 0", "seat 120 player 2 player", "battle 130 140 sector 2 player 2 present"},
           100);
    Assert::IsTrue(kept == std::vector<std::string>{"seat 20 player 2 deputy", "seat 50 player 1 deputy", "seat 120 player 2 player",
                                                    "battle 130 140 sector 2 player 2 present"},
                   L"seat 2's at 20 and seat 1's at 50 start the day; the rest before it goes");
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
