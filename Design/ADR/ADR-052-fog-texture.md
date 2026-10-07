# ADR-052 — The fog is a texture, written when it changes, and the ground and the minimap both sample it

Status: **accepted** · 2026-10-03

## Context

The client used to do three things every frame:

1. **Update the fog.** `FogOfWar::Update` allocated a vector the size of the grid, which is 62,500 cells on the 5 km map, and rasterized every own entity's sight.
2. **Draw it on the ground.** `GroundMaskPipeline` copied all 250 KB of shades, as floats, into an upload buffer. The pixel shader then read four of them per pixel through a root shader resource view, from upload memory, which on a discrete GPU sits across the PCIe bus, and blended them itself.
3. **Draw it on the minimap.** `GameClient` copied the 250 KB of shades into `Hud::Content`, and the HUD laid out one rectangle for each run of one shade along each row. Those rectangles take quads from the interface's 8,192, which `UiPipeline` silently stops drawing past.

The shades change only when a player's entity moves into a new 20 m cell. The server ticks far less often than the screen draws.

## Decision

1. **`FogOfWar` is brought up to date once a snapshot, not once a frame.** `GameClient` updates it when the newest snapshot's tick changes, from that frame's entities.
   - `Update` reuses its own storage.
   - `FogOfWar::Revision` moves on whenever `Reset` or `Update` changes a shade, and only then.
2. **The shades are an `R8_UNORM` texture of 512 × 512 texels, owned by `GroundMaskPipeline`**, room for the 10 km map's 500 cells a side (Phase 4 design §11).
   - Cell (x, z) is texel (x, z), and its byte is the shade × 255, rounded. A shade of 0.55 becomes 140/255, a change of 0.001.
   - The row and the column just past the grid repeat its last cells, so sampling at the grid's far edge blends nothing else in.
   - `SetShades` writes the bytes into this frame's slot of an upload buffer and records a copy into the frame's command list, between barriers. `GameClient` calls it only when the revision differs from the one the texture holds.
   - The texture's view is in the renderer's shader-visible heap (ADR-051).
3. **The ground mask samples the texture with a bilinear, clamped sampler.** Its pixel shader holds the point between the first and the last cell's centers, then samples at that point's texel center. That gives what the four loads and the blend gave, down to the sampler's filtering precision.
4. **The minimap's fog is one panel of a new `Hud::Fill::Fog`** over the whole minimap, after the marks, as the runs were.
   - `Hud::Content` says only whether there is fog, not what it is.
   - `GameClient` fills the panel with `UiPipeline::DrawImage` from the same texture, the minimap's top at the map's far z.
   - `UiPipeline` takes the image through a second descriptor table. Its pixel shader tells an image quad from an atlas quad by a `u` past 2, as it tells a hatched one by a negative `u` (ADR-030).

## Consequences

- **A frame no longer uploads the fog.** When the fog does change, the upload is 64 KB of bytes, not 250 KB of floats. The ground's pixels read a texture in video memory. The minimap's fog is one quad instead of hundreds, so it no longer competes with text for the interface's quads.
- **The minimap's fog is now smooth, as the ground's always was.** At its size, a cell is about a pixel, so the runs' hard edges were not visible there either.
- **A cell is cleared up to one snapshot later than before.** At 20 m a cell, that is not visible.
- **Forecloses** a grid larger than 512 cells a side, `GroundMaskPipeline::TEXTURE_SIDE`, which was already the grid's limit: a map over 10,240 m a side at 20 m a cell.
