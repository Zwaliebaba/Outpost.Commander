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

/// ADR-016 section 3: the world's resolution is a named constant and its default is 1:1, which on
/// the Surface Pro's panel is this. The 0.5 scale is the other settled point.
inline constexpr std::int32_t PANEL_WIDTH = 2880;
inline constexpr std::int32_t PANEL_HEIGHT = 1920;
inline constexpr std::int32_t HALF_SCALE_WIDTH = 1440;
inline constexpr std::int32_t HALF_SCALE_HEIGHT = 960;
} // namespace

TEST_CLASS(TheWorldFit)
{
public:
  TEST_METHOD(AtTheOneToOneDefaultItIsIdentityAndUnfiltered)
  {
    // ADR-016's default: the world target is the panel, so there is no resample at all.
    const Neuron::FitTransform fit = Neuron::ComputeFit(PANEL_WIDTH, PANEL_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(Code(Neuron::FitFilter::None), Code(fit.filter));
    Assert::AreEqual(1.0f, fit.scale);
    Assert::AreEqual(PANEL_WIDTH, fit.width);
    Assert::AreEqual(PANEL_HEIGHT, fit.height);
    Assert::AreEqual(0, fit.offsetX);
    Assert::AreEqual(0, fit.offsetY);
  }

  TEST_METHOD(AtTheHalfScaleItIsExactlyTwoAndPointSampled)
  {
    // The other settled point. EXACTLY two, not 1.99: the scale comes from an integer division
    // that was proved exact, not from a float that happened to land near a whole number.
    const Neuron::FitTransform fit = Neuron::ComputeFit(HALF_SCALE_WIDTH, HALF_SCALE_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(Code(Neuron::FitFilter::Point), Code(fit.filter));
    Assert::AreEqual(2.0f, fit.scale);
    Assert::AreEqual(PANEL_WIDTH, fit.width);
    Assert::AreEqual(PANEL_HEIGHT, fit.height);
    Assert::AreEqual(0, fit.offsetX);
    Assert::AreEqual(0, fit.offsetY);
  }

  TEST_METHOD(TheDevelopmentWindowIsBilinearAndPillarboxed)
  {
    // The development case, and deliberately not the optimized one: a 3:2 world in a 16:9 window
    // leaves bars down the SIDES.
    const Neuron::FitTransform fit = Neuron::ComputeFit(PANEL_WIDTH, PANEL_HEIGHT, 1920, 1080);
    Assert::AreEqual(Code(Neuron::FitFilter::Bilinear), Code(fit.filter));
    Assert::AreEqual(0.5625f, fit.scale);
    Assert::AreEqual(1620, fit.width);
    Assert::AreEqual(1080, fit.height);
    Assert::AreEqual(150, fit.offsetX);
    Assert::AreEqual(0, fit.offsetY);
    Assert::IsTrue(fit.IsPillarboxed());
    Assert::IsFalse(fit.IsLetterboxed());
  }

  TEST_METHOD(AThreeByTwoWindowHasNoBarsAtAll)
  {
    // A 3:2 window against a 3:2 source: aspect already matches, so nothing is boxed whatever the
    // scale turns out to be.
    const Neuron::FitTransform fit = Neuron::ComputeFit(PANEL_WIDTH, PANEL_HEIGHT, 1200, 800);
    Assert::IsFalse(fit.IsPillarboxed());
    Assert::IsFalse(fit.IsLetterboxed());
    Assert::AreEqual(1200, fit.width);
    Assert::AreEqual(800, fit.height);
  }

  TEST_METHOD(ATallWindowLetterboxes)
  {
    // The other direction, so both branches of the limiting-axis decision are covered: a 3:2
    // source in a window taller than 3:2 leaves bars TOP AND BOTTOM.
    const Neuron::FitTransform fit = Neuron::ComputeFit(PANEL_WIDTH, PANEL_HEIGHT, 1440, 1440);
    Assert::IsTrue(fit.IsLetterboxed());
    Assert::IsFalse(fit.IsPillarboxed());
    Assert::AreEqual(1440, fit.width);
    Assert::AreEqual(960, fit.height);
    Assert::AreEqual(240, fit.offsetY);
  }

  TEST_METHOD(AnExactMultipleIsPointSampledEvenWhenTheWindowIsBoxed)
  {
    // The scale is exactly two and the window is taller than the source, so it is point sampled
    // AND letterboxed. Deciding the filter from the fitted scale rather than from whether the
    // window divides evenly is what gets this right.
    const Neuron::FitTransform fit = Neuron::ComputeFit(HALF_SCALE_WIDTH, HALF_SCALE_HEIGHT, PANEL_WIDTH, 2000);
    Assert::AreEqual(Code(Neuron::FitFilter::Point), Code(fit.filter));
    Assert::AreEqual(2.0f, fit.scale);
    Assert::AreEqual(40, fit.offsetY);
  }

  TEST_METHOD(ADegenerateSizeIsRefusedRatherThanDivided)
  {
    for (const Neuron::FitTransform fit : {Neuron::ComputeFit(0, 960, 2880, 1920), Neuron::ComputeFit(1440, 0, 2880, 1920),
                                           Neuron::ComputeFit(1440, 960, 0, 1920), Neuron::ComputeFit(1440, 960, 2880, 0)})
    {
      Assert::AreEqual(0, fit.width);
      Assert::AreEqual(0, fit.height);
    }
  }
};

TEST_CLASS(TheInterfaceFit)
{
public:
  TEST_METHOD(ItIsExactlyTwoWhicheverTheWorldIsDoing)
  {
    // THE MOST VALUABLE TEST IN THIS STEP, and the regression the split exists to prevent.
    // ADR-016 corrects an earlier design that shared one transform between the world and the
    // interface: at the 1:1 world default that shared value is IDENTITY, which renders every
    // panel, every glyph and every touch target at half size in one corner of the screen.
    //
    // The interface fit takes the authored layout space, never the world target, so it is two
    // here and two in the assertion below -- and the world is doing something different in each.
    const Neuron::FitTransform worldAtOneToOne = Neuron::ComputeFit(PANEL_WIDTH, PANEL_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT);
    const Neuron::FitTransform worldAtHalf = Neuron::ComputeFit(HALF_SCALE_WIDTH, HALF_SCALE_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(1.0f, worldAtOneToOne.scale);
    Assert::AreEqual(2.0f, worldAtHalf.scale);

    const Neuron::FitTransform interface = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(2.0f, interface.scale, L"the interface must not borrow the world's scale");
    Assert::AreEqual(Code(Neuron::FitFilter::Point), Code(interface.filter));
    Assert::AreEqual(PANEL_WIDTH, interface.width);
    Assert::AreEqual(PANEL_HEIGHT, interface.height);
  }

  TEST_METHOD(TheAuthoredSpaceIsTheOneInterfaceStates)
  {
    Assert::AreEqual(1440, Neuron::INTERFACE_AUTHORED_WIDTH);
    Assert::AreEqual(960, Neuron::INTERFACE_AUTHORED_HEIGHT);
  }

  TEST_METHOD(AFortyEightPixelTargetIsNinetySixPhysicalOnThePanel)
  {
    // ADR-007: the minimum interactive touch target is 48 authored pixels, which is 96 physical
    // and 9.15 mm. That arithmetic is against the PANEL, and this is the transform that carries
    // it -- which is the whole of ADR-016 section 2 in one assertion.
    const Neuron::FitTransform interface = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(96.0f, 48.0f * interface.scale);
  }

  TEST_METHOD(ItBoxesLikeAnythingElseOnADevelopmentWindow)
  {
    const Neuron::FitTransform interface = Neuron::ComputeInterfaceFit(1920, 1080);
    Assert::AreEqual(Code(Neuron::FitFilter::Bilinear), Code(interface.filter));
    Assert::IsTrue(interface.IsPillarboxed());
    Assert::AreEqual(1080, interface.height);
  }
};

} // namespace NeuronClientTests
