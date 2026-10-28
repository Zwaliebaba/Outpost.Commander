# ADR-015 — The interface is text and panels from one DirectWrite glyph atlas, drawn as quads by D3D12

Status: **accepted** · 2026-10-10
Owns: glyph-atlas, ui-pipeline, hud-layout, minimap-view, ui-input-focus

## Context

The game draws its own interface over the D3D12 scene (ADR-001), laid out in 1920×1080 reference units and scaled to the back buffer (ADR-006). It needs text, panels and lines, and it has to decide which clicks are its own and which reach the world. R14 rules out the usual libraries, which leaves three ways to draw text, all Windows SDK content: Direct2D and DirectWrite through a D3D11 device wrapped over D3D12 (D3D11On12) every frame, a bitmap font baked offline and shipped, or DirectWrite rasterizing a glyph atlas once for D3D12 to draw. The owner chose the atlas (owner, 2026-10-01).

## Decision

1. **`Neuron::RasterizeFont`** asks DirectWrite (`IDWriteFactory2`) for the atlas's characters in an installed font, at the size it will be shown in pixels. Each glyph is drawn in grayscale with `IDWriteGlyphRunAnalysis` into one byte of coverage per pixel. `Neuron::PackGlyphs` packs them in rows into one `R8_UNORM` texture, with a texel of space round each and a solid block for panels. The fonts, the characters and the sprites the atlas holds are [ADR-030](ADR-030-typography-and-sprites.md)'s; any other character shows as `?`. The engine draws a line of text at a time, glyph after glyph, with no shaping and no right-to-left text; breaking text into lines is the layout's ([ADR-061](ADR-061-measured-text.md) decision 3), with the engine's measure ([ADR-100](ADR-100-the-canvas.md) decision 2).
2. **`Neuron::UiPipeline`** draws quads in back-buffer pixels: one root signature, one pipeline state with straight alpha blending and no depth, and up to `MAX_QUADS`, 8,192, quads a frame written to an upload buffer per frame in flight; a quad past them is dropped. The atlas's view is in the renderer's shader-visible heap ([ADR-051](ADR-051-one-shader-visible-heap.md)). It draws into the back buffer at one sample, after the scene is resolved ([ADR-050](ADR-050-shader-resolve.md)). Text, panels, which sample the solid block, hatched rectangles ([ADR-030](ADR-030-typography-and-sprites.md) decision 6), sprites (ADR-030 decision 5) and a second single-channel image, the fog's shades for the minimap ([ADR-052](ADR-052-fog-texture.md)), are one draw call. So is a line at any angle (`DrawSegment`): one quad laid along a `Neuron::ScreenSegment`, half its width to either side and past either end, so that lines meeting at a corner close it. `ScreenSegment` works out the quad's corners on the CPU and knows no game concept, so `NeuronClientTests` tests it without a GPU. Shaders `UiVS.hlsl` and `UiPS.hlsl` are compiled into the executable like the mesh shaders (AGENTS.md §2).
3. **Text stays sharp at every scale.** The atlas is rasterized at each font's reference size times ADR-006's scale, with the player's factor ([ADR-070](ADR-070-interface-scale.md)), and again when the scale moves a font or a sprite by a whole pixel (ADR-030). The first atlas is rasterized on another thread while the device is created ([ADR-049](ADR-049-startup-in-parallel.md)). Rasterizing it again waits for the GPU, so it happens when the scale changes, on a resize or a step of the factor, not every frame. Glyphs are placed on whole pixels.
4. **The HUD's layout is the game's.** `Outpost::Hud` in `GameApp` describes the HUD's content from the newest snapshot and the selection, and lays it out in reference units with each element anchored to a corner, an edge or the middle (ADR-006), into the engine's display list of panels, lines at any angle, sprites and lines of text in pixels ([ADR-100](ADR-100-the-canvas.md) decision 1), which [ADR-031](ADR-031-floating-windows.md) decision 4 draws in layers. It keeps no GPU state and no camera, so its tests run without one.
   - **Where the HUD's parts stand:**
     - The Ore and its income at the top left, and under it one column of the status, territory and alerts panels ([ADR-066](ADR-066-production-status-on-the-hud.md) decision 3).
     - A hint at the top middle while a placement is armed, and the match's end under its place. Where they would meet the column under the Ore or the tag at its widest, as the interface's scale grows ([ADR-070](ADR-070-interface-scale.md)), they stand under the top row instead, in the middle of the room right of the column, and the match's end under the hint.
     - The tag that says where the match runs at the top right ([ADR-086](ADR-086-where-the-match-runs.md)).
     - The minimap at the bottom left. The camera can center any point of the map, so nothing stays under it.
     - The selection at the bottom middle, with a structure's construction and hit points, and the buttons against its right edge ([ADR-088](ADR-088-selection-and-commands.md) decision 2). A button gives an order, opens the build window of the Constructors' structures, or opens a selected structure's windows; a button the player cannot afford or use is dim and takes no click ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 4).
   - **The minimap** is the map's square, +x to the right and +z up. Its marks are ADR-043 decisions 7 and 8's. The camera's view is the ground the screen's four corners show, joined in turn as lines, so that it shows the near side and the far and turns with the camera; what runs past the map is cut at the minimap's edge.
   - **The windows** float over the HUD ([ADR-031](ADR-031-floating-windows.md)): the designer, production and research ([ADR-017](ADR-017-research-and-the-designer.md)), the Controls window ([ADR-068](ADR-068-interface-polish.md)), the orders and away windows ([ADR-080](ADR-080-events-and-scheduled-orders.md)), a universe's leave window ([ADR-078](ADR-078-dedicated-server.md) decision 11), and the build window (ADR-031 decision 6).
5. **Input focus is a rectangle test.** A mouse button press on a HUD panel or a window belongs to the HUD and does not reach the player's controls. A release always does, so that a drag begun in the world ends wherever it is let go. The windows are tested front to back before the HUD (ADR-031). The designer's name field is the one element that takes keys ([ADR-017](ADR-017-research-and-the-designer.md) decision 8).

## Consequences

- **Nothing ships for text.** The fonts are ones Windows installs (ADR-030), and nothing is baked or packaged.
- **Design names are printable ASCII.** The designer takes no other character into a name (ADR-017). Text from elsewhere with a character the atlas does not hold shows `?` in its place.
- **The view's outline is four quads, and stays inside the minimap.** `ScreenSegmentTests` checks a segment's corners at 0°, 45° and 90° and none for a segment of no length, and `HudTests.DrawsTheViewAsItsFourCorners` the view's lines with the camera unturned and turned a quarter, no box left, and a view over the map's edge cut there.
- **The interface costs one draw call and one upload of its vertices per frame.**
- **`HudTests.StandsTheHintAndTheBannerAsideOfACrowdedTopRow`** holds the hint and the banner in the middle of the top row at 100%, and under it at 150%, clear of the column and the tag.

## What this forecloses

- Direct2D, D3D11On12 and GDI for the interface, without a new ADR.
- Shipping a font file.
