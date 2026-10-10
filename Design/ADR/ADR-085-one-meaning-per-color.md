# ADR-085 — One meaning per color: a warning is a chip, build progress is no side's, and Ore is a gem

Status: **accepted** · 2026-10-09

## Context

The second interface review of 2026-10-08 ([interface plan 2](../../GameDesign/ImplementationPlan-Interface2.md), finding 4) found the player's own warnings in the enemy's color. `WARNING_COLOR` stands at an sRGB hue of 17.5° and `ENEMY_COLOR` at 14.6°. In the review's first screenshot, "3 IDLE" stood directly over the enemy's "2" and "642" in what read as the same salmon. Three things were written in the warning's color as plain text: a status line's IDLE ([ADR-066](ADR-066-production-status-on-the-hud.md)), an income of nothing ([ADR-047](ADR-047-a-fight-seen-whole.md) decision 5) and the newest alert ([ADR-059](ADR-059-alerts-and-standing-orders.md) decision 2).

The same review found a blast's amber diamond particles to be Ore's glyph in shape and color: `Neuron::DrawSprite` drew Ore's mark as a filled square on a corner, the shape of [ADR-026](ADR-026-explosions-and-particles.md)'s particle.

The owner decided gate V4 as proposed on 2026-10-08. This ADR is milestone UI2's.

## Decision

1. **A warning is a chip** (task UI2.1).
   - A chip is its words in the windows' navy, `WINDOW_COLOR`, on a filled tag of `WARNING_COLOR`. The tag reaches 4 units beyond the words on either side and covers their whole line, so a descender stays on it. `Painter::Chip` draws it, and nothing else in the HUD draws a filled tag behind its words in that color, so a warning differs from the enemy's figures by its shape and its fill, not by a hue 3° away.
   - **It marks three things:**
     - **a status line's IDLE.** A status line is a list of runs, each plain or a chip (`Hud::StatusRun`), and the Shipyards' idle count is the chip "2 IDLE". A plain run is cut short to leave room for the runs after it; a chip never is.
     - **an income of nothing,** "+0/s", right-aligned where the income stands.
     - **the newest alert.** Its tag starts 4 units left of the other lines, so its words stay in line with theirs. The older alerts are in the text's color.
   - **The enemy's figures stay plain text** in `ENEMY_COLOR`.
   - **Two warnings stay plain text in the warning's color,** since each stands where no enemy figure does: a name the designer refuses, in its own field, and the main menu's notice, which wraps over several lines.
   - The chip's words stand at 10.3:1 against its tag, computed as WCAG's ratio of the two linear colors.
2. **Build progress is a neutral light gray** (task UI2.2). The bar under a structure's health bar that shows the share built is (0.5, 0.52, 0.55) linear on every side's structures, where it was a blue one shade from the player's, so an enemy's site no longer reads as the player's. It is the HUD's `BUILD_BAR_COLOR`, named once, since the HUD draws the bars ([ADR-088](ADR-088-selection-and-commands.md) decision 5). The HUD's own progress fills (`BAR_FILL_COLOR`) stay blue, since they stand on the player's own buttons and queues.

3. **Ore's glyph is a cut gem** (task UI2.3), where it was a diamond.
   - It has a flat top whose corners are cut, slopes out to its widest at two fifths of its height, and narrows to a point at its foot: the corners (0.28, 0.14), (0.72, 0.14), (0.97, 0.40), (0.50, 0.94) and (0.03, 0.40) of its square, y down. `Neuron::SpriteShape::Gem` draws it on the CPU, its edges smoothed by each pixel's distance to the nearest side's line, as the other sprites are ([ADR-030](ADR-030-typography-and-sprites.md) decision 5). The diamond is gone, since nothing else drew it.
   - It stays in `GOLD_COLOR`, everywhere Ore is written ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 3). The blast's particles stay ADR-026's diamonds.
   - The owner picked it on 2026-10-09 from three drawn the way `DrawSprite` draws them, at 8, 13 and 20 pixels: a hexagon, a tall crystal and this gem. At 8 pixels, the size of a card's cost at 1080p, the hexagon rounded off into a dot; the gem stayed unlike a diamond at every size.

## Consequences

- **The build bar is `GameClient`'s,** which needs D3D12, so it has no test; CI builds it, and the owner's run sees it on an enemy's site and an own one.

- **Tests:** `GlyphAtlasTests.DrawsTheSprites` pins the gem: full in its middle and its shoulders, where a diamond is empty, flat on top, symmetric, and narrowing to its foot. `HudTests.WritesOreOneWay` and `KeepsTheOreGemStillAsTheFigureChanges` hold with it. `HudTests.MarksEachWarningWithAChip` (the three, each at 4.5:1 or more on its tag; an older alert and the run before the chip plain; the enemy's figure plain in its color), `WarnsOfNoIncome`, `ListsTheAlerts`, and `ShowsWhatProductionAndResearchAreDoing` with the chip after the counts and taking the line's click. Tests that count the HUD's panels set an income, so that the Ore panel holds no chip.
- The whole-HUD contrast test (task 14.2) and the overlap test (task 14.1) see the chips through `LongestContent`.
- Run in the Linux container against a stand-in for the Windows headers, with substitute fonts. Not yet seen on screen.

## What this forecloses

- A warning written as plain text in `WARNING_COLOR` beside an enemy figure.
- A chip in any other color, or behind anything but a warning.
