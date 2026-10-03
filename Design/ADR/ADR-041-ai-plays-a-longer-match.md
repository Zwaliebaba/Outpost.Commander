# ADR-041 — The AI attacks in bigger groups that fall back and regroup, and fortifies its base, so that two AIs play a 45–60 minute match

Status: **accepted** · 2026-10-03 · supersedes ADR-020's gate G9 value of 12 ships, and adds to its decision 8

## Context

P1 asks whether a match lasts 45–60 minutes. Beside the owner's own matches, it asks for a figure that repeats: the median of seeded matches between two AIs (Phase 1 design §2, ADR-038).

Two AIs on the 5 km map ended a match in a median of 24 minutes, over 140 seeds with task 7.2's arcs:

- **Minute 4:** each side had 12 warships and started attacking, as gate G9 has it.
- **Minute 10:** both fleets stood at about 53 ships.
- **Minute 12:** the two fleets met in one battle. The loser kept about 13 of its 65 ships and the winner 60 to 80.
- **About minute 23:** the winner ground down the base.

Both AIs opened tier 2 at 15:04, and the match was decided before tier 2 could change anything. A battle between clumps follows Lanchester's square law, so the side ahead stays ahead.

The owner asked on 2026-10-03 for AI matches to last 45–60 minutes, and decided that:

- **The AI's behavior is the lever.** Its numbers go in `Opponent.json` where they can. The game's rules change only if that is not enough, and then only after the owner is asked again.
- **The measure is the median of seeds 1 to 40.** It falls within 45–60 minutes, with the spread reported.

This goes beyond design §13, which had the AI change only as far as Phase 1's rules need.

## Decision

1. **The attack group is 20 warships, and 12 more for each tier the AI has opened past the first.** Gate G9's 12 is superseded. `attackGroupShips` is 20 and `attackGroupGrowthPerTier` is 12; the tier is the highest gateway the AI has researched.
2. **An attack that has lost 30% of the ships it set out with falls back.** It goes to the rally with a move order, out of the fight, and its ships rejoin the reserve. The reserve then waits 150 seconds before it attacks again, however many ships it has (`retreatLossShare` 0.3, `regroupSeconds` 150). The losses count against the group's size when it last grew. A lost battle so costs part of a fleet, not all of it.
3. **It plans 2 Defence Platforms round its base for each Shipyard** (`homePlatformsPerShipyard` 2). They stand toward the map's center, 260 m from the Command Station, at turns of 0 and ±0.5 and ±1 radian, then a ring 70 m further out. Each is built once the income reaches its Shipyard's share. An attack on a base that has grown so costs the attacker more than it costs the base.
4. **Nothing else in its play changes.** It still goes for production first (ADR-037), defends its structures, counters what it has seen, and follows the ore.

## Consequences

All measured in the Linux container: clang 18 at `-O2`, `--ai-matches`' code against the real server and data.

- **The target is met.**
  - Seeds 1 to 40 give a median of 53.9 minutes, the middle half from 43.0 to 74.0, every match ended.
  - Seeds 41 to 80, run as a check after the numbers were chosen, give 50.8 minutes, counting the 3 matches that had not ended at 150 minutes as the longest.
  - The switch's own seeds 1 to 10 give 56.3.
- **The spread is wide.** Of the 80 matches, 23 end within 45–60 minutes, 25 before and 32 after. 3 had not ended at 150 minutes, all of seeds 41 to 80. Why they stall has not been looked at.
- **The seat bias is gone, for now.** Player 1 wins 39 and player 2 wins 38, where under the old play one side won about two-thirds (ADR-038, ADR-039). Its cause is still not known, so it may come back.
- **Tier 3 is rare.** 78 of the 80 matches reach tier 2 and 2 reach tier 3: an AI at war spends its Ore on ships and seldom affords the 600-Ore gateway. P2 asks whether each tier changes the game, so this matters for task 13.2.
- **Two more ideas were tried and dropped**, with figures for seeds 1 to 40:
  - Falling back from a fleet stronger than the group, by hit points times damage a second: a median of 32.2 to 33.5 minutes, no better than without it.
  - Leaving the enemy's base alone until tier 2 or 3: a median of 25.7 at tier 2, and at tier 3 alone 36 of 40 matches still going at 150 minutes, since few AIs reach tier 3.
- **What it does to a human's match is not measured.** The AI attacks later and in bigger waves, falls back sooner, and fortifies, so a player who does not rush it meets fewer, larger attacks. The owner's runs are the measure.
- **`AiPlayerTests` check the behavior with the numbers set in the test**: falling back after losing half and regrouping, the group growing with the tier, and the base fortified for each Shipyard. `AiSettingsTests` checks the file's values and refuses a share of 1 or more.

## What this forecloses

- Gate G9's 12-ship attack, without a new decision.
- A match lengthened by the game's rules, such as structure hit points, research times or income, until the owner is asked.
