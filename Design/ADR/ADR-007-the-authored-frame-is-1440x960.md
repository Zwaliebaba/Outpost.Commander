# ADR-007 — The authored frame is 1440 × 960

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** Stefan Zwaal — the target device; the resolution follows from it

## Context

`AGENTS.md` R13 requires every pass to draw into a scene target at "the resolution the game is authored
for" and the frame to end by fitting that target into the back buffer — 1:1 unfiltered when it already
matches, point sampling at an exact integer multiple, bilinear otherwise. **R13 deliberately does not say
what that resolution is**, and names the two things that bind whoever settles it: that a flip-model back
buffer cannot be multisampled while a scene target can, and that **a scale is not free and text is what it
costs**, because a glyph baked to an exact pixel height reaches the glass resampled unless the scale is
exactly 1.

That could not be settled until the target device was, which it was on 2026-09-20: **the Surface Pro**.

## Decision

**The game is authored at 1440 × 960**, 3:2.

The Surface Pro's panel is 2880 × 1920 physical pixels at 267 PPI, and Windows ships it at 200% scale, so
the `CoreWindow` reports 1440 × 960 device-independent pixels. **The swap chain is created at physical
pixels — 2880 × 1920 — not at DIPs.** The scene target is the authored 1440 × 960 and the present step
scales it by exactly 2.

**The consequence worth stating in one line: on the target device the fit is an exact integer multiple, so
it is point sampling, so it is crisp.** R13's exact-multiple path is not an optimisation for a lucky
window size here; it is the only path the target device takes.

**The minimum interactive touch target is 48 × 48 authored pixels** (`Design/Interface.md` §1), which is 96
physical pixels and 9.15 mm — the recommended 9 mm rounded up, landing on an even number of physical
pixels at the 2× fit.

## Consequences

**A 1.38-megapixel scene target is small**, and that is the second thing this buys. Four-times
multisampling costs 5.5 megasamples, which is affordable on this hardware, and a fleet of thin bright
silhouettes against black is exactly the content that wants it. The MVP still ships one sample
(`Design/TechnicalDesign.md` §6); this decision is what makes four a cheap change rather than a budget
conversation.

**Everything that is not a current Surface Pro is correct rather than crisp**, which is precisely what R13
buys and is stated rather than regretted. A Surface Pro 7 at 2736 × 1824 fits 1.9× and resamples slightly.
A 1080p monitor — **the display a developer actually works on** — fits 1.125× and pillarboxes. The
development case is deliberately not the one optimised for, and anyone judging glyph quality on a desktop
monitor is judging the wrong picture.

**1440 × 960 is a low authored resolution for a 2026 game**, and the interface will look chunky beside one
authored at 4K. For a touch interface whose smallest target is 9 mm that is correct rather than a
compromise — there is no room for fine detail in a control a fingertip covers — but it does mean **the
interface cannot get denser later without reauthoring every layout number**, because R13 requires them all
to be unconditional.

**What would reopen it:** a different target device. The derivation in `Design/Interface.md` §1 is written
out longhand precisely so it can be redone against another panel rather than re-argued.

## Measurements

None yet. Two are owed:

1. **The frame time at 1440 × 960 on an actual Surface Pro**, at one sample and at four, on **both x64 and
   ARM64**. The Surface Pro 11 is a Snapdragon X part, so ARM64 is the target platform — and CI builds
   `Debug|x64` only (`AGENTS.md` §6), so it is also the platform nothing automated ever compiles.
2. **That the present step takes the point-sampled path on the device**, confirmed by looking at it rather
   than by reading the code. R13's whole arrangement is worthless if a conversion error lands the scale at
   1.99.

The figures above are **arithmetic on the device's published specification**, not measurements: 10.82
inches is the width of a 13-inch 3:2 panel, 133 authored pixels per inch is 1,440 ÷ 10.82, 48 pixels is
9 mm at 5.24 pixels per millimetre rounded up, and 1.38 megapixels is 1,440 × 960.
