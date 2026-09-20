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

**The world is authored at 1440 × 960** and drawn into a scene target at that size, then fitted into the
back buffer. **The interface is authored at 1440 × 960 too — and drawn afterwards, at physical
resolution** ([`ADR-011`](ADR/ADR-011-the-interface-draws-after-the-scale.md)). Every layout number in
this document is an authored number and is unconditional, exactly as R13 requires; at draw time each one
is carried through the same fit transform the present step already computed, so nothing branches on the
window size and **text is not resampled**.

**That fit is exactly 2×, so it is point sampling and it is crisp** — and it is 3:2, so there is no
letterbox. This is not a coincidence: 200% is the scale Microsoft ships on this panel, so the authored
frame *is* the DIP frame and the swap chain is exactly twice it. R13's whole architecture exists to make
the exact-multiple path common, and on the target device it is the only path taken.

Everything else is correct rather than crisp, which is what R13 buys. A Surface Pro 7 (12.3 inches,
2736 × 1824) scales 1.9× and resamples slightly. A 1080p monitor — the display a developer actually works
on — fits 1.125× and pillarboxes; that is the *development* case and it is deliberately not the one
optimised for.

**A 1,440 × 960 scene target is 1.38 megapixels**, which is small. That matters more than it sounds:
4× multisampling costs 5.5 megasamples, which is affordable, and space is thin bright silhouettes against
black — exactly the content that wants it (`TechnicalDesign.md` §6). The interface pass cannot be
multisampled, because a flip-model back buffer cannot be; for rectangles and text quads that costs
nothing.

### The touch target, derived

A 13-inch 3:2 panel is **10.82 inches wide** (height² × 3.25 = 169). The authored frame spans that width,
so 1,440 authored pixels over 10.82 inches is **133 authored pixels per inch, 5.24 per millimetre**.

Microsoft's touch guidance puts the minimum target at 7 mm and the recommended one at 9 mm, which is
**37 and 47 authored pixels**.

**The minimum interactive target is 48 × 48 authored pixels**, with at least 12 pixels of clear space
between adjacent targets. Forty-eight because it is the 9 mm recommendation rounded up, and because at the
exact 2× scale it lands on **96 physical pixels — 9.15 mm** — with no fractional edge anywhere. A control
smaller than this is a defect, not a style choice.

### Where the hands are

A tablet is held at its sides and the thumbs reach the bottom corners. **The frequently used controls
live in the bottom corners deliberately** — selection on the left, build on the right — and the middle of
the bottom edge is kept comparatively clear, because that is the part of the screen a held tablet's thumbs
cannot reach without regripping.

---

## 2. The seam

`Windows::UI::Input::GestureRecognizer` is the single path from the `CoreWindow` into the game (R21).
`PointerPressed`, `PointerMoved` and `PointerReleased` are forwarded to it; **a `PointerPoint` whose
`PointerDeviceType` is not `Touch` is dropped at exactly one site**, and keyboard events are not
subscribed at all. The recogniser emits `Tapped`, `Holding` and manipulation updates carrying translation,
scale and rotation, and those are the only interactions that exist.

**The seam counts contacts, because the recogniser does not tell you.** A manipulation means two different
things depending on how many fingers began it, so the seam records the contact count at
`ManipulationStarted` and the manipulation keeps that meaning until it ends. **Putting a second finger
down mid-drag does not change what the drag is doing** — the alternative is a camera that lurches whenever
a thumb brushes the glass.

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
| **One-finger hold on a ship** | Selects that ship **and every ship of the same design within a circle centred on it**. |
| **One-finger drag** | Pans the camera. **Unconditionally** — it has no second meaning and never has. |
| **Two-finger pinch** | Zooms — and with it, pitches. |
| **Two-finger rotate** | Orbits. |
| **Two-finger drag** | Pans, identically to one finger. It comes free from the same manipulation and refusing it would be a surprise. |

**One finger has exactly two meanings, separated by whether it moved**, which is as unambiguous as a single
pointer gets. A tap is the verb, a drag is the camera, and the recogniser's own movement threshold decides
which — so a slightly sloppy tap pans a few pixels instead of issuing an order. That is the right failure:
an accidental pan costs nothing and an accidental move order costs a fleet.

