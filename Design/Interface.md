# Interface — what the commander sees and touches

The interface of the MVP described in [`GameDesign.md`](GameDesign.md), inside `AGENTS.md` R13, R18 and
R21. **Touch is the only input and there is no fallback**, which is the constraint every decision here
answers to.

**Status: DRAFT.** §7 lists what this document finds missing and does not settle.

---

## 1. The frame

**The target device is the Surface Pro** (`OpenQuestions.md` Q6), and every number below is derived from
it rather than assumed.

| | |
|---|---|
| **Panel** | 13 inches, 3:2, **2880 × 1920** physical pixels, 267 pixels per inch. |
| **Default Windows scale** | 200%, so the `CoreWindow` reports **1440 × 960** device-independent pixels. |
| **Swap chain** | 2880 × 1920 — physical pixels, not DIPs. R18 requires the conversion that gets there to be one tested pure function. |

**The client runs fullscreen**, entered at launch. Nothing previously said so, which quietly made
everything below conditional on however the window happened to be sized; a touch-only game in a resizable
window is not a coherent object anyway.

**The world is drawn into a scene target and fitted into the back buffer, and its resolution is a scale of
the panel whose default is 1:1 — 2880 × 1920**
([`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)). **The interface is authored at 1440 × 960
and drawn afterwards, at physical resolution**
([`ADR-011`](ADR/ADR-011-the-interface-draws-after-the-scale.md)). Every layout number in this document is
an authored number and is unconditional, exactly as R13 requires.

**Those are two numbers doing two different jobs, and each has its own transform.** The world fit maps the
scene target into the back buffer; the interface fit maps this document's authored coordinates into the
back buffer. Both are values from the one place that asks the window how big it is, so nothing branches on
the window size and **text is not resampled**. They were a single transform until ADR-016, and separating
them is what makes 1440 × 960 mean what it reads as here — a statement about fingertips rather than about
fill rate. **The world's resolution can change and no number below moves.**

At the 1:1 default the world reaches the panel unfiltered, which is R13's best path; at a 0.5 scale it is
point-sampled at an exact 2×, which is sharp but doubles every pixel. It is 3:2 either way, so there is no
letterbox. **The 1440 × 960 the `CoreWindow` reports is a DIP figure and enters neither fit** — the swap
chain is created at physical pixels, so the display scale never reaches the arithmetic. An earlier version
of this section called that correspondence "not a coincidence"; ADR-007 corrected it, because dressing a
free choice as a natural consequence invites someone to "fix" the swap chain to DIPs and break it.

Everything that is not this panel is correct rather than crisp, which is what R13 buys. A Surface Pro 7
(12.3 inches, 2736 × 1824) resamples slightly. A 1080p monitor — the display a developer actually works
on — now *downscales* by 0.5625, which is supersampling; that is the *development* case and it is still
deliberately not the one optimized for, but it has stopped being the ugly one.

**At 1:1 the scene target is 5.53 megapixels and at 0.5 it is 1.38**, and that difference is the whole of
what the scale buys. 4× multisampling costs 22.1 megasamples at the first and 5.5 at the second, and space
is thin bright silhouettes against black — exactly the content that wants it (`TechnicalDesign.md` §6).
Worth carrying: **1,440 × 960 at four samples and 2,880 × 1,920 at one are the same 5,529,600 samples.**
The interface pass cannot be multisampled, because a flip-model back buffer cannot be; for rectangles and
text quads that costs nothing.

### The touch target, derived

A 13-inch 3:2 panel is **10.82 inches wide** (height² × 3.25 = 169). The authored frame spans that width,
so 1,440 authored pixels over 10.82 inches is **133 authored pixels per inch, 5.24 per millimetre**.

Microsoft's touch guidance puts the minimum target at 7 mm and the recommended one at 9 mm, which is
**37 and 47 authored pixels**.

**The minimum interactive target is 48 × 48 authored pixels**, with at least **16 pixels of clear space**
between adjacent targets. Forty-eight because it is the 9 mm recommendation rounded up, and because the
interface fit is an exact 2× so it lands on **96 physical pixels — 9.15 mm** — with no fractional edge
anywhere. A control smaller than this is a defect, not a style choice. Sixteen pixels of clearance is
3.05 mm, above Microsoft's 2 mm minimum rather than exactly on it, because this is a game and a mis-hit
costs a fleet rather than a menu.

**There are three tiers, and the tier is decided by what the player is doing when they reach for it.**

| | Authored | Physical | Millimetres | What |
|---|---|---|---|---|
| **Floor** | 48 × 48 | 96 | 9.15 | Anything interactive. Nothing is smaller. |
| **Combat** | 64 × 64 | 128 | 12.2 | Anything hit while the match is running — the selection panel's design groups, the clear target, the cancel on the build queue. |
| **Under fire** | 96 × 96 | 192 | 18.3 | The build buttons, which are hit while something is exploding (§6). |

### The gesture constants, derived

These are the numbers under the seam and the hit test. R21 requires the arithmetic beneath a gesture to be
a pure function with a suite over it, and a number nobody wrote down is a number the suite cannot pin.

| | Authored | Millimetres | Why this number |
|---|---|---|---|
| **Pick radius** | 24 | 4.58 | Half the 48-pixel floor, so a world tap has the same reach as an interface target and a 4-pixel ship at tactical zoom is still hittable. A tap takes the **nearest** candidate inside it. |
| **Tap slop** | 16 | 3.05 | The travel that separates a tap from a pan. Above handheld tremor, below a third of the floor. **Do not inherit `GestureRecognizer`'s default** — pin this, and begin the pan at the point the threshold was crossed so the first 16 pixels are not lost to a jump. |
| **Double-tap window** | — | — | 300 ms, and the second tap is matched **by entity identity** rather than by distance ([`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md)) — a moving ship must still be the same ship. |
| **Contact rejection** | 78 | 14.9 | A contact whose `ContactRect` exceeds this in either dimension is a palm, not a finger, and is dropped at the seam (§2). A fingertip is 8–12 mm. |

