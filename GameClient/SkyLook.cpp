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

  description.brightestSizePixels = 8.0f;
  description.faintestSizePixels = 1.5f;

  description.brightestValue = SKY_POINT_LUMINANCE_CEILING;

  // **THE FAINT END IS NOT IN THE ADR AND IS DERIVED HERE**, against the two things it has to survive.
  //
  // Away from the band the baked cubemap is essentially zero, so a faint star there is the only light
  // in its neighbourhood and almost any value would read. The binding case is a faint star ON the
  // band, where it has to clear the rim's 0.035 and ideally the centre's 0.10. Eight per cent is a bit
  // over twice the rim, which reads plainly, and a little under the centre, where the brightest tiers
  // are what carry the sky anyway. It also has to leave the field's total lit area nowhere near the
  // 12% ceiling, and the whole field comes to a small fraction of one per cent of the frame --
  // `SkyLookTests` asserts both rather than assuming them.
  description.faintestValue = 0.08f;

  // Hot stars are luminous, so the bright tiers skew blue-white and the faint ones orange. 12,000 K is
  // solidly blue-white and 3,400 K is an orange dwarf; the Sun's 5,800 falls in the middle tiers, which
  // is where most of the stars are.
  description.brightestKelvin = 12000.0f;
  description.faintestKelvin = 3400.0f;
  description.temperatureJitterKelvin = 900.0f;

  // ADR-019: "desaturate to roughly 20%". Oversaturated red and blue confetti is the single most common
  // way a procedural star field announces itself as fake.
  description.saturation = 0.2f;

  // **THE FIELD AND THE BAND SHARE A POLE**, which is what ties the two halves into one sky rather than
  // a star field with a stripe painted over it.
  description.planeConcentration = 1.7f;
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
