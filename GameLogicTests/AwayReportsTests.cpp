#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};

Outpost::Snapshot SnapshotOf(Outpost::PlayerId _player, std::uint64_t _tick, std::vector<Outpost::EventView> _events = {})
{
  return {.tick = _tick, .player = _player, .events = std::move(_events)};
}
} // namespace

// Phase 5 design §11 (ADR-080): each seat's report, kept while its deputy plays, and handed to its player when it takes the
// seat again.
TEST_CLASS(AwayReportsTests)
{
public:
  TEST_METHOD(KeepsASeatsReportWhileItsDeputyPlaysAndHandsItOver)
  {
    Outpost::AwayReports reports;
    Outpost::Snapshot present = SnapshotOf(BLUE, 10, {{.kind = Outpost::EventKind::ShipBuilt}});
    reports.Take(present, false);
    Assert::IsTrue(reports.Reports().empty() && !present.away.has_value(), L"nothing while the player plays");

    Outpost::Snapshot first = SnapshotOf(BLUE, 20, {{.kind = Outpost::EventKind::ShipBuilt}});
    reports.Take(first, true);
    Outpost::Snapshot second = SnapshotOf(BLUE, 21, {{.kind = Outpost::EventKind::ShipLost}});
    reports.Take(second, true);
    Outpost::Snapshot other = SnapshotOf(RED, 21, {{.kind = Outpost::EventKind::ShipLost}});
    reports.Take(other, true);
    Assert::AreEqual(std::size_t{2}, reports.Reports().size(), L"a report a seat");
    Assert::IsFalse(first.away.has_value() || second.away.has_value(), L"the deputy's snapshots carry none");

    Outpost::Snapshot back = SnapshotOf(BLUE, 22);
    reports.Take(back, false);
    Assert::IsTrue(back.away.has_value(), L"the player's first snapshot back carries it");
    Assert::AreEqual(std::uint64_t{20}, back.away->sinceTick, L"from the tick the deputy took the seat");
    Assert::AreEqual(1u, back.away->shipsBuilt);
    Assert::AreEqual(1u, back.away->shipsLost);
    Outpost::Snapshot next = SnapshotOf(BLUE, 23);
    reports.Take(next, false);
    Assert::IsFalse(next.away.has_value(), L"once");
    Assert::IsTrue(reports.Reports().size() == 1 && reports.Reports()[0].first == RED, L"the other seat's stays");
  }

  // A report a save kept goes on from where it was, and is handed over as any other.
  TEST_METHOD(GoesOnFromARestoredReport)
  {
    Outpost::AwayReports reports;
    reports.Restore({{BLUE, {.sinceTick = 5, .shipsBuilt = 3}}});
    Outpost::Snapshot played = SnapshotOf(BLUE, 40, {{.kind = Outpost::EventKind::ShipBuilt}});
    reports.Take(played, true);
    Outpost::Snapshot back = SnapshotOf(BLUE, 41);
    reports.Take(back, false);
    Assert::IsTrue(back.away.has_value() && back.away->sinceTick == 5 && back.away->shipsBuilt == 4);
  }
};
} // namespace GameLogicTests
