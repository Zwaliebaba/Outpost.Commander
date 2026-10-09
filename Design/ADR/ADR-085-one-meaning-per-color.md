# ADR-085 — One meaning per color: a warning is a chip, and build progress is no side's

Status: **accepted** · 2026-10-09

## Context

The second interface review of 2026-10-08 ([interface plan 2](../../GameDesign/ImplementationPlan-Interface2.md), finding 4) found the player's own warnings in the enemy's color. `WARNING_COLOR` stands at an sRGB hue of 17.5° and `ENEMY_COLOR` at 14.6°. In the review's first screenshot, "3 IDLE" stood directly over the enemy's "2" and "642" in what read as the same salmon. Three things were written in the warning's color as plain text: a status line's IDLE ([ADR-066](ADR-066-production-status-on-the-hud.md)), an income of nothing ([ADR-047](ADR-047-a-fight-seen-whole.md) decision 5) and the newest alert ([ADR-059](ADR-059-alerts-and-standing-orders.md) decision 2).

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
2. **Build progress is a neutral light gray** (task UI2.2). The bar beyond a structure's health bar that shows the share built is (0.5, 0.52, 0.55) linear on every side's structures, where it was a blue one shade from the player's, so an enemy's site no longer reads as the player's. It is `GameClient`'s `BUILD_BAR_COLOR`, named once. The HUD's own progress fills (`BAR_FILL_COLOR`) stay blue, since they stand on the player's own buttons and queues.

## Consequences

- **The build bar is `GameClient`'s,** which needs D3D12, so it has no test; CI builds it, and the owner's run sees it on an enemy's site and an own one.

- **Tests:** `HudTests.MarksEachWarningWithAChip` (the three, each at 4.5:1 or more on its tag; an older alert and the run before the chip plain; the enemy's figure plain in its color), `WarnsOfNoIncome`, `ListsTheAlerts`, and `ShowsWhatProductionAndResearchAreDoing` with the chip after the counts and taking the line's click. Tests that count the HUD's panels set an income, so that the Ore panel holds no chip.
- The whole-HUD contrast test (task 14.2) and the overlap test (task 14.1) see the chips through `LongestContent`.
- Run in the Linux container against a stand-in for the Windows headers, with substitute fonts. Not yet seen on screen.

## What this forecloses

- A warning written as plain text in `WARNING_COLOR` beside an enemy figure.
- A chip in any other color, or behind anything but a warning.
