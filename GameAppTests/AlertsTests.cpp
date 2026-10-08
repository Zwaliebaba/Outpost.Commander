#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};
constexpr Outpost::PlayerId ENEMY{2};
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr Outpost::EntityId RELAY{7};

// The player's snapshot at _tick: it holds the south, whose Relay stands on its node, and the enemy holds the north.
Outpost::Snapshot At(std::uint64_t _tick)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = PLAYER};
  snapshot.sectors = {{.id = 2,
                       .nameUtf8 = "South",
                       .minXMeters = -500.0f,
                       .maxXMeters = 500.0f,
                       .minZMeters = -1000.0f,
                       .maxZMeters = 0.0f,
                       .node = {.xMeters = 0.0f, .zMeters = -500.0f},
                       .holder = PLAYER},
                      {.id = 8,
                       .nameUtf8 = "North",
                       .minXMeters = -500.0f,
                       .maxXMeters = 500.0f,
                       .minZMeters = 0.0f,
                       .maxZMeters = 1000.0f,
                       .node = {.xMeters = 0.0f, .zMeters = 500.0f},
                       .holder = ENEMY}};
  snapshot.entities = {{.id = RELAY,
                        .kind = Outpost::EntityKind::Structure,
                        .owner = PLAYER,
                        .structure = Outpost::StructureKind::Relay,
                        .position = {.xMeters = 0.0f, .zMeters = -500.0f}}};
  return snapshot;
}

// _snapshot with _event among its events.
Outpost::Snapshot With(Outpost::Snapshot _snapshot, Outpost::EventView _event)
{
  _snapshot.events.push_back(_event);
  return _snapshot;
}

std::vector<std::string> Texts(const Outpost::Alerts& _alerts, std::uint64_t _tick)
{
  std::vector<std::string> texts;
  for (const Outpost::Alerts::Alert& alert : _alerts.Shown(_tick, TICKS_PER_SECOND))
    texts.push_back(alert.text);
  return texts;
}
} // namespace

