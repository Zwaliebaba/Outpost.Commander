#include "pch.h"

#include <string>

// The packaged client, and the whole of it: an IFrameworkView over a CoreWindow that activates,
// drains its dispatcher and exits. There is no XAML anywhere in this tree and no SwapChainPanel.
//
// This project holds Windows Runtime glue and nothing else (AGENTS.md R20). Anything a suite could
// pin belongs below it in NeuronClient or GameClient -- a thing an Application holds is a thing no
// suite can reach.

using winrt::Windows::ApplicationModel::Core::CoreApplication;
using winrt::Windows::ApplicationModel::Core::CoreApplicationView;
using winrt::Windows::ApplicationModel::Core::IFrameworkView;
using winrt::Windows::ApplicationModel::Core::IFrameworkViewSource;
using winrt::Windows::UI::Core::CoreDispatcher;
using winrt::Windows::UI::Core::CoreProcessEventsOption;
using winrt::Windows::UI::Core::CoreWindow;

namespace
{
void ReportLibrary(std::string_view _name)
{
  std::string line{_name};
  line.push_back('\n');
  OutputDebugStringA(line.c_str());
}

struct App : winrt::implements<App, IFrameworkViewSource, IFrameworkView>
{
  IFrameworkView CreateView()
  {
    return *this;
  }

  void Initialize(const CoreApplicationView&)
  {
    // The one thing this shell does today: name the libraries it was linked against, so that a
    // deploy proves the whole chain reached the package rather than merely compiled.
    ReportLibrary(Neuron::CoreLibraryName());
    ReportLibrary(Neuron::ClientLibraryName());
    ReportLibrary(Outpost::CoreLibraryName());
    ReportLibrary(Outpost::ClientLibraryName());
  }

  void Load(const winrt::hstring&) {}

  void SetWindow(const CoreWindow&) {}

  void Run()
  {
    const CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    const CoreDispatcher dispatcher = window.Dispatcher();
    dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
  }

  void Uninitialize() {}
};
} // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
  CoreApplication::Run(winrt::make<App>());
  return 0;
}