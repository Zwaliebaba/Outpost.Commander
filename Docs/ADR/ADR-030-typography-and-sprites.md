# ADR-030 — The interface draws several fonts, a few characters past ASCII, sprites and hatching from one atlas

Status: **accepted** · 2026-10-02
Owns: interface-fonts, font-fallback, atlas-characters, letter-tracking, ui-sprites, ui-hatching

## Context

The ship designer, and the research and production windows with it, follow the owner's mockup, `Docs/Mockups/ShipDesigner.png`. The mockup sets titles in a condensed bold face, labels in small spaced capitals, names in a semibold face and figures in a monospaced one at two sizes; it marks a part research has yet to unlock with a box and hatching, and a window's corners with brackets; and it writes × and ·. Ore has a mark of its own, a cut gem ([ADR-085](ADR-085-one-meaning-per-color.md) decision 3). ADR-015 forecloses shipping a font. The game uses Bahnschrift and Cascadia Mono, which Windows 11 installs, so that nothing ships, and Consolas where Cascadia Mono is not installed (owner, 2026-10-02).

## Decision

1. **One atlas, several fonts.** `Neuron::UiPipeline` is built with a list of `Neuron::FontDesc`: the families to look for in order, a weight, a stretch and a size in reference units. `Neuron::RasterizeFont` draws each with DirectWrite as ADR-015 has it, at its size times ADR-006's scale, and `Neuron::PackGlyphs` packs every font into one `R8_UNORM` texture, so the interface is one draw call. A font is named by its index in the list; `Outpost::Hud::Typefaces` gives the game's list, and `Hud::Typeface` names them: `Body` in Segoe UI semibold, and `Title`, `Label`, `Name`, `Figure`, `LargeFigure` and `Detail` in the mockup's faces (`Docs/Mockups/ShipDesigner.png`), Bahnschrift for the words and Cascadia Mono or Consolas for the figures. Each face's size is [ADR-062](ADR-062-type-scale-and-contrast.md) decision 2's. The atlas is drawn again when the scale moves any font or sprite by a whole pixel.
2. **A font that is not installed fails.** The first installed family of a font's list is used, and a font none of whose families is installed throws `Neuron::Exception` naming them, rather than drawing in whatever the system offers first. The figures list Cascadia Mono and then Consolas (owner, 2026-10-02), and in Debug the game writes which it found to the debugger.
3. **Text is UTF-8, and the atlas holds printable ASCII, · and ×.** `Neuron::NextCodePoint` reads one character at a time; a character of more than two bytes, or a byte that starts no character, shows as `?`, once. Other characters are added to `GlyphAtlas::EXTRA_CHARACTERS` when the interface needs them.
4. **Letters can be spaced.** `DrawText` and `TextWidth` take a tracking in pixels, added between each two characters, for the mockup's small capitals.
5. **Sprites are drawn into the atlas, not shipped.** `Neuron::DrawSprite` draws a cut gem ([ADR-085](ADR-085-one-meaning-per-color.md) decision 3), a checkbox outline or a corner bracket in a square of any size, edges smoothed, on the CPU, and the atlas packs them after the fonts. `UiPipeline::DrawSprite` draws one into a rectangle in any color, mirrored to make a bracket's other three corners. `Hud::Sprites` lists the game's: Ore's gem, the locked part's box, and the window's corner.
6. **Hatching is drawn by the pixel shader.** `FillHatched` sends one quad whose texture coordinate u is negative: `UiPS.hlsl` then draws diagonal stripes rising to the right, v pixels wide every −u pixels, on the screen's pixels, so neighboring hatched rectangles line up. A repeating tile in the atlas would take a quad per tile, hundreds for a window's header, and the sampler clamps.
7. *(retired)*

## Consequences

- **The atlas holds seven fonts of 97 characters and three sprites.** It is uploaded at startup and again when the scale changes.
- **Bahnschrift and Segoe UI must be installed**, as they are on Windows 11. The game stops with a message if one is not, rather than drawing the interface in the wrong face.
- **The shader samples the atlas with `SampleLevel`**, since a branch may not take derivatives; the atlas has no mipmaps, so it loses nothing.
- **`NeuronClientTests` checks it without a GPU:** packing several fonts and sprites without overlap, UTF-8 and the fallback, tracking, the sprites' shapes, rasterizing Segoe UI with its ×, and a font list falling back to its second family, or failing with none installed. The HUD test in `GameAppTests` checks the typefaces' order.

## What this forecloses

- Shipping a font file (ADR-015).
- Text shaping and right-to-left text: lines are laid out one glyph after another.
- Characters past two bytes of UTF-8 without a change to `NextCodePoint`.
