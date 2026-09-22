# ADR-019 — The sky is generated from the seed, and it is dim on purpose

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal — asked for a procedural star field with a galaxy behind it
**Amended:** 2026-09-22 — **the galaxy band is withdrawn and the sky is stars and nothing else.** Looked at
on the device, the band read as a painting behind the fleet rather than as sky; see decision 1 and the
second look under Measurements. The star count, the faint end and the sprite falloff moved with it.
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

**The sky was designed as two things with two treatments, because it had two frequencies** — a
low-frequency band baked into a cubemap and high-frequency stars drawn as geometry. The band is withdrawn
(decision 1); the stars are the sky.

### 1. There is no galaxy band — withdrawn 2026-09-22

**The sky is stars and nothing else.** Between them the scene target's clear is black.

This decision first baked the galaxy once into a 512² cubemap, 6.3 MB, with a bulge toward the
galactic centre and subtractive dust lanes, sampled with one fetch per pixel behind the stars. It was
built and looked at twice. The first look found it a coloured smear outshining the stars, and it was dimmed
to 0.030 and nearly greyed. **The second look found that no setting fixes it**: a band this dim is under
the threshold where it reads as light, so what the eye picks up is not a glow but its *structure* — four
octaves of value noise, magnified 8.4× from a 512² face, reading as mottled brushwork behind the fleet. It
is the painted backdrop this ADR was written to avoid, arrived at by another road. Dimmer makes it
invisible, which is the same as removing it at the cost of a full-screen fetch; brighter brings back the
smear the first look found.

**What survives is the galactic plane.** Star density still rises toward it (decision 2), which is how the
naked-eye Milky Way is resolved in the first place: a crowding of points, not a wash. `GameClient` keeps
the plane's pole; the cubemap bake, its two shader pairs and the backdrop draw are deleted.

If a band is ever wanted back, the evidence here says it has to be made of points — a second, denser,
fainter population concentrated on the plane — rather than of a baked texture.

### 2. The stars are geometry, and they are modeled on a real sky

**8,000 instanced quads** on a unit sphere, positions and properties from the match seed. One draw call,
generatable from `SV_VertexID` with no vertex buffer at all. The count is not arbitrary: a dark-site sky to
magnitude 6.5 holds about 9,000 stars, so **a realistic count and a cheap count are the same count.**

**AND THE COUNT IS JUDGED PER FRAME, NOT PER SPHERE.** This first said about 3,000, on the naked-eye sky
to magnitude 6. But the 40° frame at 3:2 subtends 0.66 steradian — a nineteenth of the sphere — so 3,000
put about 160 stars on screen and, on average, under half of one of the brightest tier: the standouts the
magnitude law below exists for were usually not in the frame. At 8,000 there are about 420 in it and one
standout on average, more facing the plane and fewer at its poles. (Arithmetic on the field's own figures,
ignoring the plane concentration.)

**Brightness follows the magnitude law, which is what stops it looking like salt and pepper.** Real star
counts multiply by about 2.5 per magnitude step, so six tiers in the ratio **1 : 3 : 9 : 27 : 81 : 243**
— **22, 66, 198, 593, 1,780 and 5,341 stars**. The ratio sums to 364, which divides no round number of stars
evenly, so those are the rounded shares with the leftover given to the faintest tier rather than an exact
division; `Neuron::TierCounts` is where that rounding happens. The brightest tier being *a handful* —
twenty-two over the whole sphere, one or two in any frame — is the whole effect: a field of uniformly
bright dots reads as noise, and a field with a handful of standouts reads as a sky.

**Size follows brightness, because apparent size is the point-spread function and not the star.** Every
star is a point source; what differs is how much the eye and the sensor smear it. Sizes run from
**10 scene-target pixels at the bright end down to 3.0 at the faint one**, with a soft radial falloff
baked into the sprite — which also approximates a bloom this design is not going to write.

**The falloff is flat-topped: `1 − smoothstep(0, 1, r)`.** It was `(1 − r)²`, which has no edge but no
top either — on a small sprite the nearest pixel centre sits a third of the way out and received a third
of the peak. The Hermite curve holds near full value across the core and still reaches zero, with zero
slope, at the quad's edge.

**AND THE SIZE IS DRAWN CONTINUOUSLY, NOT TAKEN FROM THE TIER.** The tier decides how many stars there
are and nothing else. The first implementation gave every star in a tier the same size and the same
brightness, so three thousand stars were drawn at six sizes — which the eye reads immediately as six
kinds of dot scattered at random, and which no test caught because both ends of the range were right.
A star's size, brightness and temperature are drawn continuously across its tier's interval.

