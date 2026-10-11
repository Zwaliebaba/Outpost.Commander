# Outpost Commander — The Lit Vector Look

Status: **open** · accepted by the owner on 2026-10-11, with gates A1–A6 decided and A7 to come · From a review on 2026-10-11 of four screenshots of a mobile war game the owner likes, set against what [ADR-011](../Design/ADR/ADR-011-meshes-and-shading.md), [ADR-027](../Design/ADR/ADR-027-rock-crease-lines.md), [ADR-028](../Design/ADR/ADR-028-vector-grid-and-crosses.md) and [ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) built

The owner asked what it would take to give the game that game's look and feel in space: not its colors, but the way its buildings look and the plates over its squads. The review found two things. The plates carry over as they are. The buildings' look is mostly light and value: soft light from above, occlusion in every recess, a high-key palette and soft shadows on a lit ground. That is the reverse of the vector look, whose lines must stand well above dark faces (ADR-040), and space has no ground. So the owner asked whether the vector look could stay with a different light and palette. It can: the light, the occlusion, a second paint on each model and a phosphor glow on the lines carry over, while the brightness, the cast shadows and the bevels do not. This plan is that "lit vector" look, and the plates.

This plan puts that in order, as a queue of tasks. It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the ADRs the task names.** The review is not in the repository; what it found is summarized below.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in the milestone order below.
3. **A gate is an owner decision.** The task does not start until the owner has answered, and the answer is written into the gates table, with its date, first.
4. **ADRs are edited in place** (AGENTS.md §6). A new one takes the next free number when its task lands.
5. **Every task is presentation.** It is built, its tests run, and the game is run and looked at. The owner's run accepts it. An agent in a cloud container has no Windows, no MSBuild, no shader compiler and no GPU: what is pure runs there in a standalone build, and the rest is CI's and the owner's. The PR says which ran where.
6. **Every look keeps ADR-040's lines over its faces**, by the test LV1.1 adds: no model's lines fall below the floor it sets, on any face the camera can see.

Task numbers carry the prefix LV, and gates are lettered A.

---

## What the review found

| # | Finding | Task |
|---|---|---|
| 1 | **The reference's look is light, not shape.** Its buildings read as solid because of soft light from above, occlusion in every crease, soft shadows on a lit ground and a bright, low-contrast palette. Its shapes are chunky and bevelled. | LV1, LV2 |
| 2 | **Raising the faces toward that brightness undoes ADR-040.** A Human ship's lines stand 2.75:1 above its faces on the darkest face the camera sees, and 3.94:1 on the brightest; a face twice as bright brings the darkest under 2:1, where the faces outshone the lines before ADR-040. | LV1.1 |
| 3 | **The light can change its color without changing that ratio.** A face and the line beside it are lit alike, so the ratio moves only with how bright the light is. A warm key and a cool fill, no darker than today's 0.3 ambient where the camera looks, keep every model within 0.1 of its ratio today. | LV1.1 |
| 4 | **The reference's softness has a vector form: the glow of a vector screen.** The faces are dark, so only the lines, the exhaust, the shots and the blasts are bright enough to glow, and an 8-bit scene is enough to find them. | LV1.2 |
| 5 | **Every model is one color.** The reference paints its side's color on a building's body and keeps its base and trim neutral; a building's flag says whose it is. Every source today has exactly one material, `team`, and no vertex colors or texture coordinates. | LV2, LV3 |
| 6 | **Nothing darkens a recess.** The faces are lit by one light and an ambient floor (ADR-011), so a model reads as flat facets. | LV2.1 |
| 7 | **The reference counts a squad, not a soldier.** A plate over each squad, its count and a badge in its side's color with its kind's glyph, stands in for every health bar and still reads at a glance. The game has no squads: a fleet is ships near each other, and the enemy's groups are not in the snapshot. | LV4 |
| 8 | **Bevels and the crease lines do not mix.** A 90° edge bevelled in three segments is four creases above `CREASE_DEGREES`, so one line becomes four. A single chamfer gives a double line, which can read as a panel's edge. | not a task: the owner's models |

