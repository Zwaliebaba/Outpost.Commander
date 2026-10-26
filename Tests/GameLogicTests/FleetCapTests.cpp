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
constexpr Outpost::PlanePosition SECOND_YARD{.xMeters = -400.0f, .zMeters = 400.0f};
constexpr Outpost::PlanePosition STATION{.xMeters = 400.0f, .zMeters = -400.0f};
// Far from everything, where parked warships keep out of the way.
constexpr Outpost::PlanePosition PARK{.xMeters = 1000.0f, .zMeters = 1200.0f};

// The repository's fleet cap at each Command Station level, and without a station level 1's (ADR-071 decision 1).
constexpr std::array<std::int32_t, 5> CAPS{12, 20, 30, 40, 50};

Outpost::Command Queue(Outpost::PlayerId _player, Outpost::EntityId _producer, Outpost::DesignId _design = {})
{
  return Order(_player, Outpost::QueueShipCommand{.producer = _producer, .design = _design});
}

size_t Warships(const MatchArena& _arena, Outpost::PlayerId _player)
{
  return static_cast<size_t>(std::ranges::count_if(_arena.Owned(_player, Outpost::EntityKind::Ship),
                                                   [](const Outpost::Entity* _ship) { return _ship->role == Outpost::ShipRole::Warship; }));
}

// _count Small warships of _player's, parked in a row from PARK.
void Park(MatchArena& _arena, Outpost::PlayerId _player, std::int32_t _count)
{
  for (std::int32_t i = 0; i < _count; ++i)
    (void)_arena.Ship(_player, SMALL, MASS_DRIVER, {.xMeters = PARK.xMeters + (30.0f * static_cast<float>(i)), .zMeters = PARK.zMeters});
}

// How long a Small hull takes to build, in ticks.
std::uint32_t SmallBuildTicks(const MatchArena& _arena)
{
  return static_cast<std::uint32_t>(_arena.TuningData().hulls[0].buildSeconds * MatchArena::TICKS_PER_SECOND);
}
} // namespace