**The pick order is stated, because "what is under it" is not a total order when things overlap:** **any
interface target first**, then own ship, then own station or module, then hostile, then asteroid, then
empty space. Nearest wins inside a tier; the tier wins across one.

**The interface heading that list is not obvious and was missing.** This order named only world entities
until [`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md), and said nothing about a tap
that lands on a panel and a ship at once — which is every tap near the bottom of the screen.

### Where the hands are

**The posture is the kickstand on a desk or a lap, and one or two index fingers.** This document
previously said the tablet is held at its sides with the thumbs reaching the bottom corners, which is the
right model for a phone or an 8-inch tablet and the wrong one for this device: a Surface Pro 11 is
**287 × 208 mm and 895 grams**. Nobody plays a twenty-minute match holding nine hundred grams in two
hands, and at 287 mm across the far bottom corner is outside a comfortable thumb arc even if they did.

**That changes which constraint binds.** Reach stops mattering and **occlusion starts**: a player reaching
in covers the target and a wedge of screen around it with hand and forearm, on the side of their dominant
hand. The frequently used controls still belong along the bottom edge, where they are out of the way of
the playfield and close to a resting hand — but *which* corner is a handedness question rather than a
reach question. **It is a setting, right-handed by default** (`OpenQuestions.md` Q33, answered 2026-09-23):
left-handed is the mirror, and nothing else changes.

**One consequence is already actionable**: anything the player must *read* while their hand is on the
screen must not be under that hand. The selection panel is the readout that matters during a gesture, and
§4 puts it on the opposite side of the interaction it reports.

---

## 2. The seam

`Windows::UI::Input::GestureRecognizer` is the single path from the `CoreWindow` into the game (R21).
`PointerPressed`, `PointerMoved` and `PointerReleased` are forwarded to it; **a `PointerPoint` whose
`PointerDeviceType` is not `Touch` is dropped at exactly one site**, and keyboard events are not
subscribed at all. The recognizer emits `Tapped`, `Holding` and manipulation updates carrying translation,
scale and rotation, and those are the only interactions that exist.

**The seam counts contacts, because the recognizer does not tell you.** A manipulation means two different
things depending on how many fingers began it, so the seam records the contact count at
`ManipulationStarted` and the manipulation keeps that meaning until it ends. **Putting a second finger
down mid-drag does not change what the drag is doing** — the alternative is a camera that lurches whenever
a thumb brushes the glass.

**The seam rejects palms, at the same site that drops non-touch pointers.** On a 287 mm screen played on a
desk the heel of a hand reaches the glass routinely, and the contact-count latch above only protects a
gesture already in progress — a palm landing *first* starts a manipulation of its own and the camera
lurches. `PointerPoint.Properties.ContactRect` gives the contact's extent; anything wider or taller than
**78 authored pixels, 14.9 mm** (§1) is not a fingertip and never becomes an input record. One test, one
site, and the game stops being unpleasant on a table.

Everything downstream of the seam is a pure function over an input event and the camera, lives in
`NeuronClient` or `GameClient`, and has a suite over it. R21 is explicit about why: the sign of a pinch
and the sign of a rotation are things a package can hide and a test cannot.

---

## 3. The vocabulary

**One finger is intent. Two fingers are the camera.** That is the whole rule, and every gesture below is
an instance of it.

| Gesture | What it does |
|---|---|
| **One-finger tap** | The verb. What it does depends on what is under it — §4. |
| **One-finger double tap on your ship** | Selects that ship, then expands to **every ship of the same design within a circle centered on it**. The first tap fires at once; the second upgrades it ([`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md)). |
| **One-finger drag** | Pans the camera. **Unconditionally** — it has no second meaning and never has. |
| **Two-finger pinch** | Zooms — and with it, pitches. |
| **Two-finger rotate** | Orbits. |
| **Two-finger drag** | Pans, identically to one finger. It comes free from the same manipulation and refusing it would be a surprise. |
| **One-finger hold on empty space** | **Recenters** the camera on the selection, or on your station when nothing is selected ([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)). |
| **One-finger hold on a ship** | **Nothing**, and deliberately — the second of the two verbs ADR-017 freed is still banked (§7). |

