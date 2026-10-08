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
// A sector of the repository's map next to Blue's home.
constexpr std::int32_t B1 = 2;

std::vector<Outpost::EventView> EventsOf(TerritoryMatch& _match, Outpost::PlayerId _player)
{
  return _match.World().BuildSnapshot(_player).events;
}

std::size_t Count(const std::vector<Outpost::EventView>& _events, Outpost::EventKind _kind)
{
  return static_cast<std::size_t>(std::ranges::count(_events, _kind, &Outpost::EventView::kind));
}

const Outpost::EventView* Find(const std::vector<Outpost::EventView>& _events, Outpost::EventKind _kind)
{
  const auto found = std::ranges::find(_events, _kind, &Outpost::EventView::kind);
  return found != _events.end() ? &*found : nullptr;
}

// Runs a tick at a time, at most _seconds, until _player's snapshot holds an event of _kind; its events of that tick, which
// are empty when none came.
std::vector<Outpost::EventView> RunUntil(TerritoryMatch& _match, Outpost::PlayerId _player, Outpost::EventKind _kind,
                                         std::uint32_t _seconds)
{
  for (std::uint32_t tick = 0; tick < _seconds * TICKS_PER_SECOND; ++tick)
  {
    _match.Run(1);
    std::vector<Outpost::EventView> events = EventsOf(_match, _player);
    if (Find(events, _kind) != nullptr)
      return events;
  }
  return {};
}

const Outpost::Entity& StationOf(TerritoryMatch& _match, Outpost::PlayerId _player)
{
  const auto& entities = _match.World().Entities();
  return *std::ranges::find_if(entities, [_player](const Outpost::Entity& _entity)
                               { return _entity.owner == _player && _entity.structure == Outpost::StructureKind::CommandStation; });
}
} // namespace

