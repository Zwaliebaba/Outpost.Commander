#include "pch.h"
#include "AiMatches.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
constexpr std::uint32_t SECONDS_PER_MINUTE = 60;

// Reads an AI's settings from a file in Opponent.json's format, as LoadPackagedAiSettings reads the packaged one.
Outpost::AiSettings LoadAiSettingsFile(const std::filesystem::path& _path)
{
  const std::string name = winrt::to_string(_path.wstring());
  std::ifstream file(_path, std::ios::binary);
  if (!file)
    throw Neuron::Exception(std::format("The AI settings file {} is missing or cannot be read.", name));
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  if (file.bad())
    throw Neuron::Exception(std::format("The AI settings file {} cannot be read.", name));
  try
  {
    return Outpost::LoadAiSettings(text);
  }
  catch (const Neuron::Exception& error)
  {
    throw Neuron::Exception(std::format("{}: {}", name, error.what()));
  }
}

// Plays one match until it ends or _limitTicks have run, both players an AI, player 1 of _settings[0] and player 2 of
// _settings[1], and logs both players' snapshots. Returns whether it ended.
bool PlayOne(Outpost::Server& _server, const std::array<Outpost::AiSettings, 2>& _settings, std::uint64_t _seed, std::uint64_t _limitTicks,
             std::ostream& _log)
{
  // Both are made in place: an AiPlayer is never moved, since moving its maps may allocate.
  const std::uint32_t ticksPerSecond = _server.TicksPerSecond();
  const std::array<std::unique_ptr<Outpost::Transport>, 2> connections{_server.Connect(Outpost::PlayerId{1}),
                                                                       _server.Connect(Outpost::PlayerId{2})};
  std::array<Outpost::AiPlayer, 2> ais{Outpost::AiPlayer(_settings[0], ticksPerSecond), Outpost::AiPlayer(_settings[1], ticksPerSecond)};

  Outpost::MatchLog log(_log, _seed, ticksPerSecond);
  bool ended = false;
  for (std::uint64_t tick = 0; tick < _limitTicks && !ended; ++tick)
  {
    _server.Step();
    // A stepped server still times its ticks (task 8.1). Nothing here reads them, so they are taken and dropped every tick
    // rather than left to grow over the match.
    (void)_server.TakeTickTimings();
    for (size_t seat = 0; seat < connections.size(); ++seat)
    {
      for (const Outpost::Snapshot& snapshot : connections[seat]->Receive())
      {
        log.Record(snapshot);
        ended = ended || snapshot.matchOver;
        for (Outpost::Command& command : ais[seat].Update(snapshot))
          connections[seat]->Send(std::move(command));
      }
    }
  }
  log.Finish();
  return ended;
}
} // namespace

std::uint32_t Outpost::PlayAiMatches(const AiMatchesOptions& _options)
{
  std::array<AiSettings, 2> settings;
  for (size_t player = 0; player < settings.size(); ++player)
  {
    const std::filesystem::path& path = _options.settings[player];
    settings[player] = path.empty() ? LoadPackagedAiSettings() : LoadAiSettingsFile(path);
  }
  const std::string name = winrt::to_string(_options.log.wstring());
  std::ofstream log(_options.log, std::ios::trunc);
  if (!log)
    throw Neuron::Exception(std::format("The log {} cannot be written.", name));
  const std::uint32_t ended = PlayAiMatches(log, settings, _options.desc);
  if (!log)
    throw Neuron::Exception(std::format("The log {} could not be written in full.", name));
  return ended;
}

// The game data is read from the package and checked once, before any match starts. The matches then play on as many
// threads as the machine has, each on a server of its own made from a copy of that data by the thread that plays it, and
// each into a log of its own; the logs are written out in seed order.
std::uint32_t Outpost::PlayAiMatches(std::ostream& _log, const std::array<AiSettings, 2>& _settings, const AiMatchesDesc& _desc)
{
  const ServerFactory createServer = InProcessServerFactory();

  std::vector<std::ostringstream> logs(_desc.matches);
  std::vector<std::uint8_t> ended(_desc.matches, 0);
  std::atomic<std::uint32_t> next = 0;
  std::mutex failureMutex;
  std::exception_ptr failure;
  {
    const std::uint32_t threads = std::clamp(std::thread::hardware_concurrency(), 1U, std::max(_desc.matches, 1U));
    std::vector<std::jthread> workers;
    workers.reserve(threads);
    for (std::uint32_t thread = 0; thread < threads; ++thread)
    {
      workers.emplace_back(
        [&]
        {
          for (std::uint32_t match = next++; match < _desc.matches; match = next++)
          {
            try
            {
              const std::unique_ptr<Server> server = createServer({.seed = _desc.firstSeed + match});
              const std::uint64_t limitTicks = std::uint64_t{_desc.limitMinutes} * SECONDS_PER_MINUTE * server->TicksPerSecond();
              ended[match] = static_cast<std::uint8_t>(PlayOne(*server, _settings, _desc.firstSeed + match, limitTicks, logs[match]));
            }
            catch (...)
            {
              const std::scoped_lock lock(failureMutex);
              if (!failure)
                failure = std::current_exception();
            }
          }
        });
    }
  }
  if (failure)
    std::rethrow_exception(failure);
  for (const std::ostringstream& log : logs)
    _log << log.str();
  _log.flush();
  return static_cast<std::uint32_t>(std::ranges::count(ended, std::uint8_t{1}));
}
