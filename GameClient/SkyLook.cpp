#include "pch.h"

#include "SkyLook.h"

namespace Outpost
{

Neuron::StarFieldDescription ShippedStarField() noexcept
{
  const GalacticPlane plane = ShippedGalacticPlane();

  Neuron::StarFieldDescription description;

  // **THE COUNT IS JUDGED PER FRAME, NOT PER SPHERE.** ADR-019 first shipped 3,000, and the 40-degree
  // frame at 3:2 subtends 0.66 steradian, a nineteenth of the sphere: some 160 stars, most of them
  // faint, and under half of one of the eight standouts on average -- the effect the magnitude law was
  // buying was usually not on screen. 8,000 puts about 420 in the frame and one standout on average
  // (more when the camera faces the plane, fewer at the poles), and it is still a real
  // count: a dark-site sky to magnitude 6.5 holds about 9,000. It divides into the magnitude ratio as
  // 22, 66, 198, 593, 1,780 and 5,341; the ratio sums to 364 and divides no round number evenly, so
  // those are `TierCounts` rounding rather than a property of 8,000.
  description.starCount = 8000;

  // **SIZE IS DRAWN CONTINUOUSLY BETWEEN THESE, NOT PICKED FROM SIX STEPS.** The first version of this
  // sky took one size per tier, so three thousand stars were drawn at six sizes -- which the eye reads
  // as six kinds of dot rather than as a range. `GenerateStarField` now varies it within the tier.
  //
  // The faint end is 3.0 rather than ADR-019's first 1.5 because a small sprite covers few pixel
  // centres and none of them at its peak; see `StarField.h`. **2.4 cleared the rasterization floor and
  // was still not seen on the device**: the nearest pixel centre sat at a third of the peak, which put
  // two thirds of the sky at about 16 of 255 -- present in the frame and invisible on the glass.
  description.brightestSizePixels = 10.0f;
  description.faintestSizePixels = 3.0f;

  description.brightestValue = SKY_POINT_LUMINANCE_CEILING;

  // **THE FAINT END IS SET AGAINST BLACK, AND AGAINST WHAT A PIXEL ACTUALLY RECEIVES.** It was 0.08
  // under a 0.10 band, then 0.18 over a 0.030 one. With the band withdrawn there is nothing behind it,
  // so the figure that matters is the nearest pixel centre: `StarPS.hlsl`'s flat-topped falloff gives it
  // about three quarters of the peak on a 3-pixel sprite, and 0.24 of that is about 46 of 255 -- the
  // dimmest level that reliably reads as a point rather than as noise. It is not raised further
  // because the brightness range is what makes the eight standouts stand out: 0.45 over 0.24 is still
  // nearly two to one in value, and the size range multiplies that by eleven in area.
  description.faintestValue = 0.24f;

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

  // **THE MILKY WAY IS THE STARS AND NOTHING ELSE.** There is no band behind them any more, so the
  // plane is read entirely from the stars crowding toward it -- which is how the naked-eye Milky Way
  // is resolved in the first place, a density rather than a glow.
  description.planeConcentration = 2.3f;
  description.poleX = plane.poleX;
  description.poleY = plane.poleY;
  description.poleZ = plane.poleZ;

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

GalacticPlane ShippedGalacticPlane() noexcept
{
  // The defaults ARE the shipped plane; the struct carries them so that the figure is documented where
  // it is declared rather than here.
  return GalacticPlane{};
}

} // namespace Outpost
