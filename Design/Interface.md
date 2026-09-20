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

**The game is authored at 1440 × 960** and every layout, glyph and position behind that is unconditional
(R13). The frame is drawn into a scene target at that size and fitted into the back buffer at the end.

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
black — exactly the content that wants it (`TechnicalDesign.md` §6).

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

The minimap is the only other way to move the camera: a tap on it jumps the focus point. **No orders can be
given through the minimap** in the MVP — a 320-pixel map of a 16,384-unit square puts fifty units under a
fingertip, and an order placed that imprecisely is an order given by accident.

---

## 6. The panels

Five things are drawn over the scene. All of them are `GameClient` (R20).

| | Where | What |
|---|---|---|
| **Credits** | Top left | The number, and the income rate once there is one. |
| **Minimap** | Top right, 240 × 240 | The square map, ships as owner-coloured dots, the camera's view as an outline. Tap to jump. |
| **Selection** | Bottom left, thumb zone | What is selected, grouped by design with a count and a hull bar. Tapping a group narrows the selection to it; a **clear** target deselects everything, which is the only way to do it (§4). |
| **Build** | Bottom right, thumb zone | Visible when your station is selected. Three targets — Miner, Fighter, Battleship — each with its cost, greyed when unaffordable. Below them the queue, each item tappable to cancel. |
| **System** | Top centre, small | Connection state, the **reconnecting** overlay after a resume (§7), and the one button that quits via `CoreApplication::Exit` — there is no Alt+F4 and no title bar. |

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

**Two sizes**: a body size for readouts and a larger one for the build buttons. Text is drawn into the
scene target like everything else, so on the target device it reaches the glass pixel-doubled at the exact
2× fit. At 267 PPI a doubled pixel is a 133-PPI effective pixel — ordinary desktop density — and this is
large type with a dozen strings rather than the dense small type R13 warns about, so the cost is small.
It is still a cost, and what it actually looks like is a measurement owed at M1.

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

### What is left to a hand and a screen

Neither of these is a question. Both are **confirmations owed at M1**, and both are settled by using the
thing rather than by arguing about it:

1. **Whether 192 pixels is the right circle** (§4).
2. **Whether pixel-doubled text reads acceptably** (§6). If it does not, the lever is the authored
   resolution — [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) — and not the text path.

**Anything a second player needs to say to a first is out of the MVP deliberately.** There is no chat, no
ping and no map drawing; solo against AI is the only configuration the MVP can test, and the gesture a
ping would have used is the one being held in reserve above.

**How a client finds a host is settled and is not here:** it is a configuration value with a compiled-in
default, there is no discovery and no address entry, and the consequences are
[`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md).
