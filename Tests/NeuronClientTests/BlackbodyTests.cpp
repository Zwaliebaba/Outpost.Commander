#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
/// How blue a colour is against how red, which is the one number that says what a temperature looks
/// like. Above one is blue-white, below one is orange.
[[nodiscard]] float BlueOverRed(const Neuron::Chromaticity& _color) noexcept
{
  return (_color.red > 0.0001f) ? (_color.blue / _color.red) : 1000.0f;
}

[[nodiscard]] float Luminance(const Neuron::Chromaticity& _color) noexcept
{
  return (0.2126f * _color.red) + (0.7152f * _color.green) + (0.0722f * _color.blue);
}
} // namespace

/// `TechnicalDesign.md` section 8 names this suite's first job: **the blackbody temperature-to-
/// chromaticity table, eight stops, interpolated, pinned exactly** -- because it is the number that
/// decides whether the sky reads as a sky or as confetti (ADR-019).
TEST_CLASS(TheBlackbodyTable)
{
public:
  /// **EIGHT STOPS, AND THESE ARE THEM.** Pinned to a thousandth: the values come out of Planck's law
  /// through an analytic CIE observer, so they are a computation rather than somebody's typing, and
  /// what this catches is that computation changing without anybody meaning it to.
  TEST_METHOD(TheEightStopsArePinned)
  {
    const std::array<Neuron::Chromaticity, Neuron::BLACKBODY_STOP_COUNT>& table = Neuron::BlackbodyTable();

    Assert::AreEqual(static_cast<std::size_t>(8), table.size());
    Assert::AreEqual(2000.0f, Neuron::BLACKBODY_STOPS.front());
    Assert::AreEqual(20000.0f, Neuron::BLACKBODY_STOPS.back());

    // **THE TABLE ITSELF, AND THESE ARE LINEAR sRGB RATHER THAN ENCODED** -- the shader writes into a
    // linear target and applies no gamma, so a published blackbody table's figures do not belong here
    // and will not match. Taken from this chain's own output, which is what makes a change to Planck's
    // constants, the CIE fit, the 5 nm step or the colour matrix arrive as a failure rather than as a
    // sky that looks slightly different to nobody in particular.
    const std::array<Neuron::Chromaticity, Neuron::BLACKBODY_STOP_COUNT> expected{{
      {1.0000f, 0.2663f, 0.0077f}, //  2,000 K
      {1.0000f, 0.4840f, 0.1545f}, //  3,000 K
      {1.0000f, 0.6590f, 0.3788f}, //  4,000 K
      {1.0000f, 0.7954f, 0.6297f}, //  5,000 K
      {1.0000f, 0.9444f, 0.9925f}, //  6,500 K
      {0.7662f, 0.8020f, 1.0000f}, //  8,000 K
      {0.5291f, 0.6347f, 1.0000f}, // 12,000 K
      {0.4075f, 0.5362f, 1.0000f}, // 20,000 K
    }};

    for (std::size_t stop = 0; stop < Neuron::BLACKBODY_STOP_COUNT; ++stop)
    {
      Assert::AreEqual(expected[stop].red, table[stop].red, 0.001f, L"a stop's red moved");
      Assert::AreEqual(expected[stop].green, table[stop].green, 0.001f, L"a stop's green moved");
      Assert::AreEqual(expected[stop].blue, table[stop].blue, 0.001f, L"a stop's blue moved");
    }

    // Every stop normalizes its brightest channel to one, which is what makes these a chromaticity
    // rather than a radiance -- a blackbody's total output goes as the fourth power of temperature and
    // nothing here wants a star ten thousand times brighter than another.
    for (const Neuron::Chromaticity& stop : table)
    {
      const float brightest = std::max(stop.red, std::max(stop.green, stop.blue));
      Assert::AreEqual(1.0f, brightest, 0.0001f, L"a stop is not normalized");
    }
  }

  /// **COOL IS ORANGE AND HOT IS BLUE-WHITE, MONOTONICALLY.** This is the property the sky rests on and
  /// the one a wrong sign in the colour matrix would invert -- which would read as an odd palette
  /// rather than as a bug.
  TEST_METHOD(BlueOverRedRisesMonotonicallyWithTemperature)
  {
    float previous = -1.0f;
    for (const float kelvin : Neuron::BLACKBODY_STOPS)
    {
      const float ratio = BlueOverRed(Neuron::BlackbodyColor(kelvin));
      Assert::IsTrue(ratio > previous, L"a hotter stop was not bluer than the one below it");
      previous = ratio;
    }
  }

  /// A cool dwarf near 3,000 K is orange-red: red is the brightest channel and blue is well below it.
  TEST_METHOD(AThreeThousandKelvinStarIsOrange)
  {
    const Neuron::Chromaticity cool = Neuron::BlackbodyColor(3000.0f);
    Assert::AreEqual(1.0f, cool.red, 0.0001f, L"red should be the brightest channel");
    Assert::IsTrue(cool.blue < 0.7f, L"3,000 K is not orange enough");
    Assert::IsTrue(cool.green < cool.red);
    Assert::IsTrue(cool.blue < cool.green);
  }

  /// And a hot one at 12,000 K is blue-white: blue is brightest and red is below it, but **not far
  /// below** -- a star that came out saturated blue would be the confetti failure ADR-019 names.
  ///
  /// **THE FLOOR IS HALF, AND IT IS A BOUND RATHER THAN THE VALUE.** The chain gives 0.529 and the stop
  /// above pins that; what this asserts is the property that matters if the chain is ever replaced --
  /// red keeps at least half of blue, so the star still reads as white with a cast. A first draft of
  /// this test asserted 0.55 and failed against a correct implementation, which is what a threshold
  /// picked to sit just under an imagined value does.
  TEST_METHOD(ATwelveThousandKelvinStarIsBlueWhiteRatherThanBlue)
  {
    const Neuron::Chromaticity hot = Neuron::BlackbodyColor(12000.0f);
    Assert::AreEqual(1.0f, hot.blue, 0.0001f, L"blue should be the brightest channel");
    Assert::IsTrue(hot.red > 0.5f, L"12,000 K came out saturated blue rather than blue-white");
    Assert::IsTrue(hot.red < hot.blue);
  }

  /// **THE SUN IS VERY NEARLY WHITE**, which is the sanity check on the whole chain: around 5,800 K the
  /// three channels sit close together, and a chain with a broken matrix or a wrong radiation constant
  /// does not manage that by accident.
  TEST_METHOD(TheSunIsNearlyWhite)
  {
    const Neuron::Chromaticity sun = Neuron::BlackbodyColor(5800.0f);
    const float spread = std::max(sun.red, std::max(sun.green, sun.blue)) - std::min(sun.red, std::min(sun.green, sun.blue));
    Assert::IsTrue(spread < 0.22f, L"5,800 K is not close to white");
  }

  /// **BETWEEN THE STOPS AS WELL AS AT THEM** -- section 8 asks for interpolated values pinned, and the
  /// implementation computes rather than interpolating, so what is asserted is that a value between two
  /// stops lies between them.
  TEST_METHOD(EveryTemperatureBetweenTwoStopsLiesBetweenThem)
  {
    for (std::size_t stop = 0; (stop + 1) < Neuron::BLACKBODY_STOP_COUNT; ++stop)
    {
      const float low = Neuron::BLACKBODY_STOPS[stop];
      const float high = Neuron::BLACKBODY_STOPS[stop + 1];

      const float lowRatio = BlueOverRed(Neuron::BlackbodyColor(low));
      const float highRatio = BlueOverRed(Neuron::BlackbodyColor(high));

      for (float t = 0.1f; t < 1.0f; t += 0.1f)
      {
        const float between = BlueOverRed(Neuron::BlackbodyColor(low + ((high - low) * t)));
        Assert::IsTrue(between > lowRatio, L"a value between two stops fell below the lower one");
        Assert::IsTrue(between < highRatio, L"a value between two stops rose above the upper one");
      }
    }
  }

  /// **CLAMPED RATHER THAN EXTRAPOLATED.** A negative temperature has no colour and a million kelvin is
  /// not in this sky; both give the nearest end rather than something the display cannot say.
  TEST_METHOD(OutsideTheRangeClampsToItsEnds)
  {
    Assert::IsTrue(Neuron::BlackbodyColor(-5000.0f) == Neuron::BlackbodyColor(2000.0f));
    Assert::IsTrue(Neuron::BlackbodyColor(0.0f) == Neuron::BlackbodyColor(2000.0f));
    Assert::IsTrue(Neuron::BlackbodyColor(1000000.0f) == Neuron::BlackbodyColor(20000.0f));
  }

  /// The same temperature twice is the same colour. It is a pure function and the sky depends on it
  /// being one.
  TEST_METHOD(ItIsAPureFunction)
  {
    Assert::IsTrue(Neuron::BlackbodyColor(7345.0f) == Neuron::BlackbodyColor(7345.0f));
  }
};

