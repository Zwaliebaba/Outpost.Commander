# ADR-092 — The baker works out how open each vertex is, and a face's light is darkened by it

Status: **accepted** · 2026-10-11
Owns: baked-occlusion, vertex-openness, occlusion-strength

## Context

A model reads as solid when every recess is darker than what stands out. A face lit by [ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md)'s key and fill, by its normal alone, has nothing that does this. So the baker computes each vertex's occlusion from the model's own triangles, so that no model needs a step in Blender and every edit bakes it again (owner, 2026-10-11). The other way considered is the owner baking it in Blender into a color attribute.

Three facts constrain it:

- **The sources are flat-shaded and split.** Every one of the 45 sources has three vertices a triangle, 48,948 vertices for 16,316 triangles, counted from the sources, so a vertex belongs to one face and its normal is the face's.
- **The models are built from parts that overlap.** A face buried inside another part is shut in wholly, and a face that runs into another part has a corner inside it.
- **The bake must stay the same bytes on every machine**, which `--check` proves in CI ([ADR-018](ADR-018-nmf-and-hardpoints.md) decision 7). CPython's `+`, `-`, `*`, `/` and `math.sqrt` are correctly rounded IEEE doubles; its trigonometric functions come from the platform's C library.

## Decision

1. **`Tools/BakeMeshes.py` works out how open each vertex is, from 0, shut in, to 1, nothing within reach above its surface.**
   - **37 rays** are cast over the hemisphere about the vertex's normal: the middles of a 7 × 7 grid's cells that lie inside the unit disk, lifted straight up onto the hemisphere. Points spread evenly over a disk and lifted so fall as the cosine of their angle from the normal (Malley's method), so the rays count alike in a cosine-weighted share, as diffuse light does. The tangents round the normal are made without trigonometry (Duff and others, 2017). So the bake uses arithmetic and square roots only, and is the same bytes everywhere.
   - **A ray is stopped by any of the model's triangles within a quarter of the model's largest extent**, both faces of each, a spinning part's as it stands at rest ([ADR-045](ADR-045-levels-and-spinning-parts.md)). How open a vertex is is the share of its rays that are not stopped. The reach is a share of the model's size, so it is the same at any size the game fits the model to.
   - **Each vertex is sampled a quarter of the way in toward its triangle's middle**, a ten-thousandth of the extent off the surface, since at the corner itself a corner where another part begins would count as shut in, and long triangles would streak dark between their corners. A vertex several triangles use is the mean of its samples; one no triangle uses is open.
   - **The work is cut by distance and by plane:** for each triangle, only the triangles near enough to be reached, and with a corner above its plane, are tested. A bake of every source runs one source a process, a core each.
2. *(retired)*
3. **`Neuron::MeshVertex` carries `openness`**, a float from 0 to 1, read from its vertex's byte over 255, which the baker writes from 0 to 255, rounded to the nearest step. A vertex made on the CPU, such as a ring's, a line's or the grid's, is open. The mesh pipeline's input layout gives it to the vertex shaders as `OPENNESS`.
4. **A face's light is darkened by how shut in it is, and a line's never is.** `MeshPS.hlsl` multiplies the key and the fill by `1 − occlusionStrength × (1 − openness)`, `openness` interpolated across the face. `occlusionStrength` is a frame constant of `MeshPipeline`. The game sets it from `SceneShading.h`'s `OCCLUSION_STRENGTH`, 1: a face shut in wholly is black, and one half shut in is lit by half. The lines' shaders take no openness, so a line in a recess stands out the more against the darker faces beside it. `Outpost::FaceLight` darkens its light the same way.

## Consequences

- **The contrast holds:** a face only ever gets darker, so its lines stand further above it. `SceneShadingTests.KeepsEveryModelsLinesOverItsFaces` tries every face wholly open, half open and shut in.
- **The tests.**
  - `BakeMeshes.py --self-test`: a triangle with nothing above it is wholly open; a floor under a lid is shut in, and the lid less so; a floor's corners at a wall are shut in by some share, and those out of the wall's reach are wholly open; a wall moved out of reach shuts nothing in; and a bake is the same bytes twice.
  - `MeshDataTests`: the openness read from its byte, a vertex's nonzero padding refused, versions 1, 2 and 4 refused, and the shipped Shipyard and rock read with the openness they were baked with.
  - `SceneShadingTests.DarkensAFaceByHowShutInItIs`.
- **A buried corner darkens a seen face.** Where a face runs into another part, the corner inside the part is shut in and the face darkens toward it. That is where a recess is, and the inset keeps it from reaching the whole face.
- **A spinning part is shut in as it stands at rest.** As it turns, its occlusion turns with it rather than staying where the body is. The parts are radars and rings, which are much the same all round.
- **Nothing is shut in by another entity:** a Mining Rig is not darkened by its rock, nor a ship by the ship beside it. That would need the scene, not the model.

## What this forecloses

- Occlusion painted by hand, or read from a source's colors, without a new decision: the baker computes it.
- A line darkened by occlusion.
- An `.nmf` of any version but [ADR-045](ADR-045-levels-and-spinning-parts.md) decision 3's.
