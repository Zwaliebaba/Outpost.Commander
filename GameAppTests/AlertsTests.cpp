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

Outpost::EntityView EnemyWarship(std::uint32_t _id, Outpost::PlanePosition _position)
{
  return {.id = Outpost::EntityId{_id}, .kind = Outpost::EntityKind::Ship, .owner = ENEMY, .position = _position};
}

std::vector<std::string> Texts(const Outpost::Alerts& _alerts, std::uint64_t _tick)
{
  std::vector<std::string> texts;
  for (const Outpost::Alerts::Alert& alert : _alerts.Shown(_tick, TICKS_PER_SECOND))
    texts.push_back(alert.text);
  return texts;
}
} // namespace

// Phase 2 design §9, ADR-059: the alerts the client makes from its snapshots.
TEST_CLASS(AlertsTests)
{
public:
  TEST_METHOD(AlertsToEnemyShipsInAHeldSectorOnce)
  {
    Outpost::Alerts alerts;
    alerts.Observe(At(1), TICKS_PER_SECOND);
    Assert::IsNull(alerts.Newest());

    Outpost::Snapshot entered = At(2);
    entered.entities.push_back(EnemyWarship(20, {.xMeters = 300.0f, .zMeters = -800.0f}));
    entered.entities.push_back(EnemyWarship(21, {.xMeters = 310.0f, .zMeters = -800.0f}));
    // An enemy in its own sector alerts nothing.
    entered.entities.push_back(EnemyWarship(22, {.xMeters = 0.0f, .zMeters = 600.0f}));
    alerts.Observe(entered, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 2) == std::vector<std::string>{"Enemy ships in South"});
    Assert::IsNotNull(alerts.Newest());
    Assert::IsTrue(alerts.Newest()->position == Outpost::PlanePosition{.xMeters = 300.0f, .zMeters = -800.0f});

    // Still there: no new alert. Gone, and back soon after: none either, within the repeat time.
    entered.tick = 3;
    alerts.Observe(entered, TICKS_PER_SECOND);
    alerts.Observe(At(4), TICKS_PER_SECOND);
    entered.tick = 5;
    alerts.Observe(entered, TICKS_PER_SECOND);
    Assert::AreEqual(size_t{1}, alerts.Shown(5, TICKS_PER_SECOND).size());

    // It shows for eight seconds.
    Assert::AreEqual(size_t{1}, alerts.Shown(2 + (8 * TICKS_PER_SECOND) - 1, TICKS_PER_SECOND).size());
    Assert::IsTrue(alerts.Shown(2 + (8 * TICKS_PER_SECOND), TICKS_PER_SECOND).empty());
  }

  // ADR-073: pirates in a held sector, as the ships an outpost has left once the player has claimed it, are named so.
  // Phase 4 design §13: one of the player's ships going back to be repaired, as it starts, and a sector the pirates guarded
  // cleared of them.
  TEST_METHOD(AlertsToARetreatAndPiratesCleared)
  {
    Outpost::Alerts alerts;
    Outpost::Snapshot before = At(1);
    before.sectors[1].guarded = true;
    Outpost::EntityView ship{
      .id = Outpost::EntityId{30}, .kind = Outpost::EntityKind::Ship, .owner = PLAYER, .position = {.zMeters = -700.0f}};
    before.entities.push_back(ship);
    alerts.Observe(before, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 1).empty());

    Outpost::Snapshot after = At(2);
    ship.retreating = true;
    after.entities.push_back(ship);
    alerts.Observe(after, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 2) == std::vector<std::string>{"Pirates cleared: North", "Ship retreating: South"});
    after.tick = 3;
    alerts.Observe(after, TICKS_PER_SECOND);
    Assert::AreEqual(size_t{2}, Texts(alerts, 3).size(), L"once each");
  }

  TEST_METHOD(AlertsToPirateShipsByName)
  {
    Outpost::Alerts alerts;
    Outpost::Snapshot entered = At(2);
    Outpost::EntityView pirate = EnemyWarship(20, {.xMeters = 300.0f, .zMeters = -800.0f});
    pirate.owner = Outpost::PIRATES;
    entered.entities.push_back(pirate);
    alerts.Observe(entered, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 2) == std::vector<std::string>{"Pirate ships in South"});
  }

  TEST_METHOD(AlertsToARelaySuppressedOrHitAndARigLost)
  {
    Outpost::Alerts alerts;
    alerts.Observe(At(1), TICKS_PER_SECOND);
    Outpost::Snapshot hit = At(2);
    hit.shots = {{.shooter = Outpost::EntityId{30}, .target = RELAY}};
    alerts.Observe(hit, TICKS_PER_SECOND);
    Outpost::Snapshot suppressed = At(3);
    suppressed.sectors[0].suppressed = true;
    alerts.Observe(suppressed, TICKS_PER_SECOND);
    Outpost::Snapshot lost = At(4);
    lost.destroyed = {{.id = Outpost::EntityId{40},
                       .kind = Outpost::EntityKind::Structure,
                       .structure = Outpost::StructureKind::MiningRig,
                       .owner = PLAYER,
                       .position = {.xMeters = -200.0f, .zMeters = -900.0f}},
                      {.id = Outpost::EntityId{41},
                       .kind = Outpost::EntityKind::Structure,
                       .structure = Outpost::StructureKind::MiningRig,
                       .owner = ENEMY,
                       .position = {.xMeters = 0.0f, .zMeters = 900.0f}}};
    alerts.Observe(lost, TICKS_PER_SECOND);
    Assert::IsTrue(Texts(alerts, 4) ==
                   std::vector<std::string>{"Mining Rig lost: South", "Relay suppressed: South", "Relay under attack: South"});

    alerts.Reset();
    Assert::IsNull(alerts.Newest());
  }
};
} // namespace GameAppTests
