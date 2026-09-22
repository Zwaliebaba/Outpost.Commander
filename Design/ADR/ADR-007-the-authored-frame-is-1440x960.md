# ADR-007 — The authored frame is 1440 × 960

**Status:** Accepted — **amended 2026-09-20 by
[`ADR-011`](ADR-011-the-interface-draws-after-the-scale.md), which binds this decision to the world
only.** The interface no longer draws into the scene target, so the authored frame no longer sets the
interface's resolution. **Further amended 2026-09-21 by
[`ADR-016`](ADR-016-the-world-resolution-is-a-scale.md): the world's resolution is a scale defaulting to
1:1, this decision's crispness argument is corrected, and the interface gets a transform of its own.** **Date:** 2026-09-20 **Owner:** Stefan Zwaal — the target device; the resolution
follows from it

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

**The consequence originally stated here was that the fit is an exact integer multiple, so it is point
sampling, so it is crisp. [`ADR-016`](ADR-016-the-world-resolution-is-a-scale.md) corrects the last
clause:** point sampling at an exact multiple is *sharp*, not crisp — it doubles every pixel, so nothing
finer than two physical pixels can exist on screen. R13's exact-multiple path is still the one a 0.5 scale
takes; it is no longer the only path the target device takes, because the default is now 1:1.

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
development case is deliberately not the one optimized for, and anyone judging glyph quality on a desktop
monitor is judging the wrong picture.

**1440 × 960 is a low authored resolution for a 2026 game**, and the interface will look chunky beside one
authored at 4K. For a touch interface whose smallest target is 9 mm that is correct rather than a
compromise — there is no room for fine detail in a control a fingertip covers — but it does mean **the
interface cannot get denser later without reauthoring every layout number**, because R13 requires them all
to be unconditional.

**Two things this ADR originally got wrong, corrected here rather than quietly.**

**The 2× is not a consequence of the 200% display scale.** `Design/Interface.md` §1 said "this is not a
coincidence: 200% is the scale Microsoft ships on this panel". The swap chain is created at *physical*
pixels, so the DPI scale factor never enters the fit: the 2× is between 1440 × 960, which was freely
chosen, and 2880 × 1920, which is the panel. Dressing a free choice as a natural consequence invites
someone to "fix" the swap chain to device-independent pixels and break it.

**Nothing forced the client fullscreen, so the exact 2× was never guaranteed.** A `CoreWindow`
application can run windowed at an arbitrary size, in which case the fit is an arbitrary bilinear scale
and this decision buys nothing. **The client now enters fullscreen at launch**
(`Design/OpenQuestions.md` Q23); a touch-only game in a resizable window is not a coherent object.

**What would reopen it:** a different target device, or a measured frame time that does not fit at
1440 × 960. The derivation in `Design/Interface.md` §1 is written out longhand so it can be redone against
another panel rather than re-argued. Since ADR-011 it costs the world's fill rate and nothing in the
interface.

## Measurements

### Frame time on the device — 2026-09-22

**1,118 microseconds mean GPU time, 1,089 minimum, over 3,600 frames with none discarded**, on the
**Surface Pro's ARM64** part at the **1:1 world scale** (2,880 × 1,920) and **one sample**, in a
**Debug** build.

**Three separate runs put the mean at 1,119, 1,115 and 1,118** — a spread of four microseconds, which
is what makes this a figure rather than a reading. **The maximum is not stable in the same way**: the
same three runs gave 1,803, 5,001 and 5,056 microseconds. A worst frame that moves by a factor of
three across runs while the mean does not move at all is the compositor rather than the renderer, and
it is the reason the mean is quoted first.

**Against a 16.7 ms budget at 60 Hz the mean is 6.7% of a frame.** **The renderer is nowhere near
being the thing that decides the frame rate**, which is the answer this measurement existed to get.

**IT IS ONE CELL OF THE TABLE BELOW AND NOT THE TABLE.** What is still owed is four samples against
one, and x64 against ARM64, and the 0.5 scale against 1:1 — and until those exist this says nothing
about which scale should ship, which is the decision ADR-016 left to this figure. It is also a Debug
build, which for GPU time matters less than it would for processor time and is still not the number
that ships.

### Frame time with the hulls — 2026-09-22, M1.9

**1,482 microseconds mean GPU time, 1,464 minimum, 1,547 maximum, over 3,600 frames with none
discarded**, on the same **Surface Pro ARM64** part at the same **1:1 scale** and **one sample** — but
a **Release** build this time, and drawing **three CMO meshes as three instanced calls** rather than
M0.21b's generated arrow.

**Two runs before this one gave 1,491 and 1,476**, so the figure is steady in the way the arrow's was.

**The hulls cost about 360 microseconds**, which is arithmetic on the two means rather than a separate
measurement: 1,482 against 1,118. That is 2,674 triangles a frame over three draws, against five
vertices in one.

**Against a 16.7 ms budget at 60 Hz the mean is now 8.9% of a frame**, and against the panel's 120 Hz
it is 17.8% of an 8.3 ms one. **The renderer is still nowhere near being the thing that decides the
frame rate**, which is the claim this ADR's first measurement made and the one M1.9 could have broken.

**THE TWO NUMBERS ARE NOT A CLEAN COMPARISON AND THAT IS STATED RATHER THAN GLOSSED.** The first is a
Debug build and this is Release, so some of the 360 microseconds is the configuration rather than the
geometry. What the comparison supports is the shape of the answer — hulls cost hundreds of
microseconds, not milliseconds — and not the figure to three digits.

### Still owed

Two:

1. **The frame time on an actual Surface Pro**, at one sample and at four, on **both x64 and
   ARM64** — and since [`ADR-016`](ADR-016-the-world-resolution-is-a-scale.md) at **both** 2880 × 1920 and
   1440 × 960, because that measurement is what now settles which one ships. The Surface Pro 11 is a Snapdragon X part, so ARM64 is the target platform — and CI builds
   `Debug|x64` only (`AGENTS.md` §6), so it is also the platform nothing automated ever compiles.
2. **That the present step takes the point-sampled path on the device**, confirmed by looking at it rather
   than by reading the code. R13's whole arrangement is worthless if a conversion error lands the scale at
   1.99.

The figures above are **arithmetic on the device's published specification**, not measurements: 10.82
inches is the width of a 13-inch 3:2 panel, 133 authored pixels per inch is 1,440 ÷ 10.82, 48 pixels is
9 mm at 5.24 pixels per millimetre rounded up, and 1.38 megapixels is 1,440 × 960.
