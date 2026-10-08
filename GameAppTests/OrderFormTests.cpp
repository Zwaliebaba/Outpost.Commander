#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId ME{1};
constexpr Outpost::PlayerId ENEMY{2};
constexpr std::int32_t NORTH = 1;
constexpr std::int32_t SOUTH = 2;
constexpr std::int32_t EAST = 3;

Outpost::EntityView Entity(std::uint32_t _id, Outpost::EntityKind _kind, Outpost::PlayerId _owner, float _xMeters, float _zMeters)
{
  return {.id = Outpost::EntityId{_id}, .kind = _kind, .owner = _owner, .position = {.xMeters = _xMeters, .zMeters = _zMeters}};
}

// Three sectors in a row, North and South held by the player and East by the enemy; two warships and a Constructor of the
// player's in North; and in East an enemy ship, two enemy structures, a pirate structure, and two asteroids, one run dry.
Outpost::Snapshot World()
{
  Outpost::Snapshot snapshot{.tick = 100, .player = ME};
  const auto sector = [](std::int32_t _id, const char* _name, float _minX, Outpost::PlayerId _holder)
  {
    return Outpost::SectorView{.id = _id,
                               .nameUtf8 = _name,
                               .minXMeters = _minX,
                               .maxXMeters = _minX + 1000.0f,
                               .minZMeters = -500.0f,
                               .maxZMeters = 500.0f,
                               .node = {.xMeters = _minX + 500.0f, .zMeters = 100.0f},
                               .holder = _holder};
  };
  snapshot.sectors = {sector(NORTH, "North", -1500.0f, ME), sector(SOUTH, "South", -500.0f, ME), sector(EAST, "East", 500.0f, ENEMY)};
  snapshot.structureTypes = {{.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay"},
                             {.structure = Outpost::StructureKind::MiningRig, .nameUtf8 = "Mining Rig"}};
  snapshot.hulls = {{.id = Outpost::HullId{2}, .nameUtf8 = "Medium"}};
  snapshot.entities = {Entity(1, Outpost::EntityKind::Ship, ME, -1000.0f, 0.0f), Entity(2, Outpost::EntityKind::Ship, ME, -1000.0f, 20.0f),
                       Entity(3, Outpost::EntityKind::Ship, ME, -1000.0f, 40.0f)};
  snapshot.entities[2].role = Outpost::ShipRole::Constructor;
  Outpost::EntityView enemyShip = Entity(10, Outpost::EntityKind::Ship, ENEMY, 900.0f, 0.0f);
  enemyShip.hull = Outpost::HullId{2};
  Outpost::EntityView relay = Entity(11, Outpost::EntityKind::Structure, ENEMY, 1000.0f, 100.0f);
  relay.structure = Outpost::StructureKind::Relay;
  Outpost::EntityView rig = Entity(12, Outpost::EntityKind::Structure, ENEMY, 1100.0f, 0.0f);
  rig.structure = Outpost::StructureKind::MiningRig;
  Outpost::EntityView pirate = Entity(13, Outpost::EntityKind::Structure, Outpost::PIRATES, 1200.0f, 0.0f);
  pirate.structure = Outpost::StructureKind::Relay;
  Outpost::EntityView ore = Entity(20, Outpost::EntityKind::Asteroid, {}, 1300.0f, 200.0f);
  ore.oreReserveHundredths = 1'200'000;
  Outpost::EntityView dry = Entity(21, Outpost::EntityKind::Asteroid, {}, 1300.0f, -200.0f);
  dry.oreReserveHundredths = 0;
  Outpost::EntityView unseen = Entity(22, Outpost::EntityKind::Asteroid, {}, -1300.0f, 0.0f);
  snapshot.entities.insert(snapshot.entities.end(), {enemyShip, relay, rig, pirate, ore, dry, unseen});
  return snapshot;
}

std::vector<std::uint32_t> IdsOf(const std::vector<const Outpost::EntityView*>& _entities)
{
  std::vector<std::uint32_t> ids;
  ids.reserve(_entities.size());
  for (const Outpost::EntityView* entity : _entities)
    ids.push_back(entity->id.value);
  return ids;
}

// The order a form gives, which the test needs it to give.
Outpost::ScheduleOrderCommand Given(const std::optional<Outpost::ScheduleOrderCommand>& _order)
{
  Assert::IsTrue(_order.has_value(), L"the form gives its order");
  return _order.value_or(Outpost::ScheduleOrderCommand{});
}

constexpr std::array SELECTED{Outpost::EntityId{1}, Outpost::EntityId{2}, Outpost::EntityId{3}};
} // namespace

