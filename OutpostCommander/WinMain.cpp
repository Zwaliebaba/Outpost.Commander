#include "pch.h"
#include "AiMatches.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
// Linear color; the render target view encodes it to sRGB (ADR-006). Black: the stars are all the sky has (ADR-021).
constexpr std::array<float, 4> CLEAR_COLOR{0.0f, 0.0f, 0.0f, 1.0f};
constexpr auto GAME_TITLE = L"Outpost Commander";
// Minimized there is no frame to wait for, so the loop wakes this often to let the simulation run on (ADR-009).
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
// Phase 1 plan task 13.1's switch: ten seeded AI-against-AI matches on the real server, played headlessly as fast as it
// ticks, into this log in the temporary folder, for P1's repeatable figure. No window opens; a message says when they
// are done, and Tools/MatchLog.py --ai-matches summarizes them.
constexpr std::wstring_view AI_MATCHES_SWITCH = L"--ai-matches";
constexpr auto AI_MATCH_LOG = L"OutpostCommander-ai-matches.log";

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

// A seed for one match (ADR-009), logged so that the match can be reproduced from it and its command log.
std::uint64_t NewSeed()
{
  const auto seed = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
  OutputDebugStringA(std::format("Match seed {}\n", seed).c_str());
  return seed;
}

// One match: its server, the human's connection, the other player's with whatever drives it, and its log.
struct Match
{
  std::unique_ptr<Outpost::Server> server;
  std::unique_ptr<Outpost::Transport> player;
  std::unique_ptr<Outpost::Transport> rival;
  std::optional<Outpost::AiPlayer> ai;
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
  // is the exit code, so the game never closes without saying why (ADR-006).
  try
  {
    wchar_t filename[MAX_PATH];
    GetModuleFileNameW(nullptr, filename, MAX_PATH);
    auto path = std::wstring(filename);
    path = path.substr(0, path.find_last_of('\\'));

    Neuron::FileSys::SetHomeDirectory(path);

    const std::wstring_view commandLine = _cmdLine != nullptr ? std::wstring_view(_cmdLine) : std::wstring_view();
    if (commandLine.find(AI_MATCHES_SWITCH) != std::wstring_view::npos)
    {
      const std::filesystem::path logPath = std::filesystem::temp_directory_path() / AI_MATCH_LOG;
      std::ofstream log(logPath, std::ios::trunc);
      const Outpost::AiMatchesDesc desc;
      const auto started = std::chrono::steady_clock::now();
      const std::uint32_t ended = Outpost::PlayAiMatches(log, Outpost::LoadPackagedAiSettings(), desc);
      const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
      const std::wstring message = std::format(L"Played {} AI-against-AI matches in {} seconds: {} ended within {} minutes.\n\n"
                                               L"The log is {}.\nTools\\MatchLog.py --ai-matches summarizes them.",
                                               desc.matches, seconds, ended, desc.limitMinutes, logPath.wstring());
      MessageBoxW(nullptr, message.c_str(), GAME_TITLE, MB_OK | MB_ICONINFORMATION);
      return EXIT_SUCCESS;
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

    // The next match's server is made while the menu shows, and the first before the window opens, so that bad tuning
    // or map data is reported before the screen goes full screen; so are the AI's settings.
    const Outpost::ServerDesc serverDesc{.seed = seed, .measurementLoad = load, .stressLoad = stress};
    std::unique_ptr<Outpost::Server> nextServer = Outpost::CreateInProcessServer(serverDesc);
    const std::uint32_t ticksPerSecond = nextServer->TicksPerSecond();
    const Outpost::AiSettings aiSettings = Outpost::LoadPackagedAiSettings();
    std::optional<Match> match;

    Neuron::Window window({.title = GAME_TITLE, .windowedClientWidthPixels = 1280, .windowedClientHeightPixels = 720});
    Neuron::Renderer renderer(window.Handle(), window.ClientWidthPixels(), window.ClientHeightPixels());
    // Loads every model before the first frame, so a missing or broken mesh is reported rather than skipped (ADR-011).
    Outpost::GameClient client(renderer, ticksPerSecond);

    auto lastFrame = std::chrono::steady_clock::now();
    const auto startMatch = [&]
    {
      match.emplace();
      match->server = std::move(nextServer);
      match->player = match->server->Connect(HUMAN_PLAYER);
      // The rival always connects, so that the server builds both players' snapshots as in a match against the AI. Under
      // load the rival's ships are kept moving through it; the stress scene orders its own; otherwise it is the AI.
      match->rival = match->server->Connect(RIVAL_PLAYER);
      if (!load && !stress)
      {
        match->ai.emplace(aiSettings, ticksPerSecond);
        match->logFile.open(std::filesystem::temp_directory_path() / MATCH_LOG, std::ios::app);
        match->log.emplace(match->logFile, seed, ticksPerSecond);
      }
      // Its ticks run on the server's own thread from here, at their fixed rate whatever the frame rate (ADR-025).
      match->server->Start();
      client.StartMatch();
      lastFrame = std::chrono::steady_clock::now();
    };
    if (skipMenu)
      startMatch();
    UINT loggedWidthPixels = 0;
    UINT loggedHeightPixels = 0;
    for (;;)
    {
      // Minimized, there is nothing to show, so the loop sleeps until a message arrives or the simulation is due to run.
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
      if (match)
      {
        // Taken every frame, logged or not, so that they do not pile up. This is also where a failure on the server's thread
        // reaches this one.
        for (const Outpost::TickTiming& tick : match->server->TakeTickTimings())
        {
          if (!measure)
            continue;
          measurements << std::format("tick_ns {}\ntick_parts_ns", tick.total.count());
          for (const std::chrono::nanoseconds part : tick.parts)
            measurements << ' ' << part.count();
          measurements << '\n';
        }

        std::vector<Outpost::Snapshot> snapshots = match->player->Receive();
        if (load)
        {
          for (const Outpost::Snapshot& snapshot : snapshots)
          {
            for (Outpost::Command& command : match->playerLoad.Update(snapshot))
              match->player->Send(std::move(command));
          }
        }
        // The AI plays as a client: its snapshots in, its orders out, as the human's (ADR-002).
        for (const Outpost::Snapshot& snapshot : match->rival->Receive())
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
        }
        if (match->log)
        {
          for (const Outpost::Snapshot& snapshot : snapshots)
            match->log->Record(snapshot);
        }
        // The client draws only what the snapshots say, interpolated (ADR-002 decision 5, ADR-013).
        client.Receive(std::move(snapshots));
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
          client.ShowMenu();
          seed = NewSeed();
          nextServer = Outpost::CreateInProcessServer({.seed = seed, .measurementLoad = load, .stressLoad = stress});
          break;
        case Outpost::GameClient::Request::Quit:
          PostQuitMessage(0);
          break;
        }
      }
      ID3D12GraphicsCommandList* commandList = renderer.BeginFrame(CLEAR_COLOR);
      client.Render(renderer, commandList);
      renderer.EndFrame();

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
    MessageBoxW(nullptr, message.c_str(), GAME_TITLE, MB_OK | MB_ICONERROR);
    return error.code();
  }
  catch (const std::exception& error)
  {
    // Neuron::Exception carries UTF-8, such as the tuning loader's report of a bad data file (ADR-008).
    const winrt::hstring message = winrt::to_hstring(std::string_view(error.what()));
    MessageBoxW(nullptr, message.c_str(), GAME_TITLE, MB_OK | MB_ICONERROR);
    return EXIT_FAILURE;
  }
}