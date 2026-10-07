#include "pch.h"
#include "MatchArena.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = MatchArena::BLUE;
constexpr Outpost::PlayerId RED = MatchArena::RED;
constexpr std::uint32_t TICKS_PER_SECOND = MatchArena::TICKS_PER_SECOND;
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::HullId MEDIUM{2};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId MISSILE_RACK{3};
// A Small Ion Mass Driver ship stands here, facing west, away from the Defence Platform east of it: in the gun's range,
// and out of its own, so that only the platform fires.
constexpr Outpost::PlanePosition FRONT{.xMeters = 0.0f, .zMeters = 0.0f};
constexpr Outpost::PlanePosition PLATFORM{.xMeters = 240.0f, .zMeters = 0.0f};
// The Repair Bay is the farthest of the repairers, which the ship still goes to first (Phase 4 design §10).
constexpr Outpost::PlanePosition BAY{.xMeters = -1500.0f, .zMeters = 0.0f};
constexpr Outpost::PlanePosition STATION{.xMeters = -1000.0f, .zMeters = 600.0f};
constexpr Outpost::PlanePosition SHIPYARD{.xMeters = -700.0f, .zMeters = -500.0f};

// A ship of BLUE's at the front, set to _retreat, under the fire of a Defence Platform of RED's.
Outpost::EntityId UnderFire(MatchArena& _arena, Outpost::RetreatThreshold _retreat)
{
  (void)_arena.Structure(RED, Outpost::StructureKind::DefensePlatform, PLATFORM);
  const Outpost::EntityId ship = _arena.World().SpawnShip(BLUE, _arena.Design(BLUE, SMALL, MASS_DRIVER), FRONT, std::numbers::pi_v<float>);
  (void)_arena.Tick({Order(BLUE, Outpost::SetRetreatCommand{.ships = {ship}, .retreat = _retreat})});
  return ship;
}

// Runs until _ship is retreating, at most a minute; whether it is, which it is not once it is destroyed.
bool RunUntilRetreating(MatchArena& _arena, Outpost::EntityId _ship)
{
  for (std::uint32_t tick = 0; tick < 60 * TICKS_PER_SECOND; ++tick)
  {
    const Outpost::Entity* ship = _arena.World().FindEntity(_ship);
    if (ship == nullptr || ship->retreating)
      return ship != nullptr;
    _arena.Run(1);
  }
  return _arena.Get(_ship).retreating;
}

// _ship's hit points, in percent of its full ones.
std::int64_t PercentLeft(const Outpost::Entity& _ship)
{
  return std::int64_t{_ship.hitPointsHundredths} * 100 / _ship.maxHitPointsHundredths;
}

float FootprintGap(const Outpost::Entity& _a, const Outpost::Entity& _b)
{
  return Outpost::Distance(_a.position, _b.position) - _a.radiusMeters - _b.radiusMeters;
}
} // namespace