**How the figures were found:** by a Python script in the Linux container that replays `MeshPS.hlsl`'s light, `GameClient.cpp`'s face and line colors and `Models.json`'s side colors, over every unit normal in 3° steps that the camera can see at its lowest pitch, 40° (`Camera.json`): those with y at least −sin 50°. Contrast is WCAG's ratio of the luminances of the linear colors, as ADR-040 computed it. The sources' materials and attributes were read from their glTF JSON.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| LV1.1 | A warm key, a cool fill, and lines that keep their hue | — | A2 | merged, [#107](https://github.com/Zwaliebaba/Outpost.Commander/pull/107); the owner's run accepts it |
| LV1.2 | A phosphor glow | — | A2 | merged, [#107](https://github.com/Zwaliebaba/Outpost.Commander/pull/107); the owner's run accepts it, zoomed in and out |
| LV2.1 | Occlusion baked into every vertex | LV1 | A3 | built; awaiting CI and the owner's run |
| LV2.2 | Paint slots: team, trim and light | LV2.1 | A4 | todo |
| LV2.3 | Two-tone lines | LV2.2 | A4 | todo |
| LV3.1 | Beacons | LV2.2 | A5 | todo |
| LV4.1 | Fleets on the screen | — | A6, A7 | todo |
| LV4.2 | Plates over the fleets | LV4.1 | A7 | todo |
| LV4.3 | Health bars where the plates are not | LV4.2 | A7 | todo |

### Milestone order

LV1, LV2, LV3, LV4.

- **LV1 changes no file format and no art.** It is the cheapest way to see whether the owner likes the direction.
- **LV2's paint slots are one format change**, so the owner's models can take a second paint and lights in one bake.
- **LV3 needs LV2's light slot only for its look,** not to work; it goes after LV2 so a beacon and a lit window are judged together.
- **LV4 is independent of the rest** and can go at any time once A7 is decided.

The owner's models are a track of their own beside the plan: chunky proportions, one silhouette per building kind, a prop that says what it is, a mast or a dish, and `trim` and `light` materials once LV2.2 reads them. They need no task.

---

## Gates

| Gate | Decision | Proposed in | Blocks |
|---|---|---|---|
| A1 | **The ground.** Proposed in the review: held sectors drawn as dark plates tinted in their holder's color, with a glowing band at their borders and the sector's name laid on them, as the reference draws its countries. **Decided on 2026-10-11:** not yet. | the review | nothing |
| A2 | **The light and the glow** (LV1). A warm key and a cool fill from above in `MeshPS.hlsl`, with a soft terminator, no face darker than today's ambient where the camera looks; a glow on what is bright, added in the resolve; and a test that holds every model's lines over its faces. **Decided on 2026-10-11:** as proposed. | the review | LV1 |
| A3 | **Where occlusion comes from** (LV2.1). Proposed: `BakeMeshes.py` computes it from each model's own triangles, so no model needs a step in Blender and every edit bakes it again; the alternative was the owner baking it in Blender into a color attribute. **Decided on 2026-10-11:** the baker computes it. | the review | LV2.1 |
| A4 | **Paint slots** (LV2.2, LV2.3). Proposed: `team`, `trim` and `light`, by material name, in one format version. **Decided on 2026-10-11:** as proposed. | the review | LV2.2, LV2.3 |
| A5 | **Beacons** (LV3.1). Proposed: a hardpoint tag `beacon` that blinks a light in its side's color, so a mast or an antenna can carry one. **Decided on 2026-10-11:** as proposed. | the review | LV3.1 |
| A6 | **Plates over the fleets as a milestone of this plan** (LV4). **Decided on 2026-10-11:** yes. | the review | LV4 |
| A7 | **What a plate is** (LV4.1–LV4.3). Proposed in LV4's tasks: fleets found by nearness on the screen, a plate's count and the arc of its hit points, its hull's glyph, plates that merge or move apart where they meet, a click that selects the fleet, and health bars only on Alt, under the pointer and close in. It rewrites [ADR-047](../Design/ADR/ADR-047-a-fight-seen-whole.md) decision 4 and [ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5. **Open.** | LV4.1 | LV4 |

---

## Milestone LV1 — The light

### LV1.1 — A warm key, a cool fill, and lines that keep their hue

- **Gate:** A2.
- **Goal:** findings 1–3. The reference's mood is warm light on the faces that face it and cool light in their shade.
- **Scope:**
  - **`MeshPS.hlsl` lights a face by a key and a fill.** The key is a warm color from `TOWARD_LIGHT`, whose terminator is softened by a wrap; the fill blends from a cool sky above to a dimmer ground below by the face's normal. Both are frame constants of `MeshPipeline`, in place of the ambient share.
  - **A line takes the light's brightness, not its color,** in a pixel shader of its own, `MeshLinePS.hlsl`. The faces carry the mood; the lines carry whose a model is, and every line the scene draws keeps the color it has: the grid, the sectors' borders, the rings, the selection.
  - **The light is no darker than ADR-040's ambient where the camera looks,** so the ratio of a model's lines to its faces holds.
  - **The light and the colors of a model's faces and lines move out of `GameClient.cpp` into a unit of their own**, so that a test reads what the game draws.
- **ADR:** a new one. It rewrites ADR-011 decision 6's light and ADR-027 decision 3's line shader.
- **Acceptance:** `GameAppTests`:
  - every model's lines stand at least at its floor over its faces, for every side in `Models.json`, ships and structures and rocks, on every face the camera can see: ships 2.7:1, structures 3.3:1, rocks 2.3:1, which are ADR-040's worst less a margin;
  - the light on a face that looks straight up is within a few percent of gray, so a mark that lies flat keeps its color;
  - the key is warmer than the fill.
- **Verify:** CI; run, at the default zoom and close in; **owner run.**
- **As built (2026-10-11):** [ADR-090](../Design/ADR/ADR-090-a-warm-key-and-a-cool-fill.md).
  - The key is (0.627, 0.539, 0.427), the sky (0.365, 0.421, 0.571), the ground (0.314, 0.287, 0.389) and the wrap 0.3. The least light on a face the camera sees is 0.315 in luminance, and the light on a face that looks up is within 7% of gray.
  - `SceneShadingTests` ran in the Linux container with gcc and clang against stand-ins, and on Windows in CI. The owner ran a Release build on 2026-10-11.

### LV1.2 — A phosphor glow

- **Gate:** A2.
- **Goal:** finding 4.
- **Scope:**
  - **`Renderer` adds a glow between the scene and the interface.** Half the screen's resolution and four halvings below it hold what is brighter than a threshold, blurred and added back up the chain, and the resolve adds it to the scene in linear color. The interface is drawn after it and never glows.
  - **The game sets the threshold, its knee, the glow's strength and how far it spreads**, as presentation constants in `GameClient`. The faces stay under the threshold.
  - **It costs a pass per level and one more texture read in the resolve,** measured with `--measure --stress`.
- **ADR:** a new one. It uses what [ADR-050](../Design/ADR/ADR-050-shader-resolve.md) left open: work between the resolve and the interface.
- **Acceptance:** `NeuronClientTests`: the levels' sizes at 1920×1080, at odd sizes and at the smallest window. The look is the owner's run.
- **Verify:** CI; run; `--measure --stress` against the last figures; **owner run.**
- **As built (2026-10-11):** [ADR-091](../Design/ADR/ADR-091-a-phosphor-glow.md).
  - `Neuron::Bloom`: five levels of `R11G11B10_FLOAT`, a bright pass, thirteen-tap down passes and tent up passes, added in the resolve. `SCENE_BLOOM` starts at a threshold of 0.35, a knee of 0.15, an intensity of 2 and a spread of 0.6, chosen by replaying the passes in numpy on a lit ship's line.
  - `BloomTests` ran in the Linux container and on Windows in CI. The owner's Release runs of `--measure --stress` at 1920×1080, this change against `main`, put the glow and the light together at 0.30 ms of GPU work a frame on average: a mean of 0.67 ms against 0.37 ms, and a 99th percentile of 1.19 ms against 0.81 ms.

---

## Milestone LV2 — Two paints and occlusion

### LV2.1 — Occlusion baked into every vertex

- **Gate:** A3.
- **Goal:** finding 6.
- **Scope:**
  - **`BakeMeshes.py` computes each vertex's occlusion** by casting a fixed set of rays over the hemisphere about its normal against the model's own triangles, within a reach that is a share of the model's size. The directions are made with arithmetic and square roots only, which IEEE doubles round the same on every machine, so `--check` stays byte for byte (ADR-018 decision 7).
  - **The NMF gains a version**, with a byte of occlusion per vertex ([ADR-045](../Design/ADR/ADR-045-levels-and-spinning-parts.md) owns the layout). `ParseNmf` reads it, and a `MeshVertex` made on the CPU, such as a ring's, is unoccluded.
  - **The occlusion darkens a face's light, and never a line's**, so a line in a recess stands out more.
  - **The bake's time is measured** over every source, and CI's mesh job keeps running it.
- **ADR:** ADR-045's layout and ADR-018's bake are rewritten.
- **Acceptance:** the baker's `--self-test` checks an open face unoccluded and a face inside a box occluded; `MeshDataTests` read the byte; the contrast test of LV1.1 holds with the darkest occlusion the shipped models bake.
- **As built (2026-10-11):** [ADR-092](../Design/ADR/ADR-092-baked-occlusion.md).
  - 37 rays a vertex, cosine-weighted, sampled a quarter of the way in toward the triangle's middle, within a quarter of the model's extent. NMF version 3: a byte of openness and three zero bytes a vertex. A face's light is darkened by `OCCLUSION_STRENGTH`, 1, and a line's never.
  - A bake of every source took 117 s one at a time and 33 s on 4 cores in the Linux container; the largest model 19.5 s.
  - The baker's self-test, `MeshDataTests` and `SceneShadingTests` ran in the Linux container; the shaders passed glslang. The look was judged in an offline render of seven models; the game's run is the owner's.

### LV2.2 — Paint slots: team, trim and light

- **Gate:** A4.
- **Goal:** finding 5.
- **Scope:**
  - **A face's paint is its material's name:** `team`, `trim` or `light`. A source with no material is `team`. Any other name fails the bake, naming the file, as an unknown hardpoint tag fails the load.
  - **`team` takes the side's color, as every face does today; `trim` a neutral color at the same shade; `light` a bright color that no light reaches**, so that it glows (LV1.2). The ADR picks the trim's and the light's colors.
  - **The paint is a byte per vertex beside the occlusion,** in one of the three zero bytes version 3 keeps after it (ADR-092).
- **Acceptance:** the baker's self-test refuses an unknown material; `MeshDataTests` read the paint; the contrast test covers a trim face and its line.

### LV2.3 — Two-tone lines

- **Gate:** A4.
- **Goal:** finding 5, in the vector look.
- **Scope:** a line takes its faces' paint: team lines in the side's line color, trim lines in a neutral one, light lines in the light's. A line between two paints takes `team`'s. `BuildCreaseLines` carries the paint.
- **Acceptance:** `MeshDataTests`: a crease between two paints, and between two of one paint.

---

## Milestone LV3 — Lights that say whose

### LV3.1 — Beacons

- **Gate:** A5.
- **Goal:** finding 5. A building's flag says whose it is; in space, a blinking light on a mast.
- **Scope:**
  - **`hp_beacon` is a hardpoint kind** (`HardpointKindOf`, [ADR-018](../Design/ADR/ADR-018-nmf-and-hardpoints.md) decision 4). Its glow is in its side's line color, drawn with the exhaust's pipeline ([ADR-019](../Design/ADR/ADR-019-glows-and-exhaust.md)), and it blinks.
  - **No two beacons blink together:** the phase follows the entity's identifier, so the same entity always blinks the same way.
  - **A beacon on a structure being built does not blink**, and a remembered structure's shows none.
- **ADR:** ADR-018's foreclosure of lights is rewritten.
- **Acceptance:** `HardpointsTests`: the tag's kind, the blink's on and off times, two identifiers out of phase, none while built.

---

## Milestone LV4 — Plates over the fleets

### LV4.1 — Fleets on the screen

- **Gate:** A7.
- **Goal:** finding 7: what a plate stands over.
- **Scope, as proposed:**
  - **A fleet is a side's ships within reach of each other on the screen**, so that two fleets whose plates would meet become one as the camera pulls back, and split as it comes in.
  - **A fleet keeps its identity from frame to frame** by the ships it shares with the last frame's, so a plate does not jump as a ship dies or joins.
  - **It is the client's,** from what the snapshot shows: no fleet is added to the simulation. A fleet in the simulation is the horizon's, for orders given in advance.
- **Acceptance:** `GameAppTests`: two groups apart and together at two zooms, a fleet that loses a ship keeping its identity, two sides never one fleet.

### LV4.2 — Plates over the fleets

- **Gate:** A7.
- **Goal:** finding 7.
- **Scope, as proposed:**
  - **A plate is a count over a disc in its side's color**, ringed in white, with its fleet's commonest hull's glyph in white, and no panel behind it: the count is white with a dark halo, in Bahnschrift's bold semicondensed face. It stands over its fleet's models, as a bar does ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5).
  - **The ring is an arc of the fleet's hit points against their full**, since a ship is not one hit as a soldier is; white, so it takes none of health's colors.
  - **A plate of one ship shows its glyph and no count.**
  - **Plates that meet:** two of one side merge; two of different sides move apart up and down.
  - **A click on a plate selects its fleet.**
  - **The halo is drawn into the atlas** beside each glyph, by widening and blurring its coverage, so it costs a second quad a character ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md)); the disc and the glyphs are sprites drawn on the CPU.
- **Acceptance:** `HudTests`: a plate's parts and places, its arc, merging and moving apart, its click, and its text at 4.5:1 against its halo.

### LV4.3 — Health bars where the plates are not

- **Gate:** A7.
- **Scope, as proposed:** a ship's own bar shows while Alt is held, under the pointer, and close in; elsewhere its fleet's plate stands for it. A structure keeps its bar. It rewrites ADR-047 decision 4 and ADR-088 decision 5.
- **Acceptance:** `HudTests.PutsABarOnlyOverWhatShowsOne` for each case.

---

## Not in this plan

- **The ground** (A1): the sector plates, their glowing borders and their names. The owner does not want them yet.
- **The reference's brightness, its soft cast shadows, smooth normals and bevels.** Each undoes the lines (findings 2 and 8).
- **A tone map.** The glow can push a line's brightest channel past 1, where it clips toward white. Whether that shows is LV1.2's run.
- **The owner's models.** Silhouettes, props and paint are modelled in Blender, and need no task.
