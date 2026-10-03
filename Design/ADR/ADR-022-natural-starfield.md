# ADR-022 — The sky is an even field of stars at infinity, drawn as points

Status: **accepted** · 2026-10-01

## Context

Until 2026-10-01 the battlefield was drawn over a dark blue clear color, with a grid on the ground. That day the owner asked for that color to go and for a space backdrop in its place. The first sky was about 193,000 Gaussian points: an even field and a Milky Way band, with a bulge, star clouds and dust lanes made of nothing but stars. The owner ran it and did not like it, and asked the same day for three changes: remove the Milky Way, make the sky look more natural, and see whether two sprites added to `OutpostCommander/Assets/Textures/` make sense for it. The sprites are `glow.dds`, a soft round falloff, and `starburst.dds`, a bright core with diffraction spikes. Both are 128 by 128, uncompressed 8-bit BGRA in the legacy DDS header, with one mip level, white in their color and with their shape in their alpha. The owner added them.

Three facts about the view set the problem. The RTS camera looks down at 40 to 70 degrees with a 45-degree vertical field of view (ADR-012), so it only ever sees the lower half of the sky and never the horizon. Q and E turn it freely. Panning moves it but does not change which way it looks.

## Decision

1. **The sky is one field of 50,000 stars spread evenly over the whole sphere, over a black clear color.** It has no galaxy: no band, bulge, clouds or dust. `Outpost::BuildStarfield` in `GameApp` makes it from a fixed seed, so it is the same every run. It uses its own SplitMix64 rather than `<random>`, whose distributions differ between standard libraries.
2. **Brightness is the natural distribution.** The count of stars brighter than a light falls as that light to the power −3/2, which is how stars spread evenly through space look. So most stars are near the faintest, few are bright and very few are very bright. This is what makes the sky look natural rather than evenly speckled. A point's light is its peak times its Gaussian's area: a brighter point is drawn wider, up to a spread of 1.6 pixels, rather than brighter than a peak of 0.75. No point's spread is below 0.7 pixels, which keeps a slowly turning sky from flickering. Colors come from seven blackbody tints between 3,000 K and 10,000 K, each mixed with white so that 65% of the tint remains. These looks are constants in `Starfield.cpp`, like task 3.5's in `CombatEffects.cpp`.
3. **The brightest stars are drawn as crosses, and the rest stay Gaussian points** ([ADR-028](ADR-028-vector-grid-and-crosses.md)).
4. **`glow.dds` is not used.** Shrunk to a point star's 4–6 pixels, it is the same soft spot as the Gaussian in `StarPS.hlsl`. The Gaussian is exact at every size and needs no texture, sampler or descriptor heap. The starburst has its own soft core, so it does not need a glow under it either.
5. **`Neuron::StarPipeline` in `NeuronClient` draws each star as a square facing the screen, at infinity.** A star is a direction, a size and a linear color with its brightness included. The vertex shader, `StarVS.hlsl`, builds each star's quad from `SV_VertexID` as a strip of four vertices. It transforms the direction with w = 0, so the camera's position drops out and only turning the camera moves the sky. It then offsets the corners by a number of pixels, scaled by w so that the size survives the divide, and puts z at w/2, so that only a star behind the camera is clipped. A star's size is `radiusPixels`, half its square's side, in pixels on ADR-006's 1920×1080 reference frame, scaled by the same uniform factor as the HUD, so the sky looks the same on every monitor. The pipeline's `Shape` picks its pixel shader ([ADR-028](ADR-028-vector-grid-and-crosses.md)). For a Gaussian, `StarPS.hlsl` subtracts the Gaussian's value at the rim, three standard deviations out, so a quad's edge never shows. With a `TextureData`, `StarSpritePS.hlsl` returns the star's color times the sprite's alpha, upright on the screen whichever way the camera turns, as a camera's diffraction spikes are. A sprite pipeline adds a descriptor table for the texture at t0, with its view in the renderer's shader-visible heap ([ADR-051](ADR-051-one-shader-visible-heap.md)), and a static trilinear sampler with clamped addressing. The blend adds the result to the frame. The sky is drawn first, straight after the clear, with no depth test and no depth write, so everything drawn after it covers it. Each pipeline's stars are uploaded once and drawn in one instanced draw, and `GameClient` draws the points, then the bursts: two instanced draws. The shaders are shader model 5.1, like the others. The pipeline knows no game concept.

   Stars are points rather than a texture. In a cube map or panorama each star is a texel, which blurs when the view magnifies it and shimmers when the view shrinks it. A cube map sharp enough for 4K would also need six faces of about 2,048 texels square, or 100 MB at four bytes a texel. A point stays sharp at any resolution.
6. **`Neuron::ParseDds` reads a DDS file without a library (R14).** It reads only what the game uses: an uncompressed 2D texture of 8-bit BGRA or RGBA texels, in the legacy header or the DX10 one, with any mip levels the file has. It throws, naming the file, for anything else: compressed formats, cube maps, volumes and arrays, a short file, or bytes left over.
7. **`Neuron::BuildMipLevels` makes the sprite's mip chain on the CPU at start**, from 128 by 128 down to 1 by 1. Each texel is the rounded average of the 2 by 2 above it. `Renderer::CreateStaticTexture` gains an overload that uploads every level. Without mips, a 128-pixel sprite drawn 18 to 72 pixels across would sample its thin spikes at a texel here and there, and they would shimmer as the zoom changes the camera's pitch.
8. **The grid stays, barely visible**: how it is drawn and its color are [ADR-028](ADR-028-vector-grid-and-crosses.md)'s. The sky does not move when the view pans, so the grid is what shows the ground moving, and where the map ends.
9. **`starburst.dds` is packaged as deployment content**, a `None` item with `DeploymentContent`, like every file the game reads at runtime. Nothing draws it now, but it stays in the package ([ADR-028](ADR-028-vector-grid-and-crosses.md)). Of the other four textures, `Particle.dds` is deployed too ([ADR-026](ADR-026-explosions-and-particles.md)); the rest stay as the owner added them, as `Image` items, until something reads them.

## Consequences

- **The sky costs two draw calls a frame.** Its 50,000 stars are 200,000 vertex-shader invocations, and its pixels are a few thousand small points and a handful of crosses. None of this is measured on a GPU.
- **Building the sky is 50,000 random draws and a sort.** There is no noise.
- **The sky is not tied to the camera's yaw.** With no band, every direction of the sky looks alike, and Q and E turn it without anything going out of view.
- **`GameAppTests` checks it without a GPU.**
  - The sky is the same every time.
  - It has 50,000 stars, and the brightest few are bursts, each larger than any point ([ADR-028](ADR-028-vector-grid-and-crosses.md)).
  - Every point is a unit direction within its size and brightness limits.
  - Each 30-degree cap round the six axes holds its share of the sky within 10%.
  - `ParseDds` reads the shipped `starburst.dds` and rejects what it cannot read.
  - `BuildMipLevels` averages correctly and builds a full chain.

## What this forecloses

- A galaxy, band, nebula or other diffuse light in the sky, without a new ADR.
- A textured sky, such as a cube map or a panorama, and a texture for the point stars.
- Parallax: the sky never moves when the view pans.
- A different sky per match or map. There is one seed.
- Stars that twinkle or move.
- Compressed textures and textures stored with their mips. The game reads uncompressed 8-bit BGRA or RGBA and builds the mips itself, until a texture needs more.
