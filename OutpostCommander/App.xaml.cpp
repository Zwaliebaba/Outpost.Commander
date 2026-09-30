#include "pch.h"

#include "App.xaml.h"
#include "MainWindow.xaml.h"

namespace winrt::OutpostCommander::implementation
{

void App::OnLaunched([[maybe_unused]] const Microsoft::UI::Xaml::LaunchActivatedEventArgs& _args)
{
  m_window = make<MainWindow>();
  m_window.Activate();
}

} // namespace winrt::OutpostCommander::implementation
