# Outpost Commander — MVP mesh handoff

**Date:** 2026-09-22 · **Revision 2** — triangle budget withdrawn · **Scope:** the five shapes the MVP
builds, as CMO content · **Status:** handoff, one gate failed on purpose

This folder is self-sufficient. Every figure below is either **echoed** from the brief (marked so) or
**decided here** (marked so). Where the brief disagrees with itself, both readings are left standing and
the disagreement is named.

| File | What it is |
|---|---|
| `meshes.json` | the geometry — per mesh, face-split vertex positions, per-face normals, vertex-colour selector, 16-bit indices, in world units |
| `materials.json` | hull palette, team-colour convention, the one material, the light rig |
| `render.js` | flat-shaded z-buffered software rasteriser. **The plates are rendered by this, from `meshes.json`.** |
| `Outpost Commander Meshes.dc.html` | the design canvas — open it; every plate is rasterised live in the browser from the published arrays |
| `frames/01…07.png` | the plates, pre-rendered at authored pixel scale |
| `CLAUDE_CODE_PROMPT.md` | paste into Claude Code to build the reader, the converter step and the renderer path |

---

## 1. The answer to the hard question, first

**The silhouettes do not survive at 3.5 authored pixels. Geometry alone fails the gate.**

Plate 1 is the acceptance plate and it is rendered at true authored scale with nothing magnified. At
17.06 units per authored pixel a Scout is 3.5 px and a Frigate is 5.3 px. What actually survives at that
size, measured off the render rather than asserted:

- **Gross area** survives. A Frigate covers roughly 2.1× the pixels of a Scout. A reviewer can see that
  one cluster is heavier than another.
- **Aspect ratio survives weakly.** The Scout is a blunt wedge at 1.11 : 1 in plan; the Frigate is a long
  cruciform spine at 2.14 : 1. At 3.5 px versus 5.3 px, after antialiasing, that is a difference of about one and a
  half pixels along one axis. It reads when the two are adjacent and stationary. It does not read when a
  hull is rotated to a diagonal heading, and heading is continuous.
- **Plan outline does not survive at all.** The Frigate's outrigger step is 14 units — under one authored
  pixel. The Scout's blunt nose versus the Frigate's point is one pixel. The notch, the chine,
  every feature that makes plate 2 legible is gone.
- **Team colour survives easily and is not the problem.** Owner reads at 3.5 px. Role does not.

So: **own-versus-hostile is solved by geometry. Miner-versus-fighter is not.** The brief's own fallback is
the honest answer and it is specified in plate 7.

### What the overlay has to be

A screen-space glyph drawn by the interface at constant pixel size, stroked in the owner's team colour,
centred on the entity's plane position:

- **Ring, 14 px across, 1.25 px stroke** — miner.
- **Chevron, 14 px across, 1.25 px stroke, pointing along the entity's heading** — fighter.
- Constant size at every zoom, so it is 4× the hull at tactical and a quarter of it at combat. It should
  **fade out below roughly 8 units per authored pixel**, where the hull itself starts carrying the read —
  otherwise it fights the geometry in the view where the look is judged.
- It costs no triangles and no draw call per entity; it is one instanced quad pass or one batched
  line list in the HUD layer, which already exists.

**This is a readout that is not on the MVP list.** It was asked for and approved on 2026-09-22 before
being drawn. It needs a question-register entry and a HUD-handoff amendment before it is built. Nothing
else on this page invents a readout, a state or a shape.

### The consequence I took, having been told the overlay carries identity

Because the overlay takes over the tactical read, the hulls are **tuned for the combat view** rather than
bent into shapes that lose a fight at 3.5 px anyway. That is why the Scout has a chined keel and a dorsal
ridge worth 48 triangles instead of a flatter 24-triangle plate: none of it is visible at tactical, all of
it is visible at 53 px, and the overlay makes that trade legal.

---

