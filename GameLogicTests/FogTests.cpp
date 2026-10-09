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
constexpr Outpost::WeaponId LANCE{2};

const Outpost::EntityView* FindView(const Outpost::Snapshot& _snapshot, Outpost::EntityId _id)
{
  const auto found = std::ranges::find(_snapshot.entities, _id, &Outpost::EntityView::id);
  return found != _snapshot.entities.end() ? &*found : nullptr;
}

// Two clumps that attack-move on each other: six swarm ships against four lines.
void StageBattle(MatchArena& _arena)
{
  std::vector<Outpost::EntityId> blue;
  std::vector<Outpost::EntityId> red;
  blue.reserve(6);
  red.reserve(4);
  for (int i = 0; i < 6; ++i)
    blue.push_back(_arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -1400.0f + (25.0f * static_cast<float>(i)), .zMeters = -1000.0f}));
  for (int i = 0; i < 4; ++i)
    red.push_back(_arena.Ship(RED, MEDIUM, LANCE, {.xMeters = -1400.0f + (45.0f * static_cast<float>(i)), .zMeters = -500.0f}));
  (void)_arena.Tick({Order(BLUE, Outpost::AttackMoveCommand{.ships = blue, .destination = {.xMeters = -1350.0f, .zMeters = -400.0f}}),
                     Order(RED, Outpost::AttackMoveCommand{.ships = red, .destination = {.xMeters = -1350.0f, .zMeters = -1100.0f}})});
}
} // namespace