// Phase 2 design §9, ADR-059: the alerts the client reads from the events the server raises (ADR-080).
TEST_CLASS(AlertsTests)
{
public:
  TEST_METHOD(AlertsToEnemyShipsInAHeldSectorOnce)
  {
    Outpost::Alerts alerts;
    alerts.Observe(At(1), TICKS_PER_SECOND);
    Assert::IsNull(alerts.Newest());

    const Outpost::EventView entered{.kind = Outpost::EventKind::EnemyEntered,
                                     .sector = 2,
                                     .position = {.xMeters = 300.0f, .zMeters = -800.0f},
                                     .subject = Outpost::EntityId{20},
                                     .other = ENEMY};
    alerts.Observe(With(At(2), entered), TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 2) == std::vector<std::string>{"Enemy ships in South"});
    Assert::IsNotNull(alerts.Newest());
    Assert::IsTrue(alerts.Newest()->position == Outpost::PlanePosition{.xMeters = 300.0f, .zMeters = -800.0f});

    // Raised again soon after, as when they leave and come back: no new alert within the repeat time.
    alerts.Observe(At(3), TICKS_PER_SECOND);
    alerts.Observe(With(At(5), entered), TICKS_PER_SECOND);
    Assert::AreEqual(size_t{1}, alerts.Shown(5, TICKS_PER_SECOND).size());

    // It shows for eight seconds.
    Assert::AreEqual(size_t{1}, alerts.Shown(2 + (8 * TICKS_PER_SECOND) - 1, TICKS_PER_SECOND).size());
    Assert::IsTrue(alerts.Shown(2 + (8 * TICKS_PER_SECOND), TICKS_PER_SECOND).empty());

    // Once the repeat time is up, it is raised again.
    const std::uint64_t later = 2 + (std::uint64_t{Outpost::Alerts::REPEAT_SECONDS} * TICKS_PER_SECOND);
    alerts.Observe(With(At(later), entered), TICKS_PER_SECOND);
    Assert::AreEqual(size_t{1}, alerts.Shown(later, TICKS_PER_SECOND).size());
  }

  // ADR-073: pirates in a held sector, as the ships an outpost has left once the player has claimed it, are named so.
  // Phase 4 design §13: one of the player's ships going back to be repaired, and a sector the pirates guarded cleared of
  // them.
  TEST_METHOD(AlertsToARetreatAndPiratesCleared)
  {
    Outpost::Alerts alerts;
    Outpost::Snapshot after = With(At(2), {.kind = Outpost::EventKind::PiratesCleared, .sector = 8, .position = {.zMeters = 500.0f}});
    after = With(
      after, {.kind = Outpost::EventKind::ShipRetreating, .sector = 2, .position = {.zMeters = -700.0f}, .subject = Outpost::EntityId{30}});
    alerts.Observe(after, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 2) == std::vector<std::string>{"Ship retreating: South", "Pirates cleared: North"});
  }

  TEST_METHOD(AlertsToPirateShipsByName)
  {
    Outpost::Alerts alerts;
    alerts.Observe(With(At(2), {.kind = Outpost::EventKind::EnemyEntered,
                                .sector = 2,
                                .position = {.xMeters = 300.0f, .zMeters = -800.0f},
                                .other = Outpost::PIRATES}),
                   TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 2) == std::vector<std::string>{"Pirate ships in South"});
  }

  TEST_METHOD(AlertsToARelaySuppressedOrHitAndARigLost)
  {
    Outpost::Alerts alerts;
    alerts.Observe(With(At(2), {.kind = Outpost::EventKind::RelayAttacked,
                                .sector = 2,
                                .position = {.zMeters = -500.0f},
                                .subject = RELAY,
                                .structure = Outpost::StructureKind::Relay,
                                .other = ENEMY}),
                   TICKS_PER_SECOND);
    alerts.Observe(With(At(3), {.kind = Outpost::EventKind::RelaySuppressed, .sector = 2, .position = {.zMeters = -500.0f}}),
                   TICKS_PER_SECOND);
    Outpost::Snapshot lost = With(At(4), {.kind = Outpost::EventKind::StructureLost,
                                          .sector = 2,
                                          .position = {.xMeters = -200.0f, .zMeters = -900.0f},
                                          .subject = Outpost::EntityId{40},
                                          .structure = Outpost::StructureKind::MiningRig});
    // A rig in no sector is lost in the field.
    lost = With(lost, {.kind = Outpost::EventKind::StructureLost,
                       .position = {.xMeters = 4000.0f},
                       .subject = Outpost::EntityId{41},
                       .structure = Outpost::StructureKind::MiningRig});
    alerts.Observe(lost, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 4) == std::vector<std::string>{"Mining Rig lost: the field", "Mining Rig lost: South",
                                                                "Relay suppressed: South", "Relay under attack: South"});

    alerts.Reset();
    Assert::IsNull(alerts.Newest());
  }

  // What the player built and lost, other than a rig, and the sectors it gained and lost, are for its report of a time
  // away (design §11), not alerts.
  TEST_METHOD(RaisesNoAlertForWhatTheReportTells)
  {
    Outpost::Alerts alerts;
    Outpost::Snapshot snapshot = At(2);
    for (const Outpost::EventKind kind : {Outpost::EventKind::ShipBuilt, Outpost::EventKind::StructureBuilt, Outpost::EventKind::ShipLost,
                                          Outpost::EventKind::SectorGained, Outpost::EventKind::SectorLost})
      snapshot = With(snapshot, {.kind = kind, .sector = 2});
    snapshot = With(snapshot, {.kind = Outpost::EventKind::StructureLost, .sector = 2, .structure = Outpost::StructureKind::Shipyard});
    alerts.Observe(snapshot, TICKS_PER_SECOND);
    Assert::IsNull(alerts.Newest());
  }
};
} // namespace GameAppTests
