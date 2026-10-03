#pragma once

namespace Outpost
{
// Both ends of one player's loopback connection: in-process queues with no serialization (ADR-002 decision 6). The
// transport and the server share it, so either may be destroyed first.
struct LoopbackChannel
{
  // Guards both queues: the server's thread takes the commands and adds the snapshots while the client's thread does the
  // opposite (ADR-025).
  std::mutex mutex;
  std::vector<Command> commands;
  std::vector<Snapshot> snapshots;
};

// A client's end of the loopback.
class LoopbackTransport final : public Transport
{
public:
  explicit LoopbackTransport(std::shared_ptr<LoopbackChannel> _channel) noexcept;

  void Send(Command _command) override;
  [[nodiscard]] std::vector<Snapshot> Receive() override;

private:
  std::shared_ptr<LoopbackChannel> m_channel;
};

// A command as the server applied it: the tick it was applied at the start of, and the order with its player as the
// connection set it. A seed and this log reproduce the match on the same build (ADR-009).
struct LoggedCommand
{
  std::uint64_t tick = 0;
  Command command;
};

// The server inside the client (ADR-002). Started, it runs its ticks on a thread of its own (ADR-025); a test may instead
// step it by hand with Advance, on the test's thread. Match setup, World and the command log belong to whichever thread
// steps it, so a started server is touched only through its connections and TakeTickTimings.
class InProcessServer final : public Server
{
public:
  // Places _map in a new simulation seeded from _desc. Throws Neuron::Exception when a hull's footprint is wider than the
  // map's minimum gap, since such a ship could be walled off. It builds no pathfinding graph: match setup ends with
  // PreparePathfinding, and until then each graph is built by the first path that needs it.
  InProcessServer(Tuning _tuning, Map _map, const ServerDesc& _desc);

  // Throws Neuron::Exception once the server has started.
  [[nodiscard]] std::unique_ptr<Transport> Connect(PlayerId _player) override;
  // Throws Neuron::Exception when it has started already.
  void Start() override;
  void Step() override;
  [[nodiscard]] std::uint32_t TicksPerSecond() const noexcept override;
  [[nodiscard]] std::vector<TickTiming> TakeTickTimings() override;

  // Steps a server that has not started, for tests: runs the ticks due after _elapsedWallTime more wall time, applying the
  // commands that have arrived and sending each connected player a snapshot per tick. This and the started server's
  // thread are where wall time becomes ticks (ADR-009). Throws Neuron::Exception once the server has started.
  void Advance(std::chrono::nanoseconds _elapsedWallTime);

  // For match setup and for tests: the state the server owns.
  [[nodiscard]] Simulation& World() noexcept
  {
    return m_simulation;
  }

  [[nodiscard]] const Tuning& TuningData() const noexcept
  {
    return m_tuning;
  }

  [[nodiscard]] const Map& MapData() const noexcept
  {
    return m_map;
  }

  [[nodiscard]] const std::vector<LoggedCommand>& CommandLog() const noexcept
  {
    return m_commandLog;
  }

  // Match setup for a measurement run: places task 3.7's stress scene and keeps it at full size before every tick.
  void StartStressLoad();

  // The end of match setup: builds the pathfinding graphs for every hull and the Constructor ahead of their first order,
  // once every structure of the setup stands (ADR-032). Called once, after the bases and any load are placed.
  void PreparePathfinding();

private:
  // Runs one tick: the commands that arrived since the last, in connection order and then in the order each client sent
  // them, and then a snapshot for every connected player.
  void RunTick();

  // The started server's thread: it sleeps until the next tick is due, runs the ticks that are, and stops when asked.
  void Run(const std::stop_token& _stop);

  struct Connection
  {
    PlayerId player;
    std::shared_ptr<LoopbackChannel> channel;
  };

  Tuning m_tuning;
  Map m_map;
  Neuron::TickHost m_tickHost;
  Simulation m_simulation;
  std::vector<Connection> m_connections;
  std::vector<LoggedCommand> m_commandLog;
  std::optional<StressLoad> m_stressLoad;
  // Times the parts of each tick on the thread that runs them (task 8.1): the simulation tells it where each begins and
  // ends, and the server times its snapshots.
  TickProfiler m_profiler;
  // Guards the tick timings and the failure, which the started server's thread writes and TakeTickTimings reads.
  std::mutex m_reportMutex;
  std::vector<TickTiming> m_tickTimings;
  std::exception_ptr m_failure;
  // Last, so that it is destroyed first: the thread stops and is joined before anything it uses goes.
  std::jthread m_thread;
};
} // namespace Outpost