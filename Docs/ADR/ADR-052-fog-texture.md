# ADR-052 — The fog is a texture, written when it changes, and the ground and the minimap both sample it

Status: **accepted** · 2026-10-08
Owns: fog-update, fog-texture, minimap-fog

## Context

The fog's shades change only when one of the player's entities moves into a new 20 m cell (`FogOfWar::CELL_METERS`), or a held sector comes into or goes out of sight. The server ticks far less often than the screen draws.

On the repository's map ([ADR-036](ADR-036-map-sectors.md) decision 4) the grid is hundreds of thousands of cells, and a held sector is thousands of them, seen whole ([ADR-024](ADR-024-fog-of-war.md) decision 9). Working the whole grid out again for every change costs far more than what changed.

Both the ground and the minimap draw the fog. The ground's pixel shader reads the shades for every pixel, and upload memory, on a discrete GPU, sits across the PCIe bus. `UiPipeline` draws at most a frame's `MAX_QUADS` and drops the rest (ADR-015 decision 2), so a minimap fog laid out as rectangles competes with text for them.

## Decision

1. **`FogOfWar` is brought up to date once a snapshot, not once a frame, and only where something changed.** `GameClient` updates it when the newest snapshot's tick changes, from that frame's entities.
   - Each cell keeps a count of the player's entities that see it and of the held sectors, not suppressed, that hold it; it is in sight while either is above zero.
   - An update counts again only what changed: a sector that came into or went out of sight, an entity that appeared or went, and an entity whose place or sight moved. A circle of sight is a run of cells on each row, so a moved one is counted out and in only where its old and new runs differ.
   - Only the cells whose counts left or reached zero have their shades worked out again. The shades are cell for cell those a whole update gives: the same test of each cell's center.
   - `FogOfWar::Revision` moves on whenever `Reset` or `Update` changes a shade, and only then. `FogOfWar::ChangedRows` flags each row with a changed shade until `ClearChangedRows`; `Reset` flags every row.
2. **The shades are an `R8_UNORM` texture of 512 × 512 texels, owned by `GroundMaskPipeline`** (`TEXTURE_SIDE`), room for a map up to 10,240 m a side, which holds the repository's map ([`sizeMeters`](../Reference/Catalogue.md#sizeMeters), [ADR-036](ADR-036-map-sectors.md) decision 4).
   - Cell (x, z) is texel (x, z), and its byte is the shade × 255, rounded. A shade of 0.55 becomes 140/255, a change of 0.001.
   - The row and the column just past the grid repeat its last cells, so sampling at the grid's far edge blends nothing else in.
   - `SetShades` writes the bytes into this frame's slot of an upload buffer and records a copy into the frame's command list, between barriers. `GameClient` calls it only when the revision differs from the one the texture holds, with the fog's changed rows, and clears them: only those rows are written and copied, one region for each run of them, and the rest of the texture keeps what it had. A grid of a new size is written whole.
   - The texture's view is in the renderer's shader-visible heap (ADR-051).
3. **The ground mask samples the texture with a bilinear, clamped sampler.** Its pixel shader holds the point between the first and the last cell's centers, then samples at that point's texel center, which gives what four loads and a blend would, down to the sampler's filtering precision. It then maps the shade through the knee the caller sets ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 6).
4. **The minimap's fog is one panel of the display list's image fill** (`Neuron::CanvasFill::Image`, [ADR-100](ADR-100-the-canvas.md) decision 1) over the whole minimap, after the marks.
   - `Hud::Content` says only whether there is fog, not what it is.
   - `GameClient` has `UiPipeline::Draw` fill the panel from the same texture, the minimap's top at the map's far z, and it is the display list's only image.
   - `UiPipeline` takes the image through a second descriptor table. Its pixel shader tells an image quad from an atlas quad by a `u` past 2, as it tells a hatched one by a negative `u` (ADR-030).

## Consequences

- **A frame uploads the fog only when it changes,** and then only the changed rows' bytes: at most the texture's 512 × 512 bytes, 256 KB.
- An update in which nothing moved touches no cell. The ground's pixels read a texture in video memory. The minimap's fog is one quad, so it does not compete with text for the interface's quads.
- **The minimap's fog is smooth, as the ground's is.** At its size, a cell is about a pixel.
- **A cell clears up to one snapshot after an entity's sight reaches it.** At 20 m a cell, that is not visible.

## What this forecloses

- A grid larger than 512 cells a side, `GroundMaskPipeline::TEXTURE_SIDE`, which is the grid's limit: a map over 10,240 m a side at 20 m a cell.
