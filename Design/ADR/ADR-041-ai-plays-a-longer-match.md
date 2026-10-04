# ADR-041 — The AI attacks in bigger groups that fall back and regroup, and fortifies its base, so that two AIs play a 45–60 minute match

Status: **accepted** · 2026-10-03

## Context

P1 asks whether a match lasts 45–60 minutes. Beside the owner's own matches, it asks for a figure that repeats: the median of seeded matches between two AIs (Phase 1 design §2, ADR-038).

Two AIs on the 5 km map ended a match in a median of 24 minutes, over 140 seeds with task 7.2's arcs:

- **Minute 4:** each side had 12 warships and started attacking, as gate G9 had it.
- **Minute 10:** both fleets stood at about 53 ships.
- **Minute 12:** the two fleets met in one battle. The loser kept about 13 of its 65 ships and the winner 60 to 80.
- **About minute 23:** the winner ground down the base.

Both AIs opened tier 2 at 15:04, and the match was decided before tier 2 could change anything. A battle between clumps follows Lanchester's square law, so the side ahead stays ahead.

The owner asked on 2026-10-03 for AI matches to last 45–60 minutes, and decided that:

- **The AI's behavior is the lever.** Its numbers go in `Opponent.json` where they can. The game's rules change only if that is not enough, and then only after the owner is asked again.
- **The measure is the median of seeds 1 to 40.** It falls within 45–60 minutes, with the spread reported.

This goes beyond design §13, which had the AI change only as far as Phase 1's rules need.

## Decision

1. **The attack group is 25 warships, and 12 more for each tier the AI has opened past the first.** `attackGroupShips` is 25, Phase 3's task 25.2's value over gate G9's 20, and `attackGroupGrowthPerTier` is 12; the tier is the highest its Research Lab's level has opened, the snapshot's `researchTier` (Phase 3 design §6).
2. **An attack that has lost 15% of the ships it set out with falls back.** It goes to the rally with a move order, out of the fight, and its ships rejoin the reserve. The reserve then waits 240 seconds before it attacks again, however many ships it has (`retreatLossShare` 0.15, `regroupSeconds` 240). The losses count against the group's size when it last grew. A lost battle so costs part of a fleet, not all of it. Phase 2's task 19.2 set both against S4 on the map with territory, at 10%, and Phase 3's task 25.2 the share against T1 and T4 at 15%; the owner kept the 240 seconds over Phase 1's 120 on 2026-10-04; on Phase 1's map they were 30% and 120 seconds ([ADR-047](ADR-047-a-fight-seen-whole.md) decision 6).
3. **It plans 2 Defence Platforms round its base for each Shipyard** (`homePlatformsPerShipyard` 2). They stand toward the map's center, 260 m from the Command Station, at turns of 0 and ±0.5 and ±1 radian, then a ring 70 m further out. Each is built once the income reaches its Shipyard's share. An attack on a base that has grown so costs the attacker more than it costs the base.
4. **Nothing else in its play changes here.** It still goes for production first (ADR-037), defends its structures, counters what it has seen, and follows the ore. On a map with territory it also plays for territory ([ADR-020](ADR-020-ai-and-match-flow.md) decision 13).
5. **On a map with territory the main attack waits for a lead in nodes** (Phase 2 design §12, plan task 18.1). The reserve joins the attack group once it holds the group's size and the AI holds `attackNodeLead`, 1, node more than the enemy, or once the reserve holds `attackWithoutLeadShare`, 1.5, times the group's size. A free node counts for neither side. Both players see who holds each node ([ADR-056](ADR-056-territory.md) decision 10), so the AI counts them from its snapshot.

## Consequences

All measured in the Linux container: clang 18 at `-O2`, `--ai-matches`' code against the real server and data. The first bullets are Phase 1's, with a 30% fall-back, `regroupSeconds` at 150 and before a group kept lanes round obstacles; [ADR-047](ADR-047-a-fight-seen-whole.md) measures 120 seconds with lanes. The last ones are Phase 2's, on the map with territory.

