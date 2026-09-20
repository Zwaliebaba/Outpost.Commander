# ADR-010 — Selection is a tap, or a hold that takes the same design nearby

**Status:** Accepted
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

**A tap selects one ship. A hold on a ship selects every ship of the same design within a circle centred
on it.**

| | |
|---|---|
| **The circle is screen-space**, 192 authored pixels in radius — four times the 48-pixel touch target, about 27% of the frame's width. | The player sees exactly what they will get. |
| **It is drawn while the finger is down** and committed on release. | No invisible rule. |
| **It is centred on the ship, not on the finger.** | The finger is covering the ship. |
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

**What would reopen it:** a fleet cap materially above the design's fifty a player
(`Design/GameDesign.md` §10), or a tactic that genuinely needs an arbitrary subset appearing in play.

## Measurements

None, and none is possible yet — this is an interaction and it is judged by a hand rather than a number.
What is owed at **M1** is that someone uses it on a Surface Pro and says whether 192 pixels is the right
circle. That figure is arithmetic on `Design/Interface.md` §1's touch target, not a measurement: four times
48, which is 36.6 mm in radius at 5.24 authored pixels per millimetre.

**This supersedes `Design/Interface.md` §3's hold-then-drag band select**, which was written on the same
day and never built.
