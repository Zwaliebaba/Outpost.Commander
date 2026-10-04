# Outpost Commander — Phase 2 Implementation Plan

Status: **open** · Started 2026-10-03, when the owner accepted the Phase 2 design and decided its gates · Derived from [the Phase 2 design](OutpostCommander-Phase2.md)

The Phase 2 design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. [The Phase 1 plan](Archive/ImplementationPlan-Phase1.md) and [the MVP plan](Archive/ImplementationPlan.md) are closed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the Phase 2 design, then the Phase 1 and MVP design sections the task touches.** Phase 2 amends Phase 1, which amends the MVP; the later wins where they differ.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in milestone order.
3. **ADRs are edited in place** (AGENTS.md §6), and new ones are numbered from ADR-056.
4. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. The server, the AI and the match log build and run there against a stand-in for the Windows headers and the test framework, as Phase 1's did; the renderer and the window do not. Tasks marked *Owner run* stay `in review` until the owner has run them.
5. **A map without sectors plays Phase 1's rules.** Territory, the Relay's placement, suppression and domination apply only when the map names its sectors (ADR-036), so the tests built on open ground keep their meaning, and the repository map plays Phase 2.

Task numbers continue the Phase 1 plan's milestones, so that a number names one task across the plans.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 14.1 | Sectors held: the Relay, the lattice and sector ore | — | J1, J2 decided | done, not yet built on Windows |
| 14.2 | Suppression, and the Relay's sight | 14.1 | J2 decided | done, not yet built on Windows |
| 14.3 | The client shows territory, and builds Relays | 14.2 | — | in review: owner run |
| 14.4 | An attacking group keeps its lanes (Phase 1's 7.3) | — | — | done, not yet built on Windows |
| 14.5 | The AI claims its flanks | 14.1 | — | done, not yet built on Windows |
| 15.1 | Domination | 14.1 | J4 decided | done, not yet built on Windows |
| 15.2 | The client shows the tickets and how a match ended | 15.1, 14.3 | — | in review: owner run |
| 16.1 | The module slot and the Sensor Array | — | J6 decided | done, not yet built on Windows |
| 16.2 | The designer's module row | 16.1 | — | in review: owner run |
| 17.1 | Alerts | 14.3 | J5 decided | todo |
| 17.2 | Standing orders: hold a sector, patrol | 14.1 | J5 decided | todo |
| 18.1 | The AI on territory | 14.5, 15.1, 16.1, 17.2 | — | todo |
| 19.1 | The match log for Phase 2 | 15.1 | — | todo |
| 19.2 | S1–S5 | 18.1, 19.1 | — | todo |

### Milestone order

14, 15, 16, 17, 18, 19. Territory comes first because everything else is about it. Domination needs nodes to count. Modules are independent of territory, but the AI's scout needs them. The commanding tools need territory to talk about. The AI is reworked once every rule it plays by is in. Milestone 19 measures what the others built, so it comes last, and tunes against its figures.

---

## Gates

Each is an owner decision, from design §13. All were decided on 2026-10-03, when the owner accepted the design.

| Gate | Decision | Proposed in | Blocks |
|---|---|---|---|
| J1 | A cut-off sector earns half. **Decided on 2026-10-03:** as proposed. | design §6 | — |
| J2 | The Relay: 200 Ore and 40 s, 3,000 hit points and armor 10; suppressed within 400 m. **Decided on 2026-10-03:** as proposed, as starting values. | design §5 | — |
| J3 | The world moves to Phase 3. **Decided on 2026-10-03.** | design §7 | — |
| J4 | Domination: 1,000 tickets, and 30 × the node difference ÷ the map's nodes every 10 s. **Decided on 2026-10-03:** as proposed, as starting values. | design §8 | — |
| J5 | Commanding at scale. **Decided on 2026-10-03:** alerts and standing orders are Phase 2's; the strategic view is not. | design §9 | — |
| J6 | The Sensor Array: 700 m, 40 Ore, 10% slower. **Decided on 2026-10-03:** as proposed, as starting values; no other module yet. | design §10 | — |
| J7 | Salvage. **Decided on 2026-10-03:** none. | design §5 | — |

---

## Milestone 14 — Territory

### 14.1 — Sectors held: the Relay, the lattice and sector ore

- **Gate:** J1, J2, decided.
- **Goal:** design §4, §5 and §6 on the server.
- **Scope:**
  - **The data.** `Tuning.json` gains the Relay as a structure kind, with J2's numbers, and a `territory` block with the cut-off share and the suppression radius. The map's sectors are read into the simulation when it is placed.
  - **Who holds a sector.** A player holds its home sector while its Command Station stands on the node, and any other sector while its finished Relay stands on the node. A site under construction holds nothing yet, but takes the node.
  - **Building a Relay.** A Relay snaps to the node site of the sector it is ordered in, as a rig snaps to its asteroid. The server refuses one in a sector with a structure on its node, and in a sector not adjacent to one the player holds.
  - **Sector ore.** A Mining Rig may be ordered only onto an asteroid in a sector the player holds, and a rig earns only while its player holds the rig's sector. A sector cut off from its owner's home sector through the sectors it holds earns half.
  - **The snapshot** carries every sector: its rectangle, node and adjacency, who holds it, whether it is cut off, and the Relay on its node. Who holds what is known to both players, fog or not.
- **ADR:** a new one: the Relay, holding, the lattice, sector ore and what the snapshot shows.
- **Acceptance:** `TerritoryTests` cover holding by station and by Relay, a site holding nothing, the adjacency rule, the node taken, rigs refused outside held sectors, a rig earning nothing once its sector is lost, the cut-off half, and a map without sectors playing as before.
- **Verify:** CI.
- **As built:** [ADR-056](../Design/ADR/ADR-056-territory.md), decisions 1–8 and 10.
  - **Territory is worked out at the end of every tick** from what stands on the nodes (`Simulation::UpdateTerritory`), and a rig's income is its rate times its sector's share (`TerritoryShare`).
  - **New refusals:** `NotAdjacent` for a Relay and `SectorNotHeld` for a rig.
  - **Tests.** `TerritoryTests`, on the repository map, and `TuningTests` for the new object and kind. Every `GameLogicTests` suite was run in the container against a stand-in for the test framework, as in Phase 1: 207 pass, and `InProcessServerTests.RunsItsTicksOnItsOwnThread` cannot run without Windows.

### 14.2 — Suppression, and the Relay's sight

- **Gate:** J2, decided.
- **Goal:** design §5's suppression, and a held sector seen whole.
- **Scope:** a Relay is suppressed while an enemy warship is within 400 m of it and none of its owner's is. A suppressed Relay's sector earns nothing, and the Relay sees only as an unarmed structure does. A Command Station is never suppressed. A held sector that is not suppressed is in its holder's sight, the whole rectangle, under fog of war. Suppression is in the snapshot's sectors.
- **ADR:** the one 14.1 writes.
- **Acceptance:** `TerritoryTests` cover suppression starting and ending, a defender lifting it, a Constructor not suppressing, the Command Station never suppressed, no income while suppressed, the lattice and adjacency unchanged by it, and `FogTests` the sector's sight.
- **Verify:** CI.
- **As built:** [ADR-056](../Design/ADR/ADR-056-territory.md), decisions 6 and 9. The sector's sight is tested in `TerritoryTests.AHeldSectorIsSeenWhole`, beside the rest of territory, rather than in `FogTests`.

### 14.3 — The client shows territory, and builds Relays

- **Goal:** the player can see and claim territory.
- **Scope:** the Relay in the HUD's build buttons, its ghost snapping to the node site of the sector under the cursor and showing red where the server would refuse it; the Relay drawn with a model from `Models.json`; the minimap tinted by who holds each sector, hatched where suppressed; and a line in the HUD naming how many nodes each side holds.
- **ADR:** the one 14.1 writes.
- **Acceptance:** `PlacementTests` for the Relay's ghost; `HudTests` for the build button, the minimap's sectors and the line.
- **Verify:** CI; **owner run.**
- **As built:** [ADR-056](../Design/ADR/ADR-056-territory.md) decision 11.
  - **The fog** clears the sectors the player holds too (`FogOfWar::Update`), as the server's sight does; `FogOfWarTests.SeesAHeldSectorWhole`.
  - **The Relay borrows the Research Lab's model**, brighter, until the owner adds one.
  - **Built and run in the container** against DirectXMath and stand-ins for the Windows headers: every `GameAppTests` suite but `GlyphAtlasTests`, whose fonts are DirectWrite's. 190 pass; the 12 that fail there read the shipped meshes and textures by Windows paths. `GameClient`, which draws, is CI's first build, and the owner's run is its first look.

### 14.4 — An attacking group keeps its lanes

- **Goal:** Phase 1's task 7.3: a group given an Attack order on one enemy passes an obstacle side by side, as a group given a Move or an attack-move order does after task 9.7 (ADR-047).
- **Scope:** the group paths again together, with its band, when its target has moved, at most once a second, instead of each ship alone.
- **ADR:** ADR-047, edited in place.
- **Acceptance:** a `MovementTests` case as `AGroupPassesAnObstacleSideBySide`, with an Attack order on a moving target.
- **Verify:** CI.
- **As built:** [ADR-047](../Design/ADR/ADR-047-a-fight-seen-whole.md) decision 3, edited in place.
  - **An Attack order lays lanes** at its ships' places across a grid of the group's size, and **`ChaseTargets` paths the ships of one player that path again to one target in the same tick as a group**, along a fresh route in a band; a lone ship paths as before. The formation's order and lanes are shared with a move's (`SortForFormation`, `FormationColumns`, `FormationAcross`), which a move gives bit for bit as before.
  - **`MovementTests.AnAttackingGroupPassesAnObstacleSideBySide`**: 25 ships attack a target moving away beyond a 200 m field. The group is at most 2.5 times as long as wide, 1.5 while it passes the field; with the old pathing grafted back in, it was 6.2. The bound is looser than a move's 2.0 because the ships close on one point past the field.

### 14.5 — The AI claims its flanks

- **Goal:** the AI still plays once rigs need held sectors: it builds a Relay in each sector whose asteroids its plan wants, before the rigs.
- **Scope:** the minimum to keep the AI's economy: Relays on its plan's sectors, nearest first, each when one of its sectors is adjacent. 18.1 makes it play territory.
- **Acceptance:** `AiPlayerTests` pass on the repository map with territory.
- **Verify:** CI; the AI-against-AI matches in the container.
- **As built:** [ADR-056](../Design/ADR/ADR-056-territory.md) decision 12 and ADR-020 decision 5. `AiPlayerTests` pass unchanged, `ReachesTierThree` included. Seeds 1 to 10 all end, at a median of 45:48.

---

## Milestone 15 — Domination

### 15.1 — Domination

- **Gate:** J4, decided.
- **Goal:** design §8.
- **Scope:** each player starts with the tuning data's tickets. Every 10 seconds, the player who holds fewer nodes loses 30 × the difference ÷ the map's nodes; a suppressed Relay counts for its owner, and an unheld node for no one. A player whose tickets reach zero loses. The match records how it ended: the base or domination. Both are in the snapshot.
- **ADR:** a new one, with the tickets' units.
- **Acceptance:** `DominationTests`: a drain of the design's size and interval, none at equal nodes, a lead of one draining a side in 50 minutes, the end at zero, and the end reason.
- **Verify:** CI.
- **As built:** [ADR-057](../Design/ADR/ADR-057-domination.md).
  - **Tickets are kept in shares, as many to a ticket as the map has nodes**, so 30 × the difference ÷ 9 is whole and a lead of one ends a match in exactly 50 minutes; the snapshot shows whole tickets, rounded up.
  - **`MatchEnding`** says how a match ended, in the simulation and the snapshot; `Simulation::EndMatch` ends it for either reason.
  - **Tests.** `DominationTests`, on a fixture shared with `TerritoryTests` (`TerritoryMatch.h`); 212 `GameLogicTests` pass in the container.
  - **Seeds 1 to 10** all end, at a median of 41:50; three end by domination.

### 15.2 — The client shows the tickets and how a match ended

- **Scope:** both sides' tickets in the HUD beside the nodes; the banner names a domination.
- **Acceptance:** `HudTests`.
- **Verify:** CI; **owner run.**
- **As built:** [ADR-057](../Design/ADR/ADR-057-domination.md) decision 8. The territory panel moved under the research line's place, where its second row fits; `HudTests.ShowsTheTickets` and `DescribesHowTheMatchEnded`.

---

## Milestone 16 — Modules

### 16.1 — The module slot and the Sensor Array

- **Gate:** J6, decided.
- **Goal:** design §10 on the server and in the protocol.
- **Scope:** `Tuning.json` gains a `modules` list, with the Sensor Array's sight, cost and speed factor. A design gains an optional module: `DesignComponents`, `DesignStats`, `DesignView`, `EntityView` and `SaveDesignCommand` carry it, a module's cost adds to the design's, its speed factor slows it, and its sight replaces the weapon's when it is further. Starting designs have none. A design's abbreviation gains the module's initials. The balance check leaves modules out.
- **ADR:** a new one.
- **Acceptance:** `DesignTests` and `SaveDesignTests` for a design with a module, `FogTests` for its sight, `TuningTests` for the list.
- **Verify:** CI.
- **As built:** [ADR-058](../Design/ADR/ADR-058-modules.md), decisions 1–6 and 8. No research unlocks a module. The ship's sight is tested in `SaveDesignTests.SavesADesignWithASensorArray`, with the rest of the module, rather than in `FogTests`. 214 `GameLogicTests` pass in the container.

### 16.2 — The designer's module row

- **Scope:** a fourth row of cards under the weapon, with an empty card for no module; the bars show sight.
- **Acceptance:** `DesignerTests`, `HudTests`.
- **Verify:** CI; **owner run.**
- **As built:** [ADR-058](../Design/ADR/ADR-058-modules.md) decision 7. A module's design is named by its module's initials where its whole name would pass 32 characters, "Small+Ion+Mass Driver+SA".

---

## Milestone 17 — Commanding at this scale

### 17.1 — Alerts

- **Gate:** J5, decided.
- **Scope:** design §9's alerts, made by the client from its snapshots: one of its Relays suppressed or attacked, one of its rigs lost, an enemy group entering a sector it holds. Each is a short message in the HUD for a few seconds and a mark on the minimap; one key moves the camera to the newest.
- **ADR:** a new one, with the alerts' rules and the key.
- **Acceptance:** `AlertsTests`, `HudTests`.
- **Verify:** CI; **owner run.**

### 17.2 — Standing orders: hold a sector, patrol

- **Gate:** J5, decided.
- **Scope:** two new orders. *Hold a sector*: the group answers any enemy it sees in the sector, and goes back to the sector's node once none is left. *Patrol*: the group attack-moves between two points until given another order. Both are kept by the server, as orders are, and given from keys.
- **ADR:** the one 17.1 writes.
- **Acceptance:** `StandingOrderTests`; `PlayerControlsTests` for the keys.
- **Verify:** CI; **owner run.**

---

## Milestone 18 — The AI

### 18.1 — The AI on territory

- **Goal:** design §12.
- **Scope:** the AI builds Relays on its flanks early; sends a Sensor Array scout in the first two minutes; claims nodes adjacent to its territory and fortifies the ones on its front with a Defence Platform; raids a sector it sees weakly held with a few fast ships and pulls back when it loses; holds its sectors with standing orders; and masses for a main attack only with a lead in nodes or when the enemy's front has thinned.
- **ADR:** ADR-020 and ADR-041, edited in place.
- **Acceptance:** `AiPlayerTests`.
- **Verify:** CI; the AI-against-AI matches.

---

## Milestone 19 — Measuring Phase 2

### 19.1 — The match log for Phase 2

- **Goal:** the figures S1–S4 need (design §2).
- **Scope:** the log records the first shot, each engagement (ships of both sides firing in one sector), each change of a sector's holder, each side's tickets every 30 seconds, and how the match ended. `Tools/MatchLog.py` reports S1–S4 for a match and for the ten AI-against-AI matches.
- **ADR:** ADR-038, edited in place.
- **Acceptance:** `MatchLogTests`.
- **Verify:** CI.

### 19.2 — S1–S5

- **Goal:** answer design §2's questions and record them there. A failed answer is still a result.
- **Scope:** the AI-against-AI run, tuned against S1–S4 where the AI's own settings can reach them; the owner's matches and judgement for S5.
- **Verify:** **owner run.**
