#include "pch.h"
#include "InProcessServer.h"

#include <algorithm>
#include <utility>

namespace
{
// Ticks run by one Advance at most: 250 ms of simulation at 20 Hz. After a longer stall the simulation drops the rest
// and resumes at its own pace (ADR-009).
constexpr std::uint32_t MAX_TICKS_PER_ADVANCE = 5;
// Where the server finds its data, under the package's Assets folder (ADR-008).
constexpr std::string_view TUNING_FILE = "Tuning.json";
constexpr std::string_view MAP_FILE = "Map.json";

std::string ReadDataFile(std::string_view _fileName)
{
  // The names above are ASCII, so widening them character by character is exact.
  const Neuron::ByteBuffer bytes = Neuron::BinaryFile::ReadFile(std::wstring(_fileName.begin(), _fileName.end()));
  if (bytes.empty())
    throw Neuron::Exception(std::format("The game data file Assets\\{} is missing or cannot be read.", _fileName));
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// Match setup on a server just made: the map is placed, and now every player's starting fleet (task 2.5), and the load a
// measurement run asks for.
std::unique_ptr<Outpost::Server> SetUpMatch(std::unique_ptr<Outpost::InProcessServer> _server, const Outpost::ServerDesc& _desc)
{
  _server->World().PlaceStartingBases(_server->MapData());
  if (_desc.measurementLoad)
    Outpost::PlaceMeasurementLoad(_server->World(), _server->MapData(), _server->TuningData());
  if (_desc.stressLoad)
    _server->StartStressLoad();
  // Match setup is over, and every structure it places stands: the graphs are built once, now, so that the first order of
  // the match does not pay for them (ADR-032).
  _server->PreparePathfinding();
  return _server;
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
    m_map(std::move(_map)),
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
}

void Outpost::InProcessServer::PreparePathfinding()
{
  // The same float a ship of the hull is given (MovementFor), since the graphs are kept by radius.
  for (const HullTuning& hull : m_tuning.hulls)
    m_simulation.PreparePathfinding(static_cast<float>(hull.footprintRadiusMeters));
  m_simulation.PreparePathfinding(static_cast<float>(m_tuning.constructor.footprintRadiusMeters));
}

std::unique_ptr<Outpost::Transport> Outpost::InProcessServer::Connect(PlayerId _player)
{
  if (!_player.IsValid())
    throw Neuron::Exception("InProcessServer: a connection needs a player");
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: players connect before the server starts");
  if (std::ranges::any_of(m_connections, [_player](const Connection& _connection) { return _connection.player == _player; }))
    throw Neuron::Exception(std::format("InProcessServer: player {} is already connected", _player.value));

  auto channel = std::make_shared<LoopbackChannel>();
  m_connections.push_back({_player, channel});
  return std::make_unique<LoopbackTransport>(std::move(channel));
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
  if (m_thread.joinable())
    throw Neuron::Exception("InProcessServer: the server has started already");
  m_thread = std::jthread([this](const std::stop_token& _stop) { Run(_stop); });
}

void Outpost::InProcessServer::Run(const std::stop_token& _stop)
{
  std::mutex sleeping;
  std::condition_variable_any wake;
  auto last = std::chrono::steady_clock::now();
  try
  {
    while (true)
    {
      {
        // Nothing wakes it early but a request to stop.
        std::unique_lock lock(sleeping);
        (void)wake.wait_until(lock, _stop, last + m_tickHost.UntilNextTick(), [] { return false; });
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
      // The connection says who sent it; what the client wrote there is never trusted (ADR-002).
      command.player = connection.player;
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
  // A rejected command changes nothing. The protocol cannot tell the client yet; that arrives with the task that needs it.
  (void)m_simulation.Tick(commands, &m_profiler);

  {
    const ObservedPart part(&m_profiler, TickPart::Snapshots);
    for (const Connection& connection : m_connections)
    {
      Snapshot snapshot = m_simulation.BuildSnapshot(connection.player);
      const std::scoped_lock lock(connection.channel->mutex);
      connection.channel->snapshots.push_back(std::move(snapshot));
    }
  }
  const TickTiming timing{.total = std::chrono::steady_clock::now() - started, .parts = m_profiler.Take()};
  const std::scoped_lock lock(m_reportMutex);
  m_tickTimings.push_back(timing);
}

void Outpost::InProcessServer::StartStressLoad()
{
  m_stressLoad.emplace(m_simulation, m_map, m_tuning);
}

std::unique_ptr<Outpost::Server> Outpost::CreateInProcessServer(const ServerDesc& _desc)
{
  return SetUpMatch(std::make_unique<InProcessServer>(LoadTuning(ReadDataFile(TUNING_FILE)), LoadMap(ReadDataFile(MAP_FILE)), _desc),
                    _desc);
}

Outpost::ServerFactory Outpost::InProcessServerFactory()
{
  // Read and checked as CreateInProcessServer reads them, but once. Every server is given copies, so that none shares state
  // with another, and the data itself is never written again.
  auto data = std::make_shared<const std::pair<Tuning, Map>>(LoadTuning(ReadDataFile(TUNING_FILE)), LoadMap(ReadDataFile(MAP_FILE)));
  return [data = std::move(data)](const ServerDesc& _desc)
  {
    const auto& [tuning, map] = *data;
    return SetUpMatch(std::make_unique<InProcessServer>(tuning, map, _desc), _desc);
  };
}