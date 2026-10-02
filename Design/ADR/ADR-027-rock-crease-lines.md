# ADR-027 — Asteroids are low-poly rocks that show their ridges as thin lit lines

Status: **accepted** · 2026-10-02

## Context

On 2026-10-02, during task C.1 (ADR-026), the owner asked for a thin line on every edge of an asteroid's triangles, for the vector look of the eighties. The first try drew every triangle edge of the one 514-triangle `Asteroid` mesh in white, which buried the rock in lines. Drawing only the edges where the surface folds, in the rock's own color made brighter, read better, but on that mesh nearly every fold is a few degrees and the lines barely showed. The owner asked whether fewer faces would help, then asked for three low-poly rocks to replace the old one. With those rocks, the owner judged the look "perfect" and kept it. The owner also asked for the faces to be darker, so that the lines stand out more.

## Decision

1. **Three generated rocks replace the old asteroid.** `Tools/MakeAsteroids.py` writes `Art/Models/Asteroids/Small.glb`, `Medium.glb` and `Large.glb` with 32, 54 and 78 triangles. Each is the convex hull of points spread over a jittered ellipsoid, with a few corners pushed in, and flat-shaded. The tips of the long axis are placed before the hull is built so that no corner reaches past the half length the game fits the mesh to (ADR-018). A fixed seed per size goes through SplitMix64 written out in the script, so a run writes the same bytes on every machine. The sources are baked like any other model. `GameClient` picks the small rock up to a radius of 35 m, the large one from 55 m and the medium one between.
2. **`Neuron::BuildCreaseLines` turns a mesh into its creases.** It welds corners by position, keeps every edge whose two faces fold by more than an angle and every edge with only one face, and returns them as a line list. Each line's normal is the mean of its faces' normals, so it is lit as the surface beside it. Each corner is lifted along that normal by a share of the mesh's largest extent, because Direct3D gives a line no depth bias. The rocks use 10° and 0.5%. `GameClient` builds each rock's lines once, at start.
3. **`Neuron::MeshPipeline::DrawLines` draws a line list with the mesh shaders.** A second pipeline state rasterizes lines. It tests depth with less-or-equal and writes none, so a line never hides another. A line is one pixel wide at any distance.
4. **The lines are brighter than the faces, and the faces are darker than the rock's color.** A rock's lines are drawn in its color times 2, clamped to 1, and its faces in its color times 0.65. The lines therefore stand out on the lit side and the dark side alike. A field's rocks are 0.7 of an asteroid's color before both factors, as before.

## Consequences

- **Every rock on screen costs a second draw call**, for its lines. That is computed, not measured, and the owner's Q4 measurement includes it.
- **The lines are not antialiased.** The swap chain has no multisampling, so a line that runs nearly along a pixel row steps. Smoothing it would take multisampling or a post pass, neither of which this ADR adds.
- **Ships and structures have no lines.** Their meshes are too dense for this method: at a 30° fold Human/Small has 474 creases, Human/Large 682, Human/Station 680 and Tarkan/Colonizer 968, and each has 128 to 252 edges with only one face. At the RTS camera's distance that is a smudge, not an outline. Lines for them wait on low-poly art or a screen-space edge pass, either of which needs its own decision.

## What this forecloses

- The 514-triangle `Asteroid` mesh, which is deleted.
- Lines drawn in a width in meters. A line is a pixel, so its weight does not change as the camera zooms.