**There is no band select.** A finger cannot draw a rectangle without that drag meaning two things, and the
hold-and-drag arrangement that would have allowed it consumed `Holding` — one of only three verbs R21
gives — to buy a gesture that is not self-evident and is 300 ms slow.
[`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md)
records why, and §4 says what replaces it.

**`Holding` over empty space is unassigned**, deliberately. It is the one gesture this design has left over.

## 4. Selection and orders

**A tap's meaning comes from what is under it**, which is the only way to have a verb without a modifier
key. With something selected:

| Tapped | Order |
|---|---|
| Empty space | Move there. |
| A hostile ship or station | Attack it. |
| An asteroid with ore | Mine it — miners in the selection take it, the rest move to it. |
| Your own station | Opens the build panel; the selection is unchanged. |
| One of your own ships | Replaces the selection with that ship. |

A tap on empty space with **nothing** selected does nothing.

### Selecting more than one

**A hold on one of your ships selects it and every ship of the same design within a circle centred on it.**
The circle is **screen-space, 192 authored pixels in radius** — four times the touch target, about a
quarter of the frame's width — and it is **drawn while the finger is down** so there is no invisible rule
about what is included. It is centred on the *ship* rather than the finger, because the finger is covering
the ship. Own ships only, same design only, and the radius does not grow with the hold.

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
needs a gesture nobody has proposed and it is not in the MVP.

## 5. The camera

The camera always looks at a **focus point on the plane** and never rolls. Pan moves the focus point,
which is clamped to the play area plus a margin. Orbit turns the camera around it. Zoom changes the
distance.

**Pitch is coupled to zoom and is not separately controllable.** Fully zoomed out the camera is near
top-down; fully zoomed in it rakes low across the plane. This is not a compromise made reluctantly — it
removes a whole degree of freedom from a gesture budget that has very little left, and it gives an RTS
exactly what it wants at each end: a tactical read when out, a fleet in silhouette when in. What it costs
is the deliberate low-angle overview shot, which is a screenshot rather than a control.

**Rotation has a deadzone**: the camera ignores a manipulation's rotation until it exceeds about eight
degrees, because two fingers dragging to pan are never exactly parallel and a camera that yaws whenever
you pan is unusable.

**There is no minimap.** Because pitch is coupled to zoom, **maximum zoom-out is already a top-down
tactical view of the whole map** — a minimap would be a second, smaller, lower-fidelity copy of a view
that is one gesture away, costing a second render of every entity, a second coordinate space and a second
hit test, for a quarter of the frame's height. With a symmetric map and no fog of war there is nothing on
it a player does not already know. The camera is the only way to move the camera.

---

## 6. The panels

Four things are drawn over the scene. All of them are `GameClient` (R20), and all draw in the interface
pass at physical resolution (§1).

| | Where | What |
|---|---|---|
| **Credits** | Top left | The number, and the income rate once there is one. |
| **Selection** | Bottom left, thumb zone | What is selected, grouped by design with a count and a hull bar. Tapping a group narrows the selection to it; a **clear** target deselects everything, which is the only way to do it (§4). |
| **Build** | Bottom right, thumb zone | Visible when your station is selected. Two targets — Miner and Fighter — each with its cost, greyed when unaffordable. Below them **the item currently building and its progress**, tappable to cancel. |
| **System** | Top centre, small | Connection state, the **reconnecting** overlay after a resume (§7), the **result overlay** when a match ends, and the one button that quits via `CoreApplication::Exit` — there is no Alt+F4 and no title bar. |

Every target in every panel is at least 48 × 48 (§1), and **the build buttons are 96 × 96** — 18 mm,
twice the minimum — because they are the ones a player hits while something is exploding.

**Nothing here is a Windows Runtime control.** There is no XAML anywhere in this tree (R18), so every panel
is geometry and text the renderer draws, and a "button" is a rectangle the hit test knows about. That is a
real cost — no free text layout, no free scrolling, no accessibility — and it is what R18 buys elsewhere.

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

## 7. Suspend, resume, and what is still open

**A packaged application is suspended when it loses the foreground, and the match runs on without it.**
On resume the client reconnects and shows a **reconnecting** overlay until the first snapshot lands, then
returns straight to play. That is mechanically free: snapshots are self-contained
([`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md)), so there is nothing to catch up on and no
resynchronisation to get wrong.

**The player's fleet was at risk the whole time they were away**, and nothing mitigates that. It is the
honest consequence of a match that does not pause, and it is the same behaviour a disconnected player gets
(`GameDesign.md` §2).

### What this document does not settle

**`Holding` over empty space means nothing, and that is a decision rather than an omission.** It is the
one gesture left over after [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md). R21 hands out
three verbs and this design has already refused a feature for want of one — **order queueing has no
gesture and is out of the MVP because of it** (§4). An idle affordance costs nothing; a gesture spent on
something marginal is not there when something real needs it. A map ping, a map-wide select-by-design and
a jump-to-station were each considered and each declined.

### When a match ends

**The host reseeds and starts another; the client shows a result overlay and reconnects into it.** Nothing
previously said what happened at victory — whether the host exited, reset or simply stopped — and for a
solo-against-AI testing loop **restart is the single most-used operation in the project**. `GameDesign.md`
§10 makes twenty matches in an evening the point of the reduced MVP; that is not possible if playing again
means relaunching a packaged application.

### What is left to a hand and a screen

None of these is a question. All are **confirmations owed at M1**, and all are settled by using the thing
rather than by arguing about it:

1. **Whether 192 pixels is the right circle** (§4) — and specifically whether the raking-camera case
   selects the wrong ships, since a circle on screen is a wedge in the world
   ([`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md)).
2. **Whether the interface pass costs more GPU time than the world pass**, which is likely: five instanced
   draws of simple geometry against an unbatched quad per glyph.

**Anything a second player needs to say to a first is out of the MVP deliberately.** There is no chat, no
ping and no map drawing; solo against AI is the only configuration the MVP can test, and the gesture a
ping would have used is the one being held in reserve above.

**How a client finds a host is settled and is not here:** it is a configuration value with a compiled-in
default, there is no discovery and no address entry, and the consequences are
[`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md).
