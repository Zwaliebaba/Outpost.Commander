# M1 — The fleet

[`GameDesign.md`](../GameDesign.md) §10: two stations, the two designs, the current build item, move orders
with ring assignment, selection by tap and by double tap, a generated sky behind it, two clients on one host, host-side command validation.

**What it proves** is the one thing M0 deliberately left out — that
[`ADR-006`](../ADR/ADR-006-a-ship-is-a-composition.md)'s component model carries the game rather than
merely existing beside it. Two ships and a station come out of one catalog with no special case anywhere,
and if that is going to be false it is false here, while it costs a few hundred lines to fix.

**Read [`README.md`](README.md) first.** Seventeen steps and two gates — **plus M1.4, which was neither: it
was a decision the design had not taken** (F2). Nothing told an arriving client which player it was, and
everything after M1.3 needs to know, so it blocked the milestone exactly as a gate would. It is taken:
[`ADR-013`](../ADR/ADR-013-a-client-is-told-which-player-it-is.md), Accepted 2026-09-22.

**Entry state.** M0 complete: bytes cross the wire, the host ticks, the client draws one shape and taps
move it. Every suite carries real tests. One fixed seed, one entity, no ownership.

---

## The model

### M1.1 — The component catalog · `GameCore` · `GameCoreTests` · agent

**Read first:** `GameDesign.md` §6; ADR-006; R24; `TechnicalDesign.md` §7's last paragraph — the catalog is
`constexpr` tables in `GameCore`, not files, and a content format is post-MVP.

**Adds:** `GameDesign.md` §6's catalog — **five hulls**, two drives, three slot components and four
module ones — as tables referred to **by identity**. The hull count is the section's own and includes the
two that are not ships: the station is a hull and so is a module frame, which is what §5 is built on. That indirection is the only property research needs (ADR-006), and it is the
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

**Files:** `GameCore/Entity.h`, `GameCore/EntityRecord.h` `.cpp`; `GameLogic/World.h` `.cpp`,
`GameLogic/Host.cpp`; `GameClient/Interpolation.h`; `Tests/GameCoreTests/SnapshotTests.cpp` extended and
the `GameLogic` suites swept.

**`Entity.cpp` IS NOT IN THAT LIST BECAUSE THERE IS NO SUCH FILE** — `Entity` is a header-only aggregate
and this step did not give it a reason to stop being one. `EntityRecord.h` is in it because the heading
quantizer M0.19 left in `GameClient` moved here, beside `QuantizePosition`, the day the host needed it.

**Done when:** a design identity above four round trips intact — the two-bit packing ADR-003 corrected is
the exact defect this asserts against; hull percentage quantizes and dequantises within one percent; and
the entity's simulation heading stays 16-bit while the wire's stays 8.

### M1.4 — ADR-013: the join record · `GameCore`, `GameLogic`, `GameClient` · all three · **human**, then agent

**Read first:** `README.md` F2; `GameDesign.md` §2; `TechnicalDesign.md` §4 and §5; ADR-003; ADR-008.

**THE DECISION IS TAKEN.** [`ADR-013`](../ADR/ADR-013-a-client-is-told-which-player-it-is.md) was
Accepted on 2026-09-22 and this step is ordinary work against it. What follows is the question it
answered, kept because a step that cites a ruling without stating what it was for is a step nobody can
check.

`GameDesign.md` §2 configures the slots on the host before the match starts, and `TechnicalDesign.md` §4
specifies the snapshot, the command and the heartbeat — but **nothing told an arriving client which of the
per-player blocks was its own**, and nothing said what a host does with a client it was not expecting.
§5's "the protocol version in the header refuses a mismatched build" implied a handshake that was defined
nowhere.

Everything after this step needs the answer: selection needs to know which entities are mine, validation
needs to know who is sending, and the credits readout needs to know which block to read.

**What the ADR settled:** a `Join` carrying a session token or zero, answered by a `JoinReply` carrying a
result, the slot, a token and the seed; **the host assigns the slot and a client does not choose**, which
collapses "a second client on a taken slot" into "no slot free" and answers it with a reason; a returning
client is recognized by a token **the host issued**, not one the client chose; the version check stays in
the packet header, so a mismatched build is dropped and looks like an unreachable host; and **a timeout
forgets an endpoint but never a slot**, which is the one place §4 and §2 read as though they disagreed.

**The seed was missing from this list and R23 already requires it.** The client runs the asteroid
generator itself — that is the whole of R23 — so it cannot draw a map without the seed, and the join
record is the only place it can arrive. [`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md)
is a second consumer and costs nothing extra because of it. Whether it also needs a reconnecting client to
get the *same* seed back is part of what this ADR settles.

**Files:** [`ADR-013`](../ADR/ADR-013-a-client-is-told-which-player-it-is.md) and its row in
`Design/ADR/README.md`; `NeuronCore/PacketHeader.h` for the two types and the version; `GameCore/Join.h`
`.cpp`; `GameLogic/Sessions.h` `.cpp` and `GameLogic/Host.h` `.cpp`; `NeuronClient/SessionToken.h` `.cpp`
for the file in `LocalState`; `GameClient/JoinState.h` `.cpp` and `GameClient/ClientFrame.h` `.cpp`;
`Server/Server.cpp` for `--seed`; `OutpostCommander/App.cpp`; the project files and `.filters`; tests in
**five** suites rather than three — the packet types are `NeuronCore`'s and the token file is
`NeuronClient`'s.

**Done when:** the ADR is Accepted; a client learns its player index and validates against it; a second
client on a taken slot is refused in a way the client can show; and a reconnect is recognized rather than
treated as a new player — which self-contained snapshots make cheap
([`ADR-003`](../ADR/ADR-003-the-record-and-the-command.md)) and which nothing else makes correct.

**All four are met and every one is pinned without a socket**, because `Sessions` holds no transport and
`JoinState` holds no clock — the split M0.18 forced on the gesture seam, taken again here. What the suite
cannot reach is the one thing that made the token necessary: **a client's endpoint changing across a real
relaunch.** **Observed 2026-09-23 on one machine instead**: ADR-022's churner closed its socket and
rejoined on a new one nineteen times against a real `Server`, and it was resumed into the same seat each
time. A new socket is a new endpoint, which is exactly what a relaunch presents. The two-machine
version of this was withdrawn with the rest (`OpenQuestions.md` Q50).

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

### M1.9 — The hulls, as meshes · `NeuronClient`, `GameClient`, `OutpostCommander` · `NeuronClientTests`, hand · agent

**Read first:** [`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md) in full — **it replaced the opposite
decision and the tree held the old one for two days**, so read the record rather than remembering it;
[`ADR-021`](../ADR/ADR-021-content-ships-with-the-package.md) on how content is carried and on the ASTA
trap; `Design/design_handoff_meshes/`, which specifies the shapes; `OpenQuestions.md` **Q37**, because a
mesh is not scaled at draw time so its authored extent *is* its size; `TechnicalDesign.md` §6 and §7; R9;
R14.

