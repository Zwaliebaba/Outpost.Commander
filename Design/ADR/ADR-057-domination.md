# ADR-057 — Domination: tickets drain by the nodes behind, whatever the map, and a match records how it ended

Status: **accepted** · 2026-10-03

## Context

Phase 2 design §8 adds a second way to win beside Phase 1's: each player starts with 1,000 tickets, and every 10 seconds the player who holds fewer nodes loses 30 × the difference ÷ the number of nodes on the map. A player whose tickets reach zero loses. The owner decided the rule on 2026-10-03, and gate J4 took the numbers as starting values. The design leaves open the tickets' units, when the drain falls, what the snapshot shows, and how a client tells the two endings apart.

On the 10 km map's 25 nodes (Phase 4 design §6) that rule made a lead of one node take 139 minutes, against 50 on the 5 km map's nine, and every AI match ended by it at a median of two hours (Phase 4 plan task 28.4). The owner decided on 2026-10-07 that a lead of one node takes 50 minutes whatever the map: the drain no longer divides by the map's nodes.

## Decision

1. **The numbers are data**: `Tuning.json`'s `territory` object has `tickets` 1,000, `drainIntervalSeconds` 3 and `drainTicketsPerNodeDifference` 1. The loader requires them, the counts at least 1 and the interval positive.
2. **Tickets are whole, and a drain takes the tuning data's tickets for each node a player is behind**, whatever the map's nodes. A lead of one node takes exactly 1,000 drains of 3 seconds, 50 minutes, on any map, and a lead of three about 17. Tickets are part of each player's state, so they replay with the rest (ADR-009).
3. **The drain falls at the end of every tick that completes an interval**, after territory is worked out (ADR-056 decision 8). With 20 ticks a second, the first falls at the end of the 60th tick. Each player loses for every node it is behind the player that holds the most. With two players that is the design's rule; with more it reads the same way. A suppressed Relay counts for its owner, and a free node for no one, as territory says, and so does a node the pirates guard ([ADR-073](ADR-073-pirates.md)).
4. **Tickets start when the bases are placed**, on a map with territory, and stop once the match is over. A world has none, and no domination ([ADR-081](ADR-081-world-without-an-end.md)).
5. **A player out of tickets loses.** The match ends as Phase 1's does: once, with the world running on. `Simulation::EndMatch` ends it for either reason. A loss of production is still decided mid-tick, after the fight, and domination at the tick's end, so a player who loses both in one tick loses by production.
6. **The match records how it ended**: `MatchEnding::LostProduction` or `MatchEnding::Domination`, in `Simulation::Ending` and `Snapshot::ending`.
7. **Every snapshot carries every player's tickets**, and the tickets each started with. Both players see both, as both see the territory (ADR-056 decision 10).
8. **The client shows them.** The territory panel moves under the status panel's place ([ADR-066](ADR-066-production-status-on-the-hud.md)), below the Ore, and grows a second row: "Tickets", the player's in its color and the enemy's in theirs, as the nodes are. A domination's banner says "By domination." before the match's length.

## Consequences

- **Tests.** `DominationTests` covers the starting tickets, no drain at equal nodes, the first drain at 3 seconds and not before, a suppressed Relay counting, a lead of one ending the match by domination in exactly 50 minutes, the outcome standing after it, and no tickets without territory. `HudTests` covers the row and the banner. The fixture for a match on the repository map, `TerritoryMatch`, is shared with `TerritoryTests`.
- **Domination already ends some AI matches.** Measured in the Linux container as ADR-056 measured them, seeds 1 to 10 all end, at a median of 41:50, from 31:03 to 58:38. Three of the ten, which ran 50 to 87 minutes before, now end by domination at 42:00, 43:00 and 51:20, each won by player 2. The AI does not yet play for nodes (Phase 2 plan, milestone 18): the leads come from following the ore. In seed 2, followed with a probe that printed the territory every five minutes, both sides held their home and flanks until about 25 minutes. Then player 2's home rigs ran dry, and the next asteroids it chose stood in the center and the northwest, where it built Relays. Player 1 held three nodes to its five from then on and lost by domination at 51:20.
- **Not built or run on Windows here.** CI is the first build. The panel and the banner are the owner's run.

## What this forecloses

- Tickets hidden by fog of war, and a drain that is not in whole shares, without a new decision.