## 2. The shape language — **DECIDED HERE**

Plan view designed first, profile second, as instructed.

| Mesh | Longest | Plan aspect | Plan reading | Tris | Verts | Y extent |
|---|---|---|---|---|---|---|
| `Scout` | 60 Z | 1.11 : 1 | **blunt wide wedge** — eight-point angular plan, flat nose, hard shoulders | 158 | 474 | −8 … +10 |
| `Frigate` | 90 Z | 2.14 : 1 | **long pointed spine** with a hard outrigger step — cruciform | 190 | 570 | −9 … +10 |
| `ModuleFrame` | 83.5 | — | **octagonal beamed ring** — open, four corner nodes, cross-braced | 248 | 744 | −9 … +9 |
| `ModuleShipyardL1` | 90 Z | — | **open lattice spine** — three N-truss bays, six voids, heavy command block aft | 204 | 612 | −10 … +20 |
| `ModuleShipyardL2` | 90 Z | — | **four bays, eight voids**, second stacked block, mast | 264 | 792 | −10 … +34 |
| `ModuleOreProcessorL1` | 78.4 X | — | **one closed 16-sided drum**, top hatch, three intakes | 212 | 636 | −11 … +17 |
| `ModuleOreProcessorL2` | 88.3 X | — | **two drums** joined by a transfer trunk | 268 | 804 | −11 … +17 |
| `Station` | 220 | — | **chamfered plus plate**, 8-sided hub tower, 2 mounts, 4 dock nodes | 500 | 1500 | −12 … +42 |
| `AsteroidA…E` | 62 / 82 / 111 / 128 / 167 | — | nine-sided, seven-section irregular lumps | 126 each | 378 each | symmetric about Y = 0 |

**Totals: 13 meshes, 2,674 triangles, 8,022 vertices.**

Every hull straddles the plane, as required — the mesh origin is the simulated position and Y = 0 cuts
through the hull, not under it. The tactical view sees only the top; the combat view sees all of it.

**Why blunt-versus-pointed and not wide-versus-narrow alone.** Two hulls that differ only in aspect are
the trap the brief names. Blunt-nosed versus pointed is a second, independent axis, and it is the one that
survives longest as the hull rotates: an aspect difference vanishes at 45° of heading, a nose does not.
It still does not survive to 3.5 px. It survives to about 9 px, which is where the overlay should be
fading out.