**One finger has exactly two meanings, separated by whether it moved**, which is as unambiguous as a single
pointer gets. A tap is the verb, a drag is the camera, and **§1's 16-pixel tap slop** decides which — so a
slightly sloppy tap pans instead of issuing an order. That is the right failure: an accidental pan costs
nothing and an accidental move order costs a fleet.

**The threshold is pinned here rather than inherited from the recognizer**, because the asymmetry above
only holds if the number is right. Too small and a normal handheld tap — five to ten physical pixels of
travel is ordinary — loses the order *and* displaces the camera, which is two costs rather than none. The
pan begins at the point the threshold was crossed, so nothing jumps when it engages.

**A double tap is not a third meaning.** It arrives on `Tapped` with a count, the first tap has already
acted, and the second only ever expands a selection — so no tap in this game waits to find out what it
is (`ADR-017`).

**There is no band select.** A finger cannot draw a rectangle without that drag meaning two things, and the
hold-and-drag arrangement that would have allowed it consumed `Holding` — one of only three verbs R21
gives — to buy a gesture that is not self-evident and is slow.
[`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md)
records why, and §4 says what replaces it.

**`Holding` is unassigned everywhere**, deliberately, and since
[`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md) that is two gestures in reserve rather than
one — over a ship as well as over empty space. ADR-010 rejected hold-and-drag for a latency it then paid
itself by adopting a plain hold; a double tap is faster, is the idiom every RTS player already knows, and
hands the verb back.

## 4. Selection and orders

**A tap's meaning comes from what is under it** — within §1's **24-pixel pick radius**, nearest first,
in the stated tier order — which is the only way to have a verb without a modifier key. A point hit test
against a four-pixel silhouette is a coin flip, and the failure is the expensive one: you miss the ship,
hit empty space, and the fleet you had selected flies there. With something selected:

| Tapped | Order |
|---|---|
| Empty space | Move there — **or, with your station selected and a module chosen, place that module** (§6). |
| A hostile ship or station | Attack it. |
| An asteroid with ore | Mine it — miners in the selection take it, the rest move to it. **This is a standing order**: the miner shuttles until told otherwise. |
| Your own station | Opens the build panel; the selection is unchanged. |
| One of your own modules | **Nothing**, and the selection is unchanged, **unless an L2 upgrade is armed**, when it upgrades that module if it is the level the upgrade takes (`OpenQuestions.md` Q54, Q57). |
| One of your own ships | Replaces the selection with that ship. |

A tap on empty space with **nothing** selected does nothing.

**A rock is picked where it is drawn** (M2.8): at its center, including the height the client drew it at,
up to 240 units off the plane (ADR-005). A tap aims at what is on screen. The order still names the rock by
its field index, and the ships that cannot mine are sent to its place on the plane, since the simulation has
no height (R22).

### Selecting more than one

