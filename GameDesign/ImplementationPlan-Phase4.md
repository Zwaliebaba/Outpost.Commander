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
| 28.1 | The client and the engine follow the map's size | — | L6 | todo |
| 28.2 | The 10 km map and its node caps | 28.1 | L3, L4 | todo |
| 28.3 | Production follows territory | — | L5 | todo |
| 28.4 | The engine measured at 10 km (U7) | 28.2 | — | todo |
| 29.1 | Placement from the seed | 28.2 | L10 | todo |
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

## Milestones 28 to 34

Each is scoped in detail when it becomes the next milestone, from the design section its tasks name. Their tasks are on the board above.

- **28 — The 10 km map** (design §6, §11): the fog grid and its texture, the camera's focus and widest view, and the minimap follow the map's size; the 25-sector map and its node caps; Shipyards and Repair Bays only in held sectors; U7 measured.
- **29 — Placement from the seed** (design §7).
- **30 — Pirates** (design §8): the neutral owner, its outposts and their guarding rule, and how the client draws them.
- **31 — Derelicts** (design §9): salvage, its Ore and its research.
- **32 — The Repair Bay and retreat** (design §10).
- **33 — The AI plays Phase 4** (design §12).
- **34 — Measuring Phase 4** (design §2): the match log and U1–U8.
