# ADR-027 — Every model is low-poly and shows its ridges as thin lit lines

Status: **accepted** · 2026-10-02

## Context

On 2026-10-02, during task C.1 (ADR-026), the owner asked for a thin line on every edge of an asteroid's triangles, for the vector look of the eighties. The first try drew every triangle edge of the one 514-triangle `Asteroid` mesh in white, which buried the rock in lines. Drawing only the edges where the surface folds, in the rock's own color made brighter, read better, but on that mesh nearly every fold is a few degrees and the lines barely showed. The owner asked whether fewer faces would help, then asked for three low-poly rocks to replace the old one. With those rocks, the owner judged the look "perfect" and kept it. The owner also asked for the faces to be darker, so that the lines stand out more. The ships and structures were too dense for it, at 280 to 1,150 triangles and 330 to 970 creases each, so the owner made new low-poly ones and asked on 2026-10-02 for the same lines on them.

## Decision

1. **Three low-poly rocks replace the old asteroid**: `Art/Models/Asteroids/Small.glb`, `Medium.glb` and `Large.glb`, with 32, 54 and 78 triangles. Each is the convex hull of points spread over a jittered ellipsoid, with a few corners pushed in, and flat-shaded. The tips of the long axis lie on it, so that no corner reaches past the half length the game fits the mesh to (ADR-018). They were generated once by a script, which the owner removed on 2026-10-02 once the rocks were kept; they are now sources like any other model and are baked as usual. The ships and structures are the owner's own low-poly models, made in the same spirit. `GameClient` picks the small rock up to a radius of 35 m, the large one from 55 m and the medium one between.
2. **`Neuron::BuildCreaseLines` turns a mesh into its creases.** It welds corners by position, keeps every edge whose two faces fold by more than an angle and every edge with only one face, and returns them as a line list. Each line's normal is the mean of its faces' normals, so it is lit as the surface beside it. Each corner is lifted along that normal by a share of the mesh's largest extent, because Direct3D gives a line no depth bias. Every model uses 10° and 0.5%. `GameClient` builds each model's lines once, at start.
3. **`Neuron::MeshPipeline::DrawLines` draws a line list with the mesh shaders.** A second pipeline state rasterizes lines. It tests depth with less-or-equal and writes none, so a line never hides another. A line is one pixel wide at any distance. A frame's model lines are queued as their models are drawn and drawn together after every model and shard, in one switch to the line state and back.
4. **The lines are brighter than the faces, and the faces are darker than the model's color.** Every model, rock, ship or structure, has its lines drawn in its color times 2, clamped to 1, and its faces in its color times 0.65; a rock's lines are only 1.35 times its color, since it is terrain (ADR-028). The lines therefore stand out on the lit side and the dark side alike. The color is the one the model had before: a field's rocks are 0.7 of an asteroid's, and a structure is darker while it is built. An explosion's shards take the faces' color.
5. **A Mining Rig stands on its asteroid.** Its feet are its lowest points out past half its radius: its legs' tips, not a drill under its middle. It is lowered until every foot reaches the rock under it, found by `Neuron::SurfaceHeightAt` on the rock mesh turned by its asteroid's heading and drawn at its radius; a leg over higher rock goes into it, and the drill goes in under the rig. A rig without feet over the rock stands its lowest point on the rock's top. Before, it stood 0.7 of the radius up with half the rig inside the rock, then, with the owner's new rig, on its drill's tip with its legs over the rock's slopes.

## Consequences

- **Every model on screen costs a second draw call**, for its lines, but the frame switches state only twice for all of them. That is computed, not measured, and the owner's Q4 measurement includes it. The new models have 86 to 277 creases a ship and 249 to 736 a structure.
- **The lines are not antialiased.** The swap chain has no multisampling, so a line that runs nearly along a pixel row steps. Smoothing it would take multisampling or a post pass, neither of which this ADR adds.

## What this forecloses

- The 514-triangle `Asteroid` mesh, which is deleted.
- Lines drawn in a width in meters. A line is a pixel, so its weight does not change as the camera zooms.
