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
  // One player per start, each with the starting Ore and the starting designs saved (design §5, §7).
  for (size_t player = 0; player < m_map.starts.size(); ++player)
  {
    const PlayerId id{static_cast<std::uint32_t>(player + 1)};
    m_simulation.AddPlayer(id, m_tuning.rules.startingOre);
    m_simulation.SaveStartingDesigns(id, m_tuning);
  }
  PreparePathfinding();
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

std::vector<std::chrono::nanoseconds> Outpost::InProcessServer::TakeTickDurations()
{
  const std::scoped_lock lock(m_reportMutex);
  if (m_failure)
    std::rethrow_exception(m_failure);
  return std::exchange(m_tickDurations, {});
}

void Outpost::InProcessServer::RunTick()
{
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
  (void)m_simulation.Tick(commands);

  for (const Connection& connection : m_connections)
  {
    Snapshot snapshot = m_simulation.BuildSnapshot(connection.player);
    const std::scoped_lock lock(connection.channel->mutex);
    connection.channel->snapshots.push_back(std::move(snapshot));
  }
  const auto duration = std::chrono::steady_clock::now() - started;
  const std::scoped_lock lock(m_reportMutex);
  m_tickDurations.push_back(duration);
}

void Outpost::InProcessServer::StartStressLoad()
{
  m_stressLoad.emplace(m_simulation, m_map, m_tuning);
  // Its structures block movement, so the graphs are built again now rather than in the first tick.
  PreparePathfinding();
}

std::unique_ptr<Outpost::Server> Outpost::CreateInProcessServer(const ServerDesc& _desc)
{
  auto server = std::make_unique<InProcessServer>(LoadTuning(ReadDataFile(TUNING_FILE)), LoadMap(ReadDataFile(MAP_FILE)), _desc);
  // Match setup: the map is placed, and now every player's starting fleet (task 2.5).
  server->World().PlaceStartingBases(server->MapData());
  if (_desc.measurementLoad)
    PlaceMeasurementLoad(server->World(), server->MapData(), server->TuningData());
  if (_desc.stressLoad)
    server->StartStressLoad();
  return server;
}