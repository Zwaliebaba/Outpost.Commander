# ADR-005 — A mesh is a CMO file

**Status:** Accepted — **replaced 2026-09-22.** This record used to decide the opposite: it was titled
"Meshes are generated in code" and ruled that a hull is a function emitting a few dozen triangles. **The
owner reversed that on 2026-09-22 and ruled the record replaced rather than superseded**, extending the
MVP exception in [`README.md`](README.md) — which was written for removing contradictions — to cover a
decision reversed outright. The ground is that nothing has shipped: a second record arguing with this one
would cost a reader more than the history of the disagreement buys, and the git history carries what the
file no longer says. The slug changed with the title, because a filename that states the old decision is
the same defect one directory out.

**After the MVP this route closes** and the older rule takes over: a decision is superseded by a new record
and the old file stays.

Earlier rounds on the record this replaces: amended 2026-09-21 by
[`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md), which made the backdrop near-black with a
stated luminance ceiling; amended by [`ADR-015`](ADR-015-the-base-is-built-from-modules.md), which added a
sixth thing to draw; and updated 2026-09-22 alongside
[`ADR-021`](ADR-021-content-ships-with-the-package.md), which ruled that content files ship.

**Amended 2026-09-23 at M2.4** for the asteroid alone: the one mesh that *is* scaled, turned on three
axes and lifted off the plane, **baked on the processor rather than instanced**, and the draw count and
package bytes this record owed. See *The asteroid at M2.4* under Consequences.

**Amended 2026-09-23 at M2.10b** for the modules: **each module level draws with its own mesh**, the four the
handoff delivered, so a shipyard reads apart from an ore processor. The bare `ModuleFrame` mesh ships and no
design draws it. Seven meshes are drawn instanced (three hulls and four module levels) beside the five baked
asteroid variants. And **a hull's size is bounded by every mesh that draws it**, which moved the frame's
catalog figure from 84 to 90 (`OpenQuestions.md` Q37's note).

**Date:** 2026-09-22 (this decision); 2026-09-20 (the record it replaces)
**Owner:** Stefan Zwaal

## Context

**The record this replaces was right about the thing it was actually defending and wrong about what
followed from it.** Its case was `AGENTS.md` R14's dependency line: a mesh format is the single most
common place a graphics project reaches past that line without noticing — glTF wants a JSON parser, FBX wants the SDK,
and every D3D12 sample in existence pulls DirectXTK12 for the loader it happens to ship with. That
argument is untouched and this record obeys it.

What it inferred from it was that geometry therefore had to be *emitted by code*, and
[`ADR-021`](ADR-021-content-ships-with-the-package.md) already found the gap: **R14 closes a list of
libraries, not a list of files.** ADR-021 removed the prohibition and deliberately stopped there — "this
record removes a prohibition; it does not order a pipeline". **This record orders one.**

**The MVP needs five shapes**: `Scout`, `Frigate`, `ModuleFrame`, the station and the asteroid — as this was
ruled; the module is four meshes since M2.10b, one per level, and the asteroid five variants. `Cruiser`
is a sixth hull the catalog carries and the MVP never draws (`GameDesign.md` §10).

**Two costs the old decision named against itself are what spend it.** It foreclosed art as a parallel
activity — "there is no way for anyone but a programmer to change a ship's shape" — and it named a risk
that lands exactly on the feel the design sells: **two hulls emitted by one shared parameterised function
tend to be the same shape at two scales**, which is unreadable at the tactical zoom where identification
matters most. M2.13 is the gate that judges it and `Design/Plan/M2-the-field.md` M2.10b asks a harder
version of the same question — four base modules told apart at about five authored pixels, from one mesh,
with no textures and no icons.

## Decision

**A mesh is a CMO file**, authored as content and shipped with the package under ADR-021. The five shapes
are modeled rather than emitted. `Design/design_handoff_meshes/` is where they are specified.

**CMO is chosen because it is a format somebody else documents and tools already write** — not because the
MVP needs anything in it. That is the whole of the argument, and the Consequences below are what it costs.

**The reader is written in this tree, and R14 does not move.** CMO's only reader in the wild is
DirectXTK12's model loader, which is the package R14 closes and [`ADR-021`](ADR-021-content-ships-with-the-package.md)
closes again by name, along with DirectXMesh and DirectXTex. No exception is taken here and none is
available: a format used here has a reader written here.

**What the MVP uses of CMO, and what the reader must skip without rejecting:**

| Section | The MVP | The reader |
|---|---|---|
| Vertex buffer | position, normal, color | reads the fixed vertex whole; tangent and texture coordinate are zeros |
| Index buffer | 16-bit indices | as given |
| Material | one per mesh, no texture names | reads the values; the eight texture-name slots are empty and stay empty |
| Skinning vertices, bones, animation clips | none | **skips them correctly rather than failing**, and is tested against a file that has them |

**Flat shading survives the change, because it was never a consequence of generation.** Vertices are split
per face and normals are baked per face. There is no smoothing group to decide, no tangent basis to get
wrong and no vertex cache to optimize at this scale — which is what a low-polygon faceted look wants
anyway.

**Team color survives too, and it is the one property that constrains the authoring.** Team membership is
carried in the vertex color channel, selecting between a hull palette and the owner's color, so **one
instanced draw covers every ship of a shape regardless of owner.** A mesh that needs a second material to
express its team is a mesh that costs a draw call per owner, and that is the trade to argue rather than
take.

**The axis convention is pinned here rather than discovered by the first loader**: left-handed, +Y up out
of the plane, +Z forward along the entity's heading, one CMO unit to one world unit. The plane is Y = 0 and
a hull straddles it. **A mesh is not scaled at draw time**, so its authored extent is its size, which is
what makes the size a thing a script can check against the catalog. **The asteroid is the one exception**,
since M2.4, and *The asteroid at M2.4* below says why it does not touch the reason.

**Nothing loads on the frame thread.** `Package.Current.InstalledLocation` is asynchronous and the frame
thread is an ASTA, where blocking on an asynchronous operation is a deadlock rather than a delay
([`ADR-012`](ADR-012-a-shader-is-compiled-into-a-header.md) records this as observed). Meshes are read
before the frame loop starts or on a worker thread with a handoff.

**The chain that produces a CMO is owed at M1.9 and is not settled here.** Two candidates, and the
recommendation is the second: FBX through `meshconvert` from DirectXMesh, which is a tool in the build
rather than a linked library so R14 survives, and which nobody in this tree maintains; or **a writer
script in `Scripts/` that reads the mesh handoff's own JSON and emits the bytes**, which keeps the whole
chain inside the tree, has no third-party binary in it, and is the same shape as everything else in
`Scripts/`. Whichever lands, the reader is the same reader.

## Consequences

**What this buys is the reason it was taken.** Art is a parallel activity and the cost ADR-021 bought back
is now spendable. A shape a function cannot describe is available. M2.10b can tell four modules apart with
four meshes instead of one mesh and a color, which is the largest single thing this decision unlocks. And
the silhouette risk the old record named against M2.13 is answerable with a modeled hull rather than only
with the shape-coded overlay it offered as the fallback.

**A reader for a format heavier than the MVP needs.** Materials with eight texture slots, a skinning vertex
buffer, a bone hierarchy and animation clips are all in the format and none is in the MVP. A conforming
reader skips every one of them correctly, which is a few hundred lines rather than the forty the old record
budgeted for its own hand-rolled exit — "a vertex count, an index count and two arrays".

**Dead bytes on every vertex, and the figure is in Measurements**: tangent and texture coordinate are
written as zeros and are 24 bytes of a 52-byte vertex. They cost package size and vertex bandwidth, neither
of which is near a limit at this scale, and they are the price of using somebody else's layout.

**Nothing authors CMO natively**, so there is a conversion step wherever it lands, and with it the failure
ADR-021 costed and declined for shaders: an asset step somebody keeps green in CI, and a class of failure
that appears on another machine instead of at compile time, which is a file that did not get packaged.
**This is the record that incurs that cost**; ADR-012 is still right that shaders should not.

**The asteroid loses its generator, and this is the sharpest thing given up.** The old record drove the
asteroid from the match PRNG so that no two rocks were identical, and **a file is one rock.** The
replacement is a set of authored variants with the match seed choosing among them and applying its own
yaw, pitch and scale jitter — which is weaker, because a variant set repeats and a generator does not.
Two figures move with it: **the field becomes one instanced draw per variant** where `TechnicalDesign.md`
§6 states one for the whole field, and each variant is package bytes. The visual jitter stays in the
client and never reaches a `GameCore` record, which is R22 and is unchanged.

### The asteroid at M2.4

**A rock is the one mesh that is scaled, and the rule above is about hulls.** "A mesh is not scaled at
draw time" exists so that an authored extent *is* a hull's size and a script can check it against the
catalog (Q37). No catalog row states an asteroid's size and nothing is spaced by one, so the handoff's
section 8 jitter applies as authored: **uniform scale 0.75 to 1.35, a turn on all three axes, and up to
240 units above or below the plane.** It is uniform because the baked per-face normals are not
renormalized anywhere, so a non-uniform scale would light them wrong.

**The scale is clamped so that no two rocks touch.** The generator keeps centers 150 apart and
`AsteroidE` at 1.35 is 225 across, so the seed alone would put rocks through each other. Each rock is held
to half the distance to its nearest neighbor, over the exact sphere of its loaded mesh. Over 200 seeds at
two and four players that clamps **3.5% of rocks, and none below 0.75**, so every rock stays inside the
handoff's range.

**The field is baked, not instanced.** The ship pass turns an instance about Z and nothing else, so
drawing a rock that turns on three axes, scales and lifts from an instance would need a second vertex
stage and a second instance layout. The field never moves once it is derived. So `GameClient/AsteroidMesh`
places each variant's rocks into one static mesh when the join names the field, and the ship pass draws
it with one identity instance. **Five draws for the field, as this record predicted, and no new shader.**
What it costs is memory: a placed copy of the geometry per rock rather than one per variant.

**None of this reaches `GameCore`** (R22). The look is drawn from the match seed on PCG32 stream 4, a
client stream nothing on the host touches, and it is separate from the generator's stream 3 so that how
a rock looks can never move where one is.

**`Cruiser` costs a file.** [`ADR-006`](ADR-006-a-ship-is-a-composition.md) claims that reinstating the
heavy design at M4 is "a table row and nothing else". Under this record it is a table row and a mesh. The
claim is weakened rather than broken, and it is named here so that it is not discovered at M4.

**A geometry change stops being a change the compiler checks**, which was the old record's sharpest
defense and is not recoverable. The replacement is a script that asserts each mesh's bounds against the
size the catalog states and against the handoff's JSON — a gate rather than a compile error, and weaker
than one. That is the same arrangement `Scripts/CheckHudGeometry.py` already has for the interface pass,
for the same reason, and it works there.

**The backdrop is still what makes a faceted hull read as deliberate.** The sky is capped at **12% of full
white** over any large area, with only the brightest eight stars reaching 45% over a few dozen pixels
([`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md)). If the fleet stops reading at the tactical
zoom there are now two levers where there was one — that ceiling, and the mesh itself, which is authored
and can be changed by somebody who is not a programmer. **Reach for the mesh first**; the ceiling is doing
work that nothing else does.

