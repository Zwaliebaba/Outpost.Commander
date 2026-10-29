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
constexpr std::string_view MATCHUPS_FILE = "Matchups.json";
// Where a seat's player connects: the listener takes connections on the loopback address only (ADR-060).
constexpr std::string_view LOOPBACK_HOST = "127.0.0.1";
// How long Close waits for the overseer to hear how the server ends.
constexpr std::chrono::seconds CLOSING_WAIT{1};

void Refuse(Neuron::QuicChannel& _channel, Outpost::CloseReason _reason) noexcept
{
  _channel.Shutdown(static_cast<std::uint64_t>(_reason));
}

// The host a seat's player reaches a server listening on _address at, from the same machine: the loopback address for
// the loopback itself or every interface, and otherwise the address it listens on.
std::string ListenHost(std::string_view _address)
{
  if (_address.empty() || _address == "0.0.0.0" || _address == Neuron::QuicListener::EVERY_ADDRESS)
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
  // DataHash of the two files, which names them in a universe's save (ADR-077).
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
// measurement run asks for. A universe whose folder holds a save comes back as that universe instead, made with its seed
// (ADR-077).
std::unique_ptr<Outpost::InProcessServer> SetUpServer(const GameData& _data, Outpost::ServerDesc _desc)
{
  std::optional<Outpost::UniverseFolder::Recovery> recovery;
  if (!_desc.universe.empty())
  {
    recovery = Outpost::UniverseFolder::Recover(_desc.universe);
    if (recovery)
      _desc.seed = recovery->header.identity.seed;
  }
  auto server = std::make_unique<Outpost::InProcessServer>(_data.tuning, _data.map, _desc);
  if (!recovery)
  {
    // A universe has no end, and a player who loses restarts (ADR-084 decision 1); a save carries its rules with it.
    if (!_desc.universe.empty())
      server->SimulationState().UseUniverseRules();
    // A battle matchup's fleets fight with no bases (ADR-083).
    if (_desc.matchup.has_value())
    {
      const std::vector<Outpost::Matchup> matchups = Outpost::LoadMatchups(ReadDataFile(MATCHUPS_FILE), server->TuningData());
      if (*_desc.matchup >= matchups.size())
        throw Neuron::Exception(std::format("Matchups.json has {} matchups, and no matchup {}.", matchups.size(), *_desc.matchup + 1));
      server->SimulationState().PlaceMatchup(matchups[*_desc.matchup]);
    }
    else
      server->SimulationState().PlaceStartingBases(server->MapData());
    if (_desc.measurementLoad)
      Outpost::PlaceMeasurementLoad(server->SimulationState(), server->MapData(), server->TuningData());
    if (_desc.stressLoad)
      server->StartStressLoad();
  }
  if (!_desc.universe.empty())
    server->UseUniverse(_desc.universe, _data.hash, std::move(recovery));
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
  // A client that far ahead of the server's ticks is broken, as a QUIC client is that the server closes for it (ADR-009).
  if (m_channel->commands.size() >= COMMANDS_WAITING_LIMIT)
    throw Neuron::Exception(std::format("LoopbackTransport: {} commands wait for the server's ticks already", COMMANDS_WAITING_LIMIT));
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
    m_isUniverse(!_desc.universe.empty()),
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
  // Every match is played under fog of war (ADR-024), and a battle matchup in the open (ADR-083).
  if (!_desc.matchup.has_value())
    m_simulation.UseFog();
  // One player per start, each with the starting Ore and the starting designs saved (ADR-016 decision 3).
  for (size_t player = 0; player < m_map.starts.size(); ++player)
  {
    const PlayerId id{static_cast<std::uint32_t>(player + 1)};
    m_simulation.AddPlayer(id, m_tuning.rules.startingOre);
    m_simulation.SaveStartingDesigns(id, m_tuning);
  }
  // The pirates' outposts and the derelicts the seed placed, after the players, so that each player's designs are numbered as before. A
  // measurement run is not a match, and its scenes stay as they were measured.
  if (!_desc.measurementLoad && !_desc.stressLoad && !_desc.matchup.has_value())
  {
    m_simulation.PlacePirates(m_map);
    m_simulation.PlaceDerelicts(m_map);
  }
  // Last, since a client may connect as soon as it listens.
  if (_desc.quic)
  {
    // A universe keeps its certificate in its folder, so that a player pins it once (ADR-078). A client may send no more
    // than a hello until its hello has taken a seat (ADR-060).
    m_listener =
      std::make_unique<Neuron::QuicListener>(Neuron::QuicListener::Desc{.applicationProtocol = std::string(QUIC_APPLICATION_PROTOCOL),
                                                                        .address = _desc.quicAddress,
                                                                        .port = _desc.quicPort,
                                                                        .identity = _desc.universe,
                                                                        .messageLimitBytes = HELLO_LIMIT_BYTES},
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
      // A hello of another build may not decode at all, so its version and its messages' layout are read first (ADR-060).
      if (!commands)
      {
        const std::optional<std::uint32_t> version = PeekHelloVersion(_message);
        const std::optional<std::uint64_t> layout = PeekHelloLayout(_message);
        if (version.has_value() && (*version != PROTOCOL_VERSION || layout != WireLayoutHash()))
        {
          KeepJoin({.outcome = JoinOutcome::WrongVersion, .address = _peer.PeerAddress(), .layout = layout.value_or(0)});
          Refuse(_peer, CloseReason::WrongVersion);
          return;
        }
      }
      Message message = DecodeMessage(_message);
      if (commands)
      {
        auto* command = std::get_if<Command>(&message);
        const auto* held = std::get_if<HeldMessage>(&message);
        if (command == nullptr && held == nullptr)
        {
          KeepJoin({.outcome = JoinOutcome::Malformed, .player = seated, .address = _peer.PeerAddress()});
          Refuse(_peer, CloseReason::MalformedMessage);
          return;
        }
        // A connection the seat was taken from sends nothing more to the universe (ADR-078).
        if (!HoldsSeat(seated, &_peer))
          return;
        // The seat's next states go as changes of the newest state its client holds (ADR-098).
        if (held != nullptr)
        {
          const std::scoped_lock lock(commands->mutex);
          commands->held = std::max(commands->held.value_or(0), held->tick);
          return;
        }
        // The player is set from the seat when the tick takes the command; what the client wrote is never trusted
        // (ADR-002). A client so far ahead of the ticks is broken, and closed once the queue's lock is let go (ADR-009).
        bool broken = false;
        {
          const std::scoped_lock lock(commands->mutex);
          broken = commands->commands.size() >= COMMANDS_WAITING_LIMIT;
          if (!broken)
            commands->commands.push_back(std::move(*command));
        }
        if (broken)
        {
          KeepJoin({.outcome = JoinOutcome::Malformed, .player = seated, .address = _peer.PeerAddress()});
          Refuse(_peer, CloseReason::MalformedMessage);
        }
        return;
      }

      const auto* hello = std::get_if<HelloMessage>(&message);
      if (hello == nullptr)
      {
        KeepJoin({.outcome = JoinOutcome::Malformed, .address = _peer.PeerAddress()});
        Refuse(_peer, CloseReason::MalformedMessage);
        return;
      }
      if (hello->protocolVersion != PROTOCOL_VERSION || hello->layoutHash != WireLayoutHash())
      {
        KeepJoin({.outcome = JoinOutcome::WrongVersion, .address = _peer.PeerAddress(), .layout = hello->layoutHash});
        Refuse(_peer, CloseReason::WrongVersion);
        return;
      }
      commands = TakeSeat(hello->player, hello->token, connection.lock());
      if (!commands)
      {
        KeepJoin({.outcome = JoinOutcome::SeatRefused, .player = hello->player, .address = _peer.PeerAddress()});
        Refuse(_peer, CloseReason::SeatRefused);
        return;
      }
      seated = hello->player;
      KeepJoin({.outcome = JoinOutcome::Welcomed, .player = seated, .address = _peer.PeerAddress()});
      // A seated client sends commands, which may be longer than a hello (ADR-060).
      _peer.LimitMessages(SEATED_MESSAGE_LIMIT_BYTES);
    }
    catch (const Neuron::Exception&)
    {
      KeepJoin({.outcome = JoinOutcome::Malformed, .player = seated, .address = _peer.PeerAddress()});
      Refuse(_peer, CloseReason::MalformedMessage);
    }
  };
}

void Outpost::InProcessServer::KeepJoin(JoinRecord _join)
{
  const std::scoped_lock lock(m_reportMutex);
  m_joins.push_back(std::move(_join));
}

std::shared_ptr<Outpost::LoopbackChannel> Outpost::InProcessServer::TakeSeat(PlayerId _player, const SeatToken& _token,
                                                                             std::shared_ptr<Neuron::QuicChannel> _channel)
{
  std::shared_ptr<Neuron::QuicChannel> replaced;
  std::shared_ptr<LoopbackChannel> commands;
  {
    const std::scoped_lock lock(m_seatMutex);
    // A match's seats are taken before it starts; a universe's at any time (ADR-078).
    if ((m_started && !m_isUniverse) || !_channel)
      return nullptr;
    const auto seat =
      std::ranges::find_if(m_connections, [_player](const Connection& _connection) { return _connection.player == _player; });
    if (seat == m_connections.end() || !seat->seat || seat->token != _token)
      return nullptr;
    // The welcome goes before the seat has the channel, so that it is the first the client hears, ahead of the catalog and
    // the snapshot a running universe's next tick sends (ADR-060 decisions 5 and 9).
    _channel->Send(EncodeMessage(WelcomeMessage{.player = _player}));
    // The player's newest connection holds the seat, so that one whose connection dropped without the server knowing yet
    // takes it back.
    replaced = std::exchange(seat->quic, std::move(_channel));
    ++seat->takings;
    commands = seat->channel;
  }
  // The connection it had is told why it goes, outside the lock, which its own callbacks take.
  if (replaced)
  {
    KeepJoin({.outcome = JoinOutcome::Taken, .player = _player, .address = replaced->PeerAddress()});
    replaced->Shutdown(static_cast<std::uint64_t>(CloseReason::SeatTaken));
  }
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
  const std::uint64_t dropped = m_tickHost.DroppedTicks();
  const std::uint32_t ticks = m_tickHost.Advance(_elapsedWallTime);
  KeepDropped(m_tickHost.DroppedTicks() - dropped);
  for (std::uint32_t i = 0; i < ticks; ++i)
    RunTick();
}

void Outpost::InProcessServer::KeepDropped(std::uint64_t _ticks)
{
  if (_ticks == 0)
    return;
  // What a stall cost: ticks that never run, which the universe's log sums for the minute and Health for the second
  // (ADR-009, ADR-082, ADR-101).
  if (m_log)
    m_log->Dropped(_ticks);
  const std::scoped_lock lock(m_reportMutex);
  m_healthDropped += _ticks;
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
    // A universe starts with its seats open, for its players to take when they come (ADR-078).
    for (const Connection& connection : m_connections)
    {
      if (connection.seat && !connection.quic && !m_isUniverse)
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
      const std::uint64_t dropped = m_tickHost.DroppedTicks();
      const std::uint32_t ticks = m_tickHost.Advance(now - last);
      last = now;
      KeepDropped(m_tickHost.DroppedTicks() - dropped);
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

std::vector<Outpost::SeatFigures> Outpost::InProcessServer::TakeSeatFigures()
{
  const std::scoped_lock lock(m_reportMutex);
  std::vector<SeatFigures> figures(m_seatFigures.begin(), m_seatFigures.end());
  m_seatFigures.clear();
  return figures;
}

void Outpost::InProcessServer::RunTick()
{
  // What was timed between ticks, such as the graphs a structure's placement made match setup build, is no tick's.
  (void)m_profiler.Take();
  const auto started = std::chrono::steady_clock::now();
  // The listener lets go of the connections that have gone, on this thread and never MsQuic's, and closes the ones that
  // have not said hello in time (ADR-060).
  std::vector<std::string> silent;
  if (m_listener)
    silent = m_listener->Sweep(HELLO_TIMEOUT, static_cast<std::uint64_t>(CloseReason::MalformedMessage));
  // And the overseer's, whose comings and goings the universe's log writes (ADR-101), with the actions it asked for (ADR-104).
  std::vector<OverseerRecord> overseen;
  std::vector<OverseerAct> acts;
  if (m_overseer)
  {
    overseen = m_overseer->Sweep();
    acts = m_overseer->TakeActs();
  }
  std::vector<Command> commands;
  for (Connection& connection : m_connections)
  {
    const std::scoped_lock lock(connection.channel->mutex);
    // A connection's oldest commands, at most COMMANDS_PER_TICK of them; the rest wait for the next tick (ADR-009).
    std::vector<Command>& waiting = connection.channel->commands;
    const std::size_t taken = std::min(waiting.size(), COMMANDS_PER_TICK);
    connection.orders = static_cast<std::uint32_t>(taken);
    for (Command& command : std::span(waiting).first(taken))
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
    waiting.erase(waiting.begin(), waiting.begin() + static_cast<std::ptrdiff_t>(taken));
  }

  // The stress scene's replacements and their orders come after the players'.
  if (m_stressLoad)
  {
    for (Command& command : m_stressLoad->TopUp(m_simulation))
      commands.push_back(std::move(command));
  }

  for (const Command& command : commands)
    m_commandLog.push_back({m_simulation.CurrentTick(), command});
  // A universe's log keeps them before they are applied, so that a universe killed after this tick comes back with them (ADR-077).
  if (m_folder)
    m_folder->Log(m_simulation.CurrentTick(), commands);
  // A refused command changes nothing, and its player hears why in its snapshot (ADR-094).
  (void)m_simulation.Tick(commands, &m_profiler);
  // The overseer's actions never reach the simulation (ADR-104). A save it asks for is the tick's, as one due is.
  bool saveAsked = false;
  for (const OverseerAct& act : acts)
    saveAsked = Act(act.action, saveAsked) || saveAsked;
  if (m_folder && (saveAsked || m_simulation.CurrentTick() % (std::uint64_t{UNIVERSE_SAVE_SECONDS} * TicksPerSecond()) == 0))
    SaveUniverse();
  if (saveAsked)
    m_askedSaveTick = m_simulation.CurrentTick();

  // The players the server hosts, and the snapshot each answers once every player has been sent its own.
  struct HostedTurn
  {
    Connection* connection = nullptr;
    Snapshot snapshot;
    // Whether it plays its seat now, or watches its player play it.
    bool plays = true;
  };
  std::vector<HostedTurn> turns;
  bool viewDue = false;
  std::optional<Snapshot> view;
  {
    const ObservedPart part(&m_profiler, TickPart::Snapshots);
    // Every connection's snapshot, and who plays its seat, first, for the universe's log to read them all.
    struct Prepared
    {
      Connection* connection = nullptr;
      Snapshot snapshot;
      SeatPlay play = SeatPlay::Player;
      std::shared_ptr<Neuron::QuicChannel> quic;
      std::uint32_t takings = 0;
      std::optional<Neuron::QuicStatistics> figures;
    };
    // Each seat's connection, every SEAT_FIGURES_SECONDS of ticks (ADR-004 decision 6).
    const bool sample = m_simulation.CurrentTick() % (std::uint64_t{SEAT_FIGURES_SECONDS} * TicksPerSecond()) == 0;
    // What came of the connections since the last tick: those MsQuic's threads kept, the silent ones the listener closed,
    // and each seat's connection seen gone below, which the server did not close itself (ADR-082 decision 1).
    std::vector<JoinRecord> joins;
    {
      const std::scoped_lock lock(m_reportMutex);
      joins = std::exchange(m_joins, {});
    }
    for (std::string& address : silent)
      joins.push_back({.outcome = JoinOutcome::Silent, .address = std::move(address)});
    const std::uint64_t tick = m_simulation.CurrentTick();
    std::vector<Prepared> prepared;
    prepared.reserve(m_connections.size());
    for (Connection& connection : m_connections)
    {
      Prepared& next =
        prepared.emplace_back(Prepared{.connection = &connection, .snapshot = m_simulation.BuildSnapshot(connection.player)});
      // The second's commands applied for the seat and refused it, which the server tells it of as events (ADR-094).
      connection.secondOrders += connection.orders;
      connection.secondRefused +=
        static_cast<std::uint32_t>(std::ranges::count(next.snapshot.events, EventKind::CommandRefused, &EventView::kind));
      if (connection.seat)
      {
        // A seat not yet taken has nobody to send to.
        {
          const std::scoped_lock lock(m_seatMutex);
          next.quic = connection.quic;
          next.takings = connection.takings;
        }
        // Its link, and since when it has been so, by the connection that holds it now.
        const SeatLink link = !next.quic ? SeatLink::Open : (next.quic->HasGone() ? SeatLink::Gone : SeatLink::Online);
        if (link != connection.link || next.takings != connection.linkTaking)
        {
          connection.link = link;
          connection.linkTick = tick;
          connection.linkTaking = next.takings;
        }
        // A seat's deputy comes with the controller that says when it plays (Host). While it does, the seat's report of what
        // its player misses grows, and the player's first snapshot once it plays again carries it (ADR-080).
        SeatController* control = nullptr;
        if (connection.hosted && connection.control.has_value())
          control = &*connection.control;
        if (control != nullptr)
        {
          const bool gone = next.quic && next.quic->HasGone();
          const bool plays = control->DeputyPlays(m_simulation.CurrentTick(), next.takings, gone);
          m_away.Take(next.snapshot, plays);
          next.play = plays ? SeatPlay::Deputy : SeatPlay::Player;
        }
        if (sample && next.quic)
          next.figures = next.quic->Statistics();
        // Its connection gone, once a taking: closed by the game with its code, lost without a word, or closed by the channel
        // for a broken framing. One the server closed itself was written as it refused it, and one a newer connection took
        // is no longer the seat's.
        if (next.quic && next.quic->HasGone() && connection.goneTaking != next.takings)
        {
          connection.goneTaking = next.takings;
          const std::optional<std::uint64_t> local = next.quic->LocalErrorCode();
          const std::optional<std::uint64_t> code = next.quic->PeerErrorCode();
          if (local == Neuron::FRAMING_ERROR_CODE)
            joins.push_back({.outcome = JoinOutcome::Malformed, .player = connection.player, .address = next.quic->PeerAddress()});
          else if (!local.has_value())
          {
            joins.push_back({.outcome = code.has_value() ? JoinOutcome::Closed : JoinOutcome::Lost,
                             .player = connection.player,
                             .address = next.quic->PeerAddress(),
                             .code = code.value_or(0)});
          }
        }
      }
      else if (connection.hosted)
        next.play = SeatPlay::Ai;
    }
    for (const JoinRecord& join : joins)
      ++m_joinCounts[static_cast<std::size_t>(join.outcome)];
    if (m_log)
    {
      for (const JoinRecord& join : joins)
        m_log->Joined(m_simulation.CurrentTick(), join);
      for (const OverseerRecord& record : overseen)
        m_log->Oversaw(m_simulation.CurrentTick(), record);
      for (const OverseerAct& act : acts)
        m_log->Acted(m_simulation.CurrentTick(), act);
      for (const WrittenSave& written : m_folder->TakeWritten())
      {
        m_log->Written(written.tick, written.write);
        // The overseer hears of the save it asked for once it is written.
        if (written.tick == m_askedSaveTick && m_overseer)
        {
          m_overseer->SendResult({.action = OverseerAction::SaveNow,
                                  .done = true,
                                  .wordsUtf8 = std::format("Saved at tick {}, written in {:.1f} ms.", written.tick,
                                                           static_cast<double>(written.write.count()) / 1000.0)});
          m_askedSaveTick.reset();
        }
      }
      std::vector<SeatTick> seats;
      seats.reserve(prepared.size());
      for (const Prepared& seat : prepared)
      {
        seats.push_back({.player = seat.connection->player,
                         .play = seat.play,
                         .snapshot = &seat.snapshot,
                         .connection = seat.figures,
                         .orders = seat.connection->orders});
      }
      m_log->Record(m_simulation.CurrentTick(), seats);
    }
    if (sample)
    {
      const std::scoped_lock lock(m_reportMutex);
      for (const Prepared& seat : prepared)
      {
        if (seat.figures.has_value())
          m_seatFigures.push_back({.tick = m_simulation.CurrentTick(), .player = seat.connection->player, .connection = *seat.figures});
      }
      while (m_seatFigures.size() > SEAT_FIGURES_KEPT)
        m_seatFigures.pop_front();
    }
    // What Health says of the seats, kept each second of ticks with their figures, while their snapshots are whole (ADR-101).
    if (sample)
    {
      KeptSecond second{.tick = tick, .joins = m_joinCounts, .connections = static_cast<std::uint32_t>(ListenerConnections())};
      if (m_log)
        second.battles = m_log->Battles();
      for (const Prepared& seat : prepared)
      {
        Connection& connection = *seat.connection;
        const Snapshot& snapshot = seat.snapshot;
        SeatHealth& health = second.seats.emplace_back(
          SeatHealth{.player = connection.player,
                     .play = seat.play,
                     .link = connection.link,
                     .linkTick = connection.linkTick,
                     .orders = std::exchange(connection.secondOrders, 0u),
                     .refused = std::exchange(connection.secondRefused, 0u),
                     .ore = snapshot.ore,
                     .incomeHundredthsPerSecond = snapshot.oreIncomeHundredthsPerSecond,
                     .commandPoints = snapshot.commandPoints,
                     .fleetCap = snapshot.fleetCap,
                     .nodes = static_cast<std::int32_t>(std::ranges::count(snapshot.sectors, connection.player, &SectorView::holder)),
                     .researched = static_cast<std::int32_t>(
                       std::ranges::count_if(snapshot.research, [](const ResearchTopicView& _topic) { return _topic.researched; })),
                     .restartTick = snapshot.restartTick});
        // Its player still plays a seat whose link has gone, until its deputy takes it (ADR-079 decision 4).
        if (connection.link == SeatLink::Gone && seat.play == SeatPlay::Player && connection.control.has_value())
          health.deputyTick = connection.linkTick + (std::uint64_t{DEPUTY_DELAY_SECONDS} * TicksPerSecond());
        if (seat.figures.has_value())
          health.connection = FiguresOf(*seat.figures);
        // What its player misses while its deputy plays it, so far (ADR-080 decision 7).
        const std::vector<std::pair<PlayerId, AwayReport>>& reports = m_away.Reports().reports;
        if (const auto report = std::ranges::find(reports, connection.player, &std::pair<PlayerId, AwayReport>::first);
            report != reports.end())
          health.away = report->second;
      }
      const std::scoped_lock lock(m_reportMutex);
      m_second = std::move(second);
    }
    // The overseer's view, every tick of OVERSEER_VIEWS_PER_SECOND: full sight, or a copy of the snapshot of the seat it watches.
    viewDue = m_overseer && tick % std::max<std::uint64_t>(1, TicksPerSecond() / OVERSEER_VIEWS_PER_SECOND) == 0 && m_overseer->Held();
    if (viewDue)
    {
      const PlayerId sight = m_overseer->Sight();
      const auto watched = std::ranges::find_if(prepared, [sight](const Prepared& _seat) { return _seat.connection->player == sight; });
      if (sight.IsValid() && watched != prepared.end())
        view = watched->snapshot;
    }
    for (Prepared& seat : prepared)
    {
      Connection& connection = *seat.connection;
      if (connection.seat)
      {
        // A seat whose connection has gone is sent nothing more, rather than having it queued (ADR-096).
        if (seat.quic && !seat.quic->HasGone())
        {
          // The catalogue crosses before the seat's first state and again when it has changed (ADR-060 decision 9), and the
          // tick's events after it, reliably and in order; the state crosses on its own, and gives way to a newer one
          // (ADR-096). Then what was taken goes back, for a seat's deputy.
          CatalogMessage catalog = TakeCatalog(seat.snapshot);
          std::vector<std::byte> encoded = EncodeMessage(catalog);
          if (connection.catalogTaking != seat.takings || encoded != connection.catalogSent)
          {
            if (connection.catalogTaking != seat.takings)
            {
              connection.orderedSent = 0;
              connection.statesSent.Clear();
            }
            seat.quic->Send(encoded);
            connection.catalogSent = std::move(encoded);
            connection.catalogTaking = seat.takings;
            ++connection.orderedSent;
          }
          EventsMessage events = TakeEvents(seat.snapshot);
          if (HasAny(events))
          {
            seat.quic->Send(EncodeMessage(events));
            ++connection.orderedSent;
          }
          // The state goes as a change of the newest the seat's client holds, when the server still has it (ADR-098).
          std::optional<std::uint64_t> held;
          {
            const std::scoped_lock lock(connection.channel->mutex);
            held = connection.channel->held;
          }
          StateMessage state{.orderedSent = connection.orderedSent, .snapshot = std::move(seat.snapshot)};
          if (seat.quic->SendNewest(connection.statesSent.Encode(state, held)))
            connection.statesSent.Keep(state.snapshot);
          seat.snapshot = std::move(state.snapshot);
          if (connection.hosted)
          {
            PutEvents(std::move(events), seat.snapshot);
            PutCatalog(std::move(catalog), seat.snapshot);
          }
        }
        if (connection.hosted && connection.control.has_value())
          turns.push_back({.connection = &connection, .snapshot = std::move(seat.snapshot), .plays = seat.play == SeatPlay::Deputy});
        continue;
      }
      if (connection.hosted)
      {
        turns.push_back({.connection = &connection, .snapshot = std::move(seat.snapshot), .plays = true});
        continue;
      }
      const std::scoped_lock lock(connection.channel->mutex);
      connection.channel->snapshots.push_back(std::move(seat.snapshot));
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
      // A hosted player as far ahead of the ticks as a broken client is a fault of the server's own (ADR-009, ADR-025).
      if (turn.connection->channel->commands.size() > COMMANDS_WAITING_LIMIT)
        throw Neuron::Exception(std::format("InProcessServer: player {}'s hosted player has more than {} orders waiting",
                                            turn.connection->player.value, COMMANDS_WAITING_LIMIT));
    }
  }
  if (viewDue)
  {
    const ObservedPart part(&m_profiler, TickPart::Overseer);
    m_overseer->SendView(view.has_value() ? std::move(*view) : m_simulation.BuildFullSight());
  }
  const TickTiming timing{.total = std::chrono::steady_clock::now() - started, .parts = m_profiler.Take()};
  // The minute's longest tick, and its dropped ticks, as the minute ends (ADR-082 decision 1).
  if (m_log)
    m_log->Timed(m_simulation.CurrentTick(), timing);
  // Every record the tick wrote, to the overseer as it was written (ADR-101).
  if (m_overseer && m_log)
    m_overseer->SendRecords(m_log->TakeLines());
  const std::scoped_lock lock(m_reportMutex);
  m_tickTimings.push_back(timing);
}

void Outpost::InProcessServer::UseUniverse(const std::filesystem::path& _folder, std::uint64_t _dataHash,
                                           std::optional<UniverseFolder::Recovery> _recovery)
{
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: a started server cannot become a universe");
  m_identity = {.seed = m_seed, .ticksPerSecond = TicksPerSecond(), .dataHash = _dataHash};
  std::optional<std::uint64_t> savedTick;
  std::size_t replayed = 0;
  if (_recovery)
  {
    savedTick = _recovery->header.tick;
    replayed = _recovery->commands.size();
    m_away.Restore(DecodeUniverse(_recovery->save, m_identity, m_simulation));
    m_commandLog.clear();
    // The commands logged since the save, each at the tick it was applied at, as the universe that ran applied them.
    auto next = _recovery->commands.begin();
    while (next != _recovery->commands.end())
    {
      const std::uint64_t tick = m_simulation.CurrentTick();
      if (next->tick < tick)
        throw Neuron::Exception(
          std::format("The universe in {} logged a command at tick {}, before its save's tick {}.", _folder.string(), next->tick, tick));
      std::vector<Command> commands;
      for (; next != _recovery->commands.end() && next->tick == tick; ++next)
        commands.push_back(next->command);
      for (const Command& command : commands)
        m_commandLog.push_back({tick, command});
      (void)m_simulation.Tick(commands);
    }
  }
  m_universeFolder = _folder;
  m_recoveredSaveTick = savedTick;
  m_replayedCommands = replayed;
  m_folder = std::make_unique<UniverseFolder>(_folder);
  m_logFile.open(_folder / UNIVERSE_LOG_FILE, std::ios::app);
  if (!m_logFile)
    throw Neuron::Exception(std::format("The universe's log in {} cannot be written.", _folder.string()));
  m_log.emplace(m_logFile, TicksPerSecond());
  m_log->Started(m_simulation.CurrentTick(), savedTick, replayed);
  SaveUniverse();
}

void Outpost::InProcessServer::ReplaceHosted(PlayerId _player, std::unique_ptr<HostedPlayer> _hosted)
{
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: a started server's hosted players are its own");
  const auto connection = std::ranges::find(m_connections, _player, &Connection::player);
  if (connection == m_connections.end() || !connection->hosted || !_hosted)
    throw Neuron::Exception(std::format("InProcessServer: player {} is not hosted", _player.value));
  connection->hosted = std::move(_hosted);
  const std::scoped_lock lock(connection->channel->mutex);
  connection->channel->commands.clear();
}

void Outpost::InProcessServer::SaveUniverse()
{
  // How long the save holds up the server's thread is its encoding; the folder writes it on a thread of its own
  // (ADR-077 decision 7).
  const auto started = std::chrono::steady_clock::now();
  std::vector<std::byte> save = EncodeUniverse(m_simulation, m_identity, m_away.Reports());
  const auto encode = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
  const std::size_t bytes = save.size();
  m_folder->Save(m_simulation.CurrentTick(), std::move(save));
  m_lastSaveTick.store(m_simulation.CurrentTick());
  if (m_log)
    m_log->Saved(m_simulation.CurrentTick(), bytes, encode);
  // The universe's folder keeps the commands from here on.
  m_commandLog.clear();
}

bool Outpost::InProcessServer::Act(const ActionMessage& _action, bool _saveAsked)
{
  const auto answer = [&](bool _done, std::string _words)
  { m_overseer->SendResult({.action = _action.action, .seat = _action.seat, .done = _done, .wordsUtf8 = std::move(_words)}); };
  switch (_action.action)
  {
  case OverseerAction::SaveNow:
    // One save asked for at a time: the next waits until the folder has written it.
    if (m_askedSaveTick.has_value() || _saveAsked)
    {
      answer(false,
             std::format("The save asked for at tick {} is still being written.", m_askedSaveTick.value_or(m_simulation.CurrentTick())));
      return false;
    }
    return true;
  case OverseerAction::Stop:
    m_stopAsked.store(true);
    answer(true, "The server is stopping.");
    return false;
  case OverseerAction::DropLink:
  case OverseerAction::NewToken:
    break;
  }
  // The link took only a player's seat of the universe's (OverseerLink::MayAsk).
  const auto seat = std::ranges::find_if(m_connections, [&_action](const Connection& _connection)
                                         { return _connection.seat && _connection.player == _action.seat; });
  if (seat == m_connections.end())
  {
    answer(false, std::format("Player {} has no seat here.", _action.seat.value));
    return false;
  }
  if (_action.action == OverseerAction::DropLink)
  {
    if (!DropLink(*seat, _action.deputyAtOnce))
      answer(false, std::format("Seat {} has no link to drop.", _action.seat.value));
    else if (_action.deputyAtOnce)
      answer(true, std::format("Seat {}'s link dropped; its deputy plays it now.", _action.seat.value));
    else
      answer(true, std::format("Seat {}'s link dropped; its deputy plays it in {} s unless its player joins again.", _action.seat.value,
                               DEPUTY_DELAY_SECONDS));
    return false;
  }
  // A new token: written into the universe's files first, so that the server takes it only once they hold it, and then the
  // link of whoever joined with the old one dropped, the seat's deputy playing it at once (owner, 2026-10-28).
  if (!m_listener)
  {
    answer(false, "The server takes no player over QUIC.");
    return false;
  }
  const SeatToken token = NewSeatToken();
  std::filesystem::path joinFile;
  try
  {
    joinFile = WriteSeatToken(m_universeFolder, _action.seat, token, m_listener->Certificate());
  }
  catch (const Neuron::Exception& error)
  {
    answer(false, error.what());
    return false;
  }
  {
    const std::scoped_lock lock(m_seatMutex);
    seat->token = token;
  }
  (void)DropLink(*seat, true);
  const std::u8string file = joinFile.u8string();
  answer(true, std::format("Seat {} has a new token, and its join file is written again: {}", _action.seat.value,
                           std::string(reinterpret_cast<const char*>(file.data()), file.size())));
  return false;
}

bool Outpost::InProcessServer::DropLink(Connection& _connection, bool _deputyAtOnce)
{
  std::shared_ptr<Neuron::QuicChannel> quic;
  std::uint32_t takings = 0;
  {
    const std::scoped_lock lock(m_seatMutex);
    quic = _connection.quic;
    takings = _connection.takings;
  }
  if (!quic || quic->HasGone())
    return false;
  // The server closed it itself, so the tick that sees it gone writes nothing more of it.
  KeepJoin({.outcome = JoinOutcome::Dropped, .player = _connection.player, .address = quic->PeerAddress()});
  quic->Shutdown(static_cast<std::uint64_t>(CloseReason::DroppedByOwner));
  if (_connection.control.has_value())
    _connection.control->Dropped(m_simulation.CurrentTick(), takings, _deputyAtOnce);
  return true;
}

Outpost::ServerAddress Outpost::InProcessServer::Oversee(const OverseerDesc& _desc, std::vector<OverseerSeat> _seats)
{
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: the overseer's link is made before the server starts");
  if (!m_folder || !m_log)
    throw Neuron::Exception("InProcessServer: only a universe's server has an overseer");
  if (m_overseer)
    throw Neuron::Exception("InProcessServer: the server has its overseer's link already");
  // The sectors' shapes and the asteroid fields never change, so the welcome carries them once, as a catalogue does (ADR-060
  // decision 9).
  Snapshot sight = m_simulation.BuildFullSight();
  CatalogMessage catalog = TakeCatalog(sight);
  const std::u8string folder = m_universeFolder.u8string();
  const OverseerWelcomeMessage welcome{.wireLayoutHash = WireLayoutHash(),
                                       .protocolVersion = PROTOCOL_VERSION,
                                       .stateVersion = UNIVERSE_STATE_VERSION,
                                       .ticksPerSecond = TicksPerSecond(),
                                       .folderUtf8 = std::string(reinterpret_cast<const char*>(folder.data()), folder.size()),
                                       .seats = std::move(_seats),
                                       .sectors = std::move(catalog.sectors),
                                       .fields = std::move(catalog.fields),
                                       .recoveredSaveTick = m_recoveredSaveTick,
                                       .replayedCommands = m_replayedCommands,
                                       .saveSeconds = UNIVERSE_SAVE_SECONDS,
                                       .dayRecords = {}};
  // The day's records of the day of universe time before the tick the universe stands at (owner, 2026-10-28).
  const std::uint64_t now = m_simulation.CurrentTick();
  const std::uint64_t day = DAY_RECORDS_SECONDS * TicksPerSecond();
  m_overseer = std::make_unique<OverseerLink>(_desc, m_universeFolder, welcome,
                                              UniverseLog::ReadDayRecords(m_universeFolder / UNIVERSE_LOG_FILE, now > day ? now - day : 0),
                                              UniverseLog::ReadLastRecords(m_universeFolder / UNIVERSE_LOG_FILE, OVERSEER_RECORDS_KEPT));
  m_log->KeepLines();
  return {.host = ListenHost(_desc.address), .port = m_overseer->Port(), .certificate = m_overseer->Certificate(), .token = _desc.token};
}

Outpost::HealthMessage Outpost::InProcessServer::TakeHealth(const Neuron::ProcessFigures& _process)
{
  const std::vector<TickTiming> timings = TakeTickTimings();
  HealthMessage health{
    .wallMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()};
  {
    const std::scoped_lock lock(m_reportMutex);
    health.tick = m_second.tick;
    health.seats = m_second.seats;
    health.battles = m_second.battles;
    health.joins = m_second.joins;
    health.connections = m_second.connections;
    health.droppedTicks = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(std::exchange(m_healthDropped, std::uint64_t{0}), std::numeric_limits<std::uint32_t>::max()));
  }
  // Each tick's time and the parts summed, in whole microseconds, and the largest part of the longest tick.
  const auto microseconds = [](std::chrono::nanoseconds _time)
  { return static_cast<std::uint64_t>(std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::microseconds>(_time).count())); };
  std::array<std::uint64_t, TICK_PART_COUNT> parts{};
  std::optional<std::chrono::nanoseconds> longest;
  for (const TickTiming& timing : timings)
  {
    health.tickMicroseconds.push_back(
      static_cast<std::uint32_t>(std::min<std::uint64_t>(microseconds(timing.total), std::numeric_limits<std::uint32_t>::max())));
    for (std::size_t part = 0; part < TICK_PART_COUNT; ++part)
      parts[part] += microseconds(timing.parts[part]);
    if (!longest.has_value() || timing.total > *longest)
    {
      longest = timing.total;
      health.longestPart = static_cast<TickPart>(std::ranges::max_element(timing.parts) - timing.parts.begin());
    }
  }
  for (std::size_t part = 0; part < TICK_PART_COUNT; ++part)
    health.partMicroseconds[part] =
      static_cast<std::uint32_t>(std::min<std::uint64_t>(parts[part], std::numeric_limits<std::uint32_t>::max()));
  health.process = {.workingSetBytes = _process.workingSetBytes,
                    .privateBytes = _process.privateBytes,
                    .cpuHundredths = _process.cpuHundredths,
                    .threads = _process.threads,
                    .handles = _process.handles,
                    .uptimeSeconds = static_cast<std::uint64_t>(std::max<std::int64_t>(0, _process.uptime.count())),
                    .diskFreeBytes = _process.diskFreeBytes};
  return health;
}

void Outpost::InProcessServer::Report(const Neuron::ProcessFigures& _process)
{
  const HealthMessage health = TakeHealth(_process);
  if (m_overseer && m_overseer->Held())
    m_overseer->SendHealth(health);
}

void Outpost::InProcessServer::Close(ServerEnding _ending, std::string _message)
{
  if (m_overseer)
    m_overseer->SendClosing({.ending = _ending, .messageUtf8 = std::move(_message), .lastSaveTick = m_lastSaveTick.load()}, CLOSING_WAIT);
}

void Outpost::InProcessServer::StartStressLoad()
{
  m_stressLoad.emplace(m_simulation, m_map, m_tuning);
}

std::unique_ptr<Outpost::Server> Outpost::CreateInProcessServer(const ServerDesc& _desc)
{
  return SetUpServer(ReadGameData(), _desc);
}

std::unique_ptr<Outpost::InProcessServer> Outpost::CreateUniverseServer(const ServerDesc& _desc)
{
  return SetUpServer(ReadGameData(), _desc);
}

std::vector<Outpost::Matchup> Outpost::ReadPackagedMatchups()
{
  return LoadMatchups(ReadDataFile(MATCHUPS_FILE), LoadTuning(ReadDataFile(TUNING_FILE)));
}

Outpost::ServerFactory Outpost::InProcessServerFactory()
{
  // Read and checked as CreateInProcessServer reads them, but once. Every server is given copies, so that none shares state
  // with another, and the data itself is never written again.
  auto data = std::make_shared<const GameData>(ReadGameData());
  return [data = std::move(data)](const ServerDesc& _desc) { return SetUpServer(*data, _desc); };
}