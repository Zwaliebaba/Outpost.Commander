#include "pch.h"

#include <algorithm>
#include <cstddef>
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

TEST_CLASS(TheCalibrationPattern)
{
public:
  TEST_METHOD(EveryStripIsExactlyOnePixelThick)
  {
    // THE CLAIM THE WHOLE GATE RESTS ON. M0.16 is a human putting a hard ONE-pixel edge on the
    // glass and deciding by eye whether it came through unfiltered; a strip two pixels thick still
    // looks like a line and settles nothing, because the failure it exists to catch -- a scale that
    // landed at 1.99 rather than 2 -- shows as gray on a one-pixel edge and as almost nothing on a
    // two-pixel one.
    for (const Neuron::CalibrationStrip& strip : Neuron::CalibrationStrips(PANEL_WIDTH, PANEL_HEIGHT))
    {
      const std::int32_t thickness = std::min(strip.right - strip.left, strip.bottom - strip.top);
      Assert::AreEqual(1, thickness);
    }
  }

  TEST_METHOD(TheEdgesAreTheOutermostRowsAndColumns)
  {
    // One pixel in from the edge is a strip that proves nothing: the fitted rectangle's own border
    // is where an offset error has the least to hide behind, so the strips have to BE the border.
    const auto strips = Neuron::CalibrationStrips(PANEL_WIDTH, PANEL_HEIGHT);

    Assert::AreEqual(0, strips[0].top);
    Assert::AreEqual(1, strips[0].bottom);
    Assert::AreEqual(PANEL_HEIGHT - 1, strips[1].top);
    Assert::AreEqual(PANEL_HEIGHT, strips[1].bottom);
    Assert::AreEqual(0, strips[2].left);
    Assert::AreEqual(1, strips[2].right);
    Assert::AreEqual(PANEL_WIDTH - 1, strips[3].left);
    Assert::AreEqual(PANEL_WIDTH, strips[3].right);
  }

  TEST_METHOD(TheCrossIsAtTheCenterAtBothSettledScales)
  {
    // A scale error accumulates toward the middle, which is the half the edges cannot see. At both
    // of ADR-016's settled points the target's extents are even, so the center is exact and the
    // arm's offset is a figure the eye can check against the fitted rectangle's midpoint.
    const auto atDefault = Neuron::CalibrationStrips(PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(PANEL_HEIGHT / 2, atDefault[4].top);
    Assert::AreEqual(PANEL_WIDTH / 2, atDefault[5].left);

    const std::int32_t halfWidth = Neuron::ScaledExtentPixels(PANEL_WIDTH, HALF_NUMERATOR, HALF_DENOMINATOR);
    const std::int32_t halfHeight = Neuron::ScaledExtentPixels(PANEL_HEIGHT, HALF_NUMERATOR, HALF_DENOMINATOR);
    const auto atHalf = Neuron::CalibrationStrips(halfWidth, halfHeight);
    Assert::AreEqual(halfHeight / 2, atHalf[4].top);
    Assert::AreEqual(halfWidth / 2, atHalf[5].left);
  }

  TEST_METHOD(TheStripsSpanTheTargetRatherThanAPartOfIt)
  {
    // An edge that stops short of the corner cannot show a letterbox bar being off by a pixel,
    // which is the other thing a human is looking for at this gate.
    const auto strips = Neuron::CalibrationStrips(PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(PANEL_WIDTH, strips[0].right - strips[0].left);
    Assert::AreEqual(PANEL_HEIGHT, strips[2].bottom - strips[2].top);
  }

  TEST_METHOD(ADegenerateTargetDrawsNothingRatherThanNegativeRectangles)
  {
    // A negative rectangle is a debug-layer error rather than a blank frame, and the size that
    // produces one is the same single frame `ScaledExtentPixels` guards against above. Empty is
    // also what tells SceneTarget to record no clear at all: a clear of zero rectangles clears the
    // ENTIRE view, which would paint the frame white and read as a present step failing wide open.
    for (const Neuron::CalibrationStrip& strip : Neuron::CalibrationStrips(0, PANEL_HEIGHT))
    {
      Assert::AreEqual(0, strip.AreaPixels());
    }
    for (const Neuron::CalibrationStrip& strip : Neuron::CalibrationStrips(PANEL_WIDTH, -1))
    {
      Assert::AreEqual(0, strip.AreaPixels());
    }
  }

  TEST_METHOD(AOnePixelTargetIsNotDegenerate)
  {
    // Degenerate is non-positive and nothing else. A one-pixel target has every strip collapsed
    // onto the same pixel, which is legal, empty of information, and NOT a negative rectangle.
    for (const Neuron::CalibrationStrip& strip : Neuron::CalibrationStrips(1, 1))
    {
      Assert::AreEqual(1, strip.AreaPixels());
    }
  }

  TEST_METHOD(ItIsConstantEvaluated)
  {
    // It is reached once per frame from inside a command list recording, so it costing nothing at
    // run time is worth pinning rather than assuming.
    static constexpr auto strips = Neuron::CalibrationStrips(PANEL_WIDTH, PANEL_HEIGHT);
    static_assert(strips[0].bottom == 1, "The top edge is one pixel thick");
    static_assert(strips.size() == Neuron::CALIBRATION_STRIP_COUNT, "Four edges and a two-armed cross");
    Assert::AreEqual(std::size_t{6}, strips.size());
  }
};

} // namespace NeuronClientTests
