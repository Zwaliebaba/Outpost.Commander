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
constexpr Outpost::WeaponId MASS_DRIVER{1};
// Where the tests build, and where a Constructor already in reach of it stands.
constexpr Outpost::PlanePosition SITE{.xMeters = 0.0f, .zMeters = 0.0f};

Outpost::Command Build(std::vector<Outpost::EntityId> _constructors, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
{
  return Order(BLUE, Outpost::BuildStructureCommand{.constructors = std::move(_constructors), .structure = _kind, .position = _position});
}

// Ticks until the structure is built, at most _limit.
std::uint32_t TicksToBuild(MatchArena& _arena, Outpost::EntityId _structure, std::uint32_t _limit)
{
  std::uint32_t ticks = 0;
  while (!_arena.Get(_structure).IsBuilt() && ticks < _limit)
  {
    _arena.Run(1);
    ++ticks;
  }
  return ticks;
}
} // namespace

TEST_CLASS(ConstructionTests)
{
public:
  // Design §6: a structure's circle overlaps nothing and stays inside the map; the Command Station is not built; and a
  // player has one Research Lab.
  TEST_METHOD(ChecksWhereAStructureMayStand)
  {
    MatchArena arena;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {-300.0f, 0.0f});
    const float shipyard = static_cast<float>(arena.StructureData(Outpost::StructureKind::Shipyard).footprintRadiusMeters);
    const auto result = [&](Outpost::StructureKind _kind, Outpost::PlanePosition _at)
    { return arena.Tick({Build({constructor}, _kind, _at)})[0]; };

    // Over an asteroid, or past the edge; and the Command Station anywhere.
    Assert::IsTrue(
      result(Outpost::StructureKind::Shipyard, {MatchArena::HOME_ASTEROID.xMeters + 60.0f, MatchArena::HOME_ASTEROID.zMeters}) ==
      Outpost::CommandResult::InvalidPlacement);
    Assert::IsTrue(result(Outpost::StructureKind::Shipyard, {2000.0f - shipyard + 1.0f, 0.0f}) == Outpost::CommandResult::InvalidPlacement);
    Assert::IsTrue(result(Outpost::StructureKind::CommandStation, SITE) == Outpost::CommandResult::NotBuildable);

    // Clear of both by a meter, and then not on top of itself.
    const Outpost::PlanePosition clear{MatchArena::HOME_ASTEROID.xMeters + MatchArena::ASTEROID_RADIUS_METERS + shipyard + 1.0f,
                                       MatchArena::HOME_ASTEROID.zMeters};
    Assert::IsTrue(result(Outpost::StructureKind::Shipyard, clear) == Outpost::CommandResult::Applied);
    Assert::IsTrue(result(Outpost::StructureKind::Shipyard, {clear.xMeters + shipyard, clear.zMeters}) ==
                   Outpost::CommandResult::InvalidPlacement);

    Assert::IsTrue(result(Outpost::StructureKind::ResearchLab, {-600.0f, -600.0f}) == Outpost::CommandResult::Applied);
    Assert::IsTrue(result(Outpost::StructureKind::ResearchLab, {600.0f, -600.0f}) == Outpost::CommandResult::LimitReached);
  }

  TEST_METHOD(OnlyConstructorsBuild)
  {
    MatchArena arena;
    const Outpost::EntityId warship = arena.Ship(BLUE, SMALL, MASS_DRIVER, {-300.0f, 0.0f});
    const Outpost::EntityId enemy = arena.World().SpawnConstructor(RED, {-300.0f, 100.0f});
    Assert::IsTrue(arena.Tick({Build({warship}, Outpost::StructureKind::Shipyard, SITE)})[0] == Outpost::CommandResult::NotAConstructor);
    Assert::IsTrue(arena.Tick({Build({enemy}, Outpost::StructureKind::Shipyard, SITE)})[0] == Outpost::CommandResult::NotOwned);
  }

  // Gate G8: one Constructor builds a structure in its build time, and a second adds half of one more. Hit points rise
  // with the work from a tenth to full, and the order ends when the structure is built.
  TEST_METHOD(BuildsFasterWithASecondConstructor)
  {
    const Outpost::StructureTuning shipyard = MatchArena().StructureData(Outpost::StructureKind::Shipyard);
    const auto buildTicks = static_cast<std::uint32_t>(shipyard.buildConstructorSeconds.value_or(0.0) * MatchArena::TICKS_PER_SECOND);
    const float reach = static_cast<float>(shipyard.footprintRadiusMeters) + 10.0f + 15.0f;
    for (const std::uint32_t crew : {1u, 2u})
    {
      MatchArena arena;
      std::vector<Outpost::EntityId> constructors;
      constructors.reserve(crew);
      for (std::uint32_t i = 0; i < crew; ++i)
        constructors.push_back(arena.World().SpawnConstructor(BLUE, {-reach, (static_cast<float>(i) * 30.0f) - 15.0f}));
      (void)arena.Tick({Build(constructors, Outpost::StructureKind::Shipyard, SITE)});
      const Outpost::Entity& site = *arena.Owned(BLUE, Outpost::EntityKind::Structure).back();
      const Outpost::EntityId id = site.id;
      // A site starts at a tenth, and the order's tick already did a tick's work.
      const std::int64_t maximum = site.maxHitPointsHundredths;
      const std::int64_t work = crew == 1 ? 1000 : 1500;
      Assert::AreEqual(static_cast<std::int32_t>((maximum / 10) + ((maximum - (maximum / 10)) * work / site.buildWorkNeeded)),
                       site.hitPointsHundredths);

      arena.Run(buildTicks / 2);
      const Outpost::Entity& half = arena.Get(id);
      Assert::IsTrue(half.hitPointsHundredths > half.maxHitPointsHundredths / 2, L"hit points rise with the work");

      const std::uint32_t ticks = (buildTicks / 2) + 1 + TicksToBuild(arena, id, buildTicks * 2);
      // One Constructor takes the build time; two take two thirds of it. The order tick did a tick's work too.
      const std::uint32_t expected = crew == 1 ? buildTicks : (buildTicks * 2 + 2) / 3;
      Assert::IsTrue(ticks >= expected && ticks <= expected + 1, std::to_wstring(ticks).c_str());
      Assert::AreEqual(arena.Get(id).maxHitPointsHundredths, arena.Get(id).hitPointsHundredths);
      for (const Outpost::EntityId constructor : constructors)
        Assert::IsTrue(arena.Get(constructor).order == Outpost::ShipOrder::None);
    }
  }

  // A Constructor ordered from afar heads for the site, and builds once in reach.
  TEST_METHOD(AConstructorGoesToItsSite)
  {
    MatchArena arena;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {-600.0f, -300.0f});
    (void)arena.Tick({Build({constructor}, Outpost::StructureKind::DefensePlatform, SITE)});
    const Outpost::EntityId site = arena.Owned(BLUE, Outpost::EntityKind::Structure).back()->id;
    const auto buildTicks = static_cast<std::uint32_t>(
      arena.StructureData(Outpost::StructureKind::DefensePlatform).buildConstructorSeconds.value_or(0.0) * MatchArena::TICKS_PER_SECOND);
    Assert::IsTrue(TicksToBuild(arena, site, 60 * MatchArena::TICKS_PER_SECOND) > buildTicks, L"travel takes time");
    Assert::IsTrue(arena.Get(site).IsBuilt());
  }

  // Structures block movement (ADR-016): a ship ordered across one goes round it, and one placed across a ship's way
  // turns it aside.
  TEST_METHOD(StructuresBlockMovement)
  {
    MatchArena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, SMALL, MASS_DRIVER, {-400.0f, 0.0f});
    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {ship}, .destination = {400.0f, 0.0f}})});
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {0.0f, -200.0f});
    (void)arena.Tick({Build({constructor}, Outpost::StructureKind::Shipyard, SITE)});
    const Outpost::Entity& site = *arena.Owned(BLUE, Outpost::EntityKind::Structure).back();
    const float clearance = site.radiusMeters + arena.Get(ship).radiusMeters;
    const Outpost::EntityId siteId = site.id;
    float closest = 1e9f;
    for (int tick = 0; tick < 30 * 20; ++tick)
    {
      arena.Run(1);
      closest = std::min(closest, Outpost::Distance(arena.Get(ship).position, arena.Get(siteId).position));
    }
    Assert::IsTrue(closest >= clearance - 0.01f, std::to_wstring(closest).c_str());
    Assert::IsTrue(Outpost::Distance(arena.Get(ship).position, {400.0f, 0.0f}) < 5.0f, L"the ship arrived");
  }

  // Design §6, §9: a right-click on a damaged friendly repairs it, at the tuning data's percentage of its hit points per
  // second for each Constructor, for nothing; the order ends when it is whole.
  TEST_METHOD(RepairsADamagedFriendly)
  {
    MatchArena arena;
    const Outpost::EntityId shipyard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, SITE);
    // A raider does some damage and leaves.
    const Outpost::EntityId raider = arena.Ship(RED, SMALL, MASS_DRIVER, {100.0f, 0.0f});
    arena.Run(10 * MatchArena::TICKS_PER_SECOND);
    (void)arena.Tick({Order(RED, Outpost::MoveCommand{.ships = {raider}, .destination = {1500.0f, 0.0f}})});
    arena.Run(10 * MatchArena::TICKS_PER_SECOND);
    const std::int32_t maximum = arena.Get(shipyard).maxHitPointsHundredths;
    const std::int32_t damaged = arena.Get(shipyard).hitPointsHundredths;
    Assert::IsTrue(damaged < maximum);

    const float reach = arena.Get(shipyard).radiusMeters + 25.0f;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {-reach, 0.0f});
    const std::int64_t ore = arena.World().OreHundredths(BLUE);
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::RepairCommand{.constructors = {constructor}, .target = shipyard})})[0] ==
                   Outpost::CommandResult::Applied);
    const auto perTick =
      static_cast<std::int32_t>(maximum * arena.TuningData().constructor.repairPercentPerSecond / 100.0 / MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(std::min(maximum, damaged + perTick), arena.Get(shipyard).hitPointsHundredths);
    arena.Run(60 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(maximum, arena.Get(shipyard).hitPointsHundredths);
    Assert::IsTrue(arena.Get(constructor).order == Outpost::ShipOrder::None);
    Assert::AreEqual(ore, arena.World().OreHundredths(BLUE), L"repair is free");

    // Whole, or an enemy's, it is not repaired.
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::RepairCommand{.constructors = {constructor}, .target = shipyard})})[0] ==
                   Outpost::CommandResult::NotRepairable);
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::RepairCommand{.constructors = {constructor}, .target = raider})})[0] ==
                   Outpost::CommandResult::NotRepairable);
  }

  // Further Constructors join a site under construction with a repair order, and speed it up.
  TEST_METHOD(ARepairOrderHelpsBuild)
  {
    MatchArena arena;
    const Outpost::EntityId first = arena.World().SpawnConstructor(BLUE, {-55.0f, 0.0f});
    (void)arena.Tick({Build({first}, Outpost::StructureKind::ResearchLab, SITE)});
    const Outpost::EntityId site = arena.Owned(BLUE, Outpost::EntityKind::Structure).back()->id;
    const Outpost::EntityId second = arena.World().SpawnConstructor(BLUE, {55.0f, 0.0f});
    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::RepairCommand{.constructors = {second}, .target = site})})[0] ==
                   Outpost::CommandResult::Applied);
    const std::int32_t before = arena.Get(site).buildWorkDone;
    arena.Run(1);
    Assert::AreEqual(1500, arena.Get(site).buildWorkDone - before, L"a tick and a half of work");
  }
};
} // namespace GameLogicTests