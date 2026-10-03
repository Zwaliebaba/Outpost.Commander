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
constexpr Outpost::ResearchTopicId IMPROVED_EXTRACTION{1};
constexpr Outpost::ResearchTopicId HULL_PLATING{2};
constexpr Outpost::ResearchTopicId RELAY_ARCHIVES{9};
constexpr Outpost::ResearchTopicId DEEP_CORE_SURVEY{12};

Outpost::BuildStructureCommand Build(Outpost::EntityId _constructor, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
{
  return {.constructors = {_constructor}, .structure = _kind, .position = _position};
}
// The ore asteroid placed at _position.
const Outpost::Entity& AsteroidAt(MatchArena& _arena, Outpost::PlanePosition _position)
{
  const auto found = std::ranges::find_if(_arena.World().Entities(), [_position](const Outpost::Entity& _entity)
                                          { return _entity.kind == Outpost::EntityKind::Asteroid && _entity.position == _position; });
  Assert::IsTrue(found != _arena.World().Entities().end());
  return *found;
}

std::int64_t ReserveAt(MatchArena& _arena, Outpost::PlanePosition _position)
{
  return AsteroidAt(_arena, _position).oreReserveHundredths.value_or(-1);
}

// Researches the topics one after another at _lab, each from its order to the tick it is done.
void ResearchAll(MatchArena& _arena, Outpost::PlayerId _player, Outpost::EntityId _lab,
                 std::initializer_list<Outpost::ResearchTopicId> _topics)
{
  for (const Outpost::ResearchTopicId topic : _topics)
  {
    Assert::IsTrue(_arena.Tick({Order(_player, Outpost::StartResearchCommand{.lab = _lab, .topic = topic})})[0] ==
                   Outpost::CommandResult::Applied);
    for (int tick = 0; tick < 600 * static_cast<int>(MatchArena::TICKS_PER_SECOND) &&
                       std::ranges::find(_arena.World().Researched(_player), topic) == _arena.World().Researched(_player).end();
         ++tick)
      _arena.Run(1);
    Assert::IsTrue(std::ranges::find(_arena.World().Researched(_player), topic) != _arena.World().Researched(_player).end());
  }
}

// The Ore left that _player's snapshot shows for the asteroid at _position, or -1 for none.
std::int64_t ReserveSeenBy(MatchArena& _arena, Outpost::PlayerId _player, Outpost::PlanePosition _position)
{
  const Outpost::Snapshot snapshot = _arena.World().BuildSnapshot(_player);
  const auto view = std::ranges::find(snapshot.entities, AsteroidAt(_arena, _position).id, &Outpost::EntityView::id);
  Assert::IsTrue(view != snapshot.entities.end());
  return view->oreReserveHundredths.value_or(-1);
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

  // Phase 1 design §8: a rig draws its asteroid's reserve down by what it earns, and once the reserve is gone earns a
  // trickle, a fifth of its rate, for the rest of the match. Its asteroid's reserve is in the snapshot, the rig's too.
  TEST_METHOD(AnAsteroidRunsDryToATrickle)
  {
    // 50 Ore: ten seconds of a home rig.
    MatchArena arena(50);
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    const Outpost::EntityId rig = arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    Assert::AreEqual(std::int64_t{5000}, ReserveAt(arena, MatchArena::HOME_ASTEROID));
    arena.Run(100);
    Assert::AreEqual(std::int64_t{2500}, ReserveAt(arena, MatchArena::HOME_ASTEROID));
    arena.Run(100);
    Assert::AreEqual(std::int64_t{0}, ReserveAt(arena, MatchArena::HOME_ASTEROID));
    Assert::AreEqual(start + 5000, arena.World().OreHundredths(BLUE), L"all 50 Ore");

    const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
    Assert::AreEqual(100, snapshot.oreIncomeHundredthsPerSecond, L"a fifth of 5 Ore a second");
    const auto rigView = std::ranges::find(snapshot.entities, rig, &Outpost::EntityView::id);
    Assert::AreEqual(std::int64_t{0}, rigView->oreReserveHundredths.value_or(-1));
    arena.Run(20);
    Assert::AreEqual(start + 5100, arena.World().OreHundredths(BLUE));
    Assert::AreEqual(std::int64_t{0}, ReserveAt(arena, MatchArena::HOME_ASTEROID), L"a trickle draws nothing");

    // An asteroid the map gave no reserve never runs out.
    MatchArena endless;
    (void)endless.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    endless.Run(400);
    Assert::IsFalse(AsteroidAt(endless, MatchArena::HOME_ASTEROID).oreReserveHundredths.has_value());
    Assert::AreEqual(500, endless.World().BuildSnapshot(BLUE).oreIncomeHundredthsPerSecond);
  }

  // Phase 1 design §8: Improved Extraction draws the reserve as fast as it earns, so the reserve lasts a fifth less, and
  // Deep Core Survey makes every reserve last 30% longer, what is left included, for the player who researched it only.
  TEST_METHOD(ResearchChangesHowFastAReserveDrains)
  {
    MatchArena arena(5000);
    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    (void)arena.Structure(RED, Outpost::StructureKind::MiningRig, MatchArena::CONTESTED_ASTEROID);
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {-600.0f, -600.0f});
    // What each asteroid loses in a second.
    const auto drain = [&arena](Outpost::PlanePosition _asteroid)
    {
      const std::int64_t before = ReserveAt(arena, _asteroid);
      arena.Run(MatchArena::TICKS_PER_SECOND);
      return before - ReserveAt(arena, _asteroid);
    };

    Assert::AreEqual(std::int64_t{500}, drain(MatchArena::HOME_ASTEROID));
    ResearchAll(arena, BLUE, lab, {IMPROVED_EXTRACTION});
    Assert::AreEqual(std::int64_t{625}, drain(MatchArena::HOME_ASTEROID), L"6.25 Ore a second");
    Assert::AreEqual(std::int64_t{800}, drain(MatchArena::CONTESTED_ASTEROID), L"Red's rig, without research");

    ResearchAll(arena, BLUE, lab, {HULL_PLATING, RELAY_ARCHIVES, DEEP_CORE_SURVEY});
    const std::int64_t ore = arena.World().OreHundredths(BLUE);
    Assert::AreEqual(std::int64_t{481}, drain(MatchArena::HOME_ASTEROID), L"6.25 Ore a second over 1.3");
    Assert::AreEqual(ore + 625, arena.World().OreHundredths(BLUE), L"the income is the same");
    Assert::AreEqual(std::int64_t{800}, drain(MatchArena::CONTESTED_ASTEROID), L"only for the player who researched it");
  }

  // Phase 1 design §8, ADR-024: under fog of war a player sees the Ore left in an asteroid it can see, and remembers the
  // figure from when it last saw it; of one it has never seen it knows nothing.
  TEST_METHOD(RemembersTheReserveItLastSaw)
  {
    MatchArena arena(5000);
    arena.World().UseFog();
    (void)arena.Structure(RED, Outpost::StructureKind::MiningRig, MatchArena::CONTESTED_ASTEROID);
    const Outpost::EntityId scout = arena.Ship(BLUE, SMALL, MASS_DRIVER, {MatchArena::CONTESTED_ASTEROID.xMeters - 120.0f, 0.0f});
    arena.Run(20);
    const std::int64_t seen = ReserveSeenBy(arena, BLUE, MatchArena::CONTESTED_ASTEROID);
    Assert::AreEqual(ReserveAt(arena, MatchArena::CONTESTED_ASTEROID), seen, L"in sight");
    Assert::AreEqual(std::int64_t{-1}, ReserveSeenBy(arena, BLUE, MatchArena::HOME_ASTEROID), L"never seen");

    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {scout}, .destination = {-1200.0f, 0.0f}})});
    arena.Run(30 * MatchArena::TICKS_PER_SECOND);
    const std::int64_t remembered = ReserveSeenBy(arena, BLUE, MatchArena::CONTESTED_ASTEROID);
    Assert::IsTrue(remembered < seen && remembered > ReserveAt(arena, MatchArena::CONTESTED_ASTEROID), L"as it was when the scout left");
    Assert::AreEqual(ReserveAt(arena, MatchArena::CONTESTED_ASTEROID), ReserveSeenBy(arena, RED, MatchArena::CONTESTED_ASTEROID),
                     L"its own rig's asteroid, in sight");
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
      Assert::IsTrue(arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::Shipyard, at))})[0] ==
                     Outpost::CommandResult::Applied);
    }
    const std::int64_t left = arena.World().OreHundredths(BLUE);
    Assert::IsTrue(arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::Shipyard, {600.0f, -400.0f}))})[0] ==
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
    const Outpost::PlanePosition beside{MatchArena::HOME_ASTEROID.xMeters, MatchArena::HOME_ASTEROID.zMeters - 70.0f};
    Assert::IsTrue(arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::MiningRig, beside))})[0] ==
                   Outpost::CommandResult::Applied);
    const Outpost::Entity& rig = *arena.Owned(BLUE, Outpost::EntityKind::Structure).back();
    Assert::IsTrue(rig.position == MatchArena::HOME_ASTEROID);
    const Outpost::EntityId rigId = rig.id;

    // Taken, by either player; and nowhere near an asteroid is no place for a rig.
    Assert::IsTrue(arena.Tick({Order(RED, Build(enemy, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID))})[0] ==
                   Outpost::CommandResult::InvalidPlacement);
    Assert::IsTrue(arena.Tick({Order(RED, Build(enemy, Outpost::StructureKind::MiningRig, {-500.0f, -500.0f}))})[0] ==
                   Outpost::CommandResult::InvalidPlacement);

    // Destroyed, the rig frees the asteroid.
    for (int i = 0; i < 6; ++i)
      (void)arena.Ship(RED, SMALL, MASS_DRIVER, {-100.0f + (static_cast<float>(i) * 40.0f), 700.0f});
    for (int tick = 0; tick < 60 * 20 && arena.World().FindEntity(rigId) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(rigId), L"the rig survived");
    Assert::IsTrue(arena.Tick({Order(RED, Build(enemy, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID))})[0] ==
                   Outpost::CommandResult::Applied);
  }

  // A rig earns nothing until it is built.
  TEST_METHOD(ARigUnderConstructionEarnsNothing)
  {
    MatchArena arena;
    const Outpost::PlanePosition nearAsteroid{MatchArena::HOME_ASTEROID.xMeters, MatchArena::HOME_ASTEROID.zMeters - 80.0f};
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, nearAsteroid);
    (void)arena.Tick({Order(BLUE, Build(constructor, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID))});
    const std::int64_t paid = arena.World().OreHundredths(BLUE);
    const Outpost::Entity& rig = *arena.Owned(BLUE, Outpost::EntityKind::Structure).back();
    Assert::IsFalse(rig.IsBuilt());
    const Outpost::EntityId rigId = rig.id;
    arena.Run(20);
    Assert::AreEqual(paid, arena.World().OreHundredths(BLUE));

    const auto buildTicks = static_cast<std::uint32_t>(
      arena.StructureData(Outpost::StructureKind::MiningRig).buildConstructorSeconds.value_or(0.0) * MatchArena::TICKS_PER_SECOND);
    arena.Run(buildTicks);
    Assert::IsTrue(arena.Get(rigId).IsBuilt());
    Assert::IsTrue(arena.World().OreHundredths(BLUE) > paid);
  }
};
} // namespace GameLogicTests