#pragma once

namespace Neuron
{
// A press or a release that happened since the last frame, in the order it happened (ADR-012). Held state alone would
// miss a click that goes down and up between two frames.
enum class InputEventKind : std::uint8_t
{
  KeyDown,
  ButtonDown,
  ButtonUp,
  // A character typed, for text input: what the keyboard layout made of the keys, auto-repeats included.
  Character
};

struct InputEvent
{
  InputEventKind kind = InputEventKind::KeyDown;
  // The virtual-key code: a key, or VK_LBUTTON, VK_RBUTTON or VK_MBUTTON for a mouse button. Zero for a character.
  std::uint8_t key = 0;
  // A character's UTF-16 code unit, as WM_CHAR carries it; zero for anything else.
  std::uint32_t character = 0;
  // The cursor in the client area when it happened, in physical pixels.
  std::int32_t xPixels = 0;
  std::int32_t yPixels = 0;
  // When it happened, in the system's millisecond clock, for telling a double click from two clicks.
  std::uint32_t timeMilliseconds = 0;
  // When the game read it from the queue, on std::chrono::steady_clock, for measuring latency (task 2.7).
  std::chrono::steady_clock::time_point read;
  bool shift = false;
  bool control = false;
};

// The mouse and keyboard as one frame sees them, read from the window once per frame (ADR-012). It knows no game
// concept: which key does what is the caller's.
struct InputState
{
  // The window is in the foreground and not minimized. When it is not, no key reads as down.
  bool active = false;
  // The cursor is held inside the window, so that it can reach the screen's edges without leaving the game (ADR-012).
  bool cursorClipped = false;
  // The cursor in the client area, in physical pixels from its top-left corner; it may be outside the client area.
  std::int32_t cursorXPixels = 0;
  std::int32_t cursorYPixels = 0;
  // How far the wheel turned since the last read, in notches; positive is away from the player.
  float wheelNotches = 0.0f;
  // Indexed by virtual-key code, mouse buttons included (VK_MBUTTON).
  std::bitset<256> keysDown;
  // Key presses, not their auto-repeats, and mouse button presses and releases since the last read. Empty while the
  // window is not active.
  std::vector<InputEvent> events;

  [[nodiscard]] bool IsDown(std::uint8_t _virtualKey) const noexcept
  {
    return keysDown.test(_virtualKey);
  }
};

// The top-level window the game presents into (ADR-006). It opens borderless and full screen on the primary monitor,
// and Alt+Enter toggles it to a resizable window and back. Alt alone does not open the window's menu, so that a game may
// read it as a key. It knows no game concept: its title and windowed size come from the caller.
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
  [[nodiscard]] bool ProcessMessages();

  // The input for this frame, after ProcessMessages. It also holds the cursor inside the window while the game is in the
  // foreground and full screen, and lets it go otherwise (ADR-012). The wheel's movement is counted once.
  [[nodiscard]] InputState ReadInput() noexcept;

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
  // The wheel's movement since the last ReadInput, in the units WM_MOUSEWHEEL counts.
  int m_wheelDelta = 0;
  // Presses and releases since the last ReadInput.
  std::vector<InputEvent> m_events;
  std::uint32_t m_windowedClientWidthPixels = 0;
  std::uint32_t m_windowedClientHeightPixels = 0;
  int m_exitCode = 0;
  bool m_fullScreen = true;
  bool m_cursorClipped = false;
};
} // namespace Neuron