#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
// Phase 5 design §11 (ADR-080): what a player's report of a time away makes of its events.
TEST_CLASS(AwayReportTests)
{
public:
  TEST_METHOD(CountsWhatWasBuiltAndLost)
  {
    Outpost::AwayReport report;
    for (const Outpost::EventKind kind :
         {Outpost::EventKind::ShipBuilt, Outpost::EventKind::ShipBuilt, Outpost::EventKind::StructureBuilt, Outpost::EventKind::ShipLost,
          Outpost::EventKind::StructureLost, Outpost::EventKind::StructureLost})
      report.Record({.kind = kind});
    Assert::AreEqual(2u, report.shipsBuilt);
    Assert::AreEqual(1u, report.structuresBuilt);
    Assert::AreEqual(1u, report.shipsLost);
    Assert::AreEqual(2u, report.structuresLost);
  }

  // A sector gained and lost again is in neither list, and one lost and gained back neither.
  TEST_METHOD(KeepsTheSectorsGainedAndLostAsItComesBackToThem)
  {
    Outpost::AwayReport report;
    report.Record({.kind = Outpost::EventKind::SectorGained, .sector = 4});
    report.Record({.kind = Outpost::EventKind::SectorGained, .sector = 7});
    report.Record({.kind = Outpost::EventKind::SectorLost, .sector = 4});
    report.Record({.kind = Outpost::EventKind::SectorLost, .sector = 2});
    report.Record({.kind = Outpost::EventKind::SectorLost, .sector = 9});
    report.Record({.kind = Outpost::EventKind::SectorGained, .sector = 9});
    Assert::IsTrue(report.sectorsGained == std::vector<std::int32_t>{7});
    Assert::IsTrue(report.sectorsLost == std::vector<std::int32_t>{2});
  }

  // The orders that fired, the last few; and what only alerts tell, nothing.
  TEST_METHOD(KeepsTheLastOrdersFired)
  {
    Outpost::AwayReport report;
    for (std::uint32_t order = 1; order <= Outpost::AwayReport::REPORTED_ORDERS + 2; ++order)
      report.Record({.kind = Outpost::EventKind::OrderFired, .order = order});
    Assert::AreEqual(Outpost::AwayReport::REPORTED_ORDERS, report.ordersFired.size());
    Assert::AreEqual(3u, report.ordersFired.front().order);
    const Outpost::AwayReport before = report;
    for (const Outpost::EventKind kind :
         {Outpost::EventKind::RelaySuppressed, Outpost::EventKind::RelayAttacked, Outpost::EventKind::EnemyEntered,
          Outpost::EventKind::ShipRetreating, Outpost::EventKind::PiratesCleared})
      report.Record({.kind = kind, .sector = 1});
    Assert::IsTrue(report == before);
  }
};
} // namespace GameProtocolTests
