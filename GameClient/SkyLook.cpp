#include "pch.h"

#include "SkyLook.h"

namespace Outpost
{

Neuron::StarFieldDescription ShippedStarField() noexcept
{
  GalaxyLook galaxy = ShippedGalaxy();

  Neuron::StarFieldDescription description;

  // ADR-019: "about 3,000", and the whole naked-eye sky to magnitude six holds roughly 5,000 to 6,000 --
  // so a realistic count and a cheap count are the same count. It divides into the magnitude ratio as
  // 8, 25, 74, 223, 668 and 2,002; the ratio sums to 364 and divides no round number evenly, so those
  // are `TierCounts` rounding rather than a property of 3,000.
  description.starCount = 3000;

  // **SIZE IS DRAWN CONTINUOUSLY BETWEEN THESE, NOT PICKED FROM SIX STEPS.** The first version of this
  // sky took one size per tier, so three thousand stars were drawn at six sizes -- which the eye reads
  // as six kinds of dot rather than as a range. `GenerateStarField` now varies it within the tier.
  //
  // The faint end is 2.4 rather than ADR-019's 1.5 because below about two pixels a sprite with a
  // radial falloff covers no pixel centre at its peak and disappears entirely; see `StarField.h`.
  description.brightestSizePixels = 10.0f;
  description.faintestSizePixels = 2.4f;

  description.brightestValue = SKY_POINT_LUMINANCE_CEILING;

  // **THE FAINT END IS NOT IN THE ADR AND IS DERIVED HERE**, against the thing it has to be seen
  // against: the band. It was 0.08, chosen when the band's centre was 0.10 -- so the faintest two
  // thirds of the sky were dimmer than the backdrop they sat on and simply were not there. The band is
  // now 0.030 at its brightest and this is six times it, which is the ratio that makes a star a star
  // rather than a slightly lighter patch of sky.
  description.faintestValue = 0.18f;

  // Hot stars are luminous, so the bright end skews blue-white and the faint end orange. **The jitter
  // is what stops that being a gradient**: it is wide enough that a bright star can be cool and a faint
  // one hot, which is true of the real sky and is what keeps the correlation from reading as a rule.
  description.brightestKelvin = 15000.0f;
  description.faintestKelvin = 3000.0f;
  description.temperatureJitterKelvin = 1600.0f;

  // ADR-019 said "roughly 20%", which over the old narrow temperature range left every star the same
  // off-white -- the tints were computed and none of them was visible. Oversaturated confetti is still
  // the failure to avoid, and 38% of a blackbody tint is nowhere near it: the reddest star here is
  // still mostly white.
  description.saturation = 0.38f;

  // **THE FIELD AND THE BAND SHARE A POLE**, which is what ties the two halves into one sky rather than
  // a star field with a stripe painted over it. The concentration is up from 1.7 because the band is
  // now read mostly from the stars crowding toward the plane rather than from the baked glow.
  description.planeConcentration = 2.3f;
  description.poleX = galaxy.poleX;
  description.poleY = galaxy.poleY;
  description.poleZ = galaxy.poleZ;

  return description;
}

std::vector<Neuron::StarInstance> ShippedStarInstances(std::uint64_t _seed)
{
  const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(_seed, ShippedStarField());

  std::vector<Neuron::StarInstance> instances;
  instances.reserve(stars.size());

  for (const Neuron::Star& star : stars)
  {
    Neuron::StarInstance instance;
    instance.direction[0] = star.directionX;
    instance.direction[1] = star.directionY;
    instance.direction[2] = star.directionZ;
    instance.sizePixels = star.sizePixels;

    // **THE COLOUR ARRIVES ALREADY LIT.** `GenerateStarField` has multiplied the tier's value through,
    // so the pixel stage multiplies by its falloff and nothing else -- the tier's brightness never has
    // to be found again at draw time, where it would be one more thing to get out of step.
    instance.colour[0] = star.red;
    instance.colour[1] = star.green;
    instance.colour[2] = star.blue;

    instances.push_back(instance);
  }

  return instances;
}

GalaxyLook ShippedGalaxy() noexcept
{
  // The defaults ARE the shipped look; the struct carries them so that every figure is documented where
  // it is declared rather than here. This function exists so there is one name to call and one place to
  // change if the two ever have to differ.
  return GalaxyLook{};
}

GalaxyConstants ToConstants(const GalaxyLook& _look) noexcept
{
  GalaxyConstants constants;

  constants.pole[0] = _look.poleX;
  constants.pole[1] = _look.poleY;
  constants.pole[2] = _look.poleZ;
  constants.pole[3] = 0.0f;

  constants.centre[0] = _look.centreX;
  constants.centre[1] = _look.centreY;
  constants.centre[2] = _look.centreZ;
  constants.centre[3] = 0.0f;

  constants.shape[0] = _look.thicknessAwayFromCentre;
  constants.shape[1] = _look.thicknessAtCentre;
  constants.shape[2] = _look.dustDepth;
  constants.shape[3] = _look.dustFrequency;

  constants.core[0] = _look.coreRed;
  constants.core[1] = _look.coreGreen;
  constants.core[2] = _look.coreBlue;
  constants.core[3] = _look.centreLuminance;

  constants.rim[0] = _look.rimRed;
  constants.rim[1] = _look.rimGreen;
  constants.rim[2] = _look.rimBlue;
  constants.rim[3] = _look.rimLuminance;

  return constants;
}

} // namespace Outpost
