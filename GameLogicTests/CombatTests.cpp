#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::HullId MEDIUM{2};
constexpr Outpost::HullId LARGE{3};
constexpr Outpost::DriveId ION{1};
constexpr Outpost::DriveId FUSION{2};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId LANCE{2};
constexpr Outpost::WeaponId MISSILE_RACK{3};

// A simulation on open ground, with the repository's tuning data to make designs from.
class Arena
{
public:
  Arena()
    : m_tuning(Outpost::LoadTuning(ReadRepositoryTuning())),
      m_simulation(7, TICKS_PER_SECOND)
  {
    m_simulation.PlaceMap({.sizeMeters = 4000.0f, .minimumGapMeters = 60.0f, .starts = {}, .oreAsteroids = {}, .asteroidFields = {}});
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_simulation;
  }

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept
  {
    return m_tuning;
  }

  // A ship of _owner's design of these components, saved on first use.
  Outpost::EntityId Ship(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::DriveId _drive, Outpost::WeaponId _weapon,
                         Outpost::PlanePosition _position)
  {
    const Outpost::DesignComponents components{_hull, _drive, _weapon};
    const Outpost::ShipDesign* design = m_simulation.FindDesign(_owner, components);
    const Outpost::DesignId id = design != nullptr ? design->id
                                                   : m_simulation.SaveDesign(_owner, Outpost::DesignName(m_tuning, components), components,
                                                                             Outpost::DesignStatsFor(m_tuning, _hull, _drive, _weapon));
    return m_simulation.SpawnShip(_owner, id, _position);
  }

  // An unarmed target that cannot move, with plenty of hit points.
  Outpost::EntityId Structure(Outpost::PlayerId _owner, Outpost::PlanePosition _position, std::int32_t _armor = 0)
  {
    return m_simulation.SpawnStructure(_owner, Outpost::StructureKind::Shipyard, _position, 20.0f, 1'000'000, _armor * Outpost::HUNDREDTHS);
  }

  // Runs a tick and returns the shots fired in it.
  std::vector<Outpost::ShotView> Tick(const std::vector<Outpost::Command>& _commands = {})
  {
    (void)m_simulation.Tick(_commands);
    return m_simulation.BuildSnapshot(BLUE).shots;
  }

private:
  Outpost::Tuning m_tuning;
  Outpost::Simulation m_simulation;
};

Outpost::Command Order(Outpost::PlayerId _player, Outpost::Order _order)
{
  return {.player = _player, .order = std::move(_order)};
}
} // namespace

