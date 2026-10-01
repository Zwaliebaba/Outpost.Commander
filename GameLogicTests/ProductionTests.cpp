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
    const float gap = Outpost::Distance(ship.position, YARD) - arena.Get(yard).radiusMeters - ship.radiusMeters;
    Assert::IsTrue(gap >= 0.0f && gap < 10.0f, std::to_wstring(gap).c_str());
    Assert::IsTrue(ship.position.xMeters > YARD.xMeters, L"on the side facing the map's center");
  }

  // Design §5: a job that cannot be paid for waits at the front of the queue until it can.
  TEST_METHOD(AJobWaitsForTheOre)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::DesignId medium = arena.Design(BLUE, MEDIUM, MASS_DRIVER);
    const std::int32_t cost = arena.World().FindDesign(medium)->stats.cost;
    // Spend the Ore down to less than one Medium ship, on Shipyards elsewhere.
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {600.0f, -600.0f});
    for (int i = 0; i < 3; ++i)
      (void)arena.Tick({Order(BLUE, Outpost::BuildStructureCommand{.constructors = {constructor},
                                                                   .structure = Outpost::StructureKind::Shipyard,
                                                                   .position = {600.0f, -1200.0f + (static_cast<float>(i) * 300.0f)}})});
    Assert::IsTrue(arena.World().OreHundredths(BLUE) < std::int64_t{cost} * Outpost::HUNDREDTHS);

    (void)arena.Tick({Queue(BLUE, yard, medium)});
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
