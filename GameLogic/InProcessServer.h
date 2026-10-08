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

// The server inside the client (ADR-002). Started, it runs its ticks on a thread of its own (ADR-025); a test may instead
// step it by hand with Advance, on the test's thread. Match setup, World and the command log belong to whichever thread
// steps it, so a started server is touched only through its connections and TakeTickTimings. Made to, it also takes
// players over QUIC on the loopback address, each into a seat opened for it (ADR-060).
class InProcessServer final : public Server
{
public:
  // Places _map in a new simulation seeded from _desc, with the pirates' outposts and the derelicts the seed placed unless
  // _desc asks for a measurement or stress run (ADR-073, ADR-074). Throws Neuron::Exception when a hull's footprint is wider
  // than the map's minimum gap, since such a ship could be walled off. It builds no pathfinding graph: match setup ends
  // with PreparePathfinding, and until then each graph is built by the first path that needs it.
  InProcessServer(Tuning _tuning, Map _map, const ServerDesc& _desc);

  // Throws Neuron::Exception once the server has started.
  [[nodiscard]] std::unique_ptr<Transport> Connect(PlayerId _player) override;
  [[nodiscard]] ServerAddress OpenSeat(PlayerId _player, const SeatToken& _token) override;
  // Throws Neuron::Exception when it has started already, or when a seat has not been taken.
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

  // The commands applied so far; for a world, since its last save, its folder keeping the rest.
  [[nodiscard]] const std::vector<LoggedCommand>& CommandLog() const noexcept
  {
    return m_commandLog;
  }

  // Match setup for a measurement run: places task 3.7's stress scene and keeps it at full size before every tick.
  void StartStressLoad();

  // The end of match setup: builds the pathfinding graphs for every hull and the Constructor ahead of their first order,
  // once every structure of the setup stands (ADR-032). Called once, after the bases and any load are placed.
  void PreparePathfinding();

  // Match setup for a world (Phase 5 design §5, ADR-077), before PreparePathfinding: from here the server logs every tick's
  // commands to _folder and saves the world there every WORLD_SAVE_SECONDS of ticks. With _recovery, which
  // WorldFolder::Recover made of _folder, the simulation becomes the world it saved, and the commands logged since are
  // applied to it, tick by tick, so that it comes back at the tick after the last of them. The world is saved at once,
  // and its log begins anew there. _dataHash is DataHash of the tuning data's and the map's files. Throws
  // Neuron::Exception when the save is of another world, or once the server has started.
  void UseWorld(const std::filesystem::path& _folder, std::uint64_t _dataHash, std::optional<WorldFolder::Recovery> _recovery);

private:
  // Runs one tick: the commands that arrived since the last, in connection order and then in the order each client sent
  // them, and then a snapshot for every connected player. A world's tick logs its commands and saves when a save is due.
  void RunTick();

  // Hands a save of the world as it stands to its folder, which begins its log anew there (ADR-077).
  void SaveWorld();

  // The started server's thread: it sleeps on a timer until the next tick is due (ADR-055), runs the ticks that are, and
  // stops when asked.
  void Run(const std::stop_token& _stop);

  // One player's connection. Its commands arrive in the channel's queue whichever way the player connected. A player who
  // connected over the loopback finds its snapshots in the channel too; a seat sends them over QUIC once it is taken.
  struct Connection
  {
    PlayerId player;
    std::shared_ptr<LoopbackChannel> channel;
    bool seat = false;
    // What a hello shows to take the seat (ADR-078).
    SeatToken token{};
    // Guarded by m_seatMutex: MsQuic's thread sets it when the player's hello takes the seat, and again when a newer
    // connection takes it.
    std::shared_ptr<Neuron::QuicChannel> quic;
  };

  // Adds a connection for _player, unless the server has started or the player has one already.
  std::shared_ptr<LoopbackChannel> AddConnection(PlayerId _player, bool _seat, const SeatToken& _token = {});

  // What the listener does with a client that has connected over QUIC: its first message is a hello that takes a seat,
  // and every message after it a command (ADR-060). Called on MsQuic's thread.
  [[nodiscard]] Neuron::QuicChannel::Receiver Admit(const std::shared_ptr<Neuron::QuicChannel>& _channel);

  // Gives _player's seat to _channel, which showed _token, and returns where the seat's commands go; none when there is
  // no such seat, the token is not the seat's, or a match has started. A connection that held the seat is closed with
  // CloseReason::SeatTaken: the player's newest connection holds it (ADR-078). Called on MsQuic's thread.
  [[nodiscard]] std::shared_ptr<LoopbackChannel> TakeSeat(PlayerId _player, const SeatToken& _token,
                                                          std::shared_ptr<Neuron::QuicChannel> _channel);

  // Whether _channel holds _player's seat now, which a connection the seat was taken from no longer does. Called on
  // MsQuic's thread.
  [[nodiscard]] bool HoldsSeat(PlayerId _player, const Neuron::QuicChannel* _channel);

  Tuning m_tuning;
  Map m_map;
  std::uint64_t m_seed = 0;
  // Whether the server is a world's (ServerDesc::world), which its seats may be taken in while it runs. Set when it is
  // made, and only read after, by any thread.
  bool m_isWorld = false;
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
  // Guards the connections while they can still change: until the server starts, MsQuic's threads seat players as the
  // shell's thread adds them. Each connection's QUIC channel stays guarded after it.
  std::mutex m_seatMutex;
  bool m_started = false;
  // Where players connect over QUIC, when the server takes them (ServerDesc::quic). Destroyed after the thread and before
  // the connections, so that no callback of MsQuic's outlives what it touches.
  std::unique_ptr<Neuron::QuicListener> m_listener;
  // The host OpenSeat names for the listener, set with it.
  std::string m_listenHost;
  // A world's folder and which world it is, once UseWorld has made the server a world's. Destroyed after the thread, so
  // that the saves it was handed are written before the server goes.
  std::unique_ptr<WorldFolder> m_world;
  WorldIdentity m_identity;
  // Last, so that it is destroyed first: the thread stops and is joined before anything it uses goes.
  std::jthread m_thread;
};
} // namespace Outpost