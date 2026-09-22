#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
[[nodiscard]] std::uint8_t Code(Neuron::FitFilter _filter) noexcept
{
  return static_cast<std::uint8_t>(_filter);
}

/// The target device's panel, which is what the world's scale is a scale OF (ADR-016).
inline constexpr std::int32_t PANEL_WIDTH = 2880;
inline constexpr std::int32_t PANEL_HEIGHT = 1920;

/// The other settled point, as a rational rather than as 0.5.
inline constexpr std::int32_t HALF_NUMERATOR = 1;
inline constexpr std::int32_t HALF_DENOMINATOR = 2;
} // namespace

TEST_CLASS(TheWorldScale)
{
public:
  TEST_METHOD(ItDefaultsToOneToOne)
  {
    // ADR-016 section 3, and the judgment it states as one: at the single sample the MVP ships,
    // native strictly dominates -- the same content with no upscale and no anti-aliasing either
    // way. M0.16 is the gate that can take this to 0.5, and these two numbers are what it moves.
    Assert::AreEqual(1, Neuron::WORLD_SCALE_NUMERATOR);
    Assert::AreEqual(1, Neuron::WORLD_SCALE_DENOMINATOR);
    Assert::AreEqual(PANEL_WIDTH, Neuron::WorldTargetWidthPixels(PANEL_WIDTH));
    Assert::AreEqual(PANEL_HEIGHT, Neuron::WorldTargetHeightPixels(PANEL_HEIGHT));
  }

  TEST_METHOD(TheMvpShipsOneSample)
  {
    // `TechnicalDesign.md` section 6, and the first number the design expects to move. It is
    // asserted here so that raising it is a decision somebody took rather than a line that drifted:
    // a multisampled scene target cannot be sampled by the present step and needs a resolve, which
    // `SceneTarget.cpp` asserts at compile time in the one place that would break.
    Assert::AreEqual(1u, Neuron::WORLD_SAMPLE_COUNT);
    Assert::AreEqual(Neuron::WORLD_SAMPLE_COUNT, Neuron::SceneTarget::SAMPLE_COUNT);
  }

  TEST_METHOD(TheConstantIsTheOnlyThingThatChanges)
  {
    // THE PROPERTY ADR-016 EXISTS TO BUY, asserted end to end rather than argued: the panel's size
    // goes through the scale to a target size and that target size goes through M0.12's fit, and
    // BOTH settled points come out on a path R13 already calls good. Change the two constants and
    // nothing else has to move -- which is only true if this chain is exact at both ends, and it is
    // exact because every step of it is an integer.
    const std::int32_t defaultWidth = Neuron::ScaledExtentPixels(PANEL_WIDTH, 1, 1);
    const std::int32_t defaultHeight = Neuron::ScaledExtentPixels(PANEL_HEIGHT, 1, 1);
    const Neuron::FitTransform atDefault = Neuron::ComputeFit(defaultWidth, defaultHeight, PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(1.0f, atDefault.scale);
    Assert::AreEqual(Code(Neuron::FitFilter::None), Code(atDefault.filter));

    const std::int32_t halfWidth = Neuron::ScaledExtentPixels(PANEL_WIDTH, HALF_NUMERATOR, HALF_DENOMINATOR);
    const std::int32_t halfHeight = Neuron::ScaledExtentPixels(PANEL_HEIGHT, HALF_NUMERATOR, HALF_DENOMINATOR);
    Assert::AreEqual(1440, halfWidth);
    Assert::AreEqual(960, halfHeight);

    const Neuron::FitTransform atHalf = Neuron::ComputeFit(halfWidth, halfHeight, PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(2.0f, atHalf.scale, L"a 0.5 world scale must fit at exactly two, not at 1.99");
    Assert::AreEqual(Code(Neuron::FitFilter::Point), Code(atHalf.filter));
  }

  TEST_METHOD(TheInterfaceDoesNotMoveWhenTheWorldDoes)
  {
    // ADR-016's third owed measurement, which it says is a test rather than a look. The interface
    // fit takes the authored layout space and never the scene target's size, so it is the same
    // value at both world scales -- and the assertion is worth having here, beside the constants
    // that move, as well as in the fit's own suite.
    const std::int32_t halfWidth = Neuron::ScaledExtentPixels(PANEL_WIDTH, HALF_NUMERATOR, HALF_DENOMINATOR);
    const std::int32_t halfHeight = Neuron::ScaledExtentPixels(PANEL_HEIGHT, HALF_NUMERATOR, HALF_DENOMINATOR);

    // The world is doing two different things in these two lines...
    Assert::AreEqual(1.0f, Neuron::ComputeFit(PANEL_WIDTH, PANEL_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT).scale);
    Assert::AreEqual(2.0f, Neuron::ComputeFit(halfWidth, halfHeight, PANEL_WIDTH, PANEL_HEIGHT).scale);

    // ... and the interface is doing the same thing in both, because it reads neither of them.
    const Neuron::FitTransform interfaceFit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(2.0f, interfaceFit.scale, L"the interface must not borrow the world's scale");
    Assert::AreEqual(PANEL_WIDTH, interfaceFit.width);
    Assert::AreEqual(PANEL_HEIGHT, interfaceFit.height);
  }

  TEST_METHOD(ADevelopmentWindowScalesLikeAnyOther)
  {
    // 1920 x 1080 at the 1:1 default is a target the window has to DOWNSCALE, which ADR-016 points
    // out is supersampling and makes the development case the good one by design rather than by
    // accident. The scale is the panel's, not the window's: the world is authored at whatever the
    // swap chain reports, so this is the same arithmetic on a different panel.
    Assert::AreEqual(1920, Neuron::WorldTargetWidthPixels(1920));
    Assert::AreEqual(1080, Neuron::WorldTargetHeightPixels(1080));
  }

  TEST_METHOD(ADegeneratePanelIsRefusedRatherThanDivided)
  {
    Assert::AreEqual(0, Neuron::ScaledExtentPixels(0, 1, 1));
    Assert::AreEqual(0, Neuron::ScaledExtentPixels(-2880, 1, 1));
    Assert::AreEqual(0, Neuron::ScaledExtentPixels(PANEL_WIDTH, 0, 1));
    Assert::AreEqual(0, Neuron::ScaledExtentPixels(PANEL_WIDTH, 1, 0));
  }

  TEST_METHOD(ATinyPanelStillHasAPixel)
  {
    // NEVER ZERO FOR A POSITIVE PANEL. A window dragged between monitors can report a size that
    // rounds to nothing for one frame, and a render target of zero width does not fail gracefully
    // -- it fails at creation, on a frame nobody was watching.
    Assert::AreEqual(1, Neuron::ScaledExtentPixels(1, HALF_NUMERATOR, HALF_DENOMINATOR));
    Assert::AreEqual(1, Neuron::ScaledExtentPixels(3, 1, 4));
  }
};

} // namespace NeuronClientTests
