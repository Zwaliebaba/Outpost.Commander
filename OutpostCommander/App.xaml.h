#pragma once

#include "App.xaml.g.h"

namespace winrt::OutpostCommander::implementation
{

/// The application object. It opens the one window the game runs in and keeps it for the life of the process.
struct App : AppT<App>
{
  void OnLaunched(const Microsoft::UI::Xaml::LaunchActivatedEventArgs& _args);

private:
  Microsoft::UI::Xaml::Window m_window{nullptr};
};

} // namespace winrt::OutpostCommander::implementation
