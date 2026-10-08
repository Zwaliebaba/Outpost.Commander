#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace OpponentTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr Outpost::DesignId LIGHT{7};
constexpr Outpost::DesignId HEAVY{8};
constexpr Outpost::HullId SMALL_HULL{1};
constexpr Outpost::HullId LARGE_HULL{3};
constexpr std::int32_t LIGHT_COST = 50;
constexpr std::int32_t STATION_UPGRADE_COST = 300;
constexpr std::int32_t RIG_COST = 100;
constexpr Outpost::PlanePosition ASTEROID{.xMeters = 400.0f, .zMeters = 0.0f};

Outpost::AiSettings RepositorySettings()
{
  return Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
}

// A snapshot of BLUE's empire as the deputy reads it, built by hand: a sector BLUE holds round the origin and a free one
// east of it, and what each test adds, each entity given the next identifier, so that they stay in identifier order.
class Empire
{
public:
  Empire()
  {
    m_snapshot.player = BLUE;
    m_snapshot.ore = 1000;
    m_snapshot.mapSizeMeters = 8000.0f;
    m_snapshot.constructorCost = 40;
    m_snapshot.structureTypes = {
      {.structure = Outpost::StructureKind::CommandStation, .radiusMeters = 60.0f, .levels = {{.cost = STATION_UPGRADE_COST}}},
      {.structure = Outpost::StructureKind::Shipyard, .radiusMeters = 40.0f, .buildable = true, .cost = 200},
      {.structure = Outpost::StructureKind::ResearchLab, .radiusMeters = 40.0f, .buildable = true, .cost = 200},
      {.structure = Outpost::StructureKind::MiningRig, .radiusMeters = 30.0f, .buildable = true, .cost = RIG_COST}};
    m_snapshot.hulls = {{.id = SMALL_HULL, .available = true, .shipyardLevel = 1, .commandPoints = 2},
                        {.id = LARGE_HULL, .available = true, .shipyardLevel = 3, .commandPoints = 6}};
    m_snapshot.designs = {{.id = LIGHT, .hull = SMALL_HULL, .cost = LIGHT_COST}, {.id = HEAVY, .hull = SMALL_HULL, .cost = 60}};
    m_snapshot.sectors = {
      {.id = 1, .minXMeters = -1000.0f, .maxXMeters = 1000.0f, .minZMeters = -1000.0f, .maxZMeters = 1000.0f, .holder = BLUE},
      {.id = 2, .minXMeters = 1000.0f, .maxXMeters = 3000.0f, .minZMeters = -1000.0f, .maxZMeters = 1000.0f}};
  }

  Outpost::EntityId Add(Outpost::EntityView _entity)
  {
    _entity.id = Outpost::EntityId{m_nextId++};
    m_snapshot.entities.push_back(std::move(_entity));
    return m_snapshot.entities.back().id;
  }

  Outpost::EntityId Structure(Outpost::StructureKind _kind, Outpost::PlanePosition _position, Outpost::PlayerId _owner = BLUE)
  {
    return Add({.kind = Outpost::EntityKind::Structure,
                .owner = _owner,
                .structure = _kind,
                .position = _position,
                .radiusMeters = 40.0f,
                .hitPointsHundredths = 100'000,
                .maxHitPointsHundredths = 100'000});
  }

  Outpost::EntityId Ship(Outpost::ShipRole _role, Outpost::PlanePosition _position, Outpost::PlayerId _owner = BLUE,
                         Outpost::DesignId _design = LIGHT)
  {
    return Add({.kind = Outpost::EntityKind::Ship,
                .owner = _owner,
                .design = _role == Outpost::ShipRole::Warship ? _design : Outpost::DesignId{},
                .hull = _role == Outpost::ShipRole::Warship ? SMALL_HULL : Outpost::HullId{},
                .role = _role,
                .position = _position,
                .radiusMeters = 8.0f});
  }

  Outpost::EntityView& Get(Outpost::EntityId _id)
  {
    return *std::ranges::find(m_snapshot.entities, _id, &Outpost::EntityView::id);
  }

  void Remove(Outpost::EntityId _id)
  {
    std::erase_if(m_snapshot.entities, [_id](const Outpost::EntityView& _entity) { return _entity.id == _id; });
  }

  // The snapshot _seconds later.
  Outpost::Snapshot& After(double _seconds)
  {
    m_snapshot.tick += static_cast<std::uint64_t>(std::llround(_seconds * TICKS_PER_SECOND));
    return m_snapshot;
  }

  [[nodiscard]] Outpost::Snapshot& Now() noexcept
  {
    return m_snapshot;
  }

private:
  Outpost::Snapshot m_snapshot;
  std::uint32_t m_nextId = 1;
};

template <typename Order> std::vector<Order> OrdersOf(const std::vector<Outpost::Command>& _commands)
{
  std::vector<Order> orders;
  for (const Outpost::Command& command : _commands)
  {
    if (const Order* order = std::get_if<Order>(&command.order))
      orders.push_back(*order);
  }
  return orders;
}
} // namespace

// Phase 5 design §6, gate H3 (ADR-079): a deputy keeps its player's empire running, and plays for nothing more.
TEST_CLASS(DeputyTests)
{
public:
  // A Shipyard whose queue empties builds again what it last built; one it never saw build, the design most of its
  // player's warships are of.
  TEST_METHOD(BuildsAgainWhatEachShipyardLastBuilt)
  {
    Empire empire;
    const Outpost::EntityId watched = empire.Structure(Outpost::StructureKind::Shipyard, {200.0f, 0.0f});
    empire.Get(watched).queue = {{.role = Outpost::ShipRole::Warship, .design = LIGHT},
                                 {.role = Outpost::ShipRole::Warship, .design = HEAVY}};
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    deputy.Watch(empire.Now());

    const Outpost::EntityId unseen = empire.Structure(Outpost::StructureKind::Shipyard, {-200.0f, 0.0f});
    empire.Get(watched).queue.clear();
    for (int i = 0; i < 3; ++i)
      (void)empire.Ship(Outpost::ShipRole::Warship, {0.0f, 100.0f + (20.0f * static_cast<float>(i))}, BLUE, LIGHT);
    (void)empire.Ship(Outpost::ShipRole::Warship, {0.0f, -100.0f}, BLUE, HEAVY);
    const std::vector<Outpost::QueueShipCommand> queued = OrdersOf<Outpost::QueueShipCommand>(deputy.Play(empire.After(1.0)));
    Assert::AreEqual(size_t{2}, queued.size());
    Assert::IsTrue(std::ranges::contains(queued, HEAVY, &Outpost::QueueShipCommand::design), L"the last job it saw queued there");
    Assert::IsTrue(std::ranges::any_of(queued, [&](const Outpost::QueueShipCommand& _queued)
                                       { return _queued.producer == unseen && _queued.design == LIGHT; }),
                   L"the design most of the fleet is of, at a Shipyard it never saw build");
  }

  // A job waits in its queue until the Ore and the fleet cap let it start, so a Shipyard is kept a job ahead whatever its
  // player can pay for; when the cap holds the next ship back, the Command Station's next level raises it.
  TEST_METHOD(KeepsAJobQueuedAndUpgradesTheStationWhenTheCapHoldsItBack)
  {
    Empire empire;
    const Outpost::EntityId station = empire.Structure(Outpost::StructureKind::CommandStation, {0.0f, 0.0f});
    const Outpost::EntityId yard = empire.Structure(Outpost::StructureKind::Shipyard, {200.0f, 0.0f});
    empire.Get(yard).queue = {{.role = Outpost::ShipRole::Warship, .design = LIGHT}};
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    deputy.Watch(empire.Now());
    empire.Get(yard).queue.clear();

    empire.Now().ore = LIGHT_COST - 1;
    std::vector<Outpost::Command> orders = deputy.Play(empire.After(1.0));
    Assert::AreEqual(size_t{1}, OrdersOf<Outpost::QueueShipCommand>(orders).size(), L"a job that waits for the Ore");
    Assert::IsTrue(OrdersOf<Outpost::UpgradeStructureCommand>(orders).empty());

    empire.Get(yard).queue = {{.role = Outpost::ShipRole::Warship, .design = LIGHT}};
    empire.Now().ore = STATION_UPGRADE_COST;
    empire.Now().fleetCap = 10;
    empire.Now().commandPoints = 9;
    orders = deputy.Play(empire.After(1.0));
    Assert::IsTrue(OrdersOf<Outpost::QueueShipCommand>(orders).empty(), L"one job ahead is enough");
    const std::vector<Outpost::UpgradeStructureCommand> upgrades = OrdersOf<Outpost::UpgradeStructureCommand>(orders);
    Assert::IsTrue(upgrades.size() == 1 && upgrades.front().structure == station, L"the station's next level, which raises the cap");
  }

  // Owner, 2026-10-08: as many Constructors as its player had when it took the seat.
  TEST_METHOD(KeepsThePlayersConstructors)
  {
    Empire empire;
    const Outpost::EntityId station = empire.Structure(Outpost::StructureKind::CommandStation, {0.0f, 0.0f});
    const Outpost::EntityId first = empire.Ship(Outpost::ShipRole::Constructor, {100.0f, 100.0f});
    (void)empire.Ship(Outpost::ShipRole::Constructor, {120.0f, 100.0f});
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    Assert::IsTrue(OrdersOf<Outpost::QueueShipCommand>(deputy.Play(empire.Now())).empty(), L"it has the two it keeps");

    empire.Remove(first);
    const std::vector<Outpost::QueueShipCommand> queued = OrdersOf<Outpost::QueueShipCommand>(deputy.Play(empire.After(1.0)));
    Assert::IsTrue(queued.size() == 1 && queued.front().producer == station && !queued.front().design.IsValid(), L"a Constructor lost");

    empire.Get(station).queue = {{.role = Outpost::ShipRole::Constructor}};
    Assert::IsTrue(OrdersOf<Outpost::QueueShipCommand>(deputy.Play(empire.After(1.0))).empty(), L"and one queued is counted");
  }

  TEST_METHOD(KeepsItsPlayerResearching)
  {
    Empire empire;
    const Outpost::EntityId lab = empire.Structure(Outpost::StructureKind::ResearchLab, {0.0f, 200.0f});
    empire.Now().research = {{.id = Outpost::ResearchTopicId{1}, .researched = true},
                             {.id = Outpost::ResearchTopicId{2}, .prerequisites = {Outpost::ResearchTopicId{1}}},
                             {.id = Outpost::ResearchTopicId{3}, .prerequisites = {Outpost::ResearchTopicId{4}}},
                             {.id = Outpost::ResearchTopicId{5}, .tier = 2}};
    Outpost::AiSettings settings = RepositorySettings();
    settings.researchOrder = {Outpost::ResearchTopicId{9}, Outpost::ResearchTopicId{5}, Outpost::ResearchTopicId{2}};
    Outpost::Deputy deputy(settings, TICKS_PER_SECOND);
    std::vector<Outpost::StartResearchCommand> started = OrdersOf<Outpost::StartResearchCommand>(deputy.Play(empire.Now()));
    Assert::IsTrue(started.size() == 1 && started.front().lab == lab && started.front().topic == Outpost::ResearchTopicId{2},
                   L"the AI's order, past a topic the match lacks and one of a tier the Lab has not opened");

    settings.researchOrder.clear();
    Outpost::Deputy noOrder(settings, TICKS_PER_SECOND);
    started = OrdersOf<Outpost::StartResearchCommand>(noOrder.Play(empire.Now()));
    Assert::IsTrue(started.size() == 1 && started.front().topic == Outpost::ResearchTopicId{2}, L"then any topic the Lab can take");

    empire.Get(lab).research = {Outpost::ResearchTopicId{2}};
    Assert::IsTrue(OrdersOf<Outpost::StartResearchCommand>(deputy.Play(empire.After(1.0))).empty(), L"not while its one slot is busy");
  }

  TEST_METHOD(RebuildsALostRigInASectorItsPlayerHolds)
  {
    Empire empire;
    (void)empire.Add({.kind = Outpost::EntityKind::Asteroid, .position = ASTEROID, .radiusMeters = 30.0f, .oreReserveHundredths = 50'000});
    const Outpost::EntityId rig = empire.Structure(Outpost::StructureKind::MiningRig, ASTEROID);
    const Outpost::EntityId second = empire.Ship(Outpost::ShipRole::Constructor, {300.0f, 0.0f});
    const Outpost::EntityId nearest = empire.Ship(Outpost::ShipRole::Constructor, {350.0f, 0.0f});
    const Outpost::EntityId distant = empire.Ship(Outpost::ShipRole::Constructor, {-800.0f, 0.0f});
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    deputy.Watch(empire.Now());

    empire.Remove(rig);
    const std::vector<Outpost::BuildStructureCommand> built = OrdersOf<Outpost::BuildStructureCommand>(deputy.Play(empire.After(1.0)));
    Assert::AreEqual(size_t{1}, built.size());
    Assert::IsTrue(built.front().structure == Outpost::StructureKind::MiningRig && built.front().position == ASTEROID);
    Assert::IsTrue(std::ranges::contains(built.front().constructors, second) &&
                     std::ranges::contains(built.front().constructors, nearest) &&
                     !std::ranges::contains(built.front().constructors, distant),
                   L"the two nearest Constructors");
    Assert::IsTrue(OrdersOf<Outpost::BuildStructureCommand>(deputy.Play(empire.After(1.0))).empty(), L"once, while its site is due");

    empire.Now().sectors.front().holder = RED;
    Assert::IsTrue(OrdersOf<Outpost::BuildStructureCommand>(deputy.Play(empire.After(5.0))).empty(), L"not in a sector it lost");
  }

  TEST_METHOD(RepairsAndFinishesItsPlayersStructures)
  {
    Empire empire;
    const Outpost::EntityId damaged = empire.Structure(Outpost::StructureKind::Shipyard, {200.0f, 0.0f});
    empire.Get(damaged).hitPointsHundredths = 40'000;
    const Outpost::EntityId site = empire.Structure(Outpost::StructureKind::ResearchLab, {-200.0f, 0.0f});
    empire.Get(site).builtPermille = 300;
    (void)empire.Structure(Outpost::StructureKind::CommandStation, {0.0f, 0.0f});
    (void)empire.Ship(Outpost::ShipRole::Constructor, {150.0f, 0.0f});
    (void)empire.Ship(Outpost::ShipRole::Constructor, {-150.0f, 0.0f});
    const Outpost::EntityId busy = empire.Ship(Outpost::ShipRole::Constructor, {0.0f, 150.0f});
    empire.Get(busy).order = Outpost::ShipOrder::Work;
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    const std::vector<Outpost::RepairCommand> repairs = OrdersOf<Outpost::RepairCommand>(deputy.Play(empire.Now()));
    Assert::AreEqual(size_t{2}, repairs.size(), L"one Constructor each, and none that is busy");
    Assert::IsTrue(std::ranges::contains(repairs, damaged, &Outpost::RepairCommand::target) &&
                   std::ranges::contains(repairs, site, &Outpost::RepairCommand::target));
    Assert::IsTrue(std::ranges::none_of(repairs, [busy](const Outpost::RepairCommand& _repair)
                                        { return std::ranges::contains(_repair.constructors, busy); }));
  }

  // Owner, 2026-10-08: its idle warships go at an attack on a sector its player holds, and back to where they stood once
  // it is over.
  TEST_METHOD(DefendsItsPlayersSectorsAndGoesBack)
  {
    Empire empire;
    const Outpost::PlanePosition west{-500.0f, 300.0f};
    const Outpost::PlanePosition south{0.0f, -600.0f};
    const Outpost::EntityId first = empire.Ship(Outpost::ShipRole::Warship, west);
    const Outpost::EntityId second = empire.Ship(Outpost::ShipRole::Warship, south);
    const Outpost::EntityId moving = empire.Ship(Outpost::ShipRole::Warship, {100.0f, 100.0f});
    empire.Get(moving).order = Outpost::ShipOrder::Move;
    const Outpost::EntityId holding = empire.Ship(Outpost::ShipRole::Warship, {150.0f, 100.0f});
    empire.Get(holding).standing = Outpost::StandingOrder::HoldSector;
    const Outpost::EntityId retreating = empire.Ship(Outpost::ShipRole::Warship, {200.0f, 100.0f});
    empire.Get(retreating).retreating = true;
    // One that waits on a scheduled order of its player's, which an order of the deputy's would take it out of (ADR-080).
    const Outpost::EntityId waiting = empire.Ship(Outpost::ShipRole::Warship, {250.0f, 100.0f});
    empire.Get(waiting).scheduledOrder = 1;
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    Assert::IsTrue(OrdersOf<Outpost::AttackMoveCommand>(deputy.Play(empire.Now())).empty(), L"nothing to answer");

    const Outpost::PlanePosition attack{700.0f, 0.0f};
    const Outpost::EntityId raider = empire.Ship(Outpost::ShipRole::Warship, attack, RED);
    (void)empire.Ship(Outpost::ShipRole::Warship, {2000.0f, 0.0f}, RED);
    const std::vector<Outpost::AttackMoveCommand> sent = OrdersOf<Outpost::AttackMoveCommand>(deputy.Play(empire.After(1.0)));
    Assert::AreEqual(size_t{1}, sent.size());
    Assert::IsTrue(sent.front().destination == attack, L"at the enemy in its sector, not the one beyond it");
    Assert::IsTrue(std::ranges::contains(sent.front().ships, first) && std::ranges::contains(sent.front().ships, second));
    Assert::AreEqual(size_t{2}, sent.front().ships.size(), L"its idle warships only, not one that waits on an order");

    empire.Remove(raider);
    empire.Get(first).position = attack;
    empire.Get(second).position = attack;
    Assert::IsTrue(OrdersOf<Outpost::MoveCommand>(deputy.Play(empire.After(1.0))).empty(), L"they hold there a while");
    const std::vector<Outpost::MoveCommand> back =
      OrdersOf<Outpost::MoveCommand>(deputy.Play(empire.After(RepositorySettings().defenseHoldSeconds)));
    Assert::AreEqual(size_t{2}, back.size());
    for (const Outpost::MoveCommand& move : back)
    {
      Assert::AreEqual(size_t{1}, move.ships.size());
      Assert::IsTrue(move.destination == (move.ships.front() == first ? west : south), L"each to where it stood");
    }
    Assert::AreEqual(size_t{0}, deputy.Defenders());
  }

  // A turn begins afresh from what it sees: the Constructors it keeps are the ones its player has when it takes the seat
  // again, not at its last turn.
  TEST_METHOD(BeginsEachTurnFromWhatItsPlayerHas)
  {
    Empire empire;
    (void)empire.Structure(Outpost::StructureKind::CommandStation, {0.0f, 0.0f});
    (void)empire.Ship(Outpost::ShipRole::Constructor, {100.0f, 100.0f});
    const Outpost::EntityId second = empire.Ship(Outpost::ShipRole::Constructor, {120.0f, 100.0f});
    const Outpost::EntityId third = empire.Ship(Outpost::ShipRole::Constructor, {140.0f, 100.0f});
    Outpost::Deputy deputy(RepositorySettings(), TICKS_PER_SECOND);
    (void)deputy.Play(empire.Now());
    deputy.Watch(empire.After(1.0));
    empire.Remove(second);
    empire.Remove(third);
    deputy.Watch(empire.After(1.0));
    Assert::IsTrue(OrdersOf<Outpost::QueueShipCommand>(deputy.Play(empire.After(1.0))).empty(),
                   L"its player had one when it took the seat again");
  }
};
} // namespace OpponentTests