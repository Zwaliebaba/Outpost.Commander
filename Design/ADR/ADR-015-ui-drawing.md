# ADR-015 — The interface is text and panels from one DirectWrite glyph atlas, drawn as quads by D3D12

Status: **accepted** · 2026-10-01

## Context

The game draws its own interface over the D3D12 scene (ADR-001), laid out in 1920×1080 reference units and scaled to the back buffer (ADR-006). Gate G7 asked how it draws text, panels and input focus. R14 rules out the usual libraries, so the choice was between Windows SDK content: Direct2D and DirectWrite through a wrapped D3D11 device every frame, a bitmap font baked offline and shipped, or DirectWrite rasterizing glyphs once for D3D12 to draw. Task 3.6's first HUD, the Ore stockpile and a selection panel, is the first user. Q4 measures frame time with the HUD on screen (design §3).

## Decision

The owner decided on 2026-10-01: **DirectWrite rasterizes a glyph atlas once, and D3D12 draws text and panels as quads.**

1. **`Neuron::RasterizeGlyphs`** asks DirectWrite (`IDWriteFactory2`) for printable ASCII in an installed font, `Segoe UI` semibold, at the size it will be shown in pixels. Each glyph is drawn in grayscale with `IDWriteGlyphRunAnalysis` into one byte of coverage per pixel. `Neuron::PackGlyphs` packs them in rows into one `R8_UNORM` texture, with a texel of space round each and a solid block for panels. Any other character shows as `?`. Text is left-aligned single lines; there is no wrapping, shaping or right-to-left text.
2. **`Neuron::UiPipeline`** draws quads in back-buffer pixels: one root signature, one pipeline state with straight alpha blending and no depth, one shader-visible descriptor for the atlas, and up to 8,192 quads a frame written to an upload buffer per frame in flight. A panel samples the solid block, so text and panels are one draw call. Shaders `UiVS.hlsl` and `UiPS.hlsl` are compiled into the executable like the mesh shaders (AGENTS.md §2).
3. **Text stays sharp at every scale.** The atlas is rasterized at the reference size times ADR-006's scale, and again when the scale changes by a whole pixel of font size. Rasterizing waits for the GPU, so it happens on a resize, not every frame. Glyphs are placed on whole pixels.
4. **Layout is the game's.** `Outpost::Hud` in `GameApp` describes the HUD's content from the newest snapshot and the selection, and lays it out in reference units with each element anchored to a corner or an edge (ADR-006), as panels and lines of text in pixels. It keeps no GPU state, so its tests run without one.
   - **What the full HUD holds** (task 4.5, design §9):
     - The Ore and its income at the top left.
     - The selection at the bottom middle, with a structure's construction, hit points and queue.
     - The buttons at the bottom right, which build structures and queue ships. A button's label carries its cost, and a button the player cannot afford or use is dim and takes no click.
     - The minimap at the bottom left.
     - A hint at the top middle while a placement is armed.
   - **The minimap** is the map's square, +x to the right and +z up. Every entity is a square in its side's color, at its size or a few pixels. The camera's view is outlined as the box around the ground the screen shows.
   - **Research and the designer** are [ADR-017](ADR-017-research-and-the-designer.md)'s: a line under the Ore for the topic under way, the Research Lab's queue and topics in its panels, and the designer at the top right beside a selected Shipyard.
5. **Input focus is a rectangle test.** A mouse button press on a HUD panel belongs to the HUD and does not reach the player's controls. A release always does, so that a drag begun in the world ends wherever it is let go. The designer's name field is the one element that takes keys: while it has them, no order, control group or camera key reads the keyboard (ADR-017).

## Consequences

- **Nothing ships for text.** The font is the one Windows installs, and nothing is baked or packaged.
- **Only printable ASCII.** Design names and numbers are ASCII, and the designer takes no other character into a name (ADR-017). Text from elsewhere with other characters shows `?` in their place.
- **One font size.** Headings or small print need a second atlas or a second size in this one.
- **The interface costs one draw call and one upload of its vertices per frame.** Q4's measurement (task 3.7) includes it.

## What this forecloses

- Direct2D, D3D11On12 and GDI for the interface, without a new ADR.
- Shipping a font file.
