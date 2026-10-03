# ADR-050 — A shader resolves the scene through the back buffer's sRGB view, and the interface is drawn after it at one sample

Status: **accepted** · 2026-10-03 · supersedes [ADR-040](ADR-040-lines-over-dark-faces.md) decision 3's resolve, copy and HUD in the scene target, and [ADR-006](ADR-006-renderer-shape.md) decision 4 in part

## Context

ADR-040 draws the scene into a 4-sample `R8G8B8A8_UNORM_SRGB` target. `EndFrame` then did two things:

- it resolved that target into a single-sample sRGB texture, so the samples were averaged in linear color;
- it copied that texture into the `R8G8B8A8_UNORM` back buffer.

ADR-040 explains why both steps were needed: a flip-model back buffer cannot be sRGB, and `ResolveSubresource` cannot change the format.

That is true of the resource, but not of a view of it. ADR-006 decision 4 already relies on that: the back buffer takes an `R8G8B8A8_UNORM_SRGB` render target view. So a pixel shader can read the scene's samples through an sRGB shader resource view, which decodes them to linear color, average them, and write the back buffer through its sRGB render target view. That gives the same mean, in linear color, with no texture in between.

The interface was drawn into the 4-sample target with the scene, so every interface pixel went through the resolve as well. Its text is laid on whole pixels (ADR-030), so the samples gave it nothing.

## Decision

1. **`Renderer::BeginInterface` resolves the scene with a shader.**
   - Its pipeline is one triangle over the screen, made from the vertex index (`Shader/ResolveVS.hlsl`).
   - Its pixel shader (`Shader/ResolvePS.hlsl`) loads the pixel's samples from the scene through its `TEXTURE2DMS` view in the renderer's shader-visible heap (ADR-051), and returns their mean. The shader's `SAMPLE_COUNT` matches `Renderer::SAMPLE_COUNT`, so its loop unrolls.
   - It writes the back buffer through an sRGB render target view; each back buffer has one, after the scene target's in the renderer's RTV heap.
   - The intermediate texture, the `ResolveSubresource` and the `CopyResource` are gone.
2. **The interface is drawn after it, straight into the back buffer, at one sample and with no depth buffer.** `UiPipeline`'s pipeline state is built for `RENDER_TARGET_FORMAT`, one sample and `DXGI_FORMAT_UNKNOWN` depth.
   - `GameClient::Render` draws the world, and `GameClient::RenderInterface` draws the HUD, the windows or the menu.
   - `WinMain` calls `BeginInterface` between the two. `EndFrame` calls it itself when no one has.
3. **The scene target is a pixel shader resource from the resolve until the frame ends,** and goes back to a render target before the next frame clears it.

## Consequences

- A frame no longer reads and writes a full-screen intermediate texture: at 1920×1080 that is about 8 MB written by the resolve and read again by the copy, every frame. The resolve now runs in the pixel shader instead of in the hardware's resolve path. Some GPUs resolve compressed multisampled surfaces faster than a shader reads them, so the GPU time is measured with `--measure --stress` (`frame_gpu_ns`) before this is taken as settled. If the shader is slower, the old path can come back without the interface moving.
- **The interface loses multisampling.** Text and sprites are laid on whole pixels and do not change. A panel edge that falls between pixels is now either drawn or not, where four samples blended it. The HUD is laid out in reference units scaled to the screen (ADR-006), so some edges can fall between pixels. This is a visible change to check on screen.
- The interface no longer pays for four samples a pixel in the scene target.
- **Forecloses** drawing anything multisampled after `BeginInterface`. **Leaves open** post-processing between the resolve and the interface, which would read the back buffer or a target of its own.
