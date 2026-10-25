# ADR-091 — What is bright glows, blurred over five levels and added in the resolve, and the interface never glows

Status: **accepted** · 2026-10-11

## Context

The review of 2026-10-11 ([the look plan](../../GameDesign/ImplementationPlan-Look.md), finding 4) found that the softness of the game the owner likes has a form in the vector look: the glow of a vector screen's phosphor around its lines. The owner decided gate A2 as proposed on 2026-10-11, and this is task LV1.2.

[ADR-050](ADR-050-shader-resolve.md) resolves the four-sample scene into the back buffer with a shader and draws the interface over it, and leaves open work between the two. The scene target is 8-bit sRGB ([ADR-040](ADR-040-lines-over-dark-faces.md)), so nothing in it is brighter than 1. That is enough to find what should glow, because the faces are dark: a Human ship's faces are at most 0.12 in luminance, its lines on a lit facet about 0.57, the selection's ring about 0.67, and the exhaust, the shots and the blasts add light on top. [ADR-019](ADR-019-glows-and-exhaust.md)'s `GlowPipeline` draws the exhaust's own soft glows, so this one is a bloom, named so in the code, and the phosphor glow in prose.

## Decision

1. **`Neuron::Bloom` in `NeuronClient` makes the glow, and `Renderer::BeginInterface` runs it before the resolve.**
   - **Five levels** of `R11G11B10_FLOAT`, the first half the scene's size, rounded down, and each after it half the one before, never less than a pixel (`Bloom::LevelExtents`). Float, so that light added up the chain neither bands nor clips; feature level 11_0 guarantees it as a render target that blends ([ADR-006](ADR-006-renderer-shape.md)).
   - **The first level keeps the scene's bright light.** Each of its pixels averages the four samples of each of two by two scene pixels, through the scene's sRGB view, as the resolve does, keeps of each pixel the share of its light above a threshold, eased in over a knee either side of it so that nothing starts to glow at a hard edge, and averages the four (`BloomBrightPS.hlsl`).
   - **Down the chain**, each level is drawn from the one above it with thirteen bilinear taps in five overlapping boxes, the middle box weighed as much as the four corner boxes, so that a thin bright line does not flicker as it crosses the larger level's texels (`BloomDownPS.hlsl`).
   - **Back up**, each level is added into the one above it through a three by three tent of bilinear taps, weighed by the spread (`BloomUpPS.hlsl`), with an additive blend.
   - **The resolve adds the first level** to the scene's mean, in linear color, through the same tent, scaled by the intensity over the sum of the levels' weights (`Bloom::CompositeScale`), so that the intensity is the share of the bright light added back however far it spreads. Its root signature gains the glow's view at t1, its constants at b0 and a bilinear clamped sampler.
   - **The interface is drawn after the resolve, so it never glows.**
   - The levels rest as pixel shader resources between frames, and each is written whole before it is read, so none is cleared. Their render target views are in a heap of the bloom's own; their shader views take five of the renderer's 32 slots ([ADR-051](ADR-051-one-shader-visible-heap.md)). They are made again with the render targets on a resize. The passes draw one triangle over their level from `BloomVS.hlsl`, as the resolve does over the screen. The pipeline knows no game concept.
2. **`Neuron::BloomDesc` says how the scene glows**: the threshold, its knee, the intensity and the spread. `Renderer::SetBloom` takes it, and until it is called the glow adds nothing.
3. **The game's glow is `GameClient.cpp`'s `SCENE_BLOOM`**, set when the client is made: a threshold of 0.35 in luminance, a knee of 0.15, an intensity of 2 and a spread of 0.6, which keeps the glow close to what is bright, as a phosphor's is. They are first values, to be judged in the owner's run.

## Consequences