**A double tap on one of your ships selects it and then every ship of the same design within a circle
centered on it** ([`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md)). **The first tap selects
that one ship immediately**, exactly as a single tap always has; a second tap that resolves to the *same
ship* within 300 milliseconds expands the result. Nothing is deferred waiting to see whether a second tap
arrives, so no tap in this game got slower, and an expansion abandoned halfway leaves one ship selected
rather than nothing.

It replaced a hold, which fired no faster than the hold-and-drag ADR-010 rejected for being slow, and
which asked a hand to stay motionless for half a second on a handheld device in the middle of a fight.

The circle is unchanged: **screen-space, 192 authored pixels in radius** — four times the touch floor,
about a quarter of the frame's width — **drawn during the gesture** so there is no invisible rule about
what is included, centered on the *ship* rather than the finger because the finger is covering the ship,
own ships only, same design only, fixed radius.

**The circle is largely under the player's hand, so the count is read somewhere else.** At 192 pixels of
radius it is 73 mm across and a reaching hand covers a good part of it. The drawn circle stays — it is the
rule made visible — but the **selection panel updates live during the gesture**, and it is on the opposite
side of the screen from the hand that is doing it (§1, §6). That is the readout the player actually
watches.

**The camera is the group-size control.** Because the circle is screen-space, zooming in takes a squad and
zooming out takes the fleet, using a gesture the player is already driving constantly. This is what a
world-space radius would not give: the drawn circle would shrink on screen as the camera pulled back and
stop meaning anything.

**What this cannot do is select an arbitrary subset.** There is no way to take fourteen particular
fighters; you take one ship, or a design within a circle, so splitting a fleet means two camera positions
and two holds. At fifty ships across three designs that is a small loss, and it is stated rather than
discovered.

Beyond that, tapping a design group in the selection panel narrows an existing selection to it (§6), which
is the complementary operation — the hold expands spatially, the panel filters by design.

**Deselecting is the clear button on the selection panel.** There is no gesture for it: a tap on empty
space is already a move order, and the band-that-encloses-nothing this document used to rely on no longer
exists.

**An order is acknowledged the instant you give it, locally.** A tap is not visible on the ships for 152
milliseconds at best (`TechnicalDesign.md` §4), and a touchscreen has no cursor to say it registered. So
the client draws a destination marker and a line from the selection **the moment the gesture resolves**,
and clears it when the host acknowledges that command's sequence. **Nothing is predicted** — the ships do
not move until the host says they did. R19 forbids the client simulating, not the client drawing what it
asked for.

Orders replace; there is no queueing and no shift-equivalent, because there is no shift. Order queueing
needs a gesture, and since [`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md) there is one free —
it is still out of the MVP, but for want of a decision rather than for want of a verb (§7).

**There is no attack-move and no stop, and neither is missing.** Attack-move exists to substitute for a
target you cannot see; with no fog of war in the MVP you can always see the target and tap it directly.
Stop is a move order to where the selection already is. **Both become real gaps the day fog ships**, and
that is the milestone to reopen them at, not this one.

## 5. The camera

The camera always looks at a **focus point on the plane** and never rolls. Pan moves the focus point,
which is clamped to the play area plus a margin. Orbit turns the camera around it. Zoom changes the
distance.

**How a finger becomes that motion is one decision and it is
[`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md): the camera is anchored to the plane.** A
ray through the contact centroid at `ManipulationStarted` meets the plane and that world point is kept;
each update solves for the focus that puts it back under the current centroid. **The ground sticks to the
finger**, which is what every map application does and what hands already expect — the alternative, moving
the focus by the finger's delta times a constant, slides the ground at a different rate from the finger
and at a different rate again at the top of the screen than the bottom, because that is what perspective
does.

**One solve does all of it**, so scale gives the new distance and pitch, rotation gives the new heading,
and then a single anchor solve places the focus. **The recognizer's translation is not applied on top** —
that double-counts, and a camera that accelerates is what it looks like. Orbit turns about the anchor
rather than the focus for free, and a one-finger drag is the same solve with no scale and no rotation,
which is why §3's "two fingers pan identically to one" is true by construction here rather than by care.

**At the clamp the ground stops and the finger slides over it.** The focus is clamped after the solve, so
the anchor slips at the edge of the play area. That is correct and it is stated because it reads as a
defect the first time it is seen. **There is no inertia**: the action immediately after positioning this
camera is a precise tap, and momentum fights that.

**Pitch is coupled to zoom and is not separately controllable.** Fully zoomed out the camera is near
top-down; fully zoomed in it rakes low across the plane. This is not a compromise made reluctantly — it
removes a whole degree of freedom from a gesture budget that has very little left, and it gives an RTS
exactly what it wants at each end: a tactical read when out, a fleet in silhouette when in. What it costs
is the deliberate low-angle overview shot, which is a screenshot rather than a control.

**Low has a floor, and the floor is what keeps a tap accurate.** As the camera rakes, a screen pixel near
the top of the frame maps to an ever-larger world distance — so tap precision collapses exactly when the
player has zoomed in *for* precision, and ADR-010's screen circle stretches into an ever-longer wedge.
Both are the same effect and one number bounds both.

**The vertical field of view is 40° and the minimum pitch is 30° above the plane.** At that floor the top
edge of the frame looks 10° down, which meets the plane at **5.67 camera heights against 1.73 at the
frame's center — a 3.27× stretch, top to middle** (M1.8, `GameClientTests`), and the horizon is never on
screen. Lowering the floor buys a more raking silhouette and pays for it on that ratio, which grows
without bound as the top edge approaches the horizontal; raising it costs the look. The wedge ADR-010
warns about cannot be worse than this.

**M1.8 measured a second ratio, because the one above is not the one that bounds a tap.** The
ground-distance ratio is **not monotonic in pitch** — 3.27× at the 30° floor and **5.33× at the 85°
ceiling** — which reads as though the camera is worse when zoomed out and is not: near top-down the
frame's center sits almost directly beneath the camera, so a small absolute difference is a large ratio.
What decides how much world a pixel covers is the projection's local scale, which goes as
`1 / sin²` of the depression angle. **That ratio is monotonic and is worst exactly at the floor — 8.29×
there against 1.21× fully out** — so it is the measure this paragraph's claim is true of, and both are
pinned. The floor bounds a third thing:
an anchor near the horizon would need an unbounded focus movement to stay under the finger (ADR-018).

**The floor saturates the pitch; it does not end the zoom.** `pitch(distance)` clamps its *output* at 30°
and distance keeps decreasing below the point where it bottoms out. Read the coupling the other way — a
floor on pitch terminating the range — and the camera quietly loses its close zoom, which is not what the
floor is for.

**Rotation has a deadzone, and it latches**: the camera ignores a manipulation's rotation until it exceeds
about eight degrees, because two fingers dragging to pan are never exactly parallel and a camera that yaws
whenever you pan is unusable. **Once it engages it stays engaged for the rest of that manipulation** —
without the latch the camera stutters every time the player crosses back under the threshold mid-gesture,
which is worse than no deadzone at all. **And it rebases at the crossing, exactly as §3's tap slop does**
— the heading follows the fingers from the point the eight degrees was passed, so engaging rotation moves
nothing (`OpenQuestions.md` Q38). What settles that is what the deadzone is *for*: a pan and a pinch
rotate by accident, so the crossing is usually reached unintentionally, and passing the whole cumulative
through would snap the world eight degrees in the middle of a pan — making the accident this exists to
absorb worse rather than better. **It costs about eight degrees of every deliberate orbit** against the
fifty a kickstand grip has before a re-grip
([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)), which is the same price §3 pays for the
pan and one more item on the bill if orbit is cut.

**Zoom needs a small deadzone too, and for the mirror-image reason.** Fingers that rotate also change
separation slightly, so a pure orbit otherwise creeps the zoom — and since pitch is coupled to zoom, an
orbit silently re-pitches the camera. **Two per cent of scale** is below that noise and above nothing a
player intends. Pan and zoom still compose freely, as §3 says; rotation is the one axis that is gated, and
gating it needs both numbers.

**On release the heading snaps to the nearest cardinal** when it is within a stated threshold of one.
There is no minimap and no compass, so a map that is reliably north-up whenever the player is not
deliberately turning it is worth one constant — spatial memory is what stands in for the minimap this
design declines.

**Orbit is the fragile gesture, and it is the named candidate to cut.** The three constants above exist
*only* because rotation shares a manipulation with zoom, and on a kickstand a thumb-and-index rotation has
about 50° of arc before a re-grip, so a half-turn is three or four gestures. For a symmetric map on a
plane that is effort spent on something largely cosmetic. It stays for the MVP; if M1 finds two-finger
manipulation unreliable, **cutting it removes four constants and makes pinch pure zoom**
([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)) — a reliability gain rather than a
feature loss.

**The zoom range is about two gestures, which is why no gain constant is needed.** At the 40° field of
view above, showing the whole 16,384-unit square puts the camera at **22,500 units**, and the 3:2 aspect
gives roughly 24,600 units of visible width so the square fits on its height. **The near end is 1,400**
(Q40), making the range **16.07×**, pinned by `GameClientTests` at M1.8 — and a comfortable pinch spans
about 4× of scale, so two of them cover it. If the range ever grows much past this the lever is a gain on
the scale rather than a different gesture.

**The near plane is 50 units and the far plane is 50,000** (Q39). Neither was stated, and the range they
span is wide: the tactical camera sits at 22,500 units over hulls 18 to 54 units tall. **Depth precision
is set by the near plane, not the far one** — at a 24-bit buffer, near 1 resolves about 30 units at
tactical range and the station's 12-unit plate step disappears into z-fighting; near 50 resolves about
**0.6 units**, a factor of twenty in hand, and is still far closer than the 1,400-unit camera ever gets to
a hull. **The case to look at on the device is the station at maximum zoom-out**, where every term is at
its worst at once; a reversed-Z buffer is the lever if it is ever not enough.

**The close end of the zoom is 1,400 units and the plates were rendered at 1,500** (Q40). The difference
is not visible — a 60-unit hull is 57 authored pixels at 1,400 and 53 at 1,500, both inside the 53–79 the
mesh handoff's combat plate was accepted against — and what it changes is the zoom range, 16.07× against
15.0×. **1,400 stands** because this document and
[`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md) both already said it and
`GameClient/Camera.h` was built to it; the handoff's README records the discrepancy and is corrected
rather than this.

**The camera opens on your own station at 2,400 units**, which shows about 2,620 units across — a
220-unit station at roughly 120 authored pixels, with its 400-unit module radius on screen around it. It
could not do this before [`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md), because nothing
told the client which player it was; the opening pose was an arbitrary point near the middle. **A
recenter moves the focus and leaves the zoom alone**, so the opening distance is what decides how large
your base is, and it is deliberately not the near end: 1,400 would be pinned against the limit with
nowhere to go but in, and — because pitch is coupled to zoom — would also be the most raking view the
camera has.

**There is no minimap.** Because pitch is coupled to zoom, **maximum zoom-out is already a top-down
tactical view of the whole map** — a minimap would be a second, smaller, lower-fidelity copy of a view
that is one gesture away, costing a second render of every entity, a second coordinate space and a second
hit test, for a quarter of the frame's height. With a symmetric map and no fog of war there is nothing on
it a player does not already know.

**The sky is the other half of what a minimap gives, and it is free.**
[`ADR-019`](ADR/ADR-019-the-sky-is-generated-from-the-seed.md) fixes a generated star field in world
space, so it rotates with heading and pitch and does **not** translate with pan — panning across the map
leaves it still. With no minimap and no compass, that is which-way-am-I-facing, answered by the backdrop.
It is why ADR-019 is not only decoration, and it is dim enough (12% over any large area) not to cost the
silhouette legibility this design is built on.

**What a minimap does provide is a way back, and that is a hold rather than a panel.** `GameDesign.md` §7
names the problem — a defender has to be watching the right part of a 16,384-unit map at the right moment
— and until now the only way back to your own base was panning there. **A hold on empty space recenters**
on the selection, or on your station when nothing is selected (ADR-018, spending one of the two verbs
ADR-017 freed). It is the cheap half of a minimap without the second render, the second coordinate space
or the second hit test.

---

## 6. The panels

Five things are drawn over the scene. All of them are `GameClient` (R20), and all draw in the interface
pass at physical resolution (§1).

| | Where | What |
|---|---|---|
| **Credits** | Top left | The number, and **a change flash under it**, cyan on a gain and amber on a spend — **and no income rate**, which `OpenQuestions.md` Q36 ruled out of the MVP on 2026-09-23 for a reason that holds. |
| **Selection** | Bottom, away from the reaching hand | What is selected, grouped by design with a count, a hull bar, and **a cargo bar on anything that carries ore** — four chips, zero to four lit, which is what the wire carries (`TechnicalDesign.md` §4, `OpenQuestions.md` Q53). Tapping a group narrows the selection to it; a **clear** target deselects everything, which is the only way to do it (§4). **It updates live during a double tap**, because it is the count the player can read while their hand covers the circle (§4). |
| **Build** | Bottom, on the reaching hand's side (Q33) | Visible when your station is selected. **Two rows**: ships on top — Miner and Fighter — and modules below. **Unaffordable and unavailable are two different states and look different**: an item you cannot yet pay for keeps its button lit and reddens its cost, and an item you have no module for is dimmed entirely — in the MVP an L2 with no L1 of its kind, and a module past the cap of four. From M2 a module gates what can be built, so a single gray would leave a player unable to tell "save up" from "build something else first". Below both, **the item currently building and its progress**, tappable to cancel. One queue slot serves both, so a module and a miner compete for it — and **every available button stays live while it builds, so a tap replaces what is in progress**, which is the common path into `OpenQuestions.md` Q35 rather than the cancel target. |
| **Alert** | The screen edge, in the direction of the event | **You are being attacked somewhere you cannot see** ([`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md)). Derived from the fire events and the removal list the client already has, so it costs no wire bytes; **nothing shows if the event is already on screen**; one indicator per cluster with a count, **at most three at once** — two that would collide merge and sum, a fourth replaces the oldest; fades over a few seconds. **No scrim and no plate behind it**: the moment an alert looks like chrome it becomes chrome. **Tapping it recenters the camera there** — a hit-test rectangle rather than a gesture, so R21's budget is untouched. 64 × 64, the combat tier. |
| **System** | Top center, small | Connection state, the **reconnecting** overlay after a resume (§7), the **result overlay** when a match ends — **which names the winner**, since from M4 there are three and four players and "you lost" does not say to whom — and the button that quits via `CoreApplication::Exit`, **which arms on the first tap and quits on the second, and disarms itself after four seconds** — there is no pause, no save and no rejoin, so a single stray contact would otherwise end a five-minute match with no recovery path anywhere in the system. There is no Alt+F4 and no title bar. |

