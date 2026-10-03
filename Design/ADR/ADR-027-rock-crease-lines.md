# ADR-027 — Every model is low-poly and shows its ridges as thin lit lines

Status: **accepted** · 2026-10-02

## Context

On 2026-10-02, during task C.1 (ADR-026), the owner asked for a thin line on every edge of an asteroid's triangles, for the vector look of the eighties. The first try drew every triangle edge of the one 514-triangle `Asteroid` mesh in white, which buried the rock in lines. Drawing only the edges where the surface folds, in the rock's own color made brighter, read better, but on that mesh nearly every fold is a few degrees and the lines barely showed. The owner asked whether fewer faces would help, then asked for three low-poly rocks to replace the old one. With those rocks, the owner judged the look "perfect" and kept it. The owner also asked for the faces to be darker, so that the lines stand out more. The ships and structures were too dense for it, at 280 to 1,150 triangles and 330 to 970 creases each, so the owner made new low-poly ones and asked on 2026-10-02 for the same lines on them.

## Decision

1. **Three low-poly rocks replace the old asteroid**: `Art/Models/Asteroids/Small.glb`, `Medium.glb` and `Large.glb`, with 32, 54 and 78 triangles. Each is the convex hull of points spread over a jittered ellipsoid, with a few corners pushed in, and flat-shaded. The tips of the long axis lie on it, so that no corner reaches past the half length the game fits the mesh to (ADR-018). They were generated once by a script, which the owner removed on 2026-10-02 once the rocks were kept; they are now sources like any other model and are baked as usual. The ships and structures are the owner's own low-poly models, made in the same spirit. `GameClient` picks the small rock up to a radius of 35 m, the large one from 55 m and the medium one between.
2. **`Neuron::BuildCreaseLines` turns a mesh into its creases.** It welds corners by position, keeps every edge whose two faces fold by more than an angle and every edge with only one face, and returns them as a line list. Each line's normal is the mean of its faces' normals, so it is lit as the surface beside it. The lines lie on the edges; what keeps them in front of the faces is [ADR-040](ADR-040-lines-over-dark-faces.md)'s. Every model uses 10°. `GameClient` builds each model's lines once, at start.
3. **`Neuron::MeshPipeline::DrawLines` draws a line list with the mesh pixel shader** and a vertex shader of its own ([ADR-040](ADR-040-lines-over-dark-faces.md)). A second pipeline state rasterizes lines. It tests depth with less-or-equal and writes none, so a line never hides another. A line is one pixel wide at any distance. A frame's model lines are queued as their models are drawn and drawn together after every model and shard, in one switch to the line state and back.
4. **The lines are brighter than the faces, and the faces are darker than the model's color**, so that the lines stand out on the lit side and the dark side alike. How much is [ADR-040](ADR-040-lines-over-dark-faces.md)'s, and a rock's lines are [ADR-028](ADR-028-vector-grid-and-crosses.md)'s. The color is the one the model had before: a field's rocks are 0.7 of an asteroid's, and a structure is darker while it is built. An explosion's shards take the faces' color.
5. **A Mining Rig stands on its asteroid, on its legs** ([ADR-044](ADR-044-rig-stands-on-its-legs.md)).

## Consequences

- **The lines cost a second draw for each mesh a frame draws** ([ADR-053](ADR-053-instanced-meshes.md)), but the frame switches state only twice for all of them. That is computed, not measured, and the owner's Q4 measurement includes it. The new models have 86 to 277 creases a ship and 249 to 736 a structure.
- **The lines are antialiased by the scene's multisampling** ([ADR-040](ADR-040-lines-over-dark-faces.md)).

## What this forecloses

- The 514-triangle `Asteroid` mesh, which is deleted.
- Lines drawn in a width in meters. A line is a pixel, so its weight does not change as the camera zooms.
