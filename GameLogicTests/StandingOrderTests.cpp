#include "pch.h"
#include "MatchArena.h"
#include "TerritoryMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr Outpost::PlayerId RED = TerritoryMatch::RED;
constexpr std::uint32_t TICKS_PER_SECOND = TerritoryMatch::TICKS_PER_SECOND;
// The repository map's sector east of Blue's home (ADR-036).
constexpr std::int32_t B1 = 2;

// Warships of Blue's in a row of _count, 30 m apart, at _position.
std::vector<Outpost::EntityId> Group(TerritoryMatch& _match, std::size_t _count, Outpost::PlanePosition _position)
{
  std::vector<Outpost::EntityId> ships;
  ships.reserve(_count);
  for (std::size_t i = 0; i < _count; ++i)
    ships.push_back(_match.Warship(BLUE, {.xMeters = _position.xMeters + (30.0f * static_cast<float>(i)), .zMeters = _position.zMeters}));
  return ships;
}

Outpost::PlanePosition CenterOf(TerritoryMatch& _match, const std::vector<Outpost::EntityId>& _ships)
{
  float x = 0.0f;
  float z = 0.0f;
  for (const Outpost::EntityId id : _ships)
  {
    x += _match.World().FindEntity(id)->position.xMeters;
    z += _match.World().FindEntity(id)->position.zMeters;
  }
  const auto count = static_cast<float>(_ships.size());
  return {.xMeters = x / count, .zMeters = z / count};
}

Outpost::CommandResult Give(TerritoryMatch& _match, Outpost::Order _order)
{
  return _match.World().Tick({{.player = BLUE, .order = std::move(_order)}}).front();
}
} // namespace

TEST_CLASS(StandingOrderTests)
{
public:
  // Phase 2 design §9: a group holding a sector goes to its node, answers an enemy ship it sees in the sector, and goes back
  // to the node once none is left; it keeps the order, which only its player sees.
  TEST_METHOD(AGroupHoldsItsSector)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    const Outpost::PlanePosition node = match.Placement(B1).node;
    const std::vector<Outpost::EntityId> ships = Group(match, 4, {.xMeters = -3300.0f, .zMeters = -3700.0f});
    Assert::IsTrue(Give(match, Outpost::HoldSectorCommand{.ships = ships, .position = {.xMeters = -1700.0f, .zMeters = -4300.0f}}) ==
                   Outpost::CommandResult::Applied);
    match.Run(40 * TICKS_PER_SECOND);
    Assert::IsTrue(Outpost::Distance(CenterOf(match, ships), node) < 150.0f, L"the group went to the sector's node");

    // An enemy in the far corner of the sector, seen through the Relay's sector and out of its reach of suppression.
    const Outpost::EntityId enemy = match.Warship(RED, {.xMeters = -1350.0f, .zMeters = -3300.0f});
    match.FightToTheEnd(RED, {enemy});
    for (std::uint32_t tick = 0; tick < 90 * TICKS_PER_SECOND && match.World().FindEntity(enemy) != nullptr; ++tick)
      match.Run(1);
    Assert::IsNull(match.World().FindEntity(enemy), L"the group never answered the enemy in its sector");
    Assert::IsTrue(Outpost::Distance(CenterOf(match, ships), node) > 300.0f, L"it went out to fight");

    match.Run(60 * TICKS_PER_SECOND);
    Assert::IsTrue(Outpost::Distance(CenterOf(match, ships), node) < 200.0f, L"the group went back to the node");
    for (const Outpost::EntityId id : ships)
      Assert::IsTrue(match.World().FindEntity(id)->standing == Outpost::StandingOrder::HoldSector);
    const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    Assert::IsTrue(std::ranges::find(blue.entities, ships.front(), &Outpost::EntityView::id)->standing ==
                   Outpost::StandingOrder::HoldSector);

    // Another order ends it.
    Assert::IsTrue(Give(match, Outpost::StopCommand{.ships = ships}) == Outpost::CommandResult::Applied);
    Assert::IsTrue(match.World().FindEntity(ships.front())->standing == Outpost::StandingOrder::None);
  }

  // Phase 2 design §9: a patrolling group attack-moves to its point, then back to where it was ordered from, and on.
  TEST_METHOD(AGroupPatrolsBetweenTwoPoints)
  {
    TerritoryMatch match;
    const Outpost::PlanePosition from{.xMeters = -600.0f, .zMeters = -600.0f};
    const Outpost::PlanePosition to{.xMeters = -600.0f, .zMeters = 100.0f};
    const std::vector<Outpost::EntityId> ships = Group(match, 3, {.xMeters = from.xMeters - 30.0f, .zMeters = from.zMeters});
    Assert::IsTrue(Give(match, Outpost::PatrolCommand{.ships = ships, .destination = to}) == Outpost::CommandResult::Applied);
    bool reachedTo = false;
    bool backAtFrom = false;
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && !backAtFrom; ++tick)
    {
      match.Run(1);
      const float toMeters = Outpost::Distance(CenterOf(match, ships), to);
      reachedTo = reachedTo || toMeters < 60.0f;
      backAtFrom = reachedTo && Outpost::Distance(CenterOf(match, ships), from) < 60.0f;
    }
    Assert::IsTrue(reachedTo, L"the group never reached its point");
    Assert::IsTrue(backAtFrom, L"the group never came back");
    Assert::IsTrue(match.World().FindEntity(ships.front())->standing == Outpost::StandingOrder::Patrol);
    // And it goes out again: a leg takes about ten seconds.
    match.Run(6 * TICKS_PER_SECOND);
    Assert::IsTrue(Outpost::Distance(CenterOf(match, ships), from) > 200.0f, L"the group did not set out again");
  }

  // A standing order is for warships: a hold needs a sector, and Constructors take no part.
  TEST_METHOD(RefusesWhatCannotStand)
  {
    TerritoryMatch match;
    const std::vector<Outpost::EntityId> constructors = match.Constructors(BLUE);
    Assert::IsTrue(Give(match, Outpost::HoldSectorCommand{.ships = constructors, .position = match.Placement(B1).node}) ==
                   Outpost::CommandResult::NoShips);
    Assert::IsTrue(Give(match, Outpost::PatrolCommand{.ships = constructors, .destination = {}}) == Outpost::CommandResult::NoShips);

    MatchArena arena;
    const Outpost::EntityId ship = arena.Ship(MatchArena::BLUE, Outpost::HullId{1}, Outpost::WeaponId{1}, {});
    Assert::IsTrue(arena.Tick({Order(MatchArena::BLUE, Outpost::HoldSectorCommand{.ships = {ship}, .position = {}})})[0] ==
                     Outpost::CommandResult::NoSector,
                   L"a map without sectors has none to hold");
    Assert::IsTrue(arena.Tick({Order(MatchArena::BLUE, Outpost::PatrolCommand{.ships = {ship}, .destination = {300.0f, 0.0f}})})[0] ==
                   Outpost::CommandResult::Applied);
    const Outpost::Snapshot red = arena.World().BuildSnapshot(MatchArena::RED);
    const auto seen = std::ranges::find(red.entities, ship, &Outpost::EntityView::id);
    Assert::IsTrue(seen != red.entities.end() && seen->standing == Outpost::StandingOrder::None,
                   L"the other player does not see the order");
  }
};
} // namespace GameLogicTests
