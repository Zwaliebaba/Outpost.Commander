# ADR-051 — Every shader view lives in one shader-visible heap, bound once a frame

Status: **accepted** · 2026-10-03

## Context

Three pipelines kept a shader-visible descriptor heap of their own, each with one view:

- the particle sprites' `GlowPipeline`;
- the interface's `UiPipeline`;
- a sprite `StarPipeline`, which the game does not use today.

Each called `SetDescriptorHeaps` with its own heap when it drew, so a frame switched shader-visible heaps twice.

Microsoft's documentation of `ID3D12GraphicsCommandList::SetDescriptorHeaps` says that "changing descriptor heaps can incur a pipeline flush on some hardware". It recommends a single shader-visible heap of each type, set once per frame. Whether the GPUs the game runs on flush has not been measured; the cost of avoiding it is a few lines.

ADR-050's resolve and ADR-052's fog each add another view, which would have meant two more heaps.

## Decision

1. **The renderer owns one shader-visible CBV/SRV/UAV heap of `Renderer::SHADER_VIEW_COUNT` slots**, 32 today. `BeginFrame` binds it right after resetting the command list, and nothing else calls `SetDescriptorHeaps`.
2. **A pipeline takes a slot with `TakeShaderView` for each view it keeps.** It writes the view at `ShaderViewCpu(slot)` and binds `ShaderViewGpu(slot)` as a descriptor table.
   - Slots are not given back; the views are made at startup.
   - A view is rewritten in place when its resource is replaced, such as the atlas on a resize or the resolve's view of a resized scene. It is rewritten only after the GPU is done with the old one.
   - Taking a slot from a full heap throws, so running out shows at startup rather than as a wrong texture.
3. **No sampler heap.** Every pipeline's samplers are static samplers in its root signature.

## Consequences

- A frame binds its descriptor heap once, whatever it draws.
- A pipeline that draws on a command list it did not get from `BeginFrame` must bind the renderer's heap itself. None does today.
- **Forecloses** per-frame descriptor tables written on the fly, which would need a ring of slots per frame in flight. Nothing needs them yet; the views are all static.
