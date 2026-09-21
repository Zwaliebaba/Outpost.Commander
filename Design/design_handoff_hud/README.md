# Handoff: Outpost Commander — in-match HUD (interface pass only)

## Overview

This package specifies the complete in-match heads-up display for **Outpost Commander**: the five
interface surfaces (credits, selection, build, damage alert, system), their overlays, the palette,
the type scale, the motion table, and every rectangle in integer authored coordinates.

**Scope: the interface pass only.** The 3D scene — camera, star field, galaxy band, plane grid,
ship and station meshes, asteroid meshes — is explicitly **out of scope for this handoff**. The
design frames draw a representative scene behind the HUD so contrast could be judged; none of it
is a specification and none of it should be implemented from this package. Where this document
mentions the scene, it is stating an assumption the HUD depends on (near-black background, large-area
luminance capped at 12%), not asking for work.

Implement: everything the interface pass draws.
Do not implement: anything the world pass draws.

## About the design files

The file in this bundle is a **design reference authored in HTML**. It is a prototype showing the
intended look, geometry and states — **not production code and not a component library**. Nothing in
it should be ported, transpiled or copied.

The target codebase is **C++ against Direct3D 12 with no UI framework** (`GameClient`, per R18/R20).
There is no XAML, no Direct2D, no CSS, no SVG, no image assets and no texture atlas other than the
DirectWrite glyph atlas. Recreating this design therefore means emitting:

- **axis-aligned solid rectangles** with per-vertex colour and per-vertex alpha,
- **line segments** of a given width at any angle,
- **flat-filled triangles** (used in exactly two places here),
- **glyph quads** from the Segoe UI atlas (ADR-009),

and a hit-test table of rectangles. Every value in this document is expressed so it can go straight
into that emitter. The HTML uses `div` elements and `clip-path` purely as a way to draw those same
primitives in a browser.

## Fidelity

**High fidelity.** Colours, type sizes, weights and every rectangle are final and are stated in
authored pixels as integers. Reproduce them exactly. Where a value is derived (panel width as a
function of group count, alert position from a bearing), the formula is given rather than a number.

## The frame and the unit system

| | |
|---|---|
| Authored frame | **1440 × 960**, 3:2, origin top-left, integers only |
| Physical panel | 2880 × 1920, 13", 267 PPI |
| Interface fit | exactly **2×** — every authored pixel is a clean 2 × 2 block, nothing is resampled |
| Density | **133 authored px / inch, 5.24 / mm** — divide authored px by 5.24 for millimetres on glass |
| Viewing distance | 500–600 mm, kickstand on a desk or lap |

The HUD draws **on top of the full 1440 × 960 frame**. There is no reserved chrome region and no
letterbox: every pixel of panel is a pixel of battlefield the player cannot see.

Half-pixel values are a defect. The interface pass has **no multisampling** (a flip-model back buffer
forbids it), so axis-aligned edges are clean and every diagonal is a deliberate cost.

## Touch tiers — asserted, not eyeballed

| Tier | Authored | On glass | Applies to |
|---|---|---|---|
| **Floor** | 48 × 48 | 9.15 mm | anything interactive; nothing is ever smaller |
| **Combat** | 64 × 64 | 12.2 mm | selection groups, clear, build cancel, damage alert |
| **Under fire** | 96 × 96 | 18.3 mm | the six build buttons |

**Minimum clear space between adjacent interactive targets: 16 authored pixels.**

`geometry.json` in this bundle is the machine-readable form of the geometry table and is intended to
be consumed by a unit test that asserts both rules. The tightest clearances in the shipped layout are
all exactly 16 and all vertical: ships row → modules row (752 → 768), modules row → cancel
(864 → 880), selection group[3] → clear (704 → 720).

## Handedness

The two bottom-edge panels — **selection** and **build** — swap sides under **one boolean**. The flip
is a pure reflection of x:

```
x' = 1440 - x - w
```

applied to those two panels and everything inside them. Credits, system and the alert do **not** move.
Right-handed is primary: build bottom-right under the reaching hand, selection bottom-left where it
can be read while that hand is on the glass. `geometry.json` carries the mirrored x for every affected
rect; do not recompute them by hand.

