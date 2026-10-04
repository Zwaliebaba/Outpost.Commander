# ADR-065 — The player picks the AI's difficulty on the menu: Easy, Normal or Hard, each a settings file of its own

Status: **accepted** · 2026-10-04

## Context

The owner, playing the AI of Phase 3's milestone 24, found it too strong to beat (owner, 2026-10-04). It outproduced the player early and took nodes faster than the player could hold them, while the player did upgrade the Command Station. The AI's settings are tuned so that AI-against-AI matches meet Phase 2's S1–S4 and Phase 3's T1–T4, and those figures are measured against one settings file, `Opponent.json` ([ADR-020](ADR-020-ai-and-match-flow.md) decision 2, [ADR-041](ADR-041-ai-plays-a-longer-match.md)). Weakening that file for everyone would move the measure with it. The owner chose difficulty levels, picked on the menu, over one weaker AI (owner, 2026-10-04).

## Decision

1. **Three difficulties, each a settings file in `Opponent.json`'s format.** `Opponent.json` is Normal, the AI the AI-against-AI figures measure and the tuning moves. `OpponentEasy.json` and `OpponentHard.json` beside it are whole files, read by the same loader as strictly, so that each can be tuned on its own. `LoadPackagedAiSettings` takes the file's name, and names it in an error.
2. **Easy plays the same rules with a smaller hand.** Against the Normal file it keeps 3 Constructors not 4, plans 1 contested rig not 3, builds a Shipyard for every 15 Ore a second of income not 10, keeps 1 job queued at a Shipyard not 2, reviews its counter every 150 s not 60, gathers 30 warships before its main attack not 20, attacks without a lead only with twice that, sends no raids and claims no sector beyond its rigs. It does not cheat, and it does not hold back from what it has: it has less.
3. **Hard plays the same rules with a larger hand:** 5 Constructors, 4 contested rigs, a Shipyard for every 8 Ore a second, 3 jobs queued, a review every 45 s, raids of 3 and 2 claims.
4. **The menu offers a skirmish at each**, "Easy skirmish", "Normal skirmish" and "Hard skirmish", above Quit. The button's action carries the difficulty, `GameClient` keeps the last one asked for, and the shell gives the match's AI that difficulty's settings. The three files are read while the window is still hidden, with the first match's server ([ADR-049](ADR-049-startup-in-parallel.md)), so a bad one is reported before the screen goes full screen. `--measure` and the switches that skip the menu meet Normal.
5. **`--ai-matches` and the self-play probe keep Normal's file**, and take Easy's or Hard's by name (`--ai1`, `--ai2`), as any settings file ([ADR-063](ADR-063-self-play-probe.md)).

## Consequences

- **What Easy and Hard do is measured only against Normal**, AI against AI, on seeds 1 to 20, each with the difficulty in seat 1 for seeds 1 to 10 and seat 2 for 11 to 20: FIGURES. What they do against a human is the owner's run.
- **Normal's tuning (Phase 3 plan task 25.2) moves Normal, and Easy and Hard with it only where they share a number.** A number tuned in `Opponent.json` is left in the other two as it is until it is tuned there.
- **`AiSettingsTests.LoadsEachDifficulty`** loads all three files and checks that Easy and Hard lie either side of Normal on each number this ADR names. `HudTests.LaysOutTheMenu` checks the menu's four buttons and the difficulty each carries.
- **Neither file is packaged unchecked:** each is in the executable's project and filters as content, as `Opponent.json` is.

## What this forecloses

- An AI that is helped by the rules: more Ore, vision or hit points than the player at the same difficulty. A difficulty that needs it is a new decision.
- A difficulty chosen in the middle of a match, or remembered between runs of the game.
