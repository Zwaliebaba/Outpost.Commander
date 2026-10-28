# ADR-070 — The player scales the interface, as far as its windows fit

Status: **accepted** · 2026-10-05
Owns: interface-scale-factor, interface-fit-check

## Context

[ADR-006](ADR-006-renderer-shape.md) scales the interface to the screen by one factor, the largest at which the 1920×1080 reference frame fits. Xbox Accessibility Guideline 101 asks that text can be enlarged to 200% of its 18 px. The HUD's smallest text is 13 units ([ADR-062](ADR-062-type-scale-and-contrast.md)), which is 13 px at 1080p. A window of a fixed size that grows past the screen cannot be used, so the player scales the interface beyond the screen's fit with Ctrl+= and Ctrl+-, as far as the windows fit (owner, 2026-10-03). The designer's part rows and the Controls window's lines scroll, so that a window stands as tall as the screen lets it, and the HUD's bottom row, which no window covers, counts as well (owner, 2026-10-27).

## Decision

1. **A factor multiplies ADR-006's scale.** `Hud::Scale(width, height, factor)` is the screen's fit times the factor. The HUD, the windows and the menu are laid out, measured and hit-tested at it, so a window's place is kept in reference units and `Hud::KeepOnScreen` keeps it reachable at any step.
2. **The factor steps through 100%, 110%, 125%, 150%, 175% and 200%** (`Hud::INTERFACE_STEPS`). Ctrl+= takes the next step and Ctrl+- the one before, on the keys that read = and - on a US keyboard (`KEY_LARGER_INTERFACE` and `KEY_SMALLER_INTERFACE`, [ADR-068](ADR-068-interface-polish.md)). The Controls window lists them.
3. **A step up is taken only while the HUD's bottom row and the windows of a fixed size fit the screen between its margins** (`Hud::WindowsFit`):
   - **The bottom row,** in a match, at its widest: the minimap with its margin and the gap after it, the selection panel and its buttons at their widest, and the right margin, 1,280 units ([ADR-088](ADR-088-selection-and-commands.md) decision 2). It does not stack to reach a larger step, since a taller column would cover more of the battlefield.
   - **Each window at its least,** its scroll views giving up the room they can ([ADR-100](ADR-100-the-canvas.md) decision 7), as its row of the windows' table counts it (ADR-100 decision 8). In a match, that is the designer laid out for every component of the match, locked or not, its part rows a line of cards tall; the Controls window; the orders window; the away window at its most lines; and the build window with every structure the match offers.
   - On the menu there is no match, so the Controls window alone.
   - The production window grows with the saved designs and is not counted, nor is research, nor the leave window, which is smaller than the away window.
4. **When the screen's size changes, or a match starts or ends, the factor comes down** to the largest step that fits (`Hud::StepInterface` with no step), so a designer never opens taller than the screen.
5. **The step the player takes is kept from one session to the next** ([ADR-099](ADR-099-interface-settings-kept.md) decision 1), and the game starts at it, brought down as far as decision 4 says. A step down that decision 4 makes is not kept. Without a kept step, the factor starts at 100%.
6. **The atlas is drawn again at the new scale**, as it is when the scale moves a font by a whole pixel ([ADR-030](ADR-030-typography-and-sprites.md)). `UiPipeline::UseScale` does it before the frame lays out its text.
7. **What is drawn in the world keeps the screen's own scale:** the stars ([ADR-022](ADR-022-natural-starfield.md) decision 5), the bars over the entities ([ADR-088](ADR-088-selection-and-commands.md) decision 5) and the selection's ring ([ADR-067](ADR-067-selection-ring-at-a-fixed-width.md)). A fleet's plate takes the player's factor as the HUD does, since its count is text from the atlas ([ADR-101](ADR-101-plates-over-the-fleets.md) decision 2).

## Consequences

- **In a match, the bottom row limits the factor.** A 16:9, 16:10 or 3:2 screen is 1,280 units wide at 150%, which the row fills, and 1,097 at 175%, which it would run 183 units past. So every such screen, 1080p or 2160p alike, reaches 150% in a match and no further. A 21:9 screen, 2560×1080, reaches 175%; at 200% it is 540 units tall, shorter than the orders window and the designer at their least with the margins.
- **The windows at their least:** the designer 818 by 540 units, 565 with the Sensors bar of the tuning data's Sensor Array; the Controls window 700 by 500; the orders window 640 by 560; the away window 600 by 530. Each is `WindowsFit`'s own measure, read from a build of the window at its least, not measured on a screen.
- **On the menu the Controls window alone limits it,** and at 500 units it fits a 16:9 screen at 200% (540 units) between the margins, so the menu reaches 200% on every screen above.
- **What scrolls on a 16:9 screen at 150%:** the designer, 913 units tall with five weapons and the module row and 938 with the Sensors bar, stands 688 units tall, and its part rows show 244 units of their 469, or 219 with the Sensors bar; the Controls window, 784 units with all its lines, stands 688 units tall too. Neither scrolls at 110%, and the Controls window does not at 125%.
- **The column under the Ore clears the minimap at 150%:** on a 16:9 screen, 720 units tall, the column with the longest content ends at 350 units and the minimap starts at 412.
- **Text at 150% of 13 units is 19.5 px at 1080p,** past the guideline's 18. 200% is reached on the menu and not in a match.
- **At 110% and up, windows overlap at first.** Production, research and the designer stand side by side only at 100%. A window opens at its default place, measured in the smaller reference screen, and the player moves it.
- **From 125% on 16:9, the hint and the banner stand under the top row** ([ADR-015](ADR-015-ui-drawing.md) decision 4), where at 100% and 110% they stand in it.
- **The fit is checked only when it can change**: on a key, on a new size of screen, and as a match starts or ends. Checking builds each window it counts once on the canvas, without text, on a screen of no size, so that its scroll views give up all the room they can.
- **The tests.**
  - `HudTests.StepsTheInterfaceAsFarAsItsWindowsFit` holds the steps and their limits on five screens in a match and two on the menu.
  - `KeepsAMovedWindowOnTheScreenAcrossAStep` keeps a moved window reachable at every step.
  - `ScrollsTheDesignersPartRows` holds the designer within the screen at 150% and its rows scrolled to their foot.
  - The layout tests of [ADR-061](ADR-061-measured-text.md) and the contrast test of ADR-062 run at every step the longest content's windows fit, and the menu at every step it reaches, at 1920×1080 and 1280×720.

## What this forecloses

- A factor at which a window of a fixed size is taller than the screen at its least, or the bottom row wider than it.
- A bottom row that stacks to reach 175% on a 16:9 screen.
