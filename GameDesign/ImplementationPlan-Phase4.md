# Outpost Commander — Phase 4 Implementation Plan

Status: **open** · Started 2026-10-06, when the owner accepted [the Phase 4 design](OutpostCommander-Phase4.md) with every gate decided as proposed and L11 waived · Derived from the Phase 4 design

The Phase 4 design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. [The Phase 3 plan](Archive/ImplementationPlan-Phase3.md) is closed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the Phase 4 design, then the earlier design sections the task touches.** Phase 4 amends Phase 3, which amends Phase 2, Phase 1 and the MVP. Where they differ, the later document wins.
2. **Take the lowest-numbered task whose status is `todo` and whose dependencies are `done`.** One PR per milestone (owner, 2026-09-30), in milestone order.
3. **ADRs are edited in place** (AGENTS.md §6), and new ones take the next free number, ADR-071 onward.
4. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. The server, the AI and `GameLogicTests` build and run there against a stand-in for the Windows headers, the test framework and MsQuic, as Phases 2 and 3 did. The client and the renderer do not. Tasks marked *Owner run* stay `in review` until the owner has run them.
5. **The AI keeps playing at every milestone.** A rule that would stop the AI's plan comes with the least AI change that keeps `AiPlayerTests` passing, as Phase 3's did. Milestone 33 then makes the AI play Phase 4 well.
6. **The gates are the design's, L1 to L11** (design §16), all decided on 2026-10-06.

Task numbers continue the Phase 3 plan's, whose last was 26.1, so that a number names one task across the game's phase plans. The Interface plan's milestones 14 to 17 are a separate series.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 27.1 | Ships and the Defence Platform cost three times as much | — | L1 | built, in milestone 27's PR |
| 27.2 | The fleet cap, on the server | — | L2 | built, in milestone 27's PR |
| 27.3 | The client shows the fleet and its cap | 27.2 | L2 | built, in milestone 27's PR; awaiting CI and the owner's run |
| 27.4 | The AI plays within the cap | 27.1, 27.2 | L2 | built, in milestone 27's PR |
| 27.5 | Milestone 27 measured on today's map | 27.4 | — | measured in the container |
| 27.6 | The rigs earn about a third less | 27.5 | owner, 2026-10-07 | built, in milestone 27's PR |
| 28.0 | A destroyed structure is taken out of the path graphs in place | — | owner, 2026-10-07 | built, in milestone 28's PR |
| 28.1 | The client and the engine follow the map's size | — | L6 | built, in milestone 28's PR; awaiting CI and the owner's run |
| 28.2 | The 10 km map and its node caps | 28.1 | L3, L4 | built, in milestone 28's PR |
| 28.3 | Production follows territory | — | L5 | built, in milestone 28's PR; the ghost's test awaits CI |
| 28.4 | The engine measured at 10 km (U7) | 28.2 | — | measured in the container; U7 awaits the owner's run |
| 29.0 | A lead of one node takes 50 minutes on any map | 28.4 | owner, 2026-10-07 | built, in milestone 29's PR |
| 29.1 | Placement from the seed | 28.2 | L10 | built, in milestone 29's PR |
| 29.2 | The fog works out only what changed | 28.1 | owner, 2026-10-07 | built, in milestone 29's PR; awaiting CI and the owner's run |
| 30.1 | Pirates: the neutral owner and its outposts | 29.1 | L7 | todo |
| 30.2 | The client draws pirates | 30.1 | L7 | todo |
| 31.1 | Derelicts and salvage | 29.1 | L8 | todo |
| 31.2 | The client shows derelicts and salvage | 31.1 | L8 | todo |
| 32.1 | The Repair Bay and retreat, on the server | — | L9 | todo |
| 32.2 | The client sets retreat and draws the Repair Bay | 32.1 | L9 | todo |
| 33.1 | The AI plays Phase 4 | 28.3, 30.1, 31.1, 32.1 | — | todo |
| 34.1 | The match log for Phase 4 | 30.1, 31.1, 32.1 | — | todo |
| 34.2 | U1–U8 | 33.1, 34.1 | — | todo |

### Milestone order

27, 28, 29, 30, 31, 32, 33, 34. Milestone 27 changes the economy and the cap on today's 5 km map (owner, 2026-10-06), so their effect is measured before the map changes, as Phase 3 kept its map fixed. Milestone 28 is the engine's largest risk, so it comes before the content that fills the map. Placement from the seed comes before pirates and derelicts, since both are placed by it. The Repair Bay stands on its own and can move earlier if a milestone stalls. The AI is reworked once every rule it plays by is in, and milestone 34 measures what the others built.

