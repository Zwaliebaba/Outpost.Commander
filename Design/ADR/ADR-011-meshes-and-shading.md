# ADR-011 — Meshes are sized by data and drawn flat-lit in a team color

Status: **accepted** · 2026-09-30

## Context

Milestone 1 puts a ship on screen (design §14), and gate G2 decided how meshes reach the game. Tasks 1.3 and 1.4 built the loader and the first shaded pipeline. How a mesh file is made and read is ADR-018's: each model is baked from a glTF source into an `.nmf` file.

The meshes do not share a scale (design §11). The Tarkan `Medium` is longer than the Tarkan `Large`. R14 rules out DirectXTK and DirectXMesh. The design asks for flat lighting and a team color, with no materials or textures (§11).

## Decision

1. **`Neuron::ParseNmf` in `NeuronClient` reads an `.nmf` file's bytes into a `MeshData`: positions, normals, 32-bit indices, hardpoints (ADR-018) and spinning parts (ADR-045).** Reading is separate from uploading, so the reader is tested without a GPU.
2. **How big a model is is data, in `OutpostCommander/Assets/Models.json`; which way it faces is in its mesh.** Each set has a name, a linear color, and its models, or names another set whose meshes and models it borrows in its own color, as the pirates' set borrows the Tarkan set's ([ADR-073](ADR-073-pirates.md)). Each model has a `lengthMeters`. Every mesh faces +x with +y up (ADR-018). `Neuron::FitMesh` centers the bounds on the origin and scales the mesh, and its hardpoints, uniformly, so that its length along x is `lengthMeters`. +x is the front because the server's heading is counterclockwise from +x (`EntityView::headingRadians`). Each rock is 2 m long, so its scale is its radius (ADR-027). The lengths follow the footprint radii, and both are final (gate G5; owner, 2026-10-01): Small 20 m, Medium 35 m and Large 60 m, about 1.25 times each footprint's diameter.
3. **The set's color is the team color** (owner, 2026-10-01): blue for the player's Human set, orange-red for the AI's Tarkan set, violet for the pirates' set, and gray for the asteroids. A structure's color is its side's taken toward gray, and its faces are darker than a ship's ([ADR-040](ADR-040-lines-over-dark-faces.md)).
4. **Every model is loaded at start, and a model that fails to load stops the game with a message naming the file.** No model is skipped silently. `Models\<set>\<model>.nmf`, or a model's first level ([ADR-045](ADR-045-levels-and-spinning-parts.md)), is read from the package's `Assets` folder. A set that borrows another's meshes reads that set's folder, so the pirates' meshes are loaded a second time, in their own color.
5. **A `Neuron::Mesh` holds its vertices and indices in default-heap buffers that it shares with other static buffers, uploaded once in a batch** ([ADR-048](ADR-048-batched-static-uploads.md)).
6. **`Neuron::MeshPipeline` draws a mesh with one root signature and one pipeline state.** Parameter 0 is a root constant buffer view of the frame's constants: the view-projection matrix, the direction toward the light, an ambient share, and the eye's position for lines (ADR-040). There is one 256-byte slot per frame in flight, in an upload buffer that stays mapped. Each object drawn, its world matrix and its color, is an instance ([ADR-053](ADR-053-instanced-meshes.md)). The shaders are `NeuronClient/Shader/MeshVS.hlsl` and `MeshPS.hlsl`, shader model 5.1, compiled by `FXCompile` into `CompiledShader/` headers with warnings as errors (AGENTS.md §2). Lighting is Lambert from one directional light plus an ambient floor: `color × (ambient + (1 − ambient) × max(0, n·l))`.
7. **The renderer has a 32-bit float depth buffer the size of the back buffer**, cleared to 1 with a less-than test. It is recreated whenever the back buffers are. Back faces are culled. In Direct3D's default, a front face is clockwise, which is how the baked files wind (ADR-018).
8. **A frame is `BeginFrame`, then the caller's draws, then `EndFrame`.** `BeginFrame` clears the scene target and the depth buffer, binds them and sets a viewport over the whole of them. The interface is drawn after `BeginInterface` ([ADR-050](ADR-050-shader-resolve.md)). `EndFrame` presents. The renderer still knows no game concept.

## Consequences

- **A mesh's front can be wrong.** For several Tarkan hulls the two ends are close in size. A wrong one is turned in its source and baked again (ADR-018).
- **`GameAppTests` checks what the owner cannot see at a glance.** Every model the game ships parses and winds clockwise about its normals. The hulls load in size order in both sets, and the same hull is the same length in both.
- **Nothing reads a material.** When materials arrive after the MVP, the sources still hold their texture coordinates, and the format gains a version (ADR-018).

## What this forecloses

- Meshes scaled by anything other than `Models.json`.
- Loading a mesh from disk after startup.