Every target in every panel is at least 48 × 48, and §1's three tiers decide which are larger. **The build
buttons are 96 × 96** — 18.3 mm, twice the floor — because they are the ones a player hits while something
is exploding. **The selection panel's design groups, its clear target and the build queue's cancel are
64 × 64**, 12.2 mm: they are hit under the same pressure as the build buttons and were previously at the
floor, which is the size for something you reach for between fights.

**Hull bars are drawn in the world too, and only on damaged ships.** The selection panel shows the hull of
what you have selected, which leaves everything else invisible — and reading fleet health at a glance is
most of what an RTS player does with their eyes. A ship at full hull draws nothing, so the map is quiet
until something is wrong and the cost is one quad per damaged ship, at most 110. **They are positioned by
projecting the ship's world position and then through the interface's fit transform** (ADR-011, ADR-016),
which is the same path a world-anchored marker already takes.

**Nothing here is a Windows Runtime control.** There is no XAML anywhere in this tree (R18), so every panel
is geometry and text the renderer draws, and a "button" is a rectangle the hit test knows about. That is a
real cost — no free text layout, no free scrolling, no accessibility — and it is what R18 buys elsewhere.

### Placing a module

**Choosing a module in the build panel arms a placement**, and the interaction it uses was dead: a tap on
empty space is a move order, and the station cannot move. So with the station selected and a module chosen,
**a tap inside the build radius places it** ([`ADR-015`](ADR/ADR-015-the-base-is-built-from-modules.md)).

