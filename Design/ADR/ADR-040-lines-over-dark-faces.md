# ADR-040 — The lines carry the shape: dark faces, multisampled lines pulled toward the eye, and structures behind the ships

Status: **accepted** · 2026-10-03

## Context

On 2026-10-03 the owner shared a screenshot zoomed in on the Human Command Station, with two Constructors beside it, and asked for a review of how the scene reads. The review found five faults, and the owner asked for every recommendation to be carried out.

1. **The faces outshone the lines.** Faces were drawn at 0.65 of a model's color and lines at 2 times it, clamped to 1. The Human blue is (0.18, 0.42, 0.95), so its blue channel clamps and the line comes out (0.36, 0.84, 1.0), which is cyan, the Ion exhaust's color (ADR-019); the Tarkan line clamps to orange the same way. By the luminance of linear sRGB, a lit Human facet's line stood about 2.5:1 above the face beside it, and 2.1:1 on the dark side, while the face stood about 6:1 above the black of space. The eye read solid blue shapes and found the lines second, which is the reverse of the eighties vector look ADR-027 was after.
2. **The lines stood off the silhouettes.** Each line was lifted along its faces' mean normal by 0.5% of the mesh's largest extent, because Direct3D gives a line no depth bias. On a silhouette that normal points across the screen, so the line was drawn a few pixels outside the shape with sky between them, worst on the largest models at the closest zoom: 0.3 m on a 60 m Command Station.
3. **Nothing was antialiased.** The swap chain had one sample a pixel, so every line and silhouette stepped, and pixel-wide lines crawled as the camera moved.
4. **A side's structures and ships were one color at one brightness**, so the structures, which are large and do not take orders, took the eye from the ships, which do. A structure also stood on nothing: the grid is faint and it had no mark on the ground, so it read as floating and its size was hard to judge.
5. **On the minimap, an asteroid field and an ore asteroid were the same gray**, so a field, which is only in the way, read as heavier than the ore asteroid, which is worth going to, since a field's square is the larger.

## Decision

1. **The faces are dark and the lines are bright and keep their hue.** Every model's faces are drawn at 0.3 of its color, not 0.65. A ship's or structure's lines are its color made up to 2 times brighter, but no further than its brightest channel reaching 1, so that the hue holds, and then 35% of the way to a white as bright as that channel (`EdgeColor` in `GameClient`). The Human line is (0.47, 0.64, 1.0), a light blue rather than cyan, and the Tarkan line (1.0, 0.53, 0.44), a light red rather than orange. A darker color, such as a structure's while it is built, gives darker lines in proportion. A rock's lines are 1.35 times its gray, as ADR-028 decision 4 has them; its faces darken to 0.3 with every other model's, so the terrain recedes further.
2. **A line is pulled toward the eye, not along its normal.** `Neuron::BuildCreaseLines` no longer lifts anything: its lines lie on the edges. `Neuron::MeshPipeline` draws lines with a vertex shader of their own, `MeshLineVS.hlsl`, which moves each vertex a share of the way from where it is toward the camera's eye. A point moved along its own sightline stays where it is on the screen, so a line can no longer stand off a silhouette, yet it is nearer than the face it lies on and wins the depth test. The share is per instance: `MeshPipeline::Instance::liftShare`, beside the world matrix and the color ([ADR-053](ADR-053-instanced-meshes.md)), and `FrameConstants::eyePosition` gives the eye. Model lines use 0.2%, about 2.5 pixels of depth at the camera's 45° vertical field of view on a 1080-pixel screen, which is far more than the depth buffer's resolution there, since `Camera` puts the near plane at 1% of the distance to the focus. The grid uses none: nothing lies on it, and pulled, it would show through the hulls that cut it.
3. **The scene is multisampled, four samples a pixel.** `Renderer` draws every frame's scene into a 4-sample `R8G8B8A8_UNORM_SRGB` scene target with a 4-sample depth buffer, and every pipeline state that draws the scene draws with `Renderer::SAMPLE_COUNT`. A shader resolves it into the back buffer, with the samples averaged in linear color, and the interface is drawn after it at one sample ([ADR-050](ADR-050-shader-resolve.md)). The line pipeline state turns `MultisampleEnable` on, so a line is rasterized as a quadrilateral a pixel wide that the samples smooth. Feature level 11_0, the floor ADR-006 sets, guarantees four samples for both formats.
4. **A structure stands back from the ships.** A structure's color is taken 35% of the way to a gray as light as it is, and its faces are drawn at 0.22 of that rather than 0.3, for its shards too. It keeps its side's hue, and the ships are the brightest, most saturated things of their side's color.
5. **Every structure stands on a faint ring, which puts it on the ground and says its size** ([ADR-042](ADR-042-faint-footprint-lines.md)).
6. **On the minimap, a field is darker than an ore asteroid.** The ore asteroid is [ADR-043](ADR-043-hud-in-the-windows-look.md)'s darkened gold, and the field is [ADR-046](ADR-046-second-look-at-the-screen.md)'s near-black.

## Consequences

- **The contrast figures are computed, not measured.** By the luminance of linear sRGB, with the light's full share on a facet and its 0.3 ambient on the dark side: a Human ship's lines stand about 3.9:1 above its faces lit and 2.8:1 dark, a Tarkan ship's 4.1:1 and 2.8:1, and a structure's about 5.4:1 and 3.4:1. They were 2.5:1 and 2.1:1 for a Human model, and 2.2:1 and 1.8:1 for a Tarkan one.
- **Multisampling costs GPU time that has not been measured.** The scene target and its depth buffer are four times the memory of one sample, about 66 MB together at 1920×1080, and every frame adds a resolve ([ADR-050](ADR-050-shader-resolve.md)). Q4 was met with a mean of 2.25 ms of GPU work a frame (design §3); it is measured again with `--measure --stress` before this is taken as settled. If it costs too much, two samples is a change to `Renderer::SAMPLE_COUNT` and to the resolve shader's copy of it.
- **A line's pull hides it behind less than about 2.5 pixels of depth.** A line on a far face shows through a near part thinner than that, and a line on the far side of a silhouette shows within that depth of the rim. Both are below what the eye picks out at that scale.
- **The Tarkan lines turn from orange toward a light red.** Lightening a red toward white gives pink before it gives orange; the side still reads as the red one.

## What this forecloses

- A line lifted along its normal, which stands off a silhouette.
- An unantialiased scene, without changing this ADR.
- A structure as bright and saturated as its side's ships.