TEST_CLASS(CombatTests)
{
public:
  // Task 3.3: each weapon fires at its interval, and each hit does its damage after the target's armor.
  TEST_METHOD(FiresAtItsIntervalAndDamagesAfterArmor)
  {
    for (const auto [weapon, intervalTicks] : {std::pair{MASS_DRIVER, 8u}, std::pair{LANCE, 60u}})
    {
      Arena arena;
      const Outpost::EntityId ship = arena.Ship(BLUE, SMALL, ION, weapon, {});
      const Outpost::EntityId target = arena.Structure(RED, {100.0f, 0.0f}, 8);
      const std::int32_t damage = Outpost::DesignStatsFor(arena.TuningData(), SMALL, ION, weapon).damageHundredths;

      std::vector<std::uint64_t> shotTicks;
      std::int32_t previousHitPoints = arena.World().FindEntity(target)->hitPointsHundredths;
      for (std::uint32_t tick = 0; tick < 4 * 60; ++tick)
      {
        const std::vector<Outpost::ShotView> shots = arena.Tick();
        const std::int32_t hitPoints = arena.World().FindEntity(target)->hitPointsHundredths;
        if (!shots.empty())
        {
          Assert::AreEqual(size_t{1}, shots.size());
          Assert::IsTrue(shots[0].shooter == ship && shots[0].target == target && shots[0].weapon == weapon);
          shotTicks.push_back(arena.World().CurrentTick());
          Assert::AreEqual(Outpost::HitHundredths(damage, 800), previousHitPoints - hitPoints);
        }
        else
          Assert::AreEqual(previousHitPoints, hitPoints);
        previousHitPoints = hitPoints;
      }
      Assert::IsTrue(shotTicks.size() >= 3);
      // The first shot comes within one interval of the target coming into range, and the rest one interval apart.
      Assert::IsTrue(shotTicks.front() <= intervalTicks);
      for (size_t i = 1; i < shotTicks.size(); ++i)
        Assert::AreEqual(std::uint64_t{intervalTicks}, shotTicks[i] - shotTicks[i - 1]);
    }
  }

  // Task 3.3: the nearest enemy ship in range, over any structure, and kept while it lives and stays in range.
  TEST_METHOD(TargetsTheNearestShipAndKeepsIt)
  {
    Arena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, LARGE, FUSION, MASS_DRIVER, {});
    (void)arena.Structure(RED, {30.0f, 0.0f});
    const Outpost::EntityId farShip = arena.Ship(RED, SMALL, ION, LANCE, {0.0f, 110.0f});
    const Outpost::EntityId nearShip = arena.Ship(RED, SMALL, ION, LANCE, {0.0f, -90.0f});
    // Out of range: never chosen.
    (void)arena.Ship(RED, SMALL, ION, LANCE, {0.0f, 130.0f});
    (void)arena.Tick();
    Assert::IsTrue(arena.World().FindEntity(ship)->target == nearShip,
                   (L"first " + std::to_wstring(arena.World().FindEntity(ship)->target.value)).c_str());

    // A nearer one arriving does not take the target away.
    (void)arena.Ship(RED, SMALL, ION, LANCE, {50.0f, 0.0f});
    for (int tick = 0; tick < 10; ++tick)
      (void)arena.Tick();
    Assert::IsTrue(arena.World().FindEntity(ship)->target == nearShip,
                   (L"kept " + std::to_wstring(arena.World().FindEntity(ship)->target.value)).c_str());

    // Once it is destroyed, the next nearest ship is, and still not the structure.
    for (int tick = 0; tick < 200 && arena.World().FindEntity(nearShip) != nullptr; ++tick)
      (void)arena.Tick();
    Assert::IsNull(arena.World().FindEntity(nearShip));
    (void)arena.Tick();
    const Outpost::EntityId next = arena.World().FindEntity(ship)->target;
    Assert::IsTrue(next.IsValid() && next != farShip && arena.World().FindEntity(next)->kind == Outpost::EntityKind::Ship);
  }

  // Task 3.3: with no enemy ship in range, the nearest enemy structure.
  TEST_METHOD(TargetsAStructureWhenNoShipIsInRange)
  {
    Arena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, SMALL, ION, MASS_DRIVER, {});
    (void)arena.Structure(RED, {100.0f, 0.0f});
    const Outpost::EntityId nearer = arena.Structure(RED, {0.0f, 60.0f});
    (void)arena.Ship(RED, SMALL, ION, MASS_DRIVER, {0.0f, 400.0f});
    (void)arena.Tick();
    Assert::IsTrue(arena.World().FindEntity(ship)->target == nearer);
  }

  // Task 3.3: an attack-moving ship stops at its own range and fires.
  TEST_METHOD(AttackMoveStopsAtRange)
  {
    Arena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, SMALL, ION, LANCE, {});
    const Outpost::EntityId target = arena.Ship(RED, LARGE, FUSION, MASS_DRIVER, {600.0f, 0.0f});
    (void)arena.Tick({Order(BLUE, Outpost::AttackMoveCommand{.ships = {ship}, .destination = {1000.0f, 0.0f}})});
    bool fired = false;
    for (int tick = 0; tick < 20 * static_cast<int>(TICKS_PER_SECOND); ++tick)
      fired |= !arena.Tick().empty();
    Assert::IsTrue(fired);
    const float distance = Outpost::Distance(arena.World().FindEntity(ship)->position, arena.World().FindEntity(target)->position);
    // Within one tick's travel inside the Lance's 220 m, and the Mass Driver's 120 m never reached it.
    Assert::IsTrue(distance <= 220.0f && distance > 220.0f - 78.0f / TICKS_PER_SECOND - 1.0f, std::to_wstring(distance).c_str());
    Assert::AreEqual(arena.World().FindEntity(ship)->maxHitPointsHundredths, arena.World().FindEntity(ship)->hitPointsHundredths);
  }

  // Design §7, decided by the owner on 2026-10-01: a group attack-moving on an enemy stands at its own range. Its rear
  // ships move up round its front ships, which give way sideways round the target, instead of pushing them forward into
  // the enemy; so nearly every ship gets into range.
  TEST_METHOD(AGroupKeepsItsStandOff)
  {
    Arena arena;
    std::vector<Outpost::EntityId> group;
    for (int i = 0; i < 16; ++i)
    {
      // Four rows of four, 30 m apart, the front row first.
      const int row = i / 4;
      const int column = i % 4;
      group.push_back(arena.Ship(BLUE, MEDIUM, ION, LANCE, {-static_cast<float>(row) * 30.0f, static_cast<float>(column) * 30.0f - 45.0f}));
    }
    // Sturdy enough to outlast every Lance in range for the whole test.
    const Outpost::EntityId target =
      arena.World().SpawnStructure(RED, Outpost::StructureKind::Shipyard, {600.0f, 0.0f}, 20.0f, 1'000'000'000, 0);
    (void)arena.Tick({Order(BLUE, Outpost::AttackMoveCommand{.ships = group, .destination = {600.0f, 0.0f}})});
    for (int tick = 0; tick < 30 * static_cast<int>(TICKS_PER_SECOND); ++tick)
      (void)arena.Tick();

    const Outpost::PlanePosition targetPosition = arena.World().FindEntity(target)->position;
    float nearestMeters = std::numeric_limits<float>::infinity();
    for (const Outpost::EntityId id : group)
      nearestMeters = std::min(nearestMeters, Outpost::Distance(arena.World().FindEntity(id)->position, targetPosition));
    // No closer than a tick's travel inside the Lance's 220 m.
    Assert::IsTrue(nearestMeters > 220.0f - 52.0f / TICKS_PER_SECOND - 1.0f, std::to_wstring(nearestMeters).c_str());
    const auto firing =
      std::ranges::count_if(group, [&arena](Outpost::EntityId _id) { return arena.World().FindEntity(_id)->target.IsValid(); });
    Assert::IsTrue(firing >= 12, (std::to_wstring(firing) + L" of 16 in range").c_str());
  }

  // Task 5.3, ADR-014: a Missile Rack hit also hits every other enemy ship and structure whose center is within its 30 m
  // splash of the target's, each after its own armor, and never the shooter's own side.
  TEST_METHOD(SplashHitsEveryEnemyNearTheTarget)
  {
    Arena arena;
    // The enemy ships are unarmed Constructors, so that the missile is the only thing that does damage.
    arena.World().UseTuning(arena.TuningData());
    const Outpost::EntityId launcher = arena.Ship(BLUE, SMALL, ION, MISSILE_RACK, {0.0f, 0.0f});
    const Outpost::EntityId target = arena.World().SpawnConstructor(RED, {250.0f, 0.0f});
    const Outpost::EntityId near = arena.World().SpawnConstructor(RED, {250.0f, 22.0f});
    const Outpost::EntityId far = arena.World().SpawnConstructor(RED, {250.0f, -40.0f});
    // Small footprints, so that nothing is pushed apart before the first missile lands.
    const Outpost::EntityId armored =
      arena.World().SpawnStructure(RED, Outpost::StructureKind::Shipyard, {275.0f, 0.0f}, 5.0f, 1'000'000, 10 * Outpost::HUNDREDTHS);
    const Outpost::EntityId friendly =
      arena.World().SpawnStructure(BLUE, Outpost::StructureKind::Shipyard, {250.0f, -22.0f}, 5.0f, 1'000'000);

    std::vector<Outpost::ShotView> shots;
    for (int tick = 0; tick < 3 * static_cast<int>(TICKS_PER_SECOND) && shots.empty(); ++tick)
      shots = arena.Tick();
    Assert::AreEqual(size_t{1}, shots.size());
    Assert::IsTrue(shots[0].shooter == launcher && shots[0].target == target);
    Assert::AreEqual(30.0f, shots[0].splashRadiusMeters);

    const auto lost = [&arena](Outpost::EntityId _id)
    {
      const Outpost::Entity& entity = *arena.World().FindEntity(_id);
      return entity.maxHitPointsHundredths - entity.hitPointsHundredths;
    };
    // 40 against a Constructor's armor of 2, and against the structure's 10.
    Assert::AreEqual(3800, lost(target));
    Assert::AreEqual(3800, lost(near), L"22 m from the target");
    Assert::AreEqual(0, lost(far), L"40 m from the target");
    Assert::AreEqual(3000, lost(armored), L"a structure 25 m away, after its armor");
    Assert::AreEqual(0, lost(friendly), L"no friendly fire");
  }

  // Task 3.3: weapons are turrets, so a ship fires while it moves.
  TEST_METHOD(FiresWhileMoving)
  {
    Arena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, SMALL, ION, MASS_DRIVER, {-300.0f, 0.0f});
    (void)arena.Structure(RED, {0.0f, 60.0f});
    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {ship}, .destination = {300.0f, 0.0f}})});
    int shotsWhileMoving = 0;
    for (int tick = 0; tick < 10 * static_cast<int>(TICKS_PER_SECOND); ++tick)
    {
      const Outpost::PlanePosition before = arena.World().FindEntity(ship)->position;
      const bool fired = !arena.Tick().empty();
      if (fired && arena.World().FindEntity(ship)->position != before)
        ++shotsWhileMoving;
    }
    Assert::IsTrue(shotsWhileMoving >= 3, std::to_wstring(shotsWhileMoving).c_str());
  }

  // Task 3.3: an attack order closes on its target, destroys it, and ends; the target leaves the world and the snapshot
  // says where it went.
  TEST_METHOD(AnAttackOrderClosesAndDestroys)
  {
    Arena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, MEDIUM, ION, LANCE, {});
    const Outpost::EntityId target = arena.Ship(RED, SMALL, ION, MASS_DRIVER, {800.0f, 0.0f});
    (void)arena.Tick({Order(BLUE, Outpost::AttackCommand{.ships = {ship}, .target = target})});
    std::optional<Outpost::DestroyedView> destroyed;
    for (int tick = 0; tick < 60 * static_cast<int>(TICKS_PER_SECOND) && !destroyed; ++tick)
    {
      (void)arena.World().Tick({});
      const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(RED);
      if (!snapshot.destroyed.empty())
        destroyed = snapshot.destroyed.front();
    }
    Assert::IsTrue(destroyed.has_value(), L"the target was never destroyed");
    const Outpost::DestroyedView view = destroyed.value_or(Outpost::DestroyedView{});
    Assert::IsTrue(view.id == target && view.owner == RED && view.hull == SMALL);
    Assert::IsNull(arena.World().FindEntity(target));
    Assert::IsTrue(arena.World().FindEntity(ship)->order == Outpost::ShipOrder::None);
  }

  TEST_METHOD(AttackOrdersOnlyEnemies)
  {
    Arena arena;
    const Outpost::EntityId ship = arena.Ship(BLUE, SMALL, ION, LANCE, {});
    const Outpost::EntityId friendly = arena.Ship(BLUE, SMALL, ION, LANCE, {50.0f, 0.0f});
    const std::vector<Outpost::CommandResult> results = arena.World().Tick({
      Order(BLUE, Outpost::AttackCommand{.ships = {ship}, .target = friendly}),
      Order(BLUE, Outpost::AttackCommand{.ships = {ship}, .target = Outpost::EntityId{1}}),
    });
    // Entity 1 is the first thing the arena placed after the empty map: the ship itself, so also not an enemy.
    Assert::IsTrue(results[0] == Outpost::CommandResult::NotAnEnemy && results[1] == Outpost::CommandResult::NotAnEnemy);
  }

  // Task 3.3: ships that find a target together fire their first shots at random moments within one interval.
  TEST_METHOD(FirstShotsAreSpreadOverAnInterval)
  {
    Arena arena;
    for (int i = 0; i < 10; ++i)
      (void)arena.Ship(BLUE, SMALL, ION, MASS_DRIVER, {static_cast<float>(i) * 20.0f, 0.0f});
    (void)arena.Structure(RED, {90.0f, 50.0f});
    std::vector<std::uint64_t> firstShotTicks;
    std::vector<Outpost::EntityId> fired;
    for (int tick = 0; tick < 8; ++tick)
    {
      for (const Outpost::ShotView& shot : arena.Tick())
      {
        Assert::IsTrue(std::ranges::find(fired, shot.shooter) == fired.end(), L"a ship fired twice within one interval");
        fired.push_back(shot.shooter);
        firstShotTicks.push_back(arena.World().CurrentTick());
      }
    }
    Assert::AreEqual(size_t{10}, fired.size());
    Assert::IsTrue(std::ranges::min(firstShotTicks) != std::ranges::max(firstShotTicks), L"every first shot landed together");
  }

  // ADR-009: a battle replays from its seed and commands.
  TEST_METHOD(ABattleReplays)
  {
    const auto fight = []
    {
      Arena arena;
      std::vector<Outpost::EntityId> blue;
      std::vector<Outpost::EntityId> red;
      for (int i = 0; i < 12; ++i)
      {
        blue.push_back(
          arena.Ship(BLUE, i % 2 == 0 ? SMALL : MEDIUM, ION, i % 3 == 0 ? LANCE : MASS_DRIVER, {-400.0f, static_cast<float>(i) * 30.0f}));
        red.push_back(
          arena.Ship(RED, i % 2 == 0 ? MEDIUM : SMALL, ION, i % 4 == 0 ? LANCE : MASS_DRIVER, {400.0f, static_cast<float>(i) * 30.0f}));
      }
      arena.World().SetTargetRule(Outpost::TargetRule::Random);
      (void)arena.Tick({Order(BLUE, Outpost::AttackMoveCommand{.ships = blue, .destination = {400.0f, 150.0f}}),
                        Order(RED, Outpost::AttackMoveCommand{.ships = red, .destination = {-400.0f, 150.0f}})});
      for (int tick = 0; tick < 40 * static_cast<int>(TICKS_PER_SECOND); ++tick)
        (void)arena.Tick();
      return arena;
    };
    Arena first = fight();
    Arena second = fight();
    Assert::IsTrue(first.World() == second.World());
    // And something was destroyed, so the replay covered combat.
    Assert::IsTrue(first.World().Entities().size() < 24);
  }
};
} // namespace GameLogicTests