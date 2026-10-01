# ADR-018 — Meshes are baked from glTF sources into NMF files, which carry hardpoints

Status: **accepted** · 2026-10-01

## Context

ADR-011 loaded meshes from DirectX's `.cmo` files, which the owner converted from `.obj` sources with DirectXMesh's `meshconvert`. Neither format can say where something attaches to a model. The game needs that for where a shot leaves a ship (design §11's muzzle flash and tracer or beam) and for where its exhaust shows (ADR-019). Later it will need it for lights and docking. The owner also wants to edit the meshes and their hardpoints in Blender. On 2026-10-01 the owner decided that the current meshes are placeholders, to be replaced, and that exhaust is in the MVP.

Two measurements shaped the decision, both made on 2026-10-01 by scripts that parsed every file:

- **The game drew every model mirrored.** `meshconvert` copied the `.obj` coordinates and triangle order unchanged: all 29 files have the same bounds as their sources, and 22,544 of their 22,552 triangles wind right-handed about their stored normals. The game is left-handed (ADR-012's `XMMatrixLookAtLH`), so it showed each model as the left–right mirror of what a modeling tool shows. On a symmetric model nothing changes. The Tarkan `Station` is not symmetric: only 86% of its vertices have a mirror partner within 2% of its size. Nor is the Tarkan `Medium`, at 95%. With hardpoints the mirror would also swap left and right ones.
- **Four vertices had no normal.** Two are in the Tarkan `Colonizer`, one in the `Satellite` and one in the `Tiny`. The pixel shader normalizes a zero vector into NaN.

## Decision

1. **A model's source is a binary glTF, `Art/Models/<Set>/<Model>.glb`, edited with Blender's own glTF import and export.** No Blender add-on is written, and the game never reads glTF. The `.obj` sources, the converted `.cmo` files and `Tools/meshconvert.exe` are gone.
2. **`Tools/BakeMeshes.py` bakes each source into `OutpostCommander/Assets/Models/<Set>/<Model>.nmf`.** It uses the Python standard library only (R14 binds what the executable is built from, not tools). It refuses what the game cannot draw rightly, naming the file: anything but triangles, a vertex without a position and a normal or with a number that is not finite, a skin, a morph target or an animation, an extension the file requires (Draco or quantized meshes), a sparse accessor, and data outside the file. It bakes in world space, so in Blender a hardpoint may or may not be parented to its model, and transforms need not be applied. Materials, texture coordinates, cameras and lights stay in the source. Texture coordinates were carried into the sources, so materials after the MVP mean a new version of the format and a bake, not new art.
3. **The change of frame keeps the model as Blender shows it.** glTF is right-handed with +y up, +z forward and −x to the right, and Blender's exporter turns its own z-up frame into that one. A source point (x, y, z) is (z, y, x) in the game, and each triangle's corners are reversed. That maps front to +x, up to +y and right to right, and keeps triangles clockwise seen from their front, Direct3D's default. So in Blender a model faces −y with +z up. The game no longer draws the mirror image, and the Tarkan `Station` and `Medium` change visibly.
4. **A hardpoint is an empty named `hp_<tag>`.** Blender's own `.001` suffixes may follow, and they set the order. The tag is lowercase letters and digits, at most 31 characters. Its frame is the empty's: forward is its +z, the axis Blender's Single Arrow display draws, up is its +y, and its scale, which must be uniform, is its size. The engine stores the tag and never reads it (R9). `Outpost::HardpointKindOf` in `GameApp` gives the tags their meaning, and a model with a tag the game does not know fails to load, naming the file. The game knows `gun` and `exhaust` (design §7, ADR-019).
5. **The NMF layout, version 1, little-endian, is read strictly by `Neuron::ParseNmf` in `NeuronClient`:**

   ```
   char magic[4] = "NMF\0"; u32 version = 1; u32 vertexCount; u32 indexCount; u32 hardpointCount; u32 flags = 0
   vertexCount    x { f32 position[3]; f32 normal[3]; }      24 bytes, Neuron::MeshVertex
   indexCount     x u32, a multiple of 3, each below vertexCount
   hardpointCount x { u8 tagLength; char tag[tagLength]; f32 position[3]; f32 forward[3]; f32 up[3]; f32 size; }
   ```

   It has no sections and no tolerance for other versions. The assets and the executable ship in one MSIX package, so a file never meets a game older or newer than itself, and a change of layout is a new version and a bake of every source. The reader rejects what `ParseCmo` rejected: a file cut short, bytes left over, no triangles, an index past the vertices. It also rejects a number that is not finite, a malformed tag, a hardpoint whose directions are not unit vectors at right angles within 0.001, and a size that is not positive. The message names the file and the byte offset. 32-bit indices match `MeshData` and the index buffer. The largest model has 3,972 vertices, so 16-bit ones would fit, but they would save 135 KB across all 29. The bounds are measured at load.
6. **A model's front is in its source, and its length stays data.** `Neuron::FitMesh` centers a mesh's bounds on the origin and scales it so that its length along x is `lengthMeters` in `Models.json`. Its hardpoints move and scale with it. `forwardAxis` is gone from `Models.json`, and with it `OrientMesh`'s turn.
7. **The `.nmf` files are committed, and CI proves them current.** A Linux job runs `python Tools/BakeMeshes.py --check`, which fails when any `.nmf` is not byte for byte what its source bakes to, or has no source. It also runs `--self-test`, which bakes small sources it builds and checks that every refusal fires. CPython's floats are IEEE doubles with no fused operations, so a bake gives the same bytes on every machine. Committing the output of a tool sits close to AGENTS.md §2's rule against committing what a build step generates. The bake is not a build step, as `meshconvert` was not. Making it one would make the C++ build depend on Python, and the check gives the guarantee the rule is after: nothing committed can drift from its source.
8. **The first sources were written from the `.cmo` files, not the `.obj` ones**, so that the geometry is what the game drew, by a one-off script that is not kept. It turned each model to face +z using `Models.json`'s `forwardAxis`, which is a rotation and never a mirror. It kept texture coordinates. It gave each of the four vertices without a normal the mean of its triangles' normals. It placed placeholder hardpoints by rule:
   - a `gun` on top of each hull's front half, and on the Large hull a second on its back half;
   - a `gun` on top of the `Station` and the `Mine`, which carry the Defence gun (design §6);
   - an `exhaust` at the stern of each hull and of the `Colonizer`, the Constructor. It is one nozzle, or two where the stern is two separate engines, which is the Human `Small`. Its radius is half the stern's span, clamped to 3–8% of the length.

   Rendered views of the twelve models were checked by eye. They stay placeholders until the meshes are replaced.

## Consequences

- **The bake matches what the game drew, mirrored on purpose.** A script read every baked file and its `.cmo` on 2026-10-01. Positions are bit for bit the `.cmo` positions turned to face +x and mirrored across the game's z. Every triangle's corners are reversed exactly, and normals agree to within 1.2e-7, apart from the four that were repaired. `GameAppTests` checks that at least 99% of each shipped model's triangles wind clockwise about their normals. The lowest is the Tarkan `Colonizer`, at 99.39%.
- **Editing is Blender's import, edit and export, then a bake.** A source exported without +Y Up turns the model on its side, and the bake cannot see that. A wrong front shows in the owner's run, and is fixed in the source rather than in data.
- **`GameAppTests` checks that every model has the hardpoints its role needs** in both players' sets: a gun and an exhaust on each hull, an exhaust on the Constructor, and a gun on the two armed structures.
- **The server never reads a mesh.** Hardpoints are presentation, and `GameLogic` cannot include `NeuronClient` (ADR-002). The reader needs no GPU, so it can move to `NeuronCore` on the day the server needs one, for docking say.

## What this forecloses

- Editing a baked `.nmf`. Sources flow one way, and there is no NMF importer for Blender.
- Skinned, animated or morphing meshes, and hardpoints on moving parts, until the format has a version for them.
- A mesh drawn in the mirror image of its source.