Within-row order reverses under the reflection, which is correct — "inner" and "outer" are preserved.

---

## Surfaces

### 1. Credits — top left, non-interactive

Panel 272 × 88 flush to the top-left corner. A 3 px own-team index bar down the leading edge, a 1 px
hairline on the bottom and right (the playfield-facing edges) with a 16 × 2 registration tick at each
end of the bottom rule.

- `CREDITS` label at BODY, `TEXT.2`, letter-spacing 0.10em, at (16, 14).
- The balance at DISPLAY, `TEXT`, at (16, 40). Integer, thousands separator, worst case five digits.
- A 120 × 3 **change flash** rule at (16, 76): cyan on a gain, amber on a spend, exponential decay.

There is **no income rate**. It has no data path and must not appear.

### 2. Selection — bottom, away from the reaching hand

Only drawn when something is selected. Panel height 128, flush to the bottom and to its side edge.
Width is a function of the group count `n` (1–4):

```
sel.panel.w   = 16 + 160n + 16(n-1) + 16 + 96 + 16     // n=1:304  n=2:480  n=3:656  n=4:832
sel.clear.x   = 16 + 160n + 16(n-1) + 16               // n=1:192  n=2:368  n=3:544  n=4:720
sel.group[i].x = 16 + 176i
```

Each group cell is 160 × 96, hit rect the same, combat tier. Inside a cell (offsets from the cell):

| Element | x | y | w | h | Notes |
|---|---|---|---|---|---|
| index bar | +0 | +0 | 4 | 96 | `TEAM.OWN` at 0.75 alpha |
| count | +16 | +10 | — | 32 | DISPLAY, `TEXT`. Up to 50 per group |
| design name | +16 | +18 | 128 | 20 | BODY, `TEXT.2`, **right-aligned**, letter-spacing 0.06em |
| hull trough | +16 | +52 | 128 | 10 | 1 px `RULE` keyline over `TRACK` |
| hull remaining | +17 | +53 | round(126·h/100) | 8 | `HULL` |
| hull lost | +17+rem | +53 | 126−rem | 8 | `HULL.LOST` |
| hull index ticks ×3 | +32 / +64 / +95 | +53 | 1 | 8 | `TICK.BAR`, drawn over the fill |
| cargo chip[i] | +16+33i | +72 | 29 | 10 | i = 0..3 |

Hull is **continuous 0–100** (aggregate across the group). Cargo is **four discrete buckets** and must
look discrete: filled chips are `ORE`, empty chips are `TRACK` with a 1 px `RULE` keyline, and there is
no trough behind them. Four discriminators keep the two bars apart — continuity, keyline, hue, and a
fixed row (hull always upper, cargo always lower). A design that does not carry ore draws **no cargo
row at all**; the hull row does not move.

Tapping a group narrows the selection to that design. **CLEAR** (96 × 64, combat tier) deselects
everything and is the only way to do it — there is no gesture for deselection.

**This panel updates live during the double-tap group-select gesture** and the count must not be
animated or interpolated: the 192 px selection circle in the world is largely under the player's hand,
so this is the readout they are actually watching. It is the most latency-sensitive text on screen.

### 3. Build — bottom, on the reaching hand's side

Only drawn when your own station is selected. Panel 560 × 320, flush to the bottom and its side edge.
Two rows, no scrolling, everything visible at once; a 1 px hairline on the top and inner edges with
registration ticks.

| Button | x | y | Cost |
|---|---|---|---|
| MINER | 896 | 656 | 150 |
| FIGHTER | 1032 | 656 | 300 |
| SHIPYARD L1 | 896 | 768 | 400 |
| SHIPYARD L2 | 1032 | 768 | 700 |
| ORE PROC L1 | 1168 | 768 | 350 |
| ORE PROC L2 | 1304 | 768 | 600 |

Every button is **120 × 96** with 16 px clearance. 120 rather than 96 wide because `SHIPYARD` at BODY
needs the room; the hit rect exceeds the under-fire tier in one axis, which is allowed.

