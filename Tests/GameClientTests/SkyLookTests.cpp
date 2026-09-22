#include "pch.h"

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

[[nodiscard]] float Luminance(float _red, float _green, float _blue) noexcept
{
  return (0.2126f * _red) + (0.7152f * _green) + (0.0722f * _blue);
}
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

  /// **THE BAND IS THE LARGEST LIT AREA IN THE FRAME AND SO THE ONE MOST ABLE TO BREAK THE CEILING.**
  /// Both its ends are checked, because the bulge is the bright one and the rim is the one that covers
  /// the most sky.
  TEST_METHOD(TheGalaxysBothEndsAreUnderTheAreaCeiling)
  {
    const Outpost::GalaxyLook galaxy = Outpost::ShippedGalaxy();

    Assert::IsTrue(galaxy.centreLuminance <= Outpost::SKY_AREA_LUMINANCE_CEILING, L"the galactic centre is brighter than the area ceiling");
    Assert::IsTrue(galaxy.rimLuminance <= Outpost::SKY_AREA_LUMINANCE_CEILING, L"the band's rim is brighter than the area ceiling");

    // The colour is applied on top of the luminance, so a core colour brighter than white would lift
    // the result past a luminance that looked compliant on its own.
    Assert::IsTrue(Luminance(galaxy.coreRed, galaxy.coreGreen, galaxy.coreBlue) <= 1.0f);
    Assert::IsTrue(Luminance(galaxy.rimRed, galaxy.rimGreen, galaxy.rimBlue) <= 1.0f);

    // **THE BULGE IS THE DIFFERENCE BETWEEN THE TWO THICKNESSES**, and a band of uniform thickness is
    // the stain ADR-019 names. Asserted so that "simplifying" the two into one is a failure.
    Assert::IsTrue(galaxy.thicknessAtCentre > galaxy.thicknessAwayFromCentre, L"the band has no bulge and will read as a stain");

    // Dust lanes are not optional, for the same reason.
    Assert::IsTrue(galaxy.dustDepth > 0.0f, L"the band carries no dust lanes");
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

  /// The faint end has to read against the band it may be sitting on -- see the derivation beside the
  /// figure in `SkyLook.cpp`. Over the rim it is better than twice the background; that is the claim.
  TEST_METHOD(TheFaintestStarClearsTheBandsRim)
  {
    const Neuron::StarFieldDescription field = Outpost::ShippedStarField();
    const Outpost::GalaxyLook galaxy = Outpost::ShippedGalaxy();

    Assert::IsTrue(field.faintestValue > (galaxy.rimLuminance * 2.0f), L"the faintest tier will not read against the band's rim");
    Assert::IsTrue(field.faintestValue < field.brightestValue, L"the tiers do not run bright to faint");
  }

  /// **THE CEILING IS ON AREA, SO THIS IS THE NUMBER IT IS ACTUALLY ABOUT.** Three thousand sprites of
  /// one and a half to eight pixels cover a small fraction of one per cent of a 2880 x 1920 frame --
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
  /// **THE FIELD AND THE BAND SHARE A POLE**, which is what ties the two halves into one sky rather
  /// than a star field with a stripe painted over it. Two poles that drifted apart would give a
  /// density ridge that did not line up with the band -- subtly, unnameably wrong.
  TEST_METHOD(TheStarsAndTheBandShareAPole)
  {
    const Neuron::StarFieldDescription field = Outpost::ShippedStarField();
    const Outpost::GalaxyLook galaxy = Outpost::ShippedGalaxy();

    Assert::AreEqual(galaxy.poleX, field.poleX, 0.0001f);
    Assert::AreEqual(galaxy.poleY, field.poleY, 0.0001f);
    Assert::AreEqual(galaxy.poleZ, field.poleZ, 0.0001f);
  }

  /// **TILTED RATHER THAN ALIGNED WITH AN AXIS.** A band lying exactly on the horizon or exactly
  /// overhead reads as a rendering artifact rather than as a galaxy.
  TEST_METHOD(ThePoleIsTiltedRatherThanOnAnAxis)
  {
    const Outpost::GalaxyLook galaxy = Outpost::ShippedGalaxy();
    const float length = std::sqrt((galaxy.poleX * galaxy.poleX) + (galaxy.poleY * galaxy.poleY) + (galaxy.poleZ * galaxy.poleZ));

    Assert::AreEqual(1.0f, length, 0.01f, L"the pole is not a unit vector");

    const float largest = std::max(std::fabs(galaxy.poleX), std::max(std::fabs(galaxy.poleY), std::fabs(galaxy.poleZ)));
    Assert::IsTrue(largest < 0.99f, L"the pole lies on an axis and the band will read as an artifact");
  }

  /// The instances that reach the graphics processor: three thousand of them, on the unit sphere, at
  /// the sizes ADR-019 states.
  TEST_METHOD(TheInstancesAreTheFieldInTheLayoutTheDrawWants)
  {
    const std::vector<Neuron::StarInstance> instances = Outpost::ShippedStarInstances(SEED);
    Assert::AreEqual(static_cast<std::size_t>(3000), instances.size());

    for (const Neuron::StarInstance& instance : instances)
    {
      const float length = std::sqrt((instance.direction[0] * instance.direction[0]) + (instance.direction[1] * instance.direction[1]) +
                                     (instance.direction[2] * instance.direction[2]));
      Assert::AreEqual(1.0f, length, 0.001f, L"an instance left the unit sphere");

      Assert::IsTrue(instance.sizePixels >= 1.5f, L"a sprite is smaller than the faintest tier's size");
      Assert::IsTrue(instance.sizePixels <= 8.0f, L"a sprite is larger than the brightest tier's size");
    }
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

  /// **THE CONSTANT LAYOUT IS THE `cbuffer`'S AND THE TWO MUST NOT DISAGREE.** A field that moved here
  /// without moving in `GalaxyPS.hlsl` would give a band with somebody else's thickness, which reads
  /// as a look nobody chose rather than as a bug.
  TEST_METHOD(TheConstantsAreInTheOrderTheShaderDeclares)
  {
    Outpost::GalaxyLook look;
    look.poleX = 1.0f;
    look.poleY = 2.0f;
    look.poleZ = 3.0f;
    look.centreX = 4.0f;
    look.centreY = 5.0f;
    look.centreZ = 6.0f;
    look.thicknessAwayFromCentre = 7.0f;
    look.thicknessAtCentre = 8.0f;
    look.dustDepth = 9.0f;
    look.dustFrequency = 10.0f;
    look.coreRed = 11.0f;
    look.coreGreen = 12.0f;
    look.coreBlue = 13.0f;
    look.centreLuminance = 14.0f;
    look.rimRed = 15.0f;
    look.rimGreen = 16.0f;
    look.rimBlue = 17.0f;
    look.rimLuminance = 18.0f;

    const Outpost::GalaxyConstants constants = Outpost::ToConstants(look);

    Assert::AreEqual(1.0f, constants.pole[0]);
    Assert::AreEqual(3.0f, constants.pole[2]);
    Assert::AreEqual(0.0f, constants.pole[3], L"the pad must be zero, not left over");
    Assert::AreEqual(4.0f, constants.centre[0]);

    // shape is thickness away, thickness at centre, dust depth, dust frequency -- in that order.
    Assert::AreEqual(7.0f, constants.shape[0]);
    Assert::AreEqual(8.0f, constants.shape[1]);
    Assert::AreEqual(9.0f, constants.shape[2]);
    Assert::AreEqual(10.0f, constants.shape[3]);

    // core and rim are r, g, b, luminance -- the luminance rides in the fourth slot rather than in a
    // seventh `float4`, which is what keeps this to five.
    Assert::AreEqual(11.0f, constants.core[0]);
    Assert::AreEqual(14.0f, constants.core[3]);
    Assert::AreEqual(15.0f, constants.rim[0]);
    Assert::AreEqual(18.0f, constants.rim[3]);
  }

  /// Five `float4` and nothing else, because `App.cpp` copies the struct into the root constants as
  /// bytes and a sixth field would go somewhere the shader is not looking.
  TEST_METHOD(TheConstantsAreExactlyTwentyFloats)
  {
    Assert::AreEqual(static_cast<std::size_t>(20 * sizeof(float)), sizeof(Outpost::GalaxyConstants));
  }
};

} // namespace GameClientTests
