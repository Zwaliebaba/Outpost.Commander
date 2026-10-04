# ADR-053 — Every mesh drawn is an instance, and a batch draws each mesh's copies at once

Status: **accepted** · 2026-10-03

## Context

Each drawn object used to be one draw with 20 root constants, its world matrix and its color, later 24 with a line's lift (ADR-040). Every object was one `DrawIndexedInstanced` of one instance, with its own root constants, vertex buffer and index buffer set before it.

A frame draws the same few meshes many times. Under `--stress` that is 200 ships of three hulls in two sets, 40 structures and every rock of the fields, each as faces and as crease lines, so roughly a thousand draws of a few dozen meshes. These figures are counted from the code and the data, not measured. Each draw costs command-list recording on the CPU and front-end work on the GPU, whatever its size.

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
   - `GameClient` gathers every model's faces into one batch a frame, and their creases into another, as it already did for the lines.
3. **Single draws stay as they were for their callers.** `Draw`, `DrawTriangles`, `DrawLineList` and `DrawLines` of one mesh each write one instance and draw it. Rings, discs, health bars, beams and the ghost keep their order.
4. **A frame that runs out of instances draws nothing more.** That is the way `DrawTriangles` already behaves when it runs out of vertices.

## Consequences

- A frame draws each distinct model mesh once for its faces and once for its lines, however many ships share it. The per-object cost on the CPU is one 96-byte copy into mapped memory.
- **The faces are no longer drawn in entity order**, but grouped by mesh. They are opaque and depth-tested, so only faces at exactly the same depth could show the difference. Lines are grouped the same way, and where two lines cover the same pixel, the one drawn last now depends on its mesh's group.
- A draw now sets one root constant where it set 24.
- `--measure --stress` (`frame_cpu_ns`, `frame_gpu_ns`) is what shows the gain. This ADR quotes none.
- **Forecloses** a draw that needs constants of its own beyond `Instance`; it would be a field in `Instance`. **Leaves open** culling instances on the GPU, since they are already in one buffer.