**The radius is drawn while a module is armed** — 400 world units around the station, the same distance as
its point defense — and a tap outside it, on the station, or on another module does nothing. A second tap
on the armed module in the panel disarms it. **A tap on one of your own ships is still a selection**, which
closes the panel and the arming with it. One order is sent per arming.

**An L2 is an upgrade, not a placement** (`OpenQuestions.md` Q54): arming it and tapping one of your L1
modules of that kind upgrades it in place, for the difference in cost. Its button shows that difference.

The preview is client-side and the host validates: the same rule evaluated on both sides, from `GameCore`,
which is what R19 permits and what R23 already does for the map.

### Text

**Glyphs come from DirectWrite, rasterised into a texture atlas at startup**
([`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md)). The family is Segoe UI, pinned, and a
missing family is a startup failure rather than a substitution — a substituted font has different advance
widths, and R13 requires every position in this document to be unconditional.

**Two sizes**: a body size for readouts and a larger one for the build buttons, both authored and both
rasterised at the physical size the fit transform produces — 48 physical pixels for a 24-authored-pixel
label on a Surface Pro. **Text is therefore not doubled and not resampled.**

This reverses what this document said on 2026-09-20, which accepted pixel doubling on the grounds that
this interface is large type with a dozen strings. That was true of the MVP and expires with it: **M4 is a
ship designer and a research tree**, which is precisely the dense small type R13 warns about, and by then
every layout number would have been authored against a doubled pixel. `ADR-011` moved it while it cost
eighty lines.

### Where the geometry lives

**This document settles what the interface is and why. It does not say where anything is drawn**, and
that was deliberate — a coordinate in prose is a coordinate that drifts. The gap is now filled by
[`design_handoff_hud/`](design_handoff_hud/README.md): **every rectangle in integer authored
coordinates, the palette, the type scale and the motion table**, with a machine-readable
`geometry.json` that M1.14's test asserts against rather than a reviewer measuring by eye.

**The authority is split, and the split is the point.** This document is the source for what exists and
why — the frame, the three tiers and the clear space, the gesture vocabulary, the pick order, which five
surfaces there are and what data reaches them. The handoff is the source for what this document
deliberately never stated: **where each rectangle sits, what color it is, which type size it uses, and
how it moves.** Where the handoff restates a figure from here it is echoing rather than deciding, and
**this document wins.**

That last sentence is load-bearing, because the handoff echoes a good deal of §1 and §4 — the frame, the
tiers, the sixteen pixels, the pick order, the data the client holds — so the handoff is the fourth place
this tree states some of them. **`Scripts/CheckDesign.py` reads `Design/*.md` and never reaches into that
JSON**, so [`Scripts/CheckHudGeometry.py`](../Scripts/CheckHudGeometry.py) exists to close the half that
can be closed mechanically: it fails when the frame, the three tiers or the sixteen pixels in
`geometry.json` stop matching §1's table. **The prose copies it cannot check — the pick order, the
gesture vocabulary, the data the client holds — still move by hand.**

**Four things the handoff decided rather than echoed**, each recorded above or on the register, because a
decision that lives only in a handoff is a decision nobody can find: the quit is two taps; **the alert
caps at three concurrent indicators**, never closer than 112 pixels along an edge, merging on collision
and replacing the oldest beyond three — [`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md)
had the cluster rule and no cap; **at most four order lines**, one per selected design group; and a
design that carries no ore draws **no cargo row at all** rather than an empty one.

**One thing it contradicts.** The handoff states that there is no income rate and that it *has no data
path* — and that reason is false: the per-player block, plus the build item the client already watches,
give two derivations, which is what `OpenQuestions.md` Q36 exists to choose between. The conclusion may
still be the right one, but it has to be reached rather than inherited, and **the error came from the
brief the handoff was generated from** rather than from the design. Q36 settled *whether* before M2.7: **no
rate, the change flash only**, reached for its own reason rather than the handoff's.

## 7. Suspend, resume, and what is still open

**A packaged application is suspended when it loses the foreground, and the match runs on without it.**
On resume the client reconnects and shows a **reconnecting** overlay until the first snapshot lands, then
returns straight to play. **It is recognized as the same player by the session token it kept**
([`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md)); a relaunch and a resume are the same
path, which is what makes this and `GameDesign.md` §2's disconnect one mechanism rather than two. **A resume is noticed as silence**: a seated client that goes a second without a
snapshot calls the link lost, rejoins with its token and raises the overlay. A suspension is always
longer than that, and a dropped link is the same event, so there is no separate suspend path to get
wrong. The second is untuned and M1.16 looks at it. That is mechanically cheap: every record is self-contained
([`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md)), so a rejoined client is current within
one sweep and there is no resynchronization to get wrong.

**The player's fleet was at risk the whole time they were away**, and nothing mitigates that. It is the
honest consequence of a match that does not pause, and it is the same behavior a disconnected player gets
(`GameDesign.md` §2).

### What this document does not settle

**One of the two freed `Holding` gestures is spent and the other is banked.** `Holding` was the one verb
left over after [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md); moving group selection onto
a double tap ([`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md)) freed it over a ship as well,
making the reserve two. **A hold on empty space now recenters the camera**
([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)) — it went to a navigation hole the MVP
has today rather than to a feature it might want later, which is the test.

**A hold on a ship still means nothing, and that is a decision.** Order queueing is the standing candidate
— §4 notes it is now short a decision rather than short a verb — and a stop-and-hold-position is the
other. ADR-010's instinct was right and survives its own gesture being replaced: an idle affordance costs
nothing, and a verb spent on something marginal is not there when something real needs it. A map ping and
a map-wide select-by-design were each considered and each declined; the jump-to-station that was declined
with them is what ADR-018 has now put back, which is worth noticing — it was declined for want of a verb
and returned the moment there was one.

**An asteroid's ore level has no appearance, and that is a decision** (Q42). Q22 replicates a quantized
ore bucket from M3 and the client draws it **as the number in the selection panel and nowhere else**. The
question a player is asking — is this field worth holding — is answered by that number, and four buckets
across five authored variants is twenty combinations to model for one line of text. **The cheap version if
it is ever wanted** is the vertex-color hull tone: the `G` channel already selects between three palette
entries, so a depleted rock could shift toward the deep tone with no new material and no second draw.

**The shape-coded overlay is not in the MVP, and cutting it has a cost worth naming** (Q45). Plate 7 of
the mesh handoff specifies a readout that identifies hulls at tactical zoom by shape rather than by
silhouette. It is approved as a specification and it is not on the MVP list — the readouts that ship are
the selection panel and [`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md)'s edge
alert. **What must not happen is cutting it silently**: the hulls were tuned for the combat view rather
than bent into silhouettes that stay legible at 3.5 pixels, *because* the overlay was expected to carry
identification at tactical zoom. Dropping it reopens that trade, and the answer to it would be re-authoring
thirteen meshes rather than adjusting a number.

### When a match ends

**The host reseeds and starts another; the client shows a result overlay and reconnects into it.** Nothing
previously said what happened at victory — whether the host exited, reset or simply stopped — and for a
solo-against-AI testing loop **restart is the single most-used operation in the project**. `GameDesign.md`
§10 makes twenty matches in an evening the point of the reduced MVP; that is not possible if playing again
means relaunching a packaged application.

### What is left to a hand and a screen

None of these is a question. All are **confirmations owed at M1**, and all are settled by using the thing
rather than by arguing about it:

1. ~~**Whether 192 pixels is the right circle** (§4) — and specifically whether the raking-camera case
   selects the wrong ships, since a circle on screen is a wedge in the world
   ([`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md)). §5's pitch floor now bounds how bad
   that can get; what it cannot say is whether the bound is comfortable.~~ — **CONFIRMED by the owner on the Surface Pro, 2026-09-23.**
2. ~~**Whether the interface pass costs more GPU time than the world pass**, which is likely: five instanced
   draws of simple geometry against an unbatched quad per glyph.~~ — **It does not**: 50 µs against the
   world's 852 on the device, 2026-09-23. `TechnicalDesign.md` §9.6 has the setup.
3. ~~**Whether §1's gesture constants are right** — the 16-pixel tap slop above all, because it is the one
   that decides how often an intended order becomes a pan. The 24-pixel pick radius and the 300-millisecond
   double-tap window are the other two, and all three are single constants behind a tested pure function.~~
   — **CONFIRMED by the owner on the Surface Pro, 2026-09-23.** All three constants stand.
4. **Whether occlusion, rather than reach, is the constraint that binds.** *Not checked at M1, which closed
   without it on 2026-09-23; carried to the next hand session.* Which side the panels go on is
   answered (`OpenQuestions.md` Q33: a setting, right-handed by default); whether the model behind it is
   right is what playing checks.
5. ~~**Whether the ground actually sticks to the finger** across the pitch range
   ([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)). The failure is drift over a long
   gesture, and a test catches that only if it already knows the tolerance to expect.~~ — **CONFIRMED by the owner on the Surface Pro, 2026-09-23.**
6. ~~**Whether orbit is usable one-handed on a kickstand**, and how often a re-grip is needed. This is the
   one that decides whether orbit and its three protecting constants survive §5's kill-switch.~~ —
   **CONFIRMED by the owner on the Surface Pro, 2026-09-23.** Orbit survives.
7. **The near end of the zoom range** (§5), which is the only one of the two that arithmetic does not
   already give. *Not checked at M1 either, and carried forward with item 4.*

**Anything a second player needs to say to a first is out of the MVP deliberately.** There is no chat, no
ping and no map drawing; solo against AI is the only configuration the MVP can test, and the gesture a
ping would have used is the one being held in reserve above.

**How a client finds a host is settled and is not here:** it is a configuration value with a compiled-in
default, there is no discovery and no address entry, and the consequences are
[`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md).