/// ADR-019: **desaturate to roughly 20%**, because real stars read very nearly white.
TEST_CLASS(TheDesaturation)
{
public:
  /// **IT KEEPS THE BRIGHTNESS.** The grey it pulls toward is the colour's own luminance, so a
  /// desaturated star stays as bright as it was instead of dimming as it whitens -- which would make the
  /// hot and cool tiers differ in brightness for no reason anybody chose.
  TEST_METHOD(ItPreservesLuminance)
  {
    for (const float kelvin : Neuron::BLACKBODY_STOPS)
    {
      const Neuron::Chromaticity full = Neuron::BlackbodyColor(kelvin);
      const Neuron::Chromaticity muted = Neuron::Desaturate(full, 0.2f);
      Assert::AreEqual(Luminance(full), Luminance(muted), 0.0005f, L"desaturating changed the brightness");
    }
  }

  /// At a fifth, the tint survives as a tint: the channels move most of the way to grey but not all.
  TEST_METHOD(AFifthLeavesATintRatherThanAColour)
  {
    const Neuron::Chromaticity hot = Neuron::BlackbodyColor(12000.0f);
    const Neuron::Chromaticity muted = Neuron::Desaturate(hot, 0.2f);

    const float fullSpread = hot.blue - hot.red;
    const float mutedSpread = muted.blue - muted.red;

    Assert::AreEqual(fullSpread * 0.2f, mutedSpread, 0.0005f);
    Assert::IsTrue(mutedSpread > 0.0f, L"the tint vanished entirely");
    Assert::IsTrue(mutedSpread < 0.12f, L"a fifth left it too saturated to read as a star");
  }

  /// The two ends, stated: one is untouched and zero is grey.
  TEST_METHOD(OneIsUntouchedAndZeroIsGrey)
  {
    const Neuron::Chromaticity cool = Neuron::BlackbodyColor(3000.0f);

    Assert::IsTrue(Neuron::Desaturate(cool, 1.0f) == cool);

    const Neuron::Chromaticity grey = Neuron::Desaturate(cool, 0.0f);
    Assert::AreEqual(grey.red, grey.green, 0.0001f);
    Assert::AreEqual(grey.green, grey.blue, 0.0001f);
  }

  /// Out of range clamps rather than overshooting into a colour more saturated than the original.
  TEST_METHOD(OutsideTheRangeClamps)
  {
    const Neuron::Chromaticity cool = Neuron::BlackbodyColor(3000.0f);
    Assert::IsTrue(Neuron::Desaturate(cool, 4.0f) == Neuron::Desaturate(cool, 1.0f));
    Assert::IsTrue(Neuron::Desaturate(cool, -1.0f) == Neuron::Desaturate(cool, 0.0f));
  }
};

} // namespace NeuronClientTests
