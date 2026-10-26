# ADR-076 — The AI clears pirates by detachment, salvages with one crew, builds a Repair Bay at its front, and leaves its retreats alone

Status: **accepted** · 2026-10-07
Owns: ai-pirates, ai-salvage, ai-repair-bay, ai-retreat, ai-scout-own-half

## Context

The AI plays by every rule a player meets: the pirate outposts that guard sectors ([ADR-073](ADR-073-pirates.md)), the derelicts that pay Ore and research ([ADR-074](ADR-074-salvage.md)), and the Repair Bay and retreat ([ADR-075](ADR-075-repair-and-retreat.md)). It sees what a player sees, so under fog of war it knows an outpost only from what its ships have seen of it.

The owner decided on 2026-10-07:

- **Pirates are cleared by a detachment** of the reserve that outweighs the outpost in Ore by a margin, each difficulty's `pirateMargin` in its settings. A camp is cleared once it is next to the AI's territory; a stronghold only when no free sector is left to claim and the AI is not at its node cap.
- **Salvage is one Constructor at a time**, to derelicts in or next to its territory, once its home rigs stand.
- **One Repair Bay at the front**, built anew when the front moves.
- **Its scout explores its own half first**, for the first `scoutOwnHalfSeconds` of the match.
- **A retreating ship leaves its group**, so that a retreat counts toward the group's fall-back.

The AI must therefore learn an outpost's strength under fog of war, choose which outpost it takes first and when a detachment gives up, end a salvage trip, place a Bay, and keep its orders from ending its ships' retreats.

## Decision

1. **What it knows of an outpost is what it has seen.** For each guarded sector it keeps the most Ore it has seen there, pirate ships at their design's cost and Defence Platforms at the platform's, and the most platforms: more than one is a stronghold. A sector that stops being guarded is forgotten. An outpost it has never seen is never a target.
2. **A detachment** is chosen from the reserve, the ships nearest the outpost first, until their Ore reaches `pirateMargin` times the outpost's. Its target is the nearest to the base of the outposts it has seen next to its territory: any camp, and a stronghold only when no unguarded free sector next to its territory is left and it is not at its node cap. With too few ships in the reserve it waits.
   - The detachment attack-moves on the outpost's node, ordered again every 10 seconds, until the sector is not guarded; its ships then rejoin the reserve.
   - It falls back to the rally once it has lost the raids' share of its ships, `raidLossShare`, and the next waits the raids' interval, as a raid does (ADR-020 decision 13). A retreating ship leaves it as it leaves a raid (decision 9).
   - One detachment at a time, and it is chosen after the raid, so a raid comes first.
3. **`pirateMargin`** is required in each difficulty's settings and must be positive.
4. **The scout explores its own half for the first `scoutOwnHalfSeconds` of the match or the universe,** counted by its snapshot's tick from the simulation's first, not from when its AI was made, before it goes round the enemy's flanks (ADR-020 decision 13). An AI empire made afresh in a universe after that time sends its scout round the flanks at once.
   - It moves, rather than attack-moves, from node to node of the sectors nearer its base than the enemy's, its home aside, the nearest first.
   - It looks at a guarded sector from a lookout 650 m from its node toward its base, or nearer when its base is nearer, outside the reach in which the pirates attack and within its Sensor Array's sight. A lookout is turned up to 0.7 radians either way to keep it clear of asteroids and structures, and is dropped if none is clear. The scout goes on from a lookout once within 30 m of it, from a node within 150 m.
5. **Salvage:** once it has as many Mining Rigs as `homeAsteroids`, one idle Constructor, the nearest, is sent to the nearest derelict to its base that it knows in a sector it holds, or in a free sector next to one it holds; never in a sector the pirates guard. It is sent ahead of the contested rigs of its build plan.
   - The trip ends when the derelict is gone. A trip not done in 300 seconds, or whose Constructor is lost or turns for home, is dropped, and a derelict given up for time is never chosen again.
   - The Constructor on a trip is no one's crew, so its builds go on without it.
6. **A Repair Bay at the front:** once it has a built Shipyard, it plans a Bay 220 m behind its front sector's node, toward its base, past the Relay's Defence Platforms. The front is its sector nearest the rally that borders an enemy's, else the rally's own sector while it holds it, the sector its reserve holds.
   - Each sector gets a Bay once: when the front moves to a sector without one, a new one is planned there. Bays already built stay.
   - The Bay is built as any slot of its plan is, and a slot in a sector it has lost waits.
7. **The AI leaves pirates alone but for a detachment sent to clear an outpost** (decision 2). It neither plans nor claims a Relay or a rig in a guarded sector, and a claim that is guarded is dropped and another made in its place. It counts no pirate warship as the enemy's fleet to answer, and its main attack picks no pirate structure to attack. Its fleets are not routed round the outposts, so a group whose way passes within an outpost's guard of its node fights the outpost there.
8. **A derelict is in the AI's way as it is in a player's.** The AI plans no Relay on a node a wreck covers, and a derelict blocks its structures' places as it blocks the player's ghost ([ADR-074](ADR-074-salvage.md) decision 1); how the AI salvages is decision 5's.
9. **The AI and retreat.** Its designs keep the default retreat. It takes a retreating ship out of every order it gives, since any order would end the retreat. A retreating ship leaves its attack group, raid or detachment, as a lost one does, and rejoins the reserve once it is whole; [ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 2 counts it toward the group's fall-back. A retreating Constructor leaves its crew, and is not idle until it is whole, so the AI neither waits on it nor gives it work. Its Repair Bay at the front is decision 6.

## Consequences

- **The AI plays an opening with pirates, derelicts and a Repair Bay** (`AiPlayerTests.PlaysPhaseFoursOpening`): in a half-hour match on seed 3 it orders salvage with one Constructor at a time, twice or more, salvages a derelict, sends a detachment at an outpost, and builds a Repair Bay. `SendsAScoutRoundTheEnemysFlanks` checks the scout's own half and that it keeps out of the pirates' reach; `LeavesASectorWithPiratesAlone` holds before a detachment; `AiSettingsTests.LoadsThePiratesPlay` loads the settings.
- **The lookouts are what let the AI clear a camp.** A scout that looks at a guarded sector from outside the guard sees the camp next to its land, and an outpost seen is one a detachment can take (decision 1).

## What this forecloses

- An AI that knows an outpost it has not seen.
- More than one detachment, or more than one salvage trip, at a time.
- A Repair Bay anywhere but behind its front.
