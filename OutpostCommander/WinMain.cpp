#include "pch.h"

int WINAPI wWinMain([[maybe_unused]] HINSTANCE _instance, [[maybe_unused]] HINSTANCE _previousInstance, [[maybe_unused]] LPWSTR _cmdLine,
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

  return 0;
}