- **Phase 1's target was met.**
  - Seeds 1 to 40 give a median of 53.9 minutes, the middle half from 43.0 to 74.0, every match ended.
  - Seeds 41 to 80, run as a check after the numbers were chosen, give 50.8 minutes, counting the 3 matches that had not ended at 150 minutes as the longest.
  - The switch's own seeds 1 to 10 give 56.3.
- **The spread is wide.** Of the 80 matches, 23 end within 45–60 minutes, 25 before and 32 after. 3 had not ended at 150 minutes, all of seeds 41 to 80. Why they stall has not been looked at.
- **The seat bias is gone, for now.** Player 1 wins 39 and player 2 wins 38, where under the old play one side won about two-thirds (ADR-038, ADR-039). Its cause is still not known, so it may come back.
- **Tier 3 is rare.** 78 of the 80 matches reach tier 2 and 2 reach tier 3: an AI at war spends its Ore on ships and seldom affords the 600-Ore gateway. Phase 3 replaced the gateway with the Lab's level 3, at the same 600 Ore ([ADR-064](ADR-064-structure-upgrades.md) decision 10). P2 asks whether each tier changes the game, so this matters for task 13.2.
- **Two more ideas were tried and dropped**, with figures for seeds 1 to 40:
  - Falling back from a fleet stronger than the group, by hit points times damage a second: a median of 32.2 to 33.5 minutes, no better than without it.
  - Leaving the enemy's base alone until tier 2 or 3: a median of 25.7 at tier 2, and at tier 3 alone 36 of 40 matches still going at 150 minutes, since few AIs reach tier 3.
- **Territory's play shortens the match again.** Measured as above, on the repository's map with territory, with the match log of [ADR-038](ADR-038-phase-one-match-log.md) and seeds 1 to 20 unless said:
  - Before task 18.1, when the AI built Relays only where the ore took it, seeds 1 to 10 ended at a median of 41:50 ([ADR-057](ADR-057-domination.md)).
  - Task 18.1's first cut claimed every free sector next to its territory, one at a time, and raided with 4 ships every 90 seconds. Seeds 1 to 10 ended at a median of 23:36. Followed with a probe, seed 1's winner held 7 nodes, earned 150 Ore/s and had 25,000 Ore banked by 16:00.
  - The starting values, one claim and raids of 2 every 180 seconds, give a median of 20:23, from 11:11 to 35:49, none within 45–60 minutes, every match ended and all by production. Every match has a shot by minute 5 and five engagements in three sectors before minute 20. Player 2 won 14.
  - Measured before ties were mirrored ([ADR-020](ADR-020-ai-and-match-flow.md) decision 13), when player 1 won 16 of the 20 with the starting values: without claims or raids the median was 40:34; with one claim and no raids, 26:32; with raids and no claim, 28:44; and waiting for a lead of 2 nodes or twice the group, 11:31, every match by production.
  - These numbers keep what the design asks of the AI and are where 19.2's tuning starts.
- **Task 19.2 tuned the fall-back against S4** (Phase 2 design §2), with territory's play at its starting values. Seeds 1 to 40 are the measure, as for Phase 1's P1 above; seeds 1 to 10 are the ten that `--ai-matches` plays, which S4 names.

  | Falls back after losing | Regroups for | Seeds 1–40 | Within 45–60 | Seeds 1–10 |
  |---|---|---|---|---|
  | 30% | 120 s | 20:23 over seeds 1–20 | none of 20 | — |
  | 10% | 180 s | 45:05 | 17 | 42:34 |
  | 15% | 240 s | 44:30 | 19 | 45:15 |
  | **10%** | **240 s** | **47:55** | **25** | **44:50** |

  - With the chosen numbers every match of the 40 ends, from 25:48 to 1:23:00. Every one has a shot by minute 5 and five engagements in three sectors before minute 20, a median of 16 in 6 sectors. 27 end by domination and 13 by production. Player 1 wins 16 and player 2 24. A side's peak is a median of 175.5 warships, from 72 to 347.
  - Seeds 1 to 10 alone give 44:50, 10 seconds short of S4's band. The numbers were not tuned to those ten: 15% and 240 s gives 45:15 there, but 44:30 over the 40.
