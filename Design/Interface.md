# Interface — what the operator sees and touches in M1

The M1 interface, as fixed panels at the authored resolution. It exists because the vertical slice cannot run without a specification of the screen: `m1-vertical-slice/K3` builds the toolkit from it, `K4` builds these panels from it, `G1` wires the orders, and neither is meant to invent anything. `Design/README.md` said this document is written before M1 starts; this is it.

**Status: DESIGN.** Written by `m1-vertical-slice/D1` on 2026-09-17. The owner's merge of its pull request is its acceptance, and §12 lists the rulings it takes that a reader may want to overturn before merging.

**Status: rewritten for touch on 2026-09-20** by `t1-touch-interface/T1`, under [`ADR-021`](ADR/ADR-021-touch-is-the-only-input.md), which makes **touch the only input the game takes**. §4 and §6 are new; §13 is new and carries the measurement the rest of that plan waits on. §12 ruling 3 is marked superseded and left unedited, because a superseded ruling is evidence and a deleted one is not. Everything else — the frame, the palette, the panels, the minimap, the readouts, the overlays — stands, redrawn at §13's target size rather than reinvented.

**Two things `T1` found that `ADR-021` had wrong, both in the tree's favour.** Touch *does* have a second button: `GestureSettings::RightTap` is documented as "Touch: press and hold", so §6's default-and-alternate order pair survives rather than being redesigned. And raising the authored resolution would **not** have bought room for fingers — §13 shows the required target is a fixed fraction of the frame, so [`ADR-004`](ADR/ADR-004-renderer-foundation.md) is not reopened and never could have been the answer.

**M2 restyles this, it does not replace it.** The Eclipse-shaped windows of `SpeciesCanvas.md` are M2's, and they carry the same content in movable windows. Everything here is written so that a panel's *content* survives that change and only its frame is redrawn.

---

## 1. The rules this document works under

- **Everything is authored at 1920×1080 and unconditional** (ADR-004). Every rectangle, margin and glyph in this document is in authored pixels. Exactly one place reads the window's size, `FitAuthored`, and the present pass scales the finished frame.
- **Flat rectangles, one-pixel borders, a pixel font, monochrome icons with one accent colour** (`GameDesign.md` §11.4). No gradient fills except the two the chrome palette names, no drop shadows on panels, no animation except the two this document allows.
- **Readable at a glance outranks the Species look** (`GameDesign.md` pillar 3). Where `SpeciesCanvas.md` and pillar 3 disagree, pillar 3 wins, and §12 records each time it did.
- **The interface reads the replica and emits orders. It never reads the simulation.** Every panel below names what it reads from the render view or the replica and what orders it emits. A panel that needs a number nobody replicates is a defect in this document or in `N1`, not something a panel computes from the simulation.
- **The interface is the router's first sink** (`NeuronClient/InputRouter.h`). A widget that acts on an event consumes it; a panel under the contact consumes taps so they never reach selection. **The one exception is a two-finger gesture, which drives the camera from anywhere** (§4), and it is an exception this document states rather than leaving to the sink.

---

## 2. The frame

```
 0                                                                          1920
 0 ┌────────────────────────┬──────────────────────────────┬────────────────┐
   │ POWER READOUT          │                              │ MATCH STATE    │ 40
   ├────────────────────────┴──────────────────────────────┴────────────────┤
   │                                                                        │
   │                                                                        │
   │                         THE WORLD VIEW                                 │
   │                    (the scene, drawn full frame,                       │
   │                     the panels sitting over it)                        │
   │                                                                        │
   │                                                                        │
792├───────────┬───────────────────┬─────────────────────────┬──────────────┤
   │           │                   │                         │              │
   │  MINIMAP  │    SELECTION      │        COMMAND          │    ORDERS    │
   │           │                   │   (five tabs, §7)       │              │
   │           │                   │                         │              │
1080└───────────┴───────────────────┴─────────────────────────┴──────────────┘
    0         288                 800                      1568           1920
```

| Panel | Rectangle (x, y, width, height) | Always present |
|---|---|---|
| Power readout | 0, 0, 400, 40 | Yes |
| Match state | 1520, 0, 400, 40 | Yes |
| Minimap | 0, 792, 288, 288 | Yes |
| Selection | 288, 792, 512, 288 | Yes |
| Command | 800, 792, 768, 288 | Yes |
| Orders | 1568, 792, 352, 288 | Yes |
| Match-end overlay | 560, 380, 800, 320 | On the match ending (§10) |
| Quit menu | 720, 400, 480, 320 | On the peel's last rung (§4, §10) |

**The world view is the whole frame.** The scene is drawn at 1920×1080 and the panels are drawn over it, so the camera never has a smaller viewport than the screen and a unit is never hidden behind a panel without the player being able to scroll to it. The consequence is that the bottom 288 rows and the two 40-row corners are obscured; picking refuses a ray whose origin is inside a panel rectangle (§5), so nothing is ever selected through a panel.

**Nothing moves.** No panel slides, collapses, or animates its size. The two animations this document allows are the tooltip's typewriter (§4) and the construction and research progress bars, which advance with their value and nothing else.

---

## 3. The chrome

One palette table, `GameData\Interface.json`, loaded by `GameShared` alongside the other tables, so that the look of a panel is data (`SpeciesCanvas.md` §7). Every colour below is that file's starting value, in 8-bit RGBA. The same file carries the eight commander colours of `GameDesign.md` §11, which are not chrome but are read by the same panels and the same minimap, and one file of eight rows is cheaper than a second loader (`C6`).

| Role | Colour | Where |
|---|---|---|
| `panelFill` | (24, 10, 12, 245) | Every panel's body |
| `panelBorder` | (199, 214, 220, 255) | The one-pixel border of every panel |
| `panelTitleFrom` | (199, 214, 220, 255) | The title strip's gradient, top |
| `panelTitleTo` | (112, 141, 168, 255) | The title strip's gradient, bottom |
| `titleText` | (255, 255, 150, 255) | A panel's title, uppercased |
| `bodyText` | (222, 226, 230, 255) | Labels and values |
| `dimText` | (128, 132, 136, 255) | A value that is unavailable or zero |
| `accent` | (255, 196, 64, 255) | The one accent: selection, the active tab, a ready item |
| `warning` | (232, 96, 72, 255) | A rejection, a losing state, damage under a quarter |
| `buttonFill` | (107, 37, 39, 255) | A button at rest |
| `buttonFillHover` | (140, 52, 54, 255) | **Unused under touch**: there is no hover. Kept in the table and in `Interface.json` because M2's windows may want it and a palette row costs nothing; a widget that draws it is a defect |
| `buttonFillDown` | (199, 214, 220, 255) | Held |
| `buttonDisabled` | (60, 32, 34, 255) | Unavailable; its caption takes `dimText` |
| `barEmpty` | (40, 20, 22, 255) | A progress or health bar's track |
| `barBuild` | (120, 180, 220, 255) | Construction and production progress |
| `barHealth` | (96, 200, 108, 255) | Health at or over half |
| `barHealthLow` | (232, 96, 72, 255) | Health under a quarter |

**A panel** is a filled rectangle in `panelFill`, a one-pixel `panelBorder` on all four edges, and a 20-pixel title strip along the top filled with the vertical gradient `panelTitleFrom` → `panelTitleTo`, holding the panel's name in `titleText`, uppercased, at x + 8. The client area is inset 8 pixels left, right and bottom, and starts 24 pixels from the top.

**A button** is a filled rectangle with a one-pixel `panelBorder`, **64 authored pixels high and never less than 64 wide** (§13), its caption in `bodyText` centred vertically and inset 8 pixels from the left, or centred when the button is square. Its fill is `buttonFill`, `buttonFillDown` or `buttonDisabled`; a down button's caption takes `panelFill` so that it stays readable on the light fill. There is no hover fill, because there is no hover.

**NO INTERACTIVE ELEMENT IN THIS DOCUMENT IS SMALLER THAN 64 AUTHORED PIXELS IN EITHER DIMENSION, AND THERE IS NO EXCEPTION.** §13 says where the number comes from and what it costs. The cost is real and is paid in counts rather than in sizes: where a panel held sixteen things at the mouse's scale it holds six at the finger's, and the answer is fewer things on screen with the rest behind a scroll — never a smaller thing. A floor with one exception is a floor a layout will find its way under, and `t1-touch-interface/T4` makes it a test that walks every widget the HUD places rather than a sentence a reviewer has to remember.

**A bar** is a filled `barEmpty` rectangle with a one-pixel `panelBorder`, 12 pixels high, filled from the left in its value's colour. A bar never animates toward its value; it is drawn at the value the frame has.

**An icon** is 32×32 authored pixels from `GameData\Textures\Icons.dds`, drawn as one quad in `bodyText` where it is available and `dimText` where it is not, on the panel fill. Monochrome with one accent (`GameDesign.md` §11.4): an icon is a single-channel mask tinted at draw time, never a coloured bitmap, so that the accent is the palette's and a mod recolours nothing by accident.

### Text

**Text is monospaced, 16 authored pixels a glyph box, with a 16-pixel advance and a 20-pixel line height.** The Spectrum atlas is 16 columns by 14 rows of 16×16 cells from ASCII 32 (`SpeciesCanvas.md` §4), and a glyph drawn at 16×16 is one texel to one pixel. A line of *n* characters is exactly 16*n* pixels wide, which is the arithmetic every layout in this document uses.

