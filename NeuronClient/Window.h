#pragma once

namespace Neuron
{
// The top-level window the game presents into: borderless and full screen on the primary monitor, always (ADR-006). It
// knows no game concept: its title comes from the caller.
class Window : NonCopyable
{
public:
  struct Desc
  {
    const wchar_t* title;
  };

  // Registers the window class, creates the window over the primary monitor and shows it. Throws winrt::hresult_error
  // on failure.
  explicit Window(const Desc& _desc);
  ~Window();

  // Dispatches every message waiting in the queue and returns. False once WM_QUIT has arrived: the caller's loop ends
  // and ExitCode() holds the code WM_QUIT carried.
  [[nodiscard]] bool ProcessMessages() noexcept;

  [[nodiscard]] int ExitCode() const noexcept
  {
    return m_exitCode;
  }
  [[nodiscard]] HWND Handle() const noexcept
  {
    return m_hwnd;
  }
  [[nodiscard]] bool IsMinimized() const noexcept
  {
    return IsIconic(m_hwnd) != FALSE;
  }

  // The client area in physical pixels: the monitor's resolution, or zero while minimized.
  [[nodiscard]] std::uint32_t ClientWidthPixels() const noexcept;
  [[nodiscard]] std::uint32_t ClientHeightPixels() const noexcept;

private:
  static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept;

  HINSTANCE m_instance = nullptr;
  HWND m_hwnd = nullptr;
  int m_exitCode = 0;
};
} // namespace Neuron
