#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
using Neuron::CanvasPoint;

// Three windows of the tests' own.
enum class Pane : std::uint8_t
{
  First,
  Second,
  Third
};

using Stack = Neuron::WindowStack<Pane, 3>;

// A window 600 wide on a 1920 by 1080 screen, kept 120 of its width on it and its 36-tall title bar.
CanvasPoint Kept(CanvasPoint _corner)
{
  return Neuron::KeepOnScreen(_corner, 600.0f, 1920.0f, 1080.0f, 120.0f, 36.0f);
}
} // namespace

TEST_CLASS(WindowStackTests)
{
public:
  // ADR-031 decision 5; ADR-100: a window opens at the front, an open one comes to it, and Esc closes the front one first;
  // closing every window keeps where each was left.
  TEST_METHOD(OpensAtTheFrontAndClosesTheFrontFirst)
  {
    Stack stack;
    stack.Open(Pane::First);
    stack.Open(Pane::Second);
    stack.Open(Pane::Third);
    stack.Open(Pane::First);
    Assert::IsTrue(std::ranges::equal(stack.FrontToBack(), std::array{Pane::First, Pane::Third, Pane::Second}));
    Assert::IsTrue(stack.CloseFront() == Pane::First);
    stack.Close(Pane::Second);
    Assert::IsTrue(std::ranges::equal(stack.FrontToBack(), std::array{Pane::Third}));

    stack.Place(Pane::Second, {.xUnits = 40.0f, .yUnits = 50.0f});
    Assert::IsFalse(stack.IsOpen(Pane::Second), L"placed, not opened");
    stack.CloseAll();
    Assert::IsFalse(stack.CloseFront().has_value());
    Assert::IsTrue(stack.PositionOf(Pane::Second) == CanvasPoint{.xUnits = 40.0f, .yUnits = 50.0f}, L"kept");
    Assert::IsFalse(stack.PositionOf(Pane::Third).has_value(), L"never moved");
  }

  // ADR-031 decision 5; ADR-100: a window taken by its title bar comes to the front and follows the pointer, kept where
  // it was taken hold of, until let go; past each edge it is kept on the screen, and comes back with the pointer.
  TEST_METHOD(DragsAWindowAndKeepsItOnTheScreen)
  {
    Stack stack;
    stack.Open(Pane::First);
    stack.Open(Pane::Second);
    stack.Grab(Pane::First, {.xUnits = 120.0f, .yUnits = 110.0f}, {.xUnits = 100.0f, .yUnits = 100.0f});
    Assert::IsTrue(stack.FrontToBack().front() == Pane::First);
    Assert::IsTrue(stack.Grabbed() == Pane::First);

    stack.Drag({.xUnits = 520.0f, .yUnits = 310.0f});
    Assert::IsTrue(stack.PositionOf(Pane::First) == CanvasPoint{.xUnits = 500.0f, .yUnits = 300.0f}, L"held where taken");

    const std::array<std::pair<CanvasPoint, CanvasPoint>, 4> edges{{
      {{.xUnits = -2000.0f, .yUnits = 310.0f}, {.xUnits = -480.0f, .yUnits = 300.0f}},
      {{.xUnits = 4000.0f, .yUnits = 310.0f}, {.xUnits = 1800.0f, .yUnits = 300.0f}},
      {{.xUnits = 520.0f, .yUnits = -500.0f}, {.xUnits = 500.0f, .yUnits = 0.0f}},
      {{.xUnits = 520.0f, .yUnits = 3000.0f}, {.xUnits = 500.0f, .yUnits = 1044.0f}},
    }};
    for (const auto& [pointer, kept] : edges)
    {
      stack.Drag(pointer);
      const std::optional<CanvasPoint> asked = stack.PositionOf(Pane::First);
      Assert::IsTrue(asked.has_value());
      Assert::IsTrue(Kept(asked.value_or(CanvasPoint{})) == kept, L"kept on the screen at each edge");
      stack.Settle(Pane::First, kept);
    }
    stack.Drag({.xUnits = 520.0f, .yUnits = 310.0f});
    Assert::IsTrue(stack.PositionOf(Pane::First) == CanvasPoint{.xUnits = 500.0f, .yUnits = 300.0f}, L"back with the pointer");
    stack.Release();
    Assert::IsFalse(stack.IsDragging());
    stack.Drag({.xUnits = 0.0f, .yUnits = 0.0f});
    Assert::IsTrue(stack.PositionOf(Pane::First) == CanvasPoint{.xUnits = 500.0f, .yUnits = 300.0f}, L"let go, it stays");

    Assert::IsTrue(Neuron::KeepOnScreen({.xUnits = -500.0f, .yUnits = 0.0f}, 80.0f, 1920.0f, 1080.0f, 120.0f, 36.0f) ==
                     CanvasPoint{.xUnits = 0.0f, .yUnits = 0.0f},
                   L"a window narrower than what is kept stays whole on the screen");
  }
};
} // namespace NeuronClientTests