Inside a button (offsets from the button):

| Element | x | y | w | h |
|---|---|---|---|---|
| index bar | +0 | +0 | 4 | 96 |
| name line 1 | +12 | +10 | — | 20 |
| name line 2 (L1/L2) | +12 | +32 | — | 20 |
| cost | +12 | +48 | 96 | 32 (DISPLAY, **right-aligned**) |

Item names are capped at **8 characters per line over two lines** (`SHIPYARD` / `L1`, `ORE PROC` / `L2`).
There is no text layout engine: every string's length is bounded by design, and no string wraps,
ellipsises or scrolls.

#### The four button states

| State | Plate | Border | Index bar | Name | Cost | Extra |
|---|---|---|---|---|---|---|
| **Live** | `PLATE` | 1 px `RULE.LIT` | `RULE.LIT` | `TEXT` | `TEXT` | — |
| **Unaffordable** | `PLATE` | 1 px `RULE.LIT` | `SIG.SHORT` | `TEXT` | `SIG.SHORT` | 3 px `SIG.SHORT` rule along the bottom edge (+0, +93, 120, 3) |
| **Unavailable** | `PLATE.DIM` | 1 px `#232C31` | `RULE` | `TEXT.DIM` | `TEXT.DIM` | 45° hatch: ~14 line segments, 1 px, 12 px apart, `rgba(90,106,114,0.22)`, inset 1 px |
| **Armed** | `PLATE.ARMED` | 2 px `SIG.ARMED` | `SIG.ARMED` | `#FFE1A8` | `#FFE1A8` | 4 px `SIG.ARMED` rule along the top edge; outline alpha pulses |

*Unaffordable* means "save up" — the plate stays lit, only the cost reddens. *Unavailable* means
"build something else first" — the whole button dims. A single grey for both is the specific failure
this distinction exists to prevent. Every state carries a **geometric** cue as well as a hue (index
bar, bottom rule, hatch, top bar), so the panel survives peripheral vision and colour blindness.

#### Progress strip

528 × 64 at (896, 880), 1 px `RULE` outline, own-team index bar at its leading edge.

- Item name at BODY, `TEXT`, at (912, 890).
- Percent at BODY, `TEXT.2`, right-aligned to x = 1308, same row.
- Track 396 × 12 at (912, 920): 1 px `RULE` keyline over `TRACK`, fill `TEAM.OWN`, quarter index
  ticks at 25/50/75% in `TICK.BAR`.
- **CANCEL** 96 × 64 at (1328, 880), combat tier, `SIG.SHORT` label and index bar.
- With nothing building: `SLOT EMPTY` at BODY right-aligned in the strip, empty track, and **the
  cancel target is not registered in the hit table**.

#### Placement

Choosing a module **arms a placement**. A second tap on the armed button disarms it. While armed, a
400-world-unit radius is drawn around the station; a tap inside it places the module, and a tap
outside it, on the station, or on another module does nothing. Cap of four modules per station.

**Known sharp edge, flagged rather than solved:** one queue slot serves both rows, credits are spent
at start, and all six buttons stay live while something is building — so tapping one replaces the
in-progress item and the spent credits are gone. Dimming the six would overload *unavailable* with a
second meaning. The fix is a refund-on-replace rule in `GameCore`, which is a simulation decision, not
an interface one. Raise it before shipping M2.

### 4. Damage alert — at the frame edge, in the direction of the event

"You are being attacked somewhere you cannot see." No audio and no minimap, so this is the only channel.
Derived client-side from fire events and the removal list (ADR-020) — **no wire bytes**.

Geometry for a **left-edge** indicator at along-edge position `ay`:

| Element | x | y | w | h |
|---|---|---|---|---|
| hit rect | 0 | ay | 120 | 96 |
| edge stripe | 0 | ay | 8 | 96 |
| direction triangle | 10 | ay+22 | 34 | 52 |
| count | 52 | ay+32 | 52 | 32 (DISPLAY, white, right-aligned) |

