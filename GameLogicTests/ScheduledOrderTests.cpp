#include "pch.h"
#include "TerritoryMatch.h"

#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr Outpost::PlayerId RED = TerritoryMatch::RED;
constexpr std::uint32_t TICKS_PER_SECOND = TerritoryMatch::TICKS_PER_SECOND;
// Sectors of the repository's map: Blue's home, the flank east of it and the flank north of it, and the sector north of
// that flank.
constexpr std::int32_t A1 = 1;
constexpr std::int32_t B1 = 2;
constexpr std::int32_t A2 = 6;
constexpr std::int32_t B2 = 7;
// In Blue's home, clear of its base.
constexpr Outpost::PlanePosition MUSTER{.xMeters = -3400.0f, .zMeters = -3600.0f};
// In B1, farther from its node than a warship suppresses a Relay from, so that a sector Blue holds there stays seen whole.
constexpr Outpost::PlanePosition B1_EDGE{.xMeters = -1300.0f, .zMeters = -3300.0f};

Outpost::CommandResult Give(TerritoryMatch& _match, Outpost::PlayerId _player, Outpost::Order _order)
{
  return _match.World().Tick({{.player = _player, .order = std::move(_order)}}).front();
}

Outpost::ScheduledTrigger At(std::uint64_t _tick)
{
  return {.kind = Outpost::ScheduledTriggerKind::TimeOfDay, .tick = _tick};
}

Outpost::ScheduledTrigger On(Outpost::ScheduledTriggerKind _kind, std::int32_t _sector)
{
  return {.kind = _kind, .sector = _sector};
}

Outpost::ScheduledAction Doing(Outpost::ScheduledActionKind _kind, Outpost::PlanePosition _position, Outpost::EntityId _target = {})
{
  return {.kind = _kind, .position = _position, .target = _target};
}

// _player's events of the last tick of _kind.
std::vector<Outpost::EventView> Fired(TerritoryMatch& _match, Outpost::PlayerId _player = BLUE)
{
  std::vector<Outpost::EventView> fired;
  for (const Outpost::EventView& event : _match.World().BuildSnapshot(_player).events)
  {
    if (event.kind == Outpost::EventKind::OrderFired)
      fired.push_back(event);
  }
  return fired;
}

// Runs a tick at a time, at most _seconds, until one of Blue's orders fires; the orders that fired in that tick.
std::vector<Outpost::EventView> RunUntilFired(TerritoryMatch& _match, std::uint32_t _seconds)
{
  for (std::uint32_t tick = 0; tick < _seconds * TICKS_PER_SECOND; ++tick)
  {
    _match.Run(1);
    if (std::vector<Outpost::EventView> fired = Fired(_match); !fired.empty())
      return fired;
  }
  return {};
}

const Outpost::Entity& Get(TerritoryMatch& _match, Outpost::EntityId _id)
{
  const Outpost::Entity* entity = _match.World().FindEntity(_id);
  Assert::IsNotNull(entity);
  return *entity;
}

std::vector<Outpost::EntityId> Warships(TerritoryMatch& _match, Outpost::PlayerId _owner, Outpost::PlanePosition _at, int _count)
{
  std::vector<Outpost::EntityId> ships;
  ships.reserve(static_cast<std::size_t>(_count));
  for (int i = 0; i < _count; ++i)
    ships.push_back(_match.Warship(_owner, {.xMeters = _at.xMeters + (30.0f * static_cast<float>(i)), .zMeters = _at.zMeters}));
  return ships;
}
} // namespace

