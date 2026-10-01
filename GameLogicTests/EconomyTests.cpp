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

Outpost::BuildStructureCommand Build(Outpost::EntityId _constructor, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
{
  return {.constructors = {_constructor}, .structure = _kind, .position = _position};
}
} // namespace

TEST_CLASS(EconomyTests)
{
public:
  // Design §5: a built Mining Rig earns its asteroid's rate, 5 Ore a second at home and 8 on a contested asteroid, a
  // twentieth of it every tick.
  TEST_METHOD(ARigEarnsItsAsteroidsRateEveryTick)
  {
    MatchArena arena;
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    arena.Run(1);
    Assert::AreEqual(start + 25, arena.World().OreHundredths(BLUE), L"a twentieth of 5 Ore");
    arena.Run(19);
    Assert::AreEqual(start + 500, arena.World().OreHundredths(BLUE));

    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::CONTESTED_ASTEROID);
    arena.Run(20);
    Assert::AreEqual(start + 500 + 1300, arena.World().OreHundredths(BLUE), L"5 and 8 Ore a second");
    Assert::AreEqual(1300, arena.World().BuildSnapshot(BLUE).oreIncomeHundredthsPerSecond);
    Assert::AreEqual(0, arena.World().BuildSnapshot(RED).oreIncomeHundredthsPerSecond);
  }

  // Design §5: a job is paid for when it starts, and nothing is refunded when it is lost.
  TEST_METHOD(PaysWhenTheJobStartsAndNeverRefunds)
  {
    MatchArena arena;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {0.0f, 0.0f});
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    const std::int32_t cost = arena.StructureData(Outpost::StructureKind::Shipyard).cost.value_or(0);
    const auto results = arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::Shipyard, {200.0f, 0.0f}))});
    Assert::IsTrue(results[0] == Outpost::CommandResult::Applied);
    Assert::AreEqual(start - (std::int64_t{cost} * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE));

    // An enemy destroys the site before it is built: the Ore stays spent.
    const Outpost::EntityId site = arena.Owned(BLUE, Outpost::EntityKind::Structure).back()->id;
    for (int i = 0; i < 6; ++i)
      (void)arena.Ship(RED, SMALL, MASS_DRIVER, {200.0f + (static_cast<float>(i) * 20.0f), 100.0f});
    for (int tick = 0; tick < 60 * 20 && arena.World().FindEntity(site) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(site), L"the site survived");
    Assert::AreEqual(start - (std::int64_t{cost} * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE));
  }

  TEST_METHOD(RefusesAJobThePlayerCannotAfford)
  {
    MatchArena arena;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {0.0f, 0.0f});
    // 1,000 Ore buys three Shipyards and leaves 100.
    for (int i = 0; i < 3; ++i)
    {
      const Outpost::PlanePosition at{-600.0f + (static_cast<float>(i) * 300.0f), -400.0f};
      Assert::IsTrue(
        arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::Shipyard, at))})[0] == Outpost::CommandResult::Applied);
    }
    const std::int64_t left = arena.World().OreHundredths(BLUE);
    Assert::IsTrue(
      arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::Shipyard, {600.0f, -400.0f}))})[0] ==
      Outpost::CommandResult::NotEnoughOre);
    Assert::AreEqual(left, arena.World().OreHundredths(BLUE), L"a refused job costs nothing");
  }

  // Design §5, §6: an ore asteroid holds one rig, which snaps to it, and it is free again once the rig is destroyed.
  TEST_METHOD(OneRigPerAsteroid)
  {
    MatchArena arena;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {0.0f, 400.0f});
    const Outpost::EntityId enemy = arena.World().SpawnConstructor(RED, {100.0f, 400.0f});
    // Ordered beside the asteroid's edge, the rig snaps to its center.
    constexpr Outpost::PlanePosition beside{MatchArena::HOME_ASTEROID.xMeters, MatchArena::HOME_ASTEROID.zMeters - 70.0f};
    Assert::IsTrue(
      arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::MiningRig, beside))})[0] == Outpost::CommandResult::Applied);
    const Outpost::Entity& rig = *arena.Owned(BLUE, Outpost::EntityKind::Structure).back();
    Assert::IsTrue(rig.position == MatchArena::HOME_ASTEROID);
    const Outpost::EntityId rigId = rig.id;

    // Taken, by either player; and nowhere near an asteroid is no place for a rig.
    Assert::IsTrue(
      arena.Tick({Order(RED, Build(enemy, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID))})[0] ==
      Outpost::CommandResult::InvalidPlacement);
    Assert::IsTrue(
      arena.Tick({Order(RED, Build(enemy, Outpost::StructureKind::MiningRig, {-500.0f, -500.0f}))})[0] ==
      Outpost::CommandResult::InvalidPlacement);

    // Destroyed, the rig frees the asteroid.
    for (int i = 0; i < 6; ++i)
      (void)arena.Ship(RED, SMALL, MASS_DRIVER, {-100.0f + (static_cast<float>(i) * 40.0f), 700.0f});
    for (int tick = 0; tick < 60 * 20 && arena.World().FindEntity(rigId) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(rigId), L"the rig survived");
    Assert::IsTrue(
      arena.Tick({Order(RED, Build(enemy, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID))})[0] ==
      Outpost::CommandResult::Applied);
  }

  // A rig earns nothing until it is built.
  TEST_METHOD(ARigUnderConstructionEarnsNothing)
  {
    MatchArena arena;
    constexpr Outpost::PlanePosition nearAsteroid{MatchArena::HOME_ASTEROID.xMeters, MatchArena::HOME_ASTEROID.zMeters - 80.0f};
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, nearAsteroid);
    (void)arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID))});
    const std::int64_t paid = arena.World().OreHundredths(BLUE);
    const Outpost::Entity& rig = *arena.Owned(BLUE, Outpost::EntityKind::Structure).back();
    Assert::IsFalse(rig.IsBuilt());
    const Outpost::EntityId rigId = rig.id;
    arena.Run(20);
    Assert::AreEqual(paid, arena.World().OreHundredths(BLUE));

    const auto buildTicks = static_cast<std::uint32_t>(arena.StructureData(Outpost::StructureKind::MiningRig).buildConstructorSeconds.
                                                             value_or(0.0) * MatchArena::TICKS_PER_SECOND);
    arena.Run(buildTicks);
    Assert::IsTrue(arena.Get(rigId).IsBuilt());
    Assert::IsTrue(arena.World().OreHundredths(BLUE) > paid);
  }
};
} // namespace GameLogicTests