# ADR-030 — The interface draws several fonts, a few characters past ASCII, sprites and hatching from one atlas

Status: **accepted** · 2026-10-02 · Supersedes ADR-015's one font at one size, and its ASCII-only text

## Context

Phase 1 brings the ship designer to the owner's mockup (Phase 1 design §11, `GameDesign/Mockups/ShipDesigner.png`), and the research and production panels with it. The mockup sets titles in a condensed bold face, labels in small spaced capitals, names in a semibold face and figures in a monospaced one at two sizes; it marks Ore with a ◆, a part research has yet to unlock with a box and hatching, and a window's corners with brackets; and it writes × and ·. ADR-015 rasterizes one font, Segoe UI semibold, at one size, holds printable ASCII only, and forecloses shipping a font. The owner decided on 2026-10-02 to use Bahnschrift and Cascadia Mono, which Windows 11 installs, so that nothing ships, and that the game uses Consolas where Cascadia Mono is not installed (gate H7). Task 9.1 builds what the windows of tasks 9.2–9.4 draw with.

## Decision

1. **One atlas, several fonts.** `Neuron::UiPipeline` is built with a list of `Neuron::FontDesc`: the families to look for in order, a weight, a stretch and a size in reference units. `Neuron::RasterizeFont` draws each with DirectWrite as ADR-015 did, at its size times ADR-006's scale, and `Neuron::PackGlyphs` packs every font into one `R8_UNORM` texture, so the interface is still one draw call. A font is named by its index in the list; `Outpost::Hud::Typefaces` gives the game's list, and `Hud::Typeface` names them: the HUD's text as before, and a title, a label, a name, a figure and a large figure in the mockup's faces. The atlas is drawn again when the scale moves any font or sprite by a whole pixel.
2. **A font that is not installed fails.** The first installed family of a font's list is used, and a font none of whose families is installed throws `Neuron::Exception` naming them, rather than drawing in whatever the system offers first, as ADR-015's did. The figures list Cascadia Mono and then Consolas (gate H7), and in Debug the game writes which it found to the debugger.
3. **Text is UTF-8, and the atlas holds printable ASCII, · and ×.** `Neuron::NextCodePoint` reads one character at a time; a character of more than two bytes, or a byte that starts no character, shows as `?`, once. Other characters are added to `GlyphAtlas::EXTRA_CHARACTERS` when the interface needs them.
4. **Letters can be spaced.** `DrawText` and `TextWidth` take a tracking in pixels, added between each two characters, for the mockup's small capitals.
5. **Sprites are drawn into the atlas, not shipped.** `Neuron::DrawSprite` draws a diamond, a checkbox outline or a corner bracket in a square of any size, edges smoothed, on the CPU, and the atlas packs them after the fonts. `UiPipeline::DrawSprite` draws one into a rectangle in any color, mirrored to make a bracket's other three corners. `Hud::Sprites` lists the game's: Ore's diamond, the locked part's box, and the window's corner.
6. **Hatching is drawn by the pixel shader.** `FillHatched` sends one quad whose texture coordinate u is negative: `UiPS.hlsl` then draws diagonal stripes rising to the right, v pixels wide every −u pixels, on the screen's pixels, so neighboring hatched rectangles line up. A repeating tile in the atlas would have taken a quad per tile, hundreds for a window's header, and the sampler clamps.
7. **The HUD looks as it did.** Its text stays in Segoe UI semibold at 20 units, now the first typeface; the new faces are used by the windows of tasks 9.3 and 9.4.

## Consequences

- **The atlas is larger:** six fonts of 97 characters and three sprites, against one font of 95. At the reference scale it is a texture of a few hundred texels a side, measured as 512×128 for two fonts in a test; it grows with the scale at 2880×1920. It is still uploaded only on a resize.
- **Bahnschrift and Segoe UI must be installed**, as they are on Windows 11. The game stops with a message if one is not, rather than drawing the interface in the wrong face.
- **The shader samples the atlas with `SampleLevel`**, since a branch may not take derivatives; the atlas has no mipmaps, so nothing changes.
- **`GameAppTests` checks it without a GPU:** packing several fonts and sprites without overlap, UTF-8 and the fallback, tracking, the sprites' shapes, rasterizing Segoe UI with its ×, and a font list falling back to its second family, or failing with none installed. The HUD test checks the typefaces' order. The look is the owner's run.

## What this forecloses

- Shipping a font file, still (ADR-015).
- Text shaping, wrapping and right-to-left text, still: lines are laid out one glyph after another.
- Characters past two bytes of UTF-8 without a change to `NextCodePoint`.
