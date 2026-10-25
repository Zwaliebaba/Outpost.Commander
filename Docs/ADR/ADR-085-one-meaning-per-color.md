# ADR-085 — One meaning per color: a warning is a chip, build progress is no side's, and Ore is a gem

Status: **accepted** · 2026-10-09
Owns: warning-chip, build-bar-color, ore-gem

## Context

The player's own warnings and the enemy's figures stand side by side on the HUD, and their colors are close: `WARNING_COLOR` stands at an sRGB hue of 17.5° and the Tarkan set's orange-red, player 2's, at 16.3°, computed from the linear colors in `Hud.cpp` and `Models.json` encoded to sRGB. A warning written as plain text in its color, over an enemy's figure, reads as the same salmon. A warning has to differ from the enemy's figures by more than a hue (owner, 2026-10-08).

A structure's build progress in a blue near the player's own would make an enemy's site read as the player's.

A blast's particles are amber diamonds, a filled square on a corner ([ADR-026](ADR-026-explosions-and-particles.md)), and Ore's glyph is gold: a glyph of that shape reads as a particle, and a particle as Ore.

## Decision

1. **A warning is a chip.**
   - A chip is its words in the windows' navy, `WINDOW_COLOR`, on a filled tag of `WARNING_COLOR`. The tag reaches 4 units beyond the words on either side and covers their whole line, so a descender stays on it. `Painter::Chip` draws it, and nothing else in the HUD draws a filled tag behind its words in that color, so a warning differs from the enemy's figures by its shape and its fill, not by a hue a degree away.
   - **It marks every warning that stands where the enemy's figures do:**
     - **an idle Research Lab or Shipyards that could start work now,** "Research Lab IDLE" and the Shipyards' "IDLE 2" ([ADR-066](ADR-066-production-status-on-the-hud.md) decision 2). A status line is a list of runs, each plain or a chip (`Hud::StatusRun`); a plain run is cut short to leave room for the runs after it, and a chip never is;
     - **an income of nothing,** "+0/s", right-aligned where the income stands. Any income above nothing is in the figures' blue;
     - **the newest alert** ([ADR-059](ADR-059-alerts-and-standing-orders.md) decision 2). Its tag starts 4 units left of the other lines, so its words stay in line with theirs. The older alerts are in the text's color;
     - **the drain on the player's own tickets** on the territory panel ([ADR-057](ADR-057-domination.md) decision 8);
     - **"No word from the server"** under the tag that says where the match runs ([ADR-086](ADR-086-where-the-match-runs.md) decision 2).
   - **The enemy's figures stay plain text** in the enemy's color ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 7).
   - **Two warnings are plain text in the warning's color,** since each stands where no enemy figure does: a name the designer refuses, in its own field, and the main menu's notice, which wraps over several lines.
   - The chip's words stand at 10.3:1 against its tag, computed as WCAG's ratio of the two linear colors.
2. **Build progress is a neutral light gray.** The bar under a structure's health bar that shows the share built is (0.5, 0.52, 0.55) linear on every side's structures, so that an enemy's site never reads as the player's. It is the HUD's `BUILD_BAR_COLOR`, named once, since the HUD draws the bars ([ADR-088](ADR-088-selection-and-commands.md) decision 5). The HUD's own progress fills (`BAR_FILL_COLOR`) are blue, since they stand on the player's own buttons and queues.
3. **Ore's glyph is a cut gem** (owner, 2026-10-09).
   - It has a flat top whose corners are cut, slopes out to its widest at two fifths of its height, and narrows to a point at its foot: the corners (0.28, 0.14), (0.72, 0.14), (0.97, 0.40), (0.50, 0.94) and (0.03, 0.40) of its square, y down. `Neuron::SpriteShape::Gem` draws it on the CPU, its edges smoothed by each pixel's distance to the nearest side's line, as the other sprites are ([ADR-030](ADR-030-typography-and-sprites.md) decision 5). It stays unlike a diamond, the shape of a blast's particles ([ADR-026](ADR-026-explosions-and-particles.md)), at every size it is drawn, down to the 8 pixels of a card's cost at 1080p: the figure face's 14 units times `ORE_MARK_SHARE`, rounded (`Hud.cpp`).
   - It is in `GOLD_COLOR` wherever Ore is written ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 3), but on a card or button that cannot be pressed, where the gem and its figure take the locked text's color, `LOCKED_TEXT_COLOR`.

## Consequences

- **Tests:**
  - `GlyphAtlasTests.DrawsTheSprites` pins the gem: full in its middle and its shoulders, where a diamond is empty, flat on top, symmetric, and narrowing to its foot. `HudTests.WritesOreOneWay` and `KeepsTheOreGemStillAsTheFigureChanges` hold with it.
  - `HudTests.MarksEachWarningWithAChip` checks the Research Lab's IDLE, an income of nothing and the newest alert, each at 4.5:1 or more on its tag, and that an older alert, the run before the chip and the enemy's figure in its color are plain.
  - `HudTests.WarnsOfNoIncome` keeps an income above nothing in the figures' blue and makes "+0/s" a chip. `ListsTheAlerts`, `ShowsTheTickets` and `ShowsWhereTheMatchRuns` check the newest alert's, the player's own drain's and the silence's chips, and `ShowsWhatProductionAndResearchAreDoing` the Shipyards' chip after the counts, taking the line's click.
  - `HudTests.LaysTheBarsOverTheEntities` checks the build bar's gray.
  - Tests that count the HUD's panels set an income, so that the Ore panel holds no chip.
- The whole-HUD contrast test ([ADR-062](ADR-062-type-scale-and-contrast.md) decision 3) and the overlap test ([ADR-061](ADR-061-measured-text.md) decision 5) see the chips through `LongestContent`.

## What this forecloses

- A warning written as plain text in `WARNING_COLOR` beside an enemy figure.
- A chip in any other color, or behind anything but a warning.