**What reopens it:** the reader costing more than the format buys. CMO is a convenience — a documented
layout with tools that write it — and if the skipping turns out to be where the bugs live, the old
record's own exit is still there and still cheaper: a vertex count, an index count and two arrays, written
by a script in `Scripts/` and read by about forty lines. **That is still no dependency**, and it should be
reached for before any library is.

## Measurements

**One figure, and it is arithmetic on the format rather than an observation.** CMO's vertex is fixed at
position, normal, tangent, color and one texture coordinate — **12 + 12 + 16 + 4 + 8 = 52 bytes, of which
the 24 for tangent and texture coordinate are dead here**, 46%. The reader confirms the layout against the
format before anything depends on it.

### The package cost — 2026-09-22, M1.9

**52 KiB.** The thirteen delivered `.cmo` files are **430 KiB on disk** and **53,469 bytes deflated**,
which is 12.1% — 188 KiB of the on-disk figure is literal zeros, the tangent and texture coordinate this
content does not use, and those cost essentially nothing in a package.

**Measured by deflating the thirteen files, not by reading a built appx's block map**, because the deploy
used here is loose-file registration and no `.appx` was produced. An appx **is** a zip with deflate, so
the proxy is faithful; it is stated rather than glossed because the two are not the same command.

**Against the 6.3 MB [`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md) saved by not shipping a
painted cubemap**, the entire mesh set is **0.8% of it**. That comparison is the one figure in this tree
that says what a megabyte of content is worth here, and the answer it gives is that geometry is not where
package size lives — a single baked texture is two orders of magnitude more.

**In VRAM the set is 266 KiB rather than the 423 the 52-byte vertex implies**, because the client uploads
a 32-byte vertex. See Q44: the decision not to repack was not overturned, the cost it was weighing
disappeared into the handedness conversion.

### The asteroid field — 2026-09-23, M2.4

**Five variants, five draws**, against the one draw `TechnicalDesign.md` §6 stated before this record.

**102 KiB on disk and 16 KiB deflated** for the five files: 20,985 bytes each, 104,925 together, and
16,724 deflated. **The deflated figure is not comparable to the 52 KiB above to the byte.** It was taken
with zlib at level 9 in raw deflate, which puts all thirteen files at 51,854 bytes against the 53,469 the
M1.9 measurement's tool produced. Measured on the files in `Assets/Meshes/`; no appx was built.

**In VRAM the field is 552 KiB at two players and 1,104 KiB at four.** That is 378 vertices a rock at a
32-byte vertex and a 2-byte index, over 44 or 88 rocks. It is arithmetic on the layout, not a reading
from the device. Instanced, the five variants would have been about 63 KiB, and the difference is the
price of drawing a three-axis turn without a second shader. It sits in the upload heap, which `MeshBuffer`
says is read across the bus each frame. At two players it more than doubles the 266 KiB the thirteen
meshes cost. **Whether that shows in frame time is owed**, against the 1,482 microseconds M1.9 measured,
on the device.

**The exact radii are 32.6, 46.7, 60.1, 70.9 and 94.4 units** for `AsteroidA` to `E`. They were read
through `NeuronClient/CmoReader` from the shipped files. The catalog's box corners, which the client falls
back to for a variant that did not load, are 48.8 to 135.3.

One is still owed:
1. **Whether the hulls read at the tactical zoom** (M2.13), which is looked at on the device rather than
   computed, and which this decision exists to make answerable. **They have been looked at close up**, on
   the device at M1.9, and what that found was a bug rather than an answer: the light rig was never
   converted out of the authored frame, so the key pointed nearly along the plane and every hull read as
   shapeless. Fixed; the tactical-zoom question is untouched and still M2.13's.
