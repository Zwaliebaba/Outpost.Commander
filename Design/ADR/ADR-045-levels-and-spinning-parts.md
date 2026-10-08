# ADR-045 — A model may grow through levels, and its parts may spin

Status: **accepted** · 2026-10-03

## Context

On 2026-10-03 the owner replaced the `CommandStation`, `Shipyard` and `ResearchLab` sources of both sets with five levels each, `<Model>_L1.glb` to `<Model>_L5.glb`, to be used as the structures grow. Phase 3 grows them ([ADR-064](ADR-064-structure-upgrades.md)).

The owner's `ResearchLab` sources also have a moving part. Each is a node named `*_pivot`, and a glTF animation turns its rotation. Blender wrote each spin as five linear keys a quarter turn apart, ending a whole turn from where it starts. A script read every source on 2026-10-03:

- **Human**: the `radar_pivot` turns once in 8 s at every level. Level 5 adds a `relay_pivot_L` and a `relay_pivot_R`, which turn once in 5 s.
- **Tarkan**: the `tilted_ring_pivot` turns once in 12 s at every level. Levels 3 to 5 add a `counter_ring_pivot`, which turns the other way once in 9 s.
- **No other source** is animated, and no animated node holds a hardpoint.

The baker refused every animation, so none of these sources could be baked.

## Decision

1. **A model that grows has one source a level**, `Art/Models/<Set>/<Model>_L<n>.glb`. `Tools/BakeMeshes.py` bakes each into an `.nmf` of the same name, as it bakes any other source. In `Models.json` a model has an optional `"levels"`, from 1 to 9. A model with levels is loaded from `Models\<Set>\<Model>_L<n>.nmf`, every level of it, and a model without them from `Models\<Set>\<Model>.nmf`. Every level's `.nmf` is in the package. The simulation and the snapshot carry a structure's level, and the client draws that level's mesh, each later level fitted within level 1's ground ([ADR-064](ADR-064-structure-upgrades.md) decisions 6 and 8).
2. **A spinning part is a node whose rotation an animation turns in a steady spin.** The baker reads the spin from the animation, because that is what Blender plays. It does not read extras such as `spin_period_s`. A steady spin means:
   - linear keys, each less than half a turn from the last;
   - every key about one axis through the node's origin, in its parent's frame;
   - one rate, to 0.001 radians;
   - and an end a whole number of turns from the first key.

   The part is the node and everything under it, baked as its first key stands. The baker refuses:
   - any other animation, such as a translation, a step or a spline;
   - a part under a parent that is not scaled uniformly;
   - a part inside another part;
   - a hardpoint on a part;
   - a part with no triangles;
   - a model whose every triangle spins.
3. **The NMF layout is version 2, little-endian:**

   ```
   char magic[4] = "NMF\0"; u32 version = 2; u32 vertexCount; u32 indexCount; u32 hardpointCount; u32 partCount; u32 flags = 0
   vertexCount    x { f32 position[3]; f32 normal[3]; }      24 bytes, Neuron::MeshVertex
   indexCount     x u32, a multiple of 3, each below vertexCount
   hardpointCount x { u8 tagLength; char tag[tagLength]; f32 position[3]; f32 forward[3]; f32 up[3]; f32 size; }
   partCount      x { u32 firstIndex; u32 indexCount; f32 pivot[3]; f32 axis[3]; f32 periodSeconds; }
   ```

   The triangles that stand still come first. The parts' runs of indices follow, one after another, to the end, each sorted by its node's name. A part turns once each period about its pivot, as `XMMatrixRotationAxis(axis, angle)` turns a point as the angle grows. The change of frame mirrors the source, so the baker reverses the source's axis. `Neuron::ParseNmf` reads only version 2, as strictly as ADR-018 decision 5 says, and every `.nmf` is baked again. It also refuses:
   - a part's axis that is not a unit vector;
   - a period that is not positive;
   - runs that leave no triangle standing still, are not whole triangles, overlap or leave a gap, or do not reach the last index.
4. **The client draws a model in pieces.** `Neuron::MeshPiece` cuts the triangles that stand still, and each part's, into meshes of their own. Each piece gets its own creases (ADR-027). `GameClient` draws a part through `Neuron::PartWorld`, which turns it about its pivot by the view's tick. It turns on the view's clock, as the combat effects do, so it keeps the match's time. A placement ghost spins too. `FitMesh` moves the pivots with the mesh.
5. **What stays whole:** the explosion breaks the whole mesh, with each part as it stands at rest (ADR-026). Bounds, and so a model's fitted length, include the parts at rest. Hardpoints never move.

## Consequences

- Each of the 45 sources bakes, and `--check` proves the 45 `.nmf` files current. The 15 models with no levels and no parts change only in their header: 4 bytes and the version.
- Every Research Lab in a match spins in step with every other, on the one clock. A phase for each structure would need the entity in `DrawModel`, and no one has asked for it.
- `NeuronClientTests` checks the version 2 reader and its refusals, `MeshPiece`, and the sense and period of `PartWorld`. `GameAppTests` checks the level 1 file name, and that both players' Research Labs have a part. `BakeMeshes.py --self-test` checks that a part's corners turn in the game where glTF's animation turns them, a quarter turn in. It also checks that every new refusal fires.

## What this forecloses

- A part that moves in any way but a steady spin about a fixed axis, such as a hinge, a sweep or a slide, until the format has a version for it.
- A hardpoint on a moving part, as before (ADR-018).
- A level chosen by the data. The level is the game's to set: the simulation's, which the snapshot carries ([ADR-064](ADR-064-structure-upgrades.md)).
