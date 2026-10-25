# ADR-053 — Every mesh drawn is an instance, and a batch draws each mesh's copies at once

Status: **accepted** · 2026-10-03
Owns: mesh-instancing, mesh-draw-batches

## Context

A frame draws a few dozen meshes many times. Every ship of one hull and one set shares its hull's mesh, every structure of a kind shares its model, and every rock of a field shares one of a few, each drawn as faces and as crease lines. Under `--stress` that is 200 ships of three hulls in two sets and 40 structures (`StressLoad.h`), beside every rock of the fields.

Each draw costs command-list recording on the CPU and front-end work on the GPU, whatever its size.

## Decision

1. **Every object drawn is an instance.**
   - `MeshPipeline` keeps one slot per frame in flight of `MAX_FRAME_INSTANCES` instances (16,384) in an upload buffer, mapped for its lifetime.
   - `MeshPipeline::Instance` is the world matrix, the color and the lift: 96 bytes, which is `struct Object` in the shaders.
   - The root signature has three parameters: the frame's constants, one root constant saying where a draw's instances start in the frame's slot, and the frame's slot as a root shader resource view.
   - The vertex shaders read `objects[firstObject + SV_InstanceID]`. The world matrix is held as four `float4` rows, so no matrix packing rule is involved.
   - The color reaches the pixel shader as a non-interpolated vertex output.
2. **A batch draws each mesh's copies together.** `DrawMeshes` takes a span of `MeshDraw`s, each a mesh and an instance, for faces. `DrawLines` takes the same for lines.
   - Either one puts each mesh's instances one after another in the slot, and draws them with one `DrawIndexedInstanced`.
   - The meshes are drawn in the order they first appear in the batch.
   - `GameClient` gathers every model's faces into one `DrawMeshes` batch a frame, and their creases into one `DrawLines` batch. The footprint and selection rings, the territory's lines and rings ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 3) and the memories are each a `DrawLines` batch too.
3. **A single draw writes one instance and draws it.** `Draw`, `DrawTriangles`, `DrawLineList` and `DrawLines` of one mesh each do so, in the order their callers make them. The grid, the effects' discs and rings, a rig's draped ring, beams, the explosions' shards and the placement ghost draw this way.
4. **A frame that runs out of instances draws nothing more.** That is the way `DrawTriangles` behaves when it runs out of vertices.

## Consequences

- A frame draws each distinct model mesh once for its faces and once for its lines, however many ships share it. The per-object cost on the CPU is one 96-byte copy into mapped memory, and a draw sets one root constant.
- **The faces are drawn grouped by mesh, not in entity order.** They are opaque and depth-tested, so only faces at exactly the same depth could show the difference. Lines are grouped the same way, and where two lines cover the same pixel, the one drawn last depends on its mesh's group.

## What this forecloses

- A draw that needs constants of its own beyond `Instance`; it would be a field in `Instance`.
- It leaves open culling instances on the GPU, since they are in one buffer.
