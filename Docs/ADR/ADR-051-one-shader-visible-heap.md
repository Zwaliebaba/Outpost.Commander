# ADR-051 — Every shader view lives in one shader-visible heap, bound once a frame

Status: **accepted** · 2026-10-03
Owns: shader-visible-heap, shader-view-slots, static-samplers

## Context

Several pipelines keep shader views: the particle sprites' `GlowPipeline`, the interface's `UiPipeline`, the `StarPipeline`s, the fog's ground mask ([ADR-052](ADR-052-fog-texture.md)), the resolve ([ADR-050](ADR-050-shader-resolve.md)) and the glow ([ADR-091](ADR-091-a-phosphor-glow.md)). A heap of their own for each would mean a frame switching shader-visible heaps as it draws.

Microsoft's documentation of `ID3D12GraphicsCommandList::SetDescriptorHeaps` says that "changing descriptor heaps can incur a pipeline flush on some hardware". It recommends a single shader-visible heap of each type, set once per frame. Whether the GPUs the game runs on flush has not been measured; the cost of avoiding it is a few lines.

## Decision

1. **The renderer owns one shader-visible CBV/SRV/UAV heap of 32 slots** (`Renderer::SHADER_VIEW_COUNT`). `BeginFrame` binds it right after resetting the command list, and nothing else calls `SetDescriptorHeaps`.
2. **A pipeline takes a slot with `TakeShaderView` for each view it keeps.** It writes the view at `ShaderViewCpu(slot)` and binds `ShaderViewGpu(slot)` as a descriptor table.
   - Slots are not given back; the views are made at startup.
   - A view is written again in its own slot when its resource is replaced, such as the resolve's view of a resized scene, or the atlas, which is rasterized again whenever the scale moves a font or a sprite by a whole pixel, as a resize or the interface factor ([ADR-070](ADR-070-interface-scale.md)) does. It is written again only after the GPU is done with the old one.
   - Taking a slot from a full heap throws, so running out shows at startup rather than as a wrong texture.
3. **No sampler heap.** Every pipeline's samplers are static samplers in its root signature.

## Consequences

- A frame binds its descriptor heap once, whatever it draws.
- A pipeline that draws on a command list it did not get from `BeginFrame` must bind the renderer's heap itself. None does.

## What this forecloses

- Per-frame descriptor tables written on the fly, which would need a ring of slots per frame in flight. Nothing needs them: the views are all static.
