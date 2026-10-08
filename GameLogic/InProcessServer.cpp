#include "pch.h"
#include "InProcessServer.h"

#include <algorithm>
#include <utility>

namespace
{
// Ticks run by one Advance at most: 250 ms of simulation at 20 Hz. After a longer stall the simulation drops the rest
// and resumes at its own pace (ADR-009).
constexpr std::uint32_t MAX_TICKS_PER_ADVANCE = 5;
// A waitable timer's due time is counted in hundreds of nanoseconds.
using HundredNanoseconds = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
// Where the server finds its data, under the package's Assets folder (ADR-008).
constexpr std::string_view TUNING_FILE = "Tuning.json";
constexpr std::string_view MAP_FILE = "Map.json";
// Where a seat's player connects: the listener takes connections on the loopback address only (ADR-060).
constexpr std::string_view LOOPBACK_HOST = "127.0.0.1";

void Refuse(Neuron::QuicChannel& _channel, Outpost::CloseReason _reason) noexcept
{
  _channel.Shutdown(static_cast<std::uint64_t>(_reason));
}

// The host a seat's player reaches a server listening on _address at, from the same machine: the loopback address for
// the loopback itself or every interface, and otherwise the address it listens on.
std::string ListenHost(std::string_view _address)
{
  if (_address.empty() || _address == "0.0.0.0")
    return std::string(LOOPBACK_HOST);
  if (_address == "::")
    return "::1";
  return std::string(_address);
}

std::string ReadDataFile(std::string_view _fileName)
{
  // The names above are ASCII, so widening them character by character is exact.
  const Neuron::ByteBuffer bytes = Neuron::BinaryFile::ReadFile(std::wstring(_fileName.begin(), _fileName.end()));
  if (bytes.empty())
    throw Neuron::Exception(std::format("The game data file Assets\\{} is missing or cannot be read.", _fileName));
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// The research topics a derelict may name: every one of the tuning data's, in its order (ADR-074).
std::vector<Outpost::ResearchTopicId> TopicsOf(const Outpost::Tuning& _tuning)
{
  std::vector<Outpost::ResearchTopicId> topics;
  topics.reserve(_tuning.research.size());
  for (const Outpost::ResearchTopicTuning& topic : _tuning.research)
    topics.push_back(topic.id);
  return topics;
}

// The tuning data's and the map's files as read, and what they hold.
struct GameData
{
  Outpost::Tuning tuning;
  Outpost::Map map;
  // DataHash of the two files, which names them in a world's save (ADR-077).
  std::uint64_t hash = 0;
};

GameData ReadGameData()
{
  const std::string tuning = ReadDataFile(TUNING_FILE);
  const std::string map = ReadDataFile(MAP_FILE);
  const std::array<std::string_view, 2> texts{tuning, map};
  return {Outpost::LoadTuning(tuning), Outpost::LoadMap(map), Outpost::DataHash(texts)};
}

// A server for _desc, set up: a match's map is placed, and now every player's starting base (ADR-016), and the load a
// measurement run asks for. A world whose folder holds a save comes back as that world instead, made with its seed
// (ADR-077).
std::unique_ptr<Outpost::Server> SetUpServer(const GameData& _data, Outpost::ServerDesc _desc)
{
  std::optional<Outpost::WorldFolder::Recovery> recovery;
  if (!_desc.world.empty())
  {
    recovery = Outpost::WorldFolder::Recover(_desc.world);
    if (recovery)
      _desc.seed = recovery->header.identity.seed;
  }
  auto server = std::make_unique<Outpost::InProcessServer>(_data.tuning, _data.map, _desc);
  if (!recovery)
  {
    // A world has no end, and a player who loses restarts (Phase 5 design §8); a save carries its rules with it.
    if (!_desc.world.empty())
      server->World().UseWorldRules();
    server->World().PlaceStartingBases(server->MapData());
    if (_desc.measurementLoad)
      Outpost::PlaceMeasurementLoad(server->World(), server->MapData(), server->TuningData());
    if (_desc.stressLoad)
      server->StartStressLoad();
  }
  if (!_desc.world.empty())
    server->UseWorld(_desc.world, _data.hash, std::move(recovery));
  // Match setup is over, and every structure it places stands: the graphs are built once, now, so that the first order of
  // the match does not pay for them (ADR-032).
  server->PreparePathfinding();
  return server;
}
} // namespace

Outpost::LoopbackTransport::LoopbackTransport(std::shared_ptr<LoopbackChannel> _channel) noexcept
  : m_channel(std::move(_channel))
{
}

void Outpost::LoopbackTransport::Send(Command _command)
{
  const std::scoped_lock lock(m_channel->mutex);
  m_channel->commands.push_back(std::move(_command));
}

std::vector<Outpost::Snapshot> Outpost::LoopbackTransport::Receive()
{
  const std::scoped_lock lock(m_channel->mutex);
  return std::exchange(m_channel->snapshots, {});
}

Outpost::InProcessServer::InProcessServer(Tuning _tuning, Map _map, const ServerDesc& _desc)
  : m_tuning(std::move(_tuning)),
    m_map(PlaceContent(std::move(_map), _desc.seed, TopicsOf(m_tuning))),
    m_seed(_desc.seed),
    m_isWorld(!_desc.world.empty()),
    m_tickHost(static_cast<std::uint32_t>(m_tuning.rules.tickHz), MAX_TICKS_PER_ADVANCE),
    m_simulation(_desc.seed, static_cast<std::uint32_t>(m_tuning.rules.tickHz))
{
  // Every passage is at least the map's minimum gap wide, which is what keeps every asteroid reachable (MapTests). A hull
  // wider than that could be walled off, so the two files are checked against each other here, where both are known.
  for (const HullTuning& hull : m_tuning.hulls)
  {
    if (2.0 * hull.footprintRadiusMeters > m_map.minimumGapMeters)
    {
      throw Neuron::Exception(std::format("The {} hull's footprint, {} m across, is wider than the map's narrowest passage, {} m.",
                                          hull.name, 2.0 * hull.footprintRadiusMeters, m_map.minimumGapMeters));
    }
  }
  m_simulation.PlaceMap(m_map);
  m_simulation.UseTuning(m_tuning);
  // Every match is played under fog of war (ADR-024).
  m_simulation.UseFog();
  // One player per start, each with the starting Ore and the starting designs saved (design §5, §7).
  for (size_t player = 0; player < m_map.starts.size(); ++player)
  {
    const PlayerId id{static_cast<std::uint32_t>(player + 1)};
    m_simulation.AddPlayer(id, m_tuning.rules.startingOre);
    m_simulation.SaveStartingDesigns(id, m_tuning);
  }
  // The pirates' outposts and the derelicts the seed placed, after the players, so that each player's designs are numbered as before. A
  // measurement run is not a match, and its scenes stay as they were measured.
  if (!_desc.measurementLoad && !_desc.stressLoad)
  {
    m_simulation.PlacePirates(m_map);
    m_simulation.PlaceDerelicts(m_map);
  }
  // Last, since a client may connect as soon as it listens.
  if (_desc.quic)
  {
    // A world keeps its certificate in its folder, so that a player pins it once (ADR-078).
    m_listener =
      std::make_unique<Neuron::QuicListener>(Neuron::QuicListener::Desc{.applicationProtocol = std::string(QUIC_APPLICATION_PROTOCOL),
                                                                        .address = _desc.quicAddress,
                                                                        .port = _desc.quicPort,
                                                                        .identity = _desc.world},
                                             [this](const std::shared_ptr<Neuron::QuicChannel>& _channel) { return Admit(_channel); });
    m_listenHost = ListenHost(_desc.quicAddress);
  }
}

void Outpost::InProcessServer::PreparePathfinding()
{
  // The same float a ship of the hull is given (MovementFor), since the graphs are kept by radius.
  for (const HullTuning& hull : m_tuning.hulls)
    m_simulation.PreparePathfinding(static_cast<float>(hull.footprintRadiusMeters));
  m_simulation.PreparePathfinding(static_cast<float>(m_tuning.constructor.footprintRadiusMeters));
}

std::shared_ptr<Outpost::LoopbackChannel> Outpost::InProcessServer::AddConnection(PlayerId _player, bool _seat, const SeatToken& _token)
{
  if (!_player.IsValid())
    throw Neuron::Exception("InProcessServer: a connection needs a player");
  const std::scoped_lock lock(m_seatMutex);
  if (m_started)
    throw Neuron::Exception("InProcessServer: players connect before the server starts");
  if (std::ranges::any_of(m_connections, [_player](const Connection& _connection) { return _connection.player == _player; }))
    throw Neuron::Exception(std::format("InProcessServer: player {} is already connected", _player.value));

  auto channel = std::make_shared<LoopbackChannel>();
  m_connections.push_back({.player = _player, .channel = channel, .seat = _seat, .token = _token});
  return channel;
}

std::unique_ptr<Outpost::Transport> Outpost::InProcessServer::Connect(PlayerId _player)
{
  return std::make_unique<LoopbackTransport>(AddConnection(_player, false));
}

Outpost::ServerAddress Outpost::InProcessServer::OpenSeat(PlayerId _player, const SeatToken& _token)
{
  if (!m_listener)
    throw Neuron::Exception("InProcessServer: this server does not take players over QUIC");
  AddConnection(_player, true, _token);
  return {.host = m_listenHost, .port = m_listener->Port(), .certificate = m_listener->Certificate(), .token = _token};
}

void Outpost::InProcessServer::Host(PlayerId _player, std::unique_ptr<HostedPlayer> _hosted)
{
  if (!_player.IsValid() || !_hosted)
    throw Neuron::Exception("InProcessServer: hosting needs a player and what plays it");
  const std::scoped_lock lock(m_seatMutex);
  if (m_started)
    throw Neuron::Exception("InProcessServer: players are hosted before the server starts");
  const auto connection = std::ranges::find(m_connections, _player, &Connection::player);
  // A player with no connection is the server's own, an AI empire, whose orders reach the tick as a connection's would.
  if (connection == m_connections.end())
  {
    m_connections.push_back({.player = _player, .channel = std::make_shared<LoopbackChannel>(), .hosted = std::move(_hosted)});
    return;
  }
  // A seat's deputy.
  if (!connection->seat || connection->hosted)
    throw Neuron::Exception(std::format("InProcessServer: player {} is hosted already or has a connection of its own", _player.value));
  connection->hosted = std::move(_hosted);
  connection->control.emplace(TicksPerSecond());
}

Neuron::QuicChannel::Receiver Outpost::InProcessServer::Admit(const std::shared_ptr<Neuron::QuicChannel>& _channel)
{
  // MsQuic hands the receiver one message at a time, so what it keeps between them needs no lock: the connection it
  // serves, and the queue its commands go to once its hello has taken a seat.
  return [this, connection = std::weak_ptr(_channel), commands = std::shared_ptr<LoopbackChannel>(),
          seated = PlayerId()](Neuron::QuicChannel& _peer, std::vector<std::byte> _message) mutable
  {
    try
    {
      // A hello of another version may not decode at all, so its version is read first (ADR-060).
      if (!commands)
      {
        const std::optional<std::uint32_t> version = PeekHelloVersion(_message);
        if (version.has_value() && *version != PROTOCOL_VERSION)
        {
          Refuse(_peer, CloseReason::WrongVersion);
          return;
        }
      }
      Message message = DecodeMessage(_message);
      if (commands)
      {
        auto* command = std::get_if<Command>(&message);
        if (command == nullptr)
        {
          Refuse(_peer, CloseReason::MalformedMessage);
          return;
        }
        // A connection the seat was taken from sends nothing more to the world (ADR-078).
        if (!HoldsSeat(seated, &_peer))
          return;
        // The player is set from the seat when the tick takes the command; what the client wrote is never trusted
        // (ADR-002).
        const std::scoped_lock lock(commands->mutex);
        commands->commands.push_back(std::move(*command));
        return;
      }

      const auto* hello = std::get_if<HelloMessage>(&message);
      if (hello == nullptr)
      {
        Refuse(_peer, CloseReason::MalformedMessage);
        return;
      }
      if (hello->protocolVersion != PROTOCOL_VERSION)
      {
        Refuse(_peer, CloseReason::WrongVersion);
        return;
      }
      commands = TakeSeat(hello->player, hello->token, connection.lock());
      if (!commands)
      {
        Refuse(_peer, CloseReason::SeatRefused);
        return;
      }
      seated = hello->player;
      _peer.Send(EncodeMessage(WelcomeMessage{.player = hello->player}));
    }
    catch (const Neuron::Exception&)
    {
      Refuse(_peer, CloseReason::MalformedMessage);
    }
  };
}

std::shared_ptr<Outpost::LoopbackChannel> Outpost::InProcessServer::TakeSeat(PlayerId _player, const SeatToken& _token,
                                                                             std::shared_ptr<Neuron::QuicChannel> _channel)
{
  std::shared_ptr<Neuron::QuicChannel> replaced;
  std::shared_ptr<LoopbackChannel> commands;
  {
    const std::scoped_lock lock(m_seatMutex);
    // A match's seats are taken before it starts; a world's at any time (ADR-078).
    if ((m_started && !m_isWorld) || !_channel)
      return nullptr;
    const auto seat =
      std::ranges::find_if(m_connections, [_player](const Connection& _connection) { return _connection.player == _player; });
    if (seat == m_connections.end() || !seat->seat || seat->token != _token)
      return nullptr;
    // The player's newest connection holds the seat, so that one whose connection dropped without the server knowing yet
    // takes it back.
    replaced = std::exchange(seat->quic, std::move(_channel));
    ++seat->takings;
    commands = seat->channel;
  }
  // The connection it had is told why it goes, outside the lock, which its own callbacks take.
  if (replaced)
    replaced->Shutdown(static_cast<std::uint64_t>(CloseReason::SeatTaken));
  return commands;
}

bool Outpost::InProcessServer::HoldsSeat(PlayerId _player, const Neuron::QuicChannel* _channel)
{
  const std::scoped_lock lock(m_seatMutex);
  const auto seat = std::ranges::find_if(m_connections, [_player](const Connection& _connection) { return _connection.player == _player; });
  return seat != m_connections.end() && seat->quic.get() == _channel;
}

std::uint64_t Outpost::TickOfMoment(std::int64_t _utcSeconds, std::chrono::system_clock::time_point _now, std::uint64_t _tick,
                                    std::uint32_t _ticksPerSecond) noexcept
{
  constexpr std::int64_t WEEK_SECONDS = std::int64_t{7} * 24 * 60 * 60;
  const std::int64_t nowMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(_now.time_since_epoch()).count();
  // How far ahead the moment is, in milliseconds, between none and a week, so that no far moment overflows.
  const std::int64_t aheadMilliseconds =
    std::clamp(_utcSeconds, (nowMilliseconds / 1000) - 1, (nowMilliseconds / 1000) + WEEK_SECONDS) * 1000 - nowMilliseconds;
  if (aheadMilliseconds <= 0)
    return _tick;
  // The first tick at or after it: rounded up.
  return _tick + static_cast<std::uint64_t>(((aheadMilliseconds * _ticksPerSecond) + 999) / 1000);
}

void Outpost::InProcessServer::Advance(std::chrono::nanoseconds _elapsedWallTime)
{
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: a started server runs its own ticks");
  const std::uint32_t ticks = m_tickHost.Advance(_elapsedWallTime);
  for (std::uint32_t i = 0; i < ticks; ++i)
    RunTick();
}

void Outpost::InProcessServer::Step()
{
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: a started server runs its own ticks");
  RunTick();
}

void Outpost::InProcessServer::Start()
{
  {
    const std::scoped_lock lock(m_seatMutex);
    if (m_started)
      throw Neuron::Exception("InProcessServer: the server has started already");
    // A world starts with its seats open, for its players to take when they come (ADR-078).
    for (const Connection& connection : m_connections)
    {
      if (connection.seat && !connection.quic && !m_isWorld)
        throw Neuron::Exception(std::format("InProcessServer: player {} has not taken its seat", connection.player.value));
    }
    m_started = true;
  }
  m_thread = std::jthread([this](const std::stop_token& _stop) { Run(_stop); });
}

void Outpost::InProcessServer::Run(const std::stop_token& _stop)
{
  auto last = std::chrono::steady_clock::now();
  try
  {
    // The thread sleeps on a waitable timer set for the next tick, and on an event that a request to stop signals, which
    // nothing else does (ADR-055). A high-resolution timer ends the wait when the tick is due, where a plain one, like
    // any other timed wait, ends on the system timer's next tick, 15.6 ms apart by default. Windows before 10 1803 makes
    // no high-resolution timer, and a plain one serves there.
    winrt::handle timer{CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)};
    if (!timer)
      timer.attach(winrt::check_pointer(CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS)));
    winrt::handle stopped;
    stopped.attach(winrt::check_pointer(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS)));
    // Declared after the event, so that it is gone before the event is.
    const std::stop_callback wake(_stop, [&stopped]() noexcept { (void)SetEvent(stopped.get()); });
    const std::array<HANDLE, 2> waits{stopped.get(), timer.get()};
    while (true)
    {
      // What is left of the wait for the next tick, rounded up to the timer's hundreds of nanoseconds. A due time that
      // has passed already is not waited for, as a timed wait would not wait for it.
      const auto remaining = std::chrono::ceil<HundredNanoseconds>(last + m_tickHost.UntilNextTick() - std::chrono::steady_clock::now());
      if (remaining.count() > 0)
      {
        // A negative due time is relative to now.
        LARGE_INTEGER due{};
        due.QuadPart = -remaining.count();
        winrt::check_bool(SetWaitableTimer(timer.get(), &due, 0, nullptr, nullptr, FALSE));
        if (WaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE, INFINITE) == WAIT_FAILED)
          winrt::throw_last_error();
      }
      if (_stop.stop_requested())
        return;
      const auto now = std::chrono::steady_clock::now();
      const std::uint32_t ticks = m_tickHost.Advance(now - last);
      last = now;
      for (std::uint32_t i = 0; i < ticks; ++i)
        RunTick();
    }
  }
  catch (...)
  {
    const std::scoped_lock lock(m_reportMutex);
    m_failure = std::current_exception();
  }
}

