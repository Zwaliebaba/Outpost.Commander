# Interface — what the commander sees and touches

The interface of the MVP described in [`GameDesign.md`](GameDesign.md), inside `AGENTS.md` R13, R18 and
R21. **Touch is the only input and there is no fallback**, which is the constraint every decision here
answers to.

**Status: DRAFT.** §7 lists what this document finds missing and does not settle.

---

## 1. The frame

The game is authored at **1920 × 1080** and every layout, glyph and position behind that is
unconditional (R13). The frame is drawn into a scene target at that size and fitted into the window at
the end: 1:1 and unfiltered when the client area already matches, point sampling at an exact integer
multiple, bilinear otherwise, letterboxed, aspect preserved. Exactly one piece of code asks the window
how big it is.

**1920 × 1080 is 16:9 and a Surface-class tablet is 3:2**, so on the most likely device the frame
letterboxes top and bottom and loses about fifteen per cent of the panel. The alternative — authoring
3:2 — wins that back and letterboxes on every 16:9 display instead, including the monitor a developer
actually works on. The choice is on the register in [`OpenQuestions.md`](OpenQuestions.md); 16:9 is the
recommendation because it is 1:1 on the common case and because R13's scaling makes the other case
correct rather than broken.

### The touch target, derived

A 12.3-inch 3:2 display is 10.2 inches wide. A 16:9 frame fitted to that width puts 1920 authored pixels
across 10.2 inches — **188 pixels per inch, 7.4 per millimetre**. Microsoft's own touch guidance puts the
minimum target at 7 mm and the recommended one at 9 mm, which is **52 and 67 authored pixels**.

**The minimum interactive target is 72 × 72 authored pixels**, with at least 16 pixels of clear space
between adjacent targets. Rounded up from 67 rather than down, because the derivation assumes the largest
plausible screen and a smaller one makes every number worse. A control smaller than this is a defect, not
a style choice.

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
| **Minimap** | Top right, 320 × 320 | The square map, ships as owner-coloured dots, the camera's view as an outline. Tap to jump. |
| **Selection** | Bottom left, thumb zone | What is selected, grouped by design with a count and a hull bar. Tapping a group narrows the selection to it. |
| **Build** | Bottom right, thumb zone | Visible when your station is selected. Three targets — Miner, Fighter, Battleship — each with its cost, greyed when unaffordable. Below them the queue, each item tappable to cancel. |
| **System** | Top centre, small | Connection state, and the one button that quits via `CoreApplication::Exit` — there is no Alt+F4 and no title bar. |

Every target in every panel is at least 72 × 72 (§1). The build buttons are considerably larger, because
they are the ones a player hits while something is exploding.

**Nothing here is a Windows Runtime control.** There is no XAML anywhere in this tree (R18), so every panel
is geometry and text the renderer draws, and a "button" is a rectangle the hit test knows about. That is a
real cost — no free text layout, no free scrolling, no accessibility — and it is what R18 buys elsewhere.

---

## 7. What this document does not settle

1. **How a client finds a host.** There is no keyboard, so nobody can type an address, and the MVP has no
   lobby. The proposal is **LAN discovery**: the client multicasts a probe, hosts answer with a name and a
   player count, and the client lists them as tappable rows. `DatagramSocket` supports this and
   `privateNetworkClientServer` in the manifest permits it. The fallback, if discovery proves unreliable,
   is a numeric keypad the game draws itself. **This is a real hole in the brief and it is on the
   register**, because a client that cannot reach a host is not a client.
2. **The authored aspect ratio** (§1).
3. **Whether hold-then-drag survives contact with a hand** (§3).
4. **What suspend and resume look like.** A packaged application is suspended when it loses the
   foreground, and the match continues without it. Resuming into a match that has moved on needs a
   reconnect and a resynchronisation, and full snapshots (`TechnicalDesign.md` §4) make that mechanically
   trivial — but what the player *sees* while it happens is not designed.
5. **Fonts and glyphs.** There is no text renderer in this tree and no font in R14's dependency list. A
   bitmap font generated in code, or one baked into a header, is the likely answer and it is not designed.
6. **Anything a second player needs to say to a first.** There is no chat, no ping and no drawing on the
   map, and with no keyboard the first two need a gesture and a vocabulary nobody has proposed.
