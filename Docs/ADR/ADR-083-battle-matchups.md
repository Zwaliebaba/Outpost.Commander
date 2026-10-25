# ADR-083 — Battle matchups are fought headless by the AI on both sides, and played live against it

Status: **accepted** · 2026-10-08
Owns: battle-matchups, matchup-setup, matchup-ending, matchup-player, matchup-baseline

## Context

What being present buys a player is measured against the AI (owner, 2026-10-08):

1. Pick a handful of fleet matchups.
2. Run each headlessly over many seeds, the AI's battle behavior on both sides.
3. The owner plays the same matchups live against the same AI.

The owner's win rate above the AI-against-AI baseline is what presence buys. That needs a fight between two fleets with no bases, fought headless for the baseline and started in the game for the live half (owner, 2026-10-08).

## Decision

1. **The matchups are data:** `Matchups.json`, beside the tuning data and the map, read by `LoadMatchups`.
   - Each matchup has a name, a distance between the two fleets, and two sides. Each side is a list of groups, each a count of ships of one design given by its hull, drive, weapon and optional module.
   - Every component must be the tuning data's, and each side needs a ship.
   - The file's matchups are of about equal Ore a side, by the design costs; the Catalogue lists them.
2. **A matchup is a server's setup** (`ServerDesc::matchup`, an index of the file).
   - The server places the map without pirates, derelicts or fog of war, and in place of the bases places the matchup (`Simulation::PlaceMatchup`).
   - The two fleets stand in rows of five, 60 m apart, their front rows the matchup's distance apart and facing each other. They meet across a sector's node the seed picks, along a line the seed picks.
   - Each player's ships are of designs saved for it, its starting designs where they match. They never go back for repair, since there is nothing to repair them.
3. **A matchup ends once a side's warships are all destroyed,** the other side winning (`MatchEnding::FleetDestroyed`). It ends as a draw after `MATCHUP_SECONDS`, 600 s (`MatchEnding::TimeLimit`).
   - The limit is a setting of the simulation that a save does not hold, since a matchup is never a world.
   - The banner says how it ended, "A fleet destroyed." or "Out of time.", before the match's length ([ADR-020](ADR-020-ai-and-match-flow.md) decision 9); the match log's `ending` record is [ADR-038](ADR-038-phase-one-match-log.md) decision 1's.
4. **The AI's battle behavior is `MatchupPlayer`,** in `Opponent`, on either side. Once a second its warships attack-move on the enemy ship nearest the middle of them, or the nearest enemy structure when no enemy ship is in sight. The game's own targeting picks what each ship fires at, as an attack group's does in a match ([ADR-020](ADR-020-ai-and-match-flow.md)). `AiPlayer` plays no battle without a base to plan round.
5. **The headless baseline is `OutpostServer --matchups [--seed n] [--seeds n]`:** every matchup over 40 seeds by default, the battles shared among the processor's threads. It prints how often each side won, how many were drawn and how many of those ran out of time, and the median length.
6. **The live half is the game's `--matchup <n>`, from 1.**
   - Every skirmish started from the menu is that matchup: the owner plays player 1, and `MatchupPlayer` plays player 2.
   - The match log names the matchup ([ADR-038](ADR-038-phase-one-match-log.md) decision 1), and `Tools/MatchLog.py` counts the owner's wins in each matchup.
7. **The protocol.** The two endings are the last of `MatchEnding`.

## Consequences

- **Tests.**
  - `MatchupTests`: the file's matchups load, and a bad component, one side, an empty side, a count of none and no matchup are refused. A matchup's fleets stand facing each other its distance apart, with no structure and no fog of war, never retreating. The AI on both sides fights the swarm against the line to an end, one side's warships all destroyed. Fleets that never close draw on the 600th second, not before. Two seeds meet in two places.
  - `MatchupPlayerTests`: the nearest enemy ship in sight, never a remembered one; a structure when no ship is in sight; once a second; nothing without a warship.
  - `HudTests` and `MatchLogTests`: the endings' words, and the matchup's line.
  - No test catches the same line through the field on every seed, since the seed still picks the sector.
- **Side 1's ships are made first,** and may act first in a tick.
- **A matchup the baseline never wins is a clean test** of whether presence can turn one; a mirror tests a fight between near equals.

## What this forecloses

- A matchup with bases, more than two sides, or fog of war, without a new decision.
- A baseline fought by any AI other than `MatchupPlayer`'s battle behavior.