**The faint end is set by what a pixel receives, not by what the sprite is.** This ADR first said 1.5
pixels; a 1.5-pixel quad whose falloff reaches zero at its own edge covers no pixel centre near its peak,
and the faint tiers — two thirds of the sky — did not appear on the device at all. It then said 2.4 at a
value of 0.18, which cleared that floor and was **still not seen**: under the squared falloff a star half a
pixel off a pixel centre delivered about 16 of 255 to it. At 3.0 pixels, a value of 0.24 and the
flat-topped falloff the same star delivers about 45, and `GameClientTests` asserts that figure — the
value, the size and the falloff together — rather than any of the three alone, because each looked
fine on its own.

**Color is blackbody, and it is desaturated.** Stellar color is surface temperature: hot stars
blue-white at 10,000 K and up, the Sun yellow at 5,800 K, cool dwarfs orange-red near 3,000 K. A pinned
table of about eight temperature stops with linear interpolation gives the chromaticity — generated in
code, no data file, and a table a test can pin exactly. **Then desaturate to about 38%.** Real stars
read very nearly white and the tint is subtle; oversaturated red and blue confetti is the single most
common way a procedural star field announces itself as fake. This said 20% and paired it with a narrow
temperature range, which is the other end of the same failure: every star came out the same off-white,
the tints were computed and not one of them was visible. Nearly white is not identically white, and the
difference between those two is the whole texture of a sky.

**Temperature correlates with brightness, and that correlation is the detail that sells it.** Hot stars
are luminous, so the bright tiers skew blue-white and the faint ones skew orange. Drawing color
independently of magnitude produces a sky that is subtly, unnameably wrong.

**Star density rises toward the galactic plane.** Biasing the direction sampling toward the plane is
free, and with the band withdrawn it is the whole of the Milky Way: a crowding of points along a tilted
great circle, which is what the naked eye actually resolves.

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

- **Large-area luminance never exceeds 12% of full white.** With the band withdrawn the only lit area is
  the stars' own, three orders of magnitude under it; the ceiling stays so that anything later added
  behind them has a number to answer to.
- **Point features may reach 45%, and only the brightest tier does**, which is twenty-two stars over the
  whole sphere and one or two in a frame.

**This is a constant and not a feeling**, because a backdrop that nobody pinned gets brighter one commit
at a time.

**A CEILING IS NOT A FLOOR, AND THIS SKY HAS FAILED ON BOTH SIDES OF THAT.** The first version put the
band at a 12%-compliant 0.10 under stars at 0.08 — every figure inside its limit, and the stars lost in a
smear. A ceiling only says how bright a thing may be against *white*; whether a sky reads is decided by
the comparison between its own parts and by what actually reaches a pixel. The band is gone (decision 1),
so the remaining floor is the faint star's delivered value in decision 2, which is asserted.

**The clear colour is part of this.** The scene target cleared to a dark blue, instrumentation from M0
that let a present blit which drew nothing show as black; the band's backdrop drew over it and hid it.
With the band gone the clear *is* the sky between the stars, so it is black.

### 5. It is client-only, seeded from the match

Nothing goes in `GameCore` or `GameLogic`: **zero wire bytes, zero simulation impact, zero desync risk**,
and the seed is already on the client because R23 needs it for the asteroid generator. Seeding the sky
from the same value means both players see the same sky for nothing.

**It is also floats and noise throughout**, which is correct in a renderer and forbidden in the
simulation — so [`Scripts/CheckDeterminism.py`](../../Scripts/CheckDeterminism.py) catches the sky
drifting into `GameCore` or `GameLogic`, which is the mistake a later contributor would actually make.

## Consequences

**Under a millisecond a frame instead of two to five.** With the band withdrawn there is no full-screen
pass at all: the sky is about 420 on-screen quads of a few pixels each out of 8,000 submitted, a few
thousand pixels of fill against 5.53 M, and 250 KiB of instance data. The package saves the 6.3 MB
cubemap it never shipped and the frame no longer pays its fetch.

**`ADR-016`'s 1:1 default survives**, which it would not have under a per-pixel sky. This ADR is the
reason that default is still defensible, and M1.16 now measures the frame with the sky present rather than
against a black screen, which was never the real content.

**Draw it last, with depth test on.** The sky does not shade pixels the fleet already covers, and it does
not pay a multisample resolve on a surface with no edges.

**Star size is in scene-target pixels, so the world scale reaches it.** At `ADR-016`'s 0.5 scale a
3-pixel faint star is point-doubled to six physical pixels and the faint end coarsens. That is
acceptable and it is one more small argument for the 1:1 default.