This is a ruling against `SpeciesCanvas.md` §7, which asks for sizes 12 and 13 drawn 1:1. At 1920×1080 with a 16-pixel cell, 12 and 13 are not 1:1 with anything: they resample every glyph, which is the single thing pillar 3 and ADR-004's authored resolution exist to prevent. Species drew at 12 and 13 because `GetMenuSize` scaled everything to the window; this game does not scale, so it can afford the atlas's own size. §12 records it as overturnable.

**There is no second size and no second face.** Species's title and caption faces become one; a title is distinguished by its colour and its gradient strip, not by its font.

---

## 4. The finger, the gestures and the cursor

**Touch is the only input** ([`ADR-021`](ADR/ADR-021-touch-is-the-only-input.md)). There is no keyboard, no mouse, no pen and no wheel; a `PointerPoint` whose `PointerDeviceType` is not `Touch` is dropped before the recogniser sees it (`t1-touch-interface/T5`). Every interaction below is an event `Windows::UI::Input::GestureRecognizer` actually raises, and a gesture the recogniser does not raise is not in this document.

### Coordinates

A contact's position arrives in **device-independent pixels** and reaches the frame in **client pixels**; every rectangle in this document is in **authored pixels**. `NeuronClient/ScaleMode.h` maps authored to client through `FitAuthored`, and `AuthoredFromClient(const ScaledRectangle&, clientX, clientY)` is its inverse, returning the authored position and whether the contact landed inside the letterbox at all. It is the one place the conversion happens and `K3` wrote it with the round-trip test.

A contact in the letterbox bars is over no panel and over no world, and every hit test refuses it.

### The rule

**ONE FINGER IS THE GAME AND TWO FINGERS ARE THE CAMERA.** That is the whole of it, and everything below is a consequence. It is worth one sentence because it is the only thing a player has to be told: one finger selects, orders and presses buttons, exactly where the left button used to; two fingers move, turn and raise the camera, which is what the mouse did with no button held under the aim mode `ADR-021` deleted.

The two are distinguishable at the seam and not by guesswork. `ManipulationStartedEventArgs` and `ManipulationUpdatedEventArgs` both carry `ContactCount`, and `TappedEventArgs` carries `ContactCount` and `TapCount`, so one recogniser serves both hands and the branch is a field read rather than a heuristic over timings.

### The gestures

| Gesture | Event and how it is told apart | On the world | On a panel |
|---|---|---|---|
| **Tap** | `Tapped`, `ContactCount` 1, `TapCount` 1 | Select, or give the selection its order (§6) | The widget under it acts and consumes it |
| **Double tap** | `Tapped`, `ContactCount` 1, `TapCount` 2 | Centre the camera on the point | Nothing; the panel consumes it |
| **Press and hold** | `Holding` while held, then `RightTapped` on lift | The *alternate* order (§6) | Shows the widget's tooltip while held |
| **Two-finger tap** | `Tapped`, `ContactCount` 2 | **Peel** (below) | Peel |
| **Drag** | `Manipulation*`, `ContactCount` 1, `Delta::Translation` | Draws the selection rectangle (§5) | Nothing; M1 has no draggable widget |
| **Two-finger drag** | `Manipulation*`, `ContactCount` ≥ 2, `Delta::Translation` | **Pans the camera** | **Pans the camera** — it is reachable from anywhere |
| **Pinch** | `Manipulation*`, `Delta::Scale` | **Raises and lowers the camera** | Same |
| **Twist** | `Manipulation*`, `Delta::Rotation` | **Turns the camera's yaw** | Same |

**The camera is reachable over a panel on purpose.** A panel occupies the bottom 288 rows and two corners (§2); a player whose two fingers land partly on one and expects the camera to move is right, and a player who wanted a button would have used one finger. This is the one place a panel does not consume an event, and it is stated here rather than left to `UiInputSink` to decide.

**Peel replaces Escape.** `NeuronClient/PointerMode.h`'s `EscapePeel` cancelled the innermost thing one press at a time; a two-finger tap now does it, in the same order and one tap at a time: **an armed order first, then a modal panel, and then the quit menu** (§10). The third rung of the old peel swapped the pointer's mode; `ADR-021` deleted the modes and the exit moves into the rung they vacated, which is §12 ruling 12. Two fingers were chosen because the gesture cannot be produced by accident while aiming one finger at a button, and because it needs no target: a peel that had to be aimed would be the thing a player reaches for when the aim is what went wrong.

**The failure mode to watch for is a staggered two-finger tap.** `TapMaxContactCount` of 2 says what a two-contact tap *is*; how much stagger between the two fingers landing the recogniser tolerates before it calls them two separate one-contact taps is not documented, and this document was written without a device to try it on. **The consequence of getting it wrong is not cosmetic**: two one-finger taps on the world with a selection standing are two `Move` orders (§6), so a peel that failed to register would send the army to wherever the player happened to be cancelling. `t1-touch-interface/T2` is where this is found out, on the machine that first has a recogniser, and the fallbacks in order of preference are `HoldMinContactCount` with a two-finger *hold* instead, or a permanent 64-pixel `BACK` button that costs the orders panel one more target. **`T2` is not done until a staggered pair has been tried deliberately**, which is a line worth having here because a test that taps two contacts in the same instant will never see it.

### The camera, which is what replaces aim mode

`NeuronClient/Camera.h` is a fly camera — yaw and pitch free, height the commander's, `CAMERA_MIN_CLEARANCE` of 10 above the highest ground within one spacing and `CAMERA_MAX_HEIGHT` of 5,000. Aim mode drove its yaw and pitch from raw mouse counts at `AIM_RADIANS_PER_COUNT`, and `ADR-021` deletes all of it. Two fingers drive it now, and one `ManipulationDelta` carries everything needed:

| `ManipulationDelta` field | Unit the platform gives | What it drives |
|---|---|---|
| `Translation` | device-independent pixels, x and y | **Pan.** The camera translates over the ground plane |
| `Scale` | a ratio: 2.0 is twice the contact separation | **Height.** Stretch descends, pinch rises |
| `Rotation` | degrees | **Yaw**, about the ground point under the contacts |
| `Expansion` | device-independent pixels | Unused; `Scale` says the same thing in the unit a camera wants |

**ONE RULE FIXES EVERY SIGN: THE WORLD FOLLOWS THE FINGERS.** Whatever is under the contacts when the manipulation starts stays under them until it ends — drag right and the landscape goes right, which means the camera goes *left*; stretch and the ground grows, which means the camera comes *down*; twist clockwise and the landscape turns clockwise, which means the yaw turns the other way. Every one of those is a negation somewhere in the arithmetic, every one compiles equally well inverted, and **no test can tell inverted from correct** — which is the argument `NeuronClient/PointerMode.h` already wrote down about an aim delta and the reason `t1-touch-interface/T3` pins all three against a worked example rather than trusting them.

**Pitch is not driven by a gesture, and is bound to height instead.** Two fingers have translate, scale and rotate and that is three degrees of freedom for a camera that wants four. Rather than spend a fourth gesture on the one the player will ask for least, **the camera pitches down as it rises** — low and level near the ground, steep and overhead at `CAMERA_MAX_HEIGHT` — on one monotonic curve `T3` writes and the owner tunes by flying it. Species's own play camera has no pitch control either (`SpeciesLook.md` §7). The alternative, a pair of buttons, is recorded in §12 ruling 13 for a reader who wants the freedom back.

**There is no edge-scrolling and no arrow keys**, which were the mouse interface's two other ways to move the camera. A finger is always inside the frame and there is no key to hold; the pan is the whole of it.

### What the recogniser is set to

One `GestureRecognizer`, whose `GestureSettings` is exactly:

```
Tap | DoubleTap | Hold | RightTap
  | ManipulationTranslateX | ManipulationTranslateY
  | ManipulationScale | ManipulationRotate
```

with `TapMinContactCount` 1, `TapMaxContactCount` 2, and `TranslationMinContactCount` 1.

**What is deliberately not set, each for a reason.** `HoldWithMouse` and `Drag` are mouse and pen features and there is no mouse. `CrossSlide` is a list interaction and this document has no list that rearranges. `ManipulationTranslateRailsX` and `RailsY` guide a pan onto one axis, which is wrong for a camera over a landscape. `ManipulationMultipleFingerPanning` disables zoom whenever two or more contacts are down, and two contacts are exactly when this game wants zoom. **The three inertia flags are not set**: a camera that keeps gliding after the fingers leave is a camera that is somewhere else when the player looks back, and an order given to units the player cannot see is the failure mode `GameDesign.md` pillar 3 exists to prevent. §12 ruling 11 records it as overturnable, because it is a feel decision and not an engineering one.

**The settings mask is named in one place**, `OutpostCommander/FrameworkView.cpp`, and copied into `ADR-021`'s Measurements, because a `GestureRecognizer` cannot be constructed outside a package and therefore no suite can reach it (`AGENTS.md` R20, `p1-uwp-shell/P8`).

### The cursor

