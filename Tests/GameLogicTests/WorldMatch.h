#pragma once

#include "RepositoryData.h"

namespace GameLogicTests
{
// A folder of its own under the system's temporary folder, removed with everything in it when it goes.
class TemporaryFolder
{
public:
  TemporaryFolder();

  TemporaryFolder(const TemporaryFolder&) = delete;
  TemporaryFolder& operator=(const TemporaryFolder&) = delete;

  // A destructor throws nothing: a folder that cannot be removed is left behind for the system to clear, and the test's log
  // says so.
  ~TemporaryFolder();

  [[nodiscard]] const std::filesystem::path& Path() const noexcept;

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

  explicit WorldMatch(std::uint64_t _seed, const std::filesystem::path& _world = {});

  [[nodiscard]] Outpost::InProcessServer& Server() noexcept;

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept;

  // The world this match is, as a save names it: the hash stands for the repository's data.
  [[nodiscard]] Outpost::WorldIdentity Identity() const;

  // The hash of the repository's tuning data and map, as the server's factory makes it.
  [[nodiscard]] static std::uint64_t RepositoryDataHash();

  // Runs _seconds of the world, both AIs playing.
  void Run(double _seconds);

  // Sends an order for _player, which the server applies at its next tick.
  void Send(Outpost::PlayerId _player, Outpost::Order _order);

  // A simulation made as a server makes one for this world's save, before the save is loaded into it.
  [[nodiscard]] Outpost::Simulation Blank() const;

  // Runs _simulation on to _tick, applying at each tick the commands this match's server logged for it.
  void Replay(Outpost::Simulation& _simulation, std::uint64_t _tick) const;

private:
  static Outpost::AiSettings Settings();

  static void Answer(Outpost::Transport& _connection, Outpost::AiPlayer& _ai);

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
