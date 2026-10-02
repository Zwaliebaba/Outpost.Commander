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

    // The same match setup as the server's.
    Outpost::Simulation replay(77, 20);
    replay.PlaceMap(server.MapData());
    replay.UseTuning(server.TuningData());
    replay.UseFog();
    for (const Outpost::PlayerId player : {BLUE, RED})
    {
      replay.AddPlayer(player, server.TuningData().rules.startingOre);
      replay.SaveStartingDesigns(player, server.TuningData());
    }
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

  // Design §6: every player starts with its Command Station on its start and the starting Constructors in front of it,
  // facing the map's center and clear of each other, all at full strength.
  TEST_METHOD(PlacesEachPlayersStartingBase)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    const Outpost::Map map = RepositoryMap();
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    server.World().PlaceStartingBases(map);
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    const std::unique_ptr<Outpost::Transport> red = server.Connect(RED);
    server.Advance(50ms);
    const std::vector<Outpost::Snapshot> blueSnapshots = blue->Receive();
    const std::vector<Outpost::Snapshot> redSnapshots = red->Receive();
    Assert::AreEqual(size_t{1}, blueSnapshots.size());
    Assert::AreEqual(size_t{1}, redSnapshots.size());

    // Under fog of war each player sees its own base and not the other's, across the map (ADR-024).
    std::vector<Outpost::EntityView> placed;
    std::vector<Outpost::EntityView> entities;
    for (const Outpost::Snapshot& snapshot : {blueSnapshots[0], redSnapshots[0]})
    {
      for (const Outpost::EntityView& entity : snapshot.entities)
      {
        Assert::IsTrue(!entity.owner.IsValid() || entity.owner == snapshot.player, L"the other base is out of sight");
        if (entity.owner.IsValid())
          entities.push_back(entity);
      }
    }
    for (const Outpost::EntityView& entity : entities)
    {
      placed.push_back(entity);
      const Outpost::PlanePosition start = map.starts[entity.owner.value - 1];
      Assert::IsTrue(entity.hitPointsHundredths > 0 && entity.hitPointsHundredths == entity.maxHitPointsHundredths);
      if (entity.kind == Outpost::EntityKind::Structure)
      {
        Assert::IsTrue(entity.structure == Outpost::StructureKind::CommandStation);
        Assert::AreEqual(Outpost::PERMILLE, entity.builtPermille);
        Assert::IsTrue(entity.position == start);
        continue;
      }
      Assert::IsTrue(entity.role == Outpost::ShipRole::Constructor);
      Assert::IsTrue(Outpost::Distance(entity.position, start) < 120.0f);
      Assert::AreEqual(std::atan2(-start.zMeters, -start.xMeters), entity.headingRadians, 1e-5f);
    }
    Assert::AreEqual(size_t{2} * (1 + static_cast<size_t>(tuning.rules.startingConstructors)), placed.size());
    for (const Outpost::EntityView& entity : placed)
    {
      for (const Outpost::EntityView& other : placed)
      {
        if (other.id != entity.id)
          Assert::IsTrue(Outpost::Distance(entity.position, other.position) >= entity.radiusMeters + other.radiusMeters);
      }
    }
  }

  TEST_METHOD(RefusesAStartingBaseThatDoesNotFit)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    Outpost::Map map = RepositoryMap();
    // Within the Command Station's footprint of a home asteroid.
    map.starts[0] = {map.oreAsteroids[0].position.xMeters, map.oreAsteroids[0].position.zMeters - 60.0f};
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    Assert::ExpectException<Neuron::Exception>([&] { server.World().PlaceStartingBases(map); });
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