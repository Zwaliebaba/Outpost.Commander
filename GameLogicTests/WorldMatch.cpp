#include "pch.h"

#include "WorldMatch.h"

namespace GameLogicTests
{
TemporaryFolder::TemporaryFolder()
  : m_path(std::filesystem::temp_directory_path() /
           std::format("OutpostWorldTests-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(), sm_made++))
{
  std::filesystem::create_directories(m_path);
}

TemporaryFolder::~TemporaryFolder()
{
  try
  {
    std::error_code ignored;
    std::filesystem::remove_all(m_path, ignored);
  }
  catch (...)
  {
    Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage("A test's temporary folder could not be removed.");
  }
}

const std::filesystem::path& TemporaryFolder::Path() const noexcept
{
  return m_path;
}

WorldMatch::WorldMatch(std::uint64_t _seed, const std::filesystem::path& _world)
  : m_recovery(_world.empty() ? std::nullopt : Outpost::WorldFolder::Recover(_world)),
    m_seed(m_recovery.has_value() ? m_recovery->header.identity.seed : _seed),
    m_tuning(Outpost::LoadTuning(ReadRepositoryTuning())),
    m_server(m_tuning, Outpost::LoadMap(ReadRepositoryMap()), {.seed = m_seed}),
    m_blue(m_server.Connect(BLUE)),
    m_red(m_server.Connect(RED)),
    m_blueAi(Settings(), m_server.TicksPerSecond()),
    m_redAi(Settings(), m_server.TicksPerSecond())
{
  if (!m_recovery)
    m_server.World().PlaceStartingBases(m_server.MapData());
  if (!_world.empty())
    m_server.UseWorld(_world, RepositoryDataHash(), std::exchange(m_recovery, std::nullopt));
  m_server.PreparePathfinding();
}

Outpost::InProcessServer& WorldMatch::Server() noexcept
{
  return m_server;
}

const Outpost::Tuning& WorldMatch::TuningData() const noexcept
{
  return m_tuning;
}

Outpost::WorldIdentity WorldMatch::Identity() const
{
  return {.seed = m_seed, .ticksPerSecond = m_server.TicksPerSecond(), .dataHash = RepositoryDataHash()};
}

std::uint64_t WorldMatch::RepositoryDataHash()
{
  const std::string tuning = ReadRepositoryTuning();
  const std::string map = ReadRepositoryMap();
  const std::array<std::string_view, 2> texts{tuning, map};
  return Outpost::DataHash(texts);
}

void WorldMatch::Run(double _seconds)
{
  const std::uint32_t tps = m_server.TicksPerSecond();
  const auto ticks = static_cast<std::uint64_t>(_seconds * tps);
  for (std::uint64_t tick = 0; tick < ticks; ++tick)
  {
    m_server.Advance(std::chrono::nanoseconds(1'000'000'000 / tps));
    Answer(*m_blue, m_blueAi);
    Answer(*m_red, m_redAi);
  }
}

void WorldMatch::Send(Outpost::PlayerId _player, Outpost::Order _order)
{
  (_player == BLUE ? m_blue : m_red)->Send({.order = std::move(_order)});
}

Outpost::Simulation WorldMatch::Blank() const
{
  Outpost::Simulation simulation(m_seed, m_server.TicksPerSecond());
  simulation.UseTuning(m_tuning);
  return simulation;
}

void WorldMatch::Replay(Outpost::Simulation& _simulation, std::uint64_t _tick) const
{
  const std::vector<Outpost::LoggedCommand>& log = m_server.CommandLog();
  auto next = std::ranges::lower_bound(log, _simulation.CurrentTick(), {}, &Outpost::LoggedCommand::tick);
  while (_simulation.CurrentTick() < _tick)
  {
    std::vector<Outpost::Command> commands;
    for (; next != log.end() && next->tick == _simulation.CurrentTick(); ++next)
      commands.push_back(next->command);
    (void)_simulation.Tick(commands);
  }
}

Outpost::AiSettings WorldMatch::Settings()
{
  return Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
}

void WorldMatch::Answer(Outpost::Transport& _connection, Outpost::AiPlayer& _ai)
{
  for (const Outpost::Snapshot& snapshot : _connection.Receive())
  {
    for (Outpost::Command& command : _ai.Update(snapshot))
      _connection.Send(std::move(command));
  }
}
} // namespace GameLogicTests
