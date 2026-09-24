# ADR-018 — The camera is anchored to the plane, and one solve drives it

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal — on a camera review against the target device
**Relates to:** [`ADR-001`](ADR-001-the-playfield-is-a-plane.md), which settles the camera's four degrees
of freedom and is what this implements. **Spends one of the two `Holding` gestures
[`ADR-017`](ADR-017-group-selection-is-a-double-tap.md) freed**, which that ADR deliberately left open.

## Context

**Nothing in this design says how a finger's motion becomes camera motion.** ADR-001 settles the degrees
of freedom — focus on the plane, heading, distance, pitch derived — and `Design/Interface.md` §5 settles
which gesture drives which. The mapping between them is absent, and it is the thing that decides whether a
3D touch camera feels like direct manipulation or like a joystick.

**There are two models and they are not close.**

**Rate-based**: the focus point moves by the finger's delta times a constant. It is trivial and it feels
wrong the moment the camera is pitched, because the ground then slides at a different rate from the
finger — and at a *different* different rate at the top of the screen than at the bottom, since that is
what perspective does. Every person who has used a map application notices within seconds.

**Ray-anchored**: at `ManipulationStarted` a ray through the contact centroid meets the plane, and that
world point is kept. Each update solves for the focus point that puts the anchor back under the current
centroid. **The ground sticks to the finger.** It is what every map and every competent mobile strategy
game does, and it is what a player's hands already expect.

The same question exists for zoom and for orbit, with the same answer and one complication: **pitch is
coupled to zoom**, so changing the distance changes the pitch, which changes the ray geometry, which moves
where the anchor projects. Solved naively that is a feedback loop.

## Decision

**Seven things, and the first three are one mechanism stated three ways.**

**1. The camera is ray-anchored.** At `ManipulationStarted`, cast through the contact centroid to the
plane and keep that world point for the life of the manipulation. It is never recomputed mid-gesture —
recomputing is what produces drift.

**2. One solve drives everything, and the recognizer's translation is not applied separately.** The order
is: scale → new distance, and therefore new pitch; rotation → new heading; **then one solve** that places
the focus so the anchor lands under the current centroid. Applying the translation delta *as well* as the
solve double-counts it and produces a camera that runs away. **This is the defect to expect if the camera
feels like it accelerates.**

**3. Zoom is solved at the new pose, which removes the feedback loop.** Take the anchor from the
*starting* pose; compute the new distance and hence the new pitch from the scale; then solve for the focus
that puts the anchor under the current centroid **at the new pose**. One solve, closed form, no iteration.

**4. Orbit is about the anchor**, which falls out of the above for nothing. Rotating about the focus point
instead would swing whatever is under the fingers away from them, which is the wrong answer to a gesture
whose whole premise is that the fingers are on something.

**5. On release, heading snaps to the nearest cardinal** when it is within a stated threshold of one.
There is no minimap and no compass, so a map that is reliably north-up when the player is not deliberately
turning it is worth one constant — it is what spatial memory is built on.

**6. The clamp applies after the solve, and the ground stops.** The focus is clamped to the play area plus
its margin, so at the edge the anchor slips and the finger slides over stationary ground. That is the
right behavior for a strategy game and it is what clamping the solve's output already gives — it is
stated because it reads as a defect the first time it is seen.

**7. There is no inertia, and `Holding` over empty space recenters.** The inertia gesture settings stay
off: the action immediately after positioning this camera is a precise tap, and momentum fights that.
Instead, a hold on empty space moves the focus to the selection, or to your station when nothing is
selected. `Design/GameDesign.md` §7 already names the problem — *"a defender must be watching the right
part of a 16,384-unit map at the right moment to have any counterplay"* — and with no minimap and no fog
there was no way back to your own base but panning there. ADR-017 banked two verbs; this spends one on a
navigation hole the MVP actually has, and banks the other.

## Consequences

**One code path serves one finger and two.** With no translation term to apply, a one-finger drag is the
same solve with no scale and no rotation. `Interface.md` §3 wants two-finger drag to pan "identically to
one finger"; after this it is identical by construction rather than by care.

**It is frame-rate independent by construction.** A rate-based camera moves at a speed set by how many
manipulation events arrive per frame. An anchored one is absolute positioning: the anchor is under the
finger or it is not.

**It is exactly the shape R21 asks for.** Camera pose, anchor and screen point in; focus point out. A pure
function with an obvious oracle — *project the anchor and check it lands on the centroid* — which is
worth more than most tests in this tree because it is a property rather than a case.

**The pitch floor is now protecting two things.** `Interface.md` §5 pins a 30° minimum pitch to bound tap
error and ADR-010's wedge. It also bounds this: an anchor near the horizon requires an unbounded focus
movement to keep under the finger, and the floor is what stops that. **Pitch therefore saturates at the
floor rather than terminating the zoom range** — `pitch(distance)` clamps its output and distance keeps
decreasing below it, or the floor would silently become a zoom-in limit.

**Orbit is the fragile gesture and this ADR is where to look when cutting it.** Three constants exist only
because rotation shares a manipulation with zoom — the 8° deadzone, its latch, and the 2% scale deadzone —
and on a kickstand a thumb-and-index rotation has roughly 50° of arc before a re-grip, so a half-turn is
three or four gestures. For a symmetric map on a plane that is a lot of effort for something largely
cosmetic. **It stays for the MVP and it is the named candidate to cut**: removing it removes four
constants and makes pinch pure zoom, which is a reliability gain rather than a feature loss.

**What it costs is a ray-plane intersection per manipulation update**, which is a divide, and a second one
for the anchor at gesture start. Against a frame that draws 110 entities this is not a number worth
writing down twice.

## Measurements

**The first two are CONFIRMED by the owner on the Surface Pro, 2026-09-23**: the ground sticks to the
finger across the pitch range, and orbit is usable one-handed on a kickstand, **so decision 4 and its
three protecting constants survive.** The third is M1.8's pin and was not part of that session. As they
were owed at **M1.16**, the first two looked at rather than asserted:

1. **Whether the ground actually sticks** across the pitch range, judged on the device — the failure is
   drift over a long gesture, which a test can catch only if it knows what tolerance to expect.
2. **Whether orbit is usable one-handed on a kickstand**, and how often a re-grip is needed. This is the
   measurement that decides whether decision 4 and its three protecting constants survive.
3. **The zoom range's two ends.** The far end is arithmetic: at a 40° vertical field of view, showing the
   whole 16,384-unit square puts the camera at **22,500 units**, and the 3:2 aspect gives about 24,600
   units of visible width, so the square fits on its height. **The near end is not pinned** — at a close
   view of roughly 1,500 world units it would be about 1,400 units, making the range **16×**, which is
   about two pinch gestures at unity gain since a comfortable pinch spans about 4× of scale. M1.8 pins
   the near end; if the range grows much past that, the lever is a gain on the scale rather than a
   different gesture. **The near end is 700 since 2026-09-24**, when the owner halved the opening
   distance and the near end followed it. That makes the range 32×, about two and a half pinches, and
   the gain is the lever if it feels long (`Interface.md` §5).