**There is no cursor.** `CoreWindow::PointerCursor` is `nullptr` for the life of the application, and the six cursor bitmaps of the mouse interface are gone: a finger is its own cursor, it is already on the glass, and a 32×32 quad drawn under it would be under the finger that hides it.

What the six cursors *said* still has to be said, so it moves to where the finger is not:

| What the old cursor meant | Where it is now |
|---|---|
| Select — over an own object | The object's own `accent` outline, which §5 already draws |
| Move, Attack — the default order under the pointer | The **hold ring**, below |
| Build — placing a structure | The footprint ghost, which §7.1 already draws and which is bigger than a finger |
| Refuse — illegal placement or an order the selection cannot take | The ghost turns `warning`, and a refused order draws §6's line |

**The hold ring is the one piece of the aim cursor that survives, and it survives because it earns its place.** On `Holding`, a ring is drawn **on the ground at the contact**, tinted for the order the lift will issue — which is what makes press-and-hold safe to use for the destructive half of §6: the player sees what it will do before letting go, and sliding off before the lift cancels it. It is `SpeciesLook.md` §7.1's `MouseHighlight` disc and `NeuronClient/GroundRay.h` already answers where it sits and which way it tilts (`K7`); every rule in the table below is carried over unchanged from the aim cursor it replaces.

| Rule | Value | Why |
|---|---|---|
| Orientation | `up` = the landscape's interpolated normal at the hit; `front` = `normalize(up × worldUp)`, falling back to world `+Z` when that cross product is near zero; `right` = `normalize(front × up)` | The fallback is not decoration: a normal map writes exactly `(0, 1, 0)` for flat ground, `XMVector3Normalize` of a zero vector is zero, and the ring would collapse to a point on the flat ground this game is mostly made of |
| Size | 30 world units × `sqrt(distance to the camera) / 40`, and **never smaller on screen than §13's touch target** | Species's own scaling keeps it roughly constant on screen; the floor is new, because a ring the finger covers tells the player nothing |
| Over water | the hit's height raised to at least 1 world unit | The terrain mesh continues under the sea, so the ray hits the seabed |
| Off the landscape | hidden, and the last position is not reused | |
| Depth | tested, not written; no culling; a slope-scaled bias toward the camera | It is coplanar with the ground it sits on |
| Blend | a blurred copy first (`SRC_ALPHA`, `INV_SRC_COLOR`), then the sharp one additively (`SRC_ALPHA`, `ONE`) in the tint | The dark pass is what makes a bright ring readable over bright sand |
| Pulse | `size × (1 + \|sin(4t)\| × 0.6)` while placing a structure | A placement is the one thing in M1 that wants the eye |

### Tooltips

A tooltip is shown by **press and hold on a widget** rather than by a dwell, because there is no hover to dwell in: `Holding` shows it and the lift hides it. It is a panel-framed box holding one or two lines of `bodyText`, placed **above** the contact and never below it, offset by §13's touch target so that it clears the finger, and flipped to the other side when it would leave the frame. The typewriter reveal of `SpeciesCanvas.md` §5 is kept at 50 characters a second.

This is what the disabled `ReturnToRepair` button and the two disabled retreat stances of §9.1 have always needed and never had (§11, row 20): a finger can now ask a dead button why it is dead.

---

## 5. Selection

**Where the ray comes from.** Through the contact, and through nothing else. There is no second origin and no screen centre to fall back on: a finger is always somewhere, which is the one thing the aim mode `ADR-021` deleted could not say. Every rule below is about the ray and not about the device that cast it, so this section survives the migration nearly whole.

**What can be selected.** Own devices and own structures, one at a time by tap, or several devices by rectangle. A rectangle never selects structures and never selects another commander's anything. Tapping a visible enemy device or structure selects it as an *inspection*: the selection panel shows it (§8) and the orders panel is empty, because nothing it can be ordered to do exists.

**How it is picked.** `R2`'s picking functions, as pure functions over the replica and the camera matrices: the nearest object under a screen ray, and the objects inside a screen rectangle. A ray whose origin lands inside any panel rectangle of §2 is refused before it is cast.

**What it looks like.** A selected object carries a one-pixel `accent` outline in the world, drawn by the geometry pass from the render view's selection flag; there is no selection circle on the ground and no bracket. A one-finger drag draws a one-pixel `accent` rectangle over the world from the contact's down point to where it is now, for as long as the finger is down.

**Clearing the selection is a button and not a gesture** — `CLEAR` in the orders panel (§9.1). Under the mouse it was a left click on empty ground; under §6 a tap on empty ground with a selection standing is a `Move`, which is the far commoner thing to want, and a gesture spent on deselection is a gesture wasted.

**Groups lost their keys and keep their meaning.** Ten numbered groups were `Ctrl` plus a digit to assign and a digit to select. They become ten `CLEAR`-width buttons in the orders panel (§9.1): a tap selects the group, a press and hold assigns the selection to it. It is the same press-and-hold-is-the-other-button rule §6 uses on the world, applied to a button, and it is why that rule is worth having twice.

**Groups.** Ten numbered groups, 0 to 9, held by the simulation through the `Group` order so that they survive a rejoin and appear in a replay. Assigning replaces the group. A group that has lost every member is empty and selecting it does nothing.

---

## 6. Orders on the world

**Touch has two buttons and this section is why that matters.** `GestureSettings::RightTap` is documented as "Touch: press and hold", raising `RightTapped` when the contact is lifted, so the mouse interface's *default order on the right button* survives as a gesture rather than being redesigned around its absence. `ADR-021` was written believing otherwise and is corrected.

**What a tap does is decided by what is under it, and by whether anything is selected.** This is the old default-order table with the button removed from it:

| Under the contact | Selection | A **tap** gives | A **press and hold** gives |
|---|---|---|---|
| Open ground | Devices | `Move` to that point | Nothing; the ring is drawn `dimText` |
| Open ground | Empty | Nothing | Nothing |
| A visible enemy device or structure | Devices with a weapon | `Attack` that object | Nothing |
| A visible enemy | Devices with no weapon | `Move` to the object's position | Nothing |
| A visible enemy | Empty | Select it as an inspection (§5) | Nothing |
| An **own** device or structure | Empty | Select it | Nothing |
| An **own** device or structure | Devices | **Select it**, replacing the selection | The order the object invites: `Move` adjacent to an own damaged structure or an own plan, and nothing otherwise |
| The minimap | Devices | Move the camera there (§9.2) | `Move` to the landscape point that pixel names |

**The one ambiguity is an own object with a selection standing, and it is resolved in favour of the tap.** Selecting is done many times a minute and repairing is done a few times a match, so the cheap gesture goes to the common act and the expensive one to the rare act. That is also the safer way round: a mis-tap changes what is selected and is undone by tapping again, while a mis-order sends an army somewhere.

**Press and hold is never the fast path, deliberately.** It costs `HoldStartDelay` before `Holding` fires and it does not commit until the finger lifts. §12 ruling 3's own argument against a drag threshold — "an order that arrives late is the one thing a real-time game cannot afford", against a `TechnicalDesign.md` §3 budget of 250 to 330 milliseconds from input to visible movement — applies to a dwell with more force, which is exactly why no order a player gives constantly is behind one. **What the delay buys back is a look before the leap**: the hold ring of §4 is drawn, tinted for the order the lift will issue, and sliding the finger off before lifting cancels it. A dwell is affordable precisely where it is rare, and it is safe precisely because it is slow.

**An armed order** is one of Move, Patrol, Attack-move or Build, armed by a button in the orders panel, which makes the *next* tap on the world issue that order instead of selecting. A two-finger tap disarms it (§4's peel). Patrol takes two taps, the second setting the far point. The hotkeys that used to arm an order — `M`, `P`, `R`, `B` — are gone with the keyboard, and the buttons that were their alternative were always there.

**Queueing is a button, not a modifier.** `Shift` held with an order appended it to the selection's order list; a finger holds no modifier. The orders panel gains a `QUEUE` toggle that latches: while it is lit, every order issued is appended rather than replacing, and it stays lit until it is tapped again. It is a latch rather than a held state because there is no second hand to hold it with. The queue is still the client's own and not the simulation's — the twenty kinds carry none (`GameShared/Order.h`) — so a rejoining client still does not recover it (§12 ruling 5, unchanged).

**Every order is acknowledged locally at once** — a one-frame `accent` mark at the target point and a sound — and the unit moves when the replica says it has. There is no client-side prediction (`TechnicalDesign.md` §3). **This matters more under a finger than it did under a mouse**, because the finger is covering the place the acknowledgement is drawn: the mark is drawn at §13's target size around the contact so that its edge clears the fingertip, and it is the one piece of feedback in this document sized by the hand rather than by the eye.

**A rejection is shown.** The simulation records a reject reason per seat (`S2`: `NotOwned`, `NotVisible`, `CannotAfford`, `AtCap`, `InvalidTarget`, `InvalidPlacement`, `NotResearched`, `NoCommandPost`, `Malformed`). It reaches the client in the frame, is drawn as one line of `warning` text centred at y 720 for two seconds, and replaces the line already there. `SeatState` carries it (`N4`, 2026-09-19) as three fixed-size fields — a wrapping `rejectSequence`, the `OrderKind` and the `RejectReason` — rather than the list the simulation keeps, because this section draws one line at a time. **The client redraws on the sequence and not on the value**: two identical refusals are equal field for field, so without it a commander who asks twice for what he cannot afford would watch the line sit there and read it as not having been heard.

---

## 7. The command panel

One panel, 768×288 at (800, 792), with five tabs along its title strip, selected by tap. The keys 1 to 5 that also selected them are gone with the keyboard. The active tab's button is filled `accent` with its caption in `panelFill`.

**The title strip is 20 pixels tall (§3) and a tab is a button, so a tab cannot live in it.** The five tabs become a row of 64-tall buttons along the panel's top, inside the client area, and the panel's content starts below them — which costs the command panel 64 of its 288 rows and is the single largest thing §13's target size takes from this document. `T4` owns the redraw.

### 7.1 Construct (tab 1)

A row of structure buttons, one per structure the seat has researched, each 128×64 holding a 32×32 icon, the name, and the cost in power. A button the seat cannot afford takes `buttonDisabled` and its cost takes `warning`. M1 has six structures: command post, extractor, generator, factory, research lab, tower.

Tapping one enters **placement**: a footprint ghost follows the contact on the terrain, a one-pixel outlined rectangle of the structure's footprint in cells, filled at a quarter alpha, `accent` where the placement is legal and `warning` where it is not. **The ghost is drawn where the finger is, and the finger covers it**, so placement is the one act in this document that commits on the *lift* rather than on the tap: the finger goes down, the ghost follows it and can be slid into place while it is visible around the fingertip, and the plan is placed when the finger comes up. Placement stays entered so that a wall or a row of extractors is placed without returning to the panel; a two-finger tap (§4's peel) leaves it.

