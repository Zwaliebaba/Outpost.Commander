#include "pch.h"
#include "AiMatches.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
constexpr std::uint32_t SECONDS_PER_MINUTE = 60;

// Plays one match until it ends or _limitTicks have run, both players the AI, and logs both players' snapshots. Returns
// whether it ended.
bool PlayOne(Outpost::Server& _server, const Outpost::AiSettings& _settings, std::uint64_t _seed, std::uint64_t _limitTicks,
             std::ostream& _log)
{
  struct Seat
  {
    std::unique_ptr<Outpost::Transport> connection;
    Outpost::AiPlayer ai;
  };
  const std::uint32_t ticksPerSecond = _server.TicksPerSecond();
  std::vector<Seat> seats;
  for (const Outpost::PlayerId player : {Outpost::PlayerId{1}, Outpost::PlayerId{2}})
    seats.push_back({.connection = _server.Connect(player), .ai = Outpost::AiPlayer(_settings, ticksPerSecond)});

  Outpost::MatchLog log(_log, _seed, ticksPerSecond);
  bool ended = false;
  for (std::uint64_t tick = 0; tick < _limitTicks && !ended; ++tick)
  {
    _server.Step();
    for (Seat& seat : seats)
    {
      for (const Outpost::Snapshot& snapshot : seat.connection->Receive())
      {
        log.Record(snapshot);
        ended = ended || snapshot.matchOver;
        for (Outpost::Command& command : seat.ai.Update(snapshot))
          seat.connection->Send(std::move(command));
      }
    }
  }
  log.Finish();
  return ended;
}
} // namespace

// The servers are made one after the other, since each reads the game data from the package. The matches then play on
// as many threads as the machine has, each into a log of its own, and the logs are written out in seed order.
std::uint32_t Outpost::PlayAiMatches(std::ostream& _log, const AiSettings& _settings, const AiMatchesDesc& _desc)
{
  std::vector<std::unique_ptr<Server>> servers;
  servers.reserve(_desc.matches);
  for (std::uint32_t match = 0; match < _desc.matches; ++match)
    servers.push_back(CreateInProcessServer({.seed = _desc.firstSeed + match}));

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
              Server& server = *servers[match];
              const std::uint64_t limitTicks = std::uint64_t{_desc.limitMinutes} * SECONDS_PER_MINUTE * server.TicksPerSecond();
              ended[match] = static_cast<std::uint8_t>(PlayOne(server, _settings, _desc.firstSeed + match, limitTicks, logs[match]));
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
