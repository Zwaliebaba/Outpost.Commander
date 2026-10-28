#pragma once

namespace Outpost
{
// The dedicated server's console (ADR-078): the lines it prints, and the stop that Ctrl+C, Ctrl+Break,
// closing its window, signing out or shutting down asks for. Windows ends the process once the handler of a close, a
// sign-out or a shutdown returns, so the handler waits, a few seconds at most, until the server says it has finished and
// its universe's last save is written.
class ServerConsole : Neuron::NonCopyable
{
public:
  // Takes the console's control events from here on, and writes UTF-8. One at a time.
  ServerConsole();
  ~ServerConsole();

  // Waits up to _wait for a stop to be asked for; true once one has been.
  [[nodiscard]] bool WaitForStop(std::chrono::milliseconds _wait);

  // Whether Windows asked for the stop, signing out or shutting down, rather than the server's owner.
  [[nodiscard]] bool StoppedByWindows() const;

  // Says the server has finished, which lets a handler that waits for it return.
  void Finished() noexcept;

  // A line on standard output, or on standard error.
  static void Print(std::string_view _line);
  static void PrintError(std::string_view _line);
};
} // namespace Outpost