**Adds:** geometry from files. **Three CMO meshes ship here** — `Scout`, `Frigate` and the station — as
package content declared by `OutpostCommander`.

**The split is R9's and the reader falls on the engine side of it.** A CMO reader knows nothing about a
game, so `NeuronClient` gets the reader, the vertex and index buffers, the upload and the instanced draw;
`GameClient` gets the map from a hull identity to a file and the hull palette, which is the only part that
knows what a `Scout` is.

**THE CONTENT ARRIVED EARLY AND THIS STEP NO LONGER AUTHORS IT.** `Design/design_handoff_meshes/`
delivered **thirteen** meshes rather than three, and the pipeline under them is built and green:
`Scripts/MeshesToObj.py`, `Scripts/BuildMeshes.py` and `Scripts/CheckMeshes.py`, with `meshconvert` pinned
by version and hash in `Tools/`, producing thirteen `.cmo` files that ship as appx payload. The
handedness, the vertex-colour injection and the landmark tests are all closed and measured. **What this
step keeps is everything downstream of a file existing**, and it is now a shorter list with sharper edges:

- **`NeuronClient/CmoReader.h` `.cpp`** — nothing in the tree reads a `.cmo`. The format is grounded
  against real converter output rather than recalled, including the one field that does not survive
  memory: a material's ambient, diffuse, specular and emissive are `float4`, not `float3`, with the
  specular power a single float between specular and emissive. A `float3` reading walks off the end inside
  the eight texture slots and fails several kilobytes later, somewhere unrelated.
- **The mesh catalog.** `MeshCatalog.g.h` came with the handoff and was dropped when its item groups were
  folded into the project; nothing now carries the names, package URIs, counts, extents or the hull
  palette as shader constants. It regenerates from `manifest.json`, which ships beside the meshes so the
  runtime asserts against the file the build validated.
- **`CheckMeshes.py`'s CMO stage**, which is still the handoff's stub and passes while asserting nothing.
  The real verification lives in `BuildMeshes.py` and **should move behind `CmoReader`** once that exists,
  so that the build and the client are not two parsers agreeing with each other.
- **Q37's catalog row**, which this step is the reason to write: the delivered extents answer it, and R24
  wants the size named in the catalog rather than left implicit in whatever the file contains, so a script
  compares two statements of one figure.
- **The measured appx size**, owed by Q44. `TechnicalDesign.md` §7 now names three different sizes and
  marks the package one as unmeasured; the built appx's block map is where it comes from.

**The reader is the risk in this step and the tests are its.** CMO carries a materials block with eight
texture-name slots, a skinning vertex buffer, a bone hierarchy and animation clips, **none of which the
MVP uses and all of which it must skip rather than reject** — so the suite feeds it a file that has them.
It reads position, normal and vertex color from the fixed vertex and treats tangent and texture coordinate
as the dead 24 bytes of 52 that they are. **No DirectXTK12**: R14 has not moved and ADR-005 turns on it.

**Nothing loads on the frame thread.** `Package.Current.InstalledLocation` is asynchronous and the frame
thread is an ASTA, where blocking on it is a deadlock rather than a delay. The meshes are read before the
frame loop starts or on a worker with a handoff, and this is the step that meets it first.

**Team color is the vertex color channel** selecting between the hull palette and the owner's color, so
one instanced draw covers every ship of a shape regardless of owner. A mesh that wants a second material
for its team costs a draw call per owner and is a trade to argue rather than take.

**The risk ADR-005 named has changed shape but not gone.** One shared function tending to emit the same
hull at two scales is no longer the mechanism — but `Scout` and `Frigate` are still 3.5 and 5.3 authored
pixels at the tactical zoom, and **divergent proportion in the plan view** is still what makes them
different objects there. M2.13 judges it.

**Files:** `NeuronClient/CmoReader.h` `.cpp`, `NeuronClient/MeshBuffer.h` `.cpp`,
`NeuronClient/InstancedDraw.h` `.cpp`; `GameClient/HullMesh.h` `.cpp`, `GameClient/MeshCatalog.g.h`,
`GameClient/WorldPass.h` `.cpp`, `GameClient/Ship.hlsl`; the three `.cmo` files and their declaration in
`Package.appxmanifest` or the project's content items; `Tests/NeuronClientTests/CmoReaderTests.cpp`,
`Tests/GameClientTests/HullMeshTests.cpp`; `Scripts/BuildMeshCatalog.py`; the project files and
`.filters`.