TEST_CLASS(FleetCapTests)
{
public:
  // ADR-071 decision 3: a warship job that would take its player past the cap waits at the front of its queue, neither paid
  // for nor started, and starts once a ship is lost.
  TEST_METHOD(AJobWaitsAtTheCapUntilAShipIsLost)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    Park(arena, BLUE, CAPS[0] - 1);
    // The twelfth, alone, where an enemy platform will destroy it.
    const Outpost::PlanePosition lone{.xMeters = -1500.0f, .zMeters = -1500.0f};
    const Outpost::EntityId doomed = arena.Ship(BLUE, SMALL, MASS_DRIVER, lone);
    (void)arena.Tick({Queue(BLUE, yard, arena.Design(BLUE, SMALL, MASS_DRIVER))});
    const std::int64_t ore = arena.SimulationState().OreHundredths(BLUE);
    arena.Run(2 * SmallBuildTicks(arena));
    Assert::AreEqual(size_t{12}, Warships(arena, BLUE));
    Assert::AreEqual(ore, arena.SimulationState().OreHundredths(BLUE), L"a waiting job is not paid for");
    Assert::AreEqual(0, arena.Get(yard).jobWorkNeeded, L"nor started");
    const Outpost::Snapshot view = arena.SimulationState().BuildSnapshot(BLUE);
    Assert::AreEqual(12, view.commandPoints);
    Assert::AreEqual(CAPS[0], view.fleetCap);

    (void)arena.Structure(RED, Outpost::StructureKind::DefensePlatform, {.xMeters = lone.xMeters, .zMeters = lone.zMeters + 100.0f});
    for (std::uint32_t tick = 0; tick < 60 * MatchArena::TICKS_PER_SECOND && arena.SimulationState().FindEntity(doomed) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.SimulationState().FindEntity(doomed), L"the platform did not destroy the lone ship");
    arena.Run(1);
    Assert::IsTrue(arena.SimulationState().OreHundredths(BLUE) < ore, L"paid once there is room");
    arena.Run(SmallBuildTicks(arena));
    Assert::AreEqual(size_t{12}, Warships(arena, BLUE));
  }

  // A job a Shipyard has started counts before its ship is out: with room for one ship, only one of two Shipyards starts.
  TEST_METHOD(AJobUnderWayCounts)
  {
    MatchArena arena;
    const Outpost::EntityId first = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::EntityId second = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, SECOND_YARD);
    Park(arena, BLUE, CAPS[0] - 1);
    const Outpost::DesignId swarm = arena.Design(BLUE, SMALL, MASS_DRIVER);
    (void)arena.Tick({Queue(BLUE, first, swarm), Queue(BLUE, second, swarm)});
    arena.Run(1);
    const int started = (arena.Get(first).jobWorkNeeded > 0 ? 1 : 0) + (arena.Get(second).jobWorkNeeded > 0 ? 1 : 0);
    Assert::AreEqual(1, started);
    Assert::AreEqual(CAPS[0], arena.SimulationState().BuildSnapshot(BLUE).commandPoints, L"the job under way is counted");

    arena.Run(2 * SmallBuildTicks(arena));
    Assert::AreEqual(size_t{12}, Warships(arena, BLUE));
  }

  // A hull takes its own points: at 11 of 12, a Medium's 2 do not fit where a Small's 1 would.
  TEST_METHOD(AHullTakesItsPoints)
  {
    MatchArena arena;
    // Level 2 builds Medium hulls (ADR-064 decision 9).
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD, 2);
    Park(arena, BLUE, CAPS[0] - 1);
    (void)arena.Tick({Queue(BLUE, yard, arena.Design(BLUE, MEDIUM, MASS_DRIVER))});
    arena.Run(SmallBuildTicks(arena));
    Assert::AreEqual(0, arena.Get(yard).jobWorkNeeded, L"the Medium waits");
    Assert::AreEqual(size_t{11}, Warships(arena, BLUE));
    const Outpost::Snapshot view = arena.SimulationState().BuildSnapshot(BLUE);
    Assert::AreEqual(1, view.hulls[0].commandPoints);
    Assert::AreEqual(2, view.hulls[1].commandPoints);
    Assert::AreEqual(4, view.hulls[2].commandPoints);
  }

  // A Constructor takes none of the cap: the Command Station builds one with the fleet at its cap.
  TEST_METHOD(AConstructorIsNotCounted)
  {
    MatchArena arena;
    const Outpost::EntityId station = arena.Structure(BLUE, Outpost::StructureKind::CommandStation, STATION);
    Park(arena, BLUE, CAPS[0]);
    (void)arena.Tick({Queue(BLUE, station)});
    arena.Run(static_cast<std::uint32_t>(arena.TuningData().constructor.buildSeconds * MatchArena::TICKS_PER_SECOND) + 1);
    const auto constructors = std::ranges::count_if(arena.Owned(BLUE, Outpost::EntityKind::Ship), [](const Outpost::Entity* _ship)
                                                    { return _ship->role == Outpost::ShipRole::Constructor; });
    Assert::AreEqual(std::ptrdiff_t{1}, constructors);
    Assert::AreEqual(CAPS[0], arena.SimulationState().BuildSnapshot(BLUE).commandPoints);
  }

  // The cap follows the Command Station's level, and is level 1's without a station. The station's levels say what each
  // gives, for the Upgrade button.
  TEST_METHOD(TheCapFollowsTheStationsLevel)
  {
    {
      MatchArena arena;
      Assert::AreEqual(CAPS[0], arena.SimulationState().BuildSnapshot(BLUE).fleetCap, L"without a station");
    }
    for (std::int32_t level = 1; level <= 5; ++level)
    {
      MatchArena arena;
      (void)arena.Structure(BLUE, Outpost::StructureKind::CommandStation, STATION, level);
      Assert::AreEqual(CAPS[static_cast<size_t>(level - 1)], arena.SimulationState().BuildSnapshot(BLUE).fleetCap);
    }
    MatchArena arena;
    const Outpost::Snapshot view = arena.SimulationState().BuildSnapshot(BLUE);
    const auto station =
      std::ranges::find(view.structureTypes, Outpost::StructureKind::CommandStation, &Outpost::StructureTypeView::structure);
    Assert::IsTrue(station != view.structureTypes.end());
    for (size_t level = 0; level < station->levels.size(); ++level)
      Assert::AreEqual(CAPS[level + 1], station->levels[level].commandPoints);
  }

  // Data that sets no cap has none (ADR-071 decision 1), as data without nodes has no node cap.
  TEST_METHOD(NoCapWithoutTheData)
  {
    Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    for (Outpost::StructureTuning& structure : tuning.structures)
    {
      structure.commandPoints = 0;
      for (Outpost::StructureLevelTuning& level : structure.levels)
        level.commandPoints = 0;
    }
    Outpost::Simulation simulation(11, MatchArena::TICKS_PER_SECOND);
    simulation.PlaceMap({.sizeMeters = 4000.0f, .minimumGapMeters = 60.0f, .starts = {}, .oreAsteroids = {}, .asteroidFields = {}});
    simulation.UseTuning(tuning);
    simulation.AddPlayer(BLUE, tuning.rules.startingOre);
    simulation.SaveStartingDesigns(BLUE, tuning);
    const Outpost::StructureTuning& shipyard =
      *std::ranges::find(tuning.structures, Outpost::StructureKind::Shipyard, &Outpost::StructureTuning::kind);
    const Outpost::EntityId yard =
      simulation.SpawnStructure(BLUE, Outpost::StructureKind::Shipyard, YARD, static_cast<float>(shipyard.footprintRadiusMeters),
                                shipyard.hitPoints * Outpost::HUNDREDTHS, shipyard.armor * Outpost::HUNDREDTHS);
    const Outpost::DesignId swarm = simulation.FindDesign(BLUE, {SMALL, Outpost::DriveId{1}, MASS_DRIVER})->id;
    for (int i = 0; i < 20; ++i)
      (void)simulation.SpawnShip(BLUE, swarm, {.xMeters = PARK.xMeters + (30.0f * static_cast<float>(i)), .zMeters = PARK.zMeters});
    (void)simulation.Tick({Queue(BLUE, yard, swarm)});
    for (std::uint32_t tick = 0; tick <= static_cast<std::uint32_t>(tuning.hulls[0].buildSeconds * MatchArena::TICKS_PER_SECOND); ++tick)
      (void)simulation.Tick({});
    const auto warships =
      std::ranges::count_if(simulation.Entities(), [](const Outpost::Entity& _entity)
                            { return _entity.kind == Outpost::EntityKind::Ship && _entity.role == Outpost::ShipRole::Warship; });
    Assert::AreEqual(std::ptrdiff_t{21}, warships);
    Assert::AreEqual(0, simulation.BuildSnapshot(BLUE).fleetCap);
  }
};
} // namespace GameLogicTests