- **What the first values do, computed by replaying the passes in numpy** on a one-pixel line in a lit Human ship's line color, 0.9 of (0.47, 0.64, 1.0), over a strip of its faces: the line brightens by a tenth, and the face a pixel from it by half, two pixels from it by 38% and four pixels from it by 15%; eight pixels away it is within 5% of what it was. No channel of the line passes 1, so its hue holds. Not measured on a screen.
- **What glows.** A ship's and a structure's lines on their lit side, the selection's ring, the exhaust, the beams, the blasts and the brightest stars. The faces, the grid and the borders stay under the threshold. A rock's lines on a lit facet are about 0.39 in luminance, just over it: about 15% of their light glows, against 38% of a ship's, so they stay behind the fleet ([ADR-028](ADR-028-vector-grid-and-crosses.md)).
- **The glow brightens the faces near a line.** ADR-090's test holds a model's lines over its faces as the light leaves them, before the glow. Near a lit line the face beside it is brighter by the glow, so the line reads as glowing rather than as standing higher.
- **Memory:** about 2.8 MB at 1920×1080 and 11 MB at 3840×2160, computed from the levels' sizes at four bytes a texel.
- **GPU time, measured by the owner on 2026-10-11** with a Release build of this change, `--measure --stress` at 1920×1080 on a 165 Hz display, 7,415 frames summarized by `Tools/FrameTimes.py`: a whole frame's GPU work, the glow included, has a mean of 0.67 ms, a median of 0.66 ms, a 99th percentile of 1.19 ms and a worst of 1.76 ms. Q4 is met.
  - **The glow and ADR-090's light together cost 0.30 ms of GPU work a frame on average**, against the same run of `main`, which the owner measured the same way the same day over 10,281 frames: a mean of 0.37 ms, a median of 0.32 ms, a 99th percentile of 0.81 ms and a worst of 1.17 ms. The median rises by 0.34 ms and the 99th percentile by 0.38 ms. That is 5% of a 165 Hz frame's 6.06 ms. The light is a few instructions a pixel and the glow nine passes, but the two were not measured apart.
  - The two runs also differ in CPU frame time and in server ticks, but by as much in parts this change does not touch: the server's ticks, which draw nothing, took a mean of 0.77 ms on this change's run against 0.62 ms on `main`'s, and its startup stage 311 ms against 247 ms. One run each does not separate those differences from the machine's state.
  - A frame adds nine passes: the first reads sixteen samples a pixel at half resolution, each down pass thirteen taps and each up pass nine at its level's size, and the resolve nine more taps a pixel.
  - The renderer builds the glow's three pipeline states as it is made, inside startup's `device` stage ([ADR-049](ADR-049-startup-in-parallel.md)), which this change's run finished at 2,220 ms from the start of `wWinMain` and `main`'s at 1,906 ms. The server's stage, which this change does not touch, was slower by a similar share, so the pair does not say how much of the difference the pipeline states are.
- **The tests.** `BloomTests` in `NeuronClientTests`: the levels' sizes at 1920×1080, at an odd size and at the smallest window, the composite scale for spreads of 0, 1 and 0.6, and nothing added without an intensity. They are CPU arithmetic; the passes need a GPU.
- **Built and run.** CI built it Debug|x64 and ran the tests on Windows, and the owner built it in Release and ran it on 2026-10-11. Before CI, the four new shaders and the resolve passed glslang's HLSL front end in the Linux container, and `Bloom.cpp` parsed with clang 18 and gcc 13 against Microsoft's open DirectX-Headers. glslang let through a cbuffer named `Pass`, which `fxc` refuses, since it reads `pass` as an effect's keyword whatever its case; the cbuffer is `PassConstants`. Whether the first values want moving is the owner's call.
- **Bright channels can clip.** Where the glow and the scene add up past 1 in a channel, the back buffer clips it toward white. With the first values it does not on a ship's line; on a blast it may, which is where a white core looks right. A tone map is not in this decision.

## What this forecloses

- Anything drawn after the resolve glowing: the interface never does.
- A glow from anything but the scene's brightness, such as a mask of what should glow, without a new decision.
