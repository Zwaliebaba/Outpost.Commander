#include "pch.h"

#include <cstdlib>

namespace
{
// Linear color; the render target view encodes it to sRGB (ADR-006).
constexpr std::array<float, 4> CLEAR_COLOR{0.0f, 0.02f, 0.05f, 1.0f};
constexpr const wchar_t* GAME_TITLE = L"Outpost Commander";
// Minimized there is no frame to wait for, so the loop wakes this often to let the simulation run on (ADR-009).
constexpr DWORD MINIMIZED_WAKE_MILLISECONDS = 16;
// The human player. The AI connects as another (task 6.1).
constexpr Outpost::PlayerId HUMAN_PLAYER{1};
} // namespace

int WINAPI wWinMain([[maybe_unused]] HINSTANCE _hInstance, [[maybe_unused]] HINSTANCE _hPrevInstance, [[maybe_unused]] LPWSTR _cmdLine,
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
    const std::unique_ptr<Outpost::Server> server = Outpost::CreateInProcessServer({.seed = seed});
    const std::unique_ptr<Outpost::Transport> player = server->Connect(HUMAN_PLAYER);

    Neuron::Window window({.title = GAME_TITLE, .windowedClientWidthPixels = 1280, .windowedClientHeightPixels = 720});
    Neuron::Renderer renderer(window.Handle(), window.ClientWidthPixels(), window.ClientHeightPixels());

    auto lastAdvance = std::chrono::steady_clock::now();
    for (;;)
    {
      // Minimized, there is nothing to show, so the loop sleeps until a message arrives or the simulation is due to run.
      // Otherwise it waits until the swap chain can take a frame, and reads input right after, so the frame shows the
      // freshest input it can (ADR-006).
      if (window.IsMinimized())
        MsgWaitForMultipleObjects(0, nullptr, FALSE, MINIMIZED_WAKE_MILLISECONDS, QS_ALLINPUT);
      else
        renderer.WaitForNextFrame();

      if (!window.ProcessMessages())
        break;

      // The server runs on this thread, at its own fixed rate whatever the frame rate (ADR-009).
      const auto now = std::chrono::steady_clock::now();
      server->Advance(now - lastAdvance);
      lastAdvance = now;
      // Drawing from snapshots is task 2.5. Until then they are dropped, so that they do not pile up.
      (void)player->Receive();

      if (window.IsMinimized())
        continue;

      renderer.Resize(window.ClientWidthPixels(), window.ClientHeightPixels());
      renderer.RenderFrame(CLEAR_COLOR);
    }
    return window.ExitCode();
  }
  catch (const winrt::hresult_error& error)
  {
    const std::wstring message =
      std::format(L"{}\n\nError 0x{:08X}", std::wstring_view(error.message()), static_cast<std::uint32_t>(error.code()));
    MessageBoxW(nullptr, message.c_str(), GAME_TITLE, MB_OK | MB_ICONERROR);
    return static_cast<int>(error.code());
  }
  catch (const std::exception& error)
  {
    // Neuron::Exception carries UTF-8, such as the tuning loader's report of a bad data file (ADR-008).
    const winrt::hstring message = winrt::to_hstring(std::string_view(error.what()));
    MessageBoxW(nullptr, message.c_str(), GAME_TITLE, MB_OK | MB_ICONERROR);
    return EXIT_FAILURE;
  }
}
