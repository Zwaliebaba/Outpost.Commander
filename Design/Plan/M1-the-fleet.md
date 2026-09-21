# M1 — The fleet

[`GameDesign.md`](../GameDesign.md) §10: two stations, the two designs, the current build item, move orders
with ring assignment, selection by tap and by double tap, a generated sky behind it, two clients on one host, host-side command validation.

**What it proves** is the one thing M0 deliberately left out — that
[`ADR-006`](../ADR/ADR-006-a-ship-is-a-composition.md)'s component model carries the game rather than
merely existing beside it. Two ships and a station come out of one catalog with no special case anywhere,
and if that is going to be false it is false here, while it costs a few hundred lines to fix.

**Read [`README.md`](README.md) first.** Sixteen steps and two gates — **plus M1.4, which is neither: it
is a decision the design has not taken** (F2). Nothing tells an arriving client which player it is, and
everything after M1.3 needs to know, so it blocks the milestone exactly as a gate would.

**Entry state.** M0 complete: bytes cross the wire, the host ticks, the client draws one shape and taps
move it. Every suite carries real tests. One fixed seed, one entity, no ownership.

---

## The model

### M1.1 — The component catalog · `GameCore` · `GameCoreTests` · agent

**Read first:** `GameDesign.md` §6; ADR-006; R24; `TechnicalDesign.md` §7's last paragraph — the catalog is
`constexpr` tables in `GameCore`, not files, and a content format is post-MVP.

**Adds:** the four hulls, the two drives and the three slot components of `GameDesign.md` §6, as tables
referred to **by identity**. That indirection is the only property research needs (ADR-006), and it is the
whole reason the catalog is not three structs with baked numbers.

**The size-class enumerators cannot be spelled `Small` or `Large`.** `TechnicalDesign.md` §1 names this
trap: `<windows.h>` defines `small`, and the preprocessor rewrites it before the compiler sees it. Pick a
spelling — `Light`, `Medium`, `Heavy` reads well against `GameDesign.md` §7's table — and say in a comment
which column of that table each maps to, because the design's prose and the code will not match and a
reader will wonder whether that is a bug.

**Files:** `GameCore/Catalog.h` `.cpp`, `GameCore/SizeClass.h`; `GameCore.vcxitems` + `.filters`;
`Tests/GameCoreTests/CatalogTests.cpp`.

**Done when:** every identity resolves to exactly one entry; no entry is reachable by anything other than
its identity; and the catalog holds `Cruiser` although nothing builds it — `GameDesign.md` §6 keeps it
precisely so M4 is a table row.

### M1.2 — The design table and the derived-stat function · `GameCore` · `GameCoreTests` · agent

**Read first:** ADR-006 in full; R24; `GameDesign.md` §6.

**Adds:** **the single most important function in the game's rules** — one pure integer function that takes
a design and returns its stats. Mass is the hull plus its contents; speed is thrust over mass; cost and
build time are sums. A `Frigate` carrying two mass drivers is slower than an empty one **because of
arithmetic**, and nothing anywhere writes that down.

The three designs of ADR-006's table — Miner, Fighter, Station — are **rows**, not types. Nothing in the
simulation knows what a "fighter" is, and a `grep` for the word outside a comment or a display string is
this step failing.

