# ADR-076 — The AI clears pirates by detachment, salvages with one crew, and builds a Repair Bay at its front

Status: **accepted** · 2026-10-07

## Context

Phase 4 design §12 asks the AI to play by every Phase 4 rule as a player would, and §2's U1–U5 are what Normal is tuned against. Until this milestone it skipped the sectors pirates guard ([ADR-073](ADR-073-pirates.md)), left derelicts alone ([ADR-074](ADR-074-salvage.md)) and built no Repair Bay ([ADR-075](ADR-075-repair-and-retreat.md)). What §12 already had before this milestone stays as it is: the AI plays the fleet cap and upgrades its Command Station when the cap holds back its production ([ADR-020](ADR-020-ai-and-match-flow.md) decision 15), places Shipyards only in sectors it holds, keeps its counters, and its ships retreat at the default, a quarter.

The owner decided on 2026-10-07:

- **Pirates are cleared by a detachment** of the reserve that outweighs the outpost in Ore by a margin in `Opponent.json`: 1.5 for Normal, 2.0 for Easy, 1.2 for Hard. A camp is cleared once it is next to the AI's territory; a stronghold only when no free sector is left to claim and the AI is not at its node cap.
- **Salvage is one Constructor at a time**, to derelicts in or next to its territory, once its home rigs stand.
- **One Repair Bay at the front**, built anew when the front moves.
- **Its scout explores its own half first**, for the first ten minutes, after the first measurement below showed the opening's first fight came before any salvage.

What the design leaves open: how the AI learns an outpost's strength under fog of war, which outpost it takes first, when a detachment gives up, how a salvage trip ends, and where a Bay stands.

## Decision

1. **What it knows of an outpost is what it has seen.** For each guarded sector it keeps the most Ore it has seen there, pirate ships at their design's cost and Defence Platforms at the platform's, and the most platforms: more than one is a stronghold. A sector no longer guarded is forgotten. An outpost it has never seen is never a target.
2. **A detachment** is chosen from the reserve, the ships nearest the outpost first, until their Ore reaches `pirateMargin` times the outpost's. Its target is the nearest to the base of the outposts it has seen next to its territory: any camp, and a stronghold only when no unguarded free sector next to its territory is left and it is not at its node cap. With too few ships in the reserve it waits.
   - The detachment attack-moves on the outpost's node, ordered again every 10 seconds, until the sector is no longer guarded; its ships then rejoin the reserve.
   - It falls back to the rally once it has lost the raids' share of its ships, `raidLossShare`, and the next waits the raids' interval, as a raid does (ADR-020 decision 13). A retreating ship leaves it as it leaves a raid (ADR-075 decision 9).
   - One detachment at a time, and it is chosen after the raid, so a raid comes first.
3. **`pirateMargin`** is required in each difficulty's settings and must be positive.
4. **The scout explores its own half for `scoutOwnHalfSeconds`**, 600 in each difficulty's settings, before it goes round the enemy's flanks (ADR-020 decision 13).
   - It moves, rather than attack-moves, from node to node of the sectors nearer its base than the enemy's, its home aside, the nearest first.
   - It looks at a guarded sector from a lookout 650 m from its node, toward its base, outside the 600 m in which the pirates attack and within its Sensor Array's 700 m sight. A lookout is turned up to 0.7 radians either way to keep it clear of asteroids and structures, and is dropped if none is clear. The scout goes on from a lookout once within 30 m of it, from a node within 150 m.
   - Without the lookouts the AI never saw a camp next to its land, so it never sent a detachment: the first version did not.
5. **Salvage:** once it has as many Mining Rigs as `homeAsteroids`, one idle Constructor, the nearest, is sent to the nearest derelict to its base that it knows in a sector it holds, or in a free sector next to one it holds; never in a sector the pirates guard. It is sent ahead of the contested rigs of its build plan.
   - The trip ends when the derelict is gone. A trip not done in 300 seconds, or whose Constructor is lost or turns for home, is dropped, and a derelict given up for time is never chosen again.
   - The Constructor on a trip is no one's crew, so its builds go on without it.
6. **A Repair Bay at the front:** once it has a built Shipyard, it plans a Bay 220 m behind its front sector's node, toward its base, past the Relay's Defence Platforms. The front is its sector nearest the rally that borders an enemy's, else the rally's own sector while it holds it, the sector its reserve holds.
   - Each sector gets a Bay once: when the front moves to a sector without one, a new one is planned there. Bays already built stay.
   - The Bay is built as any slot of its plan is, and a slot in a sector it no longer holds waits.

## Consequences

- **The AI plays Phase 4's opening** (`AiPlayerTests.PlaysPhaseFoursOpening`): in a half-hour match on seed 3 it salvages two derelicts or more with one Constructor at a time, sends a detachment at an outpost, and builds a Repair Bay. `SendsAScoutRoundTheEnemysFlanks` checks the scout's own half and that it keeps out of the pirates' reach; `LeavesASectorWithPiratesAlone` still holds before a detachment; `AiSettingsTests.LoadsThePiratesPlay` loads the new settings.
- **AI against AI over seeds 1–40, measured in the Linux container** (design §2), with Normal on both sides and each match played for up to three hours:
  - **U1 is met.** The players' first shot at each other comes at a median of 11:38, between minute 8 and minute 20 in 39 of the 40. Before it the median side has salvaged 3 derelicts and fought 2 pirate outposts, and both sides have salvaged 2 or more in all 40.
  - **The outposts fall.** A side sends a median of 4 detachments, the first at a median of 8:48. The median match ends with 4 of the 10 pirate structures standing and every derelict salvaged.
  - **U2 is met at its peak and missed at minute 20.** The median side has 21 warships at most, and 1 side of 80 goes over 40. At minute 20 it has 20, where U2 asks for fewer, holding 8,159 Ore against 4,636 after milestone 32: the salvage fills its cap sooner.
  - **U3:** a side turns a median of 69 warships for home, 55% of the ships it turns for home are repaired whole, against 40% after milestone 32, and 17 warships fight again, a quarter of those turned for home. The owner accepted it as measured (2026-10-07).
  - **U4 is met:** a median of 7 engagements before minute 40, in 4.5 sectors.
  - **U5:** all 40 end, all by domination, at a median of 1:39:42. Recorded rather than tuned (owner, 2026-10-07).
  - ENGINE_33
- **Two versions were set aside.** The first, whose scout went straight for the enemy's flanks, met the enemy at a median of 6:55 over the 40 seeds, with a median of one derelict salvaged before it, and so missed U1. The second explored its own half but skipped the guarded sectors, and so never saw the camp next to its land: in `PlaysPhaseFoursOpening`'s match it sent no detachment in half an hour. It was not measured over the 40 seeds.

## What this forecloses

- An AI that knows an outpost it has not seen.
- More than one detachment, or more than one salvage trip, at a time.
- A Repair Bay anywhere but behind its front.
