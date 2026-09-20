#include "pch.h"

#include "PointerMode.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The pointer's two modes (Design/Interface.md §4 and §12 ruling 3; m1-vertical-slice/G1b). Two
// things here are worth a suite and the rest of the mode is a branch in the drawing code: the ORDER
// Escape peels in, and the SIGNS of an aim delta. Both compile either way and neither is noticed
// until somebody plays it.
namespace ClientTests
{

TEST_CLASS(PointerModeTests)
{
public:
  TEST_METHOD(AimIsTheDefaultAndEscapeSwapsTheTwo)
  {
    Assert::IsTrue(Neuron::PointerMode::Point == Neuron::Swapped(Neuron::PointerMode::Aim));
    Assert::IsTrue(Neuron::PointerMode::Aim == Neuron::Swapped(Neuron::PointerMode::Point));
    // Twice is where it started, which is what "swaps" has to mean for a key pressed twice.
    Assert::IsTrue(Neuron::PointerMode::Aim == Neuron::Swapped(Neuron::Swapped(Neuron::PointerMode::Aim)));
  }

  TEST_METHOD(EscapeIsPeeledInnermostFirstAndSwapsThePointerLast)
  {
    // "Escape is peeled, not swallowed" (§7): it cancels the innermost thing open, ONE PRESS AT A
    // TIME. A commander who has armed a Build and opened a panel expects the first press to disarm,
    // not to release his mouse and leave the order armed under a pointer that has stopped aiming.
    Assert::IsTrue(Neuron::EscapePeel::ArmedOrder == Neuron::PeelOf(true, true), L"an armed order comes first");
    Assert::IsTrue(Neuron::EscapePeel::ArmedOrder == Neuron::PeelOf(true, false));
    Assert::IsTrue(Neuron::EscapePeel::ModalPanel == Neuron::PeelOf(false, true), L"then a modal panel");
    Assert::IsTrue(Neuron::EscapePeel::SwapPointer == Neuron::PeelOf(false, false), L"and the mode last of all");
  }

  TEST_METHOD(TheMouseTurnsTheCameraTheWayItIsMoved)
  {
    // THE SIGNS, which is what this case exists for. Right turns right, so yaw GAINS the horizontal
    // count. Forward is a NEGATIVE y count, because the screen's y grows downward, and forward
    // raises the view - so pitch LOSES the vertical count.
    constexpr Neuron::Aim LEVEL{0.0f, 0.0f};
    const Neuron::Aim right = Neuron::AimedBy(LEVEL, 100, 0);
    Assert::IsTrue(right.yawRadians > 0.0f, L"moving right turns right");
    Assert::AreEqual(0.0f, right.pitchRadians, 0.0001f, L"and does not change the pitch");

    const Neuron::Aim up = Neuron::AimedBy(LEVEL, 0, -100);
    Assert::IsTrue(up.pitchRadians > 0.0f, L"moving forward raises the view");
    Assert::AreEqual(0.0f, up.yawRadians, 0.0001f, L"and does not turn it");

    // The rate is the constant and not something near it: a hundred counts is a hundred times it.
    Assert::AreEqual(100.0f * Neuron::AIM_RADIANS_PER_COUNT, right.yawRadians, 0.0001f);
    Assert::AreEqual(0.005f, Neuron::AIM_RADIANS_PER_COUNT, 0.000001f, L"Species's own editor rate (SpeciesLook.md §7)");

    // It accumulates from where the camera already is rather than replacing it, which is what makes
    // it a delta camera at all.
    const Neuron::Aim from{1.0f, 0.25f};
    const Neuron::Aim moved = Neuron::AimedBy(from, 10, 10);
    Assert::AreEqual(1.0f + 10.0f * Neuron::AIM_RADIANS_PER_COUNT, moved.yawRadians, 0.0001f);
    Assert::AreEqual(0.25f - 10.0f * Neuron::AIM_RADIANS_PER_COUNT, moved.pitchRadians, 0.0001f);

    // A frame with no travel changes nothing at all, which is every frame the mouse is still.
    Assert::IsTrue(from == Neuron::AimedBy(from, 0, 0));
  }
};

} // namespace ClientTests
