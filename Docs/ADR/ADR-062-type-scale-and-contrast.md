# ADR-062 — The HUD's type scale, the contrast of its text, and the room they take

Status: **accepted** · 2026-10-04
Owns: hud-type-scale, text-contrast-floor, designer-window-width, side-by-side-window-widths

## Context

The HUD's figures and labels are what a player decides by. Xbox Accessibility Guideline 101 asks 18 px on PC for such text, and WCAG asks that text stand at 4.5:1 against what it is drawn on. The HUD's sizes and colors live in `Hud.cpp`. A larger face takes more room: the designer has to fit a hull's numbers on a part card at the detail size, and the production and research windows have to stand beside the designer at the reference width. The scale, the locked text's color and the room they take are the owner's (2026-10-03); three more colors and production's width are the owner's answers of 2026-10-04, marked below.

## Decision

1. **The type scale**, in reference units, which are pixels at 1080p (owner, 2026-10-03): the body face 20, the title face 22, the name face 16, the large figures 28, the label face 13, the detail face 13, and the figure face 14 at weight 600, Cascadia Mono's semibold, which Consolas meets with its bold.
2. **One place sets a face's size: `Hud::FACE_UNITS`, read through `Hud::FaceUnits`.** `Typefaces` rasterizes each face at that size, and Ore's gem beside a figure is sized from it. No call passes a size of its own.
3. **Every text stands at 4.5:1 or more against what it is drawn on.** Contrast is WCAG's ratio of relative luminances, computed from the linear colors in `Hud.cpp` over black. A hatched panel's stripes and the gaps between them are both counted. The labels stand at 4.8–5.9:1, and these colors are set for the floor:
   - **Locked text** is (0.20, 0.26, 0.35). It stands at 4.6:1 on a locked card's stripes, 5.1:1 on its body and 5.3:1 on a dim card. Its luminance is 29% of live text's.
   - **Labels on a window's hatched title bar** take the row labels' color, at 5.8:1 on the stripes: "SHIPYARD 05 · DESIGNER", "QUEUE · 2 / 5", "BUILT · 4", "PRODUCTION" and "RESEARCH", and the headers "CONTROLS", "SCHEDULED ORDERS", "WHILE YOU WERE AWAY" and "LEAVE UNIVERSE".
   - **The name's count**, such as "21/32", takes the labels' color, at 5.9:1.
   - **A shown chip's code** takes the accent blue on its lit face, at 5.9:1 (owner, 2026-10-04).
   - **A worse figure's red** is (0.80, 0.09, 0.05), at 4.6:1 on a window (owner, 2026-10-04). The rating bars use the same red; a health bar's colors are [ADR-088](ADR-088-selection-and-commands.md) decision 5's.
   - **The Queue button's green face and stripes** are 0.65 of the mockup's, so the gold of its cost stands at 4.6:1 (owner, 2026-10-04). The gold is the gold of every other cost.
4. **The designer is as wide as its cards.**
   - Its part cards are 220 units wide, so a hull's numbers fit at the detail size, a line like "1,400 HP · ARM 24 · 25m/s". The saved-design chips' width is [ADR-068](ADR-068-interface-polish.md) decision 1's.
   - The window is 818 units wide.
   - Its header's queue slots and its Save button are placed from its right edge.
   - A card is the mockup's 80 units tall, and 96 in a row of cards with a note, the note at 52 and the lock line 20 units from the foot.
5. **Lines get room at every scale's rounding**, as [ADR-061](ADR-061-measured-text.md) spaces them.
   - A topic card's lines are 18 units apart.
   - A figure's line is 17 units.
   - The designer's slot rows set their label and pick 18 units apart, at 21 and 39. At 1280×720 the label face, rasterized at 9 px, has a line about 11 px tall.
6. **Production is 478 units wide, and research 560** (owner, 2026-10-04). Both stand side by side under the Ore, clear of the designer, at the reference width: 16 + 478 + 16 + 560 + 16 + 818 + 16 = 1920. Production's width is computed from that sum, so a wider designer narrows production and does not push research under the designer.

## Consequences

- **The designer takes a large share of the screen.** With the mockup's three weapons, the designer is 818 × 810 units: 43% of a 1080p screen's width and 75% of its height. With five weapons and the module row, it is 818 × 913, which is 43% and 85%, and 818 × 938 with the Sensors bar a module with sight adds, 87% of the height. These figures come from `ExtentOf`'s arithmetic, not from a screen.
- **A production card is 211 units wide**, two to a row in production's 478, computed from the window's padding and gap. A name wider than the card is cut short with three dots (ADR-061).
- **The contrast figures are computed, not measured on a screen.** They come from the colors in `Hud.cpp` with WCAG's formula. The render target encodes the linear colors to sRGB, which is what makes them the displayed ones.
- **The large text keeps the same 4.5:1 floor as the small text.** WCAG would accept 3:1 for the title face, but the title face is under 15 px at 1280×720.
- **The test.** `HudTests.SetsEveryTextAtFourAndAHalfToOne` checks every text of the menu, and of every window with the longest content, at 1920×1080. It covers parts locked, parts unlocked, and a part previewed that makes some figures better and some worse. No text is exempt. `NamesItsTypefacesAndSprites` holds the sizes and the figure face's weight. ADR-061's two layout tests hold the room at 1920×1080 and 1280×720.
