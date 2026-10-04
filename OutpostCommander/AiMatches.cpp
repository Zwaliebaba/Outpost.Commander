#include "pch.h"
#include "AiMatches.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
constexpr std::uint32_t SECONDS_PER_MINUTE = 60;
// The log when the options name none: in the temporary folder, replaced by each run (ADR-038).
constexpr auto DEFAULT_LOG = L"OutpostCommander-ai-matches.log";
// The switch's options (ADR-061). Each takes a value.
constexpr std::wstring_view FIRST_SEED_OPTION = L"--first-seed";
constexpr std::wstring_view MATCHES_OPTION = L"--matches";
constexpr std::wstring_view LIMIT_MINUTES_OPTION = L"--limit-minutes";
constexpr std::wstring_view AI1_OPTION = L"--ai1";
constexpr std::wstring_view AI2_OPTION = L"--ai2";
constexpr std::wstring_view LOG_OPTION = L"--log";
constexpr std::array<std::wstring_view, 6> OPTIONS{FIRST_SEED_OPTION, MATCHES_OPTION, LIMIT_MINUTES_OPTION,
                                                   AI1_OPTION,        AI2_OPTION,     LOG_OPTION};

// A whole number at least _minimum and at most _maximum, written in decimal digits and nothing else.
std::uint64_t ReadNumber(std::wstring_view _option, std::wstring_view _value, std::uint64_t _minimum, std::uint64_t _maximum)
{
  // Every digit is ASCII, so narrowing them one by one is exact; anything else is refused below.
  std::string digits;
  for (const wchar_t character : _value)
    digits.push_back(character >= L'0' && character <= L'9' ? static_cast<char>(character) : '?');
  std::uint64_t number = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
  if (digits.empty() || error != std::errc{} || end != digits.data() + digits.size() || number < _minimum || number > _maximum)
  {
    throw Neuron::Exception(std::format("--ai-matches: {} takes a whole number from {} to {}, not '{}'.", winrt::to_string(_option),
                                        _minimum, _maximum, winrt::to_string(_value)));
  }
  return number;
}

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

Outpost::AiMatchesOptions Outpost::ReadAiMatchesOptions(std::span<const std::wstring> _arguments)
{
  AiMatchesOptions options;
  std::vector<std::wstring_view> given;
  for (size_t i = 0; i < _arguments.size(); i += 2)
  {
    const std::wstring_view option = _arguments[i];
    if (std::ranges::find(OPTIONS, option) == OPTIONS.end())
      throw Neuron::Exception(std::format("--ai-matches: {} is not an option it takes.", winrt::to_string(option)));
    if (std::ranges::find(given, option) != given.end())
      throw Neuron::Exception(std::format("--ai-matches: {} is given twice.", winrt::to_string(option)));
    given.push_back(option);
    // The value follows its option, and is neither empty nor another option.
    if (i + 1 == _arguments.size() || _arguments[i + 1].empty() || _arguments[i + 1].starts_with(L"--"))
      throw Neuron::Exception(std::format("--ai-matches: {} needs a value.", winrt::to_string(option)));
    const std::wstring_view value = _arguments[i + 1];
    if (option == FIRST_SEED_OPTION)
      options.desc.firstSeed = ReadNumber(option, value, 0, std::numeric_limits<std::uint64_t>::max());
    else if (option == MATCHES_OPTION)
      options.desc.matches = static_cast<std::uint32_t>(ReadNumber(option, value, 1, std::numeric_limits<std::uint32_t>::max()));
    else if (option == LIMIT_MINUTES_OPTION)
      options.desc.limitMinutes = static_cast<std::uint32_t>(ReadNumber(option, value, 1, std::numeric_limits<std::uint32_t>::max()));
    else if (option == AI1_OPTION)
      options.settings[0] = value;
    else if (option == AI2_OPTION)
      options.settings[1] = value;
    else if (option == LOG_OPTION)
      options.log = value;
  }
  if (options.log.empty())
    options.log = std::filesystem::temp_directory_path() / DEFAULT_LOG;
  return options;
}

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
