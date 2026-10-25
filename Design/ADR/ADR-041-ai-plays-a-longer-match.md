# ADR-041 — The AI attacks in bigger groups that fall back and regroup, and fortifies its base

Status: **accepted** · 2026-10-07
Owns: ai-attack-group, ai-fall-back, ai-base-platforms, ai-attack-node-lead

## Context

An AI that attacks with the first few warships it gathers meets the other side's fleet early, in one battle. A battle between clumps follows Lanchester's square law, so the side ahead stays ahead: the loser keeps a small part of its fleet, the winner grinds down its base, and the match is decided before research can change anything. A group that attacks too early ends a match fast and loses its fleet whole.

A match against the AI is meant to last, and the owner decided how it is lengthened (owner, 2026-10-03):

- **The AI's behavior is the lever.** Its numbers go in `Opponent.json` where they can, and each difficulty's file sets its own ([ADR-065](ADR-065-ai-difficulty.md)).
- **The game's rules change only if that is not enough**, and then only after the owner is asked again.

## Decision

1. **The attack group is `attackGroupShips` warships, and `attackGroupGrowthPerTier` more for each tier the AI has opened past the first.** The tier is the highest its Research Lab's level has opened, the snapshot's `researchTier`. Under a fleet cap that has room for fewer ships of its production design's hull, beside its scouts and its raid, the group is `attackCapShare` of that room instead, and never less than one ship ([ADR-071](ADR-071-fleet-cap.md)). Each difficulty's file sets its own ([ADR-065](ADR-065-ai-difficulty.md)).
2. **An attack that has lost `retreatLossShare` of the ships it set out with falls back.** It goes to the rally with a move order, out of the fight, and its ships rejoin the reserve. The reserve then waits `regroupSeconds` before it attacks again, however many ships it has; the wait is the owner's (owner, 2026-10-04). The losses count against the group's size when it last grew, so a lost battle costs part of a fleet, not all of it. A ship that turns for home below its retreat threshold counts as one lost (owner, 2026-10-07); how the AI treats a retreating ship is [ADR-076](ADR-076-ai-plays-phase-4.md)'s.
3. **It plans `homePlatformsPerShipyard` Defence Platforms round its base for each Shipyard.** They stand toward the map's center, 260 m from the Command Station, at turns of 0 and ±0.5 and ±1 radian, then in rings 70 m further out. Each is built once the income reaches its Shipyard's share. An attack on a base that has grown so costs the attacker more than it costs the base.
4. *(retired)*
5. **On a map with territory the main attack waits for a lead in nodes.** The reserve joins the attack group once it holds the group's size and the AI holds `attackNodeLead` nodes more than the enemy, or once the reserve holds `attackWithoutLeadShare` times the group's size, or the whole of the room the fleet cap leaves it if that is less. A free node counts for neither side. Both players see who holds each node ([ADR-056](ADR-056-territory.md) decision 10), so the AI counts them from its snapshot.
6. **The attack group goes for production first.** It ranks the enemy structures it sees or remembers: Shipyards, then the Command Station, then the rest, and never the pirates'. It attack-moves on the nearest of the best rank to the group's center, each level a structure has above the first counting as `attackLevelMeters` nearer, and keeps that target until it is gone, or until a structure of a better rank comes to light. When it knows none, it goes across the map's center to look.
7. **The attack group closes on its target structure.** A ship of the attack group within `CLOSE_IN_METERS`, 500 m, of its target structure, plus the structure's radius, is ordered to attack it, once each, because an attack-move ends at each ship's place in the group's formation, which round a large structure can stand out of the ship's range. A raid closes on a Relay the same way ([ADR-020](ADR-020-ai-and-match-flow.md) decision 13).

## Consequences

- **A player meets few, large attacks.** The AI waits for a big group, falls back once it has lost a share of it, and fortifies its base, so a player who does not rush it meets few, large attacks, and an attacker beaten back keeps part of its fleet.
- **`AiPlayerTests` check the behavior with the numbers set in the test**: falling back after losing half and regrouping, the group growing with the tier, the base fortified for each Shipyard, the attack waiting for a lead in nodes or a larger reserve, production attacked first, and a Shipyard's level counted nearer. `AiSettingsTests` checks the file's values and refuses a fall-back share of 1 or more.

## What this forecloses

- A main attack sent before its group has gathered, without a new decision.
- A match lengthened by the game's rules, such as structure hit points, research times or income, until the owner is asked.
