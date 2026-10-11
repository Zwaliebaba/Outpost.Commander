# ADR-090 — A warm key and a cool fill light the faces, and the lines take only the light's brightness

Status: **accepted** · 2026-10-11

## Context

On 2026-10-11 the owner shared four screenshots of a mobile war game and asked what it would take to give this game its look in space. The review ([the look plan](../../GameDesign/ImplementationPlan-Look.md)) found that game's look to be mostly light: warm light on what faces the sun, cool light in the shade, occlusion in every recess, and a bright palette. The owner asked to keep the vector look of [ADR-027](ADR-027-rock-crease-lines.md) and [ADR-040](ADR-040-lines-over-dark-faces.md) and change the light and colors, and decided gate A2 as proposed: a warm key and a cool fill in `MeshPS.hlsl`, no face darker than today's ambient where the camera looks, and a test that holds the lines over the faces. This is task LV1.1.

Until now a face was lit by Lambert's law from one light over an ambient floor, `color × (0.3 + 0.7 × max(0, n·l))` ([ADR-011](ADR-011-meshes-and-shading.md) decision 6), and a line by the same pixel shader, as bright as the face it lies on. Three facts set the problem:

- **A face and its line are lit alike**, so the ratio of their luminances moves only through WCAG's flare term, 0.05: a brighter light raises it, a darker one lowers it. It is set by how dark the faces are against the lines (`FILL_SHADE` against `EdgeColor`), not by the light.
- **ADR-040's worst ratios are on the darkest face the camera sees**, where only the ambient floor reaches. Replayed from the formulas, over every normal in 3° steps that the camera sees at its lowest pitch: a Human ship's lines stand 2.75:1 over its faces there and 3.94:1 at best, a Human structure's 3.38:1, an ore asteroid's 2.81:1 and a field's 2.36:1. A face twice as bright takes the Human ship's worst to 1.93:1.
- **Every line the scene draws goes through that pixel shader**: the models' ridges, the grid, the sectors' borders, the footprint rings and the selection's ring. A colored light would tint all of them, and the selection's green and a side's color carry meaning ([ADR-085](ADR-085-one-meaning-per-color.md)).

## Decision

1. **A face is lit by a key and a fill, each a linear color with its brightness in it.**
   - The key comes from `TOWARD_KEY`, which is the old light's direction: above and behind the default view's top-left. It reaches a face as far as the face turns toward it, softened by a wrap: `saturate((n·l + KEY_WRAP) / (1 + KEY_WRAP))`, so the edge between light and shade is soft rather than a crease of its own.
   - The fill blends from `GROUND_FILL` on a face that looks straight down to `SKY_FILL` on one that looks straight up, by `n.y × 0.5 + 0.5`.
   - A face's color is its color times the sum, per channel.
   - The numbers: the key (0.627, 0.539, 0.427), warm; the sky (0.365, 0.421, 0.571), cool; the ground (0.314, 0.287, 0.389), a dimmer cool violet; the wrap 0.3. They are first values, to be judged in the owner's run.
2. **A line takes the luminance of that light, in a pixel shader of its own, `MeshLinePS.hlsl`.** The faces carry the light's warmth and coolness; the lines keep their hue, so a ridge says whose a model is, and the grid, the borders and the rings keep their colors. `MeshPipeline`'s line state uses it in place of `MeshPS.hlsl`.
3. **Where the camera can look, no face gets less light than ADR-040's ambient floor**, so the ratio of each model's lines to its faces holds. Computed with the numbers above: the least light on a face the camera sees has a luminance of 0.315, against the old 0.3, and the most is 0.96, against the old 1.0. A face that looks straight up is lit (0.895, 0.877, 0.932), within 7% of gray, so a mark that lies flat on the ground, such as the placement ghost, keeps its color.
4. **The light, and the colors a model's faces and lines are drawn in, are `GameApp/SceneShading.h`'s.** `TOWARD_KEY`, `KEY_COLOR`, `KEY_WRAP`, `SKY_FILL` and `GROUND_FILL` are there, with `FaceLight` and `LineLight`, which work the light out as the shaders do, and the face and line colors that were `GameClient.cpp`'s: `FILL_SHADE`, `ROCK_FILL_SHADE`, `FIELD_SHADE`, `STRUCTURE_GRAY_SHARE`, `STRUCTURE_FILL_SHADE`, `EDGE_BRIGHTNESS`, `EDGE_WHITE_SHARE`, `ROCK_EDGE_BRIGHTNESS`, `Shaded`, `EdgeColor` and `TowardGray`. They moved unchanged, so that a test reads what the game draws.
5. **`MeshPipeline::FrameConstants` carries the light in place of the ambient share**: the direction toward the key and its wrap, the key's color, the sky's and the ground's. The pipeline still knows no game concept; the game says what the light is.

## Consequences

- **The contrast holds, computed, not measured on a screen.** The least ratio of each model's lines to its faces, over every normal in 3° steps that the camera sees at its lowest pitch, 40°, by WCAG's ratio of the linear colors' luminances:

  | Model | ADR-040's light | This light |
  |---|---|---|
  | Human ship | 2.75 | 2.77 |
  | Human structure | 3.38 | 3.43 |
  | Tarkan ship | 2.82 | 2.88 |
  | Tarkan structure | 3.44 | 3.52 |
  | Pirate ship | 2.83 | 2.83 |
  | Pirate structure | 3.44 | 3.49 |
  | Wreck ship | 3.42 | 3.49 |
  | Wreck structure | 3.84 | 3.93 |
  | Ore asteroid | 2.81 | 2.88 |
  | Field rock | 2.36 | 2.42 |

  The best ratios fall by at most 0.1, since the brightest face is a little dimmer.
- **The change is in hue, by design.** A face that turns to the key is warm and one that turns from it cool; the brightness of each is close to what it was. The mood shows most on large faces, the structures'. Occlusion and a second paint, which change more, are LV2's.
- **The test.** `SceneShadingTests`:
  - `KeepsEveryModelsLinesOverItsFaces` reads every set in `Models.json` and `Camera.json`'s pitches, and holds each kind of model's lines at its floor over its faces on every face the camera can see: ships 2.7:1, structures 3.3:1 and rocks 2.3:1, ADR-040's worst less a margin. With faces at twice the shade and a fill a third as bright, it fails for every model.
  - `LightsNoFaceTheCameraSeesBelowTheOldAmbient`, `LightsAFaceThatLooksUpNearlyGray` and `WarmsTheKeyAndCoolsTheFill`.
  - They ran in the Linux container with gcc 13 and clang 18, against DirectXMath and a stand-in for the unit-test framework and for the two loaders, fed the repository's `Models.json` and `Camera.json` values. CI runs them on Windows.
- **The shaders.** `MeshPS.hlsl`, `MeshLinePS.hlsl`, `MeshVS.hlsl` and `MeshLineVS.hlsl` passed glslang's HLSL front end in the Linux container, and `fxc` with warnings as errors in CI. The light is a few more instructions a pixel than Lambert's; it was not measured apart, and is inside the whole frame's GPU work that [ADR-091](ADR-091-a-phosphor-glow.md) records.
- **`FaceLight` and the two pixel shaders must agree**, and nothing checks it but review: each names the others.
- **Built and run.** CI built it Debug|x64 and ran `SceneShadingTests` on Windows, and the owner ran a Release build on 2026-10-11. Whether the warmth reads, and whether the numbers want moving, is the owner's call.

## What this forecloses

- A colored light on a line, without a new decision: lines keep their hue.
- A face the camera can see lit below ADR-040's ambient floor, while the lines are the vector look's.