TEST_CLASS(RetreatTests)
{
public:
  // Phase 4 design §10: a hit that leaves a ship below its threshold sends it to the nearest Repair Bay, before the Command
  // Station and a Shipyard, however much nearer they are. The bay repairs it there, and once whole it waits.
  TEST_METHOD(GoesToARepairBayAndIsRepaired)
  {
    MatchArena arena;
    const Outpost::EntityId bay = arena.Structure(BLUE, Outpost::StructureKind::RepairBay, BAY);
    (void)arena.Structure(BLUE, Outpost::StructureKind::CommandStation, STATION);
    (void)arena.Structure(BLUE, Outpost::StructureKind::Shipyard, SHIPYARD);
    const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Half);
    Assert::IsTrue(RunUntilRetreating(arena, ship), L"it went back");
    Assert::IsTrue(PercentLeft(arena.Get(ship)) < 50, L"below half");
    Assert::IsTrue(arena.Get(ship).repairer == bay, L"to the Repair Bay");

    // Its owner sees it going back; the other player does not.
    const Outpost::Snapshot blue = arena.World().BuildSnapshot(BLUE);
    const auto view = std::ranges::find(blue.entities, ship, &Outpost::EntityView::id);
    Assert::IsTrue(view->retreating && view->retreat == Outpost::RetreatThreshold::Half);

    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && arena.Get(ship).retreating; ++tick)
      arena.Run(1);
    const Outpost::Entity& whole = arena.Get(ship);
    Assert::IsFalse(whole.retreating, L"whole again");
    Assert::AreEqual(whole.maxHitPointsHundredths, whole.hitPointsHundredths);
    Assert::IsTrue(FootprintGap(whole, arena.Get(bay)) <=
                     static_cast<float>(arena.TuningData().shipRepair.value_or(Outpost::ShipRepairTuning{}).rangeMeters),
                   L"at the bay");
    arena.Run(5 * TICKS_PER_SECOND);
    Assert::IsTrue(FootprintGap(arena.Get(ship), arena.Get(bay)) <= 150.0f, L"where it waits");
  }

  // Without a Repair Bay a ship goes to the Command Station, and without that to the nearest Shipyard; with none of them it
  // has nowhere to go and fights on.
  TEST_METHOD(GoesToTheStationElseAShipyard)
  {
    {
      MatchArena arena;
      const Outpost::EntityId station = arena.Structure(BLUE, Outpost::StructureKind::CommandStation, STATION);
      (void)arena.Structure(BLUE, Outpost::StructureKind::Shipyard, SHIPYARD);
      const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Half);
      Assert::IsTrue(RunUntilRetreating(arena, ship));
      Assert::IsTrue(arena.Get(ship).repairer == station);
    }
    {
      MatchArena arena;
      (void)arena.Structure(BLUE, Outpost::StructureKind::Shipyard, {.xMeters = -1500.0f, .zMeters = 0.0f});
      const Outpost::EntityId nearer = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, SHIPYARD);
      const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Half);
      Assert::IsTrue(RunUntilRetreating(arena, ship));
      Assert::IsTrue(arena.Get(ship).repairer == nearer);
    }
    {
      MatchArena arena;
      const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Half);
      Assert::IsFalse(RunUntilRetreating(arena, ship), L"nowhere to go");
    }
  }

  // Design §10: a ship set never to retreat fights on, and the default is a quarter of its hit points.
  TEST_METHOD(NeverAndAQuarter)
  {
    {
      MatchArena arena;
      (void)arena.Structure(BLUE, Outpost::StructureKind::RepairBay, BAY);
      const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Never);
      for (std::uint32_t tick = 0; tick < 30 * TICKS_PER_SECOND && arena.World().FindEntity(ship) != nullptr; ++tick)
      {
        Assert::IsFalse(arena.Get(ship).retreating);
        arena.Run(1);
      }
      Assert::IsNull(arena.World().FindEntity(ship), L"it fought to the end");
    }
    {
      MatchArena arena;
      (void)arena.Structure(BLUE, Outpost::StructureKind::RepairBay, BAY);
      const Outpost::EntityId ship =
        arena.World().SpawnShip(BLUE, arena.Design(BLUE, SMALL, MASS_DRIVER), FRONT, std::numbers::pi_v<float>);
      Assert::IsTrue(arena.Get(ship).retreat == Outpost::DEFAULT_RETREAT && Outpost::DEFAULT_RETREAT == Outpost::RetreatThreshold::Quarter);
      const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {.xMeters = 0.0f, .zMeters = 40.0f});
      Assert::IsTrue(arena.Get(constructor).retreat == Outpost::RetreatThreshold::Quarter, L"a Constructor too");
      (void)arena.Structure(RED, Outpost::StructureKind::DefensePlatform, PLATFORM);
      Assert::IsTrue(RunUntilRetreating(arena, ship));
      Assert::IsTrue(PercentLeft(arena.Get(ship)) < 25 && PercentLeft(arena.Get(ship)) > 0);
    }
  }

  // Any order a player gives a retreating ship ends its retreat; the next hit while it is below its threshold starts it
  // again. Setting it never to retreat ends it too, and the setting is its owner's to give.
  TEST_METHOD(AnOrderEndsARetreatAndAHitStartsItAgain)
  {
    MatchArena arena;
    (void)arena.Structure(BLUE, Outpost::StructureKind::RepairBay, BAY);
    const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Half);
    Assert::IsTrue(RunUntilRetreating(arena, ship));
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {ship}, .destination = FRONT})})[0] ==
                   Outpost::CommandResult::Applied);
    Assert::IsFalse(arena.Get(ship).retreating, L"the order ended it");
    Assert::IsTrue(RunUntilRetreating(arena, ship), L"a hit started it again");

    Assert::IsTrue(arena.Tick({Order(RED, Outpost::SetRetreatCommand{.ships = {ship}, .retreat = Outpost::RetreatThreshold::Never})})[0] ==
                   Outpost::CommandResult::NotOwned);
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::SetRetreatCommand{.ships = {ship}, .retreat = Outpost::RetreatThreshold::Never})})[0] ==
                   Outpost::CommandResult::Applied);
    Assert::IsFalse(arena.Get(ship).retreating, L"never to retreat");
    Assert::IsTrue(arena.Get(ship).retreat == Outpost::RetreatThreshold::Never);
  }

  // Design §10: a ship on a standing order keeps it while it is repaired, and goes back to it once whole (ADR-059).
  TEST_METHOD(AStandingOrderWaitsForTheRepair)
  {
    MatchArena arena;
    (void)arena.Structure(BLUE, Outpost::StructureKind::RepairBay, BAY);
    const Outpost::EntityId ship = UnderFire(arena, Outpost::RetreatThreshold::Half);
    const Outpost::PlanePosition patrolEnd{.xMeters = 100.0f, .zMeters = 0.0f};
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::PatrolCommand{.ships = {ship}, .destination = patrolEnd})})[0] ==
                   Outpost::CommandResult::Applied);
    Assert::IsTrue(RunUntilRetreating(arena, ship));
    Assert::IsTrue(arena.Get(ship).standing == Outpost::StandingOrder::Patrol, L"kept while it goes back");
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && arena.Get(ship).retreating; ++tick)
      arena.Run(1);
    Assert::IsFalse(arena.Get(ship).retreating);
    arena.Run(2 * TICKS_PER_SECOND);
    Assert::IsTrue(arena.Get(ship).order == Outpost::ShipOrder::AttackMove, L"on patrol again");
  }

  // Design §10: a repairer repairs at most four of its player's ships in its reach at a time, the most damaged first, by
  // 3% of each one's full hit points a second, for nothing.
  TEST_METHOD(RepairsFourShipsAtATime)
  {
    MatchArena arena;
    // Four Small hulls and a Medium one, all within the splash of a missile at the middle one, which a hit takes a smaller
    // share of; the Medium has the lowest identifier, so that it waits for its damage, not its place. They do not retreat,
    // so that the missiles alone move nothing.
    const Outpost::EntityId medium =
      arena.World().SpawnShip(BLUE, arena.Design(BLUE, MEDIUM, MASS_DRIVER), {.xMeters = 25.0f, .zMeters = 0.0f});
    std::vector<Outpost::EntityId> ships{medium};
    for (const Outpost::PlanePosition at :
         {Outpost::PlanePosition{.xMeters = 0.0f, .zMeters = 0.0f}, Outpost::PlanePosition{.zMeters = 20.0f},
          Outpost::PlanePosition{.zMeters = -20.0f}, Outpost::PlanePosition{.xMeters = -20.0f}})
      ships.push_back(arena.World().SpawnShip(BLUE, arena.Design(BLUE, SMALL, MASS_DRIVER), at));
    // Research unlocks the Missile Rack, so RED's design is saved here.
    const Outpost::DesignComponents rack{SMALL, Outpost::DriveId{1}, MISSILE_RACK};
    const Outpost::DesignId launcherDesign =
      arena.World().SaveDesign(RED, "Launcher", rack, Outpost::DesignStatsFor(arena.TuningData(), rack));
    const Outpost::EntityId launcher = arena.World().SpawnShip(RED, launcherDesign, {.xMeters = 0.0f, .zMeters = 275.0f});
    (void)arena.Tick({Order(BLUE, Outpost::SetRetreatCommand{.ships = ships, .retreat = Outpost::RetreatThreshold::Never}),
                      Order(RED, Outpost::AttackCommand{.ships = {launcher}, .target = ships[1]})});
    arena.Run(7 * TICKS_PER_SECOND);
    (void)arena.Tick({Order(RED, Outpost::MoveCommand{.ships = {launcher}, .destination = {.xMeters = 1800.0f, .zMeters = 0.0f}})});
    arena.Run(3 * TICKS_PER_SECOND);
    for (const Outpost::EntityId id : ships)
      Assert::IsTrue(PercentLeft(arena.Get(id)) < 100, L"every one is damaged");

    const Outpost::EntityId bay = arena.Structure(BLUE, Outpost::StructureKind::RepairBay, {.xMeters = -120.0f, .zMeters = 0.0f});
    std::vector<std::int32_t> before;
    before.reserve(ships.size());
    for (const Outpost::EntityId id : ships)
      before.push_back(arena.Get(id).hitPointsHundredths);
    arena.Run(1);
    const double percent = arena.TuningData().shipRepair.value_or(Outpost::ShipRepairTuning{}).percentPerSecond;
    for (size_t i = 0; i < ships.size(); ++i)
    {
      const Outpost::Entity& ship = arena.Get(ships[i]);
      Assert::IsTrue(FootprintGap(ship, arena.Get(bay)) <= 150.0f);
      const auto perTick = static_cast<std::int32_t>(std::llround(ship.maxHitPointsHundredths * percent / 100.0 / TICKS_PER_SECOND));
      const std::int32_t expected = ships[i] == medium ? before[i] : before[i] + perTick;
      Assert::AreEqual(expected, ship.hitPointsHundredths, ships[i] == medium ? L"the least damaged waits" : L"repaired");
    }
  }

  // Design §10: a design carries a retreat, which every ship built to it starts with, and which its owner sets when it
  // saves the design or renames it.
  TEST_METHOD(ADesignCarriesItsRetreat)
  {
    MatchArena arena;
    const Outpost::DesignId design = arena.Design(BLUE, SMALL, MASS_DRIVER);
    Assert::IsTrue(arena.World().FindDesign(design)->retreat == Outpost::RetreatThreshold::Quarter);
    const Outpost::ShipDesign& saved = *arena.World().FindDesign(design);
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::SaveDesignCommand{.design = design,
                                                                      .nameUtf8 = "Picket",
                                                                      .hull = saved.components.hull,
                                                                      .drive = saved.components.drive,
                                                                      .weapon = saved.components.weapon,
                                                                      .retreat = Outpost::RetreatThreshold::Never})})[0] ==
                   Outpost::CommandResult::Applied);
    Assert::IsTrue(arena.World().FindDesign(design)->retreat == Outpost::RetreatThreshold::Never);
    const Outpost::EntityId ship = arena.World().SpawnShip(BLUE, design, FRONT);
    Assert::IsTrue(arena.Get(ship).retreat == Outpost::RetreatThreshold::Never);
    const Outpost::Snapshot blue = arena.World().BuildSnapshot(BLUE);
    Assert::IsTrue(std::ranges::find(blue.designs, design, &Outpost::DesignView::id)->retreat == Outpost::RetreatThreshold::Never);
  }
};
} // namespace GameLogicTests