**BUILT, 2026-09-22.** The reader, and the suite that feeds it a file carrying skinning, bones and
animation clips. `Scripts/BuildMeshCatalog.py` and the `MeshCatalog.g.h` it regenerates from
`manifest.json`. `GameClient/HullMesh`, which is the only part of the path that knows what a `Scout` is
and is where the authored Y-up frame becomes the camera's Z-up one — a reflection, so every triangle's
winding is reversed and back-face culling is left ON in the ship pass precisely so that a missed reversal
shows as a hull that vanishes. `CheckMeshes.py`'s CMO stage, which was a stub that passed while asserting
nothing and now decodes all thirteen files, runs the landmark tests on the decoded positions and catches
the all-white colours that mean the vertex-colour injection was skipped. **Q37's two statements now have
a script between them**, which is what the register said would close that row. And the renderer: upload
buffers, a three-deep instance ring, the `Ship` shader pair and a mesh pass that draws each shape once.

**MEASURED ON THE DEVICE: 1,482 microseconds a frame**, against 1,118 for M0.21b's arrow — about 360 for
the hulls, and the two are Debug against Release so that supports the shape of the answer rather than
three digits. In ADR-007.

**WHAT THE LOOKING FOUND WAS A BUG, WHICH IS WHY IT IS DONE BY LOOKING.** `manifest.json` states the light
rig in the authored Y-up frame like everything else it delivers; the vertices were converted and the
lights were not, so the key pointed very nearly along the plane and a station's top plate — most of what a
raking camera shows — was carried by ambient alone. It read as the meshes being vague rather than as the
light being wrong. Both now go through one conversion.

**STILL OWED, AND BOTH ARE A SCREEN.** The tactical-zoom silhouettes, which are M2.13's. And
[`ADR-021`](../ADR/ADR-021-content-ships-with-the-package.md)'s clean install: the deploy used here was a
loose-file registration of a build output on the machine that produced it, which is the one arrangement
in which a missing payload declaration cannot show.

**Done when:** **`Scout`, `Frigate` and the station** draw as one instanced call each with a per-instance
transform and team color, at the sizes Q37 settles; the reader survives a file carrying skinning, bones
and animation clips and is asserted to; a mesh's authored extent matches the size the catalog states; the
package actually carries the files on a clean install, **which is the failure ADR-021 named and which does
not appear on the machine that built it**; and **the silhouettes are looked at from the tactical zoom**,
which is a screen and not a test.

### M1.9b — The sky · `NeuronClient`, `GameClient` · `NeuronClientTests` · agent

**Read first:** [`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md) in full;
[`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md), which it amends;
[`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md); R9, R14 and R23.

**Adds:** the backdrop, and it is **stars and nothing else** — 8,000 instanced quads from `SV_VertexID`
with no vertex buffer, seeded from the match, over a black clear. **The galaxy band this step first built
is withdrawn** ([`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md) decision 1): baked into a
512² cubemap with dust lanes, it read on the device as a painting behind the fleet at every brightness
that was visible at all. The Milky Way is carried by star density rising toward the galactic plane.

**What makes it read as a sky is four properties, and each has a way of failing that is worth knowing:**

- **Magnitude tiers in the ratio 1 : 3 : 9 : 27 : 81 : 243** — 22, 66, 198, 593, 1,780 and 5,341
  stars, which are rounded shares with the leftover at the faint end rather than an exact division: the
  ratio sums to 364 and divides no round number evenly. The brightest tier being a handful — one or two
  in any frame — is the whole effect, and the count is judged per frame: 3,000 put under half a
  standout on screen. Uniform brightness reads as salt and pepper.
- **Size follows brightness**, 10 scene-target pixels down to 3.0, **drawn continuously rather than one
  size per tier** — six sizes for three thousand stars reads as six kinds of dot — and the faint end is
  set by **what the nearest pixel receives**: 2.4 pixels at 0.18 under a squared falloff delivered about
  16 of 255 and was not seen. The falloff is flat-topped, `1 − smoothstep(0, 1, r)`, and apparent size is
  the point-spread function, not the star. All of it was found by looking.
- **Color is blackbody, desaturated to about 38%.** Oversaturated tints are how a procedural sky
  announces itself; real stars read very nearly white — but *nearly* white is the point, and 20% over a
  narrow temperature range left every star the same off-white with no tint visible at all.
- **Temperature correlates with magnitude** — bright tiers blue-white, faint ones orange — and star
  density rises toward the galactic plane. Draw color independently of brightness and the sky is subtly,
  unnameably wrong.

**Nothing twinkles and nothing moves.** There is no atmosphere, so there is no scintillation; diffraction
spikes are a telescope artifact and the player is not looking through one. **The sky takes no time input
at all** — generated once, never updated, zero per-frame CPU. Do not add "a little movement" later without
reopening ADR-019.

**It is dim, and the ceiling is on area rather than peak**: large-area luminance never above **12% of full
white**, point features up to 45% and only for the brightest tier. ADR-005 leans on the backdrop being
near-black to make a faceted hull read as deliberate, and this is the number that keeps that true.

**The R9 split is the usual one.** `NeuronClient` gets the instanced sprite draw, the blackbody table
and the seeded point-field generator — none of which knows there is a game. `GameClient` gets what *this*
sky looks like: the galactic plane's orientation, the star count, the tier ratios, the palette and the
ceiling. **Nothing goes in `GameCore` or `GameLogic`**, and the sky being floats throughout means
`Scripts/CheckDeterminism.py` catches it if anyone tries.

**Draw it last, with depth test on**, so it shades no pixel the fleet already covers and pays no
multisample resolve on a surface with no edges.

**Files:** `NeuronClient/PointSprites.h` `.cpp`, `NeuronClient/Blackbody.h` `.cpp`,
`NeuronClient/StarField.h` `.cpp`, `NeuronClient/Shaders/StarVS.hlsl`, `StarPS.hlsl`;
`GameClient/SkyLook.h` `.cpp`; both project files and `.filters`;
`Tests/NeuronClientTests/BlackbodyTests.cpp`, `StarFieldTests.cpp`; `Tests/GameClientTests/SkyLookTests.cpp`.
The withdrawn band's `NeuronClient/CubemapBake.h` `.cpp` and its `Galaxy` and `Sky` shader pairs are deleted.

