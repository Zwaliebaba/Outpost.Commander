#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Outpost::WindowKind;
using Point = Outpost::WindowManager::Point;

std::vector<WindowKind> Order(const Outpost::WindowManager& _windows)
{
  return {_windows.FrontToBack().begin(), _windows.FrontToBack().end()};
}
} // namespace

TEST_CLASS(WindowManagerTests)
{
public:
  // ADR-031: a window opens at the front, an open one opened again comes to the front, and Esc closes the front one.
  TEST_METHOD(OpensAtTheFrontAndClosesTheFrontFirst)
  {
    Outpost::WindowManager windows;
    Assert::IsFalse(windows.CloseFront(), L"nothing to close");
    windows.Open(WindowKind::Designer);
    windows.Open(WindowKind::Research);
    Assert::IsTrue(Order(windows) == std::vector{WindowKind::Research, WindowKind::Designer});
    windows.Open(WindowKind::Designer);
    Assert::IsTrue(Order(windows) == std::vector{WindowKind::Designer, WindowKind::Research});

    Assert::IsTrue(windows.CloseFront());
    Assert::IsFalse(windows.IsOpen(WindowKind::Designer));
    Assert::IsTrue(windows.IsOpen(WindowKind::Research));
    windows.CloseAll();
    Assert::IsTrue(windows.FrontToBack().empty());
  }

  // ADR-031: a grabbed window follows the pointer, keeping where it was taken hold of, until it is let go; it comes to
  // the front; and it opens again where it was left, for as long as the manager lasts.
  TEST_METHOD(DragsByWhereItWasTakenHold)
  {
    Outpost::WindowManager windows;
    windows.Open(WindowKind::Designer);
    windows.Open(WindowKind::Research);
    Assert::IsFalse(windows.PositionOf(WindowKind::Designer).has_value(), L"the HUD places it until it is moved");

    windows.Grab(WindowKind::Designer, {.xUnits = 110.0f, .yUnits = 60.0f}, {.xUnits = 100.0f, .yUnits = 50.0f});
    Assert::IsTrue(windows.IsDragging());
    Assert::IsTrue(windows.FrontToBack().front() == WindowKind::Designer);
    windows.Drag({.xUnits = 300.0f, .yUnits = 200.0f});
    Assert::IsTrue(windows.PositionOf(WindowKind::Designer) == Point{.xUnits = 290.0f, .yUnits = 190.0f});
    windows.Release();
    windows.Drag({.xUnits = 0.0f, .yUnits = 0.0f});
    Assert::IsTrue(windows.PositionOf(WindowKind::Designer) == Point{.xUnits = 290.0f, .yUnits = 190.0f}, L"let go");

    windows.Close(WindowKind::Designer);
    windows.Open(WindowKind::Designer);
    Assert::IsTrue(windows.PositionOf(WindowKind::Designer) == Point{.xUnits = 290.0f, .yUnits = 190.0f}, L"where it was left");

    // Settling keeps a moved window where the HUD could lay it out, and leaves an unmoved one to the HUD.
    windows.Settle(WindowKind::Designer, {.xUnits = 280.0f, .yUnits = 0.0f});
    Assert::IsTrue(windows.PositionOf(WindowKind::Designer) == Point{.xUnits = 280.0f, .yUnits = 0.0f});
    windows.Settle(WindowKind::Research, {.xUnits = 5.0f, .yUnits = 5.0f});
    Assert::IsFalse(windows.PositionOf(WindowKind::Research).has_value());
  }

  // Closing the window being dragged lets it go.
  TEST_METHOD(ClosingAWindowLetsItGo)
  {
    Outpost::WindowManager windows;
    windows.Open(WindowKind::Designer);
    windows.Grab(WindowKind::Designer, {}, {});
    windows.Close(WindowKind::Designer);
    Assert::IsFalse(windows.IsDragging());
  }
};
} // namespace GameAppTests
