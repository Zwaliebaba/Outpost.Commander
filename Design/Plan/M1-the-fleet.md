# M1 — The fleet

[`GameDesign.md`](../GameDesign.md) §10: two stations, the two designs, the current build item, move orders
with ring assignment, selection by tap and by hold, two clients on one host, host-side command validation.

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
the exact defect this asserts against; hull percentage quantises and dequantises within one percent; and
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
client (`GameDesign.md` §2, `Interface.md` §7) is recognised as the same player; and whether the protocol
version check lives here or stays in every packet header.

**Files:** `Design/ADR/ADR-013-<slug>.md` and the row in `Design/ADR/README.md`; then
`GameCore/Join.h` `.cpp`, `GameLogic/Sessions.h` `.cpp`, and the client side of it; the project files and
`.filters`; tests in all three suites.

**Done when:** the ADR is Accepted; a client learns its player index and validates against it; a second
client on a taken slot is refused in a way the client can show; and a reconnect is recognised rather than
treated as a new player — which self-contained snapshots make cheap
([`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md)) and which nothing else makes correct.

---

## The match

### M1.5 — Two stations, placed · `GameCore`, `GameLogic` · `GameCoreTests` · agent

**Read first:** `GameDesign.md` §3 and §5; R23; `TechnicalDesign.md` §3.

**Adds:** the start anchors and a station on each. **M0 and M1 run one fixed seed with a hand-checked
layout** (`GameDesign.md` §3), so this is the placement M2's generator grows into rather than a throwaway:
it lives in `GameCore`, both sides run it, and it takes a seed it currently ignores. The station is a
`Station` hull with two `PointDefence` mounts and no drive, which under M1.2 needs no code of its own.

**Files:** `GameCore/Layout.h` `.cpp`; `GameCore.vcxitems` + `.filters`;
`Tests/GameCoreTests/LayoutTests.cpp`.

**Done when:** both sides produce identical placements from the same seed, asserted by a test that runs the
same function twice rather than by trust; and the station appears in snapshots as an ordinary entity with a
design identity, because it is one.

### M1.6 — The build, and the current item · `GameLogic`, `GameClient` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §5; `OpenQuestions.md` Q21; ADR-003's per-player block.

**Adds:** a design selected, credits deducted **when the item starts**, the ship appearing at the spawn
point when it finishes. **There is no queue** — Q21 cut it because `Interface.md` had specified a
cancellable queue that no wire record could feed, and the snapshot carries the current item and its
progress in two bytes per player. No rally point; new ships sit where they appear.

**Files:** `GameLogic/BuildSystem.h` `.cpp`; `GameLogic.vcxproj` + `.filters`;
`Tests/GameLogicTests/BuildTests.cpp`.

**Done when:** an unaffordable order is refused at the host rather than at the client alone; progress is
monotonic in ticks and reaches completion exactly; cancelling refunds what the design says it refunds —
**and if the design does not say, that is a register question rather than a guess in a commit.**

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
one- and two-finger drag panning identically. **Pitch is coupled to zoom and is not separately
controllable**, which removes a degree of freedom from a gesture budget that has very little left and gives
a near top-down tactical read at one end and a fleet in silhouette at the other. **There is no minimap**
and `Interface.md` §5 explains at length why maximum zoom-out already is one.

**Files:** `GameClient/Camera.cpp` extended, `GameClient/CameraGesture.h` `.cpp`;
`Tests/GameClientTests/CameraGestureTests.cpp`.

**Done when:** the rotation deadzone holds — two fingers dragging to pan are never exactly parallel and a
camera that yaws whenever you pan is unusable; pitch tracks zoom monotonically with both ends pinned; and
the pan clamp holds at the corners of the play area plus its margin.

### M1.9 — The hulls, as meshes · `NeuronClient`, `GameClient` · hand · agent

**Read first:** [`ADR-005`](../ADR/ADR-005-meshes-are-generated-in-code.md) in full; `TechnicalDesign.md`
§6 and §7; R9; R14.

**Adds:** geometry from functions. **The split is R9's:** `NeuronClient` gets the vertex and index buffers,
the upload and the instanced draw, which know nothing about a game; `GameClient` gets the function that
knows what a `Scout` looks like. Normals baked per face onto split vertices — flat shading, no smoothing
group to decide and no tangent basis to get wrong. Team colour is a vertex attribute selecting between a
hull palette and the owner's colour, so one instanced draw covers every ship of a hull regardless of owner.

**ADR-005 names the risk and it is this step's to watch: two hulls from one parameterised function tend to
be the same shape at two scales**, which is exactly unreadable at the zoom where identification matters
most. The function must be parameterised for **divergent proportion** — a wide flat hull against a long
narrow one — and not merely for size.

**Files:** `NeuronClient/MeshBuffer.h` `.cpp`, `NeuronClient/InstancedDraw.h` `.cpp`;
`GameClient/HullMesh.h` `.cpp`, `GameClient/WorldPass.h` `.cpp`, `GameClient/Ship.hlsl`; both project files
and `.filters`.

**Done when:** three hulls and a station draw as one instanced call each with a per-instance transform and
team colour; and **the silhouettes are looked at from the tactical zoom**, which is a screen and not a test.

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

**Done when:** a tap resolves to the nearest ship within a stated screen-space radius at several camera
pitches; the empty-space and nothing-selected cases are pinned; and a tap that lands on two overlapping
ships resolves the same way twice.

### M1.11 — Selection by hold · `GameClient` · `GameClientTests` · agent

**Read first:** ADR-010 in full; `Interface.md` §3 and §4; `OpenQuestions.md` Q8.

**Adds:** a hold on one of your ships taking **every ship of the same design within a circle centred on
it** — screen-space, 192 authored pixels, drawn while the finger is down, centred on the *ship* because the
finger is covering it, own ships only, same design only, and the radius does not grow with the hold.
**The camera is the group-size control**: because the circle is screen-space, zooming in takes a squad and
zooming out takes the fleet.

**"Same design" is only a coherent idea because ADR-006 made a design a first-class identity**, and that is
why this step sits after M1.2 rather than beside M1.10.

**Files:** `GameClient/HoldSelection.h` `.cpp`; `Tests/GameClientTests/HoldSelectionTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's requirement is met — which ships a 192-pixel radius takes at
several zoom levels, **a ship exactly on the edge**, and **the raking-camera case where the circle's world
footprint is a wedge.** ADR-010 calls that last one the single place where two accepted decisions interact
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
subpixel output would arrive as colour fringing after the scale — **and the average is taken in linear
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

**Read first:** `Interface.md` §6 and §1; R18 and R20.

**Adds:** credits top left; selection bottom left in the thumb zone, grouped by design with a count and a
hull bar, where tapping a group narrows the selection and a clear target deselects everything — **the only
way to deselect**, because a tap on empty space is already a move order; build bottom right, visible when
your station is selected, two targets with their costs, greyed when unaffordable, the current item and its
progress below them and tappable to cancel; and system top centre, carrying connection state, the
reconnecting overlay, the result overlay and the one button that quits.

**Nothing here is a Windows Runtime control.** There is no XAML anywhere in this tree (R18), so a panel is
geometry and text the renderer draws and a "button" is a rectangle the hit test knows about. **Every target
is at least 48 × 48 authored pixels with 12 pixels of clear space, and the build buttons are 96 × 96** —
`Interface.md` §1 derives both from the panel rather than assuming them, and a control smaller than the
minimum is a defect rather than a style choice.

**The frequently used controls are in the bottom corners deliberately**, because a tablet is held at its
sides and the middle of the bottom edge is where a held tablet's thumbs cannot reach.

**Files:** `GameClient/Panels.h` `.cpp`, `GameClient/HudLayout.h` `.cpp`, `GameClient/PanelHitTest.h`
`.cpp`; `GameClient.vcxproj` + `.filters`; `Tests/GameClientTests/HudLayoutTests.cpp`.

**Done when:** **every interactive target is asserted at 48 × 48 or larger with its clear space** — a test,
not a measurement by eye, because this is the rule that erodes one control at a time; a tap inside a panel
never reaches the world; and the selection panel's grouping matches what M1.11 selected.

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

### M1.16 — GATE: the three confirmations · — · hand · **human**

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

**Done when:** all three are answered on hardware and written into the documents that asked for them.

---

## Leaving M1

**The milestone is finished when** two commanders on one host each build miners and fighters from one
catalog, select them by tap and by hold, and order them somewhere they arrive in formation; the interface
reads; and `GameCoreTests` covers every catalog combination including the `Cruiser` nothing builds.

**What M1 produces besides code:** ADR-013; the register's answer on the second client; the three
confirmations above; the determinism test run on all four pairs for the first time; and §9.6 struck
through.

**The thing to watch** is M1.2. If two ships and a station do **not** fall out of one catalog with no
special case — if a `if (design == Station)` appears anywhere in the simulation — then ADR-006's central
claim is wrong and it is worth saying so loudly while the tree is still small enough to act on it.
