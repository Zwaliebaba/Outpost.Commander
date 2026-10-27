# ADR-070 — The player scales the interface, as far as its windows fit

Status: **accepted** · 2026-10-05
Owns: interface-scale-factor, interface-fit-check

## Context

[ADR-006](ADR-006-renderer-shape.md) scales the interface to the screen by one factor, the largest at which the 1920×1080 reference frame fits. Xbox Accessibility Guideline 101 asks that text can be enlarged to 200% of its 18 px. The HUD's smallest text is 13 units ([ADR-062](ADR-062-type-scale-and-contrast.md)), which is 13 px at 1080p. A window of a fixed size that grows past the screen cannot be used, so the player scales the interface beyond the screen's fit with Ctrl+= and Ctrl+-, as far as the windows fit (owner, 2026-10-03).

## Decision

1. **A factor multiplies ADR-006's scale.** `Hud::Scale(width, height, factor)` is the screen's fit times the factor. The HUD, the windows and the menu are laid out, measured and hit-tested at it, so a window's place is kept in reference units and `Hud::KeepOnScreen` keeps it reachable at any step.
2. **The factor steps through 100%, 110%, 125%, 150%, 175% and 200%** (`Hud::INTERFACE_STEPS`). Ctrl+= takes the next step and Ctrl+- the one before, on the keys that read = and - on a US keyboard (`KEY_LARGER_INTERFACE` and `KEY_SMALLER_INTERFACE`, [ADR-068](ADR-068-interface-polish.md)). The Controls window lists them.
3. **A step up is taken only while the windows of a fixed size fit the screen between its margins** (`Hud::WindowsFit`), each as its row of the windows' table counts it ([ADR-100](ADR-100-the-canvas.md) decision 8):
   - In a match, the designer, laid out for every component of the match, locked or not; the Controls window; the orders window; the away window at its most lines; and the build window with every structure the match offers.
   - On the menu there is no match, so the Controls window alone.
   - The production window grows with the saved designs and is not counted, nor is research, nor the leave window, which is smaller than the away window.
4. **When the screen's size changes, or a match starts or ends, the factor comes down** to the largest step that fits (`Hud::StepInterface` with no step), so a designer never opens taller than the screen.
5. **The step the player takes is kept from one session to the next** ([ADR-099](ADR-099-interface-settings-kept.md) decision 1), and the game starts at it, brought down as far as decision 4 says. A step down that decision 4 makes is not kept. Without a kept step, the factor starts at 100%.
6. **The atlas is drawn again at the new scale**, as it is when the scale moves a font by a whole pixel ([ADR-030](ADR-030-typography-and-sprites.md)). `UiPipeline::UseScale` does it before the frame lays out its text.
7. **What is drawn in the world keeps the screen's own scale:** the stars ([ADR-022](ADR-022-natural-starfield.md) decision 5), the bars over the entities ([ADR-088](ADR-088-selection-and-commands.md) decision 5) and the selection's ring ([ADR-067](ADR-067-selection-ring-at-a-fixed-width.md)).

## Consequences

- **The designer limits the factor.** With five weapons and the module row it is 913 units tall, 938 with the Sensors bar of the tuning data's Sensor Array, so every 16:9 screen, 1080p or 2160p alike, reaches 110% in a match and no further. A 3:2 screen such as 2880×1920 reaches 125%. The menu reaches 125% on a 16:9 screen. The orders window, 560 units tall, does not change these limits, since the designer is the tallest. These figures are computed from the designer and the Controls window as the canvas builds them, not measured on a screen.
- **200% is not reached.** That would need the designer to scroll or reflow, and it does neither. Text at 110% of 13 units is 14.3 px at 1080p, short of the guideline's 18.
- **At 110%, windows overlap at first.** Production, research and the designer stand side by side only at 100%. A window opens at its default place, measured in the smaller reference screen, and the player moves it.
- **The fit is checked only when it can change**: on a key, on a new size of screen, and as a match starts or ends. Checking builds each window it counts once on the canvas, without text.
- **The tests.**
  - `HudTests.StepsTheInterfaceAsFarAsItsWindowsFit` holds the steps and their limits on three screens.
  - `KeepsAMovedWindowOnTheScreenAcrossAStep` keeps a moved window reachable at every step.
  - The layout tests of [ADR-061](ADR-061-measured-text.md) and the contrast test of ADR-062 run at every step the longest content's windows fit, at 1920×1080 and 1280×720.

## What this forecloses

- A factor that leaves a fixed window taller than the screen, without a scrolling designer.
