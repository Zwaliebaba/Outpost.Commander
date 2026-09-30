#include "pch.h"
#include "Window.h"

// The executable's Windows Store project type does not link the windowing and GDI import libraries by default. They are
// named here, by the one file that uses them, so every binary that links NeuronClient gets them, as NeuronCore.h does
// for COM.
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace
{
constexpr const wchar_t* WINDOW_CLASS_NAME = L"NeuronWindow";
constexpr DWORD FULL_SCREEN_STYLE = WS_POPUP;
constexpr DWORD WINDOWED_STYLE = WS_OVERLAPPEDWINDOW;
constexpr DWORD WINDOW_EX_STYLE = 0;

// In a keystroke's lParam, bit 29 is set while Alt is held and bit 30 when the key was already down (an auto-repeat).
constexpr LPARAM ALT_DOWN_BIT = LPARAM{1} << 29;
constexpr LPARAM REPEAT_BIT = LPARAM{1} << 30;

// The monitor's full area in physical pixels, since the process is per-monitor DPI aware (app.manifest).
bool MonitorArea(HMONITOR _monitor, RECT& _outArea) noexcept
{
  MONITORINFO info{.cbSize = sizeof(MONITORINFO)};
  if (GetMonitorInfoW(_monitor, &info) == FALSE)
    return false;
  _outArea = info.rcMonitor;
  return true;
}

RECT ClientArea(HWND _hwnd) noexcept
{
  RECT area{};
  GetClientRect(_hwnd, &area);
  return area;
}

// A windowed frame whose client area has the given size, centered on the work area of the window's monitor.
RECT CenteredWindowedFrame(HWND _hwnd, std::uint32_t _clientWidthPixels, std::uint32_t _clientHeightPixels) noexcept
{
  MONITORINFO info{.cbSize = sizeof(MONITORINFO)};
  GetMonitorInfoW(MonitorFromWindow(_hwnd, MONITOR_DEFAULTTOPRIMARY), &info);
  const RECT& work = info.rcWork;

  RECT frame{
    .left = 0,
    .top = 0,
    .right = static_cast<LONG>(_clientWidthPixels),
    .bottom = static_cast<LONG>(_clientHeightPixels),
  };
  AdjustWindowRectExForDpi(&frame, WINDOWED_STYLE, FALSE, WINDOW_EX_STYLE, GetDpiForWindow(_hwnd));
  const LONG widthPixels = frame.right - frame.left;
  const LONG heightPixels = frame.bottom - frame.top;
  const LONG left = work.left + (work.right - work.left - widthPixels) / 2;
  const LONG top = work.top + (work.bottom - work.top - heightPixels) / 2;
  return RECT{.left = left, .top = top, .right = left + widthPixels, .bottom = top + heightPixels};
}

// Alt+Enter, but not its auto-repeat, so that holding the keys toggles once (ADR-006).
bool IsFullScreenToggle(const MSG& _message) noexcept
{
  return _message.message == WM_SYSKEYDOWN && _message.wParam == VK_RETURN && (_message.lParam & ALT_DOWN_BIT) != 0 &&
         (_message.lParam & REPEAT_BIT) == 0;
}

void PlaceWindow(HWND _hwnd, DWORD _style, const RECT& _frame) noexcept
{
  SetWindowLongPtrW(_hwnd, GWL_STYLE, static_cast<LONG_PTR>(_style | WS_VISIBLE));
  SetWindowPos(_hwnd, nullptr, _frame.left, _frame.top, _frame.right - _frame.left, _frame.bottom - _frame.top,
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
} // namespace

Neuron::Window::Window(const Desc& _desc)
  : m_instance(GetModuleHandleW(nullptr)),
    m_windowedClientWidthPixels(_desc.windowedClientWidthPixels),
    m_windowedClientHeightPixels(_desc.windowedClientHeightPixels)
{
  // Borderless full screen (ADR-006): a popup exactly covering the primary monitor. Measured before the class is
  // registered, so that nothing is left to undo if it throws.
  RECT monitor{};
  winrt::check_bool(MonitorArea(MonitorFromPoint(POINT{.x = 0, .y = 0}, MONITOR_DEFAULTTOPRIMARY), monitor));

  const WNDCLASSEXW windowClass{
    .cbSize = sizeof(WNDCLASSEXW),
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = WindowProc,
    .cbClsExtra = 0,
    .cbWndExtra = 0,
    .hInstance = m_instance,
    .hIcon = nullptr,
    .hCursor = LoadCursorW(nullptr, IDC_ARROW),
    .hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
    .lpszMenuName = nullptr,
    .lpszClassName = WINDOW_CLASS_NAME,
    .hIconSm = nullptr,
  };
  winrt::check_bool(RegisterClassExW(&windowClass));

  m_hwnd = CreateWindowExW(WINDOW_EX_STYLE, WINDOW_CLASS_NAME, _desc.title, FULL_SCREEN_STYLE, monitor.left, monitor.top,
                           monitor.right - monitor.left, monitor.bottom - monitor.top, nullptr, nullptr, m_instance, nullptr);
  if (m_hwnd == nullptr)
  {
    const DWORD error = GetLastError();
    UnregisterClassW(WINDOW_CLASS_NAME, m_instance);
    winrt::throw_hresult(HRESULT_FROM_WIN32(error));
  }

  ShowWindow(m_hwnd, SW_SHOW);
}

Neuron::Window::~Window()
{
  // The window is already gone if the user closed it; otherwise it goes now, before its class.
  if (IsWindow(m_hwnd))
    DestroyWindow(m_hwnd);
  UnregisterClassW(WINDOW_CLASS_NAME, m_instance);
}

bool Neuron::Window::ProcessMessages() noexcept
{
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
  {
    if (message.message == WM_QUIT)
    {
      m_exitCode = static_cast<int>(message.wParam);
      return false;
    }
    // Handled here rather than in WindowProc, where the window's state is out of reach. Left untranslated, the
    // keystroke makes no WM_SYSCHAR, so Windows does not beep for an unused Alt shortcut.
    if (IsFullScreenToggle(message))
    {
      ToggleFullScreen();
      continue;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return true;
}

std::uint32_t Neuron::Window::ClientWidthPixels() const noexcept
{
  const RECT area = ClientArea(m_hwnd);
  return static_cast<std::uint32_t>(area.right - area.left);
}

std::uint32_t Neuron::Window::ClientHeightPixels() const noexcept
{
  const RECT area = ClientArea(m_hwnd);
  return static_cast<std::uint32_t>(area.bottom - area.top);
}

void Neuron::Window::ToggleFullScreen() noexcept
{
  if (m_fullScreen)
  {
    if (IsRectEmpty(&m_windowedFrame) != FALSE)
      m_windowedFrame = CenteredWindowedFrame(m_hwnd, m_windowedClientWidthPixels, m_windowedClientHeightPixels);
    PlaceWindow(m_hwnd, WINDOWED_STYLE, m_windowedFrame);
  }
  else
  {
    // Full screen covers whichever monitor the window is on now, which need not be the one it started on.
    RECT monitor{};
    if (!MonitorArea(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), monitor))
      return;
    GetWindowRect(m_hwnd, &m_windowedFrame);
    PlaceWindow(m_hwnd, FULL_SCREEN_STYLE, monitor);
  }
  m_fullScreen = !m_fullScreen;
}

LRESULT CALLBACK Neuron::Window::WindowProc(HWND _hwnd, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept
{
  switch (_message)
  {
  case WM_DISPLAYCHANGE:
  {
    // The resolution changed or the monitors were rearranged. In full screen, cover the window's monitor again; a
    // window keeps its frame. The renderer sees the new client size on its next frame and resizes (ADR-006).
    RECT monitor{};
    const bool fullScreen = (GetWindowLongPtrW(_hwnd, GWL_STYLE) & static_cast<LONG_PTR>(FULL_SCREEN_STYLE)) != 0;
    if (fullScreen && MonitorArea(MonitorFromWindow(_hwnd, MONITOR_DEFAULTTOPRIMARY), monitor))
      SetWindowPos(_hwnd, nullptr, monitor.left, monitor.top, monitor.right - monitor.left, monitor.bottom - monitor.top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcW(_hwnd, _message, _wParam, _lParam);
  }
}
