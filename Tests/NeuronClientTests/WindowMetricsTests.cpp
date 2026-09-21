#include "pch.h"

#include <cstdint>
#include <limits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

TEST_CLASS(DipsToPhysical)
{
public:
  TEST_METHOD(AtOneHundredPercentTheyAreTheSame)
  {
    Assert::AreEqual(1920, Neuron::DipsToPhysicalPixels(1920.0f, 1.0f));
    Assert::AreEqual(1080, Neuron::DipsToPhysicalPixels(1080.0f, 1.0f));
  }

  TEST_METHOD(AtTwoHundredPercentTheyDouble)
  {
    // THE CASE THE GAME IS FOR. The Surface Pro's panel is 2880 x 1920 physical at 200%, so the
    // CoreWindow reports 1440 x 960 -- and ADR-007 is explicit that the swap chain is created at
    // the physical figure. A swap chain made at the DIP size here is half the panel's resolution,
    // nothing fails, and the game is simply soft on the one device it is for. That is the defect
    // R18 names this conversion to prevent.
    Assert::AreEqual(2880, Neuron::DipsToPhysicalPixels(1440.0f, 2.0f));
    Assert::AreEqual(1920, Neuron::DipsToPhysicalPixels(960.0f, 2.0f));
  }

  TEST_METHOD(AtOneHundredAndFiftyPercentTheyScaleAndRound)
  {
    Assert::AreEqual(2160, Neuron::DipsToPhysicalPixels(1440.0f, 1.5f));
    Assert::AreEqual(1440, Neuron::DipsToPhysicalPixels(960.0f, 1.5f));

    // An odd DIP size at 150% lands exactly on a half pixel -- 667 x 1.5 is 1000.5 -- so this is
    // the case that pins the rounding rule rather than merely exercising it. Away from zero.
    Assert::AreEqual(1001, Neuron::DipsToPhysicalPixels(667.0f, 1.5f));
    Assert::AreEqual(1004, Neuron::DipsToPhysicalPixels(669.0f, 1.5f), L"669 x 1.5 is 1003.5, and half rounds away from zero");
  }

  TEST_METHOD(AFractionalScaleRoundsToTheNearestPixel)
  {
    // 125% and 175% are both shipped by Windows, and neither divides a typical window evenly.
    Assert::AreEqual(1800, Neuron::DipsToPhysicalPixels(1440.0f, 1.25f));
    Assert::AreEqual(1201, Neuron::DipsToPhysicalPixels(960.0f, 1.2511f));
    Assert::AreEqual(2520, Neuron::DipsToPhysicalPixels(1440.0f, 1.75f));
  }

  TEST_METHOD(APositiveWindowIsNeverZeroPixels)
  {
    // A window dragged between two displays can report a size small enough to round away, and a
    // zero-width swap chain is not a graceful failure.
    Assert::AreEqual(1, Neuron::DipsToPhysicalPixels(0.01f, 1.0f));
    Assert::AreEqual(1, Neuron::DipsToPhysicalPixels(0.4f, 1.0f));
    Assert::AreEqual(1, Neuron::DipsToPhysicalPixels(1.0f, 0.001f));
  }

  TEST_METHOD(NothingIsNothing)
  {
    Assert::AreEqual(0, Neuron::DipsToPhysicalPixels(0.0f, 2.0f));
    Assert::AreEqual(0, Neuron::DipsToPhysicalPixels(-100.0f, 2.0f));
    Assert::AreEqual(0, Neuron::DipsToPhysicalPixels(1440.0f, 0.0f));
    Assert::AreEqual(0, Neuron::DipsToPhysicalPixels(1440.0f, -2.0f));
  }

  TEST_METHOD(ANotANumberIsNothingRatherThanAnExcursion)
  {
    // A window being torn between two displays can briefly report one. The comparison in the
    // implementation is written negated for exactly this: `<= 0` is FALSE for every NaN, so the
    // obvious spelling would have sailed straight through into a cast that is undefined.
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    Assert::AreEqual(0, Neuron::DipsToPhysicalPixels(notANumber, 2.0f));
    Assert::AreEqual(0, Neuron::DipsToPhysicalPixels(1440.0f, notANumber));
  }
};

TEST_CLASS(TheWindowMetrics)
{
public:
  TEST_METHOD(ThePanelComesOutOfTheSurfaceProsWindow)
  {
    const Neuron::WindowMetrics surfacePro{.widthDips = 1440.0f, .heightDips = 960.0f, .rawPixelsPerViewPixel = 2.0f};
    Assert::AreEqual(2880, Neuron::PhysicalWidth(surfacePro));
    Assert::AreEqual(1920, Neuron::PhysicalHeight(surfacePro));
  }

  TEST_METHOD(ADevelopmentWindowAtOneHundredPercentIsItself)
  {
    const Neuron::WindowMetrics desktop{.widthDips = 1920.0f, .heightDips = 1080.0f, .rawPixelsPerViewPixel = 1.0f};
    Assert::AreEqual(1920, Neuron::PhysicalWidth(desktop));
    Assert::AreEqual(1080, Neuron::PhysicalHeight(desktop));
  }

  TEST_METHOD(ADefaultMetricsIsNoWindow)
  {
    Assert::AreEqual(0, Neuron::PhysicalWidth(Neuron::WindowMetrics{}));
    Assert::AreEqual(0, Neuron::PhysicalHeight(Neuron::WindowMetrics{}));
  }
};

} // namespace NeuronClientTests