That is `Holding` and `RightTapped` doing for a structure exactly what §6 has them do for an order, and it is the second reason press-and-hold earns its place: a footprint is bigger than a fingertip and still wants aiming.

**The ghost's legality is the client's guess and says so.** It is evaluated on the replica's copy of the landscape by the same rules as the simulation's placement — no structure, no feature, no water, slope under 25%, every cell explored, and a deposit under an extractor. The replica does not hold flatten deltas outside a structure's own record (`TechnicalDesign.md` §5.2), so ground flattened by a structure the commander cannot see can read as illegal when the host would allow it, or the reverse. The ghost is therefore advisory; the host decides, and an `InvalidPlacement` rejection is shown as §6 says. This is a known and accepted disagreement, not a defect to design around.

The panel also shows **the plan count**, *n* of 64, in `dimText`, and turns it `warning` at the cap.

### 7.2 Produce (tab 2)

Shown for a selected own factory; empty with a line of `dimText` otherwise.

- **The design list**, left half: every design the seat has saved, one row of **64** pixels each holding the name, the cost and the build time in seconds. Tapping one appends it to that factory's queue (`SetProduction`). The list scrolls by one-finger drag — which is the one place in this document a one-finger drag is not the selection rectangle, because it is inside a panel and §4 gives a panel its events.
- **The queue**, right half: up to eight entries at 64 pixels a row, of which three fit and the rest scroll, the first with a `barBuild` progress bar and the seconds remaining, the rest as names. **A press and hold removes an entry** (`CancelProduction`), not a tap: cancelling production is destructive and a stray fingertip should not do it.
- **The remaining time** is computed by the client as the ticks the replica reports remaining, divided by the tick rate. The host owns the arithmetic that turns builder rates into progress; the panel divides and prints.

A factory whose commander is at the device cap shows `AT THE DEVICE CAP` in `warning` where the progress bar would be, and the queue does not advance (`GameDesign.md` §4).

**There is no rally point in M1.** The twenty order kinds carry none (§11, gap for a later milestone), and a device appears beside its factory and holds.

### 7.3 Research (tab 3)

A scrolling list of the research items available to the seat, one row of **64** pixels each: the name, the cost, the time in seconds, and the effect in one line from the item's `description` field. A row whose prerequisites are unmet is not listed at all; a row the seat cannot afford takes `dimText` with its cost in `warning`. It scrolls by one-finger drag.

Tapping a row starts it in the first idle lab (`SetResearch`). A lab that is researching shows above the list as a row with a `barBuild` bar and the seconds remaining; **a press and hold on that row cancels it** (`CancelResearch`), which refunds nothing. The confirm tap the mouse interface required is dropped: the hold ring's own dwell is the confirmation, and two deliberate acts for one cancellation is one more than a player will forgive on a tablet.

**The auto-research toggle** sits in the panel's top right: a 64-pixel button that fills `accent` when on. It is a match setting per seat (`S6`), so toggling it mid-match is not in the twenty order kinds; in M1 the toggle is **read-only**, showing the lobby's value, and §11 hands the order kind to the milestone that adds a lobby.

### 7.4 Design (tab 4)

Requires a standing command post. Without one the tab shows `A COMMAND POST IS NEEDED` in `dimText` and nothing else.

Three columns of buttons — chassis, drive, modules — listing what the seat has researched, one selected in each of the first two and up to the chassis's mount count in the third. M1 offers three chassis, three drives and four modules, so the columns never scroll.

Under them, **the derived statistics**, from `GameShared/DesignStats`, recomputed on every change and shown before the commander commits:

```
SPEED        104 wu/s      HIT POINTS    100
ARMOUR      5 K / 5 T      SIGHT      20 cells
COST         130 power     BUILD TIME     13 s
```

A field that a research upgrade has raised is drawn in `accent` with the base value in `dimText` beside it. A design that cannot be built shows the fault from `DesignFault` as one line of `warning` — no modules, too many modules, or a module this chassis refuses — and the Save button is disabled.

**There is no name field, and there was never anywhere for it to send what it typed.** §11 row 19 already recorded that a design's name reaches the simulation nowhere — `SaveDesign`'s four operands are all numbers and `TechnicalDesign.md` §4.7's records carry no text by `ADR-002` — so the field edited a string the host never saw. `ADR-021` removes the keyboard that fed it, which turns a latent defect into a visible one, and the honest resolution is to cut the field rather than to reach for `CoreTextEditContext` for a string with no destination. Save emits `SaveDesign` and a design is listed by its index until row 19's wire change lands, in the milestone that owns it.

Designs are per commander and saved between matches (`TechnicalDesign.md` §9). **There is no delete and no rename in M1**: the twenty kinds carry neither, and a seat holds at most sixteen designs, after which Save replaces the design in the same slot or is refused.

### 7.5 Match (tab 5)

What the lobby fixed, so that a player can check it without leaving: the landscape's name and size class, the victory condition, the device cap, the starting power, the technology tiers, and each seat's kind and alliance with its commander colour. Read-only. It exists because M1 has no lobby and the defaults are otherwise invisible.

---

## 8. The selection panel

512×288 at (288, 792). What it shows depends on what is selected.

**One own device**: its design name, an icon, a `barHealth` bar with the hit points as `current / max`, the rank badge and rank name, the chassis, drive and modules as three lines, and the current order kind with its target. A device under half health draws its bar in `barHealth` until a quarter and `barHealthLow` under it.

**Several own devices**: a grid of portraits at **64×64** with a two-pixel health strip along each one's bottom edge and a one-pixel `accent` border on the one the panel's detail lines describe. **Tapping a portrait narrows the selection to that device; a press and hold removes it from the selection.** Beyond what fits, the grid shows what fits and a count, and scrolls by one-finger drag.

**Touch closes §11 row 18 rather than needing it.** That row asked for a modifier field on `NeuronClient/InputEvent.h`, because "`Ctrl`-clicking removes it from the selection" and an input event carried no modifier state — so a sink that consumed a click could not tell the panel which kind of click it was. Under `ADR-021` there is no modifier and the two acts are **two different events**, `Tapped` and `RightTapped`, told apart at the recogniser rather than by a flag the event would have had to carry. The gap is closed by the input model changing underneath it, which is worth writing down: it is the only row in §11 this migration answers for free.

The count that fits is `T4`'s to compute and not this document's to guess. At 64 a side with 4-pixel gaps the 512×288 panel's client area holds seven columns and three rows before the detail lines, against the 32 the mouse interface fitted at 48 — which is §3's rule being paid for in counts, exactly as it said it would be.

**One own structure**: its name, an icon, a health bar, and its role's own line — a factory's current build, a lab's current item, a generator's served extractor count, an extractor's yield per second or `UNSERVED` in `warning`, a command post's trickle. A structure under construction shows a `barBuild` bar and the seconds remaining instead of health, because health follows progress (`GameDesign.md` §5).

**A structure plan** shows the structure's name, `PLAN`, its cost, and `NOT STARTED` until a builder begins it.

**A visible enemy device**: its chassis, drive and modules, its health bar, and its rank. Designs are not secrets (`GameDesign.md` §8). The parts arrive with the design record for the first device of that design a client sees; **until that record arrives the panel shows the health bar and `DESIGN UNKNOWN` in `dimText`**, and fills in when it arrives.

**A ghost structure** — one in an explored cell that the commander cannot currently see — shows its kind and the health it had when last seen, with the whole panel's body text in `dimText` and the word `REMEMBERED` in the title strip. It is never shown as live, because that is the one thing the fog exists to prevent.

