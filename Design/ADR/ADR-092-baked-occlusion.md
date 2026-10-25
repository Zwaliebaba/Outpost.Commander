# ADR-092 — The baker works out how open each vertex is, and a face's light is darkened by it

Status: **accepted** · 2026-10-11

## Context

[The look plan](../../GameDesign/ImplementationPlan-Look.md)'s review found that the game the owner likes reads as solid because every recess is darker than what stands out, and that the vector look has nothing that does this: a face is lit by [ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md)'s key and fill, by its normal alone. The owner decided gate A3 on 2026-10-11: the baker computes the occlusion from each model's own triangles, so that no model needs a step in Blender and every edit bakes it again. The other way was the owner baking it in Blender into a color attribute. This is task LV2.1.

Three facts shaped it:

- **The sources are flat-shaded and split.** Every one of the 45 sources has three vertices a triangle, 48,948 vertices for 16,316 triangles, so a vertex belongs to one face and its normal is the face's.
- **The models are built from parts that overlap.** A face buried inside another part is shut in wholly, and a face that runs into another part has a corner inside it.
- **The bake must stay the same bytes on every machine**, which `--check` proves in CI ([ADR-018](ADR-018-nmf-and-hardpoints.md) decision 7). CPython's `+`, `-`, `*`, `/` and `math.sqrt` are correctly rounded IEEE doubles; its trigonometric functions come from the platform's C library.

## Decision

1. **`Tools/BakeMeshes.py` works out how open each vertex is, from 0, shut in, to 1, nothing within reach above its surface.**
   - **37 rays** are cast over the hemisphere about the vertex's normal: the middles of a 7 × 7 grid's cells that lie inside the unit disk, lifted straight up onto the hemisphere. Points spread evenly over a disk and lifted so fall as the cosine of their angle from the normal (Malley's method), so the rays count alike in a cosine-weighted share, as diffuse light does. The tangents round the normal are made without trigonometry (Duff and others, 2017). So the bake uses arithmetic and square roots only, and is the same bytes everywhere.
   - **A ray is stopped by any of the model's triangles within a quarter of the model's largest extent**, both faces of each, a spinning part's as it stands at rest ([ADR-045](ADR-045-levels-and-spinning-parts.md)). How open a vertex is is the share of its rays that are not stopped. The reach is a share of the model's size, so it is the same at any size the game fits the model to.
   - **Each vertex is sampled a quarter of the way in toward its triangle's middle**, a ten-thousandth of the extent off the surface. Sampled at the corner itself, a corner where another part begins was counted as half shut in or wholly so, and long triangles showed diagonal streaks of dark between their corners in an offline render of the shipped models. A vertex several triangles use is the mean of its samples; one no triangle uses is open.
   - **The work is cut by distance and by plane:** for each triangle, only the triangles near enough to be reached, and with a corner above its plane, are tested. A bake of every source runs one source a process, a core each.
2. **The NMF layout is version 3** ([ADR-045](ADR-045-levels-and-spinning-parts.md) decision 3 gives the rest of it). A vertex is its position and normal, then a byte of how open it is, from 0 to 255, rounded to the nearest step, then three bytes that are zero: 28 bytes. `Neuron::ParseNmf` reads only version 3, and refuses a vertex whose three bytes are not zero. The three bytes are where task LV2.2's paint will go.
3. **`Neuron::MeshVertex` carries `openness`**, a float from 0 to 1, read from the byte over 255. A vertex made on the CPU, such as a ring's, a line's or the grid's, is open. The mesh pipeline's input layout gives it to the vertex shaders as `OPENNESS`.
4. **A face's light is darkened by how shut in it is, and a line's never is.** `MeshPS.hlsl` multiplies the key and the fill by `1 − occlusionStrength × (1 − openness)`, `openness` interpolated across the face. `occlusionStrength` is a frame constant of `MeshPipeline`, in the slot the frame constants had spare. The game sets it from `SceneShading.h`'s `OCCLUSION_STRENGTH`, 1: a face shut in wholly is black, and one half shut in is lit by half. The lines' shaders take no openness, so a line in a recess stands out the more against the darker faces beside it. `Outpost::FaceLight` darkens its light the same way.

## Consequences

- **Measured in the Linux container,** with CPython 3.13 on 4 cores:
  - A bake of every source took 117 s one source after another and 33 s one a core; the largest, the Human `Shipyard_L5`, 1,236 triangles, took 19.5 s alone. `--check` took 33 s. CI's mesh job has 4 cores and a 10-minute limit.
  - Of the 48,948 vertices, 26% are wholly open, 30% less than half open and 13% shut in wholly: the faces buried inside other parts, which no one sees. The mean is 0.65. The convex rocks are 0.97 open or more everywhere.
  - The 45 `.nmf` files grew by 4 bytes a vertex, 195,792 bytes in all, from 1,375,904 to 1,571,696.
- **The look was judged offline, not in the game.** A numpy render in the container, in ADR-090's light and colors with the models' crease lines, of the Human `Shipyard_L1`, `Shipyard_L3`, `Medium` and Command Stations L3 and L5, and the Tarkan `Shipyard_L3` and `Large`, the last four from the baked files, showed the Shipyards' bays and docks, the Command Station's crown and the ground round cockpits, fins and wing roots darkening softly. The deepest recesses, inside the Tarkan Shipyard's frame, go near black at a strength of 1, and that is where the owner's run may want `OCCLUSION_STRENGTH` lower. The game's run is the owner's.
- **The contrast holds:** a face only ever gets darker, so its lines stand further above it. `SceneShadingTests.KeepsEveryModelsLinesOverItsFaces` now tries every face wholly open, half open and shut in.
- **The tests.**
  - `BakeMeshes.py --self-test`: a triangle with nothing above it is wholly open; a floor under a lid is shut in, and the lid less so; a floor's corners at a wall are shut in by some share, and those out of the wall's reach are wholly open; a wall moved out of reach shuts nothing in; and a bake is the same bytes twice.
  - `MeshDataTests`: the openness read from its byte, a vertex's nonzero padding refused, versions 1, 2 and 4 refused, and the shipped Shipyard and rock read with the openness they were baked with.
  - `SceneShadingTests.DarkensAFaceByHowShutInItIs`.
  - The baker's, `MeshDataTests` and `SceneShadingTests` ran in the Linux container, the tests with gcc 13 and clang 18 against DirectXMath and stand-ins; the four mesh shaders passed glslang's HLSL front end. MSVC, `fxc` and the test DLLs are CI's.
- **A buried corner darkens a seen face.** Where a face runs into another part, the corner inside the part is shut in and the face darkens toward it. That is where a recess is, and the inset keeps it from reaching the whole face.
- **A spinning part is shut in as it stands at rest.** As it turns, its occlusion turns with it rather than staying where the body is. The parts are radars and rings, which are much the same all round.
- **Nothing is shut in by another entity:** a Mining Rig is not darkened by its rock, nor a ship by the ship beside it. That would need the scene, not the model.

## What this forecloses

- Occlusion painted by hand, or read from a source's colors, without a new decision: the baker computes it.
- A line darkened by occlusion.
- A version 2 `.nmf`.
