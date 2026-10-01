#include "pch.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
// Linear color; the render target view encodes it to sRGB (ADR-006).
constexpr std::array<float, 4> CLEAR_COLOR{0.0f, 0.02f, 0.05f, 1.0f};
constexpr auto GAME_TITLE = L"Outpost Commander";
// Minimized there is no frame to wait for, so the loop wakes this often to let the simulation run on (ADR-009).
constexpr DWORD MINIMIZED_WAKE_MILLISECONDS = 16;
// The human player. The AI connects as another (task 6.1).
constexpr Outpost::PlayerId HUMAN_PLAYER{1};
// The other player, which only a measurement run drives until the AI does (task 6.1).
constexpr Outpost::PlayerId RIVAL_PLAYER{2};

// Task 2.7's switches. --measure logs every tick's duration and every move order's time to its first visible response
// to MEASUREMENT_LOG in the temporary folder; --load adds the load of 200 ships and 40 structures, and keeps both fleets
// moving. Times are on std::chrono::steady_clock, which counts QueryPerformanceCounter, so a script injecting input
// can line its own timestamps up with the game's.
constexpr std::wstring_view MEASURE_SWITCH = L"--measure";
constexpr std::wstring_view LOAD_SWITCH = L"--load";
// Task 3.7's switch: the stress scene of 200 ships and 40 structures in combat, kept at full size (StressLoad). It takes
// the place of --load. With --measure, every frame's CPU and GPU time is logged as well, with the back buffer's size and
// the display's refresh rate whenever the size changes, for Q4 (ADR-006).
constexpr std::wstring_view STRESS_SWITCH = L"--stress";
constexpr auto MEASUREMENT_LOG = L"OutpostCommander-measure.log";

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

    // The match's one seed (ADR-009), logged so that a match can be reproduced from it and its command log. The server
    // starts before the window, so bad tuning data is reported before the screen goes full screen.
    const auto seed = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    OutputDebugStringA(std::format("Match seed {}\n", seed).c_str());
    const std::wstring_view commandLine = _cmdLine != nullptr ? std::wstring_view(_cmdLine) : std::wstring_view();
    const bool measure = commandLine.find(MEASURE_SWITCH) != std::wstring_view::npos;
    const bool stress = commandLine.find(STRESS_SWITCH) != std::wstring_view::npos;
    const bool load = !stress && commandLine.find(LOAD_SWITCH) != std::wstring_view::npos;
    std::ofstream measurements;
    if (measure)
    {
      measurements.open(std::filesystem::temp_directory_path() / MEASUREMENT_LOG, std::ios::trunc);
      measurements << std::format("seed {} load {} stress {}\n", seed, load ? 1 : 0, stress ? 1 : 0);
    }

    const std::unique_ptr<Outpost::Server> server =
      Outpost::CreateInProcessServer({.seed = seed, .measurementLoad = load, .stressLoad = stress});
    const std::unique_ptr<Outpost::Transport> player = server->Connect(HUMAN_PLAYER);
    // Under load both players' ships are kept moving, the rival's through its own connection, as the AI's will be. The
    // stress scene orders its own ships, but the rival connects there too, so the server builds both players' snapshots
    // as it will in a match against the AI.
    const std::unique_ptr<Outpost::Transport> rival = load || stress ? server->Connect(RIVAL_PLAYER) : nullptr;
    Outpost::LoadDriver playerLoad;
    Outpost::LoadDriver rivalLoad;

    Neuron::Window window({.title = GAME_TITLE, .windowedClientWidthPixels = 1280, .windowedClientHeightPixels = 720});
    Neuron::Renderer renderer(window.Handle(), window.ClientWidthPixels(), window.ClientHeightPixels());
    // Loads every model before the first frame, so a missing or broken mesh is reported rather than skipped (ADR-011).
    Outpost::GameClient client(renderer, server->TicksPerSecond());

    auto lastAdvance = std::chrono::steady_clock::now();
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

      // The server runs on this thread, at its own fixed rate whatever the frame rate (ADR-009).
      const auto now = std::chrono::steady_clock::now();
      const auto elapsed = now - lastAdvance;
      server->Advance(elapsed);
      lastAdvance = now;
      // Taken every frame, logged or not, so that they do not pile up.
      for (const std::chrono::nanoseconds tick : server->TakeTickDurations())
      {
        if (measure)
          measurements << std::format("tick_ns {}\n", tick.count());
      }

      std::vector<Outpost::Snapshot> snapshots = player->Receive();
      if (load)
      {
        for (const Outpost::Snapshot& snapshot : snapshots)
        {
          for (Outpost::Command& command : playerLoad.Update(snapshot))
            player->Send(std::move(command));
        }
        for (const Outpost::Snapshot& snapshot : rival->Receive())
        {
          for (Outpost::Command& command : rivalLoad.Update(snapshot))
            rival->Send(std::move(command));
        }
      }
      else if (rival)
      {
        // Received and dropped, so that they do not pile up.
        (void)rival->Receive();
      }
      // The client draws only what the snapshots say, interpolated (ADR-002 decision 5, ADR-013).
      client.Receive(std::move(snapshots));

      // Read every loop, minimized too, so that the cursor is let go as soon as the game loses the foreground (ADR-012).
      const Neuron::InputState input = window.ReadInput();
      if (window.IsMinimized())
        continue;

      renderer.Resize(window.ClientWidthPixels(), window.ClientHeightPixels());
      client.Update(input, std::chrono::duration<float>(elapsed).count(), renderer.WidthPixels(), renderer.HeightPixels());
      // Orders leave as soon as they are given; the server applies them at the start of its next tick (ADR-002).
      for (Outpost::Command& command : client.TakeCommands())
        player->Send(std::move(command));
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