**Nothing selected**: the panel body is empty but for a `dimText` line naming the two things a player who has just started needs, `TAP TO SELECT` and `TWO FINGERS MOVE THE CAMERA`. The second replaces `RIGHT CLICK TO ORDER` because ordering is now what a tap does once something is selected (§6), and the camera is the one thing a new player will otherwise fail to find.

---

## 9. The orders panel, the minimap and the readouts

### 9.1 Orders (1568, 792, 352, 288)

Shown for a selection of own devices; empty for a structure or an enemy.

**Primary orders**, as 64×64 icon buttons: Move, Attack-move, Patrol, Guard, Stop. An order that arms (§6) fills `accent` while armed. `ReturnToRepair` is listed and **disabled in M1**, because M1 has neither a repair bay nor a repair module (`S10`), with a tooltip a press and hold now shows (§4).

**This panel absorbs what the keyboard used to carry**, and it is why it is the panel `T4` should lay out first: five primary orders, `CLEAR` (§5), the latching `QUEUE` toggle (§6), and the ten numbered group buttons that were `Ctrl` plus a digit — seventeen targets where the mouse interface had five, in the same 352×288. **It does not fit, and the answer is a second tab rather than a smaller button** (§3): the orders panel gains the same tab strip the command panel has, with the orders and stances on one and the groups on the other. `T4` owns which is which; what this document fixes is that nothing shrinks to make room.

**Stances**, as four rows of 64-tall buttons, each row a mutually exclusive set, the active one filled `accent`:

| Row | Options | In M1 |
|---|---|---|
| Fire | At will / Return fire / Hold fire | All three |
| Range | Optimal / Long | Both |
| Retreat | 50% / 25% / Never | **Never only**; the other two are disabled with a tooltip, because nothing repairs |
| Movement | Pursue / Hold position | Both |

Each emits `SetStance`. A mixed selection shows no option filled in a row where its devices disagree, and tapping sets them all.

### 9.2 Minimap (0, 792, 288, 288)

The client area is exactly **256×256**, so a Small landscape of 128 cells is **two pixels a cell**, drawn without resampling — which is why the panel is this size and why M1 is a Small landscape. A Medium landscape would be one pixel a cell and a Large one pixel to four, and §11 hands the scale rule for those to the milestone that ships them.

Drawn back to front:

1. **The fog**, from the render view's fog grid: black where unexplored, the terrain colour at half brightness where explored, at full where visible. This is the same grid the fog pass reads (`K2`), so the minimap and the world can never disagree.
2. **Structures**, two pixels a cell of their footprint, in their commander's colour; a ghost structure at half brightness.
3. **Devices**, one pixel each, in their commander's colour. Only what the interest set holds, which is what the commander can see.
4. **The selection**, in `accent`, over whatever colour the object had.
5. **The camera frustum**, a one-pixel `panelBorder` quadrilateral where the frustum's four far corners meet the ground plane, clipped to the map.

A tap moves the camera to that point, keeping its height and orientation. A one-finger drag scrubs the camera continuously. A press and hold issues the default order to that landscape point (§6).

**A fingertip covers a quarter of this map, and that is stated rather than designed around.** The client area is 256×256 authored pixels and §13's target is 64, so the contact patch spans about 64 of the 256 — at two pixels a cell on a Small landscape, **thirty-two cells**. For moving the camera that is harmless: the camera is a view and the next drag corrects it. For an order it would not be, and it is the hold that makes it safe rather than the precision: the ring of §4 is drawn on the ground at the point the lift will use, the player slides until it is where it should be, and lifting commits. **This is the clearest case in the document of why press-and-hold earns its dwell** — the gesture is slow exactly where the aim is bad.

A two-finger gesture over the minimap drives the camera as it does everywhere else (§4), which replaces the wheel the mouse interface refused here: a player scrubbing with one finger cannot fall out of the sky, because height is the other hand's.

### 9.3 Power readout (0, 0, 400, 40)

One line, 16-pixel text, in `bodyText`:

```
POWER 1,240 / 2,000   +15/s
```

The stockpile and its cap, then income a second. **The stockpile and the cap are both `SeatState`'s** — this paragraph said the cap was not on the wire, and it was wrong: `stockpileCapHundredths` has been a field of the record since `N1`, written and read beside the stockpile, so the panel prints what the host says rather than recomputing 1,000 plus 500 a generator.

**The income is measured and not re-derived**, which is `K4`'s own finding (2026-09-20). It is "five a second for every served extractor plus the command post's trickle", and *served* is the host's word: `GameLogic/Economy.h` assigns each generator the four nearest unserved extractors in range and recomputes the assignment every tick, and nothing replicates the result. A client that re-derived it would be a second implementation of a simulation rule — the thing `TechnicalDesign.md` §5.3 keeps out of the replica — and it would also need the seat's research upgrades, which reach the client as a mask of completed rows and not as an extractor rate. So the panel reads the rate off `SeatState::extractedHundredths`, the running total of what this seat's served extractors have yielded: the rise over the ticks between two readings of it is the extraction rate exactly, upgrades and assignment included, with no rule copied anywhere. It is sampled over a second rather than over a frame, because a frame is two ticks and two ticks of extraction divided by two jumps whenever a generator picks up an extractor mid-interval. The command post's trickle is added to it from the replica's own standing command posts, which is a flat row value no assignment touches.

The stockpile turns `warning` at the cap, because income is being lost.

### 9.4 Match state (1520, 0, 400, 40)

The elapsed match time as `mm:ss` from the replica's tick, and, when the victory condition is Survival, the time remaining. Nothing else: a frame-rate counter and a network readout are a debug overlay, not this panel, and `G2` adds them behind F3 if it wants them.

---

## 10. Overlays

**The match-end overlay** (560, 380, 800, 320) appears when the seat's victory state leaves `Playing`. It fills the panel frame, shows `VICTORY` in `accent` or `DEFEAT` in `warning` at 32 pixels — the one place text is drawn at a size other than 16, as a doubled 16-pixel glyph, which is exactly 1:1 at twice the scale and stays crisp — the match duration, and a Quit button. The simulation stops advancing once decided (`S11`), so the world behind it is frozen and the camera still moves, which is deliberate: a player wants to look at the field.

**The quit menu** (720, 400, 480, 320) appears on **the peel's last rung** (§4): Back, Leave match and Quit, stacked as 64-tall buttons. **The way out is the third two-finger tap** — the first cancels an armed order, the second closes a modal panel, and when there is nothing left to peel the third opens this. `ADR-013` and [`ADR-020`](ADR/ADR-020-central-server-no-lobby-no-pause.md) put it on F10 because Escape's last press swapped the pointer's mode and a key that did both would do the wrong one by accident; `ADR-021` deleted the modes, which frees the rung Escape was spending on them, and F10 is not a key any more in any case. **This amends both ADRs on where the exit is and neither on what it does**: it is still not a pause, the world still runs behind it because the world is not in this process, and it still ends in `CoreApplication::Exit`.

A gesture that does nothing is a gesture a player repeats harder, which is the argument for giving the last rung something to do rather than leaving it empty. Against it: a stray two-finger tap in a quiet moment opens a menu. That is survivable — the menu is modal, so the next peel closes it, and nothing on it acts without a second deliberate tap — and it is recorded as §12 ruling 12 for a reader who would rather spend a permanent button on it.

It was a *pause* menu with Resume until 2026-09-20, when `ADR-020` removed the pause.

**The chat line is cut from the client and the `Chat` order kind stays on the wire.** Composing a message needs a keyboard, a `CoreWindow` app reaches the soft keyboard through `CoreTextEditContext` and `InputPane`, and that is real work — a text-input context, a focus model, and an on-screen keyboard that covers the bottom third of the frame, which is where every panel in §2 lives — for a feature §10 itself described as existing "to prove the order kind travels rather than to be used", in a milestone with one human in it.

**Receiving still works**, because receiving needs no keyboard: messages appear as up to four lines of `bodyText` at y 760, each for eight seconds, exactly as before. What is gone is the field that sent them. `Chat` keeps its place among the twenty kinds (`GameShared/Order.h`) because removing it is a wire change for no gain, and the client simply never emits it.

**What this defers, and to whom.** Text input arrives when something needs text that has somewhere to go — which is §11 row 19's design names, in the milestone that adds them — and `CoreTextEditContext` lands once, for both. A tree that built a soft keyboard now would build it for a field with no destination (§7.4).

---

## 11. What this document needs that does not exist yet

Each of these is a real gap found while writing this document, with the task that owns it. The ones marked **blocks K4** must land before the panels are finished.

**The touch migration changed four rows on 2026-09-20** (`t1-touch-interface/T1`, [`ADR-021`](ADR/ADR-021-touch-is-the-only-input.md)). Row 18 **closed** without anyone writing the field it asked for, because there is no modifier under touch and the two acts are two events. Row 20 kept its gap and lost its dwell to `Holding`, and got more urgent rather than less. Row 19 lost half its evidence — the name field is cut — and none of its substance. And row 21 is new, for the text input both cuts defer. Row 14's ring changed where its ray comes from and not what it needs.

**Rows 17 to 20 were added on 2026-09-20**, all four found while `K4` built the panels against this document line by line — a lobby nothing replicates, a click whose modifier no event carries, a design name nothing in the tree holds, and a tooltip with three constants and no text. None of them blocks `K4`: each is a line of a panel that says what it can rather than guessing, and each names the task that would close it. §9.3 was **corrected** at the same time: it claimed the stockpile cap was not on the wire and it has been since `N1`, and the income it asked the client to derive would have been a second copy of `GameLogic/Economy.h`'s service assignment, so the panel measures it instead.

