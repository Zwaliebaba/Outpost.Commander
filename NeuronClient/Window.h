#pragma once

namespace Neuron
{
// The top-level window the game presents into. It knows no game concept: its title and size come from the caller.
// Style, resizing and full-screen behavior are the renderer's decision (implementation plan, gate G1), so this is a
// plain overlapped window until that decision is taken.
class Window : NonCopyable
{
public:
  struct Desc
  {
    const wchar_t* title;
    std::uint32_t clientWidthPixels;
    std::uint32_t clientHeightPixels;
  };

  // Registers the window class, creates the window and shows it. Throws winrt::hresult_error on failure.
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

private:
  static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept;

  HINSTANCE m_instance = nullptr;
  HWND m_hwnd = nullptr;
  int m_exitCode = 0;
};
} // namespace Neuron
