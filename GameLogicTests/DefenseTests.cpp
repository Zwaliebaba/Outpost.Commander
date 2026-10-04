#include "pch.h"
#include "MatchArena.h"

#include <set>

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
constexpr Outpost::WeaponId LANCE{2};
constexpr Outpost::PlanePosition PLATFORM{.xMeters = 0.0f, .zMeters = 0.0f};

// The shots of the next tick.
std::vector<Outpost::ShotView> TickShots(MatchArena& _arena)
{
  _arena.Run(1);
  return _arena.World().BuildSnapshot(BLUE).shots;
}
} // namespace

TEST_CLASS(DefenseTests)
{
public:
  // Design §6, §12: the Defence gun does 30 a second at 250 m, past the Lance's 220 m and short of the Missile Rack's
  // 280 m.
  TEST_METHOD(TheGunHasTheLadderRange)
  {
    MatchArena arena;
    const Outpost::StructureWeaponTuning& gun = arena.TuningData().structureWeapons.front();
    const auto range = [&](Outpost::WeaponId _id)
    { return std::ranges::find(arena.TuningData().weapons, _id, &Outpost::WeaponTuning::id)->rangeMeters; };
    Assert::IsTrue(gun.rangeMeters > range(LANCE));
    Assert::IsTrue(gun.rangeMeters < range(Outpost::WeaponId{3}));
    Assert::AreEqual(30, gun.damage);
    Assert::AreEqual(1.0, gun.fireIntervalSeconds);

    // A Lance ship standing just outside its own range of the platform is in the gun's, and is shot at.
    const Outpost::EntityId platform = arena.Structure(BLUE, Outpost::StructureKind::DefensePlatform, PLATFORM);
    const Outpost::EntityId lance = arena.Ship(RED, MEDIUM, LANCE, {static_cast<float>(range(LANCE)) + 10.0f, 0.0f});
    std::vector<Outpost::ShotView> shots;
    for (std::uint32_t tick = 0; tick < 2 * MatchArena::TICKS_PER_SECOND; ++tick)
    {
      const std::vector<Outpost::ShotView> tickShots = TickShots(arena);
      shots.insert(shots.end(), tickShots.begin(), tickShots.end());
    }
    Assert::IsFalse(shots.empty(), L"the platform fired");
    for (const Outpost::ShotView& shot : shots)
      Assert::IsTrue(shot.shooter == platform && shot.target == lance, L"only the platform reaches");
  }

  // Design §6: armor 10 cuts a Mass Driver hit from 14 to 4 on the armed structures, and every other structure has none.
  TEST_METHOD(ArmorCutsAMassDriverHitToFour)
  {
    MatchArena arena;
    const Outpost::EntityId platform = arena.Structure(BLUE, Outpost::StructureKind::DefensePlatform, PLATFORM);
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, {600.0f, 0.0f});
    Assert::AreEqual(10 * Outpost::HUNDREDTHS, arena.Get(platform).armorHundredths);
    Assert::AreEqual(0, arena.Get(yard).armorHundredths);
    Assert::AreEqual(400, Outpost::HitHundredths(14 * Outpost::HUNDREDTHS, arena.Get(platform).armorHundredths));

    (void)arena.Ship(RED, SMALL, MASS_DRIVER, {100.0f, 0.0f});
    const std::int32_t before = arena.Get(platform).hitPointsHundredths;
    std::int32_t hits = 0;
    for (std::uint32_t tick = 0; tick < MatchArena::TICKS_PER_SECOND; ++tick)
    {
      for (const Outpost::ShotView& shot : TickShots(arena))
        hits += shot.target == platform ? 1 : 0;
    }
    Assert::IsTrue(hits > 0);
    Assert::AreEqual(before - (hits * 400), arena.Get(platform).hitPointsHundredths);
  }

  // The Command Station carries the gun too (design §6); a Shipyard does not, and a platform under construction does not
  // fire.
  TEST_METHOD(OnlyBuiltArmedStructuresFire)
  {
    MatchArena arena;
    const Outpost::EntityId station = arena.Structure(BLUE, Outpost::StructureKind::CommandStation, PLATFORM);
    (void)arena.Structure(BLUE, Outpost::StructureKind::Shipyard, {0.0f, 500.0f});
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {-900.0f, -900.0f});
    (void)arena.Tick(
      {Order(BLUE, Outpost::BuildStructureCommand{
                     .constructors = {constructor}, .structure = Outpost::StructureKind::DefensePlatform, .position = {0.0f, -500.0f}})});
    // An enemy in range of all three, and a Constructor too far to build.
    (void)arena.Ship(RED, SMALL, MASS_DRIVER, {200.0f, 0.0f});
    (void)arena.Ship(RED, SMALL, MASS_DRIVER, {200.0f, 500.0f});
    (void)arena.Ship(RED, SMALL, MASS_DRIVER, {200.0f, -500.0f});
    std::vector<Outpost::EntityId> shooters;
    for (std::uint32_t tick = 0; tick < 2 * MatchArena::TICKS_PER_SECOND; ++tick)
    {
      for (const Outpost::ShotView& shot : TickShots(arena))
      {
        if (arena.World().FindEntity(shot.shooter) != nullptr && arena.Get(shot.shooter).owner == BLUE)
          shooters.push_back(shot.shooter);
      }
    }
    Assert::IsFalse(shooters.empty());
    for (const Outpost::EntityId shooter : shooters)
      Assert::IsTrue(shooter == station, L"only the Command Station fires");
  }

  // Phase 3 design §7, gate K6: a Command Station has one Defence gun at levels 1 and 2, two at 3 and 4, and three at 5,
  // each the station's gun firing on its own rhythm; a Defence Platform has one at any level. Each shot names its gun.
  TEST_METHOD(AStationsLevelGivesItsGuns)
  {
    const auto gunsFiring = [](Outpost::StructureKind _kind, std::int32_t _level)
    {
      MatchArena arena;
      (void)arena.Structure(BLUE, _kind, PLATFORM, _level);
      // A Shipyard of Red's in range: no ship, so the guns turn on it, and it stands through the ten seconds.
      (void)arena.Structure(RED, Outpost::StructureKind::Shipyard, {.xMeters = 0.0f, .zMeters = 200.0f});
      std::set<std::uint8_t> guns;
      std::size_t shots = 0;
      for (int tick = 0; tick < 10 * 20; ++tick)
      {
        for (const Outpost::ShotView& shot : TickShots(arena))
        {
          guns.insert(shot.gun);
          ++shots;
        }
      }
      // One shot a second a gun, give or take its random first shot.
      Assert::IsTrue(shots >= (guns.size() * 9) && shots <= (guns.size() * 11), std::to_wstring(shots).c_str());
      return guns.size();
    };
    Assert::AreEqual(size_t{1}, gunsFiring(Outpost::StructureKind::CommandStation, 1));
    Assert::AreEqual(size_t{1}, gunsFiring(Outpost::StructureKind::CommandStation, 2));
    Assert::AreEqual(size_t{2}, gunsFiring(Outpost::StructureKind::CommandStation, 3));
    Assert::AreEqual(size_t{2}, gunsFiring(Outpost::StructureKind::CommandStation, 4));
    Assert::AreEqual(size_t{3}, gunsFiring(Outpost::StructureKind::CommandStation, 5));
    Assert::AreEqual(size_t{1}, gunsFiring(Outpost::StructureKind::DefensePlatform, 1));
  }
};
} // namespace GameLogicTests