#include "pch.h"

#include "ServerConsole.h"

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
constexpr std::string_view USAGE = "Outpost Commander's dedicated server.\n"
                                   "\n"
                                   "  OutpostServer --new-world <folder> [--host <name>] [--address <ip>] [--port <n>] [--seed <n>]\n"
                                   "      Makes a new world in <folder>: its settings, World.json, with a seat for each of the map's\n"
                                   "      starts. --host is the name or address players reach this machine at, localhost by\n"
                                   "      default; --address what the server listens on, 0.0.0.0 by default; --port its UDP port,\n"
                                   "      45000 by default.\n"
                                   "\n"
                                   "  OutpostServer <folder>\n"
                                   "      Runs the world in <folder>, coming back from its newest save, and writes a join file for\n"
                                   "      each seat there. Hand each player its file. Ctrl+C saves and stops it.";
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
  for (const Outpost::WorldSeat& seat : settings.seats)
  {
    const Outpost::ServerAddress address = server->OpenSeat(seat.player, seat.token);
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
} // namespace

int wmain(int _argc, wchar_t* _argv[])
{
  try
  {
    // The game data is read from Assets beside the executable, as the game reads its own (ADR-008).
    std::array<wchar_t, MAX_PATH> filename{};
    (void)GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    Neuron::FileSys::SetHomeDirectory(std::filesystem::path(filename.data()).parent_path().wstring());

    const std::vector<std::wstring> arguments(_argv + std::min(_argc, 1), _argv + _argc);
    if (arguments.size() >= 2 && arguments[0] == NEW_WORLD_SWITCH)
      return MakeWorld(arguments[1], std::span(arguments).subspan(2));
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
