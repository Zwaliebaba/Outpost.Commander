# ADR-027 — Every model is low-poly and shows its ridges as thin lit lines

Status: **accepted** · 2026-10-02
Owns: rock-meshes, crease-lines, mesh-line-drawing

## Context

The scene has the vector look of the eighties: a thin line where a model's surface folds, on every model (owner, 2026-10-02). A line on every triangle edge buries a model in lines, and on a dense mesh nearly every fold is a few degrees, so its creases barely show. The look therefore needs models low-poly enough that their folds are few and wide, rocks, ships and structures alike, and faces dark enough that the lines stand out on them ([ADR-040](ADR-040-lines-over-dark-faces.md)).

## Decision

1. **The rocks are three low-poly sources**: `Art/Models/Asteroids/Small.glb`, `Medium.glb` and `Large.glb`, with 32, 54 and 78 triangles. Each is the convex hull of points spread over a jittered ellipsoid, with a few corners pushed in, and flat-shaded. The tips of the long axis lie on it, so that no corner reaches past the half length the game fits the mesh to (ADR-018). They are sources like any other model and are baked as usual. The ships and structures are the owner's own low-poly models, made in the same spirit ([ADR-018](ADR-018-nmf-and-hardpoints.md) decision 8). `GameClient` picks the small rock up to a radius of 35 m, the large one from 55 m and the medium one between.
2. **`Neuron::BuildCreaseLines` turns a mesh into its creases.** It welds corners by position, keeps every edge whose two faces fold by more than an angle and every edge with only one face, and returns them as a line list. Each line's normal is the mean of its faces' normals, so it is lit as the surface beside it. The lines lie on the edges; what keeps them in front of the faces is [ADR-040](ADR-040-lines-over-dark-faces.md)'s. Every model uses 10°. The client builds the lines of each piece of each model once, as its assets load ([ADR-045](ADR-045-levels-and-spinning-parts.md) decision 4, [ADR-049](ADR-049-startup-in-parallel.md)).
3. **`Neuron::MeshPipeline::DrawLines` draws a line list with shaders of its own:** `MeshLineVS.hlsl`, which pulls it toward the eye ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 2), and `MeshLinePS.hlsl`, which lights it by the luminance of the light on its normal, in its own hue ([ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md) decision 2). A second pipeline state rasterizes lines, multisampled (ADR-040 decision 3). It tests depth with less-or-equal and writes none, so a line never hides another. A line is one pixel wide at any distance. A frame's model lines are queued as their models are drawn and drawn together after every model and shard, in one switch to the line state and back.
4. *(retired)*
5. *(retired)*

## Consequences

- **The lines cost a second draw for each mesh a frame draws** ([ADR-053](ADR-053-instanced-meshes.md)), but the frame switches state only twice for all of them. The frame's budget is in [`Design/Budgets.md`](../Budgets.md).
- **The lines are antialiased by the scene's multisampling** ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 3).

## What this forecloses

- Lines drawn in a width in meters. A line is a pixel, so its weight does not change as the camera zooms.
