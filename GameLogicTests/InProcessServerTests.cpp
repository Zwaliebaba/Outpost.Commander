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

// A hosted player that gives no order.
class Idle final : public Outpost::HostedPlayer
{
public:
  [[nodiscard]] std::vector<Outpost::Command> Play([[maybe_unused]] const Outpost::Snapshot& _snapshot) override
  {
    return {};
  }
};
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

  // Task 13.1: a headless run steps the server a tick at a time, with no wall time between ticks.
  TEST_METHOD(StepsOneTickAtOnce)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    server.Step();
    server.Step();
    const std::vector<Outpost::Snapshot> snapshots = blue->Receive();
    Assert::AreEqual(size_t{2}, snapshots.size());
    Assert::AreEqual(std::uint64_t{2}, snapshots[1].tick);
    Assert::AreEqual(std::uint64_t{2}, server.World().CurrentTick());
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

  // ADR-080: a scheduled order's time of day becomes the first tick at or after its moment, as the host's wall clock reads
  // now; a moment past is now, and one further than a week ahead a week ahead.
  TEST_METHOD(MakesAMomentItsTick)
  {
    const std::chrono::system_clock::time_point now{std::chrono::seconds{1'000'000}};
    Assert::AreEqual(std::uint64_t{205}, Outpost::TickOfMoment(1'000'010, now, 5, 20));
    Assert::AreEqual(std::uint64_t{5}, Outpost::TickOfMoment(1'000'000, now, 5, 20), L"now");
    Assert::AreEqual(std::uint64_t{5}, Outpost::TickOfMoment(999'000, now, 5, 20), L"past");
    Assert::AreEqual(std::uint64_t{5 + 195}, Outpost::TickOfMoment(1'000'010, now + 260ms, 5, 20), L"9.74 s rounds up to 195 ticks");
    const std::uint64_t week = std::uint64_t{7} * 24 * 60 * 60 * 20;
    Assert::AreEqual(5 + week, Outpost::TickOfMoment(1'000'000 + (30 * 24 * 60 * 60), now, 5, 20), L"a week at most");
  }

  // The host makes the tick of a scheduled order's time of day as the command arrives: the client's own tick is never
  // trusted, and the log keeps the host's, from which the world replays (ADR-009, ADR-080).
  TEST_METHOD(MakesTheTickOfAScheduledOrdersTimeOfDayAsItArrives)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
    const Outpost::EntityId ship = server.World().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    const std::int64_t sent =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 6;
    blue->Send({.order = Outpost::ScheduleOrderCommand{
                  .ships = {ship},
                  .trigger = {.kind = Outpost::ScheduledTriggerKind::TimeOfDay, .utcSeconds = sent, .tick = 999'999},
                  .action = {.kind = Outpost::ScheduledActionKind::Move, .position = {.xMeters = 100.0f}}}});
    server.Step();
    const std::vector<Outpost::ScheduledOrderView> scheduled = server.World().BuildSnapshot(BLUE).scheduled;
    Assert::AreEqual(std::size_t{1}, scheduled.size());
    // Between five and six seconds ahead of tick 0, at twenty ticks a second (100 to 120), whenever in its second the test began.
    const std::uint64_t tick = scheduled[0].trigger.tick;
    Assert::IsTrue(tick >= 100 && tick <= 120, std::format(L"tick {}", tick).c_str());
    const auto& logged = std::get<Outpost::ScheduleOrderCommand>(server.CommandLog().front().command.order);
    Assert::AreEqual(tick, logged.trigger.tick, L"the log keeps the host's tick");
  }

  // ADR-009: a match reproduces from its seed and the server's command log, on the same build.
  // ADR-025: started, the server runs its own ticks on a thread of its own. Orders sent from this thread reach it, a
  // snapshot arrives for every tick in order, and destroying the server stops its thread. Stepping it by hand, or
  // connecting, is then refused.
  TEST_METHOD(RunsItsTicksOnItsOwnThread)
  {
    auto server = std::make_unique<Outpost::InProcessServer>(RepositoryTuning(), RepositoryMap(), Outpost::ServerDesc{.seed = 5});
    const Outpost::EntityId ship = server->World().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    const std::unique_ptr<Outpost::Transport> blue = server->Connect(BLUE);
    server->Start();
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server->Connect(RED); });
    Assert::ExpectException<Neuron::Exception>([&server] { server->Advance(50ms); });
    Assert::ExpectException<Neuron::Exception>([&server] { server->Step(); });
    Assert::ExpectException<Neuron::Exception>([&server] { server->Start(); });

    // Orders sent while the server ticks, a few to each tick, the last of them sending the ship on its way.
    for (int order = 0; order < 100; ++order)
    {
      blue->Send({.order = Outpost::StopCommand{.ships = {ship}}});
      std::this_thread::sleep_for(1ms);
    }
    blue->Send({.order = Outpost::MoveCommand{.ships = {ship}, .destination = {.xMeters = 300.0f}}});
    (void)blue->Receive();
    std::vector<Outpost::Snapshot> snapshots;
    // Ten ticks take half a second; the deadline only keeps a stalled server from hanging the test.
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (snapshots.size() < 10 && std::chrono::steady_clock::now() < deadline)
    {
      std::this_thread::sleep_for(10ms);
      for (Outpost::Snapshot& snapshot : blue->Receive())
        snapshots.push_back(std::move(snapshot));
    }
    Assert::IsTrue(snapshots.size() >= 10, L"the server ran no ticks on its own");
    for (size_t i = 1; i < snapshots.size(); ++i)
      Assert::AreEqual(snapshots[i - 1].tick + 1, snapshots[i].tick, L"a snapshot for every tick, in order");
    const auto moved = std::ranges::find(snapshots.back().entities, ship, &Outpost::EntityView::id);
    Assert::IsTrue(moved != snapshots.back().entities.end() && moved->position.xMeters > 0.0f, L"the order reached the server");
    Assert::IsTrue(server->TakeTickTimings().size() >= 10);

    server.reset();
    (void)blue->Receive();
    std::this_thread::sleep_for(200ms);
    Assert::IsTrue(blue->Receive().empty(), L"no ticks once the server is gone");
  }

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
    replay.PlacePirates(server.MapData());
    replay.PlaceDerelicts(server.MapData());
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

  // ADR-079: a player the server hosts plays on the server's own thread, and its orders are applied at the next tick and
  // logged as a connection's are, so that a world of two AI empires replays from its seed and its log without them.
  TEST_METHOD(HostsAiEmpiresWhoseOrdersReplayFromTheLog)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 41});
    server.World().PlaceStartingBases(server.MapData());
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    server.Host(BLUE, std::make_unique<Outpost::AiEmpire>(settings, server.TicksPerSecond()));
    server.Host(RED, std::make_unique<Outpost::AiEmpire>(settings, server.TicksPerSecond()));
    server.PreparePathfinding();
    for (std::uint32_t tick = 0; tick < 3 * 60 * server.TicksPerSecond(); ++tick)
      server.Step();
    for (const Outpost::PlayerId player : {BLUE, RED})
    {
      Assert::IsTrue(std::ranges::any_of(server.CommandLog(),
                                         [player](const Outpost::LoggedCommand& _logged) { return _logged.command.player == player; }),
                     L"each empire played");
    }

    Outpost::InProcessServer replay(RepositoryTuning(), RepositoryMap(), {.seed = 41});
    replay.World().PlaceStartingBases(replay.MapData());
    replay.PreparePathfinding();
    const std::vector<Outpost::LoggedCommand>& log = server.CommandLog();
    size_t next = 0;
    while (replay.World().CurrentTick() < server.World().CurrentTick())
    {
      std::vector<Outpost::Command> commands;
      for (; next < log.size() && log[next].tick == replay.World().CurrentTick(); ++next)
        commands.push_back(log[next].command);
      (void)replay.World().Tick(commands);
    }
    Assert::IsTrue(replay.World() == server.World());
  }

  // W2 (Phase 5 design §2): what two AI empires cost the server's thread over an hour of world, as the tick's 99th
  // percentile and its longest, and the hosted players' part of them. Run on its own, with OUTPOST_WORLD_MEASURE set.
  TEST_METHOD(MeasuresTheHostedPlayersTicks)
  {
    if (GetEnvironmentVariableW(L"OUTPOST_WORLD_MEASURE", nullptr, 0) == 0)
    {
      Logger::WriteMessage("Not run here: OUTPOST_WORLD_MEASURE runs it.");
      return;
    }
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    for (const std::uint64_t seed : {1, 2, 3})
    {
      Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = seed});
      server.World().PlaceStartingBases(server.MapData());
      server.Host(BLUE, std::make_unique<Outpost::AiEmpire>(settings, server.TicksPerSecond()));
      server.Host(RED, std::make_unique<Outpost::AiEmpire>(settings, server.TicksPerSecond()));
      server.PreparePathfinding();
      std::vector<std::chrono::nanoseconds> totals;
      std::vector<std::chrono::nanoseconds> hosted;
      for (std::uint32_t minute = 0; minute < 60; ++minute)
      {
        for (std::uint32_t tick = 0; tick < 60 * server.TicksPerSecond(); ++tick)
          server.Step();
        for (const Outpost::TickTiming& timing : server.TakeTickTimings())
        {
          totals.push_back(timing.total);
          hosted.push_back(timing.Part(Outpost::TickPart::Hosted));
        }
      }
      const auto percentile = [](std::vector<std::chrono::nanoseconds> _values)
      {
        std::ranges::sort(_values);
        return std::chrono::duration<double, std::milli>(_values[(_values.size() * 99) / 100]).count();
      };
      Logger::WriteMessage(std::format("seed {}: a tick's 99th percentile {:.2f} ms, longest {:.2f} ms; the hosted players' 99th "
                                       "percentile {:.2f} ms, longest {:.2f} ms",
                                       seed, percentile(totals),
                                       std::chrono::duration<double, std::milli>(std::ranges::max(totals)).count(), percentile(hosted),
                                       std::chrono::duration<double, std::milli>(std::ranges::max(hosted)).count())
                             .c_str());
    }
  }

  TEST_METHOD(HostsAPlayerOnceAndBeforeItStarts)
  {
    Outpost::InProcessServer server(RepositoryTuning(), RepositoryMap(), {.seed = 1});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    Assert::ExpectException<Neuron::Exception>([&server] { server.Host(BLUE, std::make_unique<Idle>()); }, L"a player with a connection");
    Assert::ExpectException<Neuron::Exception>([&server] { server.Host(RED, nullptr); }, L"nothing to play it");
    server.Host(RED, std::make_unique<Idle>());
    Assert::ExpectException<Neuron::Exception>([&server] { server.Host(RED, std::make_unique<Idle>()); }, L"a player hosted already");
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server.Connect(RED); }, L"and it connects no more");
    server.Start();
    Assert::ExpectException<Neuron::Exception>([&server] { server.Host(Outpost::PlayerId{3}, std::make_unique<Idle>()); },
                                               L"once it has started");
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