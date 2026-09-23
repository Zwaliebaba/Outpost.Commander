# ADR-010 — Selection is a tap, or a hold that takes the same design nearby

**Status:** Accepted — **amended 2026-09-21 by
[`ADR-017`](ADR-017-group-selection-is-a-double-tap.md): the verb is a double tap, not a hold.** The
selection *rule* below — same design, screen-space circle, camera as the group-size control — is unchanged
and is what this ADR is still the record of. ADR-017 exists because the latency objection this ADR raised
against hold-and-drag applied equally to the plain hold it adopted.
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

An RTS needs multiple selection and a finger cannot draw a rectangle without that drag meaning something.
`Design/Interface.md` drafted **hold-then-drag** for a band select, so that a plain one-finger drag could
stay as camera panning — the most frequent action, and the one that most deserves the most comfortable
gesture.

That draft was wrong in a way worth recording. Hold-then-drag keeps a band select at the price of **two
meanings on one finger**, about 300 ms of latency before the band appears, and a gesture that is not
self-evident. It also consumed `Holding`, which is one of only three verbs R21 gives.

## Decision

**A tap selects one ship. A second tap on it selects every ship of the same design within a circle centered
on it.** This originally read *"a hold on a ship"*;
[`ADR-017`](ADR-017-group-selection-is-a-double-tap.md) replaced the verb and left everything below it
standing, so the table's reasoning is unchanged and reads the same for a double tap as it did for a hold.

| | |
|---|---|
| **The circle is screen-space**, 192 authored pixels in radius — four times the 48-pixel touch target, about 27% of the frame's width. | The player sees exactly what they will get. |
| **It is drawn while the finger is down** and committed on release. | No invisible rule. |
| **It is centered on the ship, not on the finger.** | The finger is covering the ship. |
| **Own ships only, same design only.** | A hold on the station, which is the only one of its design, selects the station. |
| **The radius is fixed and does not grow with the hold.** | Time as a second axis makes the gesture slow and the result unpredictable. |

**There is no band select.** **One-finger drag is panning, unconditionally, with no second meaning.**

**The camera is the group-size control.** Because the circle is screen-space, zooming in takes a squad and
zooming out takes the fleet — using a gesture the player is already driving constantly, and needing no new
one. This is the property a world-space radius would not have: the drawn circle would shrink on screen as
the camera pulled back and would stop being a reliable indicator of what is about to happen.

**Deselecting is a button on the selection panel**, because the gesture that used to do it — a band
enclosing nothing — no longer exists, and a tap on empty space is a move order.

**`Holding` over empty space is now unassigned**, and is deliberately left so.

## Consequences

**The gesture budget improves.** One finger has exactly two meanings, split by whether it moved: a tap is
the verb, a drag is the camera. That is as unambiguous as a single pointer gets, and `Holding` now does one
thing rather than arming a second mode.

**It fits the component model.** "Same design" is only a coherent idea because
[`ADR-006`](ADR-006-a-ship-is-a-composition.md) made a design a first-class identity. Selection by type
falls out of a decision taken for entirely unrelated reasons, which is usually a sign the model is right.

**What it costs is arbitrary subset selection.** You cannot draw a box around fourteen particular fighters.
You get one ship, or all of a design within a circle — so splitting a fleet in two means two camera
positions and two holds. At fifty ships across three designs that is a small loss; at five hundred it might
not be.

**A dense mixed formation is the bad case.** Miners and fighters sitting together means the circle takes
only one of them, which is usually what you wanted and occasionally is not.

**The circle is round on the screen and a wedge in the world, and this ADR did not notice.**
`Design/Interface.md` §5 couples the camera's pitch to its zoom, so at maximum zoom-in the camera rakes
low across the plane — and a screen-space circle then maps to a **strongly elongated world region**, deep
along the view direction and narrow across it. Two ships equidistant from the held ship in world space are
included or excluded depending on their bearing relative to the camera. The player is shown the circle, so
they are not working blind, but they see a circle and get a wedge. **This is the one place two accepted
decisions interact badly**, it follows directly from a pitch coupling taken for unrelated reasons, and it
is the specific thing M1 must watch for (`Design/OpenQuestions.md`, prediction P3). The mitigation, if it
proves bad, is to define the circle on the **plane** at the radius the screen circle subtends at the held
ship's depth — still one number, still drawn, no longer round on screen.

**What would reopen it:** a fleet cap materially above the design's fifty a player
(`Design/GameDesign.md` §10), or a tactic that genuinely needs an arbitrary subset appearing in play.

## Measurements

None, and none is possible yet — this is an interaction and it is judged by a hand rather than a number.
What is owed at **M1** is that someone uses it on a Surface Pro and says whether 192 pixels is the right
circle. That figure is arithmetic on `Design/Interface.md` §1's touch target, not a measurement: four times
48, which is 36.6 mm in radius at 5.24 authored pixels per millimetre.

**CONFIRMED by the owner on the Surface Pro, 2026-09-23.** 192 pixels is the right circle, and the raking-camera case does not select the wrong
ships badly enough to matter. The mitigation that is one number stays unused.

**This supersedes `Design/Interface.md` §3's hold-then-drag band select**, which was written on the same
day and never built.
