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
constexpr DWORD WINDOW_STYLE = WS_OVERLAPPEDWINDOW;
constexpr DWORD WINDOW_EX_STYLE = 0;
} // namespace

Neuron::Window::Window(const Desc& _desc)
  : m_instance(GetModuleHandleW(nullptr))
{
  // The size asked for is the client area in physical pixels. The process is per-monitor DPI aware (app.manifest), so
  // the frame around it is measured at the DPI the window opens at. This comes before the class is registered, so that
  // nothing is left to undo if it throws.
  RECT frame{
    .left = 0,
    .top = 0,
    .right = static_cast<LONG>(_desc.clientWidthPixels),
    .bottom = static_cast<LONG>(_desc.clientHeightPixels),
  };
  winrt::check_bool(AdjustWindowRectExForDpi(&frame, WINDOW_STYLE, FALSE, WINDOW_EX_STYLE, GetDpiForSystem()));

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

  m_hwnd = CreateWindowExW(WINDOW_EX_STYLE, WINDOW_CLASS_NAME, _desc.title, WINDOW_STYLE, CW_USEDEFAULT, CW_USEDEFAULT,
                           frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, m_instance, nullptr);
  if (m_hwnd == nullptr)
  {
    const DWORD error = GetLastError();
    UnregisterClassW(WINDOW_CLASS_NAME, m_instance);
    winrt::throw_hresult(HRESULT_FROM_WIN32(error));
  }

  ShowWindow(m_hwnd, SW_SHOWDEFAULT);
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

LRESULT CALLBACK Neuron::Window::WindowProc(HWND _hwnd, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept
{
  if (_message == WM_DESTROY)
  {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(_hwnd, _message, _wParam, _lParam);
}