TEST_CLASS(OrderFormTests)
{
public:
  // Phase 5 design §7: each field steps both ways round its end; a time of day by the hour and by five minutes, and the
  // condition from none up to the most command points and back to none.
  TEST_METHOD(StepsEachFieldRoundTheEnd)
  {
    const Outpost::Snapshot world = World();
    Outpost::OrderForm form;
    Assert::AreEqual(2, form.Hour(), L"the design's 02:00");
    Assert::IsTrue(form.Action() == Outpost::ScheduledActionKind::AttackMove);
    form.Step(Outpost::OrderField::Trigger, -1, world, world.entities);
    Assert::IsTrue(form.Trigger() == Outpost::ScheduledTriggerKind::RigLost, L"back round the end");
    form.Step(Outpost::OrderField::Trigger, 1, world, world.entities);
    form.Step(Outpost::OrderField::Trigger, 1, world, world.entities);
    Assert::IsTrue(form.Trigger() == Outpost::ScheduledTriggerKind::EnemyInSector);

    for (int step = 0; step < 3; ++step)
      form.Step(Outpost::OrderField::Hour, -1, world, world.entities);
    Assert::AreEqual(23, form.Hour());
    form.Step(Outpost::OrderField::Hour, 1, world, world.entities);
    Assert::AreEqual(0, form.Hour());
    form.Step(Outpost::OrderField::Minute, -1, world, world.entities);
    Assert::AreEqual(55, form.Minute());
    form.Step(Outpost::OrderField::Minute, 1, world, world.entities);
    form.Step(Outpost::OrderField::Minute, 1, world, world.entities);
    Assert::AreEqual(5, form.Minute());

    form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    Assert::IsTrue(form.Action() == Outpost::ScheduledActionKind::Attack);
    for (int step = 0; step < 4; ++step)
      form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    Assert::IsTrue(form.Action() == Outpost::ScheduledActionKind::Move, L"round the end");

    Assert::IsFalse(form.Condition().has_value());
    form.Step(Outpost::OrderField::Condition, 1, world, world.entities);
    Assert::AreEqual(0, form.Condition().value_or(-1));
    form.Step(Outpost::OrderField::Condition, 1, world, world.entities);
    Assert::AreEqual(Outpost::OrderForm::CONDITION_STEP, form.Condition().value_or(-1));
    form.Step(Outpost::OrderField::Condition, -1, world, world.entities);
    form.Step(Outpost::OrderField::Condition, -1, world, world.entities);
    Assert::IsFalse(form.Condition().has_value(), L"below nothing is none");
    form.Step(Outpost::OrderField::Condition, -1, world, world.entities);
    Assert::AreEqual(Outpost::OrderForm::MOST_COMMAND_POINTS, form.Condition().value_or(-1), L"and round to the most");
    form.Step(Outpost::OrderField::Condition, 1, world, world.entities);
    Assert::IsFalse(form.Condition().has_value());
  }

  // Owner, 2026-10-08: an event trigger watches one sector the player holds, and the action's target is picked when the
  // order is given, among those the player knows in the action's sector: for an attack the enemy's and the pirates'
  // structures and then their ships, and for a rig the asteroids with Ore left. One that is gone gives way to the first.
  TEST_METHOD(OffersTheSectorsAndTargetsThePlayerCanPick)
  {
    Outpost::Snapshot world = World();
    Outpost::OrderForm form;
    form.Update(world, world.entities);
    Assert::AreEqual(NORTH, form.TriggerSector());
    Assert::AreEqual(NORTH, form.ActionSector());
    form.Step(Outpost::OrderField::TriggerSector, 1, world, world.entities);
    Assert::AreEqual(SOUTH, form.TriggerSector());
    form.Step(Outpost::OrderField::TriggerSector, 1, world, world.entities);
    Assert::AreEqual(NORTH, form.TriggerSector(), L"East is not the player's to watch");
    form.Step(Outpost::OrderField::ActionSector, -1, world, world.entities);
    Assert::AreEqual(EAST, form.ActionSector(), L"any sector is a place to go");

    form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    Assert::IsTrue(form.Action() == Outpost::ScheduledActionKind::Attack);
    Assert::IsTrue(IdsOf(form.Targets(world, world.entities)) == std::vector<std::uint32_t>{11, 12, 13, 10});
    Assert::AreEqual(11u, form.Target().value);
    Assert::AreEqual(std::string("Pirate Relay"), Outpost::OrderForm::NameOf(*form.Targets(world, world.entities)[2], world));
    Assert::AreEqual(std::string("Medium ship"), Outpost::OrderForm::NameOf(*form.Targets(world, world.entities)[3], world));
    form.Step(Outpost::OrderField::Target, -1, world, world.entities);
    Assert::AreEqual(10u, form.Target().value);
    std::erase_if(world.entities, [](const Outpost::EntityView& _entity) { return _entity.id.value == 10; });
    form.Update(world, world.entities);
    Assert::AreEqual(11u, form.Target().value, L"gone, so the first");

    for (int step = 0; step < 3; ++step)
      form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    Assert::IsTrue(form.Action() == Outpost::ScheduledActionKind::BuildRig);
    Assert::IsTrue(IdsOf(form.Targets(world, world.entities)) == std::vector<std::uint32_t>{20}, L"not the dry one");
    Assert::AreEqual(std::string("Asteroid, Ore 12,000"), Outpost::OrderForm::NameOf(*form.Targets(world, world.entities)[0], world));
    form.Step(Outpost::OrderField::ActionSector, 1, world, world.entities);
    Assert::IsTrue(IdsOf(form.Targets(world, world.entities)) == std::vector<std::uint32_t>{22}, L"an asteroid of unknown Ore");
    Assert::AreEqual(22u, form.Target().value);

    form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    Assert::IsFalse(form.Target().IsValid(), L"a move takes no target");

    // A sector the player loses is no longer one to watch.
    world.sectors[0].holder = ENEMY;
    form.Update(world, world.entities);
    Assert::AreEqual(SOUTH, form.TriggerSector());
  }

  // The order the form gives the selection (ADR-080): its warships, or its Constructors for a rig; a time of day as the next
  // moment the player's clock reads it, or the sector its event trigger watches; the action's sector's node, or its
  // target; and the condition. Until it can be given, the button says why.
  TEST_METHOD(GivesItsOrderToTheSelection)
  {
    const Outpost::Snapshot world = World();
    const Outpost::PlayerClock clock(std::chrono::hours{2});
    const std::chrono::sys_seconds now = std::chrono::sys_days{std::chrono::year{2026} / 10 / 8} + std::chrono::hours{22};
    Outpost::OrderForm form;
    form.Update(world, world.entities);
    Assert::AreEqual(std::string("SELECT WARSHIPS"), form.Missing({}, world, world.entities));
    Assert::IsFalse(form.Command({}, now, clock, world, world.entities).has_value());

    form.Step(Outpost::OrderField::Condition, 1, world, world.entities);
    form.Step(Outpost::OrderField::Condition, 1, world, world.entities);
    form.Step(Outpost::OrderField::ActionSector, -1, world, world.entities);
    Outpost::ScheduleOrderCommand order = Given(form.Command(SELECTED, now, clock, world, world.entities));
    Assert::IsTrue(order.ships == std::vector<Outpost::EntityId>{Outpost::EntityId{1}, Outpost::EntityId{2}}, L"the warships");
    Assert::IsTrue(order.trigger.kind == Outpost::ScheduledTriggerKind::TimeOfDay);
    // 02:00 two hours ahead of UTC, after midnight there: midnight UTC on the 9th.
    const std::chrono::sys_seconds two = std::chrono::sys_days{std::chrono::year{2026} / 10 / 9};
    Assert::AreEqual(two.time_since_epoch().count(), order.trigger.utcSeconds);
    Assert::AreEqual(std::uint64_t{0}, order.trigger.tick, L"the host makes the tick");
    Assert::IsTrue(order.action.kind == Outpost::ScheduledActionKind::AttackMove);
    Assert::AreEqual(1000.0f, order.action.position.xMeters, L"East's node");
    Assert::AreEqual(2, order.unlessCommandPoints.value_or(-1));

    form.Step(Outpost::OrderField::Trigger, 1, world, world.entities);
    form.Step(Outpost::OrderField::TriggerSector, 1, world, world.entities);
    form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    form.Step(Outpost::OrderField::Target, 1, world, world.entities);
    order = Given(form.Command(SELECTED, now, clock, world, world.entities));
    Assert::IsTrue(order.trigger.kind == Outpost::ScheduledTriggerKind::EnemyInSector);
    Assert::AreEqual(SOUTH, order.trigger.sector);
    Assert::AreEqual(12u, order.action.target.value, L"the Mining Rig");
    Assert::AreEqual(1100.0f, order.action.position.xMeters);

    for (int step = 0; step < 3; ++step)
      form.Step(Outpost::OrderField::Action, 1, world, world.entities);
    order = Given(form.Command(SELECTED, now, clock, world, world.entities));
    Assert::IsTrue(order.ships == std::vector<Outpost::EntityId>{Outpost::EntityId{3}}, L"the Constructor builds the rig");
    Assert::IsFalse(order.action.target.IsValid());
    Assert::AreEqual(1300.0f, order.action.position.xMeters, L"on the asteroid");
    Assert::AreEqual(std::string("SELECT CONSTRUCTORS"), form.Missing(std::vector{Outpost::EntityId{1}}, world, world.entities));

    // Nothing to attack, nothing to watch.
    Outpost::Snapshot empty = world;
    std::erase_if(empty.entities, [](const Outpost::EntityView& _entity) { return _entity.owner == ENEMY || !_entity.owner.IsValid(); });
    form.Update(empty, empty.entities);
    Assert::AreEqual(std::string("NO ASTEROID THERE"), form.Missing(SELECTED, empty, empty.entities));
    for (auto& sector : empty.sectors)
      sector.holder = ENEMY;
    form.Update(empty, empty.entities);
    Assert::AreEqual(std::string("NO SECTOR HELD"), form.Missing(SELECTED, empty, empty.entities));
  }

  // A scheduled order in words, its time of day on the player's clock; and the line a ship's panel adds for the order it
  // waits on, or for how many its selection waits on (design §11).
  TEST_METHOD(DescribesAScheduledOrderAndTheOneAShipWaitsOn)
  {
    Outpost::Snapshot world = World();
    const Outpost::PlayerClock clock(std::chrono::hours{2});
    const std::int64_t midnight =
      std::chrono::sys_seconds{std::chrono::sys_days{std::chrono::year{2026} / 10 / 9}}.time_since_epoch().count();
    world.scheduled = {{.id = 7,
                        .ships = {Outpost::EntityId{1}},
                        .trigger = {.kind = Outpost::ScheduledTriggerKind::TimeOfDay, .utcSeconds = midnight},
                        .action = {.kind = Outpost::ScheduledActionKind::AttackMove, .position = {.xMeters = 1000.0f}},
                        .unlessCommandPoints = 8},
                       {.id = 9,
                        .ships = {Outpost::EntityId{2}},
                        .trigger = {.kind = Outpost::ScheduledTriggerKind::RelayThreatened, .sector = SOUTH},
                        .action = {.kind = Outpost::ScheduledActionKind::HoldSector, .position = {.xMeters = 0.0f}}}};
    Assert::AreEqual(std::string("at 02:00, attack-move to East, unless over 8 enemy CP"),
                     Outpost::DescribeScheduledOrder(world.scheduled[0], world, clock));
    Assert::AreEqual(std::string("when the Relay in South is threatened, hold South"),
                     Outpost::DescribeScheduledOrder(world.scheduled[1], world, clock));

    Assert::IsFalse(Outpost::PendingOrderLine(SELECTED, world.entities, world, clock).has_value(), L"none waits");
    world.entities[0].scheduledOrder = 7;
    Assert::AreEqual(std::string("Scheduled: at 02:00, attack-move to East, unless over 8 enemy CP"),
                     Outpost::PendingOrderLine(SELECTED, world.entities, world, clock).value_or(""));
    world.entities[1].scheduledOrder = 9;
    Assert::AreEqual(std::string("Scheduled: 2 orders"), Outpost::PendingOrderLine(SELECTED, world.entities, world, clock).value_or(""));
    Assert::AreEqual(std::string("Scheduled: when the Relay in South is threatened, hold South"),
                     Outpost::PendingOrderLine(std::vector{Outpost::EntityId{2}}, world.entities, world, clock).value_or(""));
  }
};
} // namespace GameAppTests