std::uint32_t Outpost::InProcessServer::TicksPerSecond() const noexcept
{
  return static_cast<std::uint32_t>(m_tuning.rules.tickHz);
}

std::vector<Outpost::TickTiming> Outpost::InProcessServer::TakeTickTimings()
{
  const std::scoped_lock lock(m_reportMutex);
  if (m_failure)
    std::rethrow_exception(m_failure);
  return std::exchange(m_tickTimings, {});
}

void Outpost::InProcessServer::RunTick()
{
  // What was timed between ticks, such as the graphs a structure's placement made match setup build, is no tick's.
  (void)m_profiler.Take();
  const auto started = std::chrono::steady_clock::now();
  std::vector<Command> commands;
  for (const Connection& connection : m_connections)
  {
    const std::scoped_lock lock(connection.channel->mutex);
    for (Command& command : connection.channel->commands)
    {
      // The connection says who sent it; what the client wrote there is never trusted (ADR-002). Nor is the tick of a
      // scheduled order's time of day, which the host makes of its moment here (ADR-080).
      command.player = connection.player;
      if (auto* schedule = std::get_if<ScheduleOrderCommand>(&command.order);
          schedule != nullptr && schedule->trigger.kind == ScheduledTriggerKind::TimeOfDay)
      {
        schedule->trigger.tick =
          TickOfMoment(schedule->trigger.utcSeconds, std::chrono::system_clock::now(), m_simulation.CurrentTick(), TicksPerSecond());
      }
      commands.push_back(std::move(command));
    }
    connection.channel->commands.clear();
  }

  // The stress scene's replacements and their orders come after the players'.
  if (m_stressLoad)
  {
    for (Command& command : m_stressLoad->TopUp(m_simulation))
      commands.push_back(std::move(command));
  }

  for (const Command& command : commands)
    m_commandLog.push_back({m_simulation.CurrentTick(), command});
  // A world's log keeps them before they are applied, so that a world killed after this tick comes back with them (ADR-077).
  if (m_world)
    m_world->Log(m_simulation.CurrentTick(), commands);
  // A rejected command changes nothing. The protocol cannot tell the client yet; that arrives with the task that needs it.
  (void)m_simulation.Tick(commands, &m_profiler);
  if (m_world && m_simulation.CurrentTick() % (std::uint64_t{WORLD_SAVE_SECONDS} * TicksPerSecond()) == 0)
    SaveWorld();

  // The players the server hosts, and the snapshot each answers once every player has been sent its own.
  struct HostedTurn
  {
    Connection* connection = nullptr;
    Snapshot snapshot;
    // Whether it plays its seat now, or watches its player play it.
    bool plays = true;
  };
  std::vector<HostedTurn> turns;
  {
    const ObservedPart part(&m_profiler, TickPart::Snapshots);
    for (Connection& connection : m_connections)
    {
      Snapshot snapshot = m_simulation.BuildSnapshot(connection.player);
      if (connection.seat)
      {
        // A seat not yet taken has nobody to send to.
        std::shared_ptr<Neuron::QuicChannel> quic;
        std::uint32_t takings = 0;
        {
          const std::scoped_lock lock(m_seatMutex);
          quic = connection.quic;
          takings = connection.takings;
        }
        // A seat's deputy comes with the controller that says when it plays (Host). While it does, the seat's report of what
        // its player misses grows, and the player's first snapshot once it plays again carries it (ADR-080).
        SeatController* control = nullptr;
        if (connection.hosted && connection.control.has_value())
          control = &*connection.control;
        const bool deputy = control != nullptr;
        bool plays = false;
        if (control != nullptr)
        {
          const bool gone = quic && quic->HasGone();
          plays = control->DeputyPlays(m_simulation.CurrentTick(), takings, gone);
          m_away.Take(snapshot, plays);
        }
        if (quic)
          quic->Send(EncodeMessage(snapshot));
        if (deputy)
          turns.push_back({.connection = &connection, .snapshot = std::move(snapshot), .plays = plays});
        continue;
      }
      if (connection.hosted)
      {
        turns.push_back({.connection = &connection, .snapshot = std::move(snapshot), .plays = true});
        continue;
      }
      const std::scoped_lock lock(connection.channel->mutex);
      connection.channel->snapshots.push_back(std::move(snapshot));
    }
  }
  {
    // Their orders wait in their connection's queue for the next tick, which logs and applies them as a client's.
    const ObservedPart part(&m_profiler, TickPart::Hosted);
    for (HostedTurn& turn : turns)
    {
      if (!turn.plays)
      {
        turn.connection->hosted->Watch(turn.snapshot);
        continue;
      }
      std::vector<Command> orders = turn.connection->hosted->Play(turn.snapshot);
      const std::scoped_lock lock(turn.connection->channel->mutex);
      std::ranges::move(orders, std::back_inserter(turn.connection->channel->commands));
    }
  }
  const TickTiming timing{.total = std::chrono::steady_clock::now() - started, .parts = m_profiler.Take()};
  const std::scoped_lock lock(m_reportMutex);
  m_tickTimings.push_back(timing);
}

