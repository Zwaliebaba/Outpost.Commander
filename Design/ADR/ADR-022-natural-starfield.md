# ADR-022 — The sky is an even field of stars, with its brightest few drawn as starburst sprites

Status: **accepted** · 2026-10-01 · Supersedes [ADR-021](ADR-021-starfield.md)

## Context

ADR-021 drew the sky as about 193,000 Gaussian points: an even field and a Milky Way band, with a bulge, star clouds and dust lanes made of nothing but stars. The owner ran it and did not like it, and asked on 2026-10-01 for three changes: remove the Milky Way, make the sky look more natural, and see whether two sprites added to `OutpostCommander/Assets/Textures/` make sense for it. The sprites are `glow.dds`, a soft round falloff, and `starburst.dds`, a bright core with diffraction spikes. Both are 128 by 128, uncompressed 8-bit BGRA in the legacy DDS header, with one mip level, white in their color and with their shape in their alpha. The owner added them.

ADR-021 still holds on everything this ADR does not change: stars are directions at infinity, drawn first with no depth test, sized in pixels on the 1920×1080 reference frame and added to the frame. The clear color is black.

## Decision

1. **The Milky Way is gone.** The bulge, the band, the clouds and the dust go with it. The sky is one field of 50,000 stars spread evenly over the whole sphere, from the same fixed seed and SplitMix64 as before.
2. **Brightness is the natural distribution.** The count of stars brighter than a light falls as that light to the power −3/2, which is how stars spread evenly through space look. So most stars are near the faintest, few are bright and very few are very bright. This is what makes the sky look natural rather than evenly speckled. A point's light is its peak times its Gaussian's area: a brighter point is drawn wider, up to a spread of 1.6 pixels, rather than brighter than a peak of 0.75. Colors keep ADR-021's seven blackbody tints, with 65% of the tint kept.
3. **The brightest 70 stars of the whole sky are drawn with `starburst.dds`; the rest stay Gaussian points.** That makes 6–9 starbursts on screen at a time. A burst's square is 9 to 36 pixels in radius on the reference frame, growing with the logarithm of its light. Its color is its tint at a peak of 0.6. The sprite is drawn upright on the screen whichever way the camera turns, which is how a camera's diffraction spikes behave.
4. **`glow.dds` is not used.** Shrunk to a point star's 4–6 pixels, it is the same soft spot as the Gaussian in `StarPS.hlsl`. The Gaussian is exact at every size and needs no texture, sampler or descriptor heap. The starburst has its own soft core, so it does not need a glow under it either.
5. **`Neuron::StarPipeline` draws either kind.** Without a sprite it draws Gaussians as before. With a `TextureData`, its pixel shader, `StarSpritePS.hlsl`, returns the star's color times the sprite's alpha. A star's size is now `radiusPixels`, half its square's side; for a Gaussian, the rim is three standard deviations out. A sprite pipeline adds a descriptor table for the texture at t0, a shader-visible heap with its one view, and a static trilinear sampler with clamped addressing. `GameClient` draws the points, then the bursts: two instanced draws.
6. **`Neuron::ParseDds` reads a DDS file without a library (R14).** It reads only what the game uses: an uncompressed 2D texture of 8-bit BGRA or RGBA texels, in the legacy header or the DX10 one, with any mip levels the file has. It throws, naming the file, for anything else: compressed formats, cube maps, volumes and arrays, a short file, or bytes left over.
7. **`Neuron::BuildMipLevels` makes the sprite's mip chain on the CPU at start**, from 128 by 128 down to 1 by 1. Each texel is the rounded average of the 2 by 2 above it. `Renderer::CreateStaticTexture` gains an overload that uploads every level. Without mips, a 128-pixel sprite drawn 18 to 72 pixels across would sample its thin spikes at a texel here and there, and they would shimmer as the zoom changes the camera's pitch.
8. **The grid is a neutral gray that is barely visible** (owner, 2026-10-01), rather than ADR-021's dim blue. Its minor lines are 0.012 and its major lines 0.020 in every channel, in linear color. Lit from above, as every mesh is (ADR-011), they show at about 26 and 35 out of 255 on screen. The grid stays for what ADR-021 gave it: it shows the ground moving when the view pans, and where the map ends.
9. **`starburst.dds` is packaged as deployment content**, a `None` item with `DeploymentContent`, like every other file the game reads at runtime. The other four textures stay as the owner added them, as `Image` items, until something reads them.

## Consequences

- **The sky is far cheaper than ADR-021's.** It has 49,930 points and 70 bursts instead of 193,390 points, so 200,000 vertex-shader invocations a frame instead of 773,560. One draw call and one 128-by-128 texture with its mip chain are added: 87,380 bytes of texels. The pixel cost is a few thousand small points plus 6–9 sprites. None of this is measured on a GPU.
- **Building the sky is quicker.** There is no noise any more. It is 50,000 random draws and a sort.
- **The look is checked offline only.** The previews were made by compiling the generator in the Linux container, then projecting its stars in Python through the camera's matrices. The preview emulated `StarPS.hlsl` and the sRGB encoding, drew the sprite resampled to each burst's size, and put the dimmed grid on top. The game itself has not run it.
- **The sky is no longer tied to the camera's starting yaw.** With no band, every direction of the sky looks alike, and Q and E turn it without anything going out of view.
- **`GameAppTests` checks it without a GPU.**
  - The sky is the same every time.
  - It has 50,000 stars, of which 70 are bursts.
  - Every point is a unit direction within its size and brightness limits.
  - Every burst is larger than any point and no larger than its sprite.
  - Each 30-degree cap round the six axes holds its share of the sky within 10%.
  - `ParseDds` reads the shipped `starburst.dds` and rejects what it cannot read.
  - `BuildMipLevels` averages correctly and builds a full chain.

## What this forecloses

- A galaxy, band or nebula in the sky, without a new ADR.
- Using a texture for the point stars.
- Compressed textures and textures stored with their mips. The game reads uncompressed 8-bit BGRA or RGBA and builds the mips itself, until a texture needs more.
- ADR-021's other foreclosures still hold: no parallax, one seed, and stars that do not twinkle or move.
