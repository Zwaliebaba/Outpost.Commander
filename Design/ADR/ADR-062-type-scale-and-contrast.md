# ADR-062 — The HUD's type scale, the contrast of its text, and the room they take

Status: **accepted** · 2026-10-04

## Context

The review of 2026-10-03, task 14.2 of the interface plan (`GameDesign/ImplementationPlan-Interface.md`), found two problems. The figures and labels a player decides by were set at 11–13 units, which are pixels at 1080p, where Xbox Accessibility Guideline 101 asks 18 px on PC. A locked card's text stood at 3.6–4.0:1 against its card, under WCAG's 4.5:1. Gate H6 accepted the mockup's sizes and colors as built (task 9.3). They live in `Hud.cpp`, not in an ADR, so this ADR records what changes from what H6 accepted.

The owner decided gate K1 on 2026-10-03 as the plan proposed. On 2026-10-04 the owner decided three findings the proposal had not predicted, and the production window's width. Those answers are marked below.

## Decision

1. **The type scale.**
   - The label face goes from 12 units to 13.
   - The figure face goes from 13 to 14, at weight 600. That is Cascadia Mono's semibold; Consolas has none and meets 600 with its bold.
   - The detail face goes from 11 to 13.
   - The body face (20), the title face (22), the name face (16) and the large figures (28) stay.
2. **One place sets a face's size: `Hud::FACE_UNITS`, read through `Hud::FaceUnits`.** `Typefaces` rasterizes each face at that size, and Ore's gem beside a figure is sized from it. The sizes written beside calls are gone: `TITLE_FACE_UNITS`, and the 13 and 22 passed with a figure.
3. **Every text stands at 4.5:1 or more against what it is drawn on.** Contrast is WCAG's ratio of relative luminances, computed from the linear colors in `Hud.cpp` over black. A hatched panel's stripes and the gaps between them are both counted. These colors are raised; the other labels stay at 4.8–5.9:1.
   - **Locked text** goes from (0.15, 0.19, 0.26) to (0.20, 0.26, 0.35). It stands at 4.6:1 on a locked card's stripes, 5.1:1 on its body and 5.3:1 on a dim card. Its luminance is 29% of live text's.
   - **Labels on a window's hatched title bar** take the row labels' color: "SHIPYARD 05 · DESIGNER", "QUEUE · 4 BUILT", "PRODUCTION" and "RESEARCH". They go from 3.6:1 on the stripes to 5.8:1.
   - **The name's count**, such as "21/32", takes the labels' color, from 3.5:1 to 5.9:1. Its old color, `FAINT_COLOR`, is gone.
   - **A shown chip's code** takes the accent blue on its lit face, from 3.1:1 to 5.9:1 (owner, 2026-10-04).
   - **A worse figure's red** goes from (0.75, 0.08, 0.044) to (0.80, 0.09, 0.05), from 4.3:1 to 4.6:1 on a window (owner, 2026-10-04). The health and rating bars use the same red.
   - **The Queue button's green face and stripes** go to 0.65 of the mockup's, so the gold of its cost goes from 3.5:1 to 4.6:1 (owner, 2026-10-04). The gold stays the gold of every other cost.
4. **The designer grows to fit.**
   - Its part cards and saved-design chips go from 190 units wide to 220, so a hull's numbers fit at the detail size. That line is like "1,400 HP · ARM 24 · 25m/s". The chips are 216 since [ADR-068](ADR-068-interface-polish.md) grew their arrows.
   - The window goes from 728 units wide to 818.
   - Its header's queue slots and its Save button are placed from its right edge, not at fixed places.
   - A card keeps the mockup's 80 units. A row of cards with a note is 2 units taller than before, 96, with the note at 52 and the lock line 20 units from the foot.
5. **Lines get room at every scale's rounding**, as [ADR-061](ADR-061-measured-text.md) spaces them.
   - A topic card's lines go from 16 units apart to 18.
   - A figure's line goes from 16 units to 17.
   - The designer's slot rows set their label and pick 18 units apart, at 21 and 39, which were 22 and 38. At 1280×720 the label face, rasterized at 9 px, has a line about 11 px tall, and 16 units rounded to 10 px on the module row.
6. **Production narrows to 478 units, and research stays 560** (owner, 2026-10-04). Both still stand side by side under the Ore, clear of the designer, at the reference width: 16 + 478 + 16 + 560 + 16 + 818 + 16 = 1920. Production's width is computed from that sum. A wider designer narrows production, and does not push research under the designer. [ADR-043](ADR-043-hud-in-the-windows-look.md) is rewritten for it.

## Consequences

- **The designer takes more of the screen.** This changes Phase 1 design §12's "about 38% of the screen's width and 65% of its height". With the mockup's three weapons, the designer is 818 × 810 units: 43% of a 1080p screen's width and 75% of its height. With five weapons and the module row, it is 818 × 913, which is 43% and 85%. Before this change it was 728 × 808 and 728 × 909. Most of the height came before this ADR, from Phase 2's module row and ADR-061's note lines. K1's proposal quoted 816 × 840 for a designer without either. These figures come from `ExtentOf`'s arithmetic, not from a screen.
- **A production card is 211 units wide, where it was 252.** Measured, "Small+Ion+Mass Driver" fits it. A name wider than the card is cut short with three dots (ADR-061).
- **The contrast figures are computed, not measured on a screen.** They come from the colors in `Hud.cpp` with WCAG's formula. The render target encodes the linear colors to sRGB, which is what makes them the displayed ones. A window's body is 97% opaque, and the 3% of the world behind it is not counted.
- **The large text keeps the same 4.5:1 floor as the small text.** WCAG would accept 3:1 for the title face, but the title face is under 15 px at 1280×720.
- **The test.** `HudTests.SetsEveryTextAtFourAndAHalfToOne` checks every text of the menu, and of every window with the longest content, at 1920×1080. It covers parts locked, parts unlocked, and a part previewed that makes some figures better and some worse. No text is exempt. `NamesItsTypefacesAndSprites` holds the sizes and the figure face's weight. ADR-061's two layout tests hold the room at 1920×1080 and 1280×720.
- **The owner's run closes K1's sizes.**
