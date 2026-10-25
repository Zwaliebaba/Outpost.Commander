# ADR-091 — What is bright glows, blurred over five levels and added in the resolve, and the interface never glows

Status: **accepted** · 2026-10-11
Owns: scene-glow, bloom-passes, scene-bloom-settings

## Context

A vector screen's phosphor glows around its lines, and that glow is the form softness takes in the vector look (owner, 2026-10-11).

[ADR-050](ADR-050-shader-resolve.md) resolves the four-sample scene into the back buffer with a shader and draws the interface over it. The glow is made from the scene before the resolve and added in it (decision 1), so the interface, drawn after, is left out of it. The scene target is 8-bit sRGB ([ADR-040](ADR-040-lines-over-dark-faces.md)), so nothing in it is brighter than 1. That is enough to find what should glow, because the faces are dark: a Human ship's faces are at most 0.12 in luminance, its lines on a lit facet about 0.57 and the selection's ring about 0.67, computed from the light and shades of `SceneShading.h` ([ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md)) and the colors of `Models.json` and the selection; and the exhaust, the shots and the blasts add light on top. [ADR-019](ADR-019-glows-and-exhaust.md)'s `GlowPipeline` draws the exhaust's own soft glows, so this one is a bloom, named so in the code, and the phosphor glow in prose.

## Decision

1. **`Neuron::Bloom` in `NeuronClient` makes the glow, and `Renderer::BeginInterface` runs it before the resolve.**
   - **Five levels** of `R11G11B10_FLOAT`, the first half the scene's size, rounded down, and each after it half the one before, never less than a pixel (`Bloom::LevelExtents`). Float, so that light added up the chain neither bands nor clips; feature level 11_0 guarantees it as a render target that blends ([ADR-006](ADR-006-renderer-shape.md)).
   - **The first level keeps the scene's bright light.** Each of its pixels averages the four samples of each of two by two scene pixels, through the scene's sRGB view, as the resolve does, keeps of each pixel the share of its light above a threshold, eased in over a knee either side of it so that nothing starts to glow at a hard edge, and averages the four (`BloomBrightPS.hlsl`).
   - **Down the chain**, each level is drawn from the one above it with thirteen bilinear taps in five overlapping boxes, the middle box weighed as much as the four corner boxes, so that a thin bright line does not flicker as it crosses the larger level's texels (`BloomDownPS.hlsl`).
   - **Back up**, each level is added into the one above it through a three by three tent of bilinear taps, weighed by the spread (`BloomUpPS.hlsl`), with an additive blend.
   - **The resolve adds the first level** to the scene's mean, in linear color, through the same tent, scaled by the intensity over the sum of the levels' weights (`Bloom::CompositeScale`), so that the intensity is the share of the bright light added back however far it spreads. Its root signature has the glow's view at t1, its constants at b0 and a bilinear clamped sampler.
   - **The interface is drawn after the resolve, so it never glows.**
   - The levels rest as pixel shader resources between frames, and each is written whole before it is read, so none is cleared. Their render target views are in a heap of the bloom's own; their shader views take five of the renderer's 32 slots ([ADR-051](ADR-051-one-shader-visible-heap.md)). They are made again with the render targets on a resize. The passes draw one triangle over their level from `BloomVS.hlsl`, as the resolve does over the screen. The pipeline knows no game concept.
2. **`Neuron::BloomDesc` says how the scene glows**: the threshold, its knee, the intensity and the spread. `Renderer::SetBloom` takes it, and until it is called the glow adds nothing.
3. **The game's glow is `GameClient.cpp`'s `SCENE_BLOOM`**, set when the client is made: a threshold of 0.35 in luminance, a knee of 0.15, an intensity of 2 and a spread of 0.6, which keeps the glow close to what is bright, as a phosphor's is (owner, 2026-10-11).

## Consequences

- **What these values do, computed by replaying the passes in numpy** on a one-pixel line in a lit Human ship's line color, 0.9 of (0.47, 0.64, 1.0), over a strip of its faces: the line brightens by a tenth, and the face a pixel from it by half, two pixels from it by 38% and four pixels from it by 15%; eight pixels away it is within 5% of the face without the glow. No channel of the line passes 1, so its hue holds. Not measured on a screen.
- **What glows.** A ship's and a structure's lines on their lit side, the selection's ring, the exhaust, the beams, the blasts and the brightest stars. The faces, the grid and the borders stay under the threshold. A rock's lines on a lit facet are about 0.39 in luminance, just over it: about 15% of their light glows, against 38% of a ship's, computed from the same light and colors and the bright pass's knee, so they stay behind the fleet ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 1).
- **The glow brightens the faces near a line.** ADR-090's test holds a model's lines over its faces as the light leaves them, before the glow. Near a lit line the face beside it is brighter by the glow, so the line reads as glowing rather than as standing higher.
- **A frame adds nine passes:** the first reads sixteen samples a pixel at half resolution, each down pass thirteen taps and each up pass nine at its level's size, and the resolve nine more taps a pixel.
- **The renderer builds the glow's three pipeline states as it is made,** inside startup's `device` stage ([ADR-049](ADR-049-startup-in-parallel.md)).
- **The tests.** `BloomTests` in `NeuronClientTests`: the levels' sizes at 1920×1080, at an odd size and at the smallest window, the composite scale for spreads of 0, 1 and 0.6, and nothing added without an intensity. They are CPU arithmetic; the passes need a GPU.
- **Bright channels can clip.** Where the glow and the scene add up past 1 in a channel, the back buffer clips it toward white. With these values it does not on a ship's line; on a blast it may, which is where a white core looks right. A tone map is not in this decision.

## What this forecloses

- Anything drawn after the resolve glowing: the interface never does.
- A glow from anything but the scene's brightness, such as a mask of what should glow, without a new decision.