---

## Milestone 27 — Fewer ships

### 27.1 — Ships and the Defence Platform cost three times as much

- **Gate:** L1, decided.
- **Scope:** in `Tuning.json`, every hull's, drive's, weapon's and module's `cost` and every hull's `buildSeconds` times three, and the Defence Platform's `cost` times three. Nothing else changes (design §4). The Q2 check's budgets in `BalanceCheck` and `Tools/BattleModel.py` move by the same factor, so its verdicts stand.
- **ADR:** ADR-014 and ADR-016 are edited in place where they quote the numbers.
- **Acceptance:** every `GameLogicTests` suite passes with the new numbers; tests that quote a cost read it from the data where they can.
- **Verify:** CI; the container's run of `GameLogicTests`; `BalanceCheckTests` at the new budgets.
- **As built:** ADR-014 and ADR-016 quote none of the numbers; ADR-058 quoted the Sensor Array's 40 Ore and is edited in place. The tests that quote a design's cost or a hull's build time quote the new ones, since they check the data. `BalanceCheckTests`' recorded counters hold at the tripled budgets. The full check (`OUTPOST_BALANCE_FULL`) was not run: a battle's Ore and its ships' costs both triple, so each buys the same ships, and only the per-battle seeds, which hash the budget, change. `GameAppTests`' designer fixtures hold the old numbers as a snapshot of their own and do not read `Tuning.json`, so they are unchanged.

### 27.2 — The fleet cap, on the server

- **Gate:** L2, decided.
- **Scope:**
  - **The data.** Each hull gains `commandPoints`: Small 1, Medium 2, Large 4. The Command Station gains `commandPoints` at level 1 and at each level: 12, 20, 30, 40 and 50. Both are optional, and data without them has no cap, as data without `nodes` has no node cap.
  - **The rule.** A player's command points are those of its warships and of the warship jobs its Shipyards have started. A warship job that would take a player past its cap waits at the front of its queue, as a job waiting for Ore does, and is neither paid for nor started. A player without a Command Station has level 1's cap (design §5). The cap never removes a ship.
  - **The snapshot** carries the player's command points and its cap, each hull's points, and each Command Station level's cap. The protocol's version goes up.
- **ADR:** a new one, ADR-071: the fleet cap.
- **Acceptance:** `FleetCapTests` cover a job waiting at the cap and starting once a ship is lost, jobs under way counted, a Constructor never counted, the cap rising with the station's level, level 1's cap without a station, and no cap without the data. `TuningTests` cover the new members and their refusals. `WireFormatTests` cover the new fields.
- **Verify:** CI; the container's run of `GameLogicTests`.
- **As built:** [ADR-071](../Design/ADR/ADR-071-fleet-cap.md); ADR-016 edited in place. `FleetCapTests` has a sixth test, that a Medium's two points do not fit where a Small's one would. The protocol's version is 7.

### 27.3 — The client shows the fleet and its cap

- **Scope:** the status panel's Shipyards line ends with "fleet 18 / 30" and counts the Shipyards waiting for the cap; a Shipyard's selection panel and its production window's queue say when its front job waits for the cap rather than for Ore; the Command Station's next level names its cap (design §13).
- **Acceptance:** `HudTests` for the fleet line, the waiting card and the upgrade's text.
- **Verify:** CI; **owner run**.
- **As built:** `HudTests.ShowsTheFleetAgainstItsCap`. The fleet is at the end of the Shipyards' status line rather than a panel of its own, so that it needs no room the HUD does not already keep (design §13 edited to match). **Not built or run in the container**, which has no Windows headers for the client: CI is its first build, and the owner's run its first look.

### 27.4 — The AI plays within the cap