The triangle is one flat triangle with its apex outward — `(10, ay+48), (44, ay+22), (44, ay+74)`.
Top-edge indicators are the transpose: 96 × 120 hit rect, 96 × 8 stripe, 52 × 34 triangle, count
centred below. Right and bottom are mirrors.

**There is no scrim and no plate behind an alert.** The moment it looks like chrome it becomes chrome,
and the failure ADR-020 names is habituation.

Placement rules:

1. **Nothing appears if the event is already on screen.**
2. Edge is chosen by the **dominant axis of the bearing** from frame centre to the event.
3. The along-edge coordinate is where the centre→event ray crosses the frame, then **clamped** so the
   whole 96 px body stays on that edge *and* clears every panel band by ≥16 px.
4. At a corner the clamp leaves at most 45° of residual bearing error, which is inside the tolerance of
   "go and look".
5. **One indicator per cluster**, clustered by proximity and time, carrying a direction and a count and
   nothing else.
6. **At most three at once**, never closer than 112 px along an edge (96 body + 16 clear). Two clusters
   that would collide **merge and sum their counts**; a fourth replaces the oldest.
7. Tapping it recentres the camera there — a hit-test rectangle, not a gesture.

### 5. System — top centre, small

Panel 192 × 64 at (624, 0), centred on x = 720. Index bar, bottom hairline with registration ticks.

- Connection dot 12 × 12 at (640, 26), `TEAM.OWN` when linked.
- `LINK` at BODY, `TEXT.2`, at (660, 22). `RECONNECTING` replaces it on resume.
- **QUIT** 64 × 48 at (736, 8) — **floor tier, deliberately the smallest target on screen.**

#### The quit question — resolved: two taps, never one

There is no pause, no save and no rejoin-in-progress, so one stray contact ends a five-minute match with
no recovery path anywhere in the system. Tap one **arms**; tap two quits.

Armed state: the system panel expands in place to 256 × 152 at (592, 0).

- `NO SAVE. NO REJOIN.` at BODY, `SIG.SHORT`, centred, at (608, 48, 224, 20).
- **STAY** 96 × 64 at (608, 80) — lit plate, left, the safe choice under the reach.
- **QUIT** 96 × 64 at (720, 80) — outlined in `SIG.SHORT` only, not filled.
- The armed state **expires after 4,000 ms** and fades out over 200 ms, so it cannot be left lying on
  the glass.

The arming target stays at the 48 px floor rather than a comfort tier: it is the one control that should
be slightly hard to hit.

#### Overlays

Both draw over a full-frame scrim and suppress nothing else.

**Reconnecting** — scrim `#04060A` at 0.60; block 360 × 88 at (540, 436).
`RECONNECTING` at DISPLAY centred, `THE MATCH DID NOT WAIT` at BODY, `TEXT.2`, centred below.

**Result** — scrim `#04060A` at 0.72; block 560 × 224 at (440, 368).
`ENGAGEMENT ENDED` at BODY, `TEXT.2`, letter-spacing 0.14em, at (+32, +40).
A 32 × 32 winner team swatch at (+32, +88) and `TEAM n HOLDS THE FIELD` at DISPLAY at (+80, +88).
`NEXT MATCH IS SEEDING` and `YOU WILL BE DROPPED IN` at BODY on two explicitly positioned lines at
(+32, +152) and (+32, +180). **Two lines, because the renderer cannot wrap** — never one long string.
No button: the host reseeds and the client reconnects itself. The only live target is QUIT.

---

## World-anchored interface elements

These are drawn by the **interface pass** (so they belong to this handoff) but positioned by projecting
a world position through the world transform and then the interface fit (ADR-011, ADR-016). The
projection itself is scene work and is out of scope; what follows is what the interface pass draws once
it has a screen position.

