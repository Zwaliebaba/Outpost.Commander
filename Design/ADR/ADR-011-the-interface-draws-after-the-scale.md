# ADR-011 — The interface draws after the scale, at physical resolution

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** Stefan Zwaal — on a second designer's review
**Amends:** [`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md), which now binds the world only.
Supersedes the pixel-doubling consequence of [`ADR-009`](ADR-009-text-is-directwrite-into-an-atlas.md);
its DirectWrite decision stands unchanged.

## Context

`AGENTS.md` R13 requires **every** pass to draw into a scene target at the authored resolution, which
[`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md) settled at 1440 × 960. On the target device the
present step scales that by exactly 2, so a glyph rasterised at 24 authored pixels reaches the glass as
2 × 2 blocks. `Design/OpenQuestions.md` Q18 accepted that, on the grounds that this interface is large
type with a dozen strings rather than the dense small type R13 warns about.

**That reasoning is correct for the MVP and expires with it.** M4 is a ship designer and a research tree:
tables of component statistics, derived values, dependency chains. That is exactly the dense small type
R13 names, rendered at a 133-PPI effective pixel — and by then every layout number in every panel will
have been authored at 1440 × 960, which R13 requires to be unconditional. **The cost of changing this
rises with every screen, and the trigger Q18 armed fires long after the fix has stopped being cheap.**

## Decision

**The world draws into the 1440 × 960 scene target and is scaled. The interface draws afterwards,
straight into the back buffer, at physical resolution.**

**The interface is still laid out in authored coordinates**, unconditionally, exactly as R13 requires. At
draw time each position is carried through **the same fit transform the present step already computes** —
the one that exists because exactly one place asks the window how big it is. Nothing branches on the
window size. Glyphs are rasterised at the physical size that transform produces, so a 24-authored-pixel
label is rasterised at 48 physical pixels on a Surface Pro rather than doubled from 24.

**What this breaks is R13's letter — "every pass draws into an off-screen colour target" — and nothing
else.** R13's stated purpose is that "every layout, every glyph and every integer position behind it is
unconditional" and that no pass branches on the window size. Both hold. The letter was written to protect
the intent, and here the intent is better served by departing from it.

## Consequences

**Text is no longer resampled.** That is the immediate gain and it is the smaller one.

**The larger gain is that the authored resolution stops binding the interface.** After this,
[`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md) is a decision about the *world* — how much the
renderer has to fill and how much multisampling costs — and changing it no longer means reauthoring every
panel. The compounding cost that made ADR-007 the expensive decision in this corpus is removed at the one
moment it costs eighty lines rather than a rewrite.

**What it costs:**

- A second render target bind and pipeline state per frame, and an ordering constraint: the interface pass
  must run after the present-step blit and before `Present`.
- **Two coordinate spaces exist where there was one**, and a reader has to know which one they are in.
  The mitigation is that only the draw call sees physical pixels; layout, hit testing and every constant
  in `Design/Interface.md` stay authored.
- **The glyph atlas is now sized against the window, not against a constant.** A window resize invalidates
  it and it must be rebuilt, which is a failure path `ADR-009` did not have.
- The interface cannot be multisampled, because a flip-model back buffer cannot be
  (`SampleDesc.Count` must be 1). For geometry that is rectangles and text quads this costs nothing; it
  would matter for a diagonal element, and there are none.

**What would reopen it:** an interface element that must composite with the world under a shared
post-process — a selection glow that bleeds, say — which would have to move back into the scene target and
accept the scale.

## Measurements

None yet. Two are owed at **M1**:

1. **The interface pass's GPU cost** against the world pass at 110 entities, which the review predicts
   will be the larger of the two (`Design/OpenQuestions.md`, prediction P5).
2. **That a window resize rebuilds the atlas without a visible stall**, which is the failure path this
   decision introduces.

The eighty-line estimate is a judgement about a second pass over existing geometry, not a measurement.