- **Scope:** the AI upgrades its Command Station when its production waits on the cap, and its attack group and raids are sized by what its cap allows, not by a count of ships it can no longer reach. The three difficulty files carry any new numbers (ADR-065).
- **ADR:** ADR-020 and ADR-041 edited in place.
- **Acceptance:** `AiPlayerTests` pass, with a test that the AI upgrades its station when capped.
- **Verify:** CI; AI-against-AI matches in the container.
- **As built:** ADR-020 decision 15 and ADR-041 decisions 1 and 5 edited in place. The station is upgraded a round of the Shipyards ahead of the cap, a ship of the production design from each, since a level takes most of a minute and waiting for the cap itself stalled production for each one. `attackCapShare` is a new setting, 0.8 for Normal and Hard and 1 for Easy. Raids are unchanged: two Small ships fit any cap. The tests of the attack group's size and of node-driven upgrades run without the cap, and three economy tests are re-timed: each rig away from home waits for a platform of 450 Ore. `AiPlayerTests.AttacksOnceItsReserveFillsTheCap` and `UpgradesItsStationWhenTheCapHoldsItBack` are new.

### 27.5 — Milestone 27 measured on today's map

- **Scope:** AI-against-AI matches over seeds 1–40 on the 5 km map: the most warships each side has, its warships at minute 20, the match's length and how it ended. Recorded here and in design §2 as an interim answer to U2 and U5, before the map changes.
- **Verify:** the container's figures, stated as such: floats replay only on the same build (ADR-009).
- **As built:** recorded in design §2. Run with a driver of the container's own, which plays two Normal AIs on the in-process server as `--ai-matches` does and counts each side's warships from its snapshots, on this milestone and on `main` before it: 4 minutes for `main`'s 40 matches and 2 for milestone 27's, on four threads. The median side peaks at 24.5 warships, against 193; every match ends, all 40 by domination, none by production; and the median side holds 8,823 Ore at minute 20. Those last two are for the owner before milestone 28.

### 27.6 — The rigs earn about a third less

- **Asked:** after 27.5, the owner chose to lower the income so that Ore runs short before the fleet cap binds, in milestone 27's PR, and to measure the sieges that no longer succeed again on the 10 km map (owner, 2026-10-07; design §3).
- **Scope:** in `Tuning.json`, the rigs' rates from 5, 6, 8 and 10 Ore a second to 3.5, 4, 5.5 and 6.5. Nothing else changes.
- **ADR:** ADR-016, ADR-017 and ADR-036 edited in place where they quote the rates, and ADR-020 where it times the AI's scout.
- **Verify:** CI; the container's run of `GameLogicTests`; seeds 1–40 again, AI against AI.
- **As built:** two cuts were measured over seeds 1–40 before choosing, about a third and half; the third is built, and both are in design §2. With it no side goes over 40 warships, and the median side holds 733 Ore at minute 20 against 8,823. The tests that quote a rate quote the new ones. The AI's economy tests are re-timed again: its scout sets out at about 2:30, inside three minutes rather than two; it has 2 Shipyards at 28.14 Ore/s rather than 4 at 41.25; and it has six rigs with ore at fourteen minutes rather than twelve. `BuildsTheAnswerToTheEnemysFleet` allows its scout in the queue beside its answer, since the scout may now still be waiting there when the first warship joins it.

---

## Milestone 28 — The 10 km map

### 28.0 — A destroyed structure is taken out of the path graphs in place

- **Asked:** before the map changed, AI-against-AI matches on a first 10 km map put the 99th percentile tick at about 20 ms, most of it whole builds of the path graphs a destroyed structure dropped (design §11). The owner chose to take the obstacle out of each graph in place, measured, and to path by sector only if that is not enough (owner, 2026-10-07). It is one PR with the rest of milestone 28.
- **Scope:** `Pathfinder` keeps each graph across any change of the obstacles that keeps the map's edge, and brings it up to date when it is next needed: reduced by the obstacles taken away, then extended over those added, to the very graph a whole build makes, to the bit. Nothing a match does changes.
- **ADR:** ADR-054 and ADR-010 edited in place.
- **Acceptance:** `PathfinderTests.TakenAwayObstaclesReduceTheGraphsToWhatAWholeBuildMakes`; every other suite unchanged.
- **Verify:** CI; the container's run of `GameLogicTests`; the same matches played before and after.
- **As built:** recorded in ADR-054 decision 7. On the 10 km map, seeds 1 to 4 for up to 120 minutes, every match plays exactly as before, and the 99th percentile tick falls from 19–22 ms to about 2 ms, the slowest from 88–140 ms to 10–16 ms. A graph of about 2,060 corners loses a structure in under 2 ms, against 22 ms to build whole. Pathing by sector is not needed for this; U7 on the development machine (28.4) decides.

### 28.1 — The client and the engine follow the map's size