**The sky is fixed in world space, which makes it a compass.** It rotates with heading and pitch and does
not translate with pan, so panning across a 16,384-unit map leaves it still. With no minimap and no
compass that is a real navigational aid rather than decoration, and it is the same problem
[`ADR-018`](ADR-018-the-camera-is-anchored-to-the-plane.md) added snap-to-cardinal and a recenter for.

**What is excluded: bloom.** A full-screen blur chain is a much larger change, it multiplies exactly the
cost `ADR-016` is exposed to, and with only points in the sky there would be very little for it to bloom. The sprite falloff stands in for it.

**What it costs is scope in an MVP that was deliberately cut by a third**, and a backdrop that has to be
kept honest against §4's ceiling for the life of the project.

## Measurements

**The third one below was taken early, by looking, and it failed.** The sky was built, deployed and
looked at on 2026-09-22, and it did not read as a sky: the stars were not visible and the galaxy was a
coloured smear. Three causes, all now fixed and all recorded in the decisions above —

1. **Six sizes and six brightnesses for three thousand stars.** Size, value and temperature were taken
   once per tier rather than per star. The field now holds **2,999 distinct sizes across 3,000 stars**,
   spanning 2.40 to 9.95 scene-target pixels.
2. **The faint two thirds did not rasterize.** At 1.5 pixels with a falloff reaching zero at the quad's
   edge, a sprite covers no pixel centre near its peak. The faint end was raised to 2.4 — and, after the
   second look below, to 3.0.
3. **The band outshone the stars it was a backdrop for** — 0.10 against 0.08 — and carried a third of a
   channel of colour either way. It is now 0.030, barely tinted, and below the faintest star.

Measured after that fix, from `GameClientTests::TheShippedSkyMeasured`: **2,999 distinct sizes, 2.40–9.95
pixels, a widest per-star tint of 0.60 of the brightest channel, and a total lit area of 0.030% of a
2880 × 1920 frame** — three orders of magnitude under the 12% ceiling, which was never the binding
constraint.

**The second look, 2026-09-22, failed again and differently.** A screenshot from the device, read by
pixel: the band covered the upper half of the frame at 3–8 of 255 as a mottled wash — visible, and
reading as brushwork rather than as light — while only about 130 pixels in the entire sky exceeded 20 of
255. The stars were in the frame and not on the glass. Two causes, both fixed above:

4. **The band's structure, not its brightness.** Decision 1 records why no brightness fixes it; it is
   withdrawn.
5. **The faint end delivered a third of its value.** 2.4 pixels at 0.18 under a squared falloff put
   about 16 of 255 on the nearest pixel. It is now 3.0 pixels at 0.24 under a flat-topped falloff, about
   45 of 255 — *arithmetic*, asserted in `GameClientTests`.

And one found by counting rather than looking: **3,000 stars put under half a standout in the frame**, so
the count is now 8,000 (decision 2).

**THE THIRD LOOK, 2026-09-22: IT READS AS A SKY.** Built on Windows, deployed to the device and
confirmed by eye. That closes the third measurement below ahead of M1.16, and it took three looks —
which is the argument for this ADR's own position that the failure modes are specific and each has a
named cause, because each of the three times it was wrong it was wrong for a reason that had a number
behind it and none of the three was visible in a test until the test was written to look for it.

**`TheShippedSkyMeasured` has now been re-run on the built code**, replacing the Python replica's
estimate: **8,000 stars at 7,992 distinct sizes, 3.00–9.96 scene-target pixels, a widest per-star tint
of 0.60 of the brightest channel, and a total lit area of 0.140% of a 2880 × 1920 frame.** The replica
had put it at "about 0.14%" and was right, which is worth recording only because it means the replica
can be trusted for the next figure it is asked for. Two orders of magnitude under the 12% ceiling.

Two remain owed at **M1.16**:

1. **The frame time with the sky present**, at both world scales, on the device — this is now the
   measurement `ADR-016` actually needs, because a black screen was never the content.
2. **Whether the fleet still reads against it** at the tactical zoom, which is the thing `ADR-005` is
   worried about and is judged by looking rather than by a number.
3. ~~**Whether the sky looks like a sky.**~~ **Answered above on the third look.** The failure modes it
   named were the right ones and two of them actually happened: faint stars in the frame but not on the
   glass, twice, for two different reasons. The third — oversaturated colour reading as confetti — never
   did, and the record should say that the 38% saturation it was worried about turned out to be fine.

The star count, the magnitude ratio and the color temperatures are **arithmetic on the real sky** rather
than measurements of this one: about 9,000 stars to magnitude 6.5, counts multiplying by about 2.5
per step, and blackbody chromaticity from surface temperature.
