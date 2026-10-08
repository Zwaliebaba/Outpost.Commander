#include "pch.h"

#include <shellapi.h>
#include <shlobj.h>

#include "AiMatches.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>

// CommandLineToArgvW is shell32's, which the executable's Windows Store project type does not link by default, as
// NeuronClient's Window.cpp says of user32.
#pragma comment(lib, "shell32.lib")

namespace
{
// Linear color; the render target view encodes it to sRGB (ADR-006). Black: the stars are all the sky has (ADR-022).
constexpr std::array<float, 4> CLEAR_COLOR{0.0f, 0.0f, 0.0f, 1.0f};
constexpr auto GAME_TITLE = L"Outpost Commander";
// Minimized there is no frame to wait for, so the loop wakes this often for its clients to keep taking their snapshots
// (ADR-009, ADR-025).
constexpr DWORD MINIMIZED_WAKE_MILLISECONDS = 16;
// The human player, and the other: the AI in a skirmish (task 6.1), or a load driver in a measurement run.
constexpr Outpost::PlayerId HUMAN_PLAYER{1};
constexpr Outpost::PlayerId RIVAL_PLAYER{2};

// Task 2.7's switches. --measure logs every tick's duration, and since task 8.1 each of its parts, and every move order's
// time to its first visible response to MEASUREMENT_LOG in the temporary folder; --load adds the load of 200 ships and 40 structures, and keeps both fleets
// moving. Times are on std::chrono::steady_clock, which counts QueryPerformanceCounter, so a script injecting input
// can line its own timestamps up with the game's.
constexpr std::wstring_view MEASURE_SWITCH = L"--measure";
constexpr std::wstring_view LOAD_SWITCH = L"--load";
// Task 3.7's switch: the stress scene of 200 ships and 40 structures in combat, kept at full size (StressLoad). It takes
// the place of --load. With --measure, every frame's CPU and GPU time is logged as well, with the back buffer's size and
// the display's refresh rate whenever the size changes, for Q4 (ADR-006).
constexpr std::wstring_view STRESS_SWITCH = L"--stress";
constexpr auto MEASUREMENT_LOG = L"OutpostCommander-measure.log";
// Plan task 6.3: every match against the AI is added to this log in the temporary folder, for Tools/MatchLog.py.
constexpr auto MATCH_LOG = L"OutpostCommander-matches.log";
// Phase 1 plan task 13.1's switch: seeded AI-against-AI matches on the real server, played headlessly as fast as it
// ticks, for P1's repeatable figure. No window opens; a message says when they are done, and Tools/MatchLog.py
// --ai-matches summarizes them. Its options name the seeds, each AI's settings and the log (AiMatchesOptions, ADR-063).
constexpr std::wstring_view AI_MATCHES_SWITCH = L"--ai-matches";
// With --ai-matches, for a script such as Tools/SelfPlay.py: no message. The exit code says how the run went, and a
// failure's message goes to standard error (ADR-063).
constexpr std::wstring_view QUIET_SWITCH = L"--quiet";
// Phase 5's switch: --join <file> joins the world a dedicated server's join file names, rather than showing the menu
// (ADR-078). Without it, the menu offers to join the world of the join file in the player's documents, when there is one.
constexpr std::wstring_view JOIN_SWITCH = L"--join";
// Phase 5's other switch: --matchup <n> makes every skirmish battle matchup n of Matchups.json, from 1, against the AI's
// battle behavior, for the owner's half of horizon §9's measurement; the match log names the matchup (ADR-083).
constexpr std::wstring_view MATCHUP_SWITCH = L"--matchup";
constexpr auto JOIN_FOLDER = L"Outpost Commander";
constexpr auto JOIN_FILE = L"Join.json";

std::int64_t Nanoseconds(std::chrono::steady_clock::time_point _time) noexcept
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(_time.time_since_epoch()).count();
}

// The refresh rate of the display the window is on, or 0 when Windows does not say.
DWORD RefreshRateHertz(HWND _window) noexcept
{
  MONITORINFOEXW monitor{};
  monitor.cbSize = sizeof(monitor);
  if (GetMonitorInfoW(MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST), &monitor) == FALSE)
    return 0;
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  if (EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &mode) == FALSE)
    return 0;
  return mode.dmDisplayFrequency;
}

// The command line's arguments after the program's name, split by Windows' rules for quotes and backslashes.
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

