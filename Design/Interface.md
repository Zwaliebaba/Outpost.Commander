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
| **One-finger drag** | Pans the camera. This is the most frequent thing a player does, so it gets the most comfortable gesture. |
| **One-finger hold, then drag** | Band select. The hold arms it, the rubber band follows the finger, release commits. |
| **Two-finger pinch** | Zooms — and with it, pitches. |
| **Two-finger rotate** | Orbits. |
| **Two-finger drag** | Pans, identically to one finger. It comes free from the same manipulation and refusing it would be a surprise. |

**Hold-then-drag is how band select coexists with panning**, and it is the standard answer to a drag that
must mean two things. It costs about 300 milliseconds of latency before a band starts and it is not
self-evident to a new player; the alternatives were putting panning on two fingers, which taxes the most
frequent action, or a selection-mode toggle, which adds a mode and a button. It is on the register as the
first thing to re-examine once anyone has actually used it.

**There is no context menu and there is no long-press on a ship.** A ship is tapped to select it and the
selection panel shows everything a menu would have; `Holding` over the playfield always means band select,
wherever it starts.

---

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

A tap on empty space with **nothing** selected does nothing. **Deselecting is a band select that encloses
nothing** — the gesture a player already has, rather than a button that exists for one purpose.

A tap and a drag are separated by the recogniser's own movement threshold: `Tapped` does not fire if the
pointer travelled, so a slightly sloppy tap pans a few pixels instead of issuing an order. That is the
right failure — an accidental pan costs nothing and an accidental move order costs a fleet.

Orders replace; there is no queueing and no shift-equivalent, because there is no shift. Order queueing
needs a gesture nobody has proposed yet and it is not in the MVP.

---

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
| **Selection** | Bottom left, thumb zone | What is selected, grouped by design with a count and a hull bar. Tapping a group narrows the selection to it. |
| **Build** | Bottom right, thumb zone | Visible when your station is selected. Three targets — Miner, Fighter, Battleship — each with its cost, greyed when unaffordable. Below them the queue, each item tappable to cancel. |
| **System** | Top centre, small | Connection state, and the one button that quits via `CoreApplication::Exit` — there is no Alt+F4 and no title bar. |

Every target in every panel is at least 48 × 48 (§1), and **the build buttons are 96 × 96** — 18 mm,
twice the minimum — because they are the ones a player hits while something is exploding.

**Nothing here is a Windows Runtime control.** There is no XAML anywhere in this tree (R18), so every panel
is geometry and text the renderer draws, and a "button" is a rectangle the hit test knows about. That is a
real cost — no free text layout, no free scrolling, no accessibility — and it is what R18 buys elsewhere.

---

## 7. What this document does not settle

1. **Whether hold-then-drag survives a real hand** (§3). Answered by using it, not by arguing about it.
2. **What suspend and resume look like.** A packaged application is suspended when it loses the
   foreground, and the match continues without it. Reconnecting is mechanically trivial because snapshots
   are self-contained (`TechnicalDesign.md` §4) — but what the player *sees* while it happens is not
   designed.
3. **Fonts and glyphs.** There is no text renderer in this tree and no font in R14's dependency list. A
   bitmap font baked into a header and drawn as instanced quads is the likely answer: no dependency, no
   file, and exact at the authored size, which is the whole point of §1's 2× fit.
4. **Anything a second player needs to say to a first.** There is no chat, no ping and no drawing on the
   map, and with no keyboard the first two need a gesture and a vocabulary nobody has proposed.

**How a client finds a host is settled and is not here:** it is a configuration value with a compiled-in
default, there is no discovery and no address entry, and the consequences are
[`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md).