- **Task 24.1's claim past the flanks undoes 19.2's tuning, for now.** Until 24.1 the AI spent its one claim on a flank its rigs were taking it to anyway, so it held three nodes and played Phase 2 at that. Claiming beyond them, as Phase 3 design §8 asks, it upgrades its Command Station to level 2 and holds four. Seeds 1 to 10, measured as above against plan task 23.1's tree: a median of 30:07 against 52:25, from 22:02 to 48:50; 1 within 45–60 minutes against 6; 2 by domination and 8 by production against 8 and 2; five engagements in three sectors before minute 20 in 5 of 10 against 10; tier 3 in 1 of 20 seats against 8. With the claim alone taken out it is 52:50, 8 by domination and tier 3 in 7, and with the levels left out of what it attacks the figures do not move, so the claim is the whole of it. The owner kept the claim and left the retuning to plan task 25.2, against T1–T3 on seeds 1 to 40 (owner, 2026-10-04).
- **Task 25.2 tuned the AI again, against Phase 3's T1 to T4** (Phase 3 design §2), measured as above with the match log of [ADR-038](ADR-038-phase-one-match-log.md). Each candidate played seeds 1 to 20; the chosen one played 1 to 40. From 24.1's numbers, of which seeds 1 to 40 gave a median of 29:48, 1 within 45–60 minutes, 36 by production, T3 in none and tier 3 in 3 of 80 seats:

  | Changed from 24.1's numbers | Median | Within 45–60 | Domination / production | T2 | T3, all three at level 3 | T4 median |
  |---|---|---|---|---|---|---|
  | none (seeds 1–20) | 29:48 | 1 | 3 / 17 | 3:52 | 0 | 6 |
  | lead 2 | 29:16 | 2 | 2 / 18 | 3:52 | 0 | 6 |
  | 2 claims | 1:00:25 | 5 | 15 / 5 | 8:48 | 8 | 0 |
  | 2 claims, lead 2 | 51:50 | 9 | 18 / 2 | 8:48 | 9 | 0 |
  | 2 claims, group 25 | 55:10 | 13 | 16 / 4 | 9:25 | 7 | 0 |
  | 2 claims, group 25, falls back at 20% | 45:48 | 9 | 13 / 7 | 9:25 | 4 | 2.5 |
  | 2 claims, group 25, falls back at 30% | 41:15 | 6 | 9 / 11 | 9:25 | 4 | 4 |
  | **2 claims, group 25, falls back at 15%** | **50:56** | **13** | **13 / 7** | **9:25** | **5** | **4.5** |
  | the same, seeds 1–40 | 52:45 | 28 of 40 | 29 / 11 | 9:28 | 11 of 40 | 4 |

  - **The second claim is what brings the match back**: it gives the AI a fifth node to want, so it upgrades its Command Station to level 3 in every match, and the nodes are contested to the end, as Phase 3 design §7 argues. Falling back later lets the attack reach a base, which T4 needs, and costs length and domination endings.
  - **Over the 40**, every match ends, S1 is met in all 40 and S2 in 39, and player 1 wins 28: a seat bias like the one ADR-038 recorded, not chased here.
  - **Two AI changes were tried and dropped**, on seeds 1 to 20 with 2 claims and a group of 25. Upgrading the Command Station whenever it is at its cap with a claim left, after its Shipyards, changed nothing, since a structure waiting for Ore always held it back. Doing so ahead of its plan started level 2 at 1:33, starved its economy, and gave a median of 41:30, T3 in none and tier 3 in 3 of 40 seats, for a stall of 6:09.
- **What it does to a human's match is not measured.** The AI attacks later and in bigger waves, falls back sooner, and fortifies, so a player who does not rush it meets fewer, larger attacks. The owner's runs are the measure.
- **`AiPlayerTests` check the behavior with the numbers set in the test**: falling back after losing half and regrouping, the group growing with the tier, the base fortified for each Shipyard, and the attack waiting for a lead in nodes or a larger reserve. `AiSettingsTests` checks the file's values and refuses a share of 1 or more.

## What this forecloses

- Gate G9's 12-ship attack, without a new decision.
- A match lengthened by the game's rules, such as structure hit points, research times or income, until the owner is asked.