**Files:** `GameCore/Design.h` `.cpp`, `GameCore/DerivedStats.h` `.cpp`; `GameCore.vcxitems` + `.filters`;
`Tests/GameCoreTests/DerivedStatsTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's requirement is met in full — **every catalog combination is
pinned, including every `Cruiser` one, which no MVP design uses.** The two shipped designs reproduce
`GameDesign.md` §6's table (150 credits at 100 u/s, 300 at 140); a hull with no drive derives a speed of
zero rather than dividing by something; and adding a component to a design lowers its speed, asserted as a
property rather than as a value.

### M1.3 — The entity gains an owner, a design and a hull · `GameCore`, `GameLogic` · both · agent

**Read first:** ADR-003's record layout; `TechnicalDesign.md` §4.

**Adds:** the fields M0.9 encoded as zeroes now carry meaning — owner in the flags, **design identity in
its own byte**, hull remaining as a percentage. The snapshot encoder does not change shape, which is the
point: M0.9 built the format and M1 fills it.

**Files:** `GameCore/Entity.h` `.cpp`, `GameCore/EntityRecord.cpp`; `GameLogic/World.cpp`;
`Tests/GameCoreTests/SnapshotTests.cpp` extended.

**Done when:** a design identity above four round trips intact — the two-bit packing ADR-003 corrected is
the exact defect this asserts against; hull percentage quantizes and dequantises within one percent; and
the entity's simulation heading stays 16-bit while the wire's stays 8.

### M1.4 — ADR-013: the join record · `GameCore`, `GameLogic`, `GameClient` · all three · **human**, then agent

**Read first:** `README.md` F2; `GameDesign.md` §2; `TechnicalDesign.md` §4 and §5; ADR-003; ADR-008.

**The decision first, and it is not the plan's to take.** `GameDesign.md` §2 configures the slots on the
host before the match starts, and `TechnicalDesign.md` §4 specifies the snapshot, the command and the
heartbeat — but **nothing tells an arriving client which of the per-player blocks is its own**, and nothing
says what a host does with a client it was not expecting. §5's "the protocol version in the header refuses
a mismatched build" implies a handshake that is defined nowhere.

Everything after this step needs the answer: selection needs to know which entities are mine, validation
needs to know who is sending, and the credits readout needs to know which block to read.

**What the ADR has to settle:** what a client sends on connect and what comes back; how a slot is claimed
and whether a client may choose; what happens to a second client claiming a taken slot; how a reconnecting
client (`GameDesign.md` §2, `Interface.md` §7) is recognized as the same player; whether the protocol
version check lives here or stays in every packet header; and **the match seed**.

**The seed was missing from this list and R23 already requires it.** The client runs the asteroid
generator itself — that is the whole of R23 — so it cannot draw a map without the seed, and the join
record is the only place it can arrive. [`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md)
is a second consumer and costs nothing extra because of it. Whether it also needs a reconnecting client to
get the *same* seed back is part of what this ADR settles.

**Files:** `Design/ADR/ADR-013-<slug>.md` and the row in `Design/ADR/README.md`; then
`GameCore/Join.h` `.cpp`, `GameLogic/Sessions.h` `.cpp`, and the client side of it; the project files and
`.filters`; tests in all three suites.