| Element | Space | Geometry |
|---|---|---|
| **Hull bar on a damaged ship** | screen (billboarded) | 18 × 6 keyline `#0A0E12`, inner 16 × 4 at +1. Remaining portion `HULL.REST`, **lost portion `SIG.ALERT`**. Centred on the projected position, 14 px above the ship's screen half-height. **Fixed size at every depth.** |
| **Selection circle** | **screen** | r = **192 authored px**, 64-segment line strip, 2 px, `TEAM.OWN` at 0.5. Centred on the *ship*, not the finger. |
| **Order marker** | **world** | 120-unit square footprint on the plane — 4 line segments, 3 px — plus a 90-unit vertical riser and a 26 × 3 head tick. |
| **Order line** | world | 2 px, `TEAM.OWN` at 0.34, **one per selected design group** from that group's centroid to the marker base. Maximum four. |
| **Placement radius** | **world** | r = 400 world units on the plane, 48 segments, **24 drawn** (dashed), 2 px, `SIG.ARMED` at 0.70. |

Three rules that are easy to get wrong and are load-bearing:

1. **A ship at full hull draws nothing.** The map stays quiet until something is wrong. Up to 110 bars
   at once. Only the *lost* portion is bright, so a healthy fleet is a row of dark ticks and a dying one
   a row of red — loudness tracks severity for free.
2. **The selection circle is screen space and the placement radius is world space.** The first stays a
   true circle at every camera angle (which is what makes the camera the group-size control); the second
   projects to an ellipse. Dashing the second keeps them from being confused.
3. **The order marker is the single most latency-critical element on screen.** A tap is not visible on
   the ships for 152 ms at best and there is no cursor, so the client draws marker and line **the instant
   the gesture resolves** and clears them when the host acknowledges that command's sequence. Nothing is
   predicted — the ships do not move until the host says they did.

---

## Design tokens

### Palette (sRGB; convert to linear for the renderer)

The full machine-readable list is `palette.json`.

**Teams — the hue band 150–240° is reserved for the local player at every player count.**

| Token | Hex | Hue | Use |
|---|---|---|---|
| `TEAM.OWN` | `#38D1F5` | 193° | your fleet and station, order marker and lines, progress fill, panel index bars |
| `TEAM.B` | `#C4E838` | 78° | hostile A — the MVP opponent |
| `TEAM.C` | `#F75FD0` | 318° | hostile B (M4) |
| `TEAM.D` | `#A45CFF` | 268° | hostile C (M4) |

**State — the reserved red family, never a team colour.**

| Token | Hex | Use |
|---|---|---|
| `SIG.ALERT` | `#FF3B2F` | alert stripe and triangle; the lost portion of every hull bar |
| `SIG.SHORT` | `#FF5A4A` | unaffordable cost, its index bar and bottom rule; cancel; the quit confirm |
| `SIG.ARMED` | `#FFB020` | armed module button, placement radius, credit-spend flash |

**Chrome**

| Token | Value | Use |
|---|---|---|
| `SCRIM` | `#07090B` @ 0.88 | every panel fill |
| `PLATE` | `#0E1214` @ 0.92 | live build button, clear, cancel |
| `PLATE.DIM` | `#090C0E` @ 0.92 | unavailable build button, under the hatch |
| `PLATE.ARMED` | `#17120A` @ 0.94 | armed build button |
| `RULE` | `#313A40` | hairlines, bar keylines, cell outlines, dim index bars |
| `RULE.LIT` | `#5A6A72` | outline and index bar of a live target |
| `TICK` | `#8A9AA2` | registration ticks at rule ends and the frame quarter points |
| `TICK.BAR` | `#3A454B` | quarter index ticks inside hull and progress troughs |
| `TRACK` | `#1A2024` | hull trough, empty cargo chip, progress trough |

**Text**

| Token | Hex | Use |
|---|---|---|
| `TEXT` | `#E8ECEC` | counts, costs, credits, live button names — about 10:1 on scrim over the galaxy band |
| `TEXT.2` | `#93A0A5` | design names, labels, percent, link state |
| `TEXT.DIM` | `#6B7A80` | unavailable button text — 4.9:1, readable and plainly recessed |

**Bars**

| Token | Hex | Use |
|---|---|---|
| `HULL` | `#CFDDE0` | hull remaining, selection panel |
| `HULL.LOST` | `#FF3B2F` @ 0.85 | hull lost, selection panel and world |
| `HULL.REST` | `#2E3942` | hull remaining, world bars only — deliberately near-invisible |
| `ORE` | `#D8A23C` | filled cargo chip |

