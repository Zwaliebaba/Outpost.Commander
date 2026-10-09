# ADR-052 — The fog is a texture, written when it changes, and the ground and the minimap both sample it

Status: **accepted** · 2026-10-03

## Context

The client used to do three things every frame:

1. **Update the fog.** `FogOfWar::Update` allocated a vector the size of the grid, which is 62,500 cells on the 5 km map, and rasterized every own entity's sight.
2. **Draw it on the ground.** `GroundMaskPipeline` copied all 250 KB of shades, as floats, into an upload buffer. The pixel shader then read four of them per pixel through a root shader resource view, from upload memory, which on a discrete GPU sits across the PCIe bus, and blended them itself.
3. **Draw it on the minimap.** `GameClient` copied the 250 KB of shades into `Hud::Content`, and the HUD laid out one rectangle for each run of one shade along each row. Those rectangles take quads from the interface's 8,192, which `UiPipeline` silently stops drawing past.

The shades change only when a player's entity moves into a new 20 m cell. The server ticks far less often than the screen draws.

On the 10 km map (Phase 4 design §6) the grid is 250,000 cells, and a held sector of 2 km is 10,000 of them, seen whole. Working the whole grid out again every snapshot took about 40 ms of each in an unoptimized build and 2 ms in an optimized one, measured in the Linux container on fifteen held sectors and 120 entities; the owner, playing a Debug build, saw ships stutter and the controls lag (2026-10-07).

## Decision

1. **`FogOfWar` is brought up to date once a snapshot, not once a frame, and only where something changed.** `GameClient` updates it when the newest snapshot's tick changes, from that frame's entities.
   - Each cell keeps a count of the player's entities that see it and of the held sectors, not suppressed, that hold it; it is in sight while either is above zero, as before.
   - An update counts again only what changed: a sector that came into or went out of sight, an entity that appeared or went, and an entity whose place or sight moved. A circle of sight is a run of cells on each row, so a moved one is counted out and in only where its old and new runs differ.
   - Only the cells whose counts left or reached zero have their shades worked out again. The shades are cell for cell those a whole update gives: the same test of each cell's center, checked against the earlier code over 1,200 random updates with ships moving, appearing and going and sectors changing hands.
   - `FogOfWar::Revision` moves on whenever `Reset` or `Update` changes a shade, and only then. `FogOfWar::ChangedRows` flags each row with a changed shade until `ClearChangedRows`; `Reset` flags every row.
2. **The shades are an `R8_UNORM` texture of 512 × 512 texels, owned by `GroundMaskPipeline`**, room for the 10 km map's 500 cells a side (Phase 4 design §11).
   - Cell (x, z) is texel (x, z), and its byte is the shade × 255, rounded. A shade of 0.55 becomes 140/255, a change of 0.001.
   - The row and the column just past the grid repeat its last cells, so sampling at the grid's far edge blends nothing else in.
   - `SetShades` writes the bytes into this frame's slot of an upload buffer and records a copy into the frame's command list, between barriers. `GameClient` calls it only when the revision differs from the one the texture holds, with the fog's changed rows, and clears them: only those rows are written and copied, one region for each run of them, and the rest of the texture keeps what it had. A grid of a new size is written whole.
   - The texture's view is in the renderer's shader-visible heap (ADR-051).
3. **The ground mask samples the texture with a bilinear, clamped sampler.** Its pixel shader holds the point between the first and the last cell's centers, then samples at that point's texel center. That gives what the four loads and the blend gave, down to the sampler's filtering precision. It then maps the shade through the knee the caller sets, which darkens what was seen before on the ground and leaves the minimap's shades as they are ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md)).
4. **The minimap's fog is one panel of a new `Hud::Fill::Fog`** over the whole minimap, after the marks, as the runs were.
   - `Hud::Content` says only whether there is fog, not what it is.
   - `GameClient` fills the panel with `UiPipeline::DrawImage` from the same texture, the minimap's top at the map's far z.
   - `UiPipeline` takes the image through a second descriptor table. Its pixel shader tells an image quad from an atlas quad by a `u` past 2, as it tells a hatched one by a negative `u` (ADR-030).

## Consequences

- **A frame no longer uploads the fog.** When the fog does change, the upload is the changed rows' bytes, at most 256 KB on the 10 km map, not the grid's floats.
- **Measured, the update.** In the Linux container, on 120 entities of which half moved a few meters every update, with sectors changing hands: 40 ms an update unoptimized with checked iterators before, 2.5 ms after; 1.1 ms optimized before, 0.4 ms after. An update in which nothing moved touches no cell. The ground's pixels read a texture in video memory. The minimap's fog is one quad instead of hundreds, so it no longer competes with text for the interface's quads.
- **The minimap's fog is now smooth, as the ground's always was.** At its size, a cell is about a pixel, so the runs' hard edges were not visible there either.
- **A cell is cleared up to one snapshot later than before.** At 20 m a cell, that is not visible.
- **Forecloses** a grid larger than 512 cells a side, `GroundMaskPipeline::TEXTURE_SIDE`, which was already the grid's limit: a map over 10,240 m a side at 20 m a cell.
