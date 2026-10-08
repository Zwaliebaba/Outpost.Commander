#include "pch.h"

#include <shellapi.h>

#include "ServerConsole.h"

#include <atomic>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>

namespace
{
// The dedicated server (Phase 5 design §4, ADR-078). It makes a world's folder with --new-world, and runs the world a folder
// holds when given the folder.
constexpr std::wstring_view NEW_WORLD_SWITCH = L"--new-world";
constexpr std::wstring_view HOST_OPTION = L"--host";
constexpr std::wstring_view ADDRESS_OPTION = L"--address";
constexpr std::wstring_view PORT_OPTION = L"--port";
constexpr std::wstring_view SEED_OPTION = L"--seed";
constexpr std::wstring_view AI_OPTION = L"--ai";
constexpr std::wstring_view WORLD_RUN_SWITCH = L"--world-run";
constexpr std::wstring_view HOURS_OPTION = L"--hours";
constexpr std::wstring_view KILLS_OPTION = L"--kills";
constexpr std::wstring_view MATCHUPS_SWITCH = L"--matchups";
constexpr std::wstring_view SEEDS_OPTION = L"--seeds";
constexpr std::string_view USAGE = "Outpost Commander's dedicated server.\n"
                                   "\n"
                                   "  OutpostServer --new-world <folder> [--host <name>] [--address <ip>] [--port <n>] [--seed <n>]\n"
                                   "                [--ai <player>[:Easy|Normal|Hard]]\n"
                                   "      Makes a new world in <folder>: its settings, World.json, with a seat for each of the map's\n"
                                   "      starts. --host is the name or address players reach this machine at, localhost by\n"
                                   "      default; --address what the server listens on, 0.0.0.0 by default; --port its UDP port,\n"
                                   "      45000 by default. --ai gives a seat to an AI empire, Normal unless named.\n"
                                   "\n"
                                   "  OutpostServer <folder>\n"
                                   "      Runs the world in <folder>, coming back from its newest save, and writes a join file for\n"
                                   "      each player's seat there. Hand each player its file. A deputy keeps a player's empire\n"
                                   "      running from a minute after the player leaves. Ctrl+C saves and stops it.\n"
                                   "\n"
                                   "  OutpostServer --world-run <folder> [--seed <n>] [--hours <n>] [--kills <n>] [--ai Easy|Normal|Hard]\n"
                                   "      Plays a world with an AI empire in every seat as fast as it can, 24 hours of it unless\n"
                                   "      --hours says, killing it at 10 random ticks unless --kills says and bringing it back from\n"
                                   "      its folder each time; then plays the same world straight through, and says whether the\n"
                                   "      two ended the same. Both worlds and their logs are kept in <folder>, which must be new.\n"
                                   "\n"
                                   "  OutpostServer --matchups [--seed <n>] [--seeds <n>]\n"
                                   "      Fights each battle matchup of Matchups.json with the AI's battle behavior on both sides, over\n"
                                   "      40 seeds from 1 unless --seeds and --seed say, and says how often each side won.";
// How often the main thread looks at the server, which is where a failure on its thread reaches it (ADR-025).
constexpr std::chrono::milliseconds CHECK_INTERVAL{1'000};

std::string Utf8(const std::filesystem::path& _path)
{
  const std::u8string text = _path.u8string();
  return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::string Utf8(std::wstring_view _text)
{
  return winrt::to_string(_text);
}

// The command line's arguments after the program's name, split by Windows' rules for quotes and backslashes, as the
// game's shell reads its own: main's narrow arguments would lose a folder named outside the system's code page.
std::vector<std::wstring> CommandLineArguments()
{
  struct LocalFreer
  {
    void operator()(LPWSTR* _arguments) const noexcept
    {
      (void)LocalFree(static_cast<HLOCAL>(_arguments));
    }
  };

  int count = 0;
  const std::unique_ptr<LPWSTR, LocalFreer> arguments(winrt::check_pointer(CommandLineToArgvW(GetCommandLineW(), &count)));
  const std::span<const LPWSTR> all(arguments.get(), static_cast<size_t>(std::max(count, 0)));
  return all.empty() ? std::vector<std::wstring>() : std::vector<std::wstring>(all.begin() + 1, all.end());
}

std::string ReadText(const std::filesystem::path& _path)
{
  std::ifstream file(_path, std::ios::binary);
  if (!file)
    throw Neuron::Exception(std::format("{} cannot be read.", Utf8(_path)));
  return {std::istreambuf_iterator<char>(file), {}};
}

void WriteText(const std::filesystem::path& _path, std::string_view _text)
{
  std::ofstream file(_path, std::ios::binary | std::ios::trunc);
  file.write(_text.data(), static_cast<std::streamsize>(_text.size()));
  file.close();
  if (!file)
    throw Neuron::Exception(std::format("{} cannot be written.", Utf8(_path)));
}

// A whole number of the option's, in its range.
template <std::unsigned_integral T> T NumberOption(std::wstring_view _option, std::wstring_view _value, T _minimum)
{
  const std::string text = Utf8(_value);
  T value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || error != std::errc{} || end != text.data() + text.size() || value < _minimum)
    throw Neuron::Exception(
      std::format("{} takes a whole number from {} to {}, not '{}'.", Utf8(_option), _minimum, std::numeric_limits<T>::max(), text));
  return value;
}

// The AI's settings file of a difficulty (ADR-065).
std::wstring_view AiSettingsFile(std::string_view _difficulty)
{
  if (_difficulty == "Easy")
    return Outpost::EASY_AI_SETTINGS;
  return _difficulty == "Hard" ? Outpost::HARD_AI_SETTINGS : Outpost::NORMAL_AI_SETTINGS;
}

// --ai's value: a seat's player, and the difficulty after a colon, Normal when it names none.
void GiveSeatToAi(Outpost::WorldSettings& _settings, std::wstring_view _value)
{
  const std::size_t colon = _value.find(L':');
  const auto player = NumberOption<std::uint32_t>(AI_OPTION, _value.substr(0, colon), 1);
  const std::string difficulty = colon == std::wstring_view::npos ? "Normal" : Utf8(_value.substr(colon + 1));
  if (!std::ranges::contains(Outpost::AI_DIFFICULTIES, difficulty))
    throw Neuron::Exception(std::format("{} takes a difficulty of Easy, Normal or Hard, not '{}'.", Utf8(AI_OPTION), difficulty));
  const auto seat = std::ranges::find(_settings.seats, Outpost::PlayerId{player}, &Outpost::WorldSeat::player);
  if (seat == _settings.seats.end())
    throw Neuron::Exception(std::format("The map has no seat for player {}.", player));
  seat->ai = difficulty;
  seat->token = {};
}

// --new-world: the settings of a new world in _folder, written as its World.json.
int MakeWorld(const std::filesystem::path& _folder, std::span<const std::wstring> _options)
{
  // A seat for each of the map's starts (Phase 5 design §1).
  const Neuron::ByteBuffer mapBytes = Neuron::BinaryFile::ReadFile(L"Map.json");
  if (mapBytes.empty())
    throw Neuron::Exception("The game data file Assets\\Map.json is missing or cannot be read.");
  const Outpost::Map map = Outpost::LoadMap({reinterpret_cast<const char*>(mapBytes.data()), mapBytes.size()});

  Outpost::WorldSettings settings =
    Outpost::NewWorldSettings(static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()), map.starts.size());
  for (std::size_t i = 0; i < _options.size(); i += 2)
  {
    const std::wstring_view option = _options[i];
    if (i + 1 == _options.size())
      throw Neuron::Exception(std::format("{} needs a value.", Utf8(option)));
    const std::wstring_view value = _options[i + 1];
    if (option == HOST_OPTION)
      settings.host = Utf8(value);
    else if (option == ADDRESS_OPTION)
      settings.address = Utf8(value);
    else if (option == PORT_OPTION)
      settings.port = NumberOption<std::uint16_t>(option, value, 1);
    else if (option == SEED_OPTION)
      settings.seed = NumberOption<std::uint64_t>(option, value, 0);
    else if (option == AI_OPTION)
      GiveSeatToAi(settings, value);
    else
      throw Neuron::Exception(std::format("{} is not an option of --new-world.", Utf8(option)));
  }

  const std::filesystem::path settingsPath = _folder / Outpost::WORLD_SETTINGS_FILE;
  if (std::filesystem::exists(settingsPath))
    throw Neuron::Exception(std::format("{} holds a world already.", Utf8(_folder)));
  std::filesystem::create_directories(_folder);
  WriteText(settingsPath, Outpost::WriteWorldSettings(settings));
  Outpost::ServerConsole::Print(std::format("Made a world of {} seats in {}, seed {}. Players reach it at {}, port {}.\n"
                                            "Run it with: OutpostServer \"{}\"",
                                            settings.seats.size(), Utf8(_folder), settings.seed, settings.host, settings.port,
                                            Utf8(_folder)));
  return EXIT_SUCCESS;
}

// Runs the world in _folder until a stop is asked for, and leaves it saved.
int RunWorld(const std::filesystem::path& _folder)
{
  Outpost::ServerConsole console;
  const Outpost::WorldSettings settings = Outpost::ReadWorldSettings(ReadText(_folder / Outpost::WORLD_SETTINGS_FILE));
  std::unique_ptr<Outpost::Server> server = Outpost::CreateInProcessServer(
    {.seed = settings.seed, .quic = true, .quicAddress = settings.address, .quicPort = settings.port, .world = _folder});
  // The players the server plays itself, on its thread (ADR-079): an AI empire for each AI seat, and for each player's seat
  // a deputy that keeps the empire running while its player is away, playing it as the Normal AI researches and defends.
  const std::uint32_t ticksPerSecond = server->TicksPerSecond();
  const Outpost::AiSettings deputySettings = Outpost::LoadPackagedAiSettings(Outpost::NORMAL_AI_SETTINGS);
  for (const Outpost::WorldSeat& seat : settings.seats)
  {
    if (!seat.ai.empty())
    {
      server->Host(seat.player,
                   std::make_unique<Outpost::AiEmpire>(Outpost::LoadPackagedAiSettings(AiSettingsFile(seat.ai)), ticksPerSecond));
      Outpost::ServerConsole::Print(std::format("Player {} is an AI empire, {}.", seat.player.value, seat.ai));
      continue;
    }
    const Outpost::ServerAddress address = server->OpenSeat(seat.player, seat.token);
    server->Host(seat.player, std::make_unique<Outpost::Deputy>(deputySettings, ticksPerSecond));
    const std::filesystem::path joinFile = _folder / Outpost::JoinFileName(seat.player);
    WriteText(joinFile, Outpost::WriteJoinTicket(Outpost::TicketFor(settings, seat, address.certificate)));
    Outpost::ServerConsole::Print(std::format("Player {}'s join file: {}", seat.player.value, Utf8(joinFile)));
  }
  server->Start();
  Outpost::ServerConsole::Print(std::format("The world in {} runs, listening on {} port {}. Ctrl+C saves and stops it.", Utf8(_folder),
                                            settings.address, settings.port));

  // The ticks run on the server's own thread (ADR-025); this one looks at it each second, which is where a failure there
  // is thrown again.
  while (!console.WaitForStop(CHECK_INTERVAL))
    (void)server->TakeTickTimings();
  Outpost::ServerConsole::Print("Stopping: the world's last save is written as the server goes.");
  server.reset();
  console.Finished();
  Outpost::ServerConsole::Print("Stopped.");
  return EXIT_SUCCESS;
}

// --world-run (ADR-082): the world run of _folder, with the options after it.
int WorldRun(const std::filesystem::path& _folder, std::span<const std::wstring> _options)
{
  Outpost::WorldRunDesc desc{.seed = 1, .folder = _folder, .ticks = 0, .kills = 10};
  std::uint64_t hours = 24;
  std::string difficulty = "Normal";
  for (std::size_t index = 0; index < _options.size(); index += 2)
  {
    const std::wstring_view option = _options[index];
    if (index + 1 >= _options.size())
      throw Neuron::Exception(std::format("{} takes a value.", Utf8(option)));
    const std::wstring_view value = _options[index + 1];
    if (option == SEED_OPTION)
      desc.seed = NumberOption<std::uint64_t>(option, value, 1);
    else if (option == HOURS_OPTION)
      hours = NumberOption<std::uint64_t>(option, value, 1);
    else if (option == KILLS_OPTION)
      desc.kills = NumberOption<std::uint32_t>(option, value, 0);
    else if (option == AI_OPTION && std::ranges::contains(Outpost::AI_DIFFICULTIES, Utf8(value)))
      difficulty = Utf8(value);
    else
      throw Neuron::Exception(std::format("{} {} is not an option of --world-run.", Utf8(option), Utf8(value)));
  }
  const Outpost::AiSettings settings = Outpost::LoadPackagedAiSettings(AiSettingsFile(difficulty));
  const std::uint32_t ticksPerSecond = Outpost::CreateWorldServer({.seed = desc.seed})->TicksPerSecond();
  desc.ticks = hours * 3600 * ticksPerSecond;
  desc.makePlayer = [&settings, ticksPerSecond](Outpost::PlayerId)
  { return std::make_unique<Outpost::AiEmpire>(settings, ticksPerSecond); };
  const auto started = std::chrono::steady_clock::now();
  desc.progress = [ticksPerSecond, started](std::string_view _world, std::uint64_t _tick)
  {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
    Outpost::ServerConsole::Print(
      std::format("The {} world is at hour {}, {} s in.", _world, _tick / (std::uint64_t{3600} * ticksPerSecond), seconds));
  };
  const Outpost::WorldRunResult result = Outpost::RunWorlds(desc);
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
  std::string kills;
  for (std::size_t kill = 0; kill < result.killTicks.size(); ++kill)
    kills += std::format("{}{} (back at {})", kill == 0 ? "" : ", ", result.killTicks[kill], result.recoveredTicks[kill]);
  Outpost::ServerConsole::Print(std::format("{} hours of world, {} ticks, twice, in {} s, the {} AI in every seat. Killed after ticks {}.",
                                            hours, desc.ticks, seconds, difficulty, kills.empty() ? std::string("none") : kills));
  Outpost::ServerConsole::Print(result.same ? "The killed world ended the same as the world run straight through."
                                            : "The killed world ended NOT the same as the world run straight through.");
  return result.same ? EXIT_SUCCESS : EXIT_FAILURE;
}

// One battle matchup, fought to its end by the AI on both sides (ADR-083): the winner, none for a draw, how it ended, and
// its ticks.
struct Battle
{
  Outpost::PlayerId winner;
  Outpost::MatchEnding ending = Outpost::MatchEnding::FleetDestroyed;
  std::uint64_t ticks = 0;
};

Battle Fight(std::uint32_t _matchup, std::uint64_t _seed)
{
  const std::unique_ptr<Outpost::InProcessServer> server = Outpost::CreateWorldServer({.seed = _seed, .matchup = _matchup});
  const std::array<std::unique_ptr<Outpost::Transport>, 2> sides{server->Connect(Outpost::PlayerId{1}),
                                                                 server->Connect(Outpost::PlayerId{2})};
  std::array<Outpost::MatchupPlayer, 2> players{Outpost::MatchupPlayer(server->TicksPerSecond()),
                                                Outpost::MatchupPlayer(server->TicksPerSecond())};
  while (true)
  {
    server->Step();
    (void)server->TakeTickTimings();
    for (std::size_t side = 0; side < sides.size(); ++side)
    {
      for (const Outpost::Snapshot& snapshot : sides[side]->Receive())
      {
        if (snapshot.matchOver)
          return {.winner = snapshot.winner, .ending = snapshot.ending, .ticks = snapshot.matchEndedTick};
        for (Outpost::Command& command : players[side].Update(snapshot))
          sides[side]->Send(std::move(command));
      }
    }
  }
}

// --matchups (ADR-083): every matchup over the seeds, the battles shared among the processor's threads.
int Matchups(std::span<const std::wstring> _options)
{
  std::uint64_t firstSeed = 1;
  std::uint32_t seeds = 40;
  for (std::size_t index = 0; index + 1 < _options.size(); index += 2)
  {
    if (_options[index] == SEED_OPTION)
      firstSeed = NumberOption<std::uint64_t>(_options[index], _options[index + 1], 1);
    else if (_options[index] == SEEDS_OPTION)
      seeds = NumberOption<std::uint32_t>(_options[index], _options[index + 1], 1);
    else
      throw Neuron::Exception(std::format("{} is not an option of --matchups.", Utf8(std::wstring_view(_options[index]))));
  }
  if (_options.size() % 2 != 0)
    throw Neuron::Exception(std::format("{} takes a value.", Utf8(std::wstring_view(_options.back()))));
  const std::vector<Outpost::Matchup> matchups = Outpost::ReadPackagedMatchups();
  std::vector<Battle> battles(matchups.size() * seeds);
  std::atomic<std::size_t> next{0};
  std::mutex failureMutex;
  std::exception_ptr failure;
  {
    std::vector<std::jthread> workers;
    for (unsigned worker = 0; worker < std::max(1u, std::thread::hardware_concurrency()); ++worker)
    {
      workers.emplace_back(
        [&]
        {
          for (std::size_t job = next++; job < battles.size(); job = next++)
          {
            try
            {
              battles[job] = Fight(static_cast<std::uint32_t>(job / seeds), firstSeed + (job % seeds));
            }
            catch (...)
            {
              const std::scoped_lock lock(failureMutex);
              failure = std::current_exception();
            }
          }
        });
    }
  }
  if (failure)
    std::rethrow_exception(failure);
  const std::uint32_t ticksPerSecond = Outpost::CreateWorldServer({.seed = firstSeed})->TicksPerSecond();
  for (std::size_t matchup = 0; matchup < matchups.size(); ++matchup)
  {
    const std::span<const Battle> fought(battles.data() + (matchup * seeds), seeds);
    const auto won = [&](std::uint32_t _player)
    { return std::ranges::count_if(fought, [_player](const Battle& _battle) { return _battle.winner.value == _player; }); };
    std::vector<std::uint64_t> lengths;
    lengths.reserve(fought.size());
    for (const Battle& battle : fought)
      lengths.push_back(battle.ticks);
    std::ranges::sort(lengths);
    const auto timedOut =
      std::ranges::count_if(fought, [](const Battle& _battle) { return _battle.ending == Outpost::MatchEnding::TimeLimit; });
    Outpost::ServerConsole::Print(std::format("{}. {}: side 1 won {}, side 2 won {}, drawn {} ({} out of time), of {}; median {} s.",
                                              matchup + 1, matchups[matchup].name, won(1), won(2), seeds - won(1) - won(2), timedOut, seeds,
                                              lengths[lengths.size() / 2] / ticksPerSecond));
  }
  return EXIT_SUCCESS;
}

// The server's run, a failure reported on standard error.
int Run()
{
  try
  {
    // The game data is read from Assets beside the executable, as the game reads its own (ADR-008).
    std::array<wchar_t, MAX_PATH> filename{};
    (void)GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    Neuron::FileSys::SetHomeDirectory(std::filesystem::path(filename.data()).parent_path().wstring());

    const std::vector<std::wstring> arguments = CommandLineArguments();
    if (arguments.size() >= 2 && arguments[0] == NEW_WORLD_SWITCH)
      return MakeWorld(arguments[1], std::span(arguments).subspan(2));
    if (arguments.size() >= 2 && arguments[0] == WORLD_RUN_SWITCH)
      return WorldRun(arguments[1], std::span(arguments).subspan(2));
    if (!arguments.empty() && arguments[0] == MATCHUPS_SWITCH)
      return Matchups(std::span(arguments).subspan(1));
    if (arguments.size() == 1 && !arguments[0].starts_with(L"--"))
      return RunWorld(arguments[0]);
    Outpost::ServerConsole::PrintError(USAGE);
    return EXIT_FAILURE;
  }
  catch (const winrt::hresult_error& error)
  {
    Outpost::ServerConsole::PrintError(
      std::format("{} (error 0x{:08X})", winrt::to_string(error.message()), static_cast<std::uint32_t>(error.code())));
    return EXIT_FAILURE;
  }
  catch (const std::exception& error)
  {
    // Neuron::Exception carries UTF-8, such as a loader's report of a bad file (ADR-008).
    Outpost::ServerConsole::PrintError(error.what());
    return EXIT_FAILURE;
  }
}
} // namespace

// main throws nothing: a failure while reporting a failure ends the process with the exit code alone.
int main()
{
  try
  {
    return Run();
  }
  catch (...)
  {
    return EXIT_FAILURE;
  }
}