### Type

**Segoe UI only**, pinned; a missing family is a startup failure, not a substitution (ADR-009). Both
sizes are even, so the exact 2× fit rasterises them at 40 and 64 physical pixels with no doubling and no
resampling.

| Token | Authored | Physical | On glass | At 550 mm | Weight | Used by |
|---|---|---|---|---|---|---|
| `BODY` | 20 px | 40 px | 3.82 mm | ~24′ | **Semibold 600** | design names, build item names, panel labels, link state, percent, overlay sublabels |
| `DISPLAY` | 32 px | 64 px | 6.11 mm | ~38′ | **Semibold 600** | the credit balance, the build cost on each button, the selection group count, the alert count, overlay headlines |

Semibold at both sizes because the pass has no anti-aliasing and a Regular stem at 40 physical pixels on
a single-channel coverage atlas breaks up against a bright silhouette.

The longest string in the design is `TEAM 2 HOLDS THE FIELD` at 22 characters. Treat every text rect as
fixed and non-wrapping: an over-long string must **clip**, because a wrap is the one failure the renderer
cannot reproduce.

### Motion

Every curve is linear, a looping linear triangle, or a single exponential decay
`v(t) = target + (start − target)·e^(−t/τ)`. Nothing needs a spring or an easing table.

| Element | Property | From → To | Curve | ms |
|---|---|---|---|---|
| Alert body | alpha | 0 → 1 | step | 0 |
| Alert body | alpha | hold at 1 | — | 1200 |
| Alert body | alpha | 1 → 0 | linear | 2400 |
| Alert edge stripe | alpha | 1 ↔ 0.55 | linear triangle, during the hold only | 1200 period |
| Order marker riser | height | 0 → 90 world units | exponential, τ = 40 | 120 |
| Order marker + line | alpha | 1 → 0 on host ack | linear | 100 |
| Order line | alpha | 0 → 0.34 | step | 0 |
| Build progress fill | width | previous → snapshot value | linear | 50 (one snapshot interval) |
| Armed button outline | alpha | 1 ↔ 0.4 | linear triangle | 800 period |
| Placement radius | alpha | 0 → 0.70 | linear | 120 |
| Credit gain flash | alpha | 1 → 0 | exponential, τ = 120 | 400 |
| Credit spend flash | alpha | 1 → 0 | exponential, τ = 180 | 600 |
| Selection group count | — | — | **none — must not lag the gesture** | 0 |
| World hull bar | alpha | 1 → 0 on death | linear | 200 |
| Reconnect scrim | alpha | 0 → 0.60 | linear | 200 |
| Reconnect label | alpha | 1 ↔ 0.45 | linear triangle | 900 period |
| Result scrim | alpha | 0 → 0.72 | linear | 200 |
| Quit confirm | alpha | 1 → 0 on expiry | linear | 200, after a 4000 ms hold |

---

## Data the HUD may read

The client receives a full state snapshot 20 times a second and renders 75 ms behind it. **It knows only
this**, and a readout that needs anything else must not be built.

- **Per entity** (up to 110): identity; position; heading; **hull remaining 0–100**; design identity;
  owning team (1 of 4); one of 8 activity states; **cargo fill as one of 4 buckets**.
- **Per player**: **credits** (integer, starts at 1,000, typically 0–5,000, worst case five digits);
  last-acknowledged command sequence; **the design currently building and its progress 0–100**.
- **Per snapshot**: tick, entity count, player count, entities that died since the last snapshot.

**Not available at all:** income rate, resource per minute, kill counts, score, DPS, enemy credits, ore
remaining in a field, any historical series. Elapsed match time *is* derivable from the tick and is
deliberately not shown — the five surfaces are closed.

## Input the HUD must assume

Touch only. No mouse, pen, keyboard, cursor, hover, tooltip, right-click, long-press menu or text entry.
The gesture vocabulary is closed and **the HUD may not add to it**: tap, double-tap on your own ship,
one-finger drag (pan, unconditionally), two-finger pinch (zoom, coupled to pitch), two-finger rotate
(orbit), hold on empty space (recentre). A hold on a ship is deliberately unassigned.

