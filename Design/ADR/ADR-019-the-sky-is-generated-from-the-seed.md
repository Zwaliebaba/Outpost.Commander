# ADR-019 — The sky is generated from the seed, and it is dim on purpose

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal — asked for a procedural star field with a galaxy behind it
**Amends:** [`ADR-005`](ADR-005-a-mesh-is-a-cmo-file.md), whose silhouette argument rests on the
backdrop being black. It is now near-black, and this ADR puts a number on how near.

## Context

**Procedural is a choice here and this record has to earn it.** An earlier draft said R14 made it the only
option, because there is no DDS loader and no DirectXTex
([`ADR-005`](ADR-005-a-mesh-is-a-cmo-file.md)) and a painted cubemap therefore could not be loaded
even if one existed. **[`ADR-021`](ADR-021-content-ships-with-the-package.md) settled that content files
ship**, so that argument is gone and the decision stands on the rest: a seeded sky costs no wire bytes,
cannot desync anything, takes no time input so it is generated once and never updated, and is 6.3 MB the
package does not have to carry. A painted one would beat it on art direction alone, which is the trade
whenever somebody wants to make it.

**The naive answer attacks a decision taken three days ago.**
[`ADR-016`](ADR-016-the-world-resolution-is-a-scale.md) defaults the world to 1:1 — 2,880 × 1,920 — and
the argument that made its 4× shading multiplier affordable was that the content is five instanced draws
against a **mostly black background**. A sky evaluated analytically per pixel fills all 5,529,600 of them
with three or four octaves of noise. At an estimated 150–250 ALU per pixel that is plausibly **2–5 ms a
frame** — 12–30% of a 16.7 ms budget, and 24–60% of the 8.3 ms one Q34 is weighing. Those figures are
estimates and are labeled as such; the point stands without them being exact.

**And a cubemap alone cannot hold the stars.** Zoom changes distance rather than field of view, so the
40° vertical FOV is fixed: 1,920 physical pixels over 40° is **0.0208° per pixel**. A cube face spans 90°,
so matching the screen needs about **4,300 texels per face** — 4096², or **402 MB**. At an affordable
512² the cubemap is 8.4× coarser than the screen, which is invisible on a smooth nebula and ruins a point
star.

## Decision

**The sky is two things with two different treatments, because it has two different frequencies.**

### 1. The galaxy bakes once into a small cubemap

Six faces at **512², RGBA8, 6.3 MB**, rendered at match start with the expensive analytic shader — about
1.6 M pixels, once. Per frame it costs one cube fetch.

**What makes it read as a galaxy rather than a smear is the structure, not the noise.** The band is
brighter and wider toward the galactic center, so it has a bulge on one side rather than uniform
thickness; and it carries **dark dust lanes**, which are subtractive. A band without dust lanes looks like
a stain.

### 2. The stars are geometry, and they are modeled on a real sky

**About 3,000 instanced quads** on a unit sphere, positions and properties from the match seed. One draw
call, generatable from `SV_VertexID` with no vertex buffer at all. Three thousand is not an arbitrary
number: the whole naked-eye sky to magnitude 6 holds roughly 5,000–6,000 stars, so **a realistic count and
a cheap count are the same count.**

**Brightness follows the magnitude law, which is what stops it looking like salt and pepper.** Real star
counts multiply by about 2.5 per magnitude step, so six tiers in the ratio **1 : 3 : 9 : 27 : 81 : 243**
— roughly 8, 25, 74, 222, 667 and 2,004 stars. The brightest tier being *eight* is the whole effect: a
field of uniformly bright dots reads as noise, and a field with a handful of standouts reads as a sky.

**Size follows brightness, because apparent size is the point-spread function and not the star.** Every
star is a point source; what differs is how much the eye and the sensor smear it. Sizes run from about
**8 scene-target pixels for the brightest tier down to 1.5 for the faintest**, with a soft radial falloff
baked into the sprite — which also approximates a bloom this design is not going to write.

**Color is blackbody, and it is heavily desaturated.** Stellar color is surface temperature: hot stars
blue-white at 10,000 K and up, the Sun yellow at 5,800 K, cool dwarfs orange-red near 3,000 K. A pinned
table of about eight temperature stops with linear interpolation gives the chromaticity — generated in
code, no data file, and a table a test can pin exactly. **Then desaturate to roughly 20%.** Real stars
read very nearly white and the tint is subtle; oversaturated red and blue confetti is the single most
common way a procedural star field announces itself as fake.

**Temperature correlates with brightness, and that correlation is the detail that sells it.** Hot stars
are luminous, so the bright tiers skew blue-white and the faint ones skew orange. Drawing color
independently of magnitude produces a sky that is subtly, unnameably wrong.