**Why the modules read the way they do.** Type is coded as **open outline versus closed mass** — a
shipyard is an open braced truss you can see through and a processor is a solid drum. The lattice
language is taken from the reference image supplied 2026-09-22: a thin-boom spine with converging outer
booms, diagonal cross-bracing, and a dense command block at the aft end. Lattice members run 3–5 units in
section on 18-unit bays (and `ModuleFrame`'s ring beams 12) — a void-to-member ratio of roughly 5:1,
which is what makes it read as open. They are sub-pixel at tactical; the lattice is a combat-view asset. At ~5 px the voids collapse but
the **heavy-end asymmetry survives**, which is a better cue than the symmetric gantry it replaced — a
shipyard has a bright mass at one end and a processor does not. Level is coded as **one more
count** — a fourth braced bay with a second stacked block and a mast, a second drum. Type first, level second, as instructed; at ~5 px plan, the
open/closed distinction is the last thing to go and the count step goes well before it. Level 1 versus
level 2 needs magnification or the interface. Plate 4 is the proof plate for this.

**The station is a plus, not a disc.** A disc at 12.9 px is a blob and reads as a very large asteroid.
Four arms at 90° read as built structure at any rotation, and the arms give the two point-defence mounts
somewhere to be that is actually visible.

---

## 3. Sizes — **ECHOED**, unmoved

60 / 90 / 90 / 220, and asteroids 60–180. I moved nothing, so Q37's recommendation stands as-is.

Two notes rather than changes:

- `ModuleOreProcessorL1` is 78.4 units across, `ModuleOreProcessorL2` is 88.3 and `ModuleFrame` is 83.5 —
  all drums and rings inscribed in the 90-unit module envelope rather than 90-unit objects. All three are
  inside the envelope; none claims the full figure.
- **The asteroid variants came out at 62, 82, 111, 128 and 167 units longest, not 60–180.** The seeded
  radial jitter that makes them irregular also makes their extents irregular. With the authored scale
  jitter of 0.75–1.35 applied the field spans 47–225 units, which covers the stated range and overshoots
  it at the top. **Flagged, not reconciled** — if the catalog asserts 60–180 as hard bounds, the jitter
  range has to come down to 1.08 at the top and the A variant has to grow.

**The packing constraint is respected and it is the reason the station stays at 220.** Plate 4 places four
modules at radius 354 from the station centre, at 45° to the arms. Station arm reach is 110; module
half-diagonal is 64. Clearance to the station is 354 − 110 − 64 = 180 units. Clearance between adjacent
modules is 500 − 128 = 372 units. A 300-unit station leaves 354 − 150 − 64 = 140 and starts crowding the
arms at the diagonals; **the 400-unit radius is not available to pay for a bigger station** and I did not
try to spend it.

---

## 4. Origin, orientation, extents — **ECHOED**

Left-handed, **Y up, +Z forward**, plane at **Y = 0**, one authored unit = one world unit, **meshes are
not scaled at draw time**. A checker can assert `extents.size` in `meshes.json` against the catalog
directly; the numbers in the table above are read out of that file, not typed in — plate 6 prints them
from the same source at render time.

---

## 5. Materials and team colour — **palette and convention DECIDED HERE**

**One material per mesh.** `OC_Hull`, identical values on every mesh, all eight CMO texture slots empty.
Diffuse is white because the albedo arrives entirely through the vertex colour channel — a reader that
ignores vertex colour renders every hull flat white, which is the intended loud failure rather than a
subtle one.

### The vertex-colour selector

| Channel | Meaning |
|---|---|
| **R** | team selector. `0` = take the hull palette. `255` = take the owning player's team colour. Anything in between is a linear blend and is **reserved** — no MVP mesh emits one, and a reader may treat an out-of-set value as a content error. |
| **G** | hull tone index, quantised to exactly three authored values: `0` → `HULL.DEEP`, `128` → `HULL.BASE`, `255` → `HULL.EDGE`. Ignored wherever R = 255. |
| **B** | reserved, always `0`. |
| **A** | always `255`. |

`albedo = lerp(HULL[G], TEAM[owner], R/255)`, with the owner colour arriving as per-instance constant
data. **One instanced draw per shape covers every owner at every player count** — the owner is never a
material and never a second submesh.

**I did not take a second material.** It would buy a specular difference between team panel and hull
plate, and cost one extra draw call per shape — 13 extra draws — for an effect invisible at 3.5 px and
marginal at 79 px.

### The hull palette — **DECIDED HERE**, nothing in the design stated one

| Token | Hex | Where |
|---|---|---|
| `HULL.DEEP` | `#1B212A` | undersides, keels, shadowed structure. About 4× the backdrop's luminance, so a hull bottom separates from `#04060A` instead of merging into it. |
| `HULL.BASE` | `#414B58` | the bulk of every hull; the silhouette-carrying tone. 3.9 : 1 against the backdrop. |
| `HULL.EDGE` | `#828E9C` | upper chines and top faces. 12.1 : 1 against the backdrop, and deliberately below the 45% starfield peak so a hull never out-reads a star. |

Three near-neutral cool greys at chroma low enough (oklch C ≈ 0.014–0.018) that none can be mistaken for
any of the four team hues, for damage `#FF3B2F`, or for armed `#FFB020`. The nearest collision in the
whole set is `HULL.EDGE` against `TEAM.D #A45CFF` — 75° apart in hue. No hull tone is within 40° of
damage red or armed amber, so the damage bar drawn over a hull always separates from it.

**How much of a hull is team-coloured, and why.** Ships carry the team colour on the **upper side band**
and keep `HULL.EDGE` grey on the dorsal cap; the station carries it on the **arm top faces** and keeps
the hub tower grey; modules carry it on their **top faces**. That gives a team-coloured ring around a grey
spine in plan view — enough colour to identify the owner at 3.5 px, enough grey left that the faceting
still reads as deliberate at 79 px. An earlier pass made the whole upper surface team-coloured and the
hulls stopped looking like objects.

### The light rig — **DECIDED HERE**, because the plates are not reproducible without it

Key `(−0.42, 0.80, 0.38)` at 0.85, pitched steeply so the plan view is lit. Fill `(0.55, −0.22, −0.62)`
at 0.16 so keels do not go to pure backdrop at combat range. Ambient `#334052` at 0.70, multiplied by
albedo and never added flat, so it cannot lift the backdrop off `#04060A`. Full values in
`materials.json`.

---

## 6. Triangle counts — **and why there is no budget**

**The triangle budget is withdrawn, on the owner's instruction of 2026-09-22.** Every mesh below is
authored to what its form needs. There is no ceiling, no per-shape allowance, and no mesh was cut to fit
one.

**What replaces it is not "more".** The brief states a constraint that is not a count: the starfield
ceiling exists *"specifically so that a faceted low-polygon hull reads as deliberate rather than cheap."*
That is a **look** requirement, and it cuts both ways — a hull can be under-built and read as cheap, and
it can be over-built and stop reading as faceted at all. So the rule I authored against is: **spend
freely on section, chine and structure; stay disciplined on the plan outline.**

**That distinction is a finding, not a preference, and it cost me a pass.** The first unbudgeted Scout had
a twelve-point plan over five sections. It came out a near-circular blob — indistinguishable from an
asteroid at tactical zoom and indistinguishable from nothing in particular at combat zoom. Adding plan
points does not add detail to a silhouette; it *removes* it, because a silhouette is made of corners and
every added point rounds one off. The shipped Scout has **eight** plan points and **158** triangles: the
extra triangles went into five hull sections, two recessed nacelles and a dorsal blister, and the plan
outline got harder rather than softer.

| Mesh | Tris | Where they went |
|---|---|---|
| `Scout` | 158 | Eight-point plan — deliberately the fewest points of any hull — over five sections (deep keel, lower chine, plane, upper chine, dorsal). Two five-sided drive nacelles recessed into the tail, one six-sided dorsal sensor blister. |
| `Frigate` | 190 | Thirteen-point plan: the outrigger step needs four points per side to be a step rather than a bevel. Five sections, two closed outrigger blisters, an aft drive block. |
| `ModuleFrame` | 248 | An actual frame rather than four slabs: eight beamed ring segments with end caps, four five-sided corner nodes, four inner cross-braces. The hole is the reading and the bracing is what makes it read as a frame and not a washer. |
| `ModuleShipyardL1` | 204 | **The one mesh where fewer triangles made it more open, and openness was the point.** Keel boom, two converging outer booms, four transverse frames, three N-truss bays (one diagonal per bay per side), two gantry posts, and a command block with a bridge and two side pods. K-bracing was tried first and rejected: four diagonals per bay fill the voids they brace, and the result read as a perforated slab rather than a lattice. See below. |
| `ModuleShipyardL2` | 264 | The same lattice with a fourth bay, a third gantry post, a second stacked block and a mast — eight voids against six. The count step is legible in profile as well as in plan. |
| `ModuleOreProcessorL1` | 212 | Sixteen-sided, not eight: the point of the drum is that it is *machined*, and at 53–79 px eight sides reads as a nut. Four sections give it a rim and a shoulder; plus a top hatch and three intake pods. |
| `ModuleOreProcessorL2` | 268 | Two drums at fourteen and ten sides, joined by a beamed transfer trunk, two intakes. The trunk is what makes it read as one machine with two stages rather than two machines parked together. |
| `Station` | 500 | The most expensive mesh, and the only one where the combat view has real work to do. Twenty-four-point chamfered plus outline over five sections, an eight-sided four-section hub tower, two six-sided point-defence mounts, four arm-end docking nodes. |
| `Asteroid`×5 | 126 each | Nine sides, seven sections. **This is the one place I held back deliberately** — twelve sides and seven sections made them smooth, and a smooth rock stops reading as a rock and starts reading as a sphere. Nine sides with strong seeded radial variance keeps the faceting the brief asks for. |

**A second finding, from the same family as the first: on an open structure, fewer members read as more
open.** The first two attempts at the shipyard both failed as lattices for the same reason — the members
were thick relative to the bays. Version A was six axis-aligned slabs with two rectangular holes; version
B was a K-braced truss at 264 triangles whose four diagonals per bay filled the voids they were bracing,
so at any zoom it read as a perforated plate. The shipped mesh is **204 triangles — fewer than version B —
and it is the first one that reads as a lattice**, because the members came down to 3–5 units on 18-unit
bays and the bracing went to a single diagonal per bay per side. A void has to be several times the width
of the member bounding it or the eye fills it in. That is a ratio, not a count, and no triangle budget
would have found it either way.

**The honest summary of the change.** Withdrawing the budget took the bundle from 1,002 triangles to
2,674 — 2.7× — and it bought a great deal at 53–193 px and **nothing whatsoever at 3.5 px.** The
acceptance gate is unmoved: plate 1 fails on geometry before and after, for the same reason, and the
overlay is still required. If anyone reads this section as the budget having been the problem, it was not.

## 7. The modules — **four distinct meshes, DECIDED as ruled**

Five module-class files ship: the bare `ModuleFrame` plus `ModuleShipyardL1/L2` and
`ModuleOreProcessorL1/L2`.

**What it costs:** four extra files (≈ 151 KB of the package), and **four extra instanced draws per frame**
instead of one — five where one would have done under the old rule. Set against a maximum of four modules
per base and a two-player MVP, that is at most 8 module entities on the map covered by 5 draws.

**What it buys:** the raid. A base's modules are individually destroyable and the entire argument for
making them separate entities is that killing the ore processor specifically is a move. That move needs an
attacker who can pick the right target from the near-top-down tactical view. Plate 4 is where that is
proved: open gantry versus closed drum survives to about 5 px because it is a topology difference. The
level step — one cross-beam, one extra drum — does not, and needs magnification or the interface.

**The alternative I rejected:** one `ModuleFrame` with type coded into the vertex-colour channel. It costs
one draw and zero extra files, and it puts type into a colour channel that is already carrying the team
selector, at a size where colour is the only thing that reads. It would have made every module look like
every other module with a different tint, which is the failure the separate-entity design exists to avoid.

---

## 8. The asteroids — **five variants, DECIDED as ruled**

Five files: `AsteroidA` … `AsteroidE`, seeds 10711 / 20443 / 31337 / 40961 / 55501, recorded in
`meshes.json` per mesh so any of them can be regenerated or replaced deterministically.

**The jitter the client applies per rock, which the variant set was designed for:**

- **Yaw, pitch and roll: unconstrained, 0–360° on all three.** Rocks have no up. Every variant was
  authored as a closed irregular solid with no flat bottom and no preferred axis, specifically so that an
  arbitrary orientation is always a legal orientation. This is the constraint that made me use five
  lofted sections rather than a plate.
- **Uniform scale: 0.75 – 1.35.** Non-uniform scale is not supported — it would break the baked per-face
  normals, which are not renormalised anywhere in this chain.
- **Off-plane offset: ±240 units in Y.** Rocks are the only thing permitted above and below the plane.

Plate 5 shows all five variants at both scale extremes and a 34-rock field at true tactical scale.

**Two consequences, stated rather than left to be discovered:**

1. The field is **one draw call per variant — five — not one for the whole field.** The technical design
   currently states one. That is a correction the tree needs.
2. Each variant is package bytes: 126 triangles → 378 vertices → ≈ 20 KB of CMO each, ≈ 101 KB for the
   set. Neither figure is near any limit at this scale, but both are stated differently in the design.

---

## 9. What the bundle costs

**Package bytes.** CMO's vertex is fixed at position `float3` + normal `float3` + tangent `float4` +
colour `uint32` + one texture coordinate `float2` = **52 bytes**, of which **24 are dead weight here** —
the tangent and the UV are written as zeros, as the brief requires. 8,022 vertices × 52 = 407.4 KB, plus
15.7 KB of 16-bit indices, plus 2.8 KB of material and submesh blocks across 13 files (roughly 220 bytes
each). All byte figures in this handoff are KiB — 1,024 bytes — throughout.

> **≈ 426 KB total, of which ≈ 188 KB is zeros.**

A format with a packed vertex would carry the same geometry in about 235 KB — 28 live bytes per vertex
plus the indices. That is not an argument
against CMO at this scale, but it is the number to have in hand: **46% of the vertex payload is tangent
and UV that nothing reads.**

**Draw calls.** **13 instanced draws per frame** at full load — one per shape, covering every owner at
every player count:

`Scout` · `Frigate` · `ModuleFrame` · `ModuleShipyardL1` · `ModuleShipyardL2` · `ModuleOreProcessorL1` ·
`ModuleOreProcessorL2` · `Station` · `AsteroidA…E`

Under the old one-mesh-per-role rule it would have been 5. Four of the eight extra draws are the module
decision and five are the asteroid variants. Adding `Cruiser` later makes it 14.

**Cost of authoring `Cruiser` later — not authored here, per the brief.** One file, ≈ 63 KB at this
density, one draw.
The real cost is not bytes, it is that **the shape language has one axis left.** Scout took blunt-and-wide,
Frigate took long-and-pointed-with-outriggers, Station took radial-arms. A four-slot heavy has to be
long-and-blunt-and-massive — the remaining quadrant — and at tactical zoom it will be 10.5 px, which is
close enough to the Station's 12.9 px that the two will confuse each other in plan. So: *"reinstating the
heavy design is a table row"* was true when a mesh was a function call with a size parameter. Under CMO it
is a file, a draw call, and **a shape-language decision that has to be made against three shapes that are
already spent.** That is the claim CMO weakens, and it should be written back onto the question register
rather than left in the design as-is.

---

## 10. Where this brief disagrees with itself — **flagged, both readings left standing**

1. **Combat camera distance.** The brief gives the close end as camera **~1,400 units** *and* as **~1,638
   units visible width / 1.14 units per authored pixel / hulls at 53 and 79 px**. At vfov 40° on a 3 : 2
   frame those are not the same camera: 1,400 gives 1,529 units of width and 1.06 u/px (hulls at 57 and
   85 px); the stated 1.14 u/px needs **d = 1,500**. A 7% disagreement. **The plates use 1,500** because
   the pixel figures are the ones the acceptance criterion is written in — but 1,400 is the number in the
   camera table and I have not overwritten it.

   > **ANSWERED 2026-09-22 — Q40: 1,400.** A 60-unit hull is 57 authored pixels at 1,400 against 53 at
   > 1,500, both inside the 53–79 this plate was accepted against, so the difference is not visible and the
   > plates stand as rendered. `Interface.md` §5 and ADR-018 both already said 1,400 and
   > `GameClient/Camera.h` was built to it, so correcting this one document beats correcting three. **The
   > plates were rendered at 1,500 and are not re-rendered** — they are evidence of silhouette and scale,
   > and four authored pixels does not change what they are evidence of.

2. **The HUD reference frame is not a combat frame.** The brief offers "34° above the plane at a focus
   distance of 2,600 units" as *"a good middle for a combat plate"*, and separately asks for the combat
   plate to show hulls at 53–79 px. At 2,600 units the scale is 1.97 u/px and a 60-unit hull is 30 px —
   well under the range. **Plate 3 uses 34° at d = 1,500**, keeping the brief's pitch and the brief's
   pixel sizes and dropping its focus distance. Both figures are left standing.

3. **Tactical pitch is unspecified.** "Near top-down" is not a number, and the plan-view plates depend on
   it. **Decided: 85°.** The canvas exposes it as a tweak from 60–90° so the owner can see what the
   ruling costs. At 60° the plan outlines stop being plan outlines and the whole identification argument
   changes.

4. **Asteroid extents versus the 60–180 table.** See §3.

5. **The tree states the withdrawn rule — CORRECTED IN THE TREE, 2026-09-22, and this item was already
   stale when the handoff landed.** This said that ADR-005, `TechnicalDesign.md` §7, `GameDesign.md` §10,
   `Design/README.md`, M1.9, M2.4 and M2.10b all still had meshes generated in code, and told the
   integrator to leave them alone. That correction had already been made: the record is
   `ADR-005-a-mesh-is-a-cmo-file.md`, §7 opens "a mesh is a CMO file", `Design/README.md` records the
   reversal, M1.9 ships three CMO meshes and M2 authors the rocks as CMO variants. There is nothing left
   to correct and nothing to leave alone.

   **The amendment is recorded here rather than in a reply** because this paragraph is inside `Design/`
   and `Scripts/CheckDesign.py` sweeps it: an assertion that the tree says the opposite of what it says
   is a gate failure, and a true one. The checker found this within an hour of the bundle landing, which
   is the whole argument for that gate.

   **What survives is the draw count, and this handoff answers it rather than contradicting it.**
   `TechnicalDesign.md` §7 says the asteroid variant count "is settled at M2.4 and the figure is owed
   there". It is settled here: **five variants**, and with `Scout`, `Frigate`, `Station` and the five
   module shapes that is **13 draw groups**. §7's prose is not wrong, it is owed a number, and the number
   is in `manifest.json`.

---

## 11. What I could not specify, because a number was missing

- **The near and far clip planes.** Nothing in the brief states them, and the 22,500-unit tactical camera
  with hulls 6–44 units tall is a depth-precision question, not a taste question. The rasteriser used to
  render these plates uses a true depth buffer with no near/far at all, so the plates cannot tell you
  whether a 24-bit depth buffer at your chosen planes will z-fight across a station's 10-unit plate step.
  **Ask for a ruling before the renderer path is built.**
- **Whether a mesh may be authored off-origin.** The brief says the mesh origin is the simulated position;
  it does not say whether the *centroid* has to be at the origin. Every hull here is centred in X and Z
  within a unit or two, but `ModuleOreProcessorL2` is deliberately asymmetric in X (its second drum), the shipyard truss is heavily
  asymmetric in Z (the command block), and `Station`'s hub tower puts its volume centroid at Y ≈ +6. If anything asserts a centred centroid, those
  two fail.
- **What an asteroid's ore state looks like.** Rocks are minable; nothing in the brief says a depleted
  rock looks different from a full one, and I did not invent a state for it. If it needs one, it is either
  a vertex-colour tone (free, weak) or a second variant set (five more files).
- **The mining laser and the mass drivers.** Both are listed as things a slot carries, and neither is on
  the list of five. Each hull has its slot positions implied by its geometry but not marked, because a
  slot marker is not a mesh and I was told not to invent one. **If the renderer needs authored attachment
  points, say so** — that is a change to what `meshes.json` carries, not a change to the geometry.
