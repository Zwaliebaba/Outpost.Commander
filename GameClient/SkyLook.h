#pragma once

#include "NeuronClient.h"

#include <cstdint>
#include <vector>

namespace Outpost
{

/// **WHAT THIS SKY LOOKS LIKE** (R9). `NeuronClient/StarField.h` generates a point field and
/// `NeuronClient/Blackbody.h` turns a temperature into a colour; neither knows there is a game. This is
/// the band's orientation, the star count, the tier ratios, the palette and the ceiling --
/// [`ADR-019`](../Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md)'s own division of the work.
///
/// **NOTHING GOES IN `GameCore` OR `GameLogic`**: zero wire bytes, zero simulation impact, zero desync
/// risk. The seed is already on the client because R23 needs it for the asteroid generator, and seeding
/// the sky from the same value means both players see the same sky for nothing. **It is floats and noise
/// throughout**, which is correct in a renderer and forbidden in the simulation -- so
/// `Scripts/CheckDeterminism.py` catches the sky drifting into either of those, which is the mistake a
/// later contributor would actually make.

/// **THE CEILING, AND IT IS A CONSTANT RATHER THAN A FEELING** (ADR-019 decision 4). A backdrop that
/// nobody pinned gets brighter one commit at a time.
///
/// Large-area luminance -- the band and the general sky -- never exceeds this fraction of full white.
/// `ADR-005`'s silhouette argument rests on the backdrop being near-black and this is how near.
inline constexpr float SKY_AREA_LUMINANCE_CEILING = 0.12f;

/// **POINT FEATURES MAY REACH THIS, AND ONLY THE BRIGHTEST TIER DOES** -- eight stars covering a few
/// dozen pixels between them.
inline constexpr float SKY_POINT_LUMINANCE_CEILING = 0.45f;

/// What the galaxy band looks like. Everything here is ADR-019's decision 1: **a band brighter and
/// wider toward the galactic centre, carrying dark dust lanes**, because a band without them looks like
/// a stain rather than a galaxy.
///
/// R8: a public aggregate.
struct GalaxyLook
{
  /// The galactic pole in world space, which the band lies perpendicular to. **Tilted rather than
  /// aligned with an axis**: a band lying exactly on the horizon or exactly overhead reads as a
  /// rendering artifact, and the camera's pitch range means a tilt is visible at both ends of the zoom.
  float poleX = 0.0f;
  float poleY = 0.35f;
  float poleZ = 0.94f;

  /// Which way along the band the centre lies, in world space. The bulge sits here.
  float centreX = 1.0f;
  float centreY = 0.0f;
  float centreZ = 0.0f;

  /// How far from the plane the band reaches, away from the centre and at it. **The bulge is the
  /// difference between the two** -- uniform thickness is the stain.
  ///
  /// **THE UNIT IS THE SINE OF THE GALACTIC LATITUDE, NOT AN ANGLE**, because that is what the shader
  /// has in hand: the latitude reaches it as a dot product against the pole and taking an arcsine to
  /// divide by an angle would buy nothing. So 0.055 is one standard deviation at about 3.2 degrees off
  /// the plane, and the band stays visible to two or three times that. Reading these as fractions of a
  /// right angle -- which an earlier draft of this comment said -- overstates the band by half again.
  float thicknessAwayFromCentre = 0.055f;
  float thicknessAtCentre = 0.16f;

  /// Peak luminance of the band at the centre, as a fraction of full white. **Under the area ceiling
  /// above**, which is what the suite asserts rather than trusting.
  ///
  /// **IT IS A LONG WAY UNDER THAT CEILING, AND THE CEILING IS NOT WHAT SETS IT.** The first version
  /// of this sky put the centre at 0.10 -- compliant, and brighter than every star below the top two
  /// tiers. What that draws is a coloured smear with the stars lost inside it, which is the exact
  /// opposite of the effect: **the band is meant to be a texture the stars sit ON, not a light source
  /// that competes with them.** So it sits below the faintest star rather than above it, and the
  /// Milky Way is read mostly from the stars crowding toward the plane.
  float centreLuminance = 0.030f;
  float rimLuminance = 0.010f;

  /// The band's colour, before luminance. Warm at the core and cooler at the rim, which is what a
  /// galaxy actually is -- old red stars in the bulge, young blue ones in the arms.
  ///
  /// **BARELY TINTED, BECAUSE THE REAL ONE IS.** An earlier draft ran from a 0.66 blue at the core to
  /// a 0.66 red at the rim, which is a third of a channel either way and reads as a colour wash rather
  /// than as a galaxy. The naked-eye Milky Way is very nearly grey -- it is too dim to engage colour
  /// vision at all -- so the tint here is a few per cent and its job is to keep the band from looking
  /// like a flat grey stripe, not to be seen as colour.
  float coreRed = 1.0f;
  float coreGreen = 0.97f;
  float coreBlue = 0.92f;
  float rimRed = 0.93f;
  float rimGreen = 0.96f;
  float rimBlue = 1.0f;

  /// **DUST LANES ARE SUBTRACTIVE AND THEY ARE NOT OPTIONAL.** This is how much of the band they may
  /// remove at their darkest, and how many times the noise wraps around the sky.
  float dustDepth = 0.75f;
  float dustFrequency = 2.6f;
};

/// This sky's star field. Every figure is ADR-019's and the derivation for each is beside it there.
[[nodiscard]] Neuron::StarFieldDescription ShippedStarField() noexcept;

/// This sky's stars for a match, ready to upload -- the field generated and then flattened into the
/// layout the sprite draw wants.
///
/// **THE SEED IS THE MATCH'S** (R23). The client already has it for the asteroid generator, so seeding
/// the sky from the same value costs nothing and means both players see the same sky without a byte on
/// the wire.
[[nodiscard]] std::vector<Neuron::StarInstance> ShippedStarInstances(std::uint64_t _seed);

/// This sky's galaxy.
[[nodiscard]] GalaxyLook ShippedGalaxy() noexcept;

/// The galaxy's parameters as the bake shader's constants, in the order its `cbuffer` declares them.
/// **The layout IS the `cbuffer`'s** and the two must not disagree, which is why this is a struct with
/// a fixed size rather than a span somebody fills.
///
/// R8: a public aggregate.
struct GalaxyConstants
{
  float pole[4]{};
  float centre[4]{};

  /// thickness away, thickness at centre, dust depth, dust frequency.
  float shape[4]{};

  /// r, g, b, luminance.
  float core[4]{};
  float rim[4]{};
};

[[nodiscard]] GalaxyConstants ToConstants(const GalaxyLook& _look) noexcept;

} // namespace Outpost
