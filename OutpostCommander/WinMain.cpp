#include "pch.h"

namespace
{
// Linear color; the render target view encodes it to sRGB (ADR-006).
constexpr std::array<float, 4> CLEAR_COLOR{0.0f, 0.02f, 0.05f, 1.0f};
constexpr const wchar_t* GAME_TITLE = L"Outpost Commander";
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

    Neuron::Window window({.title = GAME_TITLE});
    Neuron::Renderer renderer(window.Handle(), window.ClientWidthPixels(), window.ClientHeightPixels());

    for (;;)
    {
      // Minimized, there is nothing to show, so the loop sleeps until a message arrives. Otherwise it waits until the swap
      // chain can take a frame, and reads input right after, so the frame shows the freshest input it can (ADR-006).
      if (window.IsMinimized())
        WaitMessage();
      else
        renderer.WaitForNextFrame();

      if (!window.ProcessMessages())
        break;
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
}
