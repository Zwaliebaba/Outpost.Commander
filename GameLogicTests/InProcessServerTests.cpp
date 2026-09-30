#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr Outpost::DesignId SWARM{1};
constexpr Outpost::ShipMovement SMALL_ION{.speedMetersPerSecond = 78.0f, .turnRateRadiansPerSecond = 3.927f, .radiusMeters = 8.0f};

Outpost::Tuning RepositoryTuning()
{
  return Outpost::LoadTuning(ReadRepositoryTuning());
}

Outpost::Map RepositoryMap()
{
  return Outpost::LoadMap(ReadRepositoryMap());
}
} // namespace

TEST_CLASS(InProcessServerTests)
{
public:
  TEST_METHOD(TicksAtTheTunedRateAndSendsEachPlayerASnapshot)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
    Assert::AreEqual(20, server.TuningData().rules.tickHz);
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    const std::unique_ptr<Outpost::Transport> red = server.Connect(RED);

    server.Advance(49ms);
    Assert::IsTrue(blue->Receive().empty());

    server.Advance(1ms);
    const std::vector<Outpost::Snapshot> blueSnapshots = blue->Receive();
    Assert::AreEqual(size_t{1}, blueSnapshots.size());
    Assert::AreEqual(std::uint64_t{1}, blueSnapshots[0].tick);
    Assert::IsTrue(blueSnapshots[0].player == BLUE);

    server.Advance(100ms);
    const std::vector<Outpost::Snapshot> redSnapshots = red->Receive();
    Assert::AreEqual(size_t{3}, redSnapshots.size());
    Assert::AreEqual(std::uint64_t{3}, redSnapshots[2].tick);
    Assert::IsTrue(redSnapshots[2].player == RED);
  }

  TEST_METHOD(AppliesACommandAtTheNextTickAsTheConnectionsPlayer)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
    const Outpost::EntityId blueShip = server.World().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    const Outpost::EntityId redShip = server.World().SpawnShip(RED, SWARM, SMALL_ION, {});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);

    // The client claims to be red, but it is on blue's connection, so it may move only blue's ships.
    blue->Send({.player = RED, .order = Outpost::MoveCommand{.ships = {redShip}, .destination = {.xMeters = 5.0f}}});
    blue->Send({.player = RED, .order = Outpost::MoveCommand{.ships = {blueShip}, .destination = {.xMeters = 7.0f}}});
    server.Advance(49ms);
    Assert::IsFalse(server.World().FindEntity(blueShip)->destination.has_value());

    server.Advance(1ms);
    Assert::IsFalse(server.World().FindEntity(redShip)->destination.has_value());
    Assert::IsTrue(server.World().FindEntity(blueShip)->destination == Outpost::PlanePosition{.xMeters = 7.0f});

    const std::vector<Outpost::LoggedCommand>& log = server.CommandLog();
    Assert::AreEqual(size_t{2}, log.size());
    Assert::AreEqual(std::uint64_t{0}, log[0].tick);
    Assert::IsTrue(log[0].command.player == BLUE && log[1].command.player == BLUE);
  }

  // ADR-009: a match reproduces from its seed and the server's command log, on the same build.
  TEST_METHOD(ReplaysFromItsCommandLog)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 77});
    const Outpost::EntityId blueShip = server.World().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    const Outpost::EntityId redShip = server.World().SpawnShip(RED, SWARM, SMALL_ION, {});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    const std::unique_ptr<Outpost::Transport> red = server.Connect(RED);
    for (int frame = 0; frame < 120; ++frame)
    {
      if (frame % 5 == 0)
        blue->Send({.order = Outpost::MoveCommand{.ships = {blueShip}, .destination = {.xMeters = static_cast<float>(frame) * 2.0f}}});
      if (frame % 9 == 0)
        red->Send({.order = Outpost::MoveCommand{.ships = {redShip, blueShip}, .destination = {.zMeters = static_cast<float>(frame)}}});
      if (frame % 17 == 0)
        red->Send({.order = Outpost::StopCommand{.ships = {redShip}}});
      server.Advance(std::chrono::microseconds(16'667));
    }

    Outpost::Simulation replay(77, 20);
    replay.PlaceMap(server.MapData());
    (void)replay.SpawnShip(BLUE, SWARM, SMALL_ION, {});
    (void)replay.SpawnShip(RED, SWARM, SMALL_ION, {});
    const std::vector<Outpost::LoggedCommand>& log = server.CommandLog();
    size_t next = 0;
    while (replay.CurrentTick() < server.World().CurrentTick())
    {
      std::vector<Outpost::Command> commands;
      for (; next < log.size() && log[next].tick == replay.CurrentTick(); ++next)
        commands.push_back(log[next].command);
      (void)replay.Tick(commands);
    }
    Assert::AreEqual(log.size(), next);
    Assert::IsTrue(replay == server.World());
  }

  // Task 2.5: every player starts with the map's fleet around its start, facing the map's center, clear of obstacles
  // and of each other, and each ship's snapshot names its hull so that the client can draw it.
  TEST_METHOD(PlacesEachPlayersStartingFleet)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    const Outpost::Map map = RepositoryMap();
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    server.World().PlaceStartingFleets(map, tuning);
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    server.Advance(50ms);
    const std::vector<Outpost::Snapshot> snapshots = blue->Receive();
    Assert::AreEqual(size_t{1}, snapshots.size());

    std::uint32_t fleetSize = 0;
    for (const Outpost::StartingShips& group : map.startingFleet)
      fleetSize += group.count;

    std::vector<Outpost::EntityView> ships;
    for (const Outpost::EntityView& entity : snapshots[0].entities)
    {
      if (entity.kind == Outpost::EntityKind::Ship)
        ships.push_back(entity);
    }
    Assert::AreEqual(size_t{2} * fleetSize, ships.size());

    for (const Outpost::EntityView& ship : ships)
    {
      Assert::IsTrue(ship.hull.IsValid());
      const Outpost::PlanePosition start = map.starts[ship.owner.value - 1];
      Assert::IsTrue(Outpost::Distance(ship.position, start) < 100.0f);
      const float towardCenter = std::atan2(-start.zMeters, -start.xMeters);
      Assert::AreEqual(towardCenter, ship.headingRadians, 1e-5f);
      for (const Outpost::EntityView& other : ships)
      {
        if (other.id != ship.id)
          Assert::IsTrue(Outpost::Distance(ship.position, other.position) >= ship.radiusMeters + other.radiusMeters);
      }
    }
  }

  TEST_METHOD(RefusesAStartingFleetThatDoesNotFit)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    Outpost::Map map = RepositoryMap();
    map.startingFleet = {{.hull = Outpost::HullId{3}, .drive = Outpost::DriveId{1}, .count = 100}};
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    Assert::ExpectException<Neuron::Exception>([&] { server.World().PlaceStartingFleets(map, tuning); });
  }

  TEST_METHOD(RefusesAStartingFleetOfAnUnknownHull)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    Outpost::Map map = RepositoryMap();
    map.startingFleet = {{.hull = Outpost::HullId{99}, .drive = Outpost::DriveId{1}, .count = 1}};
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    Assert::ExpectException<Neuron::Exception>([&] { server.World().PlaceStartingFleets(map, tuning); });
  }

  TEST_METHOD(RefusesADuplicateOrMissingPlayer)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server.Connect(BLUE); });
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server.Connect(Outpost::PlayerId{}); });
  }

  TEST_METHOD(TransportOutlivesTheServer)
  {
    std::unique_ptr<Outpost::Transport> blue;
    {
      Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
      blue = server.Connect(BLUE);
      server.Advance(50ms);
    }
    blue->Send({.order = Outpost::StopCommand{}});
    Assert::AreEqual(size_t{1}, blue->Receive().size());
  }
};
} // namespace GameLogicTests