- **Gate:** L6, decided.
- **Scope:** the fog's texture holds the 10 km map's 500 cells a side; the ground's grid and the camera's focus reach 5,000 m from the center, and the far plane 8,000 m past the focus; the widest view is 3,000 m (design §6, §11). The minimap and the fog's grid already take the map's size from the snapshot.
- **ADR:** ADR-012, ADR-036 decision 5 and ADR-052 edited in place.
- **Verify:** CI; **owner run**, at the widest view over the 10 km map.
- **As built:** the fog texture is 512 × 512. **Not built or run in the container**: CI is its first build, and the owner's run its first look.

### 28.2 — The 10 km map and its node caps

- **Gates:** L3 and L4, decided.
- **Scope:** `Map.json` is the 10 km map of design §6, written by `Tools/MakeMap.py`; the Command Station's `nodes` are 4, 7, 10, 13 and 16. The tests that stand on the 5 km map's sectors and positions move to the 10 km map's.
- **ADR:** ADR-036 decision 4, ADR-057 and ADR-064 decision 11 edited in place, and ADR-056 where it quotes the panel.
- **Acceptance:** `MapTests` hold the new map's shape, sectors and symmetry; every suite passes on it.
- **Verify:** CI; the container's run of `GameLogicTests`.
- **As built:** sectors are named by column and row, A1 to E5, A1 player 1's home. Each player has 3 home, 7 near, 8 contested and 2 rich asteroids nearer its start, 459,000 Ore in all, and two fields on every border leave three passages through it. `python Tools/MakeMap.py --check` says whether the committed map is what the script writes; CI does not run it. Domination's rule is unchanged, so a lead of one node of 25 takes 834 drains, 139 minutes, against 50 on nine (design §6 foresaw it; 28.4 measures what it does to a match). The stress scene's rally and structures stood a share of the way from each start to the middle, which on 10 km put the fleets 7.6 km apart and out of reach of each other in its minute; they now stand 1,650 m and 1,100 m from the middle, where they stood on the 5 km map. Tests moved: `TerritoryTests`, `DominationTests`, `StandingOrderTests` and `AiPlayerTests` to the new sectors and positions; `UpgradesItsStationBeforeItsFirstClaim` becomes `…BeforeItsSecondClaim`, since level 1's four nodes now take a claim beyond the flanks; and three AI economy tests are re-timed, its rigs away from home being further out: two Shipyards by nine minutes rather than seven, and six rigs with ore by sixteen minutes rather than fourteen.

### 28.3 — Production follows territory

- **Gate:** L5, decided.
- **Scope:** on a map with sectors, a Shipyard is built only in a sector its player holds: the server refuses it as `SectorNotHeld`, the ghost is red, and the AI looks for its Shipyards' places only in sectors it holds. The Repair Bay takes the rule with milestone 32, which builds it.
- **ADR:** ADR-056 decisions 5 and 12 edited in place.
- **Acceptance:** `TerritoryTests.AShipyardNeedsAHeldSector`, `PlacementTests.AShipyardNeedsAHeldSector` and `AiPlayerTests.BuildsItsShipyardsInItsTerritory`.
- **Verify:** CI; the container's run of `GameLogicTests`.
- **As built:** a Shipyard in a sector its player later loses keeps working; only its placement is ruled. The server's test fails with the rule taken out. `PlacementTests` is `GameAppTests`', which the container does not build: its calls were checked against `PlaceGhost` in a program of the container's own, and CI is its first run. The placing hint does not say why a Shipyard's ghost is red, as it does not for a rig's; the design asks for neither.

### 28.4 — The engine measured at 10 km (U7)

- **Scope:** AI-against-AI matches over seeds 1–40 on the 10 km map, as 27.5 measured them: the ticks, the fleets, the Ore, the match's length and how it ended. Recorded in design §2. U7 itself, the 99th percentile tick and frame on the development machine in Release, is the owner's run.
- **Verify:** the container's figures, stated as such; **owner run** of `--measure` and of a match at the widest view.
- **As built:** recorded in design §2: all 40 end, all by domination, at a median of 2:09; the median side peaks at 25.5 warships; every side's station is at level 5, a cap of 50 points, by minute 20, and the median side holds 18,236 Ore at minute 30; the median match's 99th percentile tick is 2.5 ms in the container. The 40 matches took 16 minutes of processor time, 4:45 on four threads. That the matches are twice as long, all by domination, with the Ore piling up behind the cap, is for the owner before milestone 29.