**Star density rises toward the galactic plane.** Biasing the direction sampling toward the band is free
and it is what ties the two halves into one sky rather than a starfield with a stripe painted over it.

### 3. Nothing twinkles, sparkles or moves

**Stars do not twinkle in space.** Scintillation is atmospheric turbulence and there is no atmosphere, so
a twinkle is the opposite of the "natural" this was asked to be. A sparkle was asked for, that reason was
put, and the request was withdrawn on it.

**Two substitutes were considered and are also out.** Diffraction spikes on the brightest stars read as
brilliance, but they are a *telescope* artifact — the player is looking at space, not through an
instrument, so spikes would be the same mistake in a more flattering costume. A slow per-star shimmer
would stop a static field reading as a decal, but nothing here is static: the camera orbits and pitches,
so the sky already moves against the frame.

**So the sky has no time input at all.** It is a pure function of the match seed: generated once, never
updated, zero per-frame CPU. That is simpler than the alternative and it is worth stating, because "add a
little movement" is the kind of thing that arrives later without anyone deciding it.

### 4. It is dim, and the ceiling is on area rather than on peak

`ADR-005` argues that the near-black backdrop "is still what makes a faceted hull read as deliberate",
and `TechnicalDesign.md` §6 wants thin bright silhouettes against black
for legibility. **A bright sky attacks both.** The ceiling is therefore stated in two parts, because what
costs contrast is lit *area*, not peak value:

- **Large-area luminance — the band and the general sky — never exceeds 12% of full white.**
- **Point features may reach 45%, and only the brightest tier does**, which is eight stars covering a few
  dozen pixels between them.

**This is a constant and not a feeling**, because a backdrop that nobody pinned gets brighter one commit
at a time.

### 5. It is client-only, seeded from the match

Nothing goes in `GameCore` or `GameLogic`: **zero wire bytes, zero simulation impact, zero desync risk**,
and the seed is already on the client because R23 needs it for the asteroid generator. Seeding the sky
from the same value means both players see the same sky for nothing.

**It is also floats and noise throughout**, which is correct in a renderer and forbidden in the
simulation — so [`Scripts/CheckDeterminism.py`](../../Scripts/CheckDeterminism.py) catches the sky
drifting into `GameCore` or `GameLogic`, which is the mistake a later contributor would actually make.

## Consequences

**Under a millisecond a frame instead of two to five**, which is the whole point of the split: one cube
fetch over the full screen, plus roughly 3,000 quads at a handful of pixels each — about 27,000 pixels of
fill against 5.53 M, half a percent.

**`ADR-016`'s 1:1 default survives**, which it would not have under a per-pixel sky. This ADR is the
reason that default is still defensible, and M1.16 now measures the frame with the sky present rather than
against a black screen, which was never the real content.

**Draw it last, with depth test on.** The sky does not shade pixels the fleet already covers, and it does
not pay a multisample resolve on a surface with no edges.

**Star size is in scene-target pixels, so the world scale reaches it.** At `ADR-016`'s 0.5 scale a
1.5-pixel faint star is point-doubled to three physical pixels and the faint end coarsens. That is
acceptable and it is one more small argument for the 1:1 default.

**The sky is fixed in world space, which makes it a compass.** It rotates with heading and pitch and does
not translate with pan, so panning across a 16,384-unit map leaves it still. With no minimap and no
compass that is a real navigational aid rather than decoration, and it is the same problem
[`ADR-018`](ADR-018-the-camera-is-anchored-to-the-plane.md) added snap-to-cardinal and a recenter for.

**What is excluded: bloom.** A full-screen blur chain is a much larger change, it multiplies exactly the
cost `ADR-016` is exposed to, and under the luminance ceiling above there would be very little for it to
bloom. The sprite falloff stands in for it.

**What it costs is scope in an MVP that was deliberately cut by a third**, and a backdrop that has to be
kept honest against §4's ceiling for the life of the project.

## Measurements

None yet. Three are owed at **M1.16**:

1. **The frame time with the sky present**, at both world scales, on the device — this is now the
   measurement `ADR-016` actually needs, because a black screen was never the content.
2. **Whether the fleet still reads against it** at the tactical zoom, which is the thing `ADR-005` is
   worried about and is judged by looking rather than by a number.
3. **Whether the sky looks like a sky.** The failure modes are known and specific: uniform brightness
   reading as noise, oversaturated color reading as confetti, and a band without dust lanes reading as a
   stain. All three are looked at, and all three have a named cause if they appear.

The star count, the magnitude ratio and the color temperatures are **arithmetic on the real sky** rather
than measurements of this one: roughly 5,000–6,000 stars to magnitude 6, counts multiplying by about 2.5
per step, and blackbody chromaticity from surface temperature.