Because one-finger drag already means pan, **there is no scrolling anywhere**: every panel fits its
worst case in fixed space. No lists, no carousels, no paging, no build queue list.

**Pick order** for a tap, because "what is under it" is not a total order: **any interface target first**,
then own ship, own station or module, hostile, asteroid, empty space. Nearest wins inside a tier; the
tier wins across one.

## State the HUD needs

| State | Source | Drives |
|---|---|---|
| selection (set of entity ids) | client-local | whether the selection panel is drawn; its groups, counts, aggregate hull, cargo bucket |
| station-selected | client-local | whether the build panel is drawn |
| armed module (or none) | client-local | armed button state; placement radius; what a world tap means |
| credits + previous credits | snapshot | the balance; the sign of the change flash |
| building design + progress | snapshot | progress strip; whether the cancel target is registered |
| available modules | snapshot (module entities) | live vs unavailable per button |
| alert clusters (≤3, with age) | derived from fire events + removal list | alert indicators |
| pending order (marker + sequence) | client-local | order marker and lines until the host acks the sequence |
| connection state | transport | link dot, reconnecting overlay |
| quit armed + arm time | client-local | confirm state, 4,000 ms expiry |
| handedness | configuration | the x' reflection |

## Suggested build order

1. **Primitive emitter and the hit table.** Rects with per-vertex colour and alpha, line segments, flat
   triangles, glyph quads. A hit table of rectangles with a tier tag. Nothing renders yet.
2. **The tier/clearance test.** Load `geometry.json`, assert every interactive rect meets its tier and
   every adjacent pair has ≥16 px clearance, in both handedness states. This is judged by a test, not by
   eye — write it before the panels.
3. **Credits and system.** The smallest surfaces; they prove the text path, the scrim, the hairline and
   the registration ticks.
4. **Selection panel**, including the group-count formula and the live update during double-tap.
5. **Build panel**, all four button states, the progress strip, arming and the placement radius.
6. **Damage alert**, including the clamp, the corner case and cluster merging.
7. **Overlays and the quit confirm.**
8. **Motion**, last — everything above should be correct static first.

## Acceptance

- Every interactive element meets its tier, with 16 px clearance, in both handedness states — asserted
  by a test over `geometry.json`.
- Every element is drawable with the four primitives; the only diagonals in the entire interface pass are
  the alert triangle and the unavailable hatch.
- The HUD is readable over the galaxy band at its 12% ceiling, and the fleet is still readable through the
  HUD.
- A player under fire can hit the right build button without looking carefully.
- The selection panel answers "what did I just select" while a hand covers the world.
- The alert gets noticed without being read.
- All coordinates are integers and the mirrored layout works.

## Files in this bundle

| File | What it is |
|---|---|
| `README.md` | this document — self-sufficient; implement from it |
| `CLAUDE_CODE_PROMPT.md` | a prompt to paste into Claude Code to start the work |
| `geometry.json` | every rectangle, hit rect, tier and mirrored x — intended for the assertion test |
| `palette.json` | every colour token with hex, alpha and usage |
| `Outpost Commander HUD.dc.html` | the design reference. Open in a browser. **The scene behind the HUD is not a specification.** |
| `frames/01-idle.png` | Frame 1 — nothing selected, early match. Credits and system only |
| `frames/02-combat.png` | Frame 2 — 30 selected across two groups, world hull bars, left-edge alert, order marker and lines |
| `frames/03-building.png` | Frame 3 — station selected, one unaffordable, two unavailable, one armed with its placement radius, item at 40% |
| `frames/04-left-handed-worst-case.png` | Frame 4 — handedness flipped, four selection groups, five-digit balance, three concurrent alerts including a corner clamp |

Each PNG is exactly 1440 × 960 — the authored frame at 1:1. Measure off them directly; a pixel in the
image is an authored pixel. The scene behind the HUD in these captures is illustrative only.

## Assets

None. There are no images, no icons and no icon font. Every mark in the design is geometry or a Segoe UI
glyph, and that is a hard constraint, not a simplification.
