#include "pch.h"
#include "InProcessServer.h"

#include <algorithm>
#include <utility>

namespace
{
// Ticks run by one Advance at most: 250 ms of simulation at 20 Hz. After a longer stall the simulation drops the rest
// and resumes at its own pace (ADR-009).
constexpr std::uint32_t MAX_TICKS_PER_ADVANCE = 5;
// Where the server finds its tuning, under the package's Assets folder (ADR-008).
constexpr const wchar_t* TUNING_FILE = L"Data\\Tuning.json";
} // namespace

Outpost::LoopbackTransport::LoopbackTransport(std::shared_ptr<LoopbackChannel> _channel) noexcept
  : m_channel(std::move(_channel))
{
}

void Outpost::LoopbackTransport::Send(Command _command)
{
  m_channel->commands.push_back(std::move(_command));
}

std::vector<Outpost::Snapshot> Outpost::LoopbackTransport::Receive()
{
  return std::exchange(m_channel->snapshots, {});
}

Outpost::InProcessServer::InProcessServer(Tuning _tuning, const ServerDesc& _desc)
  : m_tuning(std::move(_tuning)),
    m_tickHost(static_cast<std::uint32_t>(m_tuning.rules.tickHz), MAX_TICKS_PER_ADVANCE),
    m_simulation(_desc.seed)
{
}

std::unique_ptr<Outpost::Transport> Outpost::InProcessServer::Connect(PlayerId _player)
{
  if (!_player.IsValid())
    throw Neuron::Exception("InProcessServer: a connection needs a player");
  if (std::ranges::any_of(m_connections, [_player](const Connection& _connection) { return _connection.player == _player; }))
    throw Neuron::Exception(std::format("InProcessServer: player {} is already connected", _player.value));

  auto channel = std::make_shared<LoopbackChannel>();
  m_connections.push_back({_player, channel});
  return std::make_unique<LoopbackTransport>(std::move(channel));
}

void Outpost::InProcessServer::Advance(std::chrono::nanoseconds _elapsedWallTime)
{
  const std::uint32_t ticks = m_tickHost.Advance(_elapsedWallTime);
  for (std::uint32_t i = 0; i < ticks; ++i)
    RunTick();
}

void Outpost::InProcessServer::RunTick()
{
  std::vector<Command> commands;
  for (const Connection& connection : m_connections)
  {
    for (Command& command : connection.channel->commands)
    {
      // The connection says who sent it; what the client wrote there is never trusted (ADR-002).
      command.player = connection.player;
      commands.push_back(std::move(command));
    }
    connection.channel->commands.clear();
  }

  for (const Command& command : commands)
    m_commandLog.push_back({m_simulation.CurrentTick(), command});
  // A rejected command changes nothing. The protocol cannot tell the client yet; that arrives with the task that needs it.
  (void)m_simulation.Tick(commands);

  for (const Connection& connection : m_connections)
    connection.channel->snapshots.push_back(m_simulation.BuildSnapshot(connection.player));
}

std::unique_ptr<Outpost::Server> Outpost::CreateInProcessServer(const ServerDesc& _desc)
{
  const Neuron::ByteBuffer bytes = Neuron::BinaryFile::ReadFile(TUNING_FILE);
  if (bytes.empty())
    throw Neuron::Exception("The tuning data, Assets\\Data\\Tuning.json, is missing or cannot be read.");
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return std::make_unique<InProcessServer>(LoadTuning(text), _desc);
}
