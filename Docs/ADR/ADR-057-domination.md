# ADR-057 — Domination: tickets drain by the nodes behind, whatever the map, and a match records how it ended

Status: **accepted** · 2026-10-09
Owns: domination, tickets-drain, match-ending-kind, territory-panel

## Context

A match has a second way to win beside the loss of production ([ADR-037](ADR-037-losing-all-production.md) decision 1): by tickets and nodes (owner, 2026-10-03). A player who holds fewer nodes than another loses tickets, and a player out of tickets loses. A lead of one node has to last as long on any map, whatever its number of nodes (owner, 2026-10-07). And the player has to be told who the drain takes from, how fast and when they run out, not left to work it out (owner, 2026-10-08).

So the rules have to say what a ticket is, when the drain falls, what a snapshot shows of it, and how a client tells one ending from another.

## Decision

1. **The numbers are data** (owner, 2026-10-03): `Tuning.json`'s `territory` object has `tickets`, `drainIntervalSeconds` and `drainTicketsPerNodeDifference`. The loader requires them, the counts at least 1 and the interval positive.
2. **Tickets are whole, and a drain takes the tuning data's tickets for each node a player is behind, whatever the map's nodes** (owner, 2026-10-07), so a lead of one node lasts as long on any map. Tickets are part of each player's state, so they replay with the rest (ADR-009).
3. **The drain falls at the end of every tick that completes an interval**, after territory is worked out (ADR-056 decision 8). Each player loses for every node it is behind the player that holds the most. A suppressed Relay counts for its owner, and a free node for no one, as territory says, and so does a node the pirates guard ([ADR-073](ADR-073-pirates.md) decision 6).
4. **Tickets start when the bases are placed**, on a map with territory, and stop once the match is over. A world has none, and no domination ([ADR-084](ADR-084-world-without-an-end.md)).
5. **A player out of tickets loses.** The match ends once, with the simulation running on, as [ADR-020](ADR-020-ai-and-match-flow.md) decision 9 has it; `Simulation::EndMatch` ends it for either reason. A loss of production ([ADR-037](ADR-037-losing-all-production.md) decision 1) is decided mid-tick, after the fight, and domination at the tick's end, so a player who loses both in one tick loses by production.
6. **A match records how it ended** in `Simulation::Ending` and `Snapshot::ending`: `MatchEnding::LostProduction` or `MatchEnding::Domination`. A battle matchup's endings are [ADR-083](ADR-083-battle-matchups.md) decision 7's.
7. **Every snapshot carries every player's tickets**, the tickets each started with, and the drain's two numbers from the tuning data, `drainIntervalSeconds` and `drainTicketsPerNodeDifference`, which the client tells the drain by (owner, 2026-10-08). Both players see both players' tickets, as both see the territory (ADR-056 decision 10).
8. **The client shows them.** The territory panel stands under the status panel's place ([ADR-066](ADR-066-production-status-on-the-hud.md)), below the Ore, in three rows:
   - **"Nodes"**, with the Command Station's cap of the map's nodes beside it in the labels' color, "cap 10 of 25", or "of 25" without a cap, and the player's nodes against the enemy's, "3 : 2", the player's in its color and the enemy's in theirs.
   - **"Tickets"**, the same way.
   - **Who the drain takes from, how fast, and when they run out**: "Enemy -20 a minute · out in 32:06", in the text's color, or "You -60 a minute · out in 12:09" as a warning chip ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1) when the player is behind, or "No drain" level on nodes. The rate is the drain a node difference takes, times the difference, a minute. The time is the drains their tickets last, rounded up, times the interval; the next drain may be up to an interval away, which the row does not try to show. The minus is a hyphen, since the glyph atlas has no minus sign.
   - A world has no tickets ([ADR-084](ADR-084-world-without-an-end.md)), and so neither of the last two rows. A domination's banner says "By domination." before the match's length.

## Consequences

- **Tests.** `DominationTests` covers the starting tickets with the drain's numbers in the snapshot, no drain at equal nodes, the first drain at the end of its interval and not before, a suppressed Relay counting, a lead of one ending the match by domination in exactly 50 minutes, the outcome standing after it, and no tickets without territory. `HudTests.ShowsTheTickets` covers the rows, and the drain ahead, 642 tickets a node behind out in 32:06, level, and behind three at 729 tickets out in 12:09, as a chip; `ShowsTheTerritory` the nodes' row with its cap; and `WireFormatTests` the drain's numbers on the wire. `HudTests` also covers the banner. The fixture for a match on the repository map, `TerritoryMatch`, is shared with `TerritoryTests`.

## What this forecloses

- Tickets hidden by fog of war, and a drain that is not in whole shares, without a new decision.