**Row 7 closed on 2026-09-20.** With row 4 done by `C6`, nothing marked **blocks K4** is open.

**Rows 14 and 15 were added on 2026-09-19** with the owner's ruling on the pointer (§12, ruling 3, now superseded). A ring that lies on the ground needs the ground, and nothing in the tree could answer where a ray meets it or which way it faces there; that is row 14, and it outlived the ruling that raised it — §4's hold ring needs exactly the same two answers, from the contact rather than from the screen's centre. Row 15 is the ring's texture, and it is owned by `K7` rather than by `C4` on purpose: `C4` is `done`, and the paragraph above this one is the record of what it cost the last time a live gap was hung off a finished task.

**Rows 12 and 13 were added on 2026-09-19**, found while writing `R2`'s `RenderViewBuilder`: its acceptance asked for features and projectiles before anybody checked whether the content behind them existed, and neither does. They are `C7` and `C8` in `tasks/m1-vertical-slice.yaml` as well as here, because this table is where a reader of the design finds a gap and the plan is where `Tools/CheckTaskDag.py` does — rows 3, 4 and 5 were lost for exactly the want of the second half.

**Rows 3, 4 and 5 changed owner on 2026-09-19.** They were given to `N1`, `C1` and `C2`, and those three tasks were marked `done` without them — the acceptance lines that would have caught it were never written, and `Tools/CheckTaskDag.py` reads the plan's YAML and not this table, so a row whose owner is `done` is invisible to the thing that schedules work. They are now `N4` and `C6` in `tasks/m1-vertical-slice.yaml`, where the checker can see them. Row 4 is also moved ahead of `K3`: this document previously said none of these blocks `K3`, which was true of the letter of `K3`'s acceptance and false of its intent, because a toolkit that hard-codes the palette leaves nobody owning the move to data that §3 promises.