// Phase 5 design §7 (ADR-080): the server raises each player's events in the tick they happen, and the player's snapshot of
// that tick carries them, for its alerts and its scheduled orders.
TEST_CLASS(EventTests)
{
public:
  // An enemy warship near Blue's Relay is seen entering Blue's sector and suppresses the Relay: each once, and only Blue's.
  // A suppressed sector is no longer seen whole (ADR-056), so the warship stands where the Relay itself sees it.
  TEST_METHOD(AnEnemyEnteringAndARelaySuppressedOnceForItsHolder)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    match.Run(1);
    Assert::AreEqual(std::size_t{0}, Count(EventsOf(match, BLUE), Outpost::EventKind::EnemyEntered));

    const Outpost::PlanePosition node = match.Placement(B1).node;
    const Outpost::EntityId red = match.Warship(RED, {.xMeters = node.xMeters + 150.0f, .zMeters = node.zMeters});
    match.Run(1);
    const std::vector<Outpost::EventView> blue = EventsOf(match, BLUE);
    Assert::AreEqual(std::size_t{1}, Count(blue, Outpost::EventKind::EnemyEntered));
    Assert::IsNotNull(Find(blue, Outpost::EventKind::EnemyEntered));
    const Outpost::EventView& entered = *Find(blue, Outpost::EventKind::EnemyEntered);
    Assert::AreEqual(B1, entered.sector);
    Assert::IsTrue(entered.other == RED && entered.subject == red);
    Assert::AreEqual(std::size_t{1}, Count(blue, Outpost::EventKind::RelaySuppressed));
    Assert::AreEqual(B1, Find(blue, Outpost::EventKind::RelaySuppressed)->sector);
    const std::vector<Outpost::EventView> redEvents = EventsOf(match, RED);
    Assert::AreEqual(std::size_t{0},
                     Count(redEvents, Outpost::EventKind::EnemyEntered) + Count(redEvents, Outpost::EventKind::RelaySuppressed),
                     L"they are Blue's");

    match.Run(1);
    const std::vector<Outpost::EventView> after = EventsOf(match, BLUE);
    Assert::AreEqual(std::size_t{0}, Count(after, Outpost::EventKind::EnemyEntered) + Count(after, Outpost::EventKind::RelaySuppressed),
                     L"still there, not again");
  }

  // Shots at Blue's Relay raise one event a tick, naming whose shot hit it, however many hit it in the tick.
  TEST_METHOD(ARelayHitOnceATickNamingWhoseShot)
  {
    TerritoryMatch match;
    const Outpost::EntityId relay = match.Relay(BLUE, B1);
    const Outpost::PlanePosition node = match.Placement(B1).node;
    for (int i = 0; i < 10; ++i)
      (void)match.Warship(RED, {.xMeters = node.xMeters + 80.0f, .zMeters = node.zMeters + (20.0f * static_cast<float>(i - 5))});
    std::size_t ticksHit = 0;
    std::size_t mostShots = 0;
    for (std::uint32_t tick = 0; tick < 5 * TICKS_PER_SECOND && match.World().FindEntity(relay) != nullptr; ++tick)
    {
      match.Run(1);
      const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
      const auto shots = static_cast<std::size_t>(std::ranges::count(blue.shots, relay, &Outpost::ShotView::target));
      mostShots = std::max(mostShots, shots);
      Assert::AreEqual(std::min<std::size_t>(shots, 1), Count(blue.events, Outpost::EventKind::RelayAttacked), L"once a tick it is hit");
      if (const Outpost::EventView* attacked = Find(blue.events, Outpost::EventKind::RelayAttacked))
      {
        ++ticksHit;
        Assert::IsTrue(attacked->subject == relay && attacked->other == RED && attacked->sector == B1);
      }
      Assert::AreEqual(std::size_t{0}, Count(EventsOf(match, RED), Outpost::EventKind::RelayAttacked));
    }
    Assert::IsTrue(ticksHit > 0, L"it was hit");
    Assert::IsTrue(mostShots > 1, L"several shots hit it in one tick");
  }

  // What Blue builds and loses: a Constructor, a Relay and the sector it holds, and then the Relay and the sector again.
  TEST_METHOD(WhatAPlayerBuildsAndLoses)
  {
    TerritoryMatch match;
    const Outpost::EntityId station = StationOf(match, BLUE).id;
    (void)match.World().Tick({{.player = BLUE, .order = Outpost::QueueShipCommand{.producer = station}}});
    const std::vector<Outpost::EventView> shipBuilt = RunUntil(match, BLUE, Outpost::EventKind::ShipBuilt, 120);
    Assert::AreEqual(std::size_t{1}, Count(shipBuilt, Outpost::EventKind::ShipBuilt), L"a Constructor delivered");
    const Outpost::Entity* constructor = match.World().FindEntity(Find(shipBuilt, Outpost::EventKind::ShipBuilt)->subject);
    Assert::IsTrue(constructor != nullptr && constructor->owner == BLUE && constructor->role == Outpost::ShipRole::Constructor);

    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(B1).node) == Outpost::CommandResult::Applied);
    const std::vector<Outpost::EventView> built = RunUntil(match, BLUE, Outpost::EventKind::StructureBuilt, 300);
    Assert::IsNotNull(Find(built, Outpost::EventKind::StructureBuilt), L"the Relay finished");
    Assert::IsTrue(Find(built, Outpost::EventKind::StructureBuilt)->structure == Outpost::StructureKind::Relay);
    const Outpost::EventView* gained = Find(built, Outpost::EventKind::SectorGained);
    Assert::IsTrue(gained != nullptr && gained->sector == B1, L"held in the tick it is built");
    const Outpost::EntityId relay = Find(built, Outpost::EventKind::StructureBuilt)->subject;

    const Outpost::PlanePosition node = match.Placement(B1).node;
    std::vector<Outpost::EntityId> raiders;
    raiders.reserve(4);
    for (int i = 0; i < 4; ++i)
      raiders.push_back(match.Warship(RED, {.xMeters = node.xMeters + 80.0f, .zMeters = node.zMeters + (30.0f * static_cast<float>(i))}));
    match.FightToTheEnd(RED, raiders);
    const std::vector<Outpost::EventView> lost = RunUntil(match, BLUE, Outpost::EventKind::StructureLost, 120);
    const Outpost::EventView* structureLost = Find(lost, Outpost::EventKind::StructureLost);
    Assert::IsTrue(structureLost != nullptr && structureLost->subject == relay &&
                   structureLost->structure == Outpost::StructureKind::Relay);
    const Outpost::EventView* sectorLost = Find(lost, Outpost::EventKind::SectorLost);
    Assert::IsTrue(sectorLost != nullptr && sectorLost->sector == B1, L"lost in the tick its Relay falls");
    Assert::AreEqual(std::size_t{0}, Count(EventsOf(match, RED), Outpost::EventKind::StructureLost), L"Blue's loss");
  }

  // A ship going back to be repaired is told once, as it starts: not again when the Repair Bay it heads for falls and it turns
  // for the Command Station (ADR-075). A ship lost is its owner's loss, and the other side hears nothing of it.
  TEST_METHOD(AShipGoingBackOnceAndAShipLost)
  {
    TerritoryMatch match;
    const Outpost::PlanePosition field{.xMeters = -2500.0f, .zMeters = -2500.0f};
    (void)match.Structure(RED, Outpost::StructureKind::DefensePlatform, {.xMeters = field.xMeters + 240.0f, .zMeters = field.zMeters});
    // A Repair Bay of Blue's, which a ship goes back to before the Command Station, with a hit point left.
    const Outpost::PlanePosition bayAt{.xMeters = -2500.0f, .zMeters = -1800.0f};
    const Outpost::EntityId bay = match.World().SpawnStructure(BLUE, Outpost::StructureKind::RepairBay, bayAt, 40.0f, Outpost::HUNDREDTHS);
    const Outpost::EntityId going = match.Warship(BLUE, field);
    (void)match.World().Tick(
      {{.player = BLUE, .order = Outpost::SetRetreatCommand{.ships = {going}, .retreat = Outpost::RetreatThreshold::Half}}});
    const std::vector<Outpost::EventView> back = RunUntil(match, BLUE, Outpost::EventKind::ShipRetreating, 60);
    Assert::AreEqual(std::size_t{1}, Count(back, Outpost::EventKind::ShipRetreating));
    Assert::IsTrue(Find(back, Outpost::EventKind::ShipRetreating)->subject == going);
    Assert::IsTrue(match.World().FindEntity(going)->repairer == bay, L"it heads for the bay");

    const Outpost::EntityId raider = match.Warship(RED, {.xMeters = bayAt.xMeters + 80.0f, .zMeters = bayAt.zMeters});
    match.FightToTheEnd(RED, {raider});
    (void)match.World().Tick({{.player = RED, .order = Outpost::AttackCommand{.ships = {raider}, .target = bay}}});
    std::size_t again = 0;
    for (std::uint32_t tick = 0; tick < 10 * TICKS_PER_SECOND; ++tick)
    {
      match.Run(1);
      again += Count(EventsOf(match, BLUE), Outpost::EventKind::ShipRetreating);
    }
    Assert::IsNull(match.World().FindEntity(bay), L"the bay fell");
    const Outpost::Entity* ship = match.World().FindEntity(going);
    Assert::IsTrue(ship != nullptr && ship->retreating && ship->repairer.IsValid() && ship->repairer != bay, L"it turned for another");
    Assert::AreEqual(std::size_t{0}, again, L"once, as it started");

    const Outpost::EntityId stays = match.Warship(BLUE, field);
    match.FightToTheEnd(BLUE, {stays});
    const std::vector<Outpost::EventView> lost = RunUntil(match, BLUE, Outpost::EventKind::ShipLost, 120);
    Assert::IsNotNull(Find(lost, Outpost::EventKind::ShipLost));
    Assert::IsTrue(Find(lost, Outpost::EventKind::ShipLost)->subject == stays);
    Assert::AreEqual(std::size_t{0}, Count(EventsOf(match, RED), Outpost::EventKind::ShipLost));
  }

  // A camp cleared tells both players, once (ADR-073).
  TEST_METHOD(PiratesClearedForEveryPlayerOnce)
  {
    TerritoryMatch match(true);
    const auto camp = std::ranges::find(match.Outposts(), "camp", &Outpost::OutpostPlacement::outpost);
    Assert::IsTrue(camp != match.Outposts().end());
    const std::int32_t sector = camp->sector;
    const Outpost::PlanePosition node = match.Placement(sector).node;
    const Outpost::DesignId design = match.World().FindDesign(BLUE, {Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{2}})->id;
    std::vector<Outpost::EntityId> fleet;
    fleet.reserve(10);
    for (int i = 0; i < 10; ++i)
      fleet.push_back(match.World().SpawnShip(
        BLUE, design, {.xMeters = node.xMeters - 700.0f, .zMeters = node.zMeters + (30.0f * static_cast<float>(i))}));
    match.FightToTheEnd(BLUE, fleet);
    std::size_t blueCleared = 0;
    std::size_t redCleared = 0;
    const auto guarded = [&match, sector]()
    {
      return std::ranges::any_of(match.World().Entities(),
                                 [&](const Outpost::Entity& _entity)
                                 {
                                   return _entity.owner == Outpost::PIRATES && _entity.kind == Outpost::EntityKind::Structure &&
                                          match.Placement(sector).Contains(_entity.position);
                                 });
    };
    for (std::uint32_t tick = 0; tick < 180 * TICKS_PER_SECOND && (guarded() || blueCleared == 0); ++tick)
    {
      std::vector<Outpost::Command> orders;
      if (tick % (5 * TICKS_PER_SECOND) == 0)
      {
        std::erase_if(fleet, [&match](Outpost::EntityId _ship) { return match.World().FindEntity(_ship) == nullptr; });
        orders.push_back({.player = BLUE, .order = Outpost::AttackMoveCommand{.ships = fleet, .destination = node}});
      }
      (void)match.World().Tick(orders);
      blueCleared += Count(EventsOf(match, BLUE), Outpost::EventKind::PiratesCleared);
      redCleared += Count(EventsOf(match, RED), Outpost::EventKind::PiratesCleared);
    }
    Assert::IsFalse(guarded(), L"the camp was cleared");
    Assert::AreEqual(std::size_t{1}, blueCleared);
    Assert::AreEqual(std::size_t{1}, redCleared, L"every player hears of it");
  }
};
} // namespace GameLogicTests
