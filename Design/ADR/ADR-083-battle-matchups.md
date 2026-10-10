# ADR-083 — Battle matchups are fought headless by the AI on both sides, and played live against it

Status: **accepted** · 2026-10-08

## Context

Horizon §9 measures what being present buys:
1. Pick a handful of fleet matchups.
2. Run each headlessly over many seeds, the AI's battle behavior on both sides.
3. The owner plays the same matchups live against the same AI.

The owner's win rate above the AI-against-AI baseline is what presence buys, and it is half of Phase 5's W4. The game had no way to fight a matchup at all. The owner decided on 2026-10-08 that milestone 39 builds both halves: the headless baseline, and a switch that starts a matchup in the game.

## Decision

1. **The matchups are data:** `Matchups.json`, beside the tuning data and the map, read by `LoadMatchups`.
   - Each matchup has a name, a distance between the two fleets, and two sides. Each side is a list of groups, each a count of ships of one design given by its hull, drive, weapon and optional module.
   - Every component must be the tuning data's, and each side needs a ship.
   - The file holds five matchups of about equal Ore, from the design costs:
     1. A mirror of 20 Small Mass Drivers a side.
     2. 25 Small Mass Drivers against 10 Medium Lances.
     3. 11 Medium Flak Batteries against 24 Small Mass Drivers.
     4. 5 Large Rail Cannons against 11 Medium Lances.
     5. 8 Medium Missile Racks against 10 Medium Lances.
2. **A matchup is a server's setup** (`ServerDesc::matchup`, an index of the file).
   - The server places the map without pirates, derelicts or fog of war, and in place of the bases places the matchup (`Simulation::PlaceMatchup`).
   - The two fleets stand in rows of five, 60 m apart, their front rows the matchup's distance apart and facing each other. They meet across a sector's node the seed picks, along a line the seed picks.
   - Each player's ships are of designs saved for it, its starting designs where they match. They never go back for repair, since there is nothing to repair them.
3. **A matchup ends once a side's warships are all destroyed,** the other side winning (`MatchEnding::FleetDestroyed`). It ends as a draw after `MATCHUP_SECONDS`, 600 s (`MatchEnding::TimeLimit`).
   - The limit is a setting of the simulation that a save does not hold, since a matchup is never a world.
   - The banner and the match log say how it ended: "A fleet destroyed." or "Out of time.", and `fleet` or `time`.
4. **The AI's battle behavior is `MatchupPlayer`,** in `Opponent`, on either side. Once a second its warships attack-move on the enemy ship nearest the middle of them, or the nearest enemy structure when no enemy ship is in sight. The game's own targeting picks what each ship fires at, as an attack group's does in a match ([ADR-020](ADR-020-ai-and-match-flow.md)). `AiPlayer` plays no battle without a base to plan round.
5. **The headless baseline is `OutpostServer --matchups [--seed n] [--seeds n]`:** every matchup over 40 seeds by default, the battles shared among the processor's threads. It prints how often each side won, how many were drawn, and the median length.
6. **The live half is the game's `--matchup <n>`, from 1.**
   - Every skirmish started from the menu is that matchup: the owner plays player 1, and `MatchupPlayer` plays player 2.
   - The match log writes `matchup <n>` after the match's line. `Tools/MatchLog.py` counts the owner's wins in each matchup.
7. **The protocol.** The two endings are the last of `MatchEnding`.

## Consequences

- **Tested in the Linux container.**
  - `MatchupTests`: the file's five load, and a bad component, one side, an empty side, a count of none and no matchup are refused. A matchup's fleets stand facing each other its distance apart, with no structure and no fog of war, never retreating. The AI on both sides fights the swarm against the line to an end, one side's warships all destroyed. Fleets that never close draw on the 600th second, not before. Two seeds meet in two places.
  - `MatchupPlayerTests`: the nearest enemy ship in sight, never a remembered one; a structure when no ship is in sight; once a second; nothing without a warship.
  - `HudTests` and `MatchLogTests`: the endings' words, and the matchup's line.
  - Each was run against a deliberately broken build and failed: a matchup ended only once both fleets are gone, the time run a tick long, a fleet that retreats, fog of war kept, and a remembered ship targeted. The same line through the field on every seed is not caught: the seed still picks the sector.
- **The baseline in the container**, `OutpostServer --matchups` built by clang 18 at `-O2`, seeds 1–40, side 1 first:

  | Matchup | Side 1 won | Side 2 won | Drawn | Median length |
  |---|---|---|---|---|
  | 1. Mirror | 25 | 15 | 0 | 21 s |
  | 2. Swarm against line | 0 | 40 | 0 | 31 s |
  | 3. Flak against swarm | 40 | 0 | 0 | 9 s |
  | 4. Heavy against line | 19 | 21 | 0 | 41 s |
  | 5. Missiles against line | 0 | 40 | 0 | 22 s |

  Over seeds 1–200 the mirror went 114 to 86 for side 1, 57%, with a two-sided binomial p of about 0.06. Matchups 2, 3 and 5 stayed 200 to 0, and the heavy line went 90 to 109 with one draw. Side 1's ships are made first and may act first in a tick. Whether that is worth a first strike is not looked into here.
- **Three of the five are one-sided:** 200 to 0 over 200 seeds. A fight the baseline never wins is a clean test of whether presence can turn one, and none of the five is a test of a fight between near equals bar the mirror and the heavy line.
- **Not run in play.** The owner plays the matchups live: `OutpostCommander --matchup <n>` (W4).

## What this forecloses

- A matchup with bases, more than two sides, or fog of war, without a new decision.
- A baseline fought by any AI other than `MatchupPlayer`'s battle behavior.
