#include "pch.h"

#include "WindowsHeader.h"

#include <shellapi.h>

#include "App.h"

#include <string>
#include <vector>

// The game's entry point: the command line into LaunchOptions, then App::Run, whose exit code is
// the process's. A Windows-subsystem executable has no console, so a bad command line is reported
// through the exit code and the debugger output alone.
int WINAPI wWinMain(HINSTANCE /*_instance*/, HINSTANCE /*_previousInstance*/, PWSTR /*_commandLine*/, int /*_showCommand*/)
{
  int count = 0;
  wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
  if (arguments == nullptr)
  {
    return Outpost::EXIT_FAILED;
  }
  const std::vector<std::wstring> copied(arguments, arguments + count);
  LocalFree(static_cast<HLOCAL>(arguments));
  Outpost::LaunchOptions options;
  if (!Outpost::ParseCommandLine(copied, options))
  {
    OutputDebugStringW(L"usage: OutpostCommander [--warp] [--novsync] [--capture <landscape> <ticks> <directory>]\n");
    return Outpost::EXIT_FAILED;
  }
  Outpost::App app(options);
  return app.Run();
}
