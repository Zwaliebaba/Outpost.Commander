# ADR-028 — The grid is pixel-wide lines, and the brightest stars are crosses

Status: **accepted** · 2026-10-05
Owns: ground-grid, star-crosses

## Context

The scene is drawn in one-pixel lines ([ADR-027](ADR-027-rock-crease-lines.md)), and the ground's grid and the brightest stars must match them. A width in meters changes with the zoom: zoomed in, a strip on the ground is a heavy bar, and zoomed out it thins below a pixel and breaks up. A soft sprite is a photographic element in a scene of vector lines. Both stand behind the fleets: a grid of uniform one-pixel lines every 100 m in a dim blue-gray, with no major lines, and a few small four-point crosses for the brightest stars (owner, 2026-10-02).

## Decision

1. **The grid is one line list drawn with `MeshPipeline::DrawLines`.** It has a line every 100 m along x and along z, out to `GRID_HALF_EXTENT_METERS`, 5,000 m, either way from the map's center, a constant of the client's that covers the 10,000 m map ([ADR-036](ADR-036-map-sectors.md)), all alike. Each is a pixel wide at any zoom, the weight of the ridges. Its color is (0.012, 0.013, 0.019) in linear color, a dim blue-gray, well below the ridges. Lines write no depth, so the grid never cuts through what is drawn over it. The sky does not move when the view pans ([ADR-022](ADR-022-natural-starfield.md) decision 5), so the grid is what shows the ground moving, and where the map ends. The sectors' borders are [ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 3's.
2. **`Neuron::StarPipeline` takes a `Shape`: `Spot`, `Sprite` or `Cross`.** `Spot` is [ADR-022](ADR-022-natural-starfield.md)'s Gaussian and `Sprite` its sprite, which needs its texture. `Cross` draws, in `StarCrossPS.hlsl`, two lines along the screen's axes and a dot of one pixel's radius at the center. Each line is one pixel of the screen wide, filtered over a pixel so that it stays smooth as the camera turns the sky, and fades linearly from the star's color at the center to nothing at the quad's edge. The shader finds a pixel's size in the quad from the corner's screen derivatives, so it needs no constant of its own.
3. **The brightest 25 stars are drawn as crosses, 6 to 16 pixels in radius on the reference frame** (owner, 2026-10-05). That is a handful on the screen at a time. Each is drawn at a peak of 0.6, white or a hot star's blue-white and never warm, so that no cross reads as a marker in Ore's gold; the points keep every tint (owner, 2026-10-03).
4. *(retired)*
5. *(retired)*
6. *(retired)*
7. *(retired)*
8. *(retired)*

## Consequences

- **`starburst.dds` is not drawn.** It stays in the package and in the repository, where `TextureDataTests` reads it, and the `Sprite` shape stays in `StarPipeline`.
- **The grid costs one draw.**
- **Nothing marks a larger step of the grid.** Every line is alike.
- **A cross's lines are a pixel of the screen**, so at 4K they are half as wide, relative to the scene, as at the reference 1080p, as the ridges are.
- **`StarfieldTests.DrawsNoBurstWarm`** checks that no cross is warm.

## What this forecloses

- A warm cross.
