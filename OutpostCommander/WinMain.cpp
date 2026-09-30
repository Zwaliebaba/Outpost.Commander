#include "pch.h"

int WINAPI wWinMain([[maybe_unused]] HINSTANCE _hInstance, [[maybe_unused]] HINSTANCE _hPrevInstance, [[maybe_unused]] LPWSTR _cmdLine,
                    [[maybe_unused]] int _cmdShow)
{
#if defined(_DEBUG)
//  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  wchar_t filename[MAX_PATH];
  GetModuleFileNameW(nullptr, filename, MAX_PATH);
  auto path = std::wstring(filename);
  path = path.substr(0, path.find_last_of('\\'));

  Neuron::FileSys::SetHomeDirectory(path);

  Neuron::Window window({.title = L"Outpost Commander", .clientWidthPixels = 1280, .clientHeightPixels = 720});
  while (window.ProcessMessages())
  {
    // A frame is rendered here once the renderer exists (implementation plan, task 1.2).
  }

  return window.ExitCode();
}
