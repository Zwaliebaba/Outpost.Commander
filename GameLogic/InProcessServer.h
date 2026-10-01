#pragma once

namespace Outpost
{
// Both ends of one player's loopback connection: in-process queues with no serialization (ADR-002 decision 6). The
// transport and the server share it, so either may be destroyed first.
struct LoopbackChannel
{
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

// The server inside the client (ADR-002). It runs on whichever thread calls Advance, which in the MVP is the frame loop
// (ADR-009), so nothing in it is locked.
class InProcessServer final : public Server
{
public:
  // Places _map in a new simulation seeded from _desc. Throws Neuron::Exception when a hull's footprint is wider than the
  // map's minimum gap, since such a ship could be walled off.
  InProcessServer(Tuning _tuning, Map _map, const ServerDesc& _desc);

  [[nodiscard]] std::unique_ptr<Transport> Connect(PlayerId _player) override;
  void Advance(std::chrono::nanoseconds _elapsedWallTime) override;
  [[nodiscard]] std::uint32_t TicksPerSecond() const noexcept override;
  [[nodiscard]] std::vector<std::chrono::nanoseconds> TakeTickDurations() override;

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

private:
  // Builds the pathfinding graphs for every hull and the Constructor ahead of their first order.
  void PreparePathfinding();

  // Runs one tick: the commands that arrived since the last, in connection order and then in the order each client sent
  // them, and then a snapshot for every connected player.
  void RunTick();

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
  std::vector<std::chrono::nanoseconds> m_tickDurations;
};
} // namespace Outpost