// ADR-024: fog of war, decided by the owner on 2026-10-02.
TEST_CLASS(FogTests)
{
public:
  // A player sees an enemy within its own entities' sight, measured to the enemy's edge, and not beyond; the map is always
  // seen. Without fog it sees everything.
  TEST_METHOD(SeesWhatItsShipsAndStructuresSee)
  {
    MatchArena arena;
    // A swarm ship sees 120 m and the 50 m margin; a Constructor, unarmed, 200 m.
    const Outpost::EntityId scout = arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -1000.0f, .zMeters = -1000.0f});
    const Outpost::EntityId nearby = arena.World().SpawnConstructor(RED, {.xMeters = -1000.0f, .zMeters = -825.0f});
    const Outpost::EntityId distant = arena.World().SpawnConstructor(RED, {.xMeters = -1000.0f, .zMeters = -600.0f});
    Assert::IsTrue(arena.World().Sees(BLUE, arena.Get(distant)), L"no fog yet");
    Assert::AreEqual(170.0f, arena.World().SightMetersOf(arena.Get(scout)), 1e-4f);
    Assert::AreEqual(200.0f, arena.World().SightMetersOf(arena.Get(nearby)), 1e-4f);

    Assert::IsFalse(arena.World().BuildSnapshot(BLUE).fogOfWar);

    arena.World().UseFog();
    const Outpost::Snapshot blue = arena.World().BuildSnapshot(BLUE);
    Assert::IsTrue(blue.fogOfWar);
    Assert::AreEqual(170.0f, FindView(blue, scout)->sightMeters, 1e-4f, L"its own entity's sight, which the client draws by");
    Assert::IsNotNull(FindView(blue, nearby), L"175 m off, within 170 m and its 10 m footprint");
    Assert::AreEqual(0.0f, FindView(blue, nearby)->sightMeters, L"an enemy's is not told");
    Assert::IsNull(FindView(blue, distant));
    Assert::IsFalse(arena.World().Sees(BLUE, arena.Get(distant)));
    Assert::IsTrue(
      std::ranges::any_of(blue.entities, [](const Outpost::EntityView& _view) { return _view.kind == Outpost::EntityKind::Asteroid; }),
      L"the map is always seen");
    Assert::IsNotNull(FindView(arena.World().BuildSnapshot(RED), scout), L"the Constructor sees 200 m");
  }

  // Every armed entity sees beyond its weapon's range, so fog never changes what anything fires at: a battle plays the
  // same with fog and without, tick for tick.
  TEST_METHOD(NeverChangesABattle)
  {
    MatchArena clear;
    MatchArena fogged;
    fogged.World().UseFog();
    StageBattle(clear);
    StageBattle(fogged);
    for (int tick = 0; tick < 60 * 20; ++tick)
    {
      (void)clear.Tick();
      (void)fogged.Tick();
    }
    Assert::IsTrue(std::ranges::equal(clear.World().Entities(), fogged.World().Entities()));
    Assert::IsTrue(clear.World().Entities().size() < 10 + 2, L"the battle was fought");
  }

  // A ship that hits a player is seen by that player for the tuning data's three seconds after each hit, even from beyond
  // the player's sight.
  TEST_METHOD(RevealsAShooterToTheSideItHits)
  {
    MatchArena arena;
    // The line's Lance reaches 220 m; the swarm ship sees 170 m and the line's 14 m footprint.
    const Outpost::EntityId swarm = arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -1000.0f, .zMeters = -1000.0f});
    const Outpost::EntityId line = arena.Ship(RED, MEDIUM, LANCE, {.xMeters = -1000.0f, .zMeters = -790.0f});
    arena.World().UseFog();
    Assert::IsFalse(arena.World().Sees(BLUE, arena.Get(line)));

    int ticks = 0;
    while (arena.Get(swarm).hitPointsHundredths == arena.Get(swarm).maxHitPointsHundredths && ticks < 100)
    {
      (void)arena.Tick();
      ++ticks;
    }
    Assert::IsTrue(ticks < 100, L"the line fired");
    Assert::IsTrue(arena.World().Sees(BLUE, arena.Get(line)), L"seen on the tick of the hit");
    const Outpost::Snapshot blue = arena.World().BuildSnapshot(BLUE);
    Assert::IsNotNull(FindView(blue, line));
    Assert::IsTrue(std::ranges::any_of(blue.shots, [line](const Outpost::ShotView& _shot) { return _shot.shooter == line; }));

    // Out of range, it fades three seconds after its last hit.
    (void)arena.Tick({Order(RED, Outpost::MoveCommand{.ships = {line}, .destination = {.xMeters = -1000.0f, .zMeters = 400.0f}})});
    arena.Run(10 * 20);
    Assert::IsFalse(arena.World().Sees(BLUE, arena.Get(line)));
  }

  // An enemy structure once seen stays in the player's snapshot where it was seen, marked remembered, until the player
  // sees the place again; it may be attacked from memory, and one never seen may not.
  TEST_METHOD(RemembersEnemyStructures)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(RED, Outpost::StructureKind::Shipyard, {.xMeters = -1500.0f, .zMeters = 1500.0f});
    const Outpost::EntityId hidden = arena.Structure(RED, Outpost::StructureKind::ResearchLab, {.xMeters = 1500.0f, .zMeters = 1500.0f});
    // 150 m off: inside its sight with the yard's 40 m footprint, beyond its 120 m reach.
    const Outpost::EntityId scout = arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -1500.0f, .zMeters = 1350.0f});
    arena.World().UseFog();
    Assert::IsTrue(arena.World().Sees(BLUE, arena.Get(yard)));

    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {scout}, .destination = {.xMeters = -1500.0f, .zMeters = 200.0f}})});
    arena.Run(20 * 20);
    Assert::IsFalse(arena.World().Sees(BLUE, arena.Get(yard)));
    const Outpost::Snapshot memory = arena.World().BuildSnapshot(BLUE);
    Assert::IsTrue(std::ranges::is_sorted(memory.entities, {}, &Outpost::EntityView::id), L"in identifier order, as clients pair them");
    const Outpost::EntityView* remembered = FindView(memory, yard);
    Assert::IsNotNull(remembered);
    Assert::IsTrue(remembered->remembered && remembered->structure == Outpost::StructureKind::Shipyard);
    Assert::IsTrue(remembered->queue.empty(), L"an enemy's queue is never shown");

    std::vector<Outpost::EntityId> lances;
    lances.reserve(6);
    for (int i = 0; i < 6; ++i)
      lances.push_back(arena.Ship(BLUE, SMALL, LANCE, {.xMeters = -1500.0f + (20.0f * static_cast<float>(i)), .zMeters = 400.0f}));
    const std::vector<Outpost::CommandResult> results = arena.Tick({Order(BLUE, Outpost::AttackCommand{.ships = lances, .target = hidden}),
                                                                    Order(BLUE, Outpost::AttackCommand{.ships = lances, .target = yard})});
    Assert::IsTrue(results[0] == Outpost::CommandResult::NotVisible, L"never seen");
    Assert::IsTrue(results[1] == Outpost::CommandResult::Applied, L"remembered");

    for (int tick = 0; tick < 180 * 20 && arena.World().FindEntity(yard) != nullptr; ++tick)
      (void)arena.Tick();
    Assert::IsNull(arena.World().FindEntity(yard), L"the lances destroyed it");
    (void)arena.Tick();
    Assert::IsNull(FindView(arena.World().BuildSnapshot(BLUE), yard), L"seen gone, so forgotten");
  }

  // A remembered structure carries the tick of the last snapshot its player saw it in, which the client tells its age by
  // (interface plan 2, task UI1.2), and that tick stands while the structure stays out of sight. One seen now carries none.
  TEST_METHOD(RemembersWhenAStructureWasLastSeen)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(RED, Outpost::StructureKind::Shipyard, {.xMeters = -1500.0f, .zMeters = 1500.0f});
    const Outpost::EntityId scout = arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -1500.0f, .zMeters = 1350.0f});
    arena.World().UseFog();
    const Outpost::Snapshot first = arena.World().BuildSnapshot(BLUE);
    const Outpost::EntityView* seen = FindView(first, yard);
    Assert::IsTrue(seen != nullptr && !seen->remembered);
    Assert::AreEqual(std::uint64_t{0}, seen->lastSeenTick, L"seen now, so no age");

    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {scout}, .destination = {.xMeters = -1500.0f, .zMeters = 200.0f}})});
    std::uint64_t lastSeen = 0;
    for (int tick = 0; tick < 20 * 20 && arena.World().Sees(BLUE, arena.Get(yard)); ++tick)
    {
      lastSeen = arena.World().BuildSnapshot(BLUE).tick;
      (void)arena.Tick();
    }
    Assert::IsFalse(arena.World().Sees(BLUE, arena.Get(yard)), L"the scout left it behind");
    Assert::IsTrue(lastSeen > 0);
    const Outpost::Snapshot gone = arena.World().BuildSnapshot(BLUE);
    const Outpost::EntityView* remembered = FindView(gone, yard);
    Assert::IsTrue(remembered != nullptr && remembered->remembered);
    Assert::AreEqual(lastSeen, remembered->lastSeenTick, L"the last snapshot that showed it");

    arena.Run(10 * 20);
    const Outpost::Snapshot later = arena.World().BuildSnapshot(BLUE);
    Assert::AreEqual(lastSeen, FindView(later, yard)->lastSeenTick, L"it stands while the yard is out of sight");
    Assert::IsTrue(later.tick >= lastSeen + (std::uint64_t{10} * 20));
  }

  // An attack on a ship ends once no entity of the attacker's side sees it, since the player would not know where it went.
  TEST_METHOD(EndsAnAttackOnAShipOutOfSight)
  {
    MatchArena arena;
    const Outpost::EntityId target = arena.World().SpawnConstructor(RED, {.xMeters = 0.0f, .zMeters = -1500.0f});
    const Outpost::EntityId spotter = arena.World().SpawnConstructor(BLUE, {.xMeters = 150.0f, .zMeters = -1500.0f});
    const Outpost::EntityId lance = arena.Ship(BLUE, SMALL, LANCE, {.xMeters = -1500.0f, .zMeters = -1500.0f});
    const Outpost::EntityId unseen = arena.World().SpawnConstructor(RED, {.xMeters = 1500.0f, .zMeters = 1500.0f});
    arena.World().UseFog();

    const std::vector<Outpost::CommandResult> results =
      arena.Tick({Order(BLUE, Outpost::AttackCommand{.ships = {lance}, .target = unseen}),
                  Order(BLUE, Outpost::AttackCommand{.ships = {lance}, .target = target}),
                  Order(BLUE, Outpost::MoveCommand{.ships = {spotter}, .destination = {.xMeters = 1500.0f, .zMeters = 0.0f}})});
    Assert::IsTrue(results[0] == Outpost::CommandResult::NotVisible);
    Assert::IsTrue(results[1] == Outpost::CommandResult::Applied);
    Assert::IsTrue(arena.Get(lance).order == Outpost::ShipOrder::Attack);

    arena.Run(5 * 20);
    Assert::IsFalse(arena.World().Sees(BLUE, arena.Get(target)));
    Assert::IsTrue(arena.Get(lance).order == Outpost::ShipOrder::None, L"the target went out of sight");
  }
};
} // namespace GameLogicTests
