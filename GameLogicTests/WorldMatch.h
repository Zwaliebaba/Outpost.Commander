#pragma once

#include "RepositoryData.h"

namespace GameLogicTests
{
// A folder of its own under the system's temporary folder, removed with everything in it when it goes.
class TemporaryFolder
{
public:
  TemporaryFolder()
    : m_path(std::filesystem::temp_directory_path() /
             std::format("OutpostWorldTests-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(), sm_made++))
  {
    std::filesystem::create_directories(m_path);
  }

  TemporaryFolder(const TemporaryFolder&) = delete;
  TemporaryFolder& operator=(const TemporaryFolder&) = delete;

  // A destructor throws nothing: a folder that cannot be removed is left behind for the system to clear, and the test's log
  // says so.
  ~TemporaryFolder()
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

  [[nodiscard]] const std::filesystem::path& Path() const noexcept
  {
    return m_path;
  }

private:
  // Counts the folders made, so that two made in one clock tick differ. The tests run on one thread.
  inline static std::uint32_t sm_made = 0;
  std::filesystem::path m_path;
};

// A world's tests (ADR-077): two AIs playing each other on the repository's tuning data and map, on a server stepped by
// hand, as the AI-against-AI matches are played (ADR-038). Given a folder, the server is a world's, set up as the server's
// factory sets one up: one that holds a save comes back as the world it saved, whatever _seed says.
class WorldMatch
{
public:
  static constexpr Outpost::PlayerId BLUE{1};
  static constexpr Outpost::PlayerId RED{2};

  explicit WorldMatch(std::uint64_t _seed, const std::filesystem::path& _world = {})
    : m_recovery(_world.empty() ? std::nullopt : Outpost::WorldFolder::Recover(_world)),
      m_seed(m_recovery.has_value() ? m_recovery->header.identity.seed : _seed),
      m_tuning(Outpost::LoadTuning(ReadRepositoryTuning())),
      m_server(m_tuning, Outpost::LoadMap(ReadRepositoryMap()), {.seed = m_seed}),
      m_blue(m_server.Connect(BLUE)),
      m_red(m_server.Connect(RED)),
      m_blueAi(Settings(), m_server.TicksPerSecond()),
      m_redAi(Settings(), m_server.TicksPerSecond())
  {
    // Match setup as the server's factory does it.
    if (!m_recovery)
      m_server.World().PlaceStartingBases(m_server.MapData());
    if (!_world.empty())
      m_server.UseWorld(_world, RepositoryDataHash(), std::exchange(m_recovery, std::nullopt));
    m_server.PreparePathfinding();
  }

  [[nodiscard]] Outpost::InProcessServer& Server() noexcept
  {
    return m_server;
  }

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept
  {
    return m_tuning;
  }

  // The world this match is, as a save names it: the hash stands for the repository's data.
  [[nodiscard]] Outpost::WorldIdentity Identity() const
  {
    return {.seed = m_seed, .ticksPerSecond = m_server.TicksPerSecond(), .dataHash = RepositoryDataHash()};
  }

  // The hash of the repository's tuning data and map, as the server's factory makes it.
  [[nodiscard]] static std::uint64_t RepositoryDataHash()
  {
    const std::string tuning = ReadRepositoryTuning();
    const std::string map = ReadRepositoryMap();
    const std::array<std::string_view, 2> texts{tuning, map};
    return Outpost::DataHash(texts);
  }

  // Runs _seconds of the world, both AIs playing.
  void Run(double _seconds)
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

  // Sends an order for _player, which the server applies at its next tick.
  void Send(Outpost::PlayerId _player, Outpost::Order _order)
  {
    (_player == BLUE ? m_blue : m_red)->Send({.order = std::move(_order)});
  }

  // A simulation made as a server makes one for this world's save, before the save is loaded into it.
  [[nodiscard]] Outpost::Simulation Blank() const
  {
    Outpost::Simulation simulation(m_seed, m_server.TicksPerSecond());
    simulation.UseTuning(m_tuning);
    return simulation;
  }

  // Runs _simulation on to _tick, applying at each tick the commands this match's server logged for it.
  void Replay(Outpost::Simulation& _simulation, std::uint64_t _tick) const
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

private:
  static Outpost::AiSettings Settings()
  {
    return Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
  }

  static void Answer(Outpost::Transport& _connection, Outpost::AiPlayer& _ai)
  {
    for (const Outpost::Snapshot& snapshot : _connection.Receive())
    {
      for (Outpost::Command& command : _ai.Update(snapshot))
        _connection.Send(std::move(command));
    }
  }

  // What the world's folder held to come back from, until the server takes it.
  std::optional<Outpost::WorldFolder::Recovery> m_recovery;
  std::uint64_t m_seed = 0;
  Outpost::Tuning m_tuning;
  Outpost::InProcessServer m_server;
  std::unique_ptr<Outpost::Transport> m_blue;
  std::unique_ptr<Outpost::Transport> m_red;
  Outpost::AiPlayer m_blueAi;
  Outpost::AiPlayer m_redAi;
};
} // namespace GameLogicTests
