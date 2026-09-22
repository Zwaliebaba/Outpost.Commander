#include "pch.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr std::uint64_t SEED = 20260922;

/// ADR-016's 1:1 default, which is the resolution the ceiling below is a fraction of.
constexpr std::uint32_t FRAME_PIXELS = 2880 * 1920;
} // namespace

/// **THE CEILING IS THE POINT OF THIS SUITE.** ADR-019 decision 4 says it out loud -- "a backdrop that
/// nobody pinned gets brighter one commit at a time" -- and `ADR-005`'s argument that a faceted hull
/// reads as deliberate rests on the backdrop staying near-black. These are the assertions that make
/// that a constraint rather than an intention.
TEST_CLASS(TheSkysLuminanceCeiling)
{
public:
  /// The two figures, stated. 12% for anything with area and 45% for a point.
  TEST_METHOD(TheCeilingsAreTheOnesTheDesignStates)
  {
    Assert::AreEqual(0.12f, Outpost::SKY_AREA_LUMINANCE_CEILING, 0.0001f);
    Assert::AreEqual(0.45f, Outpost::SKY_POINT_LUMINANCE_CEILING, 0.0001f);
  }

  /// **ONLY THE BRIGHTEST TIER REACHES THE POINT CEILING, AND IT REACHES IT EXACTLY.**
  TEST_METHOD(TheBrightestStarIsAtThePointCeilingAndNoneIsAboveIt)
  {
    const Neuron::StarFieldDescription field = Outpost::ShippedStarField();
    Assert::AreEqual(Outpost::SKY_POINT_LUMINANCE_CEILING, field.brightestValue, 0.0001f);

    for (const Neuron::Star& star : Neuron::GenerateStarField(SEED, field))
    {
      const float value = std::max(star.red, std::max(star.green, star.blue));
      Assert::IsTrue(value <= (Outpost::SKY_POINT_LUMINANCE_CEILING + 0.0001f), L"a star is brighter than the point ceiling");
    }
  }

  /// **THE FAINTEST STAR HAS TO REACH A PIXEL, NOT MERELY EXIST.** This is the failure the device
  /// showed and no figure caught: a 2.4-pixel sprite at 0.18 under a squared falloff delivered about
  /// 16 of 255 to the nearest pixel centre, so two thirds of the sky was in the frame and not on the
  /// glass. Asserted as what the pixel receives -- the value, the size and `StarPS.hlsl`'s falloff
  /// together -- because each of the three looked fine on its own.
  TEST_METHOD(TheFaintestStarDeliversAVisibleValueToAPixel)
  {
    const Neuron::StarFieldDescription field = Outpost::ShippedStarField();

    // A star centred half a pixel from the nearest pixel centre, which is the typical case rather
    // than the lucky one.
    const float radius = 0.5f / (field.faintestSizePixels * 0.5f);
    const float hermite = radius * radius * (3.0f - (2.0f * radius));
    const float delivered = field.faintestValue * (1.0f - hermite) * 255.0f;

    Assert::IsTrue(delivered >= 40.0f, L"the faintest star delivers too little to its nearest pixel to be seen");
    Assert::IsTrue(field.faintestValue < field.brightestValue, L"the tiers do not run bright to faint");
  }

  /// **THE CEILING IS ON AREA, SO THIS IS THE NUMBER IT IS ACTUALLY ABOUT.** Eight thousand sprites of
  /// three to ten pixels cover a small fraction of one per cent of a 2880 x 1920 frame --
  /// nowhere near 12%, and the assertion is what keeps a later "make the stars a bit bigger" honest.
  TEST_METHOD(TheWholeFieldsLitAreaIsFarUnderTheCeiling)
  {
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, Outpost::ShippedStarField());
    const float fraction = Neuron::LitAreaFraction(stars, FRAME_PIXELS);

    Assert::IsTrue(fraction > 0.0f, L"the field lit nothing at all, which is not a sky");
    Assert::IsTrue(fraction < Outpost::SKY_AREA_LUMINANCE_CEILING, L"the field's lit area broke the ceiling");
    Assert::IsTrue(fraction < 0.01f, L"the field covers more than one per cent of the frame");
  }
};