void Outpost::InProcessServer::UseWorld(const std::filesystem::path& _folder, std::uint64_t _dataHash,
                                        std::optional<WorldFolder::Recovery> _recovery)
{
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: a started server cannot become a world");
  m_identity = {.seed = m_seed, .ticksPerSecond = TicksPerSecond(), .dataHash = _dataHash};
  if (_recovery)
  {
    m_away.Restore(DecodeWorld(_recovery->save, m_identity, m_simulation));
    m_commandLog.clear();
    // The commands logged since the save, each at the tick it was applied at, as the world that ran applied them.
    auto next = _recovery->commands.begin();
    while (next != _recovery->commands.end())
    {
      const std::uint64_t tick = m_simulation.CurrentTick();
      if (next->tick < tick)
        throw Neuron::Exception(
          std::format("The world in {} logged a command at tick {}, before its save's tick {}.", _folder.string(), next->tick, tick));
      std::vector<Command> commands;
      for (; next != _recovery->commands.end() && next->tick == tick; ++next)
        commands.push_back(next->command);
      for (const Command& command : commands)
        m_commandLog.push_back({tick, command});
      (void)m_simulation.Tick(commands);
    }
  }
  m_world = std::make_unique<WorldFolder>(_folder);
  SaveWorld();
}

void Outpost::InProcessServer::SaveWorld()
{
  m_world->Save(m_simulation.CurrentTick(), EncodeWorld(m_simulation, m_identity, m_away.Reports()));
  // The world's folder keeps the commands from here on.
  m_commandLog.clear();
}

void Outpost::InProcessServer::StartStressLoad()
{
  m_stressLoad.emplace(m_simulation, m_map, m_tuning);
}

std::unique_ptr<Outpost::Server> Outpost::CreateInProcessServer(const ServerDesc& _desc)
{
  return SetUpServer(ReadGameData(), _desc);
}

Outpost::ServerFactory Outpost::InProcessServerFactory()
{
  // Read and checked as CreateInProcessServer reads them, but once. Every server is given copies, so that none shares state
  // with another, and the data itself is never written again.
  auto data = std::make_shared<const GameData>(ReadGameData());
  return [data = std::move(data)](const ServerDesc& _desc) { return SetUpServer(*data, _desc); };
}