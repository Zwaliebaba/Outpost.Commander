# ADR-050 — A shader resolves the scene through the back buffer's sRGB view, and the interface is drawn after it at one sample

Status: **accepted** · 2026-10-11
Owns: scene-resolve, interface-pass

## Context

The scene is drawn into a 4-sample `R8G8B8A8_UNORM_SRGB` target ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 3), and its samples have to reach the back buffer averaged in linear color.

A flip-model back buffer cannot be sRGB, and `ResolveSubresource` cannot change the format. That is true of the resource, but not of a view of it: the `R8G8B8A8_UNORM` back buffer takes an `R8G8B8A8_UNORM_SRGB` render target view (ADR-006 decision 4). So a pixel shader can read the scene's samples through an sRGB shader resource view, which decodes them to linear color, average them, and write the back buffer through its sRGB render target view, with no texture between the two.

The interface's text is laid on whole pixels (ADR-015 decision 3), so multisampling gives it nothing.

## Decision

1. **`Renderer::BeginInterface` resolves the scene with a shader.**
   - Its pipeline is one triangle over the screen, made from the vertex index (`Shader/ResolveVS.hlsl`).
   - Its pixel shader (`Shader/ResolvePS.hlsl`) loads the pixel's samples from the scene through its `TEXTURE2DMS` view in the renderer's shader-visible heap (ADR-051), and returns their mean in linear color, with the scene's glow added ([ADR-091](ADR-091-a-phosphor-glow.md) decision 1). The shader's `SAMPLE_COUNT` matches `Renderer::SAMPLE_COUNT`, so its loop unrolls.
   - It writes the back buffer through an sRGB render target view; each back buffer has one, after the scene target's in the renderer's RTV heap.
   - No texture stands between the scene target and the back buffer.
2. **The interface is drawn after it, straight into the back buffer, at one sample and with no depth buffer.** `UiPipeline`'s pipeline state is built for `RENDER_TARGET_FORMAT`, one sample and `DXGI_FORMAT_UNKNOWN` depth.
   - `GameClient::Render` draws the world, and `GameClient::RenderInterface` draws the HUD, the windows or the menu.
   - `WinMain` calls `BeginInterface` between the two. `EndFrame` calls it itself when no one has.
3. **The scene target is a pixel shader resource from the resolve until the frame ends,** and goes back to a render target before the next frame clears it.

## Consequences

- The resolve runs in a pixel shader, not in the hardware's resolve path. Some GPUs resolve compressed multisampled surfaces faster than a shader reads them, so the resolve's GPU time is one of the frame's budgets (`Design/Budgets.md`).
- **The interface is drawn at one sample.** Text and sprites are laid on whole pixels, so one sample loses them nothing. A panel edge that falls between pixels is either drawn or not. The HUD is laid out in reference units scaled to the screen (ADR-006 decision 5), so some edges fall between pixels.
- The interface does not pay for four samples a pixel in the scene target.

## What this forecloses

- Drawing anything multisampled after `BeginInterface`.
- It leaves open post-processing between the resolve and the interface, which would read the back buffer or a target of its own.
