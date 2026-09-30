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
constexpr DWORD WINDOW_STYLE = WS_POPUP;
constexpr DWORD WINDOW_EX_STYLE = 0;

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
} // namespace

Neuron::Window::Window(const Desc& _desc)
  : m_instance(GetModuleHandleW(nullptr))
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

  m_hwnd = CreateWindowExW(WINDOW_EX_STYLE, WINDOW_CLASS_NAME, _desc.title, WINDOW_STYLE, monitor.left, monitor.top,
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

LRESULT CALLBACK Neuron::Window::WindowProc(HWND _hwnd, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept
{
  switch (_message)
  {
  case WM_DISPLAYCHANGE:
  {
    // The resolution changed or the monitors were rearranged: cover the window's monitor again. The renderer sees the
    // new client size on its next frame and resizes the back buffers (ADR-006).
    RECT monitor{};
    if (MonitorArea(MonitorFromWindow(_hwnd, MONITOR_DEFAULTTOPRIMARY), monitor))
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
