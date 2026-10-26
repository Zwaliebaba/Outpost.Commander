# ADR-090 — A warm key and a cool fill light the faces, and the lines take only the light's brightness

Status: **accepted** · 2026-10-11
Owns: scene-light, line-light, scene-shading-constants

## Context

The scene keeps the vector look of [ADR-027](ADR-027-rock-crease-lines.md) and [ADR-040](ADR-040-lines-over-dark-faces.md), and its light gives the faces warmth and coolness: warm light on what turns toward it, cool light in the shade (owner, 2026-10-11). Three facts constrain that light:

- **Where a face and its line are lit alike,** the ratio of their luminances moves only through WCAG's flare term, 0.05: a brighter light raises it, a darker one lowers it. It is set by how dark the faces are against the lines (`FILL_SHADE` against `EdgeColor`), not by the light.
- **The lines' worst ratio over the faces is on the darkest face the camera sees,** so it is set by the least light that reaches a face the camera sees. A light that lets such a face fall darker lowers it.
- **Every line the scene draws is lit by the mesh pipeline**: the models' ridges, the grid, the sectors' borders, the footprint rings and the selection's ring. A colored light on them would tint all of them, and the selection's green and a side's color carry meaning ([ADR-085](ADR-085-one-meaning-per-color.md)).

## Decision

1. **A face is lit by a key and a fill, each a linear color with its brightness in it** (owner, 2026-10-11).
   - The key comes from `TOWARD_KEY`, above and behind the default view's top-left. It reaches a face as far as the face turns toward it, softened by a wrap: `saturate((n·l + KEY_WRAP) / (1 + KEY_WRAP))`, so the edge between light and shade is soft rather than a crease of its own.
   - The fill blends from `GROUND_FILL` on a face that looks straight down to `SKY_FILL` on one that looks straight up, by `n.y × 0.5 + 0.5`.
   - A face's color is its color times the sum, per channel, darkened by how shut in it is ([ADR-092](ADR-092-baked-occlusion.md) decision 4). A `light` face that glows is lit by neither, and drawn in its color as it is ([ADR-095](ADR-095-paints-team-trim-and-light.md) decision 3).
   - The numbers: the key (0.627, 0.539, 0.427), warm; the sky (0.365, 0.421, 0.571), cool; the ground (0.314, 0.287, 0.389), a dimmer cool violet; the wrap 0.3.
2. **A line takes the luminance of that light, in a pixel shader of its own, `MeshLinePS.hlsl`.** The faces carry the light's warmth and coolness; the lines keep their hue, so a ridge says whose a model is, and the grid, the borders and the rings keep their colors. `MeshPipeline`'s line state uses it. A `light` line is drawn in its color, unlit ([ADR-095](ADR-095-paints-team-trim-and-light.md) decision 5).
3. **No open face the camera sees is lit below `LEAST_LIGHT`, 0.3 in luminance,** which `SceneShadingTests` holds, so the ratio of each model's lines to its faces holds; occlusion ([ADR-092](ADR-092-baked-occlusion.md) decision 4) only darkens a face, down to black where it is shut in wholly, which keeps its lines the further above it. Computed with the numbers above, before occlusion: the least light on a face the camera sees has a luminance of 0.315, and the most 0.96. A face that looks straight up is lit (0.895, 0.877, 0.932), within 7% of gray, so a mark that lies flat on the ground, such as the placement ghost, keeps its color.
4. **The light, and the colors a model's faces and lines are drawn in, are `GameApp/SceneShading.h`'s.** `TOWARD_KEY`, `KEY_COLOR`, `KEY_WRAP`, `SKY_FILL` and `GROUND_FILL` are there, with `FaceLight` and `LineLight`, which work the light out as the shaders do; the face and line colors, `FILL_SHADE`, `ROCK_FILL_SHADE`, `FIELD_SHADE`, `STRUCTURE_GRAY_SHARE`, `STRUCTURE_FILL_SHADE`, `EDGE_BRIGHTNESS`, `EDGE_WHITE_SHARE`, `ROCK_EDGE_BRIGHTNESS`, `Shaded`, `EdgeColor` and `TowardGray`, which [ADR-040](ADR-040-lines-over-dark-faces.md) decisions 1 and 4 decide and this decision only keeps here; and `OCCLUSION_STRENGTH` ([ADR-092](ADR-092-baked-occlusion.md) decision 4). They are in one header so that a test reads what the game draws.
5. **`MeshPipeline::FrameConstants` carries the light**: the direction toward the key and its wrap, the key's color, the sky's and the ground's, beside the eye's position ([ADR-040](ADR-040-lines-over-dark-faces.md) decision 2) and the occlusion's strength ([ADR-092](ADR-092-baked-occlusion.md) decision 4). The pipeline knows no game concept; the game says what the light is.

## Consequences

- **The contrast holds, computed, not measured on a screen.** The least ratio of each model's lines to its faces under this light, over every normal in 3° steps that the camera sees at its lowest pitch, 40°, by WCAG's ratio of the linear colors' luminances, before occlusion, which only darkens a face:

  | Model | Least ratio |
  |---|---|
  | Human ship | 2.77 |
  | Human structure | 3.43 |
  | Tarkan ship | 2.88 |
  | Tarkan structure | 3.52 |
  | Pirate ship | 2.83 |
  | Pirate structure | 3.49 |
  | Wreck ship | 3.49 |
  | Wreck structure | 3.93 |
  | Ore asteroid | 2.88 |
  | Field rock | 2.42 |

- **The light is warm and cool, by design.** A face that turns to the key is warm and one that turns from it cool. The mood shows most on large faces, the structures'.
- **The test.** `SceneShadingTests`:
  - `KeepsEveryModelsLinesOverItsFaces` reads every set in `Models.json` and `Camera.json`'s pitches, and holds each kind of model's lines at its floor over its faces on every face the camera can see: ships 2.7:1, structures 3.3:1 and rocks 2.3:1, the least ratio of each kind in the table above less a margin. With faces at twice the shade and a fill a third as bright, it fails for every model.
  - `LightsNoFaceTheCameraSeesBelowTheOldAmbient`, which holds decision 3's `LEAST_LIGHT`, `LightsAFaceThatLooksUpNearlyGray` and `WarmsTheKeyAndCoolsTheFill`.
  - CI runs them on Windows.
- **The shaders.** CI compiles `MeshPS.hlsl`, `MeshLinePS.hlsl`, `MeshVS.hlsl` and `MeshLineVS.hlsl` with `fxc`, warnings as errors.
- **`FaceLight` and the two pixel shaders must agree**, and nothing checks it but review: each names the others.

## What this forecloses

- A colored light on a line, without a new decision: lines keep their hue.
- A face the camera can see lit, before occlusion, below decision 3's floor, while the lines are the vector look's.
