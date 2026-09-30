#pragma once

#include "MainWindow.g.h"

namespace winrt::OutpostCommander::implementation
{

/// The one window the game runs in: a SwapChainPanel that D3D12 renders into, with the HUD as XAML over it (ADR-001).
struct MainWindow : MainWindowT<MainWindow>
{
};

} // namespace winrt::OutpostCommander::implementation

namespace winrt::OutpostCommander::factory_implementation
{

struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};

} // namespace winrt::OutpostCommander::factory_implementation
