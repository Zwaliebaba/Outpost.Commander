# ADR-061 — The HUD measures its text with the fonts it draws it in

Status: **accepted** · 2026-10-04
Owns: measured-text, text-fitting, text-layout-tests

## Context

A width that matters has to be measured with the face it is drawn in. The HUD's faces are installed fonts that DirectWrite rasterizes at a whole number of pixels for the screen's scale ([ADR-030](ADR-030-typography-and-sprites.md)), so the same line is a different width at each size, and a layout that guesses a width, by a count of characters or a share of a face's size, lets text run out of its panel or into its neighbor with nothing to catch it.

## Decision

1. **The layout takes the fonts' metrics.** `Hud::Lay` and `Hud::LayMenu` take a `Hud::TextMetrics`, a view of the atlas's fonts (`Neuron::GlyphAtlas::Font`) in `Hud::Typefaces`' order, and the scale they were rasterized at. It measures a line as `UiPipeline::DrawText` sets it: each character at its advance in whole pixels, and the tracking rounded to a whole pixel, divided back into reference units. The layout holds no GPU state (ADR-015 decision 4). It reads the fonts where they are, so they outlive it.
2. **The client measures with the atlas it draws with.** `UiPipeline::UseScale` rasterizes the atlas again when the scale, which includes the player's interface factor ([ADR-070](ADR-070-interface-scale.md)), moves a font by a whole pixel. `GameClient::Update` calls it before laying out, so the first frame at a new size is measured with the fonts it is drawn in. `UiPipeline::Fonts` gives them.
3. **What is measured.** A figure against a right edge and a panel fitted to its lines take the measured width. A line that could run past what holds it is cut to that width with three dots (`TextMetrics::Fit`), leaving `FIT_GAP_UNITS` from anything beside it, such as a cost. A topic's effect is broken between words to its card's width (`TextMetrics::Wrap`).
4. **A topic's prerequisites, and a part card's splash note and lock line, get a line each.** A topic's prerequisites stand one to a line, each with its checkbox: "NEEDS · IMPROVED EXTRACTION", then "+ HULL PLATING". Every topic card is as tall as the most lines any shown topic needs, so the cards keep their places as the window scrolls. A part card's splash note and its lock line have a line each, and a row of cards with a note is taller, by 16 units at [ADR-062](ADR-062-type-scale-and-contrast.md)'s sizes. Lines are spaced for the rounding at both sizes, since at 1280×720 each face is rasterized at a whole number of pixels and its line height is rounded up. So a damage card's figure starts 30 units down, clear of the hull's name above it, and the Queue button's label and detail stand at 2 and 31 units.
5. **The tests measure with the same faces.** `HudTests` rasterize `Hud::Typefaces` with `Neuron::RasterizeUiAtlas`, which uses DirectWrite and needs no GPU, at each back buffer's scale. A face that is not installed throws, as ADR-030 has it, and the tests fail. They never fall back to an estimate. Two tests hold the layout to its panels: `KeepsEveryTextInsideItsPanel` and `OverlapsNoTwoTexts`. They lay out the menu, and every window with the longest content the game makes, at 1920×1080 and 1280×720.

## Consequences

- **The tests need the game's fonts on the machine that runs them:** Segoe UI, Bahnschrift, and Cascadia Mono or Consolas. Windows installs all four, and CI's Windows runner has them.
- **A layout can only be checked on Windows.** The fonts come from DirectWrite, so the HUD's layout cannot be tested off Windows.
- **A line box is its font's line height,** the ascent and descent DirectWrite gives, rounded up to a whole pixel. Where the layout spaces lines more tightly than that, `OverlapsNoTwoTexts` fails, even if the drawn glyphs would not touch.
- The type's sizes and contrast are [ADR-062](ADR-062-type-scale-and-contrast.md)'s.