## Milestone 29 — Placement from the seed

**Asked after 28.4** (owner, 2026-10-07): a lead of one node takes 50 minutes on any map (29.0); the Ore that piles up behind the fleet cap is left for Phase 4's later sinks, the pirates, salvage and the Repair Bay, and measured again at milestone 34; and seeded placement keeps today's counts of asteroids per sector and draws their places (29.1).

### 29.0 — A lead of one node takes 50 minutes on any map

- **Asked:** 28.4 found every match ending by domination at a median of two hours, a lead of one node taking 139 minutes on 25 nodes against 50 on nine. The owner chose a drain that does not divide by the map's nodes.
- **Scope:** tickets are whole; every `drainIntervalSeconds` a player loses `drainTicketsPerNodeDifference` for each node it is behind. The data is 1 ticket every 3 seconds, so a lead of one node takes 1,000 drains, 50 minutes, on any map.
- **ADR:** ADR-057 edited in place; design §6 and §14.
- **Acceptance:** `DominationTests`, a lead of one ending the match in exactly 50 minutes on the 10 km map.
- **Verify:** CI; the container's run of `GameLogicTests`.

### 29.1 — Placement from the seed

- **Gate:** L10, decided; the asteroids' counts by the owner on 2026-10-07.
- **Scope:** `Map.json` keeps the size, the sectors, the fields, the starts and each home's three asteroids, and gives each sector a kind; each kind names the ore asteroids it gets by yield, with today's counts. The server places them from the match's seed before the match, point-symmetric (design §7). Derelicts and pirate outposts join the kinds with milestones 30 and 31.
- **ADR:** a new one, ADR-072: seeded placement. ADR-036 decision 4 edited in place.
- **Acceptance:** `MapTests` cover the shape and the Ore over twelve seeds, symmetry, reachability, the same seed placing the same map and every asteroid keeping the placement's rules, and the loader's refusals of broken kinds.
- **Verify:** CI; the container's run of `GameLogicTests`; seeds 1–40, AI against AI.
- **As built:** recorded in [ADR-072](../Design/ADR/ADR-072-seeded-placement.md) and design §2. Seeds 1 to 2,000 all place the map, at about 10 µs each. `Tools/MakeMap.py` writes the kinds. The tests that named an asteroid's place find one the seed placed (`TerritoryMatch::AsteroidIn`), and the fixtures place their bases and loads on the server's placed map; no AI test needed re-timing. Over seeds 1–40 the median match is 1:37, every match ends by domination, and each player wins 20.

### 29.2 — The fog works out only what changed

- **Asked:** the owner, playing a Debug build on the 10 km map, found the ships stuttering and the controls lagging (2026-10-07), and chose to make the fog's update incremental. Measured in the container, the client's fog update of the whole 250,000-cell grid took about 40 ms a snapshot unoptimized, 2 ms optimized, and every changed fog texture was converted and copied whole.
- **Scope:** `FogOfWar` keeps a count per cell of the entities and the held sectors that see it, and an update counts again only the sectors that changed and the entities that appeared, went or moved, a moved circle only where its rows' runs differ. It flags the rows whose shades changed, and `GroundMaskPipeline::SetShades` writes and copies only those. The shades are those the whole update gave.
- **ADR:** ADR-052 edited in place.
- **Acceptance:** `FogOfWarTests`, with `KeepsWhatIsStillSeen` and `SaysWhichRowsChanged` new.
- **Verify:** CI; **owner run**, in Debug, on the 10 km map.
- **As built:** checked against the earlier code cell for cell over 1,200 random updates; on 120 entities, half of them moving, an update takes 2.5 ms unoptimized and 0.4 ms optimized, against 40 ms and 1.1 ms. `FogOfWarTests` ran in the container against a stand-in for the test framework; `GroundMaskPipeline` is not built there, and CI is its first build.

## Milestones 30 to 34

Each is scoped in detail when it becomes the next milestone, from the design section its tasks name. Their tasks are on the board above.

- **30 — Pirates** (design §8): the neutral owner, its outposts and their guarding rule, and how the client draws them.
- **31 — Derelicts** (design §9): salvage, its Ore and its research.
- **32 — The Repair Bay and retreat** (design §10).
- **33 — The AI plays Phase 4** (design §12).
- **34 — Measuring Phase 4** (design §2): the match log and U1–U8.
