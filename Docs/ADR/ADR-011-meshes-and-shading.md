# ADR-011 — Meshes are sized by data and drawn in their side's color

Status: **accepted** · 2026-10-11
Owns: model-loading, mesh-sizing, team-colors, mesh-pipeline, depth-buffer, frame-sequence

## Context

How a mesh file is made and read is [ADR-018](ADR-018-nmf-and-hardpoints.md)'s: each model is baked from a glTF source into an `.nmf` file.

The meshes do not share a scale: the Tarkan `Medium` is longer than the Tarkan `Large`. They carry no materials or textures, and the sides are told apart by their models' shapes and colors (Game §look.models). R14 rules out DirectXTK and DirectXMesh.

## Decision

1. **Reading a mesh is separate from uploading it.** `Neuron::ParseNmf` in `NeuronClient` reads an `.nmf` file's bytes into a `MeshData` on the CPU, as [ADR-018](ADR-018-nmf-and-hardpoints.md) decision 5 has it, so the reader is tested without a GPU.
2. **How big a model is is data, in `OutpostCommander/Assets/Models.json`; which way it faces is in its mesh.** Each set has a name, a linear color, and its models, or names another set whose meshes and models it borrows in its own color, as the pirates' set borrows the Tarkan set's ([ADR-073](ADR-073-pirates.md)). Each model has a `lengthMeters`. Every mesh faces +x with +y up ([ADR-018](ADR-018-nmf-and-hardpoints.md) decision 3). `Neuron::FitMesh` centers the bounds on the origin and scales the mesh, and its hardpoints, uniformly, so that its length along x is `lengthMeters`; a model with levels is fitted across its first level's widest extent (`FitMeshAcross`, [ADR-045](ADR-045-levels-and-spinning-parts.md)). +x is the front because the server's heading is counterclockwise from +x (`EntityView::headingRadians`). A ship is drawn at its model's length. A structure is drawn across its kind's footprint, its fitted mesh scaled so that its length is the footprint's diameter, and a derelict across its own radius, so a structure's `lengthMeters` does not change the size it is drawn at. Each rock is 2 m long, so its scale is its radius ([ADR-027](ADR-027-rock-crease-lines.md)). The hulls' lengths follow their footprints, about 1.25 times each footprint's diameter, and are final (owner, 2026-10-01).
3. **The set's color is the team color** (owner, 2026-10-01). `Models.json`'s `players` gives each seat its set: player 1 the Human set in blue, player 2 the Tarkan set in orange-red, whoever plays the seat. The pirates' set is violet, the asteroids gray, and the derelicts' Wreck set a lighter gray, which borrows the Human meshes and draws each derelict by its hull's model ([ADR-074](ADR-074-salvage.md)). A structure's color is its side's taken toward gray, and its faces are darker than a ship's ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 4).
4. **Every model is loaded at start, and a model that fails to load stops the game with a message naming the file.** No model is skipped silently. `Models\<set>\<model>.nmf`, or a model's first level ([ADR-045](ADR-045-levels-and-spinning-parts.md)), is read from the package's `Assets` folder. A set that borrows another's meshes reads that set's folder, so the pirates' meshes are loaded a second time, in their own color.
5. **A `Neuron::Mesh` holds its vertices and indices in default-heap buffers that it shares with other static buffers, uploaded once in a batch** ([ADR-048](ADR-048-batched-static-uploads.md)).
6. **`Neuron::MeshPipeline` draws meshes with one root signature and two pipeline states:** the faces', with `MeshVS.hlsl` and `MeshPS.hlsl`, and the lines', with `MeshLineVS.hlsl` and `MeshLinePS.hlsl` ([ADR-027](ADR-027-rock-crease-lines.md) decision 3). Parameter 0 is a root constant buffer view of the frame's constants: the view-projection matrix, the light ([ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md) decision 5), the eye's position for lines ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 2) and the occlusion's strength ([ADR-092](ADR-092-baked-occlusion.md) decision 4). Each frame in flight has one slot of them, `sizeof(FrameConstants)` rounded up to D3D12's 256-byte constant buffer alignment, in an upload buffer that stays mapped. Each object drawn, its world matrix and its color, is an instance ([ADR-053](ADR-053-instanced-meshes.md)). The shaders are shader model 5.1, compiled by `FXCompile` into `CompiledShader/` headers with warnings as errors (AGENTS.md §2).
7. **The renderer has a 32-bit float depth buffer the size of the back buffer**, cleared to 1 with a less-than test. It is recreated whenever the back buffers are. Back faces are culled. In Direct3D's default, a front face is clockwise, which is how the baked files wind ([ADR-018](ADR-018-nmf-and-hardpoints.md)).
8. **A frame is `BeginFrame`, then the caller's draws, then `EndFrame`.** `BeginFrame` clears the scene target and the depth buffer, binds them and sets a viewport over the whole of them. The interface is drawn after `BeginInterface` ([ADR-050](ADR-050-shader-resolve.md)). `EndFrame` presents. The renderer knows no game concept.

## Consequences

- **A mesh's front can be wrong.** For several Tarkan hulls the two ends are close in size. A wrong one is turned in its source and baked again ([ADR-018](ADR-018-nmf-and-hardpoints.md)).
- **`GameAppTests` checks what the owner cannot see at a glance.** Every model the game ships parses and winds clockwise about its normals. The hulls load in size order in both sets, and the same hull is the same length in both.
- **Nothing reads a material.** When materials come, the sources still hold their texture coordinates, and the format gains a version ([ADR-018](ADR-018-nmf-and-hardpoints.md)).

## What this forecloses

- Meshes scaled by anything other than `Models.json`.
- Loading a mesh from disk after startup.
