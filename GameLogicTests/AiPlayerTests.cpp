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
constexpr Outpost::WeaponId MISSILE_RACK{3};
constexpr Outpost::DriveId PULSE{3};
constexpr Outpost::WeaponId FLAK_BATTERY{4};
constexpr Outpost::WeaponId RAIL_CANNON{5};

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
    m_snapshot.drives = {
      {.id = Outpost::DriveId{1}, .available = true}, {.id = Outpost::DriveId{2}, .available = false}, {.id = PULSE, .available = false}};
    m_snapshot.weapons = {{.id = Outpost::WeaponId{1}, .available = true},
                          {.id = Outpost::WeaponId{2}, .available = true},
                          {.id = MISSILE_RACK, .available = false},
                          {.id = FLAK_BATTERY, .available = false},
                          {.id = RAIL_CANNON, .available = false}};
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

  FleetSnapshot& Unlock(Outpost::WeaponId _weapon)
  {
    std::ranges::find(m_snapshot.weapons, _weapon, &Outpost::WeaponView::id)->available = true;
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
  explicit AiMatch(std::uint64_t _seed = 3, std::optional<Outpost::Map> _map = std::nullopt,
                   std::optional<Outpost::AiSettings> _settings = std::nullopt)
    : m_map(_map.has_value() ? std::move(*_map) : Outpost::LoadMap(ReadRepositoryMap())),
      m_server(Outpost::LoadTuning(ReadRepositoryTuning()), m_map, {.seed = _seed}),
      m_human(m_server.Connect(HUMAN)),
      m_aiConnection(m_server.Connect(AI)),
      m_ai(_settings.has_value() ? std::move(*_settings) : RepositorySettings(), m_server.TicksPerSecond())
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
// The repository's settings with the attack's numbers fixed, so that a test of how the AI attacks does not move with
// their tuning (task 12.2): a group of 12 that falls back after losing half and regroups for 30 seconds.
Outpost::AiSettings AttackSettings()
{
  Outpost::AiSettings settings = RepositorySettings();
  settings.attackGroupShips = 12;
  settings.attackGroupGrowthPerTier = 0;
  settings.retreatLossShare = 0.5;
  settings.regroupSeconds = 30.0;
  return settings;
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

  // Owner, 2026-10-01, and design §7: once the AI has the Large hull and the Fusion drive, it answers the swarm with
  // Large+Fusion+Mass Driver and the brawler with Large+Fusion+Lance. The rest of the triangle stays as it was.
  TEST_METHOD(AnswersWithHeaviesOnceUnlocked)
  {
    const Outpost::AiSettings settings = RepositorySettings();
    const Outpost::DesignComponents heavyMassDriver{Outpost::HullId{3}, Outpost::DriveId{2}, Outpost::WeaponId{1}};
    const auto unlocked = [](const Outpost::DesignComponents& _design, int _ships)
    { return FleetSnapshot().Add(HUMAN, _design, _ships).Unlock(Outpost::HullId{3}, Outpost::DriveId{2}); };
    Assert::IsTrue(Outpost::ChooseAnswer(settings, unlocked(SWARM, 5).Get()) == heavyMassDriver);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, unlocked(BRAWLER, 5).Get()) == HEAVY_LANCE);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, unlocked(LINE, 5).Get()) == SWARM);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, unlocked(HEAVY_LANCE, 5).Get()) == PICKET);
  }

  // Task 10.4, from the balance check of task 10.3: the Flak Battery answers the swarm, the brawler, the picket and the
  // Pulse raiders once unlocked; the Rail Cannon answers the heavies; missiles answer the Small Flak Battery, and the
  // picket the Rail Cannon. Before an answer is unlocked, the MVP's answer or the default stands.
  TEST_METHOD(AnswersThePhaseOneDesigns)
  {
    const Outpost::AiSettings settings = RepositorySettings();
    const Outpost::DesignComponents mediumFlak{Outpost::HullId{2}, Outpost::DriveId{1}, FLAK_BATTERY};
    const Outpost::DesignComponents smallFlak{Outpost::HullId{1}, Outpost::DriveId{1}, FLAK_BATTERY};
    const Outpost::DesignComponents mediumMissiles{Outpost::HullId{2}, Outpost::DriveId{1}, MISSILE_RACK};
    const Outpost::DesignComponents heavyRail{Outpost::HullId{3}, Outpost::DriveId{2}, RAIL_CANNON};
    const Outpost::DesignComponents pulsePicket{Outpost::HullId{1}, PULSE, Outpost::WeaponId{2}};

    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, SWARM, 5).Unlock(FLAK_BATTERY).Get()) == mediumFlak);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, PICKET, 5).Unlock(FLAK_BATTERY).Get()) == mediumFlak);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, pulsePicket, 5).Unlock(FLAK_BATTERY).Get()) == mediumFlak);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, pulsePicket, 5).Get()) == BRAWLER, L"the default");
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, smallFlak, 5).Unlock(MISSILE_RACK).Get()) == mediumMissiles);
    Assert::IsTrue(Outpost::ChooseAnswer(settings, FleetSnapshot().Add(HUMAN, heavyRail, 5).Get()) == PICKET);
    Assert::IsTrue(
      Outpost::ChooseAnswer(
        settings, FleetSnapshot().Add(HUMAN, HEAVY_LANCE, 5).Unlock(Outpost::HullId{3}, Outpost::DriveId{2}).Unlock(RAIL_CANNON).Get()) ==
      heavyRail);
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

  // Plan task 6.1's acceptance, and the owner's of 2026-10-01: a shot on any of its structures, built or a site, sends the
  // reserve there, and once the shooting has stopped for the settings' ten seconds, back to where it gathers.
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

    // Its Command Station under fire.
    const auto station = std::ranges::find_if(snapshot.entities, [](const Outpost::EntityView& _entity)
                                              { return _entity.owner == AI && _entity.kind == Outpost::EntityKind::Structure; });
    snapshot.tick += 20;
    snapshot.shots.push_back({.shooter = Outpost::EntityId{999}, .target = station->id, .weapon = Outpost::WeaponId{2}});
    const std::vector<Outpost::AttackMoveCommand> home = OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot));
    Assert::AreEqual(size_t{1}, home.size());
    Assert::IsTrue(std::ranges::is_permutation(home.front().ships, reserve));
    Assert::AreEqual(0.0f, Outpost::Distance(home.front().destination, station->position), 0.01f);

    // A Research Lab of its, still a site, under fire elsewhere.
    const Outpost::EntityView site{.id = Outpost::EntityId{5000},
                                   .kind = Outpost::EntityKind::Structure,
                                   .owner = AI,
                                   .structure = Outpost::StructureKind::ResearchLab,
                                   .position = {.xMeters = 200.0f, .zMeters = 400.0f},
                                   .radiusMeters = 30.0f,
                                   .builtPermille = 200};
    snapshot.entities.push_back(site);
    snapshot.shots = {{.shooter = Outpost::EntityId{999}, .target = site.id, .weapon = Outpost::WeaponId{2}}};
    snapshot.tick += 20;
    const std::vector<Outpost::AttackMoveCommand> defend = OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot));
    Assert::AreEqual(size_t{1}, defend.size());
    Assert::IsTrue(std::ranges::is_permutation(defend.front().ships, reserve));
    Assert::AreEqual(0.0f, Outpost::Distance(defend.front().destination, site.position), 0.01f);

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

  // Owner, 2026-10-01: the AI builds its N-th Shipyard once its income reaches N times 10 Ore/s, so that its Shipyards
  // spend about what its rigs earn. With all six rigs and Improved Extraction it earns 48.75 Ore/s, and has 4. Without the
  // platforms task 12.2 adds for each Shipyard, which spend Ore first and so put the timings below later.
  TEST_METHOD(BuildsShipyardsByIncome)
  {
    Outpost::AiSettings settings = RepositorySettings();
    settings.homePlatformsPerShipyard = 0;
    AiMatch match(3, std::nullopt, settings);
    const auto shipyards = [&match]
    {
      const Outpost::Snapshot view = match.View(AI);
      const std::vector<const Outpost::EntityView*> structures = match.Structures(view, AI);
      return std::ranges::count_if(structures, [](const Outpost::EntityView* _structure)
                                   { return _structure->structure == Outpost::StructureKind::Shipyard; });
    };
    match.Run(60.0);
    Assert::AreEqual(1500, match.View(AI).oreIncomeHundredthsPerSecond, L"three home rigs at a minute");
    Assert::AreEqual(std::ptrdiff_t{1}, shipyards(), L"the first Shipyard is in the build order whatever the income");

    match.Run(140.0);
    // Phase 1 design §8's map: three home rigs at 5 Ore a second and three on the near ring at 6, raised by a quarter.
    Assert::AreEqual(4125, match.View(AI).oreIncomeHundredthsPerSecond, L"six rigs and Improved Extraction at 3:20");
    Assert::AreEqual(std::ptrdiff_t{4}, shipyards());
    match.Run(60.0);
    Assert::AreEqual(std::ptrdiff_t{4}, shipyards(), L"no fifth Shipyard below 50 Ore/s");
  }

  // Design §10: at its review the AI answers the enemy's fleet, and its Shipyards build the answer.
  TEST_METHOD(BuildsTheAnswerToTheEnemysFleet)
  {
    AiMatch match;
    // Two of the human's line, parked behind the AI's Command Station in the map's corner, 257 m and 278 m from it: inside
    // the station's sight, its gun's 250 m and the 50 m margin and their 14 m footprint, out of the gun's and the Lances'
    // reach, and away from where the AI builds (ADR-024).
    const Outpost::PlanePosition station = match.Start(AI);
    const float step = 310.0f / std::numbers::sqrt2_v<float>;
    (void)match.Spawn(HUMAN, LINE, 2, {station.xMeters + step, station.zMeters + step});
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

  // Under fog of war the AI answers only what it has seen (ADR-024): a fleet parked in the human's corner leaves it on its
  // default design, review after review.
  TEST_METHOD(AnswersOnlyAFleetItHasSeen)
  {
    AiMatch match;
    (void)match.Spawn(HUMAN, LINE, 6, {-850.0f, -600.0f});
    match.Run(1.0);
    Assert::IsTrue(match.Ai().ProductionDesign() == RepositorySettings().defaultDesign);
    match.Run(61.0);
    Assert::IsTrue(match.Ai().ProductionDesign() == RepositorySettings().defaultDesign);
  }

  // Gate G9, which task 12.2 raised to the settings' twenty (owner, 2026-10-03): the reserve waits until the attack group
  // has gathered, and then attacks the enemy's base.
  TEST_METHOD(AttacksOnceItsGroupHasGathered)
  {
    AiMatch match;
    const int groupShips = RepositorySettings().attackGroupShips;
    const Outpost::PlanePosition gathering{700.0f, 600.0f};
    std::vector<Outpost::EntityId> ships = match.Spawn(AI, BRAWLER, groupShips - 1, gathering);
    match.Run(5.0);
    Assert::AreEqual(size_t{0}, match.Ai().AttackGroupShips());
    const Outpost::PlanePosition enemy = match.Start(HUMAN);
    const float waiting = MeanDistance(match.View(AI), ships, enemy);
    Assert::IsTrue(waiting > 1500.0f, L"the reserve left before its group had gathered");

    const std::vector<Outpost::EntityId> last = match.Spawn(AI, BRAWLER, 1, gathering);
    ships.push_back(last.front());
    match.Run(10.0);
    Assert::AreEqual(static_cast<size_t>(groupShips), match.Ai().AttackGroupShips());
    Assert::IsTrue(MeanDistance(match.View(AI), ships, enemy) < waiting - 300.0f, L"the attack group is not on its way");
  }

  // Phase 1 design §4, §13: the attack group goes for the enemy's Shipyards first, then its Command Station, then
  // anything else, nearest first in each; it keeps its target until it is gone, or until a Shipyard comes to light.
  TEST_METHOD(AttacksProductionFirst)
  {
    AiMatch match;
    const std::vector<Outpost::EntityId> group = match.Spawn(AI, BRAWLER, 12, {700.0f, 600.0f});
    Outpost::Snapshot snapshot = match.View(AI);
    const auto enemy = [&snapshot](std::uint32_t _id, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
    {
      snapshot.entities.push_back({.id = Outpost::EntityId{_id},
                                   .kind = Outpost::EntityKind::Structure,
                                   .owner = HUMAN,
                                   .structure = _kind,
                                   .position = _position,
                                   .radiusMeters = 30.0f,
                                   .builtPermille = Outpost::PERMILLE});
      return _position;
    };
    // Nearest the group a Defence Platform, then the Command Station, and the Shipyard farthest.
    (void)enemy(9001, Outpost::StructureKind::DefensePlatform, {0.0f, 0.0f});
    const Outpost::PlanePosition station = enemy(9002, Outpost::StructureKind::CommandStation, {-700.0f, -700.0f});
    const Outpost::PlanePosition yard = enemy(9003, Outpost::StructureKind::Shipyard, {-1200.0f, -1000.0f});
    Outpost::AiPlayer ai(AttackSettings(), 20);
    const auto sentTo = [&ai, &snapshot, &group]() -> std::optional<Outpost::PlanePosition>
    {
      for (const Outpost::AttackMoveCommand& order : OrdersOf<Outpost::AttackMoveCommand>(ai.Update(snapshot)))
      {
        if (std::ranges::is_permutation(order.ships, group))
          return order.destination;
      }
      return std::nullopt;
    };

    std::optional<Outpost::PlanePosition> destination = sentTo();
    Assert::IsTrue(destination.has_value(), L"the twelve were not sent");
    Assert::AreEqual(0.0f, Outpost::Distance(destination.value_or(Outpost::PlanePosition{}), yard), 0.01f, L"not the Shipyard first");

    std::erase_if(snapshot.entities, [](const Outpost::EntityView& _entity) { return _entity.id == Outpost::EntityId{9003}; });
    snapshot.tick += 20;
    destination = sentTo();
    Assert::IsTrue(destination.has_value(), L"the group was not sent on once the Shipyard was gone");
    Assert::AreEqual(0.0f, Outpost::Distance(destination.value_or(Outpost::PlanePosition{}), station), 0.01f,
                     L"not the Command Station next");

    snapshot.tick += 20;
    Assert::IsFalse(sentTo().has_value(), L"the group was sent again with nothing changed");

    const Outpost::PlanePosition another = enemy(9004, Outpost::StructureKind::Shipyard, {-300.0f, -1100.0f});
    snapshot.tick += 20;
    destination = sentTo();
    Assert::IsTrue(destination.has_value(), L"a Shipyard came to light and the group stayed on the station");
    Assert::AreEqual(0.0f, Outpost::Distance(destination.value_or(Outpost::PlanePosition{}), another), 0.01f);
  }

  // Phase 1 design §4: once its Command Station has fallen, the AI plays on with its Shipyards, and queues no more
  // Constructors.
  TEST_METHOD(PlaysOnWithoutItsStation)
  {
    AiMatch match;
    Outpost::Snapshot snapshot = match.View(AI);
    Outpost::AiPlayer ai(RepositorySettings(), 20);
    (void)ai.Update(snapshot);

    std::erase_if(snapshot.entities,
                  [](const Outpost::EntityView& _entity) { return _entity.owner == AI && _entity.kind == Outpost::EntityKind::Structure; });
    const Outpost::PlanePosition home = match.Start(AI);
    snapshot.entities.push_back({.id = Outpost::EntityId{9001},
                                 .kind = Outpost::EntityKind::Structure,
                                 .owner = AI,
                                 .structure = Outpost::StructureKind::Shipyard,
                                 .position = {home.xMeters + 150.0f, home.zMeters + 150.0f},
                                 .radiusMeters = 40.0f,
                                 .builtPermille = Outpost::PERMILLE});
    snapshot.tick += 20;
    const std::vector<Outpost::QueueShipCommand> queued = OrdersOf<Outpost::QueueShipCommand>(ai.Update(snapshot));
    Assert::IsFalse(queued.empty(), L"its Shipyard was given no work");
    Assert::IsTrue(std::ranges::all_of(queued, [](const Outpost::QueueShipCommand& _queue)
                                       { return _queue.producer == Outpost::EntityId{9001} && _queue.design.IsValid(); }),
                   L"something other than a warship was queued");
  }

  // Task 12.2: an attack group that has lost half the ships it set out with falls back, and the reserve waits the
  // settings' 30 seconds before it attacks again, even with enough ships.
  TEST_METHOD(FallsBackAfterLosingHalfAndRegroups)
  {
    AiMatch match;
    const std::vector<Outpost::EntityId> group = match.Spawn(AI, BRAWLER, 12, {700.0f, 600.0f});
    Outpost::Snapshot snapshot = match.View(AI);
    Outpost::AiPlayer ai(AttackSettings(), 20);
    (void)ai.Update(snapshot);
    Assert::AreEqual(size_t{12}, ai.AttackGroupShips());

    // Six of them destroyed.
    std::erase_if(snapshot.entities, [&group](const Outpost::EntityView& _entity)
                  { return std::ranges::find(group.begin(), group.begin() + 6, _entity.id) != group.begin() + 6; });
    snapshot.tick += 20;
    const std::vector<Outpost::MoveCommand> moves = OrdersOf<Outpost::MoveCommand>(ai.Update(snapshot));
    Assert::AreEqual(size_t{1}, moves.size(), L"the group did not fall back");
    Assert::AreEqual(size_t{6}, moves.front().ships.size());
    Assert::AreEqual(size_t{0}, ai.AttackGroupShips());

    // Six more join the reserve: twelve, but it regroups first.
    for (std::uint32_t i = 0; i < 6; ++i)
    {
      snapshot.entities.push_back({.id = Outpost::EntityId{9100 + i},
                                   .kind = Outpost::EntityKind::Ship,
                                   .owner = AI,
                                   .hull = BRAWLER.hull,
                                   .drive = BRAWLER.drive,
                                   .weapon = BRAWLER.weapon,
                                   .role = Outpost::ShipRole::Warship,
                                   .position = {650.0f, 550.0f}});
    }
    snapshot.tick += 20;
    (void)ai.Update(snapshot);
    Assert::AreEqual(size_t{0}, ai.AttackGroupShips(), L"it attacked again before it regrouped");
    snapshot.tick += std::uint64_t{30} * 20;
    (void)ai.Update(snapshot);
    Assert::AreEqual(size_t{12}, ai.AttackGroupShips(), L"it did not attack again once it had regrouped");
  }

  // Task 12.2: the attack group grows with each tier the AI has opened.
  TEST_METHOD(GrowsItsAttackGroupWithEachTier)
  {
    AiMatch match;
    (void)match.Spawn(AI, BRAWLER, 20, {700.0f, 600.0f});
    Outpost::Snapshot snapshot = match.View(AI);
    for (Outpost::ResearchTopicView& topic : snapshot.research)
    {
      if (topic.gateway && topic.tier == 2)
        topic.researched = true;
    }
    Outpost::AiSettings settings = AttackSettings();
    settings.attackGroupGrowthPerTier = 12;
    Outpost::AiPlayer ai(settings, 20);
    (void)ai.Update(snapshot);
    Assert::AreEqual(size_t{0}, ai.AttackGroupShips(), L"twenty attacked, where tier 2 asks for twenty-four");
    (void)match.Spawn(AI, BRAWLER, 4, {700.0f, 650.0f});
    Outpost::Snapshot more = match.View(AI);
    more.research = snapshot.research;
    more.tick = snapshot.tick + 20;
    (void)ai.Update(more);
    Assert::AreEqual(size_t{24}, ai.AttackGroupShips());
  }

  // Task 12.2: the AI fortifies its base with the settings' Defence Platforms for each Shipyard, toward the map's center.
  TEST_METHOD(FortifiesItsBaseForEachShipyard)
  {
    Outpost::AiSettings settings = RepositorySettings();
    settings.homePlatformsPerShipyard = 2;
    AiMatch match(3, std::nullopt, settings);
    match.Run(5.0 * 60.0);
    const Outpost::Snapshot view = match.View(AI);
    const Outpost::PlanePosition home = match.Start(AI);
    std::ptrdiff_t shipyards = 0;
    std::ptrdiff_t homePlatforms = 0;
    for (const Outpost::EntityView* structure : match.Structures(view, AI))
    {
      if (structure->structure == Outpost::StructureKind::Shipyard)
        ++shipyards;
      if (structure->structure == Outpost::StructureKind::DefensePlatform && Outpost::Distance(structure->position, home) < 500.0f &&
          structure->builtPermille >= Outpost::PERMILLE)
        ++homePlatforms;
    }
    Assert::IsTrue(shipyards >= 2, L"fewer Shipyards than five minutes bring");
    // The one beside the Command Station, and two for each Shipyard.
    Assert::IsTrue(homePlatforms >= 1 + (2 * shipyards) - 2, L"the base is not fortified as its Shipyards grow");
  }

  // A structure under fire draws the reserve to it, and the reserve goes back once the shooting has stopped.
  TEST_METHOD(DefendsAStructureUnderFire)
  {
    AiMatch match;
    const Outpost::PlanePosition home = match.Start(AI);
    const std::vector<Outpost::EntityId> reserve = match.Spawn(AI, BRAWLER, 4, {home.xMeters - 50.0f, home.zMeters - 150.0f});
    match.Run(3.0);
    // An outpost of the AI's, far from its base, and a raider beside it.
    const Outpost::PlanePosition outpost{home.xMeters - 650.0f, home.zMeters - 600.0f};
    const Outpost::EntityId platform =
      match.World().SpawnStructure(AI, Outpost::StructureKind::DefensePlatform, outpost, 20.0f, 150000, 1000);
    const std::vector<Outpost::EntityId> raider = match.Spawn(HUMAN, LINE, 1, {outpost.xMeters, outpost.zMeters + 200.0f});
    match.Human().Send({.player = HUMAN, .order = Outpost::AttackCommand{.ships = raider, .target = platform}});
    const float before = MeanDistance(match.View(AI), reserve, outpost);
    match.Run(12.0);
    const float during = MeanDistance(match.View(AI), reserve, outpost);
    Assert::IsTrue(during < before - 400.0f, L"the reserve did not go to the outpost's defence");
  }

  // The whole AI against a player who does nothing: it builds, gathers, attacks and wins.
  // Task 11.3, Phase 1 design §13: once its home asteroids run dry the AI builds on the nearest asteroids with ore left,
  // each with a Defence Platform beside it, and keeps the dry rigs for their trickle.
  TEST_METHOD(FollowsTheOre)
  {
    Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    for (Outpost::OreAsteroidPlacement& asteroid : map.oreAsteroids)
    {
      if (asteroid.yield == Outpost::OreYield::Home)
        asteroid.reserveOre = 200;
    }
    AiMatch match(3, map);
    match.Run(8.0 * 60.0);
    const Outpost::Snapshot view = match.View(AI);
    std::ptrdiff_t dry = 0;
    std::ptrdiff_t mining = 0;
    std::ptrdiff_t platforms = 0;
    for (const Outpost::EntityView* structure : match.Structures(view, AI))
    {
      if (structure->structure == Outpost::StructureKind::DefensePlatform)
        ++platforms;
      if (structure->structure != Outpost::StructureKind::MiningRig || structure->builtPermille < Outpost::PERMILLE)
        continue;
      ++(structure->oreReserveHundredths.value_or(1) == 0 ? dry : mining);
    }
    Assert::AreEqual(std::ptrdiff_t{3}, dry, L"the home rigs, run dry, are kept");
    Assert::AreEqual(std::ptrdiff_t{6}, mining, L"six rigs on asteroids with ore");
    Assert::IsTrue(platforms >= 6, L"a platform beside each rig away from home, and one by the base");
  }

  // Task 10.4: against a player who does nothing, the AI researches through tier 2 and opens tier 3 (Phase 1 design §13).
  TEST_METHOD(ReachesTierThree)
  {
    AiMatch match;
    constexpr Outpost::ResearchTopicId PRECURSOR_VAULT{18};
    const auto researched = [&match](Outpost::ResearchTopicId _topic)
    {
      const Outpost::Snapshot view = match.View(AI);
      const auto topic = std::ranges::find(view.research, _topic, &Outpost::ResearchTopicView::id);
      return topic != view.research.end() && topic->researched;
    };
    for (int minute = 0; minute < 45 && !researched(PRECURSOR_VAULT); ++minute)
      match.Run(60.0);
    Assert::IsTrue(researched(PRECURSOR_VAULT), L"the AI has not opened tier 3 in 45 minutes");
    Logger::WriteMessage(std::format("The AI opened tier 3 at tick {}.\n", match.World().CurrentTick()).c_str());
  }

  TEST_METHOD(BeatsAPlayerWhoDoesNothing)
  {
    AiMatch match;
    match.Run(25.0 * 60.0, true);
    Assert::IsTrue(match.World().MatchOver(), L"the AI has not won in 25 minutes");
    Assert::IsTrue(match.World().Winner() == AI);
    Logger::WriteMessage(std::format("The AI won at tick {}.\n", match.View(AI).matchEndedTick).c_str());
  }

  // Under fog of war the AI cannot see that the human already holds the contested asteroids its plan wants (ADR-024). The
  // server refuses those rigs, the AI takes a site it does not see appear as refused, and it still builds, attacks and
  // wins.
  TEST_METHOD(BeatsAPlayerWhoHoldsTheMiddle)
  {
    AiMatch match;
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::StructureTuning& rig =
      *std::ranges::find(tuning.structures, Outpost::StructureKind::MiningRig, &Outpost::StructureTuning::kind);
    for (const Outpost::OreAsteroidPlacement& asteroid : Outpost::LoadMap(ReadRepositoryMap()).oreAsteroids)
    {
      if (asteroid.yield != Outpost::OreYield::Contested)
        continue;
      (void)match.World().SpawnStructure(HUMAN, Outpost::StructureKind::MiningRig, asteroid.position,
                                         std::max(static_cast<float>(rig.footprintRadiusMeters), asteroid.radiusMeters),
                                         rig.hitPoints * Outpost::HUNDREDTHS, rig.armor * Outpost::HUNDREDTHS);
    }
    match.Run(25.0 * 60.0, true);
    Assert::IsTrue(match.World().MatchOver(), L"the AI has not won in 25 minutes");
    Assert::IsTrue(match.World().Winner() == AI);
    Logger::WriteMessage(std::format("The AI won at tick {}.\n", match.View(AI).matchEndedTick).c_str());
  }
};
} // namespace GameLogicTests