// Shows a failure, or under --quiet writes it to standard error, where the script that ran the game reads it (ADR-063).
void ReportFailure(std::wstring_view _message, bool _quiet)
{
  if (!_quiet)
  {
    MessageBoxW(nullptr, std::wstring(_message).c_str(), GAME_TITLE, MB_OK | MB_ICONERROR);
    return;
  }
  const HANDLE standardError = Neuron::SafeHandle(GetStdHandle(STD_ERROR_HANDLE));
  if (standardError == nullptr)
    return;
  const std::string text = winrt::to_string(_message) + "\n";
  DWORD written = 0;
  (void)WriteFile(standardError, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}

// Where the menu looks for a join file: Outpost Commander\Join.json in the player's documents, which a packaged game reads
// as any program does, where its own application data is redirected (ADR-078). Empty when Windows does not say where the
// documents are.
std::filesystem::path DocumentsJoinFile()
{
  PWSTR documents = nullptr;
  const HRESULT found = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents);
  std::filesystem::path path;
  if (SUCCEEDED(found))
    path = std::filesystem::path(documents) / JOIN_FOLDER / JOIN_FILE;
  CoTaskMemFree(documents);
  return path;
}

// The join file at _path. Throws Neuron::Exception when it cannot be read or is not one.
Outpost::JoinTicket ReadJoinFile(const std::filesystem::path& _path)
{
  std::ifstream file(_path, std::ios::binary);
  if (!file)
    throw Neuron::Exception(std::format("The join file {} cannot be read.", winrt::to_string(_path.wstring())));
  return Outpost::ReadJoinTicket(std::string(std::istreambuf_iterator<char>(file), {}));
}

// A seed for one match (ADR-009), logged so that the match can be reproduced from it and its command log.
std::uint64_t NewSeed()
{
  const auto seed = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
  OutputDebugStringA(std::format("Match seed {}\n", seed).c_str());
  return seed;
}

// The first match's server and the AI's settings, made together.
struct ServerStart
{
  std::unique_ptr<Outpost::Server> server;
  // The AI's settings at each difficulty, in Outpost::Difficulty's order (ADR-065).
  std::array<Outpost::AiSettings, 3> ai;
};

// Something made on another thread, and when it was done.
template <typename T> struct Prepared
{
  T value;
  std::chrono::steady_clock::time_point ready;
};

// Runs _make on a thread of its own, so that it overlaps whatever the caller does next (ADR-049).
template <typename Fn> auto PrepareAsync(Fn _make)
{
  return std::async(std::launch::async,
                    [make = std::move(_make)]
                    {
                      auto value = make();
                      return Prepared<decltype(value)>{.value = std::move(value), .ready = std::chrono::steady_clock::now()};
                    });
}

// One match: its server, the human's connection, the other player's with whatever drives it, and its log. A world joined on
// a dedicated server has the human's connection alone (ADR-078).
struct Match
{
  std::unique_ptr<Outpost::Server> server;
  std::unique_ptr<Outpost::Transport> player;
  std::unique_ptr<Outpost::Transport> rival;
  std::optional<Outpost::AiPlayer> ai;
  // A battle matchup's rival, in place of the AI (ADR-083).
  std::optional<Outpost::MatchupPlayer> battle;
  Outpost::LoadDriver playerLoad;
  Outpost::LoadDriver rivalLoad;
  // The log writes to the file, so it comes after it and is destroyed before it.
  std::ofstream logFile;
  std::optional<Outpost::MatchLog> log;
};
} // namespace