**Done when:** the blackbody table is **pinned exactly** at its eight stops and between them; a given seed
produces the magnitude tiers in the stated ratio and **the same sky twice**; the faintest star delivers
a visible value to its nearest pixel; and the whole thing is **looked at on the device at the tactical zoom** — which is where
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

**BUILT, 2026-09-22.** The packer, the linear-space average pinned at 213, 156 and 141 against hand
arithmetic, and the advance round trip. The atlas also packs **a block of full coverage first**, so a
solid plate is a glyph quad over it and M1.13 draws the whole interface as one call in emission order.
Measured on the device: **3 to 17 ms to rasterize both sizes, 269 of 512 rows used** — ADR-009's second
owed figure, and small enough that caching it to `LocalState` is not worth a file. **The rebuild path is
not wired**: the client never resizes and has no device-removal path, so the atlas is built once.

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

**BUILT, 2026-09-22**, as `TextLayout` (pure, the suite's) and `TextRenderer` (the draw), with the
shader pair split as ADR-012 requires — `GlyphVS.hlsl` and `GlyphPS.hlsl` rather than one `Glyph.hlsl`.
**The baseline follows CSS**, because every text rect in the handoff is a line box as tall as its font:
ascent plus descent centered in the box. **The palette goes in as its bytes, not converted to linear**,
which contradicts `palette.json`'s note and is right for this tree: the back buffer is
`B8G8R8A8_UNORM` with no sRGB view, and the handoff's reference blends on the encoded values too.

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
`OpenQuestions.md` Q33**, open when this step was written, so it could not hard-code a side. *(Answered
2026-09-23: a setting, right-handed by default.)*

**Files:** `GameClient/Panels.h` `.cpp`, `GameClient/HudLayout.h` `.cpp`, `GameClient/PanelHitTest.h`
`.cpp`; `GameClient.vcxproj` + `.filters`; `Tests/GameClientTests/HudLayoutTests.cpp`;
`Scripts/CheckHudGeometry.py` **extended** — it already gates `geometry.json` against its own rules
and against `Interface.md` §1, and this step adds the half that compares those rects to the
constants the client actually draws from.

**BUILT, 2026-09-22.** `HudLayout.h` carries every rect with a `// geometry:` tag and
`CheckHudGeometry.py` now compares 63 of them against `geometry.json` field by field. The rows it
exempts are the alert, the world-anchored elements, and the module row's armed and unavailable states and
cargo, which are M2's and M3's. **What is not built:** the module row (no module exists until ADR-015 at
M2, and *unavailable* means "build something else first", which nobody at M1 can do); **motion**, which
the handoff's own build order puts last. ~~A **reconnecting** state, because nothing yet detects a
resume~~ — **BUILT AFTERWARDS, 2026-09-22**: `ClientFrame` calls the link lost after a second with no
snapshot, which is how a resume shows up (the frame clock jumps and the last arrival is old). It rejoins
with the token it holds and keeps the overlay up until the first snapshot after the host seats it again.
`ClientFrame::Link` is now the one place the link state is decided. It used to be decided in `App.cpp`,
where no suite could reach it. `ClientFrameLink` and `TheRejoin` pin it. **Two things the handoff did not settle and this step
did:** a refused join shows `MATCH FULL` in the reconnect overlay's block, and the system panel keeps
`LINK` rather than `RECONNECTING` because the longer word does not fit its 72 pixels before the quit.

**Done when:** **every interactive target is asserted at its own tier with its clear space, in both
handedness states** — a test, not a measurement by eye, because this is the rule that erodes one control
at a time, and a combat-tier target passing on the 48 floor is the way it erodes; **the gate agrees with
`geometry.json` rect for rect**, so the drawn interface and the designed one cannot diverge silently; a
tap inside a panel never reaches the world; **the two bottom panels swap sides on one value**, so Q33
costs a setting rather than a rewrite; and the selection panel's grouping matches what M1.11 selected.

### M1.14b — The stress harness · `GameClient`, `Bot` · `GameClientTests`, hand · agent

**Read first:** [`ADR-022`](../ADR/ADR-022-a-bot-is-a-headless-client.md) in full, and **do not start
while it is Proposed**. Then ADR-013 (above all, that a slot is never given back), ADR-008 and ADR-003's
command reliability; `GameDesign.md` §2 and §8; `AGENTS.md` §2, R19 and R20.

**Adds:** a third executable, `Bot`, an unpackaged C++/WinRT desktop console application that runs **many
headless clients from one process** against one host, in three roles: **players**, **churners** and
**flooders** (ADR-022's table). The run is the command line: host address, a count per role, a seed, and
a length in ticks after which the process prints its report and exits. **Nothing on the host and nothing
on the wire changes in this step.** A host started plainly seats two, so a run is two players or churners
plus any number of flooders; past that the host is started with `--players N --stress` (M1.14c).

**The executable holds only glue.** It parses the command line, initializes the apartment, owns one
`DatagramTransport` and `PacketQueue` per bot, runs the one loop, and prints. Everything that decides
lives in `GameClient`, where `GameClientTests` reaches it (R20):

- **`BotPolicy`**, the player's decisions. It takes the bot's replica store, its player and its
  `Neuron::Pcg32`, and returns zero or more `Command`s, acting only when the update tick has advanced by
  its decision interval. It queues a `Build` its credits cover, occasionally sends a `CancelBuild`, and
  sends a `MoveTo` for a random subset of its own ships to a point inside the playfield (`BuildMoveCommand`
  already builds that). It never names an entity it doesn't own.
- **`ChurnSchedule`**, which says when a churner drops its transport and when it rejoins with the token it
  holds, by harness tick.
- **`FloodSchedule`**, which says which malformed or refusable datagram a flooder sends on which harness
  tick: a join past a full match, a command from an unseated endpoint, a truncated header, a wrong
  version, and an impossible count. The malformed ones are built by hand, since the encoder would refuse them.
- **`StressReport`**, the counters ADR-022 lists, including update tick gaps, the refresh interval per
  entity and command-to-acknowledge time, accumulated per role and formatted as text.

The decision interval, the churn interval and the flood rates are constants in their headers, with the
reason written beside each, not tuned numbers.

**Three `NeuronClient` functions are off limits. ADR-022's table says why:** `ReadHostAddress`,
`ReadSessionToken` and `WriteSessionToken` need package identity. **If anything the bot does call turns
out to reach `ApplicationData`**, stop and report it rather than hide it behind a `try`.

**Files:** `GameClient/BotPolicy.h` `.cpp`, `GameClient/ChurnSchedule.h` `.cpp`,
`GameClient/FloodSchedule.h` `.cpp`, `GameClient/StressReport.h` `.cpp`; `GameClient.vcxproj` + `.filters`;
`GameClient/GameClient.h`; `Tests/GameClientTests/` one test file per class, plus the suite's `.vcxproj`
and `.filters`; `Bot/Bot.cpp`, `Bot/pch.h`, `Bot/pch.cpp`, `Bot/Bot.vcxproj` + `.filters`,
`Bot/packages.config` at the pinned `Microsoft.Windows.CppWinRT` version; `OutpostCommander.slnx`.
Also **`AGENTS.md`**: in §2 the executables paragraph, the import table row
`Bot | — | GameClient, NeuronClient`, the master-include diagram and the count of C++/WinRT projects; in
§3 what `Bot` is (unpackaged, desktop, no manifest); and §5's R20 sentence, which names the executables.
Add `Bot/` to `.clang-tidy`'s `HeaderFilterRegex` if it names directories. `Scripts/CheckProjectFiles.py`
finds the project by itself, so check it passes and don't edit it to make it pass.

**Not this step:** A CI job running `Server` against the harness, which ADR-022 names as enabled and which changes the workflow. And anything that makes a bot a
better player, which is Q48.

**WRITTEN 2026-09-23 ON A MACHINE WITHOUT MSVC; BUILT AND TESTED ON `Debug|x64` IN CI THE SAME DAY
(f7f1f5b), AND NOT RUN.** No other pair has been built and `Bot` has not been started against a host.
Six things the step did not anticipate were decided while writing, and each is stated where it lives:

- **`BotPolicy` owns its orders until the host applies them.** ADR-003 repeats a command in every packet
  until the block acknowledges it, and the packaged client does not, so the policy numbers its own commands
  -- adopting from the host's `lastCommandSequenceApplied` as `ClientFrame` does -- keeps the outstanding
  ones and retires them on acknowledgment. That is also where the command-to-acknowledge time is measured.
- **The refresh interval is counted by the store.** `ReplicaStore::AcceptResult` and
  `ClientFrame::DrainResult` gained three counters, and `ReplicaStoreBehavior` a test, because the store is
  the only thing that knows an entity's previous tick. Nothing in the frame reads them.
- **A flooder's join waits for a full match.** A join into a free slot is not refused, it takes the slot
  for good (ADR-013). So `FloodSchedule` sends none until the harness has seen every player and churner
  seated, and stops both unseated kinds the first time the host seats it anyway; `Bot` prints that it was.
- **Two of the five flood kinds are not refused by a decoder.** The join past a full match and the command
  from an unseated endpoint are well-formed on purpose, and the host refuses them by `MatchFull` and by the
  session table. The suite pins that they decode cleanly and carry what gets them refused; the other three
  are pinned to their decoder faults by name. Only the run against `Server` sees the refusal itself.
- **The harness tick is the host's 50 ms**, counted by the loop, which polls every 2 ms between ticks. The
  churner and flooder act once a harness tick; the policy is paced by the update's tick.
- **The ceiling is provisional.** `HARNESS_BOT_CEILING` is 128, a hundred players and a few more, until the
  first run measures the real one. The transport refuses a send while the previous one is in flight, so a
  flooder's real rate is bounded below its schedule's; the report counts those skips.

**Done when:**

- **What an agent can establish:** the suites pin that each of the four classes, fed the same inputs and
  seed, produces the same output twice. They pin that `BotPolicy` never commands an entity it doesn't own,
  never builds what its credits can't cover, and is silent between decision ticks. They pin that a churner
  always presents the token it was issued, and that every datagram `FloodSchedule` produces is one the
  host's decoders reject by name. `GameClient` still links no `GameLogic`, and `Bot/` holds no decision
  a test could have pinned.
- **What needs a Windows machine:** `Bot` builds on `Debug|x64`, and on the other three pairs or the report
  says it didn't. Against a real `Server` on the same machine, **with no loopback exemption granted to
  anything**: two players are seated and their commands are acknowledged, a churner keeps its seat across
  ten rejoins, and the host's tick is unbroken under a flooder at the highest rate the schedule offers.
  That run discharges ADR-022's owed measurements, and they are written into it, **including the
  harness's own ceiling in bots per process.**
- **What needs the owner:** ruling ADR-022. **This step doesn't close M1.15.** That gate asks whether two
  *people* can play.

**RUN 2026-09-23 ON THE SURFACE PRO, AND DONE.** It was built on all four pairs earlier that day. Then
`Release|ARM64` `Bot` ran against a real `Server` on the same machine, and neither process is in an
AppContainer, so no loopback exemption applied to either. Both players were seated, their commands were
acknowledged, and a churner kept its seat across nineteen rejoins. The host ticked 3,499 times with none
abandoned under a flooder, and 128 players held a one-tick update gap. **The figures are in ADR-022's
Measurements**, and the provisional ceiling of 128 is now a measured one. One finding: a flooder's real
rate is about one datagram a tick whatever it asks for, because the transport refuses a send while one
is in flight. ADR-022 says so.

### M1.14c — ADR-024's replication, and the player count · `NeuronCore`, `GameCore`, `GameLogic`, `GameClient`, `Server` · every suite · agent

**Read first:** [`ADR-024`](../ADR/ADR-024-replication-is-prioritized-records.md) in full, then
[`ADR-023`](../ADR/ADR-023-the-player-count-is-configurable.md); ADR-003 as cut down, for the record's
field semantics and the command path, which do not move; `TechnicalDesign.md` §4 and §6; `OpenQuestions.md`
Q49; `.claude/skills/datagram-budget/`, **whose script is run before and after**, and
`.claude/skills/determinism-audit/`, because the accumulator lives in `GameLogic` and must not touch the
tick. **There is no backward compatibility to keep**: the owner ruled it, so the old snapshot goes rather
than being kept beside the new update.

**Adds:** the replication of ADR-024 end to end, in four commits in this order, and the player count of
ADR-023 in a fifth.

1. **The wire.** `PacketHeader` loses its two fragment fields and is four bytes. `EntityRecord` is twelve:
   a three-byte identity (16-bit index, 8-bit generation, and `WIRE_INDEX_BITS` moves with it), an owner
   byte, and the team bits gone from `flags`. `Snapshot` becomes `Update`: the twenty-one-byte header
   with the live entity count and **the recipient's own block**, then records, then three-byte removals,
   then seven-byte fire events. `CommandPacket`'s header gains the view center and radius, and a selected
   identity is three bytes. `EncodedSize` of a full update is what `GameCoreTests` measures, and the
   figure goes into ADR-024's Measurements in the same commit.
2. **The host.** `GameLogic/Accumulator.h` `.cpp`: per session, a score and a last-sent tick per live
   entity; the relevance sum at Q49's weights, each a named constant with its reason; the sweep computed
   from the live count; removals and fire events queued per session with their repeat counts; and
   `Fill`, which returns up to `UPDATES_PER_TICK` (two) whole updates for one client from the world.
   `Host.cpp` calls it per session instead of `BuildSnapshot`, and resets a session's scores on
   `Rejoined`. **Nothing in `Tick` changes.** The accumulator reads the world after the tick and writes
   nothing the hash covers; `CheckDeterminism.py --review` will still see it, and the `determinism-audit`
   skill's judgment for it is stated in ADR-024.
3. **The client.** `ReplicaStore` holds three samples per entity by tick, drops a record whose tick is not
   newer, holds an entity with no sample past the render time, and forgets one three sweeps silent.
   `ClientFrame::DrainPackets` decodes updates, applies removals it has not seen, and clears the store on
   a rejoin. `HitTest` reads the owner from the record. The command packet carries the camera's view
   center and radius, which `Camera` already knows. `Panels` reads credits and the build item from the
   own block.
4. **The bot** (M1.14b) needs no change beyond recompiling, which is the point of reusing `GameClient`,
   and its report now carries the refresh interval per entity, which is ADR-022's added measurement.
5. **The count.** `Server` takes `--players N` and `--stress` and refuses a count above four without
   the switch or above what the entity index holds. `Sessions`, `CommandIntake` and `BuildSystem` size
   their per-player state at `Begin`. `GameCore/Layout.h` gains the layout above four through the sine
   table. `Panels` draws a player past the palette in the last team color.

**WRITTEN 2026-09-23; BUILT AND TESTED ON `Debug|x64` IN CI THE SAME DAY (792fd78), AND NOT RUN.** Parts 1 to 4 landed as one commit, not four: the transport
header, the record, the update, the host and the client change together or nothing links, so no split of
them compiles. The additive 24-bit codec went in first as its own commit. Two things the step did not
anticipate were decided while writing and are recorded in ADR-024: **the sweep is computed from a 65-record
floor**, with removals capped at 48 an update and fire events at 40, because a guarantee cannot be computed
from the typical fill; and **a seated client sends an empty command packet four times a second** as its view
report, because commands only go out when the player taps. It was written on a machine without MSVC;
CI's first compile found two tests still asserting the old join size and an unknown player the new capacity
seats, both fixed in 792fd78. **Part 5 landed as its own commit the same day**: `Server --players N --stress`,
`MATCH_PLAYERS` split from a `MAX_PLAYERS` of 254, the stress layout through the sine table, and the last
team color past the palette -- with ADR-023 amended where the tables ended up sized to the capacity rather
than at `Begin`.

**Files:** `NeuronCore/PacketHeader.h` `.cpp`; `GameCore/EntityRecord.h` `.cpp`, `GameCore/Entity.h`,
`GameCore/Snapshot.h` `.cpp` **renamed** `Update.h` `.cpp`, `GameCore/Command.h` `.cpp`,
`GameCore/Layout.h` `.cpp`; `GameCore.vcxitems` + `.filters`; `GameLogic/Accumulator.h` `.cpp`,
`GameLogic/Host.h` `.cpp`, `GameLogic/Sessions.h` `.cpp`, `GameLogic/CommandIntake.h` `.cpp`,
`GameLogic/BuildSystem.h` `.cpp`; `GameLogic.vcxproj` + `.filters`; `Server/Server.cpp`;
`GameClient/ReplicaStore.h` `.cpp`, `GameClient/ClientFrame.h` `.cpp`, `GameClient/Interpolation.h`
`.cpp`, `GameClient/HitTest.h` `.cpp`, `GameClient/Panels.cpp`, `GameClient/TapOrder.h` `.cpp`;
`Tests/NeuronCoreTests/PacketHeaderTests.cpp`, `Tests/GameCoreTests/SnapshotTests.cpp` **renamed**
`UpdateTests.cpp`, `Tests/GameCoreTests/CommandTests.cpp`, `Tests/GameCoreTests/LayoutTests.cpp`,
`Tests/GameLogicTests/AccumulatorTests.cpp`, `Tests/GameLogicTests/HostTests.cpp`,
`Tests/GameClientTests/ReplicaStoreTests.cpp`, `Tests/GameClientTests/ClientFrameTests.cpp`, and the
suites' `.vcxproj` + `.filters`; `Scripts/DatagramBudget.py` if a width the encoder settles differs
from its model; and **the documents that quote the figures**: ADR-024's Measurements, ADR-003's
Measurements (a line saying the 1,137 is history), `TechnicalDesign.md` §4's table, and the comments
in `EntityRecord.h` and `Update.h` that spell the widths out. `CheckDesign.py` recomputes the figures
and will say which document it missed.

**Done when:**

- **What an agent can establish:** `GameCoreTests` measures a full update at exactly the pinned payload
  with 99 records at three removals and two fire events, and round-trips every record type. The
  accumulator's suite pins that an update never exceeds the payload, that no entity goes a sweep unsent
  whatever the scores, that a removal rides ten consecutive updates and a fire event three, that the
  same world and view give the same sent set twice, and that a rejoin resets the scores.
  `ReplicaStoreTests` pins the drop-older rule, the hold, and the three-sweep forget. **M1.7's
  determinism test still hashes to `0x37f846ed90b74ca1`**, which is the proof nothing simulated moved.
  `CheckDeterminism.py`, including `--review`, `CheckDesign.py` and `CheckProjectFiles.py` are clean.
  The layout at two and four is unchanged bit for bit against M1.5's pinned output.
- **What needs a Windows machine:** a two-player match on the device draws as it did — every entity every
  tick, which the client's store can count — and ADR-003's loopback tap-to-visible of 76 ms is re-run and
  has not moved. ADR-022's harness at eight players seats eight on `--players 8 --stress` and reports the
  refresh interval per entity. Both are ADR-024's owed measurements and are written into it.

**PARTLY RUN 2026-09-23 ON THE SURFACE PRO.** The agent half was met at build: the hash held on all four
pairs, and the full update encodes to exactly 1,232 bytes. On the machine, ADR-022's harness seated eight
on `--players 8 --stress`, and every entity refreshed every tick. The same run at 2, 32, 64, 125 and 128
seats is in ADR-022, and the host's CPU per client per tick, as an upper bound, is in ADR-024. **Still
owed, and both need the packaged client with a finger on it**: that a two-player match on the device
draws as it did, and the loopback tap-to-visible re-run against the 76 ms. Both are ADR-024's fifth
measurement, and M1.16 is the session that takes them.

### M1.17 — Ships turn, and route around structures · `NeuronCore`, `GameCore`, `GameLogic` · three suites · agent

**Read first:** `OpenQuestions.md` Q51 and Q52 in full, both answered by the owner on 2026-09-23;
`GameDesign.md` §6 and §7; ADR-002; `.claude/skills/determinism-audit/`, because this rewrites the tick's
only system. **It was found by M1.16's hand session**, which is why it is numbered after the steps it
follows and sits outside the gates it was found by.

**Adds:** an integer bearing in `NeuronCore` (`BearingOf`, a vector to a binary angle), pinned at the
cardinals, the diagonals and against the sine table. `DerivedStats::turnAnglePerSecond` at Q51's gain,
zero without a drive. A move order that carries a turn per tick beside its speed, and a tick that steers
the heading toward the destination, throttles by the cosine of the error, flies along the heading, and
steers for a tangent past the nearest structure its line crosses (Q52).

**Files:** `NeuronCore/SineTable.h` `.cpp`; `GameCore/DerivedStats.h` `.cpp`; `GameLogic/World.h`
`.cpp`, `GameLogic/Tick.h` `.cpp`, `GameLogic/RingAssignment.cpp`; `Tests/NeuronCoreTests/SineTableTests.cpp`,
`Tests/GameCoreTests/DerivedStatsTests.cpp`, `Tests/GameLogicTests/TickTests.cpp`,
`Tests/GameLogicTests/HostTests.cpp`, `Tests/GameLogicTests/DeterminismTests.cpp`.

**Done when:**

- **What an agent can establish:** the bearing is exact at the cardinals and within one table step
  everywhere. Every design's turn rate is derived, and adding a component lowers it. A ship ordered
  behind itself turns before it moves, and still lands exactly on its destination. A ship whose line
  crosses a station arrives without ever entering its keep-out circle. A ship already inside one leaves
  it. **The determinism hash moves, deliberately**, and is re-pinned from all four pairs agreeing.
  `CheckDeterminism.py`, including `--review`, is clean.
- **What needs the device:** a ship visibly turns as it goes, and goes around its station.

**BUILT 2026-09-23, ALL FOUR PAIRS, NOT YET LOOKED AT.** `BearingOf` in `NeuronCore` is a binary
search of the sine table's first octant, pinned exact at the cardinals and diagonals and within one
step all the way round. Turn rate is `TURN_GAIN` × thrust ÷ mass: 23,400 for a Miner and 32,760 for a
Fighter. `GameLogic/Tick.cpp` steers, throttles, flies along the heading, and routes around the nearest
structure in the way. It steers by a circle half a mover wider than the keep-out, so the turn lag cannot
carry a ship inside. The suite pins all of it, including a Miner that crosses a station at four offsets
without ever entering its keep-out, and two Miners that still pass through each other. **The hash moved on
purpose to `0xc8b1069f59fa3f85`**, and the M0.8 scripted run moved to `0xa0141c81045fc0bc`. Both agreed
on Debug and Release, x64 and ARM64 before either literal was changed.

---

## The gates

### M1.15 — GATE: two clients on one host · — · hand · **human**

**Read first:** `README.md` F5; `GameDesign.md` §2 and §10; ADR-008.

`GameDesign.md` §10 asks for two clients on one host, and **a packaged application is single-instanced**.
Running a second instance on one machine is a manifest declaration (`SupportsMultipleInstances`, in the
`desktop4`/`iot2` namespace — confirm the current form before relying on it) which interacts with ADR-008's
already-strained loopback exemption. The alternative is a second machine, which is not an engineering
decision at all.

**THE ONE-MACHINE ROUTE IS PREPARED, NOT CONFIRMED.** `Package.appxmanifest` declares
`SupportsMultipleInstances` in both namespaces, and **that alone would have broken the gate**: two
instances of one package share `LocalState`, so both would present the same session token and the host
would seat them as one player (ADR-013). Each instance now claims a slot (`NeuronClient/InstanceSlot.h`)
and names its token file and its log by it. Slot zero keeps the old names, so a lone client changes
nothing.

**RUN ONCE ON THE SURFACE PRO, 2026-09-22, WITH THE SCREEN LOCKED, SO HALF ANSWERED.** Setup:
`Release|ARM64` host and client on the Surface Pro 11, host at `127.0.0.1`, seed 20260922, both
clients launched from the shell. What it showed:

- **Two processes ran at once**, each with its own slot: `probe-log.txt` says slot 0 and
  `probe-log-1.txt` says slot 1.
- **Two seats.** Slot 0 presented its stored token and was seated as player 1. Slot 1 had no token, was
  issued one, and was seated as player 2. The host reported `clients=2`.
- **The one loopback exemption covers both instances**, since it is keyed on the package family and
  not on a process. Both received snapshots and both drew three entities.

**What it could not show is two people playing.** The device was at the lock screen, and Windows
suspends a packaged application that is not visible: both clients stopped about four seconds after
joining. **That is also the open half of the question.** A fullscreen client is not visible behind
another one, so the expected answer is that on one panel only the foreground client runs, while the
match goes on without the other. That is `Interface.md` §7's suspend, and it is exactly how two players
could never share one screen. **Two things remain for a hand on an unlocked device.** First, whether
that expectation holds when switching between the two. Second, whether two *snapped* windows, both
visible, both keep running. The client asks for fullscreen at launch, so the second needs the window
to leave fullscreen first.

**THE OWNER CHOSE THE ROUTE, 2026-09-23: two snapped, visible windows on the unlocked Surface Pro.** The
fallback of a second machine is gone. **The register's answer is
[`OpenQuestions.md`](../OpenQuestions.md) Q50, the same day: one machine, and no second one.** So if two
snapped windows both keep running, this gate closes on them. If they don't, it closes on what the lock-screen
run already showed: two seats from one host, played one foreground window at a time. **Which of those two
it is still needs a hand on the unlocked device.**

**Done when:** the question is answered on the register — **it is, Q50** — and two clients on one host
are playing on one machine.

**CLOSED 2026-09-23, ON THE FALLBACK: two seats on one host, played one foreground window at a time.**
Setup: the unlocked Surface Pro 11, a `Release|ARM64` `Server` on `127.0.0.1` with the plain two-seat
host and seed 20260922, and two instances of the package launched from the shell. Both run on the one
loopback exemption. What the run showed:

- **Two seats, held.** Slot 0 was seated as player 1 and slot 1 as player 2. The host showed `clients=2`
  for the whole run and abandoned no tick. When slot 0 closed and was launched again, it was **rejoined
  as player 1** on its stored token.
- **Both were played.** Taps and camera gestures reached each instance, as each one's log records.
- **Only the window in front runs.** This is the owner's own observation, and the logs agree with it. Slot 1's updates
  stop for 1 to 4 seconds at a time, six times in 42 seconds (ticks 274→286, 334→364, 456→541,
  639→715, 727→790 and 831→847). Each gap ends in `LINK lost … rejoined`, which is `Interface.md` §7's
  suspend and resume, working as designed. **Two snapped windows that both keep running were not
  achieved**, so the route the owner preferred did not close this gate. The fallback did.

**What it means for testing:** two people cannot share this one panel at the same moment. A two-player
match on one machine is played by switching windows, and the match goes on without whichever client is
behind. That is enough to test the protocol, the seats and the rejoin. It is not a test of two people
playing at once, and with no second machine (Q50) nothing in this tree will be.

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
3. ~~**Whether the interface pass costs more GPU time than the world pass** — §9.6, which the design predicts
   it will: five instanced draws of simple geometry against an unbatched quad per glyph.~~ — **ANSWERED
   2026-09-23: no, 50 µs against 852.** The split is timed in the frame now, and the figures and
   setup are in `TechnicalDesign.md` §9.6, which is struck through. The panels were at rest, so a busy
   selection readout is the case still worth a glance in the probe log during the hand session.

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
   cause: uniform brightness reading as noise, oversaturated color as confetti, faint stars in the frame
   but not on the glass.
7. **The link-silence threshold**: one second of no snapshots before the reconnecting overlay goes up
   (`GameClient/ClientFrame.h`). It is not tuned. Check that a resume shows the overlay and then clears it,
   and that a bad wireless second does not put it up over a match that is still arriving.

**Done when:** all of them are answered on hardware and written into the documents that asked for them.

**THE HAND SESSION WAS HELD ON 2026-09-23, AND M1.16 IS NOT YET CLOSED.** The owner confirmed on the
Surface Pro:

- **1, the circle.** 192 pixels is right, and the raking case is acceptable (ADR-010).
- **2, the text.** It reads at 24 authored pixels.
- **4, the gesture constants.** The 16-pixel tap slop, the pick radius and the double-tap window all stand.
- **5, the camera.** The ground sticks, and orbit is usable one-handed on a kickstand, so orbit survives
  (ADR-018).
- **6, in part.** The fleet reads against the sky (ADR-019).
- **7, the link-silence threshold.** A resume shows the overlay and clears it. M1.15's run logged the
  same cycle six times.
- **M1.14c's half.** A two-player match draws as it did.

Each is written where it was asked. **3 is measured**, above, and **6's frame time is measured at both
scales**: 1,539 µs at 1:1 and 1,096 at 0.5, in ADR-016. **Tap-to-visible was then re-run by
finger: 79 ms mean over five taps against the 76**, in ADR-024. What follows is what stood open before
that run:

- **Tap-to-visible re-run against the 76 ms** (ADR-024's fifth). The session logged no `TAPVISIBLE`
  line. The instrument watches the *first drawn entity* and arms only on a tap that orders it while it
  is parked. That was right for M0's one ship and may not be for M1's match, whose first record need
  not be a ship the player owns.

`Interface.md` §7 also lists two items the session did not cover: **whether occlusion binds** (its 4)
and **the near end of the zoom range** (its 7, which is M1.8's pin).

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
