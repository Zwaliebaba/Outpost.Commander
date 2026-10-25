# ADR-065 — The player picks the AI's difficulty on the menu: Easy, Normal or Hard, each a settings file of its own

Status: **accepted** · 2026-10-04
Owns: ai-difficulty, difficulty-settings-files, skirmish-difficulty-buttons

## Context

The owner found the AI too strong to beat (owner, 2026-10-04). The AI's settings are tuned by AI-against-AI matches, and those figures are measured against one settings file, `Opponent.json` ([ADR-020](ADR-020-ai-and-match-flow.md) decision 2, [ADR-041](ADR-041-ai-plays-a-longer-match.md)). Weakening that file for everyone would move the measure with it. The owner chose difficulty levels, picked on the menu, over one weaker AI (owner, 2026-10-04).

## Decision

1. **Three difficulties, each a settings file in `Opponent.json`'s format.** `Opponent.json` is Normal, the AI the AI-against-AI figures measure and the tuning moves. `OpponentEasy.json` and `OpponentHard.json` beside it are whole files, read by the same loader as strictly, so that each can be tuned on its own. `LoadPackagedAiSettings` takes the file's name, and names it in an error.
2. **Easy plays the same rules with a smaller hand.** Against the Normal file it keeps fewer Constructors (`constructors`), plans fewer contested rigs (`contestedAsteroids`), builds a Shipyard for more income (`incomePerShipyardOrePerSecond`), keeps fewer jobs queued at a Shipyard (`shipyardQueueJobs`), reviews its counter less often (`reviewIntervalSeconds`), gathers more warships before its main attack, more again without a lead, and a larger share of the room the fleet cap leaves it (`attackGroupShips`, `attackWithoutLeadShare`, `attackCapShare`), falls back after fewer losses (`retreatLossShare`), wants a larger margin over a pirate outpost (`pirateMargin`), sends no raids (`raidShips`) and claims no sector beyond its rigs (`claimSectors`). Its other numbers are its file's own, as [the Catalogue](../Reference/Catalogue.md#opponent) shows them beside Normal's. It does not cheat, and it does not hold back from what it has: it has less.
3. **Hard plays the same rules and fights harder for territory.** Against the Normal file it keeps more Constructors (`constructors`), reviews its counter more often (`reviewIntervalSeconds`), attacks with a smaller group (`attackGroupShips`), falls back after fewer losses (`retreatLossShare`), wants a smaller margin over a pirate outpost (`pirateMargin`) and raids with more ships (`raidShips`). It claims as many sectors beyond its rigs as Normal (`claimSectors`). Its other numbers are its file's own, as [the Catalogue](../Reference/Catalogue.md#opponent) shows them beside Normal's.
4. **The menu offers a skirmish at each**, "Easy skirmish", "Normal skirmish" and "Hard skirmish", above Quit. The button's action carries the difficulty, `GameClient` keeps the last one asked for, and the shell gives the match's AI that difficulty's settings. The three files are read while the window is still hidden, with the first match's server ([ADR-049](ADR-049-startup-in-parallel.md)), so a bad one is reported before the screen goes full screen. `--measure` and the switches that skip the menu meet Normal.
5. **`--ai-matches` and the self-play probe keep Normal's file**, and take Easy's or Hard's by name (`--ai1`, `--ai2`), as any settings file ([ADR-063](ADR-063-self-play-probe.md)).

## Consequences

- **What Easy and Hard do is measured against Normal**, AI against AI, with `--ai-matches` (ADR-063). The measurements are the BalanceLog's.
- **Tuning Normal's file moves Normal alone.** A number tuned in `Opponent.json` is left in the other two as it is until it is tuned there.
- **`AiSettingsTests.LoadsEachDifficulty`** loads all three files and checks that Easy and Hard differ from Normal as decisions 2 and 3 say on each number they name but `attackCapShare`, `retreatLossShare`, `pirateMargin` and Hard's `attackGroupShips`, which it does not check. `HudTests.LaysOutTheMenu` checks the menu's four buttons and the difficulty each carries.
- **Neither file is packaged unchecked:** each is in the executable's project and filters as content, as `Opponent.json` is.

## What this forecloses

- An AI that is helped by the rules: more Ore, vision or hit points than the player at the same difficulty. A difficulty that needs it is a new decision.
- A difficulty chosen in the middle of a match, or remembered between runs of the game.