int WINAPI wWinMain([[maybe_unused]] HINSTANCE _hInstance, [[maybe_unused]] HINSTANCE _hPrevInstance, LPWSTR _cmdLine,
                    [[maybe_unused]] int _cmdShow)
{
#if defined(_DEBUG)
  //  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  // A failure anywhere below, at startup or when the graphics device is lost, ends up here once: it is shown, and its code
  // is the exit code, so the game never closes without saying why (ADR-006). A quiet run of --ai-matches sets this before
  // anything there can fail, and its failure goes to standard error instead (ADR-063).
  bool quiet = false;
  try
  {
    const auto launched = std::chrono::steady_clock::now();
    wchar_t filename[MAX_PATH];
    GetModuleFileNameW(nullptr, filename, MAX_PATH);
    auto path = std::wstring(filename);
    path = path.substr(0, path.find_last_of('\\'));

    Neuron::FileSys::SetHomeDirectory(path);

    const std::wstring_view commandLine = _cmdLine != nullptr ? std::wstring_view(_cmdLine) : std::wstring_view();
    if (commandLine.find(AI_MATCHES_SWITCH) != std::wstring_view::npos)
    {
      // The shell reads its own two words, the switch and --quiet; the rest are the switch's options (ADR-063).
      std::vector<std::wstring> arguments = CommandLineArguments();
      quiet = std::erase(arguments, QUIET_SWITCH) > 0;
      std::erase(arguments, AI_MATCHES_SWITCH);
      const Outpost::AiMatchesOptions options = Outpost::ReadAiMatchesOptions(arguments);
      const auto started = std::chrono::steady_clock::now();
      const std::uint32_t ended = Outpost::PlayAiMatches(options);
      if (quiet)
        return EXIT_SUCCESS;
      const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
      const std::wstring message = std::format(L"Played {} AI-against-AI matches in {} seconds: {} ended within {} minutes.\n\n"
                                               L"The log is {}.\nTools\\MatchLog.py --ai-matches summarizes them.",
                                               options.desc.matches, seconds, ended, options.desc.limitMinutes, options.log.wstring());
      MessageBoxW(nullptr, message.c_str(), GAME_TITLE, MB_OK | MB_ICONINFORMATION);
      return EXIT_SUCCESS;
    }
    // --join names its file in the next argument.
    std::optional<std::filesystem::path> joinOnStart;
    if (commandLine.find(JOIN_SWITCH) != std::wstring_view::npos)
    {
      const std::vector<std::wstring> arguments = CommandLineArguments();
      const auto join = std::ranges::find(arguments, JOIN_SWITCH);
      if (join == arguments.end() || join + 1 == arguments.end())
        throw Neuron::Exception("--join needs the join file a dedicated server wrote.");
      joinOnStart = *(join + 1);
    }
    // --matchup names its number in the next argument.
    std::optional<std::uint32_t> matchup;
    if (commandLine.find(MATCHUP_SWITCH) != std::wstring_view::npos)
    {
      const std::vector<std::wstring> arguments = CommandLineArguments();
      const auto named = std::ranges::find(arguments, MATCHUP_SWITCH);
      const std::wstring number = named != arguments.end() && named + 1 != arguments.end() ? *(named + 1) : std::wstring();
      if (number.empty() || number.size() > 3 ||
          !std::ranges::all_of(number, [](wchar_t _digit) { return _digit >= L'0' && _digit <= L'9'; }) || std::stoul(number) == 0)
        throw Neuron::Exception("--matchup needs the number of a battle matchup of Matchups.json, from 1.");
      matchup = static_cast<std::uint32_t>(std::stoul(number) - 1);
    }
    const bool measure = commandLine.find(MEASURE_SWITCH) != std::wstring_view::npos;
    const bool stress = commandLine.find(STRESS_SWITCH) != std::wstring_view::npos;
    const bool load = !stress && commandLine.find(LOAD_SWITCH) != std::wstring_view::npos;
    // A measurement run skips the menu and starts its match at once (task 6.2).
    const bool skipMenu = measure || load || stress;
    std::uint64_t seed = NewSeed();
    std::ofstream measurements;
    if (measure)
    {
      measurements.open(std::filesystem::temp_directory_path() / MEASUREMENT_LOG, std::ios::trunc);
      measurements << std::format("seed {} load {} stress {}\n", seed, load ? 1 : 0, stress ? 1 : 0);
      // The names of the parts each tick_parts_ns line gives, in its order (task 8.1).
      measurements << "tick_part_names";
      for (std::size_t part = 0; part < Outpost::TICK_PART_COUNT; ++part)
        measurements << ' ' << Outpost::TickPartName(static_cast<Outpost::TickPart>(part));
      measurements << '\n';
    }

    // Startup's stages, each at its time since wWinMain began (ADR-049).
    const auto logStage = [&measurements, measure, launched](std::string_view _stage, std::chrono::steady_clock::time_point _at)
    {
      if (measure)
        measurements << std::format("startup_ns {} {}\n", _stage, (_at - launched).count());
    };

    // The first match's server with the AI's settings, the client's models, and its interface's atlas are made on threads
    // of their own while the window and the device are created (ADR-049). The window stays hidden until all of them are
    // in, so that bad tuning, map or model data is still reported before the screen goes full screen. The next match's
    // server is made while the menu shows.
    // A match's players connect to its server over QUIC, as they will to a server of its own (ADR-060).
    const Outpost::ServerDesc serverDesc{.seed = seed, .measurementLoad = load, .stressLoad = stress, .quic = true, .matchup = matchup};
    // The description is captured as a copy of its own rather than as the const it is here, so that moving the lambda moves
    // it, which cannot throw, where copying its path could.
    auto serverLoad = PrepareAsync(
      [desc = Outpost::ServerDesc(serverDesc)]
      {
        return ServerStart{.server = Outpost::CreateInProcessServer(desc),
                           .ai = {Outpost::LoadPackagedAiSettings(Outpost::EASY_AI_SETTINGS),
                                  Outpost::LoadPackagedAiSettings(Outpost::NORMAL_AI_SETTINGS),
                                  Outpost::LoadPackagedAiSettings(Outpost::HARD_AI_SETTINGS)}};
      });
    auto assetsLoad = PrepareAsync(Outpost::LoadClientAssets);

    Neuron::Window window({.title = GAME_TITLE, .windowedClientWidthPixels = 1280, .windowedClientHeightPixels = 720});
    logStage("window", std::chrono::steady_clock::now());
    auto interfaceLoad = PrepareAsync([width = window.ClientWidthPixels(), height = window.ClientHeightPixels()]
                                      { return Outpost::RasterizeInterface(width, height); });
    Neuron::Renderer renderer(window.Handle(), window.ClientWidthPixels(), window.ClientHeightPixels());
    logStage("device", std::chrono::steady_clock::now());

    Prepared<ServerStart> serverStart = serverLoad.get();
    logStage("server", serverStart.ready);
    std::unique_ptr<Outpost::Server> nextServer = std::move(serverStart.value.server);
    const std::uint32_t ticksPerSecond = nextServer->TicksPerSecond();
    const std::array<Outpost::AiSettings, 3> aiSettings = std::move(serverStart.value.ai);
    std::optional<Match> match;
    Prepared<Outpost::ClientAssets> assets = assetsLoad.get();
    logStage("assets", assets.ready);
    Prepared<Neuron::UiAtlas> interfaceAtlas = interfaceLoad.get();
    logStage("interface", interfaceAtlas.ready);

    // Every model is loaded before the first frame, so a missing or broken mesh is reported rather than skipped (ADR-011).
    // The client's uploads go to the GPU together, and are waited for once (ADR-048).
    renderer.BeginUploads();
    Outpost::GameClient client(renderer, ticksPerSecond, std::move(assets.value), std::move(interfaceAtlas.value));
    logStage("client", std::chrono::steady_clock::now());
    renderer.EndUploads();
    logStage("uploads", std::chrono::steady_clock::now());
    window.Show();

    auto lastFrame = std::chrono::steady_clock::now();
    const auto startMatch = [&]
    {
      match.emplace();
      match->server = std::move(nextServer);
      // Each player takes a seat over QUIC, and the server welcomes it before the match starts (ADR-060).
      match->player =
        std::make_unique<Outpost::QuicTransport>(match->server->OpenSeat(HUMAN_PLAYER, Outpost::NewSeatToken()), HUMAN_PLAYER);
      // The rival always connects, so that the server builds both players' snapshots as in a match against the AI. Under
      // load the rival's ships are kept moving through it; the stress scene orders its own; otherwise it is the AI.
      match->rival = std::make_unique<Outpost::QuicTransport>(match->server->OpenSeat(RIVAL_PLAYER, Outpost::NewSeatToken()), RIVAL_PLAYER);
      if (!load && !stress)
      {
        if (matchup.has_value())
          match->battle.emplace(ticksPerSecond);
        else
          match->ai.emplace(aiSettings[static_cast<std::size_t>(client.RequestedDifficulty())], ticksPerSecond);
        match->logFile.open(std::filesystem::temp_directory_path() / MATCH_LOG, std::ios::app);
        match->log.emplace(match->logFile, seed, ticksPerSecond);
        if (matchup.has_value())
          match->log->Matchup(*matchup + 1);
      }
      // Its ticks run on the server's own thread from here, at their fixed rate whatever the frame rate (ADR-025).
      match->server->Start();
      client.StartMatch();
      lastFrame = std::chrono::steady_clock::now();
    };
    // A world on a dedicated server: the player's connection alone, to the seat its join file names (ADR-078). A join that
    // fails goes back to the menu, saying why.
    const std::filesystem::path documentsJoinFile = DocumentsJoinFile();
    const auto offerWorld = [&] { client.OfferWorld(!documentsJoinFile.empty() && std::filesystem::exists(documentsJoinFile)); };
    const auto joinWorld = [&](const std::filesystem::path& _joinFile)
    {
      try
      {
        const Outpost::JoinTicket ticket = ReadJoinFile(_joinFile);
        match.emplace();
        match->player = std::make_unique<Outpost::QuicTransport>(ticket.address, ticket.player);
        client.StartMatch();
        lastFrame = std::chrono::steady_clock::now();
      }
      catch (const std::exception& error)
      {
        match.reset();
        offerWorld();
        client.ShowMenu(std::format("Could not join the world: {}", error.what()));
      }
    };
    offerWorld();
    if (joinOnStart)
      joinWorld(*joinOnStart);
    else if (skipMenu)
      startMatch();
    UINT loggedWidthPixels = 0;
    UINT loggedHeightPixels = 0;
    bool firstFramePresented = false;
    for (;;)
    {
      // Minimized, there is nothing to show, so the loop sleeps until a message arrives or the snapshots are due again.
      // Otherwise it waits until the swap chain can take a frame, and reads input right after, so the frame shows the
      // freshest input it can (ADR-006).
      if (window.IsMinimized())
        MsgWaitForMultipleObjects(0, nullptr, FALSE, MINIMIZED_WAKE_MILLISECONDS, QS_ALLINPUT);
      else
        renderer.WaitForNextFrame();
      // A frame's CPU work starts once the swap chain lets it, and takes in the messages, the ticks and the drawing.
      const auto frameStarted = std::chrono::steady_clock::now();

      if (!window.ProcessMessages())
        break;

      const auto now = std::chrono::steady_clock::now();
      const auto elapsed = now - lastFrame;
      lastFrame = now;
      // A connection that goes takes the player back to the menu, saying why (ADR-078).
      std::optional<std::string> connectionLost;
      if (match)
      {
        // Taken every frame, logged or not, so that they do not pile up. This is also where a failure on the server's thread
        // reaches this one. A world on a dedicated server has no server here.
        for (const Outpost::TickTiming& tick : match->server ? match->server->TakeTickTimings() : std::vector<Outpost::TickTiming>())
        {
          if (!measure)
            continue;
          measurements << std::format("tick_ns {}\ntick_parts_ns", tick.total.count());
          for (const std::chrono::nanoseconds part : tick.parts)
            measurements << ' ' << part.count();
          measurements << '\n';
        }

        std::vector<Outpost::Snapshot> snapshots;
        try
        {
          snapshots = match->player->Receive();
        }
        catch (const Neuron::Exception& error)
        {
          connectionLost = error.what();
        }
        if (load)
        {
          for (const Outpost::Snapshot& snapshot : snapshots)
          {
            for (Outpost::Command& command : match->playerLoad.Update(snapshot))
              match->player->Send(std::move(command));
          }
        }
        // The AI plays as a client: its snapshots in, its orders out, as the human's (ADR-002).
        for (const Outpost::Snapshot& snapshot : match->rival ? match->rival->Receive() : std::vector<Outpost::Snapshot>())
        {
          if (match->log)
            match->log->Record(snapshot);
          if (load)
          {
            for (Outpost::Command& command : match->rivalLoad.Update(snapshot))
              match->rival->Send(std::move(command));
          }
          else if (match->ai)
          {
            for (Outpost::Command& command : match->ai->Update(snapshot))
              match->rival->Send(std::move(command));
          }
          else if (match->battle)
          {
            for (Outpost::Command& command : match->battle->Update(snapshot))
              match->rival->Send(std::move(command));
          }
        }
        if (match->log)
        {
          for (const Outpost::Snapshot& snapshot : snapshots)
            match->log->Record(snapshot);
        }
        // The client draws only what the snapshots say, interpolated (ADR-002 decision 5, ADR-013).
        client.Receive(std::move(snapshots));
      }
      if (connectionLost)
      {
        if (match->log)
          match->log->Finish();
        match.reset();
        offerWorld();
        client.ShowMenu(std::format("The connection to the server was lost: {}", *connectionLost));
        if (!nextServer)
        {
          seed = NewSeed();
          nextServer =
            Outpost::CreateInProcessServer({.seed = seed, .measurementLoad = load, .stressLoad = stress, .quic = true, .matchup = matchup});
        }
      }

      // Read every loop, minimized too, so that the cursor is let go as soon as the game loses the foreground (ADR-012).
      const Neuron::InputState input = window.ReadInput();
      if (window.IsMinimized())
        continue;

      renderer.Resize(window.ClientWidthPixels(), window.ClientHeightPixels());
      client.Update(input, std::chrono::duration<float>(elapsed).count(), renderer.WidthPixels(), renderer.HeightPixels());
      // Orders leave as soon as they are given; the server applies them at the start of its next tick (ADR-002).
      for (Outpost::Command& command : client.TakeCommands())
      {
        if (match)
          match->player->Send(std::move(command));
      }
      // The menu's buttons and the match end's (task 6.2). Leaving a match ends its server, and the next one is made.
      if (const std::optional<Outpost::GameClient::Request> request = client.TakeRequest())
      {
        switch (*request)
        {
        case Outpost::GameClient::Request::StartSkirmish:
          if (!match)
            startMatch();
          break;
        case Outpost::GameClient::Request::BackToMenu:
          if (match && match->log)
            match->log->Finish();
          match.reset();
          offerWorld();
          client.ShowMenu();
          // A world joined on a dedicated server left the next skirmish's server unused.
          if (!nextServer)
          {
            seed = NewSeed();
            nextServer = Outpost::CreateInProcessServer(
              {.seed = seed, .measurementLoad = load, .stressLoad = stress, .quic = true, .matchup = matchup});
          }
          break;
        case Outpost::GameClient::Request::JoinWorld:
          if (!match)
            joinWorld(documentsJoinFile);
          break;
        case Outpost::GameClient::Request::Quit:
          PostQuitMessage(0);
          break;
        }
      }
      ID3D12GraphicsCommandList* commandList = renderer.BeginFrame(CLEAR_COLOR);
      client.Render(renderer, commandList);
      // The scene is resolved into the back buffer, and the interface drawn over it (ADR-050).
      renderer.BeginInterface();
      client.RenderInterface(commandList, renderer.FrameIndex());
      renderer.EndFrame();
      if (!firstFramePresented)
      {
        firstFramePresented = true;
        logStage("first_frame", std::chrono::steady_clock::now());
      }

      // Q4: a frame's work on the CPU, up to the return from Present, and on the GPU, resolved a few frames later.
      const auto frameEnded = std::chrono::steady_clock::now();
      for (const std::chrono::nanoseconds gpuTime : renderer.TakeGpuFrameTimes())
      {
        if (measure)
          measurements << std::format("frame_gpu_ns {}\n", gpuTime.count());
      }
      if (measure)
      {
        if (renderer.WidthPixels() != loggedWidthPixels || renderer.HeightPixels() != loggedHeightPixels)
        {
          loggedWidthPixels = renderer.WidthPixels();
          loggedHeightPixels = renderer.HeightPixels();
          measurements << std::format("display {} {} refresh_hz {}\n", loggedWidthPixels, loggedHeightPixels,
                                      RefreshRateHertz(window.Handle()));
        }
        measurements << std::format("frame_cpu_ns {}\n", (frameEnded - frameStarted).count());
      }

      // Q5: from reading the order's input to presenting the first frame that shows a ship of it respond (task 2.7).
      if (const std::optional<std::chrono::steady_clock::time_point> inputRead = client.TakeResponseShown(); measure && inputRead)
      {
        const auto presented = std::chrono::steady_clock::now();
        measurements << std::format("response_ns {} input_read_ns {} presented_ns {}\n", (presented - *inputRead).count(),
                                    Nanoseconds(*inputRead), Nanoseconds(presented));
        measurements.flush();
      }
    }
    // Quitting from a match leaves it.
    if (match && match->log)
      match->log->Finish();
    return window.ExitCode();
  }
  catch (const winrt::hresult_error& error)
  {
    const std::wstring message =
      std::format(L"{}\n\nError 0x{:08X}", std::wstring_view(error.message()), static_cast<std::uint32_t>(error.code()));
    ReportFailure(message, quiet);
    return error.code();
  }
  catch (const std::exception& error)
  {
    // Neuron::Exception carries UTF-8, such as the tuning loader's report of a bad data file (ADR-008).
    const winrt::hstring message = winrt::to_hstring(std::string_view(error.what()));
    ReportFailure(message, quiet);
    return EXIT_FAILURE;
  }
}