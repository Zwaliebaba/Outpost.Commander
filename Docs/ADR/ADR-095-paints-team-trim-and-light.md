# ADR-095 — A model's faces take one of three paints, team, trim and light, and each line takes its faces' paint

Status: **accepted** · 2026-10-25
Owns: mesh-paints, paint-colors, two-tone-lines

## Context

Every model is one color: the side's, on every face and every line ([ADR-040](ADR-040-lines-over-dark-faces.md)). A building in the look the owner asked for (owner, 2026-10-11) paints its side's color on its body and keeps its base and trim neutral, and its windows and lamps are lit. The owner decided the slots on 2026-10-11: `team`, `trim` and `light`, by the material's name, in one version of the format. Every source has one material today, `team`, and no vertex colors.

Two constraints hold the colors:

- **The lines must stand over their faces** on every face the camera sees, to the floors [ADR-090](ADR-090-a-warm-key-and-a-cool-fill.md) sets, which `SceneShadingTests` holds.
- **The engine knows no game concept** (AGENTS.md R9): the side, the trim and the lamps are the game's names.

## Decision

1. **A face's paint is its material's name** in its source: `team`, `trim` or `light`, with Blender's `.001` suffixes allowed, as a hardpoint's name has them ([ADR-018](ADR-018-nmf-and-hardpoints.md) decision 4). A primitive with no material is `team`. Any other name, or a material the file does not hold, fails the bake, naming the file. Each vertex takes its primitive's paint.
2. **The paint is a byte of the vertex, in `.nmf` version 4:** 0 `team`, 1 `trim` and 2 `light`, after the openness, in one of the three zero bytes version 3 kept ([ADR-045](ADR-045-levels-and-spinning-parts.md) decision 3). `Neuron::ParseNmf` reads it into `MeshVertex::paint`, and refuses a paint of 3 or more.
3. **The engine draws a vertex in its copy's color for its paint, by number.** `MeshPipeline::Instance` carries three colors, `color`, `secondColor` and `thirdColor`, and `thirdGlows`. A paint of 2 on a copy whose third glows is drawn in its color as it is, which no light reaches and no occlusion darkens; otherwise each paint is lit as every face is. `Instance::InOneColor` gives every paint one color and lets none glow, and `Draw`, `DrawLines`, `DrawTriangles` and `DrawLineList` draw with it, so a ring, a border, the grid, a remembered structure and an explosion's shards look as they did. The vertex is 32 bytes and the instance 128.
4. **The game's colors are `SceneShading.h`'s,** and `GameClient` draws each model with them (`PaintedInstance`):
   - `team` is the model's color, its faces and lines as ADR-040 has them.
   - `trim` is `TrimColor`: the gray as light as the team color, its faces shaded and its lines made up from it as the team color's are. It reads apart from the body, and the side still reads from the body.
   - `light` is `LIGHT_PAINT_COLOR`, (1.0, 0.86, 0.62), a warm white of luminance 0.87, on its faces and its lines alike. It is well over the glow's threshold of 0.35 ([ADR-091](ADR-091-a-phosphor-glow.md)), so a lamp glows.
   - A structure under construction darkens its team and trim as it darkened its one color, and keeps its lights lit.
5. **A line takes its faces' paint** (`BuildCreaseLines`): theirs where they share one, and `team` where they differ, so a seam between body and trim is drawn in the side's color; a rim's line, which has one face, takes that face's. `team` lines are the side's line color, `trim` lines the trim's, and `light` lines the light's color, unlit.

## Consequences

- **Nothing on the screen changes until the owner paints a model:** every source still has one material, `team`, so each of the 45 `.nmf` files baked again differs from its version 3 only in the version, byte for byte otherwise. A source takes `trim` and `light` from Blender's material names, with no step in the game.
- **Trim keeps the contrast, and more:** a ship's trim lines stand 3.27–3.47:1 over its trim faces and a structure's 3.67–3.92:1, against the team color's 2.77–3.49:1 and 3.43–3.93:1, over every face the camera sees, wholly open, half and shut in. The figures are computed as `SceneShadingTests` computes them, from `Models.json`'s four colors, not measured on a screen; the test holds the trim to the ships' and the structures' floors.
- **A lamp in a recess glows as one in the open,** since occlusion does not darken it; its line is lost against it, the same color.
- **The tests.**
  - `BakeMeshes.py --self-test`: `team`, `trim.001` and `light` painted 0, 1 and 2, a primitive with no material `team`; `chrome`, `Trim` and a material the file does not hold refused.
  - `MeshDataTests`: each vertex's paint read, a paint of 3 refused, two bytes of padding, versions 1, 2, 3 and 5 refused; a crease between two paints in the first, between two of one paint in it, and a rim in its face's.
  - `SceneShadingTests`: each set's ship and structure trim held to the floors; `PaintsTeamTrimAndLight`.
- **The shaders were not compiled in the container,** which has no shader compiler; CI's `FXCompile` builds them.

## What this forecloses

- A fourth paint, or a paint the data chooses, without a new version of the format.
- Vertex colors and textures: a face's color is its paint's.
- A light that dims or goes out, such as while its structure is built, without a new decision.
