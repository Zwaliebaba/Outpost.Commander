#include "pch.h"
#include "ServerConsole.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>

namespace
{
// How long the handler of a close, a sign-out or a shutdown waits for the server: Windows gives it 5 seconds for a close
// and more for the others, and ends the process when it returns or the time is up.
constexpr std::chrono::seconds FINISH_WAIT{5};

// The console's state, which the handler, on a thread of Windows', and the server's main thread share under g_mutex.
std::mutex g_mutex;
std::condition_variable g_changed;
bool g_stopAsked = false;
bool g_finished = false;

BOOL WINAPI OnControl(DWORD _event) noexcept
{
  std::unique_lock lock(g_mutex);
  g_stopAsked = true;
  g_changed.notify_all();
  // After a close, a sign-out or a shutdown, Windows ends the process as this returns.
  if (_event == CTRL_CLOSE_EVENT || _event == CTRL_LOGOFF_EVENT || _event == CTRL_SHUTDOWN_EVENT)
    (void)g_changed.wait_for(lock, FINISH_WAIT, [] { return g_finished; });
  return TRUE;
}

void WriteLine(std::FILE* _stream, std::string_view _line)
{
  (void)std::fwrite(_line.data(), 1, _line.size(), _stream);
  (void)std::fputc('\n', _stream);
  (void)std::fflush(_stream);
}
} // namespace

Outpost::ServerConsole::ServerConsole()
{
  (void)SetConsoleOutputCP(CP_UTF8);
  winrt::check_bool(SetConsoleCtrlHandler(&OnControl, TRUE));
}

Outpost::ServerConsole::~ServerConsole()
{
  (void)SetConsoleCtrlHandler(&OnControl, FALSE);
}

bool Outpost::ServerConsole::WaitForStop(std::chrono::milliseconds _wait)
{
  std::unique_lock lock(g_mutex);
  return g_changed.wait_for(lock, _wait, [] { return g_stopAsked; });
}

void Outpost::ServerConsole::Finished() noexcept
{
  {
    const std::scoped_lock lock(g_mutex);
    g_finished = true;
  }
  g_changed.notify_all();
}

void Outpost::ServerConsole::Print(std::string_view _line)
{
  WriteLine(stdout, _line);
}

void Outpost::ServerConsole::PrintError(std::string_view _line)
{
  WriteLine(stderr, _line);
}
