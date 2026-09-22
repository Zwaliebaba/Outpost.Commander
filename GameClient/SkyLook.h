#pragma once

#include "NeuronClient.h"

#include <cstdint>
#include <vector>

namespace Outpost
{

/// **WHAT THIS SKY LOOKS LIKE** (R9). `NeuronClient/StarField.h` generates a point field and
/// `NeuronClient/Blackbody.h` turns a temperature into a colour; neither knows there is a game. This is
/// the galactic plane's orientation, the star count, the tier ratios, the palette and the ceiling --
/// [`ADR-019`](../Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md)'s own division of the work.
///
/// **NOTHING GOES IN `GameCore` OR `GameLogic`**: zero wire bytes, zero simulation impact, zero desync
/// risk. The seed is already on the client because R23 needs it for the asteroid generator, and seeding
/// the sky from the same value means both players see the same sky for nothing. **It is floats
/// throughout**, which is correct in a renderer and forbidden in the simulation -- so
/// `Scripts/CheckDeterminism.py` catches the sky drifting into either of those, which is the mistake a
/// later contributor would actually make.

/// **THE CEILING, AND IT IS A CONSTANT RATHER THAN A FEELING** (ADR-019 decision 4). A backdrop that
/// nobody pinned gets brighter one commit at a time.
///
/// Large-area luminance never exceeds this fraction of full white. `ADR-005`'s silhouette argument
/// rests on the backdrop being near-black and this is how near. **With the band withdrawn, the only lit
/// area left is the stars' own**, which sits three orders of magnitude under this -- the ceiling stays
/// so that anything added behind them later has a number to answer to.
inline constexpr float SKY_AREA_LUMINANCE_CEILING = 0.12f;

/// **POINT FEATURES MAY REACH THIS, AND ONLY THE BRIGHTEST TIER DOES** -- twenty-two stars over the
/// whole sphere, one or two in any frame, covering a few dozen pixels each.
inline constexpr float SKY_POINT_LUMINANCE_CEILING = 0.45f;

/// **WHERE THE MILKY WAY LIES, AND THAT IS ALL THAT IS LEFT OF IT.** ADR-019 first drew the galaxy as
/// a baked band with dust lanes behind the stars; on the device that read as a painted wash, and the
/// band was withdrawn. What survives is the plane itself: star density rises toward it, so the Milky
/// Way is carried by the stars crowding together rather than by any glow between them.
///
/// **TILTED RATHER THAN ALIGNED WITH AN AXIS**: a density ridge lying exactly on the horizon or exactly
/// overhead reads as a rendering artifact, and the camera's pitch range means a tilt is visible at both
/// ends of the zoom.
///
/// R8: a public aggregate.
struct GalacticPlane
{
  /// The galactic pole in world space, which the plane lies perpendicular to.
  float poleX = 0.0f;
  float poleY = 0.35f;
  float poleZ = 0.94f;
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

/// This sky's galactic plane.
[[nodiscard]] GalacticPlane ShippedGalacticPlane() noexcept;

} // namespace Outpost