| # | What is missing | Owner |
|---|---|---|
| 1 | `AuthoredFromClient`, the inverse of `FitAuthored`, with its round-trip test | `K3` |
| 2 | ~~`NeuronCore/RenderView.h` must carry a selection flag, construction progress, a commander colour index rather than a packed colour, the fog grid, and a wreck-or-projectile distinction~~ | **Done 2026-09-19 by `R2`**: `RenderInstance` carries all five, and `GeometryPass` takes `C6`'s eight commander colours once a match and resolves the index, so the palette is a table rather than a number copied into every instance of every frame |
| 3 | ~~A wire record carrying the per-seat order rejections `S2` already records, so §6 can show them~~ | **Done 2026-09-19 by `N4`**: `SeatState` carries the seat's last refusal as a sequence, a kind and a reason |
| 4 | `GameData\Interface.json`, the chrome palette of §3, and its loader row | `C6` — **blocks K4** |
| 5 | The eight commander colours, as a content table | Ruled in `GameDesign.md` §11 (owner, 2026-09-19); `C6` carries the table, and `R2` reads an index into it |
| 6 | The eight ranks' names, badges and percentages. `GameDesign.md` §6 says "a small percentage" and nothing more, and `S5` defers to a design that proposes none | `GameDesign.md` §6, then `C2` |
| 7 | ~~The icon list: six cursors and one icon per structure, module, order and stance, with `Icons.dds` laid out as a grid of 32×32 cells~~ | **Done 2026-09-20 by `K4`**, and owned by `K4` rather than by `C4` for the reason row 15 gives: `C4` is `done`, and this table's own heading records what a live gap hung off a finished task costs. `Tools/MakeIconAtlas.py` writes the sheet as eight columns of four 32×32 cells — thirty-two icons, one a cell, every cell named — and `GameData\Interface.json`'s `icons` table says which cell is which, a structure and a module by their own content ids. **The art is a placeholder and says so**, exactly as `Tools/MakePlaceholderModels.py`'s boxes are: each icon is a distinct simple shape drawn as a single-channel mask, which is what §3 asks an icon to be and what today's imported banners are not, so a panel of buttons reads as a panel of different buttons and the one wired wrong is visible at a glance. A few shapes repeat where the placeholder has nothing better to say — the chevron is Move, the order Move and the Pursue stance — and the names are distinct, so the owner's art replaces one cell at a time |
| 8 | Whether a structure's own "target priority" stance has options at all; `GameDesign.md` §8 names it and gives no set | `GameDesign.md` §8 |
| 9 | A rally-point order kind and a design-delete order kind. Both need a twenty-first order kind, which changes the wire. **A match pause was the third and is gone** ([`ADR-020`](ADR/ADR-020-central-server-no-lobby-no-pause.md), 2026-09-20): it is buildable — M1 built one — and a central server has no player entitled to stop the world for everyone | A later milestone; noted in `TechnicalDesign.md` §4.7 |
| 10 | The minimap's scale rule for Medium and Large landscapes | The milestone that ships them (M2, M3) |
| 11 | The survival clock's default duration | `GameDesign.md` §2 |
| 12 | A **feature table** in `GameShared`. `GameShared/Feature.h`'s `design` is documented "Row index in the feature table" and no such table exists — `ContentTree` has none and `GameData` ships no `Features.json` — so a feature reaches a client naming a row of nothing and `RenderViewBuilder` cannot draw one | `m2-skirmish/T13`. **Ruled 2026-09-19**: scenery is M2's, so M1 ships no feature table. The gap stays open rather than being papered over — `GameShared/Feature.h`'s `design` still names a table that does not exist and `RenderViewBuilder`'s feature branch cannot be exercised in M1 |
| 13 | ~~**What a shot looks like.** `TechnicalDesign.md` §5.3 sends projectiles as short-lived events rather than objects, and no row says what one looks like or for how long~~ | **Done 2026-09-19 by `C8`**: `ModuleDesc` carries `projectileModel` and `projectileLifetimeTicks`, validated like any other model reference and refused one without the other; `RenderViewBuilder` turns a `Shot` event into an instance of `RenderInstanceKind::Projectile` travelling from the firing module's `MarkerMuzzle` to the point it was aimed at, from the event's own tick, and drops it when its lifetime is over. The event that drives it is `C9`'s |
| 14 | **A ray against the landscape, and a normal at a world point.** §4's hold ring needs both: where the ray **through the contact** meets the terrain, and the slope it lies against. (It read "through the screen's centre" until 2026-09-20, when `ADR-021` deleted the aim mode that had no other origin to offer.) `NeuronCore/HeightView.h` carries the samples and nothing else — no ray march, no normal — and the only ray-versus-world code in the tree is `GameClient/Picking.h`'s ray-versus-sphere, which never touches the ground. `GameClient/OrderInput.cpp`'s `GroundPoint` intersects a **flat plane at y = 0** and says why: a Move names x and z only, so the height is not worth a walk. That reasoning is sound for the order's payload and does not cover *which* x and z the commander pointed at — over ground at height h the plane's answer is off by about `h / tan(pitch)`, which at 200 units and a 26.6° pitch is 400 world units, six cells. One function serves the ring, the order and the footprint ghost | **Half done 2026-09-19 by `K7`**: `NeuronClient/GroundRay.h` answers both — a march of the sample grid against the terrain mesh's own diagonal, a bilinear normal from central differences that is exactly `(0, 1, 0)` on flat ground, a clean miss off the landscape, and the sea answered by its own plane rather than by the seabed under it. What is left is `CursorPass`, which needs a device, and `OrderInput` using this instead of the plane |
| 16 | **A factory's production queue on the wire.** §7.2 draws "the queue, right half: up to eight entries, the first with a `barBuild` progress bar and the seconds remaining", and nothing replicates it. `GameShared/Seat.h` keeps the queues — one list a seat rather than one a factory, up to `MAX_PRODUCTION_ENTRIES` of `{factory, design, remaining}` — and `SeatState` carries none of it; `StructureState`'s `buildPercent` is the structure's OWN construction and not what it is producing. So the Produce tab can offer the design list and emit `SetProduction`, which is the half that makes it useful, and can show nothing of what is queued. Found on 2026-09-20 while building the panels. **It is a wire change and therefore a task of its own**, not a line to bolt onto `K4`: a per-seat list of up to sixty-four entries is 768 bytes against `SeatState`'s 46, so it wants its own encoding — sent when it changes, as `designs` are — rather than a field on a record the encoder compares by value every publish | A later milestone; `m2-skirmish` |
| 15 | **The ground ring's texture and its blurred twin** (§4, added 2026-09-19). Not a cell of `Icons.dds`: the ring is a world-space quad that wants its own texture at 128×128 or better, and the dark pass wants a pre-blurred copy of it rather than a filter at run time (`SpeciesLook.md` §7.1). Owned by `K7` and not by `C4`, because `C4` is `done` and this table's own heading records what a gap owned by a finished task costs | `K7` |
| 17 | **The lobby's own settings on the wire.** §7.5 shows "what the lobby fixed, so that a player can check it without leaving" — the landscape's size class, the victory condition, the device cap, the starting power, the technology tiers, and each seat's kind and alliance — and a joining client is sent none of it: `JoinAccepted` carries the seat and the landscape's definition (`GameShared/Messages.h`) and no `MatchSettings` at all. M1's Match tab reads the lobby the SAME PROCESS chose (`Match::Settings`), which is honest for a host thread in this executable and is nothing at all over UDP. Found on 2026-09-20 while building the panels | A later milestone; `m3-multiplayer`, with the lobby that fills it |
| 18 | ~~**A modifier on `NeuronClient/InputEvent.h`.** §8 says "Ctrl-clicking removes it from the selection", and an input event carries no modifier state at all~~ | **Closed 2026-09-20 by [`ADR-021`](ADR/ADR-021-touch-is-the-only-input.md)**, without anyone writing the field. There is no modifier under touch, and the two acts a modifier would have distinguished are two events the recogniser raises separately — `Tapped` and `RightTapped` (§8). The only row in this table the input migration answers for free, and worth the line because a reader who finds `K3` never wrote the field should know it was not forgotten |
| 19 | **A design's name.** §7.2 lists "every design the seat has saved … holding the name", §8 shows a device's "design name", and nothing in the tree carries one. (§7.4's name field, which this row also named, was **cut on 2026-09-20**: `ADR-021` removed the keyboard that fed it, and it was editing a string with no destination either way — which is this row.) A design is a chassis row, a drive row and a list of module rows in the simulation (`GameShared/Device.h`), on the wire (`GameShared/Records.h`'s `DesignState`) and in the order that saves it (`GameShared/Order.h`'s `SaveDesign`, whose four operands are all numbers). The name field therefore edits a string that reaches the simulation nowhere, and every panel that wants a design's name prints its index. It is a wire change and an order change together — `SaveDesign` has no operand left for a string, and `TechnicalDesign.md` §4.7's records carry no text by ADR-002, so the name is a per-seat table sent as `designs` are. Found on 2026-09-20 by `K4` | A later milestone; `m2-skirmish` |
| 20 | **A tooltip's text.** Still open, and half of it changed shape on 2026-09-20: the *dwell* is now `Holding` rather than a hover timer, so `UiLayout.h`'s dwell constant goes and the gesture supplies it — but a `UiWidget` still has no text, so nothing can be shown. §9.1 asks for one by name twice, on the disabled `ReturnToRepair` button and on the two disabled retreat stances, and §4 now promises that a finger can ask a dead button why it is dead. **Touch raises this row's priority rather than closing it**: under a mouse the disabled fill was the whole explanation and that was merely thin; under a finger there is no other way to ask | `K3`, in the milestone that needs it |
| 21 | **Text input at all.** §10's chat line and §7.4's name field both took characters from the keyboard `ADR-021` removed, and both are cut rather than ported (§7.4, §10). A `CoreWindow` app reaches the soft keyboard through `CoreTextEditContext` and `InputPane`, which is a text-input context, a focus model and an on-screen keyboard covering the bottom third of the frame — where every panel in §2 lives. It is written once, for whatever first needs text that has somewhere to go, which is row 19 | The milestone that closes row 19 |

---

## 12. The rulings this document takes

Each is a decision this document made that a reader may reasonably want to overturn, and the owner's merge is the ruling. Overturning one is an edit here, not an ADR, because none of them is an engineering decision.

1. **Text is 16 pixels, monospaced, one face.** Against `SpeciesCanvas.md` §7, which asks for 12 and 13. The reason is in §3: at a fixed authored resolution, a size that is not the atlas's cell resamples every glyph, and pillar 3 is what the authored resolution exists to serve. Overturning this means accepting soft text or re-authoring the atlas at 12 and 13.
2. **One-pixel borders, no drop shadow on a panel.** Against `SpeciesCanvas.md` §3's 2-pixel border with a 1-pixel outer loop, and with `GameDesign.md` §11.4 and `K3`, which both say one pixel. The yellow-twice-with-shadow title treatment is dropped with it; the title's glow was Species's answer to text over a red gradient, and this document's panels are dark.
3. ~~**The mouse aims the camera with no button at all, and Escape releases it**~~ (owner, 2026-09-19). **SUPERSEDED 2026-09-20 by [`ADR-021`](ADR/ADR-021-touch-is-the-only-input.md)**, which deletes the mouse, both pointer modes, the raw relative counts and `AIM_RADIANS_PER_COUNT` with them. Two fingers drive the camera (§4). **It is left here unedited because it is the best record in this tree of why the signs of a camera delta are written down rather than trusted**, and §4 inherits that argument whole — a pinch is as invertible as an aim delta, and as silent about it.

   The original text follows.

   *The mouse aims the camera with no button at all, and Escape releases it* (owner, 2026-09-19; this replaced the ruling of the same number, which gave aiming to the middle button held). M0 bound aiming to the right button because no orders existed yet; D1 moved it to the middle button so the right button could give orders; the owner asks for Species's own arrangement, where the mouse is connected to the camera the whole time and a key releases it. What was weighed against it is that an aimed mouse has no pointer, and §5's drag rectangle, §7's panels and §9's minimap all need one — hence two modes rather than Species's single one, and hence the pause menu moving to F10 so that Escape means one thing.
   **The aim itself is taken from the raw relative counts and not from Species's cursor-warping.** Species's free-movement camera has no yaw and no pitch: it moves a virtual cursor, ray-casts it onto the terrain, rotates the camera's forward vector toward that world point by `sin(angle) × sqrt(dt)` a frame, and then warps the operating system's pointer back so it stays glued to it, the screen's edge clamp being what makes the turn continue (`SpeciesLook.md` §7.2). It is a fine scheme and it is the wrong one to port: `NeuronClient/Camera.h` stores yaw and pitch rather than a basis, `NeuronClient/RawMouse.h` already reads relative counts precisely because `TechnicalDesign.md` §6.5 wanted them "unclamped by the screen's edge and without the pointer acceleration Windows applies", and a per-frame `SetCursorPos` is the thing that behaves worst across two monitors and a high-DPI display. Species's own editor camera is the delta camera, at 0.005 radians a pixel, and that is what M1 takes.
4. **The world view is the whole frame and the panels sit over it.** The alternative, a world viewport above a HUD strip, wastes no pixels to occlusion but makes the scene 1920×792 and breaks the authored resolution ADR-004 fixed.
5. **Shift-queued orders are a client-side convenience.** The simulation has no queue and does not gain one for M1. A rejoining client loses its queue.
6. **The auto-research toggle is read-only in M1.** Changing it mid-match needs an order kind that does not exist.
7. **The footprint ghost is advisory.** The client can disagree with the host about legality over ground whose flatten deltas it has not been sent, and the rejection message is the correction.
8. **A ghost structure is visibly remembered**, in dim text under a `REMEMBERED` title, rather than drawn as live with a subtle difference.
9. **The minimap is 256×256 and M1's landscape is Small**, so the map is two pixels a cell with no resampling.
10. **One finger is the game and two fingers are the camera** (§4). The alternative weighed was one finger for the camera and a mode button for selection, which is how a map application works — refused because an order is the commonest act in a real-time strategy game and the commonest act gets the cheapest gesture. Overturning it moves the rectangle of §5 onto two fingers and the pan onto one.
11. **The camera has no inertia.** `ManipulationTranslateInertia`, `ManipulationRotateInertia` and `ManipulationScaleInertia` are all unset (§4). A camera that glides on after the fingers leave is a camera that is somewhere else when the player looks back, and an order given to units the player can no longer see is what `GameDesign.md` pillar 3 exists to prevent. It is a feel decision and the cheapest of these to overturn: three flags and `AutoProcessInertia`.
12. **The peel's last rung opens the quit menu** (§4, §10), which amends `ADR-013` and `ADR-020` on where the exit is and neither on what it does. Against it: a stray two-finger tap in a quiet moment opens a menu. For it: a gesture that does nothing is a gesture a player repeats harder, and the rung fell vacant when `ADR-021` deleted the pointer modes Escape's last press used to swap. Overturning it spends a permanent 64-pixel button on the exit instead.
13. **Pitch is bound to height rather than to a gesture** (§4). Two fingers give three degrees of freedom and a fly camera wants four, so the fourth is the one the player asks for least. Overturning it spends two buttons on tilt.
14. **The chat line is cut and `Chat` stays on the wire** (§10). Composing needs `CoreTextEditContext`, `InputPane` and a soft keyboard over the panels, for a feature this document already described as existing to prove an order kind travels. Receiving is unaffected. Overturning it means writing the text-input path now rather than when §11 row 19 needs it.

---

## 13. The touch target, and what it costs

**This is the number `t1-touch-interface` waits on and the whole of why `T1` comes first.** Every panel in this document was laid out for a pointer one pixel wide; a fingertip is not, and the difference decides how much interface fits on the screen at all.

### What the platform asks for

Microsoft's *Guidelines for touch targets* gives one figure and three adjustments. The figure: **7.5 mm square** — "40x40 pixels on a 135 PPI display at a 1.0x scaling plateau". The adjustments that matter here, both in the direction of *larger*: targets that are **repeatedly or frequently pressed** should be bigger than the minimum, and targets with **severe consequences if touched in error** want greater padding and distance from the content edge. A real-time strategy game is made of the first and contains the second — `Demolish`, `Surrender` and `CancelResearch` all destroy something and none of them refunds.

### The arithmetic, which turns out not to need the resolution at all

`FitAuthored` scales the authored 1920×1080 frame by `s = min(W / 1920, H / 1080)` into a physical frame of `W × H` pixels at `ppi` pixels an inch. So one authored pixel is `25.4 · s / ppi` millimetres, and a target of `a` authored pixels reaches 7.5 mm when

```
a  =  7.5 · ppi / (25.4 · s)
```

On any panel at least as tall as 16:9 — which is every tablet, every 3:2 Surface and every 16:10 laptop — the fit is limited by width, so `s = W / 1920` and `ppi = W / width`. Substitute both and **`W` cancels**:

```
a  =  7.5 · 1920 / (the used width of the display, in millimetres)
   =  14400 / width_mm
```

**The resolution is gone from that formula, and so is the DPI.** The required target is a fixed *fraction* of the authored frame — 7.5 mm divided by the screen's physical width — and it depends on nothing else.

**Two consequences, and the second one kills an assumption this plan was built on.**

First, the whole touch budget of the interface is one division: the number of targets that fit across the screen is `width_mm / 7.5`, and no choice this tree can make changes it.

Second — **raising the authored resolution buys nothing.** `ADR-021` and `UwpMigration.md`'s risk table both said that if the interface would not fit a finger, [`ADR-004`](ADR/ADR-004-renderer-foundation.md)'s authored 1920×1080 is reopened. That was wrong, and the formula above is the proof: double the authored width and `a` doubles with it, exactly. The panels get twice as many authored pixels and a finger gets twice as many authored pixels, and the number of buttons on the screen is identical. **`ADR-004` is not reopened, and it never could have been the answer.** Both documents are corrected.

### Worked, for the devices this could run on

Nothing here was measured on a device. Each row is arithmetic on a published panel size, and the owner's own device is the one measurement this document still owes.

| Device | Panel | Used width | Target, authored px | Targets across × down |
|---|---|---|---|---|
| Surface Go 3 | 10.5″, 1920×1280, 3:2 | 221.9 mm | **65** | 29 × 16 |
| Surface Pro 7 | 12.3″, 2736×1824, 3:2 | 259.9 mm | **56** | 34 × 19 |
| Surface Pro 11 | 13″, 2880×1920, 3:2 | 274.7 mm | **53** | 36 × 20 |
| Surface Laptop Studio 2 | 14.4″, 2400×1600, 3:2 | 304.3 mm | **48** | 40 × 22 |
| 24″ touch monitor | 1920×1080, 16:9 | 531.3 mm | **28** | 70 × 39 |
| 27″ touch monitor | 3840×2160, 16:9 | 597.7 mm | **25** | 79 × 44 |

**The smallest device needs the largest authored target**, which is the opposite of what a reader expects and follows directly from the formula: a small screen has small pixels, and `FitAuthored` hands it the same 1920 either way. A 27″ monitor asks for 25 authored pixels, within one of the 24-pixel button height this document specified for a mouse — so the interface as written today is **already a touch interface on a desk monitor**, and is not one on the tablet the game is for. Every figure in the table rounds *up*, because a target below the requirement is not a target.

### The ruling

**THE TOUCH TARGET IS 64 AUTHORED PIXELS**, and §3 makes it the floor for every interactive element with no exception.

It is 2× the 32-pixel icon, 4× the 16-pixel glyph cell, and exactly 30 columns across the authored frame with 16 rows down and 56 rows spare — so a layout can be built on a 64-pixel grid without fractions, which is what `T4` will want.

**THE DEVICE IS A SURFACE PRO** (owner, 2026-09-20), and that settles it with room to spare. 64 authored pixels is 7.5 mm or better on any display whose used width is at least **225 mm**, and every Surface Pro ever made clears that — the narrowest, the 12″ Pro 3, is 253.6 mm:

| Surface Pro | Used width | Needs | 64 px gives | Margin | Budget across × down |
|---|---|---|---|---|---|
| 12″ (Pro 3) | 253.6 mm | 57 | **8.45 mm** | +13% | 33 × 19 |
| 12.3″ (Pro 4 – 7) | 259.9 mm | 56 | **8.66 mm** | +16% | 34 × 19 |
| 13″ (Pro 8 – 11, Pro X) | 274.7 mm | 53 | **9.16 mm** | +22% | 36 × 20 |

**The margin is the point, not the slack.** The platform asks 7.5 mm for chrome pressed deliberately and then says to go *larger* for targets pressed frequently — which is every order button in a real-time strategy game. 64 spends 13 to 22% on exactly that clause, on the device named, and it does so without the model needing to be pinned down: a 12″ Pro and a 13″ Pro take the same layout.

**Why not drop to 56, which the device would allow.** It would buy four more columns of budget and cost every scrap of the frequency margin — 7.58 mm on a 12.3″ Pro is the minimum with nothing over it — and 1920 / 56 is 34.3, so the grid stops being exact. 64 divides the authored frame into 30 columns on the nose. The four columns are not worth either.

**The 10.5″ Surface Go shortfall is now moot** and is left in the table above because the formula still says it: on a display that narrow, 64 is 7.40 mm, 0.10 mm or 1.3% under. It is not the device this is for.

**Why not 72, which would clear every device in the table.** Because the budget is 29 targets across on the worst device and 72 would cut it to 26, and §9.1's own count — seventeen targets where the mouse interface had five — is already what forces a second tab. A floor that clears every conceivable panel by spending the interface's remaining room is not a safer floor.

### What this costs the document, in one list

Each of these is a count that fell, not a size that shrank (§3):

- **The command panel loses 64 of its 288 rows** to a tab strip, because a tab is a button and the 20-pixel title strip cannot hold one (§7).
- **The orders panel gains a tab strip of its own**, because the keyboard's ten group bindings, `CLEAR` and `QUEUE` land in it alongside five orders and ten stances — seventeen targets in the space that held five (§9.1).
- **The selection grid holds about 21 portraits where it held 32**, at 64 a side instead of 48 (§8).
- **Production and research rows double from 32 pixels to 64**, so three or four are visible where six or eight were, and the rest scroll (§7.2, §7.3).
- **The structure row no longer fits across the command panel** at six 128-wide buttons in a 752-pixel client, and wraps (§7.1).
- **Nothing in the world view changes**, and nothing about the text changes: 16-pixel glyphs are 1.85 mm on the smallest device and 2.3 mm on a Surface Pro, which is a readable pixel font and not a target. §12 ruling 1 stands untouched, which is the one part of this that came for free.

### Running it without a touchscreen

`AGENTS.md` §3 requires that anything touching input is **run and looked at**, and under `ADR-021` that means run with fingers. Two answers, in order of preference:

1. **The Visual Studio Simulator**, which deploys the packaged app and injects touch, including two-contact gestures — the only one of the two that answers "look at it", because a human is driving. **Whether it still ships with Visual Studio 2026 is not verified here**, `learn.microsoft.com` being unreachable from the machine this document was written on for everything but the API reference, and it is added to `p1-uwp-shell/P1`'s list of things to establish while that task is already in front of a Windows machine and a UWP toolset.
2. **`InjectTouchInput`**, the Win32 touch-injection API, for a scripted harness that replays a fixed gesture sequence. It answers repeatability and not judgement, and it is what a regression test would use if this tree ever wants one.

**Neither is a substitute for the owner's own device**, which is why `t1-touch-interface/T6` is an owner's run on a touchscreen and not a green build.

### What the Surface Pro settles, and what it raises

**The letterbox is now a known quantity, and `T4` and `p1-uwp-shell/P9` both want it.** A Surface Pro is 3:2 and this frame is 16:9, so `FitAuthored` is width-limited and the bars are horizontal. On a 13″ Pro at 2880×1920 the scale is exactly **1.5**, the frame occupies 2880×1620, and **300 physical rows are dark** — 150 top and 150 bottom, 15.6% of the panel. On a 12.3″ Pro at 2736×1824 the scale is 1.425 and 285 rows are dark.

Two consequences. The scale is a **clean 1.5 on a 13″ Pro**, which is not an integer multiple, so `AGENTS.md` §5's point-sampled path does not run and `P9`'s bilinear branch does — worth knowing before `P9` measures rather than after. And §4's rule that a contact in the letterbox is refused by every hit test makes those two bands **dead by construction**, which is the cheapest palm rejection this document will ever get.

### What is still owed

**One judgement, and it is the owner's, in front of the game rather than in front of this document.** 7.5 mm is a recommendation for application chrome pressed deliberately; this is a game pressed quickly, under pressure. The 13 to 22% margin above is an argument that 64 is enough and not evidence that it is. `t1-touch-interface/T6` is where it becomes evidence, and if 64 proves too small **in a fight rather than at rest**, the answer is fewer things on the screen and not smaller ones (§3).

**One thing the device raises that the mouse never did: posture.** A Surface Pro on its kickstand on a table is one interface; a Surface Pro held in two hands is another, and in landscape the grip falls on the left and right edges — which is where the minimap sits at (0, 792) and the orders panel at (1568, 792). The letterbox bands are top and bottom and do not help. **This document assumes the kickstand** because that is the posture a match-length session is played in, and `T6` is where the other one is tried. If held play matters, the answer is a margin down both edges, which costs the panel strip roughly two targets of width.
