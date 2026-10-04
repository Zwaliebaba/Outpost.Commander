#include "pch.h"
#include "MatchArena.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = MatchArena::BLUE;
constexpr Outpost::PlayerId RED = MatchArena::RED;
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::HullId MEDIUM{2};
constexpr Outpost::HullId LARGE{3};
constexpr Outpost::DriveId ION{1};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::PlanePosition YARD{.xMeters = -400.0f, .zMeters = 0.0f};

Outpost::Command Queue(Outpost::PlayerId _player, Outpost::EntityId _producer, Outpost::DesignId _design = {})
{
  return Order(_player, Outpost::QueueShipCommand{.producer = _producer, .design = _design});
}

size_t Warships(const MatchArena& _arena, Outpost::PlayerId _player)
{
  return static_cast<size_t>(std::ranges::count_if(_arena.Owned(_player, Outpost::EntityKind::Ship),
                                                   [](const Outpost::Entity* _ship) { return _ship->role == Outpost::ShipRole::Warship; }));
}
} // namespace

TEST_CLASS(ProductionTests)
{
public:
  // Design §6: a queue holds five jobs, and a sixth is refused.
  TEST_METHOD(AQueueHoldsFiveJobs)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::DesignId design = arena.Design(BLUE, SMALL, MASS_DRIVER);
    for (int i = 0; i < 5; ++i)
      Assert::IsTrue(arena.Tick({Queue(BLUE, yard, design)})[0] == Outpost::CommandResult::Applied);
    Assert::IsTrue(arena.Tick({Queue(BLUE, yard, design)})[0] == Outpost::CommandResult::QueueFull);
    Assert::AreEqual(size_t{5}, arena.World().BuildSnapshot(BLUE).entities.back().queue.size());
  }

  // Design §5, §6: a job is paid for when it starts, at the front of the queue, and takes its design's build time; the
  // ship appears beside the Shipyard.
  TEST_METHOD(BuildsInTheDesignsTimeAndPaysAtTheStart)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::DesignId design = arena.Design(BLUE, SMALL, MASS_DRIVER);
    const Outpost::DesignStats& stats = arena.World().FindDesign(design)->stats;
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    const std::int64_t cost = std::int64_t{stats.cost} * Outpost::HUNDREDTHS;

    (void)arena.Tick({Queue(BLUE, yard, design), Queue(BLUE, yard, design)});
    Assert::AreEqual(start - cost, arena.World().OreHundredths(BLUE), L"only the first job has started");

    const auto buildTicks = static_cast<std::uint32_t>(stats.buildSeconds * MatchArena::TICKS_PER_SECOND);
    arena.Run(buildTicks - 2);
    Assert::AreEqual(size_t{0}, Warships(arena, BLUE));
    arena.Run(1);
    Assert::AreEqual(size_t{1}, Warships(arena, BLUE), L"the first ship is done in its build time");
    Assert::AreEqual(start - cost, arena.World().OreHundredths(BLUE));
    arena.Run(1);
    Assert::AreEqual(start - (2 * cost), arena.World().OreHundredths(BLUE), L"the second job starts the next tick");

    const Outpost::Entity& ship = *arena.Owned(BLUE, Outpost::EntityKind::Ship).back();
    Assert::IsTrue(ship.design == design);
    // Every player's snapshot carries the ship's drive, which the client colors its exhaust by (ADR-019).
    const std::vector<Outpost::EntityView> seen = arena.World().BuildSnapshot(RED).entities;
    const auto view = std::ranges::find(seen, ship.id, &Outpost::EntityView::id);
    Assert::IsTrue(view != seen.end() && view->drive == arena.World().FindDesign(design)->components.drive);
    const float gap = Outpost::Distance(ship.position, YARD) - arena.Get(yard).radiusMeters - ship.radiusMeters;
    Assert::IsTrue(gap >= 0.0f && gap < 10.0f, std::to_wstring(gap).c_str());
    Assert::IsTrue(ship.position.xMeters > YARD.xMeters, L"on the side facing the map's center");
  }

  // Phase 1 design §11: Shipyards are numbered as they are finished, each per player and never reused, and count the
  // ships they build. Only their owner sees either.
  TEST_METHOD(NumbersShipyardsAndCountsTheirShips)
  {
    MatchArena arena;
    const Outpost::EntityId first = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::EntityId second = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, {.xMeters = -400.0f, .zMeters = 200.0f});
    const Outpost::EntityId theirs = arena.Structure(RED, Outpost::StructureKind::Shipyard, {.xMeters = 400.0f, .zMeters = 0.0f});
    const Outpost::DesignId design = arena.Design(BLUE, SMALL, MASS_DRIVER);
    (void)arena.Tick({Queue(BLUE, second, design)});
    Assert::AreEqual(1u, arena.Get(first).shipyardNumber);
    Assert::AreEqual(2u, arena.Get(second).shipyardNumber);
    Assert::AreEqual(1u, arena.Get(theirs).shipyardNumber, L"each player counts its own");

    arena.Run(static_cast<std::uint32_t>(arena.World().FindDesign(design)->stats.buildSeconds * MatchArena::TICKS_PER_SECOND));
    Assert::AreEqual(1u, arena.Get(second).shipsBuilt);
    Assert::AreEqual(0u, arena.Get(first).shipsBuilt);

    const auto viewOf = [&arena](Outpost::PlayerId _player, Outpost::EntityId _yard)
    {
      const std::vector<Outpost::EntityView> entities = arena.World().BuildSnapshot(_player).entities;
      return *std::ranges::find(entities, _yard, &Outpost::EntityView::id);
    };
    Assert::AreEqual(2u, viewOf(BLUE, second).shipyardNumber);
    Assert::AreEqual(1u, viewOf(BLUE, second).shipsBuilt);
    Assert::AreEqual(0u, viewOf(RED, second).shipyardNumber, L"the enemy does not see it");
    Assert::AreEqual(0u, viewOf(RED, second).shipsBuilt);
  }

  // Phase 3 design §5, gate K1: a Shipyard builds Small hulls at level 1, Medium too at level 2, and Large too at level 3;
  // a job above its level is refused. A Large hull also needs its research, which saving the design checks.
  TEST_METHOD(BuildsTheHullsOfItsLevel)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::StructureTuning& tuning = arena.StructureData(Outpost::StructureKind::Shipyard);
    const Outpost::DesignComponents large{.hull = LARGE, .drive = ION, .weapon = MASS_DRIVER};
    const std::array<Outpost::DesignId, 3> designs{
      arena.Design(BLUE, SMALL, MASS_DRIVER), arena.Design(BLUE, MEDIUM, MASS_DRIVER),
      arena.World().SaveDesign(BLUE, "Large", large, Outpost::DesignStatsFor(arena.TuningData(), large))};
    Assert::AreEqual(1, Outpost::ShipyardLevelFor(arena.TuningData(), SMALL));
    Assert::AreEqual(2, Outpost::ShipyardLevelFor(arena.TuningData(), MEDIUM));
    Assert::AreEqual(3, Outpost::ShipyardLevelFor(arena.TuningData(), LARGE));

    const Outpost::EntityId constructor = arena.World().SpawnConstructor(
      BLUE, {.xMeters = YARD.xMeters - static_cast<float>(tuning.footprintRadiusMeters) - 25.0f, .zMeters = YARD.zMeters});
    for (std::int32_t level = 1; level <= 3; ++level)
    {
      Assert::AreEqual(level, arena.Get(yard).level);
      for (std::size_t hull = 0; hull < designs.size(); ++hull)
      {
        const Outpost::CommandResult result = arena.Tick({Queue(BLUE, yard, designs[hull])})[0];
        const bool builds = std::cmp_less(hull, level);
        Assert::IsTrue(result == (builds ? Outpost::CommandResult::Applied : Outpost::CommandResult::LevelTooLow),
                       std::format(L"level {}, hull {}", level, hull + 1).c_str());
      }
      if (level == 3)
        break;
      // The next level, during which the jobs queued so far are built; the starting Ore pays for both levels and them.
      Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::UpgradeStructureCommand{.structure = yard, .constructors = {constructor}})})[0] ==
                     Outpost::CommandResult::Applied);
      for (int tick = 0; tick < 300 * 20 && arena.Get(yard).level == level; ++tick)
        arena.Run(1);
    }
    const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
    Assert::AreEqual(2, std::ranges::find(snapshot.hulls, MEDIUM, &Outpost::HullView::id)->shipyardLevel,
                     L"the client knows each hull's level");
  }

  // Design §5: a job that cannot be paid for waits at the front of the queue until it can.
  TEST_METHOD(AJobWaitsForTheOre)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    // A Small ship, which a level 1 Shipyard builds (Phase 3 design §5).
    const Outpost::DesignId small = arena.Design(BLUE, SMALL, MASS_DRIVER);
    const std::int32_t cost = arena.World().FindDesign(small)->stats.cost;
    // Spend the Ore down to less than one Small ship, on a rig site and Shipyards elsewhere, which the Constructor leaves
    // unfinished as it goes from one order to the next.
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {600.0f, -600.0f});
    (void)arena.Tick({Order(BLUE, Outpost::BuildStructureCommand{.constructors = {constructor},
                                                                 .structure = Outpost::StructureKind::MiningRig,
                                                                 .position = MatchArena::CONTESTED_ASTEROID})});
    for (int i = 0; i < 3; ++i)
    {
      (void)arena.Tick({Order(BLUE, Outpost::BuildStructureCommand{.constructors = {constructor},
                                                                   .structure = Outpost::StructureKind::Shipyard,
                                                                   .position = {600.0f, -1200.0f + (static_cast<float>(i) * 300.0f)}})});
    }
    Assert::IsTrue(arena.World().OreHundredths(BLUE) < std::int64_t{cost} * Outpost::HUNDREDTHS);

    (void)arena.Tick({Queue(BLUE, yard, small)});
    arena.Run(40 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(size_t{0}, Warships(arena, BLUE));
    Assert::AreEqual(0, arena.World().BuildSnapshot(BLUE).entities[arena.Get(yard).id.value - 1].jobPermille, L"waiting");

    // A rig pays for it.
    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    arena.Run(60 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(size_t{1}, Warships(arena, BLUE));
  }

  // Design §6: the Command Station builds Constructors, in the tuning data's time and for its cost.
  TEST_METHOD(TheCommandStationBuildsConstructors)
  {
    MatchArena arena;
    const Outpost::EntityId station = arena.Structure(BLUE, Outpost::StructureKind::CommandStation, YARD);
    const Outpost::ConstructorTuning& constructor = arena.TuningData().constructor;
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    (void)arena.Tick({Queue(BLUE, station)});
    Assert::AreEqual(start - (std::int64_t{constructor.cost} * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE));
    arena.Run(static_cast<std::uint32_t>(constructor.buildSeconds * MatchArena::TICKS_PER_SECOND));
    const std::vector<const Outpost::Entity*> ships = arena.Owned(BLUE, Outpost::EntityKind::Ship);
    Assert::AreEqual(size_t{1}, ships.size());
    Assert::IsTrue(ships[0]->role == Outpost::ShipRole::Constructor);
  }

  TEST_METHOD(RefusesWhatCannotProduce)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {400.0f, 0.0f});
    const Outpost::DesignId mine = arena.Design(BLUE, SMALL, MASS_DRIVER);
    const Outpost::DesignId theirs = arena.Design(RED, SMALL, MASS_DRIVER);
    Assert::IsTrue(arena.Tick({Queue(BLUE, lab, mine)})[0] == Outpost::CommandResult::NotAProducer);
    Assert::IsTrue(arena.Tick({Queue(RED, yard, theirs)})[0] == Outpost::CommandResult::NotAProducer);
    Assert::IsTrue(arena.Tick({Queue(BLUE, yard, theirs)})[0] == Outpost::CommandResult::UnknownDesign);

    // A Shipyard under construction does not produce yet.
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {0.0f, -600.0f});
    (void)arena.Tick(
      {Order(BLUE, Outpost::BuildStructureCommand{
                     .constructors = {constructor}, .structure = Outpost::StructureKind::Shipyard, .position = {0.0f, -400.0f}})});
    const Outpost::EntityId site = arena.Owned(BLUE, Outpost::EntityKind::Structure).back()->id;
    Assert::IsTrue(arena.Tick({Queue(BLUE, site, mine)})[0] == Outpost::CommandResult::NotAProducer);
  }
};
} // namespace GameLogicTests