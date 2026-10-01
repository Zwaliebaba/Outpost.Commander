#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId HUMAN{1};
constexpr Outpost::PlayerId AI{2};
constexpr Outpost::DesignComponents SWARM{Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1}};
constexpr Outpost::DesignComponents BRAWLER{Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{1}};
constexpr Outpost::DesignComponents LINE{Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{2}};
constexpr Outpost::DesignComponents PICKET{Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{2}};
constexpr Outpost::DesignComponents HEAVY_LANCE{Outpost::HullId{3}, Outpost::DriveId{2}, Outpost::WeaponId{2}};

Outpost::AiSettings RepositorySettings()
{
  return Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
}

// A snapshot of the AI's player with the starting components unlocked, as a match begins, and a fleet.
class FleetSnapshot
{
public:
  FleetSnapshot()
  {
    m_snapshot.player = AI;
    m_snapshot.hulls = {{.id = Outpost::HullId{1}, .available = true},
                        {.id = Outpost::HullId{2}, .available = true},
                        {.id = Outpost::HullId{3}, .available = false}};
    m_snapshot.drives = {{.id = Outpost::DriveId{1}, .available = true}, {.id = Outpost::DriveId{2}, .available = false}};
    m_snapshot.weapons = {{.id = Outpost::WeaponId{1}, .available = true},
                          {.id = Outpost::WeaponId{2}, .available = true},
                          {.id = Outpost::WeaponId{3}, .available = false}};
  }

  FleetSnapshot& Add(Outpost::PlayerId _owner, const Outpost::DesignComponents& _design, int _ships)
  {
    for (int i = 0; i < _ships; ++i)
    {
      m_snapshot.entities.push_back({.id = Outpost::EntityId{static_cast<std::uint32_t>(m_snapshot.entities.size() + 1)},
                                     .kind = Outpost::EntityKind::Ship,
                                     .owner = _owner,
                                     .hull = _design.hull,
                                     .drive = _design.drive,
                                     .weapon = _design.weapon,
                                     .role = Outpost::ShipRole::Warship});
    }
    return *this;
  }

  FleetSnapshot& Unlock(Outpost::HullId _hull, Outpost::DriveId _drive)
  {
    std::ranges::find(m_snapshot.hulls, _hull, &Outpost::HullView::id)->available = true;
    std::ranges::find(m_snapshot.drives, _drive, &Outpost::DriveView::id)->available = true;
    return *this;
  }

  [[nodiscard]] const Outpost::Snapshot& Get() const noexcept
  {
    return m_snapshot;
  }

private:
  Outpost::Snapshot m_snapshot;
};

// A match on the repository's map and tuning data: the AI plays player 2 through its own connection, and the human
// player 1 does nothing unless a test orders it.
class AiMatch
{
public:
  explicit AiMatch(std::uint64_t _seed = 3)
    : m_map(Outpost::LoadMap(ReadRepositoryMap())),
      m_server(Outpost::LoadTuning(ReadRepositoryTuning()), m_map, {.seed = _seed}),
      m_human(m_server.Connect(HUMAN)),
      m_aiConnection(m_server.Connect(AI)),
      m_ai(RepositorySettings(), m_server.TicksPerSecond())
  {
    m_server.World().PlaceStartingBases(m_map);
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_server.World();
  }

  [[nodiscard]] Outpost::AiPlayer& Ai() noexcept
  {
    return m_ai;
  }

  [[nodiscard]] Outpost::Transport& Human() noexcept
  {
    return *m_human;
  }

  [[nodiscard]] Outpost::Snapshot View(Outpost::PlayerId _player)
  {
    return m_server.World().BuildSnapshot(_player);
  }

  [[nodiscard]] Outpost::PlanePosition Start(Outpost::PlayerId _player) const
  {
    return m_map.starts[_player.value - 1];
  }

  // Runs _seconds of the match, or until it is over when _untilOver.
  void Run(double _seconds, bool _untilOver = false)
  {
    const std::uint32_t tps = m_server.TicksPerSecond();
    const auto ticks = static_cast<std::uint64_t>(_seconds * tps);
    for (std::uint64_t tick = 0; tick < ticks; ++tick)
    {
      if (_untilOver && World().MatchOver())
        return;
      m_server.Advance(std::chrono::nanoseconds(1'000'000'000 / tps));
      (void)m_human->Receive();
      for (const Outpost::Snapshot& snapshot : m_aiConnection->Receive())
      {
        for (Outpost::Command& command : m_ai.Update(snapshot))
          m_aiConnection->Send(std::move(command));
      }
    }
  }

  // Warships of _design for _owner in a grid around _center, 30 m apart.
  std::vector<Outpost::EntityId> Spawn(Outpost::PlayerId _owner, const Outpost::DesignComponents& _design, int _ships,
                                       Outpost::PlanePosition _center)
  {
    const Outpost::DesignId design = World().FindDesign(_owner, _design)->id;
    std::vector<Outpost::EntityId> ships;
    for (int i = 0; i < _ships; ++i)
    {
      const int rowIndex = i / 4;
      const float column = static_cast<float>(i % 4) - 1.5f;
      const auto row = static_cast<float>(rowIndex - 1);
      ships.push_back(World().SpawnShip(_owner, design, {_center.xMeters + (30.0f * column), _center.zMeters + (30.0f * row)}));
    }
    return ships;
  }

  [[nodiscard]] std::vector<const Outpost::EntityView*> Structures(const Outpost::Snapshot& _snapshot, Outpost::PlayerId _owner) const
  {
    std::vector<const Outpost::EntityView*> structures;
    for (const Outpost::EntityView& entity : _snapshot.entities)
    {
      if (entity.kind == Outpost::EntityKind::Structure && entity.owner == _owner)
        structures.push_back(&entity);
    }
    return structures;
  }

private:
  Outpost::Map m_map;
  Outpost::InProcessServer m_server;
  std::unique_ptr<Outpost::Transport> m_human;
  std::unique_ptr<Outpost::Transport> m_aiConnection;
  Outpost::AiPlayer m_ai;
};

float MeanDistance(const Outpost::Snapshot& _snapshot, const std::vector<Outpost::EntityId>& _ships, Outpost::PlanePosition _to)
{
  float sum = 0.0f;
  size_t alive = 0;
  for (const Outpost::EntityId id : _ships)
  {
    const auto ship = std::ranges::find(_snapshot.entities, id, &Outpost::EntityView::id);
    if (ship == _snapshot.entities.end())
      continue;
    sum += Outpost::Distance(ship->position, _to);
    ++alive;
  }
  return alive > 0 ? sum / static_cast<float>(alive) : 0.0f;
}
// The orders of one kind among _commands.
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

TEST_CLASS(AiPlayerTests)
{
public:
  // Owner, 2026-10-01: design §7's triangle. The swarm is answered with the brawler, the brawler with the line, the line
  // and the picket with the swarm.
  TEST_METHOD(AnswersTheTriangle)
  {
    const Outpost::AiSettings settings = RepositorySettings();
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, SWARM, 5).Get()) == BRAWLER);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, BRAWLER, 5).Get()) == LINE);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, LINE, 5).Get()) == SWARM);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, PICKET, 5).Get()) == SWARM);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, HEAVY_LANCE, 5).Get()) == PICKET);
  }

  // The enemy's most common design is the one answered; its own ships are not the enemy's.
  TEST_METHOD(AnswersTheMostCommonEnemyDesign)
  {
    const Outpost::AiSettings settings = RepositorySettings();
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, SWARM, 2).Add(HUMAN, LINE, 3).Get()) == SWARM);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, SWARM, 2).Add(AI, LINE, 9).Get()) == BRAWLER);
    // A tie goes to the lowest hull: the swarm's, answered with the brawler.
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, LINE, 3).Add(HUMAN, SWARM, 3).Get()) == BRAWLER);
  }

  // No enemy fleet, a design the counters do not name, or an answer the AI has not unlocked: the default design.
  TEST_METHOD(FallsBackToTheDefault)
  {
    Outpost::AiSettings settings = RepositorySettings();
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Get()) == BRAWLER);
    const Outpost::DesignComponents unnamed{Outpost::HullId{2}, Outpost::DriveId{2}, Outpost::WeaponId{3}};
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, unnamed, 4).Get()) == BRAWLER);

    settings.counters.front().answer = HEAVY_LANCE;
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, SWARM, 4).Get()) == BRAWLER);
    Assert::IsTrue(Outpost::ChooseAnswer(
                     settings, FleetSnapshot().Add(HUMAN, SWARM, 4).Unlock(Outpost::HullId{3}, Outpost::DriveId{2}).Get()) == HEAVY_LANCE);
  }

  // Plan task 6.1's acceptance, from the match's first snapshot: a Constructor queued, and both Constructors sent to put
  // a rig on the home asteroid nearest the Command Station.
  TEST_METHOD(OrdersARigAndAConstructorFirst)
  {
    AiMatch match;
    const Outpost::Snapshot first = match.View(AI);
    Outpost::AiPlayer ai(RepositorySettings(), 20);
    const std::vector<Outpost::Command> commands = ai.Update(first);
    Assert::IsTrue(std::ranges::all_of(commands, [](const Outpost::Command& _command) { return _command.player == AI; }));

    const auto station = std::ranges::find_if(first.entities,
                                              [](const Outpost::EntityView& _entity)
                                              {
                                                return _entity.owner == AI && _entity.structure == Outpost::StructureKind::CommandStation &&
                                                       _entity.kind == Outpost::EntityKind::Structure;
                                              });
    const std::vector<Outpost::QueueShipCommand> queued = OrdersOf<Outpost::QueueShipCommand>(commands);
    Assert::AreEqual(size_t{1}, queued.size());
    Assert::IsTrue(queued.front().producer == station->id);

    const Outpost::EntityView* nearest = nullptr;
    for (const Outpost::EntityView& entity : first.entities)
    {
      if (entity.kind == Outpost::EntityKind::Asteroid && (nearest == nullptr || Outpost::Distance(entity.position, station->position) <
                                                                                   Outpost::Distance(nearest->position, station->position)))
        nearest = &entity;
    }
    const std::vector<Outpost::BuildStructureCommand> builds = OrdersOf<Outpost::BuildStructureCommand>(commands);
    Assert::AreEqual(size_t{1}, builds.size());
    Assert::IsTrue(builds.front().structure == Outpost::StructureKind::MiningRig);
    Assert::AreEqual(0.0f, Outpost::Distance(builds.front().position, nearest->position), 0.01f);
    Assert::AreEqual(size_t{2}, builds.front().constructors.size());
    Assert::AreEqual(size_t{2}, commands.size(), L"nothing else: no lab to research in, no warship to order");
  }

  // Plan task 6.1's acceptance and design §10: a shot on one of its rigs or platforms sends the reserve there, and once
  // the shooting has stopped for the settings' ten seconds, back to where it gathers. A shot on its Command Station does
  // not: the station's gun is its own defence.
  TEST_METHOD(SendsTheReserveWhereItsBaseIsShot)
  {
    AiMatch match;
    const std::vector<Outpost::EntityId> reserve = match.Spawn(AI, BRAWLER, 3, {700.0f, 600.0f});
    Outpost::Snapshot snapshot = match.View(AI);
    Outpost::AiPlayer ai(RepositorySettings(), 20);
    const std::vector<Outpost::AttackMoveCommand> gather = OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot));
    Assert::AreEqual(size_t{1}, gather.size());
    Assert::IsTrue(std::ranges::is_permutation(gather.front().ships, reserve));
    const Outpost::PlanePosition rally = gather.front().destination;

    const auto station = std::ranges::find_if(snapshot.entities, [](const Outpost::EntityView& _entity)
                                              { return _entity.owner == AI && _entity.kind == Outpost::EntityKind::Structure; });
    snapshot.tick += 20;
    snapshot.shots.push_back({.shooter = Outpost::EntityId{999}, .target = station->id, .weapon = Outpost::WeaponId{2}});
    Assert::IsTrue(OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot)).empty(), L"the reserve left for the Command Station");

    // A platform of its, as the snapshot shows it, under fire.
    const Outpost::EntityView platform{.id = Outpost::EntityId{5000},
                                       .kind = Outpost::EntityKind::Structure,
                                       .owner = AI,
                                       .structure = Outpost::StructureKind::DefensePlatform,
                                       .position = {.xMeters = 200.0f, .zMeters = 400.0f},
                                       .radiusMeters = 20.0f};
    snapshot.entities.push_back(platform);
    snapshot.shots = {{.shooter = Outpost::EntityId{999}, .target = platform.id, .weapon = Outpost::WeaponId{2}}};
    snapshot.tick += 20;
    const std::vector<Outpost::AttackMoveCommand> defend = OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot));
    Assert::AreEqual(size_t{1}, defend.size());
    Assert::IsTrue(std::ranges::is_permutation(defend.front().ships, reserve));
    Assert::AreEqual(0.0f, Outpost::Distance(defend.front().destination, platform.position), 0.01f);

    snapshot.shots.clear();
    // Five seconds later, then eleven, at twenty ticks a second.
    snapshot.tick += 100;
    Assert::IsTrue(OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot)).empty(), L"it stays while the attack may go on");
    snapshot.tick += 120;
    const std::vector<Outpost::AttackMoveCommand> back = OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot));
    Assert::AreEqual(size_t{1}, back.size());
    Assert::AreEqual(0.0f, Outpost::Distance(back.front().destination, rally), 0.01f);
  }

  // Owner, 2026-10-01: rigs on the three home asteroids come first, then the Shipyard, the Research Lab and the Defence
  // Platform beside the Command Station; it keeps four Constructors and researches in its order.
  TEST_METHOD(BuildsItsBaseInOrder)
  {
    AiMatch match;
    match.Run(240.0);
    const Outpost::Snapshot view = match.View(AI);
    std::vector<const Outpost::EntityView*> built = match.Structures(view, AI);
    std::erase_if(built, [](const Outpost::EntityView* _entity) { return _entity->structure == Outpost::StructureKind::CommandStation; });
    std::ranges::sort(built, {}, &Outpost::EntityView::id);
    Assert::IsTrue(built.size() >= 6, L"the home base is not placed after four minutes");

    std::vector<const Outpost::EntityView*> asteroids;
    for (const Outpost::EntityView& entity : view.entities)
    {
      if (entity.kind == Outpost::EntityKind::Asteroid)
        asteroids.push_back(&entity);
    }
    const Outpost::PlanePosition home = match.Start(AI);
    std::ranges::sort(asteroids, {}, [home](const Outpost::EntityView* _asteroid) { return Outpost::Distance(_asteroid->position, home); });
    for (size_t i = 0; i < 3; ++i)
    {
      Assert::IsTrue(built[i]->structure == Outpost::StructureKind::MiningRig);
      const auto onHomeAsteroid = [&](const Outpost::EntityView* _asteroid)
      { return Outpost::Distance(_asteroid->position, built[i]->position) < 1.0f; };
      Assert::IsTrue(std::any_of(asteroids.begin(), asteroids.begin() + 3, onHomeAsteroid), L"a first rig is not on a home asteroid");
    }
    Assert::IsTrue(built[3]->structure == Outpost::StructureKind::Shipyard);
    Assert::IsTrue(built[4]->structure == Outpost::StructureKind::ResearchLab);
    Assert::IsTrue(built[5]->structure == Outpost::StructureKind::DefensePlatform);
    for (size_t i = 3; i < 6; ++i)
      Assert::IsTrue(Outpost::Distance(built[i]->position, home) < 250.0f, L"a home structure is not beside the Command Station");

    const auto constructors = std::ranges::count_if(
      view.entities, [](const Outpost::EntityView& _entity)
      { return _entity.owner == AI && _entity.role == Outpost::ShipRole::Constructor && _entity.kind == Outpost::EntityKind::Ship; });
    Assert::AreEqual(std::ptrdiff_t{4}, constructors);
    const auto extraction = std::ranges::find(view.research, Outpost::ResearchTopicId{1}, &Outpost::ResearchTopicView::id);
    Assert::IsTrue(extraction->researched, L"the first topic in its order is not done after four minutes");
  }

  // Design §10: at its review the AI answers the enemy's fleet, and its Shipyards build the answer.
  TEST_METHOD(BuildsTheAnswerToTheEnemysFleet)
  {
    AiMatch match;
    // The human's line, parked in its own corner.
    (void)match.Spawn(HUMAN, LINE, 6, {-850.0f, -600.0f});
    match.Run(1.0);
    Assert::IsTrue(match.Ai().ProductionDesign() == SWARM);

    const Outpost::DesignId swarm = match.World().FindDesign(AI, SWARM)->id;
    bool queued = false;
    for (int second = 0; second < 300 && !queued; ++second)
    {
      match.Run(1.0);
      const Outpost::Snapshot view = match.View(AI);
      for (const Outpost::EntityView* yard : match.Structures(view, AI))
      {
        if (yard->structure != Outpost::StructureKind::Shipyard || yard->queue.empty())
          continue;
        Assert::IsTrue(std::ranges::all_of(yard->queue, [swarm](const Outpost::JobView& _job) { return _job.design == swarm; }));
        queued = true;
      }
    }
    Assert::IsTrue(queued, L"no Shipyard queued a ship in five minutes");
  }

  // Gate G9: the reserve waits until twelve warships have gathered, and then attacks the enemy's base.
  TEST_METHOD(AttacksOnceTwelveHaveGathered)
  {
    AiMatch match;
    const Outpost::PlanePosition gathering{700.0f, 600.0f};
    std::vector<Outpost::EntityId> ships = match.Spawn(AI, BRAWLER, 11, gathering);
    match.Run(5.0);
    Assert::AreEqual(size_t{0}, match.Ai().AttackGroupShips());
    const Outpost::PlanePosition enemy = match.Start(HUMAN);
    const float waiting = MeanDistance(match.View(AI), ships, enemy);
    Assert::IsTrue(waiting > 1500.0f, L"the reserve left before it had twelve");

    const std::vector<Outpost::EntityId> twelfth = match.Spawn(AI, BRAWLER, 1, gathering);
    ships.push_back(twelfth.front());
    match.Run(10.0);
    Assert::AreEqual(size_t{12}, match.Ai().AttackGroupShips());
    Assert::IsTrue(MeanDistance(match.View(AI), ships, enemy) < waiting - 300.0f, L"the attack group is not on its way");
  }

  // A structure under fire draws the reserve to it, and the reserve goes back once the shooting has stopped.
  TEST_METHOD(DefendsAStructureUnderFire)
  {
    AiMatch match;
    const std::vector<Outpost::EntityId> reserve = match.Spawn(AI, BRAWLER, 4, {700.0f, 600.0f});
    match.Run(3.0);
    // An outpost of the AI's, far from its base, and a raider beside it.
    const Outpost::PlanePosition outpost{-100.0f, 700.0f};
    const Outpost::EntityId platform =
      match.World().SpawnStructure(AI, Outpost::StructureKind::DefensePlatform, outpost, 20.0f, 150000, 1000);
    const std::vector<Outpost::EntityId> raider = match.Spawn(HUMAN, LINE, 1, {-100.0f, 900.0f});
    match.Human().Send({.player = HUMAN, .order = Outpost::AttackCommand{.ships = raider, .target = platform}});
    const float before = MeanDistance(match.View(AI), reserve, outpost);
    match.Run(12.0);
    const float during = MeanDistance(match.View(AI), reserve, outpost);
    Assert::IsTrue(during < before - 400.0f, L"the reserve did not go to the outpost's defence");
  }

  // The whole AI against a player who does nothing: it builds, gathers, attacks and wins.
  TEST_METHOD(BeatsAPlayerWhoDoesNothing)
  {
    AiMatch match;
    match.Run(25.0 * 60.0, true);
    Assert::IsTrue(match.World().MatchOver(), L"the AI has not won in 25 minutes");
    Assert::IsTrue(match.World().Winner() == AI);
    Logger::WriteMessage(std::format("The AI won at tick {}.\n", match.View(AI).matchEndedTick).c_str());
  }
};
} // namespace GameLogicTests
