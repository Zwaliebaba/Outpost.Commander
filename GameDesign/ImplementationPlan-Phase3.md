# Outpost Commander — Phase 3 Implementation Plan

Status: **draft** · Started 2026-10-04, from [the Phase 3 design](OutpostCommander-Phase3.md)'s draft, at the owner's request for the plan alone · Every task is blocked by gate K7 until the owner has run Phase 2 and accepted the design · Derived from the Phase 3 design

The Phase 3 design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. [The Phase 2 plan](ImplementationPlan-Phase2.md) stays the game's open plan until its owner runs are done.

**Nothing in this plan starts yet.** The owner decided on 2026-10-04 (design gate K7) that Phase 3 is accepted only after the owner has run Phase 2's open tasks, 14.3, 15.2, 16.2, 17.1, 17.2 and 19.2, and answered S5. Until then the design is a draft, and this plan is too. When the design is accepted, this plan's status becomes **open**, the K7 row of the gates table records the date, and the tasks become `todo`. If the owner's run changes Phase 2, this plan is revised before any task starts.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the Phase 3 design, then the Phase 2, Phase 1 and MVP design sections the task touches.** Phase 3 amends Phase 2, which amends Phase 1, which amends the MVP. Where they differ, the later document wins.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in milestone order.
3. **ADRs are edited in place** (AGENTS.md §6), and new ones take the next free number, ADR-064 onward.
4. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. The server, the AI and the match log build and run there against a stand-in for the Windows headers and the test framework, as Phase 2's did. The renderer and the window do not. Tasks marked *Owner run* stay `in review` until the owner has run them.
5. **The AI keeps playing at every milestone.** Each rule that would stop the AI's plan, such as a level 1 Shipyard refusing a Medium hull or a Lab without the level for tier 2, comes with the least AI change that keeps `AiPlayerTests` passing, as Phase 2's 14.5 did for territory. Milestone 24 then makes the AI play upgrades well.
6. **The gates are the design's, K1 to K7** (design §12). The [Interface plan](ImplementationPlan-Interface.md) also letters its gates K1 to K7. In this plan, a K gate always means the Phase 3 design's.

Task numbers continue the Phase 2 plan's milestones, so that a number names one task across the game's phase plans. The Interface plan's milestones 14 to 17 are a separate series.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 20.1 | Upgrading a structure, on the server | — | K5, K6 decided; K7 | blocked: K7 |
| 20.2 | The client shows levels and upgrades | 20.1 | K7 | blocked: K7 |
| 21.1 | Shipyard levels: hulls by level | 20.1 | K1 decided; K7 | blocked: K7 |
| 21.2 | The designer and production say what a Shipyard cannot build | 21.1, 20.2 | K7 | blocked: K7 |
| 22.1 | Research Lab levels open the tiers; the gateways go | 20.1 | K2 decided; K7 | blocked: K7 |
| 22.2 | The Lab's second research slot | 22.1 | K3 decided; K7 | blocked: K7 |
| 22.3 | The research window follows the Lab's level | 22.1, 22.2, 20.2 | K7 | blocked: K7 |
| 23.1 | Command Station levels: the node cap and the guns | 20.1 | K4, K6 decided; K7 | blocked: K7 |
| 23.2 | The client shows the cap and spreads the station's guns | 23.1, 20.2 | K7 | blocked: K7 |
| 24.1 | The AI plays upgrades | 21.1, 22.2, 23.1 | K7 | blocked: K7 |
| 25.1 | The match log for Phase 3 | 20.1, 23.1 | K7 | blocked: K7 |
| 25.2 | T1–T5 | 24.1, 25.1 | K7 | blocked: K7 |

### Milestone order

20, 21, 22, 23, 24, 25. The upgrade itself comes first because every structure's levels are built on it. The Shipyard goes next because its rule is the smallest. It also changes the opening (design §5), and T1 needs that change in place early. The Lab follows, because removing the gateways touches the tuning loader, the balance check and the snapshot. The Command Station's cap comes last of the three, since it is the rule most likely to stall territory (T2), and it is easiest to measure on top of the other two. The AI is reworked once every rule it plays by is in. Milestone 25 measures what the others built, so it comes last, and tunes against its figures.

### What else waits on this plan

- **The self-play plan.** [SP1.4](ImplementationPlan-SelfPlay.md) waits on "Phase 3's AI", which is 24.1. SP2.2 waits on "Phase 3 built", which is 25.2.
- **The Interface plan.** Its 16.1 no longer marks the gateway topics, which 22.1 removes (owner, 2026-10-04). Its 15.1 adds a line to a producer's and the Lab's selection panel, and its 16.3 adds to a production card. Both are laid out to leave room for what 20.2 and 21.2 add. Whichever lands second fits around the first.

---

## Gates

Each is an owner decision, from design §12.

| Gate | Decision | Proposed in | Blocks |
|---|---|---|---|
| K1 | A level 1 Shipyard builds Small hulls, level 2 adds Medium and level 3 adds Large; the Large Hull topic stays. **Decided on 2026-10-04.** | design §5 | — |
| K2 | The Lab's levels 2 and 3 replace the gateways, at their Ore and prerequisites; a rebuilt Lab starts at level 1. **Decided on 2026-10-04**, the upgrade times as starting values. | design §6 | — |
| K3 | The Lab's level 4 is a second research slot; the Shipyard stops at level 3 and the Lab at level 4. **Decided on 2026-10-04.** | design §5, §6 | — |
| K4 | The Command Station caps the nodes held at 3 to 7, with upgrades of 300, 500, 700 and 900 Ore; without a station, level 1's cap. **Decided on 2026-10-04.** | design §7 | — |
| K5 | Every level is fitted to level 1's size; placement is unchanged. **Decided on 2026-10-04.** | design §4 | — |
| K6 | +20% of base hit points a level; the Command Station has 1, 1, 2, 2 and 3 Defence guns. **Decided on 2026-10-04.** | design §4, §7 | — |
| K7 | Phase 3 is accepted after the owner has run Phase 2's open tasks and answered S5. **Decided on 2026-10-04. Not yet met.** | design §12 | every task |

### Open questions, for the owner before the task that meets them

The design leaves these open, or contradicts itself on them. Each is answered by the owner, and the answer is written into the design, before its task starts.

- **Q1, for 20.1: how a level's hit points combine with Reinforced Structures.** The design gives +20% of base hit points a level (§4), and Reinforced Structures gives +25% to every structure. `Research.cpp` sums the percents of every upgrade into one factor. *Proposed:* the level's percent adds to the same sum, so a level 3 Lab with Reinforced Structures has base × (1 + 0.40 + 0.25).
- **Q2, for 20.2: the footprint refusal.** Design §9 has the Upgrade button dim for "no room for the larger footprint", but §4 and gate K5 keep the footprint unchanged by an upgrade, so that refusal cannot happen. *Proposed:* drop it from §9.
- **Q3, for 20.1: the level of a remembered enemy structure.** Under fog of war, a player remembers an enemy structure it has seen (ADR-024). *Proposed:* it remembers the level last seen, as it remembers the structure's place, and an upgrade it did not see is not shown.
- **Q4, for 20.1: an upgrade on a damaged structure.** A Constructor told to work on a structure that is both damaged and being upgraded either repairs it or builds the level. *Proposed:* the upgrade first, since its Ore is paid, and repair after.

---

## Milestone 20 — Upgrading a structure

### 20.1 — Upgrading a structure, on the server

- **Gate:** K5, K6, decided; K7. Q1, Q3 and Q4 answered.
- **Goal:** design §4 on the server and in the protocol, for every kind that has levels. The levels do not yet change what a structure does. 21.1, 22.1 and 23.1 give them their effects.
- **Scope:**
  - **The data.** In `Tuning.json`, the Command Station, the Shipyard and the Research Lab each gain a `levels` list. For each level above the first, it gives the upgrade's Ore and Constructor seconds. A top-level `levelHitPointsPercent` is 20. A kind without a list, such as the Relay, the Defence Platform or the Mining Rig, has one level. The loader refuses a list longer than the design's top level for its kind (K3).
  - **The order.** A new command upgrades one of the player's own finished structures by one level. Its Ore is paid when the order is given, and nothing is refunded. Constructors then work on the upgrade as they work on a site (ADR-016), and several share it. The server refuses an upgrade at the top level, one already under way, one it cannot afford, and one on a structure that is still being built. Each refusal is a new `CommandResult`.
  - **The structure keeps working** while the level is built. A destroyed structure loses its levels, and the upgrade's Ore with it. A structure rebuilt where it stood starts at level 1.
  - **Hit points.** A finished level adds `levelHitPointsPercent` of the kind's base hit points, combined with research as Q1 answers. A structure keeps its share of its hit points when the level lands, as `Simulation` already rescales them when Reinforced Structures is researched.
  - **The snapshot** carries each structure's level and the upgrade under way, in thousandths, as construction is. It also carries the level a player remembers of an enemy structure, as Q3 answers. The wire format's version goes up.
- **ADR:** a new one, ADR-064: the upgrade, its order, its refusals, its hit points and what the snapshot shows. ADR-016 is edited in place where the Constructors' work is shared with an upgrade. ADR-045 is edited in place: the level is in the simulation and the snapshot.
- **Acceptance:** `UpgradeTests` cover an upgrade paid and refused for each reason, Constructors sharing it, the structure working throughout, the level landing with its hit points, a structure destroyed during an upgrade, a rebuilt structure at level 1, and a kind without levels refusing. `TuningTests` cover the `levels` list. `WireFormatTests` cover the level and the upgrade in the snapshot. `FogTests` cover a remembered level.
- **Verify:** CI.

### 20.2 — The client shows levels and upgrades

- **Gate:** K7. Q2 answered.
- **Scope:**
  - **The selected structure's panel** names its level, "SHIPYARD 01 · L2". It has an Upgrade button showing the next level's Ore, its time and what it gives. The button is dim, with the reason, wherever the server would refuse. The kinds' effects fill in what a level gives as 21.2, 22.3 and 23.2 land, and until then the button names only the level.
  - **The work under way** is drawn as construction is.
  - **The world** draws each structure at the level in its snapshot, with that level's mesh, creases and spinning parts, fitted to level 1's length (ADR-045, K5).
- **ADR:** ADR-064's client decisions; ADR-045 edited in place for the fitted levels.
- **Acceptance:** `HudTests` for the panel's level, the button and each dim reason. `ModelCatalogTests` for a level's mesh, fitted to level 1's length.
- **Verify:** CI; **owner run.** T5 starts here.

---

## Milestone 21 — Shipyard levels

### 21.1 — Shipyard levels: hulls by level

- **Gate:** K1, decided; K7.
- **Goal:** design §5 on the server.
- **Scope:** the Shipyard's `levels` gain the largest hull each level builds: Small at 1, Medium at 2 and Large at 3. A Shipyard refuses to queue a job whose hull is above its level, with a new `CommandResult`. A job already queued is never taken out by a change of level, since levels only go up while the Shipyard stands. The AI upgrades a Shipyard to level 2 before it queues its first Medium hull, and to level 3 before its first Large one. That is the least the AI needs, and 24.1 tunes it.
- **ADR:** ADR-064, its Shipyard decision.
- **Acceptance:** `ProductionTests` cover each hull at each level, and a Large hull needing both the topic and level 3. `AiPlayerTests` pass on the repository map.
- **Verify:** CI; the AI-against-AI matches in the container, with S1's first contact reported against Phase 2's.

### 21.2 — The designer and production say what a Shipyard cannot build

- **Scope:** the designer dims Queue for a design the target Shipyard cannot build, and says why (design §5, §9). The designer still saves every design the player has unlocked. In the production window, a design whose hull is above the Shipyard's level is dim, with the same reason. The Shipyard's Upgrade button names the hull its next level adds.
- **ADR:** ADR-064's client decisions.
- **Acceptance:** `DesignerTests`, `HudTests`.
- **Verify:** CI; **owner run.**

---

## Milestone 22 — Research Lab levels

### 22.1 — Research Lab levels open the tiers; the gateways go

- **Gate:** K2, decided; K7.
- **Goal:** design §6's levels 1 to 3, on the server.
- **Scope:**
  - **The gateways are removed.** Relay Archives (topic 9) and Precursor Vault (topic 18) leave `Tuning.json`, and `GatewayEffect` leaves `Tuning.h`, `Research.cpp` and the snapshot's topic view, which keeps its tier. Every topic that required a gateway drops it from its `requires`. The topic ids are kept, with a gap where the gateways were, so that a saved design or a match log keeps its meaning.
  - **The Lab's `levels`** gain the tier each level opens and the topics its upgrade requires: level 2 requires Improved Extraction and Hull Plating, and level 3 requires Large Hull. Their Ore and seconds are the design's. The loader checks that a topic's tier has a level that opens it, in place of checking its gateway (ADR-033).
  - **A Lab refuses to queue a topic above the tier its level opens**, with a new `CommandResult`. A topic queued at the right level stays queued. Finished research stays finished when a Lab is lost, and a rebuilt Lab starts at level 1.
  - **The balance check** drops its gateway case. It checks battles at each tier's components, as before.
  - **The AI** upgrades its Lab where its research order had a gateway. That is the least the AI needs, and 24.1 tunes it.
- **ADR:** ADR-033, edited in place: a tier is opened by the Lab's level, and `GatewayEffect` is removed. ADR-017, edited in place where it names the gateways.
- **Acceptance:** `ResearchTests` replace `AGatewayOpensItsTier` with a tier opened by the Lab's level, a topic refused below its tier, a rebuilt Lab at level 1, and research kept through a lost Lab. `TuningTests` cover the levels' tiers and requirements and the loader's refusals. `WireFormatTests` cover the topic view without its gateway. `AiPlayerTests.ReachesTierThree` passes.
- **Verify:** CI; the AI-against-AI matches in the container, with tier 3 reached reported against Phase 1's 2 in 80.

### 22.2 — The Lab's second research slot

- **Gate:** K3, decided; K7.
- **Scope:** at level 4 the Lab's queue stays five topics long, and its front two topics run side by side, each paid when it starts (ADR-017). A topic whose prerequisite is still being researched waits for it, and the slot behind it runs nothing until it may start. The snapshot carries the progress of both running topics.
- **ADR:** ADR-017, edited in place for the second slot.
- **Acceptance:** `ResearchTests` cover two topics researched at once, each paid at its start, a topic waiting for its prerequisite in the other slot, and both slots lost with the Lab. `WireFormatTests` cover both topics' progress.
- **Verify:** CI.

### 22.3 — The research window follows the Lab's level

- **Scope:** a topic above the Lab's tier is locked, and says which level opens it. The two running topics show their progress at level 4. The Lab's Upgrade button names the tier or the slot its next level gives, and the topics it still requires. The window's topic count follows the tuning data's 23 topics.
- **ADR:** ADR-064's client decisions.
- **Acceptance:** `HudTests`; 14.1's tests of the Interface plan, which keep text inside its card.
- **Verify:** CI; **owner run.**

---

## Milestone 23 — Command Station levels

### 23.1 — Command Station levels: the node cap and the guns

- **Gate:** K4, K6, decided; K7.
- **Goal:** design §7 on the server.
- **Scope:**
  - **The cap.** The Command Station's `levels` gain the nodes held, home included, and the Defence guns each level has. A Relay is refused beyond the player's cap, and a Relay under construction counts toward the cap, since it takes its node (ADR-056). The cap refuses new claims and never takes a node away. A player without a Command Station has level 1's cap. The refusal is a new `CommandResult`, and the snapshot carries each player's cap.
  - **The guns.** The station carries one Defence gun for each that its level gives, each the gun it carries today, each aiming and firing on its own.
  - **A map without sectors** plays as before. The cap applies only where territory does (ADR-056).
  - **The AI** upgrades its Command Station when it is at its cap and its plan wants another node. That is the least the AI needs, and 24.1 tunes it.
- **ADR:** ADR-056, edited in place for the cap. ADR-064, its Command Station decision, for the guns.
- **Acceptance:** `TerritoryTests` cover the cap at each level, a site counting toward it, no node lost to it, and level 1's cap without a station. `DefenseTests` cover each gun firing on its own target. `WireFormatTests` cover the cap.
- **Verify:** CI; the AI-against-AI matches in the container.

### 23.2 — The client shows the cap and spreads the station's guns

- **Scope:** the territory line shows nodes held against the cap, "4 / 5" (design §9). A Relay's ghost is red beyond the cap, with the reason. The station's Upgrade button names the nodes and guns its next level gives. The client spreads each gun's shots over its set's hardpoints for the level (design §7, ADR-018).
- **ADR:** ADR-056's client decision, edited in place.
- **Acceptance:** `HudTests`, `PlacementTests`, `HardpointsTests`.
- **Verify:** CI; **owner run.**

---

## Milestone 24 — The AI

### 24.1 — The AI plays upgrades

- **Goal:** design §8.
- **Scope:** the AI upgrades its Command Station to level 2 before its first claim beyond its two flanks, and further whenever it is at its cap and has a node it wants. It upgrades a Shipyard to level 2 early, and to level 3 when it wants Large hulls. It upgrades its Lab where it would have researched a gateway, and to level 4 once tier 3 is open. It counts an enemy structure's level when it chooses what to attack. Its numbers are starting values in `Opponent.json`.
- **ADR:** ADR-020 and ADR-041, edited in place.
- **Acceptance:** `AiPlayerTests`, `AiSettingsTests`.
- **Verify:** CI; the AI-against-AI matches.

---

## Milestone 25 — Measuring Phase 3

### 25.1 — The match log for Phase 3

- **Goal:** the figures T1–T4 need (design §2, §10).
- **Scope:** the log records each upgrade started, finished or lost, with its structure and level. It records each attack on a structure above level 1, and the time both sides spend at their cap with equal numbers of nodes. `Tools/MatchLog.py` reports T2–T4 for a match and over seeds 1–40, beside S1–S4 for T1.
- **ADR:** ADR-038, edited in place.
- **Acceptance:** `MatchLogTests`.
- **Verify:** CI.

### 25.2 — T1–T5

- **Goal:** answer design §2's questions and record them there. A failed answer is still a result.
- **Scope:** the AI-against-AI run over seeds 1–40, tuned against T1–T4 where the AI's own settings and the upgrade numbers can reach them. The owner's matches and judgement for T5.
- **Verify:** **owner run.**