/// What `GameClient` contributes to the sky: which sky this is (R9).
TEST_CLASS(TheShippedSky)
{
public:
  /// **THE STARS CROWD TOWARD THE SHIPPED PLANE**, and the plane is the only trace of the Milky Way
  /// left now the band is gone -- so a field that took its pole from anywhere else would put the
  /// density ridge somewhere nobody chose.
  TEST_METHOD(TheStarsCrowdTowardTheShippedPlane)
  {
    const Neuron::StarFieldDescription field = Outpost::ShippedStarField();
    const Outpost::GalacticPlane plane = Outpost::ShippedGalacticPlane();

    Assert::AreEqual(plane.poleX, field.poleX, 0.0001f);
    Assert::AreEqual(plane.poleY, field.poleY, 0.0001f);
    Assert::AreEqual(plane.poleZ, field.poleZ, 0.0001f);
    Assert::IsTrue(field.planeConcentration > 1.0f, L"the field does not crowd toward the plane and the Milky Way is gone");
  }

  /// **TILTED RATHER THAN ALIGNED WITH AN AXIS.** A density ridge lying exactly on the horizon or
  /// exactly overhead reads as a rendering artifact rather than as a galaxy.
  TEST_METHOD(ThePoleIsTiltedRatherThanOnAnAxis)
  {
    const Outpost::GalacticPlane plane = Outpost::ShippedGalacticPlane();
    const float length = std::sqrt((plane.poleX * plane.poleX) + (plane.poleY * plane.poleY) + (plane.poleZ * plane.poleZ));

    Assert::AreEqual(1.0f, length, 0.01f, L"the pole is not a unit vector");

    const float largest = std::max(std::fabs(plane.poleX), std::max(std::fabs(plane.poleY), std::fabs(plane.poleZ)));
    Assert::IsTrue(largest < 0.99f, L"the pole lies on an axis and the ridge will read as an artifact");
  }

  /// The instances that reach the graphics processor: eight thousand of them, on the unit sphere, at
  /// the sizes ADR-019 states.
  TEST_METHOD(TheInstancesAreTheFieldInTheLayoutTheDrawWants)
  {
    const std::vector<Neuron::StarInstance> instances = Outpost::ShippedStarInstances(SEED);
    Assert::AreEqual(static_cast<std::size_t>(8000), instances.size());

    const Neuron::StarFieldDescription field = Outpost::ShippedStarField();
    for (const Neuron::StarInstance& instance : instances)
    {
      const float length = std::sqrt((instance.direction[0] * instance.direction[0]) + (instance.direction[1] * instance.direction[1]) +
                                     (instance.direction[2] * instance.direction[2]));
      Assert::AreEqual(1.0f, length, 0.001f, L"an instance left the unit sphere");

      Assert::IsTrue(instance.sizePixels >= (field.faintestSizePixels - 0.001f), L"a sprite is smaller than the faint end");
      Assert::IsTrue(instance.sizePixels <= (field.brightestSizePixels + 0.001f), L"a sprite is larger than the bright end");
    }
  }

  /// **WHAT THE SHIPPED SKY ACTUALLY IS**, written to the log rather than asserted -- the figures a
  /// reader would otherwise have to re-derive, and the ones ADR-019's Measurements section quotes.
  ///
  /// It asserts only the two properties the numbers are for: that the variety is a continuum rather
  /// than six steps, and that the lit area is nowhere near the ceiling.
  TEST_METHOD(TheShippedSkyMeasured)
  {
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, Outpost::ShippedStarField());

    std::vector<float> sizes;
    float smallest = 1000.0f;
    float largest = 0.0f;
    float widestTint = 0.0f;
    for (const Neuron::Star& star : stars)
    {
      sizes.push_back(star.sizePixels);
      smallest = std::min(smallest, star.sizePixels);
      largest = std::max(largest, star.sizePixels);

      const float peak = std::max(star.red, std::max(star.green, star.blue));
      const float floor = std::min(star.red, std::min(star.green, star.blue));
      widestTint = std::max(widestTint, (peak > 0.0f) ? ((peak - floor) / peak) : 0.0f);
    }

    std::sort(sizes.begin(), sizes.end());
    const std::size_t distinctSizes = static_cast<std::size_t>(std::distance(sizes.begin(), std::unique(sizes.begin(), sizes.end())));
    const float fraction = Neuron::LitAreaFraction(stars, FRAME_PIXELS);

    Logger::WriteMessage(("SKY stars=" + std::to_string(stars.size()) + " distinctSizes=" + std::to_string(distinctSizes) +
                          " sizePixels=" + std::to_string(smallest) + ".." + std::to_string(largest) +
                          " widestTint=" + std::to_string(widestTint) + " litArea=" + std::to_string(fraction * 100.0f) + "%\n")
                           .c_str());

    Assert::IsTrue(distinctSizes > 100, L"the sky is drawn at a handful of sizes rather than a continuum");
    Assert::IsTrue(fraction < 0.01f, L"the field covers more than one per cent of the frame");
  }

  /// **THE SAME SEED GIVES THE SAME SKY**, all the way through the conversion rather than only in the
  /// generator -- which is what lets two players share one for nothing (R23).
  TEST_METHOD(TheSameSeedGivesTheSameInstancesTwice)
  {
    const std::vector<Neuron::StarInstance> first = Outpost::ShippedStarInstances(SEED);
    const std::vector<Neuron::StarInstance> second = Outpost::ShippedStarInstances(SEED);

    Assert::AreEqual(first.size(), second.size());
    for (std::size_t index = 0; index < first.size(); ++index)
    {
      Assert::AreEqual(first[index].direction[0], second[index].direction[0], 0.0f);
      Assert::AreEqual(first[index].sizePixels, second[index].sizePixels, 0.0f);
      Assert::AreEqual(first[index].colour[0], second[index].colour[0], 0.0f);
    }
  }
};

} // namespace GameClientTests
