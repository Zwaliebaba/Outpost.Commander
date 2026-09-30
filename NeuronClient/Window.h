#pragma once

namespace Neuron
{
// The top-level window the game presents into (ADR-006). It opens borderless and full screen on the primary monitor,
// and Alt+Enter toggles it to a resizable window and back. It knows no game concept: its title and windowed size come
// from the caller.
class Window : NonCopyable
{
public:
  struct Desc
  {
    const wchar_t* title;
    // The client area the window gets the first time it leaves full screen, centered on its monitor's work area.
    std::uint32_t windowedClientWidthPixels;
    std::uint32_t windowedClientHeightPixels;
  };

  // Registers the window class, creates the window over the primary monitor and shows it. Throws winrt::hresult_error
  // on failure.
  explicit Window(const Desc& _desc);
  ~Window();

  // Dispatches every message waiting in the queue and returns. False once WM_QUIT has arrived: the caller's loop ends
  // and ExitCode() holds the code WM_QUIT carried. Alt+Enter is handled here, and toggles full screen.
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

  // The client area in physical pixels: the monitor's resolution in full screen, the window's inside it otherwise, and
  // zero while minimized.
  [[nodiscard]] std::uint32_t ClientWidthPixels() const noexcept;
  [[nodiscard]] std::uint32_t ClientHeightPixels() const noexcept;

private:
  // Between borderless full screen over the window's current monitor and the last windowed frame of this session.
  void ToggleFullScreen() noexcept;

  static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept;

  HINSTANCE m_instance = nullptr;
  HWND m_hwnd = nullptr;
  // The window's frame, in screen coordinates, the last time it was windowed; empty until it first is.
  RECT m_windowedFrame{};
  std::uint32_t m_windowedClientWidthPixels = 0;
  std::uint32_t m_windowedClientHeightPixels = 0;
  int m_exitCode = 0;
  bool m_fullScreen = true;
};
} // namespace Neuron
