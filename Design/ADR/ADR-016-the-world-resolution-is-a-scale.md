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

**This default is a judgment and not a measurement, and it is stated as one.** ADR-007's 1440 × 960 was
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

**What would reopen it:** the M0.16 measurement — **which has since been taken, and did not.** 1:1 at one
sample holds the frame budget on ARM64 on the device with most of it unspent, and the filter is right at
both scales; see Measurements below, where the scale that ships is settled. What is left of this sentence
is the part M0.16 could not reach: **whether it costs enough battery to matter across a match, and
whether a frame with content in it still holds.** If either answers badly the scale goes to 0.5 and **the
constant is the only thing that changes** — which is the property this ADR exists to buy, and which M0.16
exercised end to end rather than assuming.

## Measurements

**M0.16 is closed, and the scale that ships is 1:1.** All three are now answered to the extent this build
can answer them: the filter is confirmed by eye at both scales on the device, the frame times are taken at
one sample of the two they need, and the interface regression is a test. What each still owes is stated
under it.

1. **Frame time at 2880 × 1920 and at 1440 × 960 — TAKEN AT ONE SAMPLE**, on the device, 2026-09-22.
   GPU time from a timestamp pair either side of each frame's command list, meaned over **3,600 presented
   frames** after 120 discarded as warm-up, fullscreen at the panel's 2,880 × 1,920, Release, one sample,
   nothing discarded as an artifact:

   | world scale | scene target | world fit | x64, microseconds | ARM64, microseconds |
   |---|---|---|---|---|
   | **1:1** — the default | 2880 × 1920 | ×1.000, unfiltered | 779 | **777** |
   | **0.5** | 1440 × 960 | ×2.000, point | 525 | **525** |

   The machine is a Surface Pro 11 — Snapdragon X X1P64100, Adreno X1-85, 2,880 × 1,920 — so **ARM64 is
   native and x64 is emulated**, which is exactly what shipping an x64 build to this device would mean.
   That the two agree to within three microseconds is itself the finding: this frame is GPU-bound and the
   CPU's instruction set is not in the answer.

   **READ THESE NARROWLY, because they measured a frame this game did not have.** The scene was a clear,
   six one-pixel rectangle clears and the present blit — nothing else was drawn yet. They are a real
   measurement of the **present path**, which is the cost this ADR predicted above at "about 22 MB read
   and 22 MB written per frame", and they say nothing about the objection that matters more: four times
   the pixel shader invocations over a scene with bloom in it. 777 microseconds is 4.7% of a 16.67 ms
   budget.

   **THERE IS NOW A SCENE, AND IT IS 1,482 MICROSECONDS** — M1.9, on the same device, ARM64 Release,
   1:1, one sample, over 3,600 frames, with three CMO hulls drawn as three instanced calls
   ([`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md) carries the figure). Against the 777 above
   that is **about 700 microseconds for the world**, which is arithmetic on two means rather than a
   separate measurement — and the earlier figure was a Debug build, so it supports the shape of the
   answer and not three digits.

   **8.9% of a 16.67 ms budget with the fleet's geometry in it.** What is still absent is the pixel cost
   this ADR is actually about: there is no sky, no bloom and no interface, and 2,674 triangles is
   geometry rather than shading. **The headroom question stays open**, but it is no longer open over an
   empty frame.

   **WITH THE SKY AND THE INTERFACE, BOTH SCALES, 2026-09-23** — M1.16, the same device, ARM64 Release,
   one sample, over 3,600 frames. The whole frame is split into three spans by the two timestamps
   `TechnicalDesign.md` §9.6 describes. The 0.5 figure is from a build with the denominator set to 2 for
   the run and reverted after:

   | world scale | whole frame | world | present | interface |
   |---|---|---|---|---|
   | **1:1** | **1,539** | 852 | 636 | 50 |
   | **0.5** | **1,096** | 413 | 605 | 77 |

   **1:1 now costs 443 microseconds more than 0.5**, against 252 over the empty frame, and nearly all of
   that is the world pass, which halves with its pixels. **9.2% of a 16.67 ms budget at the scale that
   ships**, so the decision below still holds with the whole of the MVP's content in the frame except
   bloom. The present barely moves between the scales because it writes the full back buffer at both.

   **The four-sample half is not here and is not yet takeable.** A multisampled scene target needs a
   resolve before the present step can sample it, `SceneTarget.cpp` asserts that at compile time, and the
   resolve does not exist. It stays a standing obligation, as `Design/Plan/README.md` measurement 5 says.

2. ~~**That the present step takes the filter the scale calls for**~~ — **CONFIRMED BY EYE at M0.16, on
   the device, at both scales, 2026-09-22.** **The pattern no longer draws**: M1.9 removed the call once
   there were hulls behind it, because a closed gate's instrumentation is a white cross over the world.
   `SceneTarget::RecordCalibrationPattern` remains and is still pinned by its suite, so re-confirming
   this is one line rather than a rewrite. It put a hard one-pixel edge in the scene target — outermost rows and columns plus a centre cross — and it reached
   the glass unfiltered and pixel-exact at 1:1 and cleanly doubled at 0.5, with no soft edge at either.
   **That is what R13's whole arrangement rested on**: a conversion error landing at 1.99 rather than 2,
   or 0.999 rather than 1, shows as grey on a one-pixel edge and shows as almost nothing on anything
   wider. Supporting it but not substituting for it: `CalibrationStrips` is pinned by a suite as exactly
   one pixel, and the probe logs the fit it actually took — `1.000000/none` and `2.000000/point`,
   integers either side.

   **Whether 1:1 at one sample looks better than 0.5 at four is a different question and is still owed.**
   It is not takeable until four samples exist, and it is the one that could still move this decision.

3. ~~**That the interface lands identically at both world resolutions**~~ — **DISCHARGED at M0.15.**
   `TheInterfaceDoesNotMoveWhenTheWorldDoes` in `Tests/NeuronClientTests/SceneTargetTests.cpp` and
   `ItIsExactlyTwoWhicheverTheWorldIsDoing` in `FitTransformTests.cpp` pin the interface fit at exactly
   2.0 at both settled world scales. It was always a test rather than a look, and the regression
   decision 1 exists to prevent now fails a build instead of a reader.

### Which scale ships: 1:1

**Settled at M0.16 on 2026-09-22, and this is the decision that gate existed to take.** Decision 3 above
defaulted to 1:1 on a judgment and said plainly it was not a measurement; it now is one, and the judgment
survived it.

1:1 costs **252 microseconds more per frame than 0.5 on ARM64** and holds the frame budget with most of
it unspent, so the frame-time half of *what would reopen it* gives no reason to move. The filter is right
at both scales, so the choice was a free one rather than a forced one. And the 0.5 path is not dead
code — it stays exercised by M0.12's fit tests at both settled points, which is what keeps it a constant
change rather than a rewrite.

**Two things this does not settle, and neither is a reason to revisit it today.** *Or costs enough
battery to matter across a match* is unmeasured. And every figure above is of a frame with nothing in it,
so the real question — four times the pixel shader invocations over a scene with bloom in it — arrives
with the scene. **What would reopen it** is unchanged and now has a sharper trigger: measurement 2, or
the first content-bearing frame that misses the budget on ARM64.

The figures in the sections above are **arithmetic on the panel's published specification**, not
measurements: 5,529,600 is both 1,440 × 960 × 4 and 2,880 × 1,920 × 1; 22.1 MB is 5,529,600 × 4 bytes;
2.65 GB/s is 44.2 MB at 60 Hz; 9.15 mm is 48 authored pixels at 133 authored pixels per inch.
