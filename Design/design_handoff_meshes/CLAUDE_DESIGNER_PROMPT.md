# Prompt for Claude Designer — the MVP meshes

Paste everything below the rule into Claude Designer. **It is deliberately self-sufficient**: Designer
does not have this repository, and the one documented error in
[`design_handoff_hud/`](../design_handoff_hud/README.md) — a readout omitted for a reason that was false —
came from a brief that assumed context the designer did not have. Every figure the mesh pass needs is
restated below with its source named.

**Where the output lands:** this folder, beside this prompt, as `design_handoff_meshes/`.

**The decision this prompt is written against** is
[`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md), ruled 2026-09-22: **a mesh is a CMO file**, authored
as content and shipped with the package under
[`ADR-021`](../ADR/ADR-021-content-ships-with-the-package.md). That record used to decide the opposite and
was **replaced rather than superseded**, under the MVP exception in
[`ADR/README.md`](../ADR/README.md). Read it before this: it carries the two costs that matter — that
CMO's only reader in the wild is the DirectXTK12 R14 closes, so the reader is written here against a
format far heavier than the MVP needs; and that nothing authors CMO natively, so a conversion step lands
somewhere. **It does not settle which**, and the chain is owed at M1.9.

---

You are designing the five meshes for **Outpost Commander**, a touch-only real-time strategy game for
Windows tablets, rendered in Direct3D 12. You do not have the repository. Everything you need is here; if
something you need is missing, **say so rather than inventing it**.

## What you are making

A design handoff that specifies five pieces of geometry completely enough to be built, and proves they
work at the two zooms the game is actually played at. The output is a design bundle, not a 3D file: you
author the geometry as explicit arrays, render the plates *from those same arrays*, and the repository
converts them to CMO.

**The plates must be rendered from the geometry you publish, not drawn.** This is the single hardest
requirement in this brief and it is not negotiable. A silhouette drawn by hand is a promise nobody can
keep; a silhouette rendered from the vertex arrays in your JSON is a specification. If a shape only reads
because of a line you drew that no triangle emits, the shape does not read.

## What you are not making

The sky, the star field and the galaxy band; wrecks, debris, tracers, beams, muzzle flashes and engine
plumes; the HUD, which is already designed; anything for a ship the MVP does not build. **Do not invent a
mesh for anything not on the list of five.** If you think something on that list is unbuildable without a
sixth thing, say so in your report and leave it out of the bundle.

## The world, and the two zooms that decide everything

The playfield is a **plane**. The simulation is two-dimensional — every entity is a point on that plane
with a heading, and nothing in the simulation has a height. The renderer may draw asteroids and debris
above and below the plane so the space reads as a volume; ships do not leave it.

The map is a **16,384-unit square**. The authored frame is **1440 × 960**, scaled 2× onto a 2880 × 1920
13-inch panel at 267 PPI, viewed at 500–600 mm on a kickstand.

The camera always looks at a focus point on the plane, never rolls, and **pitch is coupled to zoom and is
not separately controllable**. Vertical field of view is **40°**. The two ends of the range:

| | Camera | Pitch above plane | Visible width | Units per authored pixel |
|---|---|---|---|---|
| **Tactical — fully out** | 22,500 units | near top-down | ~24,600 units | **17.1** |
| **Combat — close** | ~1,400 units | **30° floor** | ~1,638 units | **1.14** |

A reference frame drawn for the HUD handoff used **34° above the plane at a focus distance of 2,600
units**, which is a good middle for a combat plate.

**The horizon is never on screen** — the 30° pitch floor guarantees it. Depth is read from ship scale,
formation stretch and the shape of world circles, and from nothing else.

**The tactical view is a plan view, and that is where identification has to work.** At 17.1 units per
authored pixel a 60-unit hull is **3.5 authored pixels** across, a 90-unit hull is **5.3**, and a 220-unit
station is **12.9**. This is the trap in the whole exercise: two hulls that differ in *profile* but share a
*plan outline* are identical at the zoom where telling them apart matters most. Design the plan view
first and the profile second.

At the close end the same hulls are about **53 and 79 authored pixels**, which is where the look is
judged but not where the reading is.

## The backdrop

Space is **#04060A**. A generated star field and galaxy band sit behind everything at a hard ceiling of
**12% of full white over any large area**, with only the brightest eight stars reaching 45% over a few
dozen pixels. That ceiling exists specifically so that a faceted low-polygon hull reads as deliberate
rather than cheap. **You are designing against near-black, and the silhouette is the whole instrument.**

## The five shapes

Sizes are a **recommendation on an open question** (Q37, "How big is each hull, in world units?"), derived
from the camera arithmetic above rather than observed. They are yours to move, but **if you move one, say
so and say why** — it is a question the owner rules on, and one of them is a packing constraint rather
than a free choice.

| Shape | Longest dimension | What it is | Slots | Drive |
|---|---|---|---|---|
| `Scout` | **60** | small hull; the MVP's **Miner** is a Scout with a mining laser where a weapon would go | 1 | yes |
| `Frigate` | **90** | medium hull; the MVP's **Fighter** is a Frigate with two mass drivers | 2 | yes |
| `ModuleFrame` | **90** | a base module's frame — one slot, no drive, never moves | 1 | no |
| `Station` | **220** | one per player, fixed, never moves, two point-defense mounts | 2 | no |
| Asteroid | **60–180** | varied; see below | — | — |

**The packing constraint:** four modules must sit inside a **400-unit radius** of the station, clear of it
and of each other. A 220-unit station with 90-unit modules leaves room; a 300-unit station does not. If
your station wants to be bigger, the module radius is not available to pay for it.

**A sixth hull, `Cruiser`, is in the catalog and the MVP never builds it** — four slots, high mass, large.
Under the old rule it had to be reachable by parameter from the same function. Under CMO it needs a file or
it needs nothing. **Do not author it**; say in your report what authoring it later would cost against the
shape language you chose, because "reinstating the heavy design is a table row" is a claim the design makes
and CMO weakens.

## Where the origin is, and which way is forward

Author in **world units, one unit to one unit** — the mesh is not scaled at draw time, so a hull's
authored extent *is* its size, and a checker will assert that against the catalog.

**Left-handed, Y up, +Z forward.** The plane is **Y = 0** and the entity's simulated position is the mesh
origin, so a hull straddles the plane rather than resting on it. State each shape's vertical extent: the
tactical view sees almost none of it and the combat view sees all of it.

## Materials, and how team color works

There are **four team colors** and the local player's is always the cool one. No team is red.

| Token | Hex | Hue | Use |
|---|---|---|---|
| `TEAM.OWN` | `#38D1F5` | 193° | your fleet and station — reserved for the local player at every player count |
| `TEAM.B` | `#C4E838` | 78° | the MVP opponent |
| `TEAM.C` | `#F75FD0` | 318° | third player, later |
| `TEAM.D` | `#A45CFF` | 268° | fourth player, later |

Two signal colors are spoken for and your hull palette must not collide with either: **`#FF3B2F`** is
damage — the lost portion of every hull bar, drawn over the ship in the world — and **`#FFB020`** is armed
state and the module placement radius, drawn on the plane under the base.

**One material per mesh, and team membership rides in the vertex color channel.** The requirement behind
that is a hard one: **one instanced draw per shape covers every owner**, so the owner's color cannot be a
material and cannot be a second submesh. CMO's vertex carries a color, so use it as the selector — state
your convention precisely (which channel, which values mean "hull palette" and "owner's color", what a
value in between means if anything).

If you believe two materials buy something worth an extra draw call per shape, **argue for it rather than
taking it**.

**You are choosing the hull palette, and nothing in the design states one.** It has to read against
#04060A under all four team hues without the team-colored faces disappearing into it, and it has to survive
the damage red being drawn on top of it.

## What CMO will accept

Your geometry has to survive conversion, so author inside these:

- **Vertices are split per face and normals are baked per face.** That is flat shading. There are no
  smoothing groups, no vertex welding and no tangent basis to get wrong.
- **CMO's vertex is fixed**: position, normal, tangent, color, one texture coordinate. Tangent and UV are
  dead weight here and will be written as zeros — **do not design anything that needs them**.
- **Indices are 16-bit.**
- **No textures, no normal maps, no UV layout, no skinning, no bones, no animation clips.** CMO carries
  slots for every one of those and every one stays empty. A ship banks as it turns; that is a transform
  the renderer applies, not animation in the file.
- **Triangles only**, no quads, no n-gons.

**State a triangle budget per shape and defend it.** The design's starting point was "a few dozen
triangles" per hull, and the format no longer forces that — but the look does, and at 3.5 pixels the
budget buys nothing. Spend triangles where the plan view reads and nowhere else.

## The first hard problem: two hulls that are not the same shape at two scales

The design promises "ships that bank as they turn and read as silhouettes". The acceptance gate is a
person looking at a screen: **a field of miners and fighters at the tactical zoom, distinguishable at a
glance.** Not nameable, not inspectable — distinguishable while a hand is on the glass and something else
is on fire.

The 60-unit Scout and the 90-unit Frigate are 3.5 and 5.3 pixels apart in size at that zoom, which is
nothing. **The difference has to be proportion and plan outline** — a wide flat hull against a long narrow
one, or a solid mass against a split one — and it has to survive both being the same team color.

**If you conclude it cannot be done in geometry alone at that pixel size, say so plainly.** The design has
already named the fallback — a shape-coded overlay drawn by the interface rather than more triangles — and
an honest "this needs the overlay" is worth more than a bundle that quietly assumes a zoom nobody plays at.

## The second hard problem: four modules off one frame

A base carries up to **four modules**, each a `ModuleFrame` with one component in it, each destroyable on
its own. The MVP ships two at two levels each: **Shipyard L1 and L2** (which raise the station's build
rate) and **Ore Processor L1 and L2** (which raise what a delivered cargo is worth). A research station is
designed but not built until later.

**The whole argument for modules being separate entities is that a raid can kill your ore processor and
leave.** That move needs an attacker who can pick the right target and a defender who can see what they
lost, both from the near-top-down tactical view where a module is about five pixels across.

Under the old rule this had to be solved with one mesh, geometry and the team-color attribute, with no
textures and no icons. **CMO lifts that: you may author four distinct module meshes.** Doing so costs a
draw call each and a file each, and it is probably the single largest thing this decision buys. **Decide,
state which you chose, and say what it costs.** Level 1 against level 2 of the same module is a second,
weaker reading — get the shipyard-against-processor distinction first.

## The asteroid, and the variation that has to come from somewhere

Under the old rule the asteroid was one function driven by the match seed, so no two rocks were identical.
**A file is one rock.** Rocks are also the one thing permitted to sit above and below the plane, so the
field reads as a volume.

Author a **set of variants** and state how many. The client picks one per rock from the match seed and
applies its own yaw, pitch and scale jitter — state the jitter ranges you designed for, because a variant
set that only works at one scale is a variant set of one.

Two consequences to state rather than leave to be discovered: the field becomes **one draw call per
variant** instead of one for the whole field, and each variant is package bytes. Neither is anywhere near a
limit at this scale, but both are figures the technical design currently states differently.

## Deliverables

| File | What it is |
|---|---|
| `README.md` | the handoff, self-sufficient, every figure final or explicitly marked as echoed |
| `meshes.json` | the geometry: per shape, vertex positions, per-face indices, per-face normals, the vertex color selector, in world units |
| `materials.json` | the hull palette, the team-color convention, material values, and what each one is for |
| `Outpost Commander Meshes.dc.html` | the design canvas — every plate rendered from `meshes.json` |
| `frames/*.png` | the plates, listed below |
| `CLAUDE_CODE_PROMPT.md` | a prompt to paste into Claude Code to build from this bundle |

**The plates, at minimum:**

1. **Plan view, tactical zoom, true scale** — a field of miners and fighters at 3.5 and 5.3 authored
   pixels, own team against hostile team, over #04060A. This is the acceptance plate and it goes first.
2. **Plan view, tactical zoom, magnified 8×** — the same outlines, readable, so a reviewer can see what
   they are being asked to judge.
3. **Combat view, 34° above the plane** — each shape at 53–79 authored pixels, own and hostile.
4. **A base** — station and four modules inside the 400-unit radius, plan view at tactical zoom and
   magnified, with the shipyard-against-processor distinction as the thing being proved.
5. **An asteroid field** — every variant, at the extremes of the scale jitter.
6. **Orthographic three-views** of each shape with the authored extents dimensioned.

## How to work

**Flag rather than reconcile.** Where two things in this brief disagree, or where a number here looks
wrong, **say so in your report and leave both standing**. Do not pick one quietly. The one documented
defect in the HUD handoff was a claim it inherited from its brief and reasoned onward from.

**Mark what you decided against what you echoed.** Sizes, colors, the camera and the tiers above are this
design's and you are restating them; the shape language, the palette, the triangle budgets, the module
scheme and the variant count are **yours**. A decision that lives only in a handoff is a decision nobody
can find later, so list yours explicitly at the end — they have to be written back into the design
documents and onto the question register.

**Do not invent a readout, a state or a shape that is not on the list.** If you want one, ask for it.

## When you are done, tell me

What you decided rather than echoed. Where this brief disagrees with itself. **Whether the silhouettes
survive at 3.5 pixels, honestly** — and if they do not, what the overlay would have to be. What the bundle
costs in package bytes and in draw calls. And anything you could not specify because a number was missing.
