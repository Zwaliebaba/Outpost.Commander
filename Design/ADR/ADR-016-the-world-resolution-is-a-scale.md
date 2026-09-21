# ADR-016 — The world's resolution is a scale, and the interface has its own transform

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal — on a challenge to the authored resolution
**Amends:** [`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md), which pinned the world at 1440 × 960,
and [`ADR-011`](ADR-011-the-interface-draws-after-the-scale.md), whose transform reuse quietly restored
the binding it had just removed.

## Context

**ADR-011's mechanism contradicts its own claim.** Its stated larger gain is that "the authored resolution
stops binding the interface" — after it, ADR-007 "is a decision about the *world* … and changing it no
longer means reauthoring every panel." But the mechanism it chose is that each interface position is
carried through **"the same fit transform the present step already computes"**, and that transform is the
*scene-target → back-buffer* fit. Its value is a function of the world's scene-target size.

Take the world to 2880 × 1920 against a 2880 × 1920 back buffer and that transform becomes **identity**.
The interface is laid out in 1440 × 960, so every panel, every glyph and every touch target renders at half
size in one corner of the screen. The binding was removed in the prose and restored by the implementation,
and the plan encodes it: M0.12 says "two consumers, one computation" in as many words. Nothing would have
caught this until the day someone changed the world's resolution — which is the day ADR-011 exists to make
cheap.

**Separately, ADR-007's justification has been hollowed out by the same ADR.** Its headline argument is
R13's "a scale is not free and **text** is what it costs": a glyph baked to an exact pixel height reaches
the glass resampled unless the scale is exactly 1. That is why 1440 × 960 was chosen — it makes the fit
exactly 2× on this panel. ADR-011 then took text out of the scaled path entirely. **The thing the exact-2×
was protecting left, and the number stayed.**

**What remains bound is the world, and there "exact integer multiple, so point sampling, so crisp" is wrong
as stated.** A point-sampled 2× upscale is *sharp*, not *crisp*: it doubles every pixel, so every edge is a
staircase with two-physical-pixel steps and nothing finer than two physical pixels can exist on screen. For
pixel art that is the entire point. For a perspective camera over thin bright silhouettes it is a cost, and
ADR-007 names that exact content — "a fleet of thin bright silhouettes against black" — as its reason to
want multisampling, then halves the resolution that the same content wants.

**The arithmetic that makes the choice concrete.** 1440 × 960 at four samples is 5,529,600 samples.
2880 × 1920 at one sample is 5,529,600 samples. They are **exactly equal**, and ADR-007 already calls the
first figure affordable and `TechnicalDesign.md` §6 calls it one "this hardware will not notice." Where
native genuinely costs more is pixel shader invocations — 5.53 M against 1.38 M per frame, a true 4×,
because multisampling rasterises coverage per sample but shades once per pixel.

**And the MVP ships one sample** (`TechnicalDesign.md` §6), so today's configuration is the worst corner of
the space available: 1.38 megapixels, no anti-aliasing at all, point-doubled to the panel. The
multisampling that justifies the small target describes a build that does not exist yet.

## Decision

**Three things, and the first is what makes the other two safe.**

**1. Two transforms, not one.** The **world fit** maps the scene target into the back buffer. The
**interface fit** maps authored layout space into the back buffer. They are separate values from the same
computation in the same place — the one place in the client that asks the window how big it is. R13's
intent is untouched: still one place, still nothing branching on the window size, still every layout number
unconditional. What changes is that the interface stops borrowing a number that means something else.

**2. The interface's authored layout space is 1440 × 960 and is not a rendering decision.** It is a
statement about fingertips: 48 authored pixels is 96 physical pixels and 9.15 mm (`Interface.md` §1), and
that arithmetic is against the *panel*, never against the world's target. **No layout number in
`Interface.md` changes** — not for this decision and not for the next one that moves the world.

**3. The world's resolution is a named constant, no longer pinned to half the panel, and its default is
1:1 — 2880 × 1920.** The two settled points are 1:1 and 0.5; nothing between them is expected. The default
is 1:1 because at one sample, which is what the MVP ships, native strictly dominates: the same content with
no upscale and no anti-aliasing either way.

**This default is a judgement and not a measurement, and it is stated as one.** ADR-007's 1440 × 960 was
not measured either — its Measurements section said "None yet" from the day it was written. The difference
is that this one names the gate that settles it: **M0.16**, which already exists in the plan, now measures
both.

## Consequences

**What it costs, and this is the only real objection.** Four times the pixel shader invocations, and four
times the cost of every full-screen pass — and a space game with glowing engines and weapon tracers will
have bloom, which is several. On a fanless tablet that is power and heat over a match. It is a performance
argument, it is unmeasured in both directions, and it is what M0.16 is now for.

**Multisampling stops being free at native.** 2880 × 1920 at four samples is 22.1 M samples and roughly
177 MB of render targets, against 44 MB at the old size. Two things cut against that: at 267 PPI aliasing
is far less visible, so native at one sample may simply beat half-resolution at four; and the Surface
Pro 11 is a Snapdragon X part whose Adreno is tile-based, so multisample resolve happens in tile memory and
never costs the naive bandwidth. **Whether four samples ever ship is now a measurement rather than an
assumption.**

**A present-step blit that buys nothing while one sample ships.** The scene target exists chiefly because a
flip-model back buffer cannot be multisampled. At 1:1 with one sample it degenerates to a pure copy —
about 22 MB read and 22 MB written per frame, 2.65 GB/s at 60 Hz against 1.66 GB/s at the old size. That is
under one percent of an LPDDR5x part's bandwidth, and it vanishes the day multisampling lands, because the
resolve writes into the back buffer. **Keep the indirection**; it is cheap and it is the thing that makes
four samples a constant change.

**What it does not cost, despite ADR-007 listing it as the expensive consequence.** Not one layout number.
Decision 2 buys that outright, and it is why decision 1 comes first.

**R13 is better served, not worse.** 1:1 unfiltered is its best path, not a fallback. The exact-integer
point-sampled path stays in the code and is what a 0.5 scale takes, so M0.12's three cases all remain live
and all remain tested.

**Everything that is not a 2880 × 1920 Surface Pro is still correct rather than crisp**, and one case
improves: a 1080p monitor — the display a developer actually works on — now **downscales** by 0.5625, which
is supersampling, where it previously upscaled 1.125 bilinear. The development case stops being the ugly
one by accident rather than by design.

**The word "crisp" is retired for the upscaled path** wherever this corpus used it. Point sampling at an
exact multiple is sharp and blocky; it was always correct about the filter and wrong about the result.

**What would reopen it:** the M0.16 measurement. If 1:1 at one sample does not hold the frame budget on
ARM64 on the device, or costs enough battery to matter across a match, the scale goes to 0.5 and **the
constant is the only thing that changes** — which is the property this ADR exists to buy.

## Measurements

None yet. Three are owed, and the first two at **M0.16**, which is already a gate:

1. **Frame time at 2880 × 1920 and at 1440 × 960**, at one sample and at four, on an actual Surface Pro,
   on **both x64 and ARM64**. The Surface Pro 11 is a Snapdragon X part, so ARM64 is the target platform
   and `AGENTS.md` §6 says CI compiles none of it.
2. **Whether 1:1 at one sample looks better than 0.5 at four**, judged by eye on the device on the same
   scene — not on a screenshot on a desktop monitor, which is the wrong picture at the wrong scale.
3. **That the interface lands identically at both world resolutions.** This is a test rather than a look,
   it belongs with M0.12's fit tests, and it is the regression decision 1 exists to prevent.

The figures above are **arithmetic on the panel's published specification**, not measurements: 5,529,600 is
both 1,440 × 960 × 4 and 2,880 × 1,920 × 1; 22.1 MB is 5,529,600 × 4 bytes; 2.65 GB/s is 44.2 MB at 60 Hz;
9.15 mm is 48 authored pixels at 133 authored pixels per inch.
