# ADR-040 — The lines carry the shape: dark faces, multisampled lines pulled toward the eye, and structures behind the ships

Status: **accepted** · 2026-10-10
Owns: face-and-line-shading, line-depth-pull, scene-multisampling, structure-shading

## Context

The scene is drawn in the eighties vector look ([ADR-027](ADR-027-rock-crease-lines.md)): the eye should find a model's lines first and its faces second, on its lit side and its dark side alike. Four things bear on how that is drawn:

- **A face as bright as its lines hides them**, and a line made brighter channel by channel clamps at 1 and loses its hue. The Human blue, (0.18, 0.42, 0.95), would clamp to cyan, the Ion exhaust's color ([ADR-019](ADR-019-glows-and-exhaust.md)), and the Tarkan red to orange.
- **Direct3D gives a line no depth bias.** A line drawn on the edge of its own faces fights them in the depth test. Lifted along its faces' normal instead, it is drawn outside the shape on a silhouette, where that normal points across the screen, with sky between the line and the shape.
- **A line one pixel wide steps and crawls as the camera moves** unless the scene is antialiased.
- **The structures are large and do not take orders; the ships do.** A side's structures must stand back from its ships, so that they do not take the eye from what the player commands (owner, 2026-10-03).

## Decision

1. **The faces are dark and the lines are bright and keep their hue,** so that the lines carry the shape on a model's lit side and its dark side alike. A ship's faces are drawn at 0.3 of its color (`FILL_SHADE`), and a structure's and a rock's at less (decision 4, and below). A ship's or structure's lines are its color made up to 2 times brighter, but no further than its brightest channel reaching 1, so that the hue holds, and then 35% of the way to a white as bright as that channel (`EdgeColor`, in `GameApp/SceneShading.h`, [ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md) decision 4). The Human line is (0.47, 0.64, 1.0), a light blue, and the Tarkan line (1.0, 0.53, 0.44), a light red. A darker color, such as a structure's while it is built, gives darker faces and lines in proportion. A rock's lines are 1.35 times its gray (`ROCK_EDGE_BRIGHTNESS`) and keep its hue, and its faces are drawn at 0.15 of its color, half a ship's (`ROCK_FILL_SHADE`), so that its lines stand out from its faces but do not outshine the ships' and structures' lines, and the terrain stands back from the fleet. A field's color is darker than an ore asteroid's by `FIELD_SHADE`, 0.7, before either.
2. **A line is pulled toward the eye, not along its normal.** `Neuron::BuildCreaseLines` lifts nothing: its lines lie on the edges. `Neuron::MeshPipeline` draws lines with a vertex shader of their own, `MeshLineVS.hlsl`, which moves each vertex a share of the way from where it is toward the camera's eye. A point moved along its own sightline stays where it is on the screen, so a line cannot stand off a silhouette, yet it is nearer than the face it lies on and wins the depth test. The share is per instance: `MeshPipeline::Instance::liftShare`, beside the world matrix and the color ([ADR-053](ADR-053-instanced-meshes.md)), and `FrameConstants::eyePosition` gives the eye. Model lines use 0.2% (`LINE_LIFT_SHARE`), about 2.5 pixels of depth at the camera's 45° vertical field of view on a 1080-pixel screen, which is far more than the depth buffer's resolution there, since `Camera` puts the near plane at 1% of the distance to the focus. The grid uses none: nothing lies on it, and pulled, it would show through the hulls that cut it.
3. **The scene is multisampled, four samples a pixel.** `Renderer` draws every frame's scene into a 4-sample `R8G8B8A8_UNORM_SRGB` scene target with a 4-sample depth buffer, and every pipeline state that draws the scene draws with `Renderer::SAMPLE_COUNT`. A shader resolves it into the back buffer, with the samples averaged in linear color, and the interface is drawn after it at one sample ([ADR-050](ADR-050-shader-resolve.md)). The line pipeline state turns `MultisampleEnable` on, so a line is rasterized as a quadrilateral a pixel wide that the samples smooth. Feature level 11_0, the floor ADR-006 sets, guarantees four samples for both formats.
4. **A structure stands back from the ships.** A structure's color is taken 35% of the way to a gray as light as it is (`STRUCTURE_GRAY_SHARE`), and its faces are drawn at 0.22 of that (`STRUCTURE_FILL_SHADE`) rather than a ship's 0.3, for its shards too. It keeps its side's hue, and the ships are the brightest, most saturated things of their side's color.
5. *(retired)*
6. *(retired)*

## Consequences

- **The lines' contrast over the faces is computed in [ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md)'s Consequences**, under the light that ADR decides, and `KeepsEveryModelsLinesOverItsFaces` holds it.
- **Multisampling costs memory and a resolve.** The scene target and its depth buffer are four times the memory of one sample: about 66 MB together at 1920×1080, computed from their formats, four bytes of color and four of depth a sample. Every frame adds a resolve ([ADR-050](ADR-050-shader-resolve.md)). Two samples would be a change to `Renderer::SAMPLE_COUNT` and to the resolve's and the bloom's copies of it.
- **A line's pull hides it behind less than about 2.5 pixels of depth.** A line on a far face shows through a near part thinner than that, and a line on the far side of a silhouette shows within that depth of the rim. Both are below what the eye picks out at that scale.
- **The Tarkan lines are a light red, not orange.** Lightening a red toward white gives pink before it gives orange; the side reads as the red one.

## What this forecloses

- A line lifted along its normal, which stands off a silhouette.
- An unantialiased scene, without changing this ADR.
- A structure as bright and saturated as its side's ships.