**Done when:** the ADR is Accepted; a client learns its player index and validates against it; a second
client on a taken slot is refused in a way the client can show; and a reconnect is recognized rather than
treated as a new player — which self-contained snapshots make cheap
([`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md)) and which nothing else makes correct.

---

## The match

### M1.5 — Two stations, placed · `GameCore`, `GameLogic` · `GameCoreTests` · agent

**Read first:** `GameDesign.md` §3 and §5; R23; `TechnicalDesign.md` §3.

**Adds:** the start anchors and a station on each. **M0 and M1 run one fixed seed with a hand-checked
layout** (`GameDesign.md` §3), so this is the placement M2's generator grows into rather than a throwaway:
it lives in `GameCore`, both sides run it, and it takes a seed it currently ignores. The station is a
`Station` hull with two `PointDefense` mounts and no drive, which under M1.2 needs no code of its own.

**Files:** `GameCore/Layout.h` `.cpp`; `GameCore.vcxitems` + `.filters`;
`Tests/GameCoreTests/LayoutTests.cpp`.

**Done when:** both sides produce identical placements from the same seed, asserted by a test that runs the
same function twice rather than by trust; and the station appears in snapshots as an ordinary entity with a
design identity, because it is one.

### M1.6 — The build, and the current item · `GameLogic`, `GameClient` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §5; `OpenQuestions.md` Q21 and Q35; ADR-003's per-player block.

**Adds:** a design selected, credits deducted **when the item starts**, the ship appearing at the spawn
point when it finishes. **There is no queue** — Q21 cut it because `Interface.md` had specified a
cancellable queue that no wire record could feed, and the snapshot carries the current item and its
progress in two bytes per player. No rally point; new ships sit where they appear.

**Files:** `GameLogic/BuildSystem.h` `.cpp`; `GameLogic.vcxproj` + `.filters`;
`Tests/GameLogicTests/BuildTests.cpp`.

**Done when:** an unaffordable order is refused at the host rather than at the client alone; progress is
monotonic in ticks and reaches completion exactly; canceling refunds what the design says it refunds —
**and the design did not say, so it is now `OpenQuestions.md` Q35 rather than a guess in a commit.** Q36
waits on it, because a client differencing credits to show an income rate is wrong by whatever a cancel
gives back.

### M1.7 — Ring slot assignment, and the determinism test · `GameLogic` · `GameLogicTests` · agent

**Read first:** `OpenQuestions.md` Q19; `TechnicalDesign.md` §1, §2 and §8; ADR-002's ordering paragraph;
R16.

**Adds:** what fifty ships do when ordered to one point — **a ring slot per ship, assigned at order time,
ordered by entity identity.** No separation force and no flocking: those are floating-point-shaped problems
in an integer simulation, and a formation system later is this same assignment with a different slot
layout. The cost is induced by [`ADR-001`](../ADR/ADR-001-the-playfield-is-a-plane.md) — in a volume ships
miss each other in the third dimension and on a plane they stack.

**And the determinism test, which `TechnicalDesign.md` §8 calls the most valuable test in the tree.** A
fixed tick count from a seed against a scripted order list, asserting the state hash M0.8 built.

**Files:** `GameLogic/RingAssignment.h` `.cpp`; `GameLogic.vcxproj` + `.filters`;
`Tests/GameLogicTests/RingAssignmentTests.cpp`, `DeterminismTests.cpp`.

**Done when:** the same selection ordered to the same point yields the same slots in the same order, across
runs and independent of the order the selection arrived in; a tie between two candidates at equal distance
breaks **on identity and never on which grid cell was visited first**; and the determinism test passes.
**Then run it on all four configuration and platform pairs by hand** — ADR-002's second owed measurement,
and `AGENTS.md` §6 guarantees nothing in CI ever will.

---

## What the commander sees

### M1.8 — The camera in full · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §5 and §3; `README.md` F6.

**Adds:** what M0.20 stubbed — two-finger pinch zooming and pitching together, two-finger rotate orbiting,
one- and two-finger drag panning identically, all through M0.20's single anchor solve
([`ADR-018`](../ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)). **Pitch is coupled to zoom and is not
separately controllable**, which removes a degree of freedom from a gesture budget that has very little
left and gives a near top-down tactical read at one end and a fleet in silhouette at the other. **There is
no minimap** and `Interface.md` §5 explains at length why maximum zoom-out already is one.

**Three things land here that M0.20 did not need:**

- **`pitch(distance)` saturates at the 30° floor rather than terminating the zoom range.** Read the
  coupling the wrong way and the floor silently becomes a zoom-in limit, which is not what it is for.
- **The zoom range's two ends.** The far end is arithmetic — 22,500 units shows the whole 16,384-unit
  square at a 40° field of view — and **the near end is this step's to pin**. At roughly 1,500 world units
  of close view the range is about 16×, which is two pinch gestures at unity gain. If it comes out much
  larger, the lever is a gain on the scale, not a different gesture.
- **Heading snaps to the nearest cardinal on release** within a stated threshold. There is no minimap and
  no compass; a map that is reliably north-up when nobody is deliberately turning it is what spatial
  memory is built on.

**And the recenter**: a hold on empty space moves the focus to the selection, or to your station when
nothing is selected (ADR-018, spending one of the two verbs ADR-017 freed). `GameDesign.md` §7 names the
problem it answers — a defender has to be watching the right part of a 16,384-unit map at the right
moment, and until now the only way back was panning there.

**Files:** `GameClient/Camera.cpp` extended, `GameClient/CameraGesture.h` `.cpp`;
`Tests/GameClientTests/CameraGestureTests.cpp`.

**Done when:** the rotation deadzone holds **and latches** — two fingers dragging to pan are never exactly
parallel, a camera that yaws whenever you pan is unusable, and one that stutters as the player crosses
back under eight degrees is worse; the **2% scale deadzone** holds, so a pure orbit does not creep the
zoom and therefore the pitch; pitch tracks zoom monotonically with both ends pinned **and never goes below
`Interface.md` §5's 30° floor at a 40° field of view**; and the pan clamp holds at the corners of the play
area plus its margin.

**The floor is the number to look at on the device, not just to assert.** It is what bounds tap error near
the top of the frame, what bounds ADR-010's wedge, and what stops an anchor near the horizon demanding an
unbounded focus movement (ADR-018). It trades directly against how raking the zoomed-in silhouette looks.
If it is too high to look good, say so with the stretch ratio in hand rather than lowering it quietly.

**Two things here are looked at rather than asserted, and both are on `Interface.md` §7's list.** Whether
the ground actually sticks across the pitch range — the failure is drift over a long gesture. And
**whether orbit is usable one-handed on a kickstand**, where a thumb-and-index rotation has about 50° of
arc before a re-grip. That second one decides whether orbit survives: `Interface.md` §5 names it the
candidate to cut, and cutting it removes the 8° deadzone, its latch, the 2% scale deadzone and the snap —
four constants — and makes pinch pure zoom. **If it is bad, say so; it is designed to be cuttable.**

### M1.9 — The hulls, as meshes · `NeuronClient`, `GameClient` · hand · agent

**Read first:** [`ADR-005`](../ADR/ADR-005-meshes-are-generated-in-code.md) in full, including the shape
list its status line corrects; `OpenQuestions.md` **Q37**, because a hull has no stated size and this step
cannot emit one without agreeing a number; `TechnicalDesign.md` §6 and §7; R9; R14.

**Adds:** geometry from functions. **The split is R9's:** `NeuronClient` gets the vertex and index buffers,
the upload and the instanced draw, which know nothing about a game; `GameClient` gets the function that
knows what a `Scout` looks like. Normals baked per face onto split vertices — flat shading, no smoothing
group to decide and no tangent basis to get wrong. Team color is a vertex attribute selecting between a
hull palette and the owner's color, so one instanced draw covers every ship of a hull regardless of owner.

**ADR-005 names the risk and it is this step's to watch: two hulls from one parameterised function tend to
be the same shape at two scales**, which is exactly unreadable at the zoom where identification matters
most. The function must be parameterised for **divergent proportion** — a wide flat hull against a long
narrow one — and not merely for size.

**Files:** `NeuronClient/MeshBuffer.h` `.cpp`, `NeuronClient/InstancedDraw.h` `.cpp`;
`GameClient/HullMesh.h` `.cpp`, `GameClient/WorldPass.h` `.cpp`, `GameClient/Ship.hlsl`; both project files
and `.filters`.

**Done when:** **the two ship hulls this milestone builds — `Scout` and `Frigate` — and the station** draw
as one instanced call each with a per-instance transform and team color, at the sizes Q37 settles; the
shared function reaches `ModuleFrame` and `Cruiser` by parameter without either being drawn here; and
**the silhouettes are looked at from the tactical zoom**, which is a screen and not a test.

### M1.9b — The sky · `NeuronClient`, `GameClient` · `NeuronClientTests` · agent

**Read first:** [`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md) in full;
[`ADR-005`](../ADR/ADR-005-meshes-are-generated-in-code.md), which it amends;
[`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md); R9, R14 and R23.

**Adds:** the backdrop, in two halves with two frequencies. **The galaxy** bakes once into a 512² cubemap,
6.3 MB, rendered at match start and sampled with one fetch per pixel — a band that is wider and brighter
toward the galactic center and carries **dark dust lanes**, because a band without them is a stain rather
than a galaxy. **The stars** are about 3,000 instanced quads from `SV_VertexID` with no vertex buffer,
seeded from the match.

**What makes it read as a sky is four properties, and each has a way of failing that is worth knowing:**

- **Magnitude tiers in the ratio 1 : 3 : 9 : 27 : 81 : 243** — roughly 8, 25, 74, 222, 667 and 2,004
  stars. The brightest tier being *eight* is the whole effect. Uniform brightness reads as salt and
  pepper.
- **Size follows brightness**, 8 scene-target pixels down to 1.5, with a soft radial falloff in the
  sprite. Apparent size is the point-spread function, not the star.
- **Color is blackbody, desaturated to about 20%.** Oversaturated tints are how a procedural sky
  announces itself; real stars read very nearly white.
- **Temperature correlates with magnitude** — bright tiers blue-white, faint ones orange — and star
  density rises toward the galactic plane. Draw color independently of brightness and the sky is subtly,
  unnameably wrong.

**Nothing twinkles and nothing moves.** There is no atmosphere, so there is no scintillation; diffraction
spikes are a telescope artifact and the player is not looking through one. **The sky takes no time input
at all** — generated once, never updated, zero per-frame CPU. Do not add "a little movement" later without
reopening ADR-019.

**It is dim, and the ceiling is on area rather than peak**: large-area luminance never above **12% of full
white**, point features up to 45% and only for the brightest tier. ADR-005 leans on the backdrop being
black to make a few-dozen-triangle hull read as deliberate, and this is the number that keeps that true.

**The R9 split is the usual one.** `NeuronClient` gets the render-to-cubemap facility, the instanced
sprite draw, the blackbody table and the seeded point-field generator — none of which knows there is a
game. `GameClient` gets what *this* sky looks like: the band's orientation, the star count, the tier
ratios, the palette and the ceiling. **Nothing goes in `GameCore` or `GameLogic`**, and the sky being
floats and noise throughout means `Scripts/CheckDeterminism.py` catches it if anyone tries.

**Draw it last, with depth test on**, so it shades no pixel the fleet already covers and pays no
multisample resolve on a surface with no edges.

**Files:** `NeuronClient/CubemapBake.h` `.cpp`, `NeuronClient/PointSprites.h` `.cpp`,
`NeuronClient/Blackbody.h` `.cpp`, `NeuronClient/StarField.h` `.cpp`, `NeuronClient/Sky.hlsl`,
`NeuronClient/Star.hlsl`; `GameClient/SkyLook.h` `.cpp`; both project files and `.filters`;
`Tests/NeuronClientTests/BlackbodyTests.cpp`, `StarFieldTests.cpp`.

**Done when:** the blackbody table is **pinned exactly** at its eight stops and between them; a given seed
produces the magnitude tiers in the stated ratio and **the same sky twice**; the cubemap bakes in one pass
over six faces; and the whole thing is **looked at on the device at the tactical zoom** — which is where
ADR-005's worry lands and is M1.16's to answer, not this step's.

### M1.10 — Selection by tap · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §4; [`ADR-010`](../ADR/ADR-010-selection-is-proximity-and-design.md);
ADR-001.

**Adds:** a tap's meaning coming from what is under it, which is the only way to have a verb with no
modifier key. `Interface.md` §4's table is the whole specification: empty space moves, a hostile attacks,
an asteroid mines, your own station opens the build panel, your own ship replaces the selection. A tap on
empty space with **nothing** selected does nothing.

At 110 entities the hit test is a linear scan over the replica store. **Do not build a spatial index on the
client** — the simulation's grid is `GameLogic`'s (M2.5) and a second one here would be a second thing to
keep correct for no measured gain.

**Files:** `GameClient/Selection.h` `.cpp`, `GameClient/HitTest.h` `.cpp`;
`Tests/GameClientTests/HitTestTests.cpp`.

**Done when:** a tap resolves to the nearest candidate within **`Interface.md` §1's 24-pixel pick radius**
at several camera pitches — the radius is now stated rather than left to this step; **the tier order is
pinned** against overlapping candidates of different kinds (own ship → own station or module → hostile →
asteroid → empty); the empty-space and nothing-selected cases are pinned; and a tap that lands on two
overlapping ships resolves the same way twice.

### M1.11 — Selection by double tap · `GameClient` · `GameClientTests` · agent

**Read first:** [`ADR-017`](../ADR/ADR-017-group-selection-is-a-double-tap.md) **before** ADR-010, which
it amends; `Interface.md` §3 and §4; `OpenQuestions.md` Q8.

**Adds:** a second tap on one of your ships taking **every ship of the same design within a circle centered
on it** — screen-space, 192 authored pixels, drawn during the gesture, centered on the *ship* because the
finger is covering it, own ships only, same design only, fixed radius. **The camera is the group-size
control**: because the circle is screen-space, zooming in takes a squad and zooming out takes the fleet.

**The structure that matters is that nothing defers.** M1.10's single tap already selected one ship and
already fired; this step only ever *upgrades* that result when a second tap within 300 ms resolves to the
**same entity**. Matching on identity rather than on screen distance is what makes it work while the fleet
is moving, which is when it is used. Build it as an upgrade to an existing selection and no tap in the
game gets slower; build it as a decision between two outcomes and every tap does.

**It replaced a hold** — ADR-010 rejected hold-and-drag for a latency its own plain hold then paid, and a
hold asks a hand to stay still for half a second on a handheld device mid-fight. `Holding` is now free
over ships as well as over empty space, and `Interface.md` §7 is deliberately not spending either yet.

**"Same design" is only a coherent idea because ADR-006 made a design a first-class identity**, and that is
why this step sits after M1.2 rather than beside M1.10.

**Files:** `GameClient/GroupSelection.h` `.cpp`; `Tests/GameClientTests/GroupSelectionTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's requirement is met — which ships a 192-pixel radius takes at
several zoom levels, **a ship exactly on the edge**, and **the raking-camera case where the circle's world
footprint is a wedge**, now bounded by §5's pitch floor. Plus the three ADR-017 cases: **one tap selects
one ship and expands nothing**; a second tap on the *same* entity within the window expands; and two taps
on **different** ships stay two single taps rather than becoming an expansion. ADR-010 calls that last one the single place where two accepted decisions interact
badly: the player sees a circle and gets a wedge, and the mitigation, if M1.16 says it is bad, is a radius
defined on the plane rather than on the screen.

### M1.12 — The glyph atlas · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** [`ADR-009`](../ADR/ADR-009-text-is-directwrite-into-an-atlas.md) in full; ADR-011;
`Interface.md` §6; R12 and R14.

**Adds:** DirectWrite rasterising Segoe UI into a D3D12 atlas we own, at startup.
`DWriteCreateFactory`, `GetSystemFontCollection`, `CreateGlyphRunAnalysis`, then `GetAlphaTextureBounds`
and `CreateAlphaTexture` for coverage bytes we upload ourselves. **No Direct2D and no `ID3D11On12Device`**
— R12 bans both by name, which closes the route every D3D12 text sample takes.

**Two details a naive implementation gets wrong, both stated in ADR-009 so they need not be rediscovered:**
coverage is rasterised as ClearType and the three subpixel values averaged into one channel, because
subpixel output would arrive as color fringing after the scale — **and the average is taken in linear
space, not on the stored gamma-encoded bytes**, or stems come out systematically thin or fat. And **a
missing font family is a startup failure rather than a substitution**, because a substituted font has
different advance widths and R13 requires every layout number to be unconditional.

**Glyphs are rasterised at the physical size the INTERFACE fit transform produces** (ADR-011, corrected by
[`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md): there are two, and this is not the world's)
— 48 physical pixels
for a 24-authored-pixel label on the target device — so **the atlas is sized against the window**, and a
resize or a device removal invalidates it. Both are rebuild paths, and neither may stall a frame visibly.

**Files:** `NeuronClient/GlyphAtlas.h` `.cpp`, `NeuronClient/AtlasPacker.h` `.cpp`;
`NeuronClient.vcxproj` + `.filters`; `Tests/NeuronClientTests/AtlasPackerTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's two are pinned — **atlas packing**, and that **a glyph's advance
width survives the round trip**; the linear-space average is asserted against a hand-computed value rather
than against itself; and the rebuild path runs without a visible stall, which is a screen rather than a
test.

### M1.13 — The text renderer · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** ADR-009; ADR-011; `Interface.md` §6.

**Adds:** instanced glyph quads in the interface pass, in authored coordinates through M0.12's transform.
Two sizes — a body size for readouts and a larger one for the build buttons — both authored, both
rasterised at the physical size the transform produces. **Text is therefore neither doubled nor
resampled**, which is what ADR-011 bought and what ADR-009's original pixel doubling cost.

**Files:** `NeuronClient/TextRenderer.h` `.cpp`, `NeuronClient/Glyph.hlsl`; `NeuronClient.vcxproj` +
`.filters`; `Tests/NeuronClientTests/TextLayoutTests.cpp`.

**Done when:** a string's laid-out advance widths are pinned; the same string at both sizes lands at the
same authored origin; and a string draws at the physically correct place, confirmed once by looking.

### M1.14 — The four panels · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §6 and §1, including *Where the geometry lives*;
[`design_handoff_hud/README.md`](../design_handoff_hud/README.md) in full; R14, R18 and R20.

**Adds:** credits top left; selection along the bottom **away from the reaching hand**, grouped by design
with a count and a hull bar — the cargo bar `Interface.md` §6 draws beside it arrives with mining at M2.7 —
where tapping a group narrows the selection and a clear target deselects everything, **the only way to
deselect**, because a tap on empty space is already a move order; build along the bottom **on the reaching
hand's side**, visible when your station is selected, two targets with their costs, grayed when
unaffordable, the current item and its progress below them and tappable to cancel; and system top center,
carrying connection state, the reconnecting overlay, the result overlay and the quit, **which arms on
the first tap and quits on the second and disarms itself after four seconds** (`Interface.md` §6).

**Nothing here is a Windows Runtime control.** There is no XAML anywhere in this tree (R18), so a panel is
geometry and text the renderer draws and a "button" is a rectangle the hit test knows about. **All three of
`Interface.md` §1's tiers appear in this step**, and it derives each from the panel rather than assuming
it: the **48 × 48** floor with **16 pixels of clear space** under anything interactive, the **64 × 64**
combat tier on the selection panel's design groups, its clear target and the build item's cancel, and
**96 × 96** on the build buttons. A control smaller than its tier is a defect rather than a style choice.

**The geometry is no longer this step's to invent.** `design_handoff_hud/` states every rectangle in
integer authored coordinates, with the palette, the two type sizes and the motion table, and
`geometry.json` is the machine-readable form carrying each rect's hit box, its tier and its mirrored x.
**Take the numbers from there rather than deriving them**, and take the build order with them — the
emitter and the hit table first, then the tier test, then credits and system, then selection, then build.
`Interface.md` §6 *Where the geometry lives* says which document wins where the two overlap.

**R14 decides how the test reads that file, and the answer is not in the test.** The handoff asks for a
suite that loads `geometry.json` and asserts the two rules, but the dependency list is closed — the
Windows SDK, the MSVC standard library and `Microsoft.Windows.CppWinRT` — so there is **no JSON parser
in this tree and adding one is a decision rather than a convenience.** The shape that costs nothing is
the one `Scripts/` already uses five times over: **the suite asserts over the constants in
`HudLayout.h`, and a gate compares those constants against `geometry.json`.** Python reads the JSON,
C++ reads none, and the two cannot drift without something failing.

**The bottom edge is deliberate; which corner is not settled here.** `Interface.md` §1 reversed the posture
this step was first written against — a Surface Pro is used on a kickstand with index fingers rather than
held at its sides — so the binding constraint is **occlusion** rather than reach: a reaching hand covers
its target and a wedge of screen around it. That is why the selection panel sits opposite the build panel;
it is the readout the player reads while their hand is on the glass. **Which side each takes is
`OpenQuestions.md` Q33, open and settled at M1 by playing**, so this step cannot hard-code a side.

**Files:** `GameClient/Panels.h` `.cpp`, `GameClient/HudLayout.h` `.cpp`, `GameClient/PanelHitTest.h`
`.cpp`; `GameClient.vcxproj` + `.filters`; `Tests/GameClientTests/HudLayoutTests.cpp`;
`Scripts/CheckHudGeometry.py` **extended** — it already gates `geometry.json` against its own rules
and against `Interface.md` §1, and this step adds the half that compares those rects to the
constants the client actually draws from.

**Done when:** **every interactive target is asserted at its own tier with its clear space, in both
handedness states** — a test, not a measurement by eye, because this is the rule that erodes one control
at a time, and a combat-tier target passing on the 48 floor is the way it erodes; **the gate agrees with
`geometry.json` rect for rect**, so the drawn interface and the designed one cannot diverge silently; a
tap inside a panel never reaches the world; **the two bottom panels swap sides on one value**, so Q33
costs a setting rather than a rewrite; and the selection panel's grouping matches what M1.11 selected.

---

## The gates

### M1.15 — GATE: two clients on one host · — · hand · **human**

**Read first:** `README.md` F5; `GameDesign.md` §2 and §10; ADR-008.

`GameDesign.md` §10 asks for two clients on one host, and **a packaged application is single-instanced**.
Running a second instance on one machine is a manifest declaration (`SupportsMultipleInstances`, in the
`desktop4`/`iot2` namespace — confirm the current form before relying on it) which interacts with ADR-008's
already-strained loopback exemption. The alternative is a second machine, which is not an engineering
decision at all.

**Done when:** the question is answered on the register, and two clients on one host are playing — on
whatever number of machines the answer turned out to require.

### M1.16 — GATE: the confirmations · — · hand · **human**

**Read first:** `Interface.md` §7's closing section; `TechnicalDesign.md` §9.6; `OpenQuestions.md` Q18 and
ADR-010's consequences.

Three things the design says are checked by a hand rather than an argument, all owed here. `Interface.md`
§7 carries the first and third; `Design/README.md`'s *What is checked by a hand* carries the second:

1. **Whether 192 pixels is the right circle** (ADR-010, `Interface.md` §4) — and specifically **whether the
   raking-camera case selects the wrong ships**, since a circle on screen is a wedge in the world. The
   mitigation if it is bad is already designed and is one number.
2. **Whether the text reads** at 24 authored pixels on the target device. There is **no doubling** —
   [`ADR-011`](../ADR/ADR-011-the-interface-draws-after-the-scale.md) moved the interface out of the scene
   target, so a glyph is rasterised at the physical size the *interface* fit transform produces — and the
   thing to
   confirm is simply legibility. *(This step originally recorded that `Design/README.md` still called the
   confirmation "pixel-doubled"; that sentence was corrected on 2026-09-20, along with three others from
   the same source. See `README.md` F4.)*
3. **Whether the interface pass costs more GPU time than the world pass** — §9.6, which the design predicts
   it will: five instanced draws of simple geometry against an unbatched quad per glyph.

**The list has grown past three and this step carries all of it.** `Interface.md` §7 now closes with
seven, and two ADRs added their own since this step was written:

4. **The gesture constants** (`Interface.md` §1) — the 16-pixel tap slop above all, since it decides how
   often an intended order becomes a pan.
5. **Whether the ground sticks to the finger** across the pitch range, and **whether orbit is usable
   one-handed on a kickstand** ([`ADR-018`](../ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)). The
   second decides whether orbit and its three protecting constants survive.
6. **The frame time with the sky present**, at both world scales
   ([`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md)) — this is the measurement
   [`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md) actually needs, because a black screen
   was never the content. **Whether the fleet still reads against it** at the tactical zoom, which is
   ADR-005's worry. And **whether the sky looks like a sky**, whose three failure modes each have a named
   cause: uniform brightness reading as noise, oversaturated color as confetti, a band without dust lanes
   as a stain.

**Done when:** all of them are answered on hardware and written into the documents that asked for them.

---

## Leaving M1

**The milestone is finished when** two commanders on one host each build miners and fighters from one
catalog, select them by tap and by double tap, and order them somewhere they arrive in formation; the interface
reads; and `GameCoreTests` covers every catalog combination including the `Cruiser` nothing builds.

**What M1 produces besides code:** ADR-013; the register's answer on the second client; the three
confirmations above; the determinism test run on all four pairs for the first time; and §9.6 struck
through.

**The thing to watch** is M1.2. If two ships and a station do **not** fall out of one catalog with no
special case — if a `if (design == Station)` appears anywhere in the simulation — then ADR-006's central
claim is wrong and it is worth saying so loudly while the tree is still small enough to act on it.