// Phase 5 design §7, gates H4 and H5 (ADR-080): a scheduled order fires once, on the tick its trigger's event is raised, and
// does its action, or holds its ships when its condition says the enemy is too strong. W5 in tests.
TEST_CLASS(ScheduledOrderTests)
{
public:
  // A time of day, which the host has made a tick, fires on that tick and not before; the ships wait on it until then, and
  // only their owner sees it.
  TEST_METHOD(ATimeOfDayFiresOnItsTick)
  {
    TerritoryMatch match;
    const std::vector<Outpost::EntityId> ships = Warships(match, BLUE, MUSTER, 2);
    match.Run(1);
    const std::uint64_t due = match.World().CurrentTick() + 40;
    const Outpost::PlanePosition there = match.Placement(B1).node;
    Assert::IsTrue(
      Give(match, BLUE,
           Outpost::ScheduleOrderCommand{.ships = ships, .trigger = At(due), .action = Doing(Outpost::ScheduledActionKind::Move, there)}) ==
      Outpost::CommandResult::Applied);
    const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    Assert::AreEqual(std::size_t{1}, blue.scheduled.size());
    Assert::IsTrue(blue.scheduled[0].ships == ships && blue.scheduled[0].trigger.tick == due);
    const auto view = std::ranges::find(blue.entities, ships[0], &Outpost::EntityView::id);
    Assert::AreEqual(blue.scheduled[0].id, view->scheduledOrder, L"the ship names the order it waits on");
    Assert::IsTrue(match.World().BuildSnapshot(RED).scheduled.empty(), L"only Blue sees Blue's");
    Assert::IsTrue(Get(match, ships[0]).order == Outpost::ShipOrder::None, L"nothing to do until it fires");

    while (match.World().CurrentTick() < due)
    {
      match.Run(1);
      Assert::IsTrue(Fired(match).empty(), L"not before its tick");
    }
    match.Run(1);
    const std::vector<Outpost::EventView> fired = Fired(match);
    Assert::AreEqual(std::size_t{1}, fired.size(), L"on its tick");
    Assert::IsTrue(fired[0].outcome == Outpost::OrderOutcome::AsGiven && fired[0].action == Outpost::ScheduledActionKind::Move);
    Assert::AreEqual(B1, fired[0].sector);
    Assert::IsTrue(Get(match, ships[0]).order == Outpost::ShipOrder::Move && Get(match, ships[1]).order == Outpost::ShipOrder::Move);
    Assert::IsTrue(match.World().BuildSnapshot(BLUE).scheduled.empty(), L"once, and gone");
    match.Run(TICKS_PER_SECOND);
    Assert::IsTrue(Fired(match).empty());
  }

  // Enemy ships in a sector Blue holds fire it; in another sector, nothing.
  TEST_METHOD(EnemyShipsFireItInItsSectorOnly)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    (void)match.Relay(BLUE, A2);
    const std::vector<Outpost::EntityId> ships = Warships(match, BLUE, MUSTER, 2);
    Assert::IsTrue(Give(match, BLUE,
                        Outpost::ScheduleOrderCommand{.ships = ships,
                                                      .trigger = On(Outpost::ScheduledTriggerKind::EnemyInSector, B1),
                                                      .action = Doing(Outpost::ScheduledActionKind::AttackMove,
                                                                      match.Placement(B1).node)}) == Outpost::CommandResult::Applied);
    const Outpost::PlanePosition a2 = match.Placement(A2).node;
    (void)match.Warship(RED, {.xMeters = a2.xMeters + 150.0f, .zMeters = a2.zMeters});
    match.Run(2 * TICKS_PER_SECOND);
    Assert::IsTrue(Fired(match).empty() && !match.World().BuildSnapshot(BLUE).scheduled.empty(), L"another sector");

    const Outpost::PlanePosition b1 = match.Placement(B1).node;
    (void)match.Warship(RED, {.xMeters = b1.xMeters + 150.0f, .zMeters = b1.zMeters});
    match.Run(1);
    const std::vector<Outpost::EventView> fired = Fired(match);
    Assert::AreEqual(std::size_t{1}, fired.size(), L"in the tick they are seen entering");
    Assert::IsTrue(Get(match, ships[0]).order == Outpost::ShipOrder::AttackMove);
  }

  // A Relay hit, and a rig lost, in the sector fire theirs.
  TEST_METHOD(ARelayHitAndARigLostFireTheirs)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    (void)match.Structure(BLUE, Outpost::StructureKind::MiningRig, match.AsteroidIn(B1));
    const std::vector<Outpost::EntityId> guards = Warships(match, BLUE, MUSTER, 1);
    const std::vector<Outpost::EntityId> avengers =
      Warships(match, BLUE, {.xMeters = MUSTER.xMeters, .zMeters = MUSTER.zMeters + 100.0f}, 1);
    (void)match.World().Tick(
      {{.player = BLUE,
        .order = Outpost::ScheduleOrderCommand{.ships = guards,
                                               .trigger = On(Outpost::ScheduledTriggerKind::RelayThreatened, B1),
                                               .action = Doing(Outpost::ScheduledActionKind::HoldSector, match.Placement(B1).node)}},
       {.player = BLUE,
        .order = Outpost::ScheduleOrderCommand{.ships = avengers,
                                               .trigger = On(Outpost::ScheduledTriggerKind::RigLost, B1),
                                               .action = Doing(Outpost::ScheduledActionKind::Patrol, match.Placement(B1).node)}}});
    Assert::AreEqual(std::size_t{2}, match.World().BuildSnapshot(BLUE).scheduled.size());

    // One raid on the rig, and one on the Relay.
    const Outpost::PlanePosition rig = match.AsteroidIn(B1);
    std::vector<Outpost::EntityId> raiders = Warships(match, RED, {.xMeters = rig.xMeters + 80.0f, .zMeters = rig.zMeters}, 4);
    const Outpost::PlanePosition node = match.Placement(B1).node;
    std::ranges::copy(Warships(match, RED, {.xMeters = node.xMeters + 150.0f, .zMeters = node.zMeters}, 2), std::back_inserter(raiders));
    bool held = false;
    bool patrolled = false;
    // What fired in the tick just run: the raid on the Relay suppresses it in the tick its raiders are set to fight.
    const auto look = [&]()
    {
      for (const Outpost::EventView& fired : Fired(match))
      {
        held = held || fired.action == Outpost::ScheduledActionKind::HoldSector;
        patrolled = patrolled || fired.action == Outpost::ScheduledActionKind::Patrol;
      }
    };
    match.FightToTheEnd(RED, raiders);
    look();
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && !(held && patrolled); ++tick)
    {
      match.Run(1);
      look();
    }
    Assert::IsTrue(held && Get(match, guards[0]).standing == Outpost::StandingOrder::HoldSector, L"the Relay suppressed or hit");
    Assert::IsTrue(patrolled && Get(match, avengers[0]).standing == Outpost::StandingOrder::Patrol, L"the rig lost");
  }

  // Each action gives its ships the order of its name, and a rig's Constructors build it.
  TEST_METHOD(EachActionGivesItsOrder)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    const Outpost::EntityId enemy = match.Warship(RED, {.xMeters = MUSTER.xMeters + 400.0f, .zMeters = MUSTER.zMeters});
    const std::vector<Outpost::EntityId> ships = Warships(match, BLUE, MUSTER, 5);
    const Outpost::EntityId constructor = match.Constructors(BLUE).front();
    match.Run(1);
    const std::uint64_t due = match.World().CurrentTick() + 2;
    const Outpost::PlanePosition node = match.Placement(B1).node;
    const std::array<Outpost::ScheduledAction, 5> actions{
      Doing(Outpost::ScheduledActionKind::Move, node), Doing(Outpost::ScheduledActionKind::AttackMove, node),
      Doing(Outpost::ScheduledActionKind::Attack, {}, enemy), Doing(Outpost::ScheduledActionKind::HoldSector, node),
      Doing(Outpost::ScheduledActionKind::Patrol, node)};
    std::vector<Outpost::Command> orders;
    orders.reserve(actions.size() + 1);
    for (std::size_t i = 0; i < actions.size(); ++i)
      orders.push_back(
        {.player = BLUE, .order = Outpost::ScheduleOrderCommand{.ships = {ships[i]}, .trigger = At(due), .action = actions[i]}});
    orders.push_back(
      {.player = BLUE,
       .order = Outpost::ScheduleOrderCommand{
         .ships = {constructor}, .trigger = At(due), .action = Doing(Outpost::ScheduledActionKind::BuildRig, match.AsteroidIn(B1))}});
    for (const Outpost::CommandResult result : match.World().Tick(orders))
      Assert::IsTrue(result == Outpost::CommandResult::Applied);
    const std::vector<Outpost::EventView> fired = RunUntilFired(match, 1);
    Assert::AreEqual(std::size_t{6}, fired.size(), L"all on the same tick");
    for (const Outpost::EventView& event : fired)
      Assert::IsTrue(event.outcome == Outpost::OrderOutcome::AsGiven);
    Assert::IsTrue(Get(match, ships[0]).order == Outpost::ShipOrder::Move);
    Assert::IsTrue(Get(match, ships[1]).order == Outpost::ShipOrder::AttackMove);
    Assert::IsTrue(Get(match, ships[2]).order == Outpost::ShipOrder::Attack && Get(match, ships[2]).attackTarget == enemy);
    Assert::IsTrue(Get(match, ships[3]).standing == Outpost::StandingOrder::HoldSector);
    Assert::IsTrue(Get(match, ships[4]).standing == Outpost::StandingOrder::Patrol);
    Assert::IsTrue(Get(match, constructor).order == Outpost::ShipOrder::Work, L"the Constructor goes to build the rig");
  }

  // The condition: enemy warships Blue sees in the action's sector worth more command points than it allows hold the ships in
  // the sector they are in; as many as it allows, and they go.
  TEST_METHOD(TheConditionHoldsThemBackFromAStrongerEnemy)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    (void)Warships(match, RED, B1_EDGE, 3);
    const std::vector<Outpost::EntityId> cautious = Warships(match, BLUE, MUSTER, 2);
    const std::vector<Outpost::EntityId> bold = Warships(match, BLUE, {.xMeters = MUSTER.xMeters, .zMeters = MUSTER.zMeters + 100.0f}, 2);
    match.Run(1);
    const std::uint64_t due = match.World().CurrentTick() + 2;
    const Outpost::ScheduledAction attack = Doing(Outpost::ScheduledActionKind::AttackMove, match.Placement(B1).node);
    (void)match.World().Tick(
      {{.player = BLUE,
        .order = Outpost::ScheduleOrderCommand{.ships = cautious, .trigger = At(due), .action = attack, .unlessCommandPoints = 2}},
       {.player = BLUE,
        .order = Outpost::ScheduleOrderCommand{.ships = bold, .trigger = At(due), .action = attack, .unlessCommandPoints = 3}}});
    const std::vector<Outpost::EventView> fired = RunUntilFired(match, 1);
    Assert::AreEqual(std::size_t{2}, fired.size());
    Assert::IsTrue(fired[0].outcome == Outpost::OrderOutcome::HeldInstead && fired[0].action == Outpost::ScheduledActionKind::HoldSector,
                   L"three Small warships are three points, more than two");
    Assert::AreEqual(A1, fired[0].sector, L"held where they are");
    Assert::IsTrue(Get(match, cautious[0]).standing == Outpost::StandingOrder::HoldSector && Get(match, cautious[0]).holdSector == A1);
    Assert::IsTrue(fired[1].outcome == Outpost::OrderOutcome::AsGiven, L"three is not more than three");
    Assert::IsTrue(Get(match, bold[0]).order == Outpost::ShipOrder::AttackMove &&
                   Get(match, bold[0]).standing == Outpost::StandingOrder::None);
  }

  // What the server refuses when the order fires is told so: a rig in a sector Blue does not hold.
  TEST_METHOD(AnActionRefusedWhenItFiresIsToldSo)
  {
    TerritoryMatch match;
    const Outpost::EntityId constructor = match.Constructors(BLUE).front();
    match.Run(1);
    Assert::IsTrue(Give(match, BLUE,
                        Outpost::ScheduleOrderCommand{.ships = {constructor},
                                                      .trigger = At(match.World().CurrentTick() + 1),
                                                      .action = Doing(Outpost::ScheduledActionKind::BuildRig, match.AsteroidIn(B2))}) ==
                   Outpost::CommandResult::Applied);
    const std::vector<Outpost::EventView> fired = RunUntilFired(match, 1);
    Assert::AreEqual(std::size_t{1}, fired.size());
    Assert::IsTrue(fired[0].outcome == Outpost::OrderOutcome::Refused);
  }

  // Any other order to a ship takes it out of the order it waits on, and the order goes once none waits; a retreat set is no
  // order; and a new scheduled order takes a ship out of its last.
  TEST_METHOD(AnotherOrderTakesAShipOut)
  {
    TerritoryMatch match;
    const std::vector<Outpost::EntityId> ships = Warships(match, BLUE, MUSTER, 3);
    const Outpost::ScheduledAction move = Doing(Outpost::ScheduledActionKind::Move, match.Placement(B1).node);
    const std::uint64_t later = match.World().CurrentTick() + (std::uint64_t{60} * TICKS_PER_SECOND);
    Assert::IsTrue(Give(match, BLUE, Outpost::ScheduleOrderCommand{.ships = ships, .trigger = At(later), .action = move}) ==
                   Outpost::CommandResult::Applied);
    (void)Give(match, BLUE, Outpost::SetRetreatCommand{.ships = ships, .retreat = Outpost::RetreatThreshold::Never});
    Assert::IsTrue(match.World().BuildSnapshot(BLUE).scheduled[0].ships == ships, L"a retreat set is no order");
    (void)Give(match, BLUE, Outpost::StopCommand{.ships = {ships[0]}});
    Assert::IsTrue(match.World().BuildSnapshot(BLUE).scheduled[0].ships == std::vector<Outpost::EntityId>{ships[1], ships[2]});
    Assert::IsTrue(Give(match, BLUE, Outpost::ScheduleOrderCommand{.ships = {ships[1]}, .trigger = At(later), .action = move}) ==
                   Outpost::CommandResult::Applied);
    Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    Assert::AreEqual(std::size_t{2}, blue.scheduled.size());
    Assert::IsTrue(blue.scheduled[0].ships == std::vector<Outpost::EntityId>{ships[2]} &&
                   blue.scheduled[1].ships == std::vector<Outpost::EntityId>{ships[1]});
    (void)Give(match, BLUE, Outpost::HoldSectorCommand{.ships = {ships[2]}, .position = match.Placement(A1).node});
    blue = match.World().BuildSnapshot(BLUE);
    Assert::AreEqual(std::size_t{1}, blue.scheduled.size(), L"the first order is gone with its last ship");
    Assert::AreEqual(std::uint32_t{0}, std::ranges::find(blue.entities, ships[2], &Outpost::EntityView::id)->scheduledOrder);
  }

  TEST_METHOD(RefusesWhatItCannotSchedule)
  {
    TerritoryMatch match;
    const std::vector<Outpost::EntityId> ships = Warships(match, BLUE, MUSTER, 1);
    const Outpost::EntityId constructor = match.Constructors(BLUE).front();
    const Outpost::EntityId enemy = match.Warship(RED, {.xMeters = MUSTER.xMeters + 400.0f, .zMeters = MUSTER.zMeters});
    const Outpost::ScheduledTrigger soon = At(match.World().CurrentTick() + 100);
    const Outpost::ScheduledAction move = Doing(Outpost::ScheduledActionKind::Move, match.Placement(B1).node);
    const auto schedule = [&](std::vector<Outpost::EntityId> _ships, Outpost::ScheduledTrigger _trigger, Outpost::ScheduledAction _action,
                              std::optional<std::int32_t> _unless = std::nullopt, Outpost::PlayerId _player = BLUE)
    {
      return Give(
        match, _player,
        Outpost::ScheduleOrderCommand{.ships = std::move(_ships), .trigger = _trigger, .action = _action, .unlessCommandPoints = _unless});
    };
    Assert::IsTrue(schedule({}, soon, move) == Outpost::CommandResult::NoShips);
    Assert::IsTrue(schedule({constructor}, soon, move) == Outpost::CommandResult::NoShips, L"Constructors take no part");
    Assert::IsTrue(schedule(ships, soon, Doing(Outpost::ScheduledActionKind::BuildRig, match.AsteroidIn(A1))) ==
                   Outpost::CommandResult::NotAConstructor);
    Assert::IsTrue(schedule(ships, On(Outpost::ScheduledTriggerKind::EnemyInSector, 999), move) == Outpost::CommandResult::NoSector);
    Assert::IsTrue(schedule(ships, soon, Doing(Outpost::ScheduledActionKind::HoldSector, {.xMeters = 90'000.0f})) ==
                   Outpost::CommandResult::NoSector);
    Assert::IsTrue(schedule(ships, soon, Doing(Outpost::ScheduledActionKind::Move, {.xMeters = std::numeric_limits<float>::quiet_NaN()})) ==
                   Outpost::CommandResult::InvalidPosition);
    Assert::IsTrue(schedule(ships, soon, Doing(Outpost::ScheduledActionKind::Attack, {}, Outpost::EntityId{99'999})) ==
                   Outpost::CommandResult::UnknownTarget);
    Assert::IsTrue(schedule(ships, soon, Doing(Outpost::ScheduledActionKind::Attack, {}, ships[0])) == Outpost::CommandResult::NotAnEnemy);
    Assert::IsTrue(schedule(ships, soon, move, -1) == Outpost::CommandResult::InvalidCondition);
    Assert::IsTrue(schedule(ships, soon, move, std::nullopt, RED) == Outpost::CommandResult::NotOwned);
    Assert::IsTrue(schedule(ships, soon, Doing(Outpost::ScheduledActionKind::Attack, {}, enemy)) == Outpost::CommandResult::Applied);
    Assert::AreEqual(std::size_t{1}, match.World().BuildSnapshot(BLUE).scheduled.size(), L"what was refused changed nothing");
  }

  // A world saved with an order pending comes back with it, and fires it on the same tick as the world that never stopped.
  TEST_METHOD(ASavedWorldKeepsItsOrders)
  {
    TerritoryMatch match;
    const std::vector<Outpost::EntityId> ships = Warships(match, BLUE, MUSTER, 2);
    const std::uint64_t due = match.World().CurrentTick() + 30;
    (void)Give(match, BLUE,
               Outpost::ScheduleOrderCommand{.ships = ships,
                                             .trigger = At(due),
                                             .action = Doing(Outpost::ScheduledActionKind::AttackMove, match.Placement(B1).node),
                                             .unlessCommandPoints = 4});
    match.Run(5);
    const Outpost::WorldIdentity identity{.seed = 3, .ticksPerSecond = TICKS_PER_SECOND};
    Outpost::Simulation loaded(identity.seed, identity.ticksPerSecond);
    loaded.UseTuning(match.TuningData());
    (void)Outpost::DecodeWorld(Outpost::EncodeWorld(match.World(), identity), identity, loaded);
    Assert::IsTrue(loaded == match.World());
    std::uint64_t firedTick = 0;
    while (match.World().CurrentTick() <= due)
    {
      match.Run(1);
      (void)loaded.Tick({});
      if (!Fired(match).empty())
        firedTick = match.World().CurrentTick();
    }
    Assert::AreEqual(due + 1, firedTick);
    Assert::IsTrue(loaded == match.World(), L"the same world after it fired");
  }
};
} // namespace GameLogicTests
