# ADR-011 — Meshes reach the game as .cmo files, are placed by data, and are drawn flat-lit in a team color

Status: **accepted** · 2026-09-30

## Context

Milestone 1 puts a ship on screen (design §14), and gate G2 decided how meshes reach the game: the owner converts them to DirectX's `.cmo` format, and the game has its own loader. Tasks 1.3 and 1.4 build that loader and the first shaded pipeline.

The meshes do not share a scale or an orientation (design §11). The Human set points along −z and the Tarkan hulls along +x. The Tarkan `Medium` is longer than the Tarkan `Large`. R14 rules out DirectXTK, which reads `.cmo` files, and DirectXMesh, which writes them. The design asks for flat lighting and a team color, with no materials or textures (§11).

The `.cmo` layout was checked against all 29 converted files on 2026-09-30 by a script that parsed every file to its last byte. Each file holds one mesh with one submesh, one vertex buffer of 52-byte vertices, one 16-bit index buffer, no skeleton, and an empty list of skinning buffers. Every triangle in every file winds the same way as its vertex normals. The one exception is one triangle of the Tarkan `Colonizer`.

## Decision

1. **`Neuron::ParseCmo` in `NeuronClient` reads the file's bytes into a `MeshData`: positions, normals and 32-bit indices.** It keeps every mesh and submesh, appended into one triangle list. It skips materials, tangents, vertex colors and texture coordinates. It rejects a file that is cut short, has bytes left over, has a skeleton, or has an index or submesh range outside its buffers. The message names the file and the byte offset. Reading is separate from uploading, so the reader is tested without a GPU.
2. **Where a model faces and how big it is are data, in `OutpostCommander/Assets/Models.json`.** Each set has a name, a linear color, and its models. Each model has a `forwardAxis` (`+x`, `−x`, `+z` or `−z`) and a `lengthMeters`. `Neuron::OrientMesh` centers the bounds on the origin, turns the mesh about y so that its front is +x, and scales it uniformly so that its length along x is `lengthMeters`. The turn is always a rotation, never a mirror, so triangles keep their winding. +x is the front because the server's heading is counterclockwise from +x (`EntityView::headingRadians`). The asteroid is 2 m long, so its scale is its radius. The lengths are provisional, like the footprint radii they follow (gate G5): Small 20 m, Medium 35 m and Large 60 m, about 1.25 times each footprint's diameter. The forward axes come from the shape of each mesh, since the files do not record them, and the owner's run confirms them.
3. **The set's color is the team color until team colors are decided (design §15):** blue for the player's Human set, orange-red for the AI's Tarkan set, and gray for the asteroid.
4. **Every model is loaded at start, and a model that fails to load stops the game with a message naming the file.** No model is skipped silently. `Models\<set>\<model>.cmo` is read from the package's `Assets` folder, and all 29 take a fraction of a second to load.
5. **A `Neuron::Mesh` holds its vertices and indices in default-heap buffers.** `Renderer::CreateStaticBuffer` uploads them once, through an upload buffer, and waits for the copy. Buffers start in the common state and are promoted implicitly, so no barriers are recorded.
6. **`Neuron::MeshPipeline` draws a mesh with one root signature and one pipeline state.** Parameter 0 is a root constant buffer view of the frame's constants: the view-projection matrix, the direction toward the light, and an ambient share. There is one 256-byte slot per frame in flight, in an upload buffer that stays mapped. Parameter 1 holds 20 root constants per object: its world matrix and its color. The shaders are `NeuronClient/Shader/MeshVS.hlsl` and `MeshPS.hlsl`, shader model 5.1, compiled by `FXCompile` into `CompiledShader/` headers with warnings as errors (AGENTS.md §2). Lighting is Lambert from one directional light plus an ambient floor: `color × (ambient + (1 − ambient) × max(0, n·l))`.
7. **The renderer has a 32-bit float depth buffer the size of the back buffer**, cleared to 1 with a less-than test. It is recreated whenever the back buffers are. Back faces are culled. In Direct3D's default, a front face is clockwise, which is how the files wind.
8. **A frame is `BeginFrame`, then the caller's draws, then `EndFrame`.** `BeginFrame` clears both buffers, binds them and sets a viewport over the whole back buffer. `EndFrame` presents. The renderer still knows no game concept.

## Consequences

- **A mesh's front can be wrong.** The Human set is unambiguous: every model is wider at the −z end. For several Tarkan hulls the two ends are close in size, and they are set to +x for consistency. A wrong one is a one-word change in `Models.json`.
- **The draw call count is one per object.** Q4's 200 ships and 40 structures are 240 draws of a few thousand triangles each, well within a frame. If 3.7's measurement says otherwise, instancing per model is the next step, and it is an edit to this ADR.
- **`GameAppTests` checks what the owner cannot see at a glance.** Every model the game ships parses. The hulls load in size order in both sets, and the same hull is the same length in both. Orienting a mesh keeps its winding.
- **Nothing reads a material.** When materials arrive after the MVP, the reader keeps the parts it now skips.

## What this forecloses

- Skinned or animated meshes, until the reader is extended.
- Meshes placed or scaled by anything other than `Models.json`.
- Loading a mesh from disk after startup.
