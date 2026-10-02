# Outpost Commander — Phase 1 Implementation Plan

Status: **open** · 2026-10-02 · Derived from [the Phase 1 design](OutpostCommander-Phase1.md)

The Phase 1 design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. [The MVP plan](ImplementationPlan.md) is closed, and its rules for how an agent works carry over unchanged.

---

## How an agent uses this plan

The MVP plan's rules hold (its "How an agent uses this plan"), with these for Phase 1:

1. **Read AGENTS.md, then the Phase 1 design, then the MVP design sections the task touches.** The Phase 1 design amends the MVP's; where they differ, Phase 1 wins.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in milestone order.
3. **A gate marked proposal is an owner decision.** The design writes a proposal down; the task does not start until the owner has confirmed or changed it, and the design is updated with the answer first.
4. **ADRs are no longer edited in place.** The MVP is done, so an accepted ADR stays as it is and a changed decision is a new ADR that supersedes it (AGENTS.md §6), numbered from ADR-029.
5. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. Tasks marked *Owner run* stay `in review` until the owner has run them.

Task numbers continue the MVP plan's milestones, so that a number names one task across both plans.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 7.1 | Ships bank in their turns | — | — | todo |
| 8.1 | Measure where an order tick's time goes | — | — | todo |
| 8.2 | Order ticks within 5 ms | 8.1 | — | todo |
| 9.1 | Typography: two faces, several sizes, sprites | — | H7 decided | todo |
| 9.2 | Floating windows | 9.1 | — | todo |
| 9.3 | The designer window after the mockup | 9.2 | H6 | todo |
| 9.4 | Research and production as windows | 9.2 | — | todo |
| 10.1 | Research tiers: the schema and the 17 topics | — | H2 decided | todo |
| 10.2 | The Pulse Drive, the Flak Battery and the Rail Cannon | 10.1 | H1 decided | todo |
| 10.3 | The Q2 check per tier, and tuning against it | 10.2 | H4 decided | todo |
| 10.4 | The AI on tiers and new designs | 10.3 | — | todo |
| 11.1 | Ore reserves and depletion | — | H3 decided | todo |
| 11.2 | The 5 km map | 11.1 | H3 decided | todo |
| 11.3 | The AI follows the ore | 11.2, 10.4 | — | todo |
| 12.1 | Losing all production | — | H5 decided | todo |
| 13.1 | The match log for Phase 1 | 12.1, 11.3 | — | todo |
| 13.2 | P1–P5 | 13.1, 9.3, 9.4, 8.2 | — | todo |

### Milestone order

7, 8, 9, 10, 11, 12, 13. Milestones 7 and 9 are presentation and can be run by the owner as soon as they land. Milestone 8 goes before 10 and 11 because they raise the load the order ticks carry. Milestone 13 measures what the others built, so it comes last.

---

## Gates

Each is an owner decision, from design §15. H1–H5 and H7 were decided on 2026-10-02; H6 and H8 are decided at owner runs.

| Gate | Decision | Proposed in | Blocks |
|---|---|---|---|
| H1 | The Flak Battery, Rail Cannon and Pulse Drive numbers. **Decided on 2026-10-02:** as proposed, as starting values. | design §5 | — |
| H2 | The 17 new research topics: prerequisites, effects, Ore and time. **Decided on 2026-10-02:** as proposed. | design §6 | — |
| H3 | The 5 km map's rings, yields, reserves, and the 20% trickle. **Decided on 2026-10-02:** as proposed; the layout is confirmed in 11.2. | design §8 | — |
| H4 | Whether Q2's (b) exempts the Pulse Drive. **Decided on 2026-10-02:** it does, and play judges it. | design §7 | — |
| H5 | A lost Command Station stays lost; the last Shipyards are revealed. **Decided on 2026-10-02:** both, as proposed. | design §4 | — |
| H6 | The designer's colors and sizes, from the mockup | design §11 | 9.3 (closed at its owner run) |
| H7 | Cascadia Mono on the development machine, or Consolas. **Decided on 2026-10-02:** 9.1 checks, and falls back to Consolas. | design §11 | — |
| H8 | The camera's zoom limit on the 5 km map | design §8 | nothing; decided after 11.2's owner run |

---

## Milestone 7 — Banking

### 7.1 — Ships bank in their turns

- **Goal:** design §9. Ships lean into their turns, drawn by the client only.
- **Scope:**
  - **The angle.** In `GameApp`, from the interpolated snapshots: the ship's sideways acceleration, its heading's rate of turn times its speed, mapped to a bank angle, clamped to the hull's limit, and smoothed by a critically damped spring. The rate comes from the two snapshots the interpolator is between, not from frame-to-frame differences, so it does not depend on the frame rate.
  - **The data.** Each hull's and the Constructor's maximum bank and response time in `OutpostCommander/Assets/Models.json`, beside its length (ADR-011, ADR-018). The design's starting values: Small about 35°, Medium about 22°, Large about 12°, the Constructor about 15°, each settling in about a quarter of a second.
  - **What rolls.** The ship's world transform, and so its faces and crease lines (ADR-027); the gun and exhaust hardpoints (ADR-019, `Hardpoints.cpp`); and the starting pose of an explosion's shards (ADR-026). Not the footprint, the selection ring, the health bar or picking.
  - **No change** to `GameProtocol`, `GameLogic` or the snapshot.
- **ADR:** a new one: how the bank is derived and smoothed, and that the simulation knows nothing of it.
- **Acceptance:** `GameAppTests` cover the angle without a GPU: none flying straight, none turning on the spot, into the turn on both sides, clamped, smoothed, the same at 30 and 144 frames a second, and a hardpoint rolled with its hull.
- **Verify:** CI; **owner run**: whether the bank reads from the RTS camera at the default view, and that the exhaust stays on the hull.

---

## Milestone 8 — The order ticks

### 8.1 — Measure where an order tick's time goes

- **Goal:** design §10. Know what costs 5.7–20.7 ms before changing it.
- **Scope:** time the parts of an order tick inside `Simulation` and `InProcessServer`: building the path graphs, the group's route, the per-ship searches, laying out the slots, the parting of ships, and the rest of the step. Log them with `--measure --load` alongside the tick times, and extend `Tools/FrameTimes.py` to summarize them. The timing is compiled into every build, as the tick timings are, and costs a few clock reads a tick.
- **Acceptance:** the summary lists each part's mean and worst over a run.
- **Verify:** CI; **owner run** of `--measure --load` in Release|ARM64 on the development machine. The breakdown is recorded in design §10 with how it was measured.

### 8.2 — Order ticks within 5 ms

- **Goal:** Q4's tick half met with the orders included: no tick over 5 ms at 200 ships and 40 structures on the development machine.
- **Scope:** what 8.1 finds. Design §10 names the ways open to it. Any part of an order that moves to later ticks is done within Q5's 150 ms, which Q5 is measured again to show.
- **ADR:** a new one: the change, superseding the parts of ADR-010 it changes.
- **Acceptance:** `MovementTests` and `PathfinderTests` still pass, and the Q2 check's tier-1 stage still gives the MVP's report: a change to when ships start moving can change a battle, and if it does, the change is reported and the owner decides.
- **Verify:** CI; **owner run** of `--measure --load` and Q5's measurement on the development machine. The figures go to design §10 and the MVP design's Q4 row is noted as superseded.

---

## Milestone 9 — The interface

### 9.1 — Typography: two faces, several sizes, sprites

- **Gate:** H7, decided.
- **Goal:** the interface can draw the mockup's text (design §11).
- **Scope:**
  - **Faces and sizes.** `Neuron::RasterizeGlyphs` takes a face, a weight, a stretch and a size, and the atlas holds several: Bahnschrift semibold and semibold-condensed at the mockup's sizes, and Cascadia Mono, or Consolas under H7, for figures. Each is rasterized at its reference size times ADR-006's scale, as now.
  - **Characters.** Printable ASCII, × and ·. Others still show `?`.
  - **Tracking.** A text run can be laid out with extra space between letters, for the spaced capitals.
  - **Sprites.** The ◆, a checkbox, a hatching tile that repeats, and a corner bracket, drawn into the atlas at start-up, so they scale with the rest. A panel can be filled with the hatching.
  - **A missing face fails loudly.** A font that is not installed is reported, not silently replaced.
- **ADR:** a new one, superseding ADR-015's one size and ASCII-only text. It keeps ADR-015's atlas, quads, one draw call, and no shipped font.
- **Acceptance:** `GlyphAtlasTests` cover several faces and sizes in one atlas, the two non-ASCII characters, tracking, and the sprites.
- **Verify:** CI; **owner run**: every existing panel still reads, and text is sharp at 1920×1080 and 2880×1920.

### 9.2 — Floating windows

- **Goal:** design §12.
- **Scope:** in `GameApp`, a window manager the HUD lays out through: windows with a title bar, a front-to-back order, dragging by the title bar, clamping to the screen with the title bar always reachable, bringing to the front on a click, closing with × and Esc, and each window's position kept in memory for as long as the process lives. Input focus tests the windows front to back, then the anchored HUD, then the world (ADR-015's rectangle test). Positions are held in reference units and clamped again when the screen's size changes.
- **ADR:** a new one: the window manager and its focus order.
- **Acceptance:** `HudTests` cover dragging, clamping after a resize, the order of focus, Esc closing the front window, and a window reopening where it was closed.
- **Verify:** CI; **owner run.**

### 9.3 — The designer window after the mockup

- **Gate:** H6, closed at this task's owner run.
- **Goal:** design §11, laid out as [`Mockups/ShipDesigner.png`](Mockups/ShipDesigner.png).
- **Scope:**
  - **The window.** One designer, opened from a selected Shipyard's panel and from a key; its target Shipyard, stepped through with arrows or set by selecting a Shipyard in the world; Shipyard numbers in the order they were finished, carried in the snapshot.
  - **The header:** the target's five queue slots, the count it has built this match, and the Ore.
  - **The content, top to bottom:** the name with its count and Save/Saved; the saved-design chips with their abbreviations, scrolling sideways; the hull, drive and weapon rows of cards, three to a line, with locked cards hatched and naming their topic; the performance bars against the best in the game; the damage cards per ship and per 100 Ore, colored by thirds; the hover preview with its change against the current design; Rename, the ×N stepper and Queue.
  - **×N** queues N jobs, at most the target's free slots, as N `QueueShipCommand`s in one frame. No protocol change is needed for it.
  - **The abbreviations** are a field of each component in `Tuning.json`, so that the designer and the match log read them from one place.
  - The MVP's rules stand: rename, never change; Queue saves first (ADR-023); 32 characters.
- **ADR:** none, unless the task finds a decision the design does not make.
- **Acceptance:** `DesignerTests` and `HudTests` cover the layout with every component locked and with every one unlocked (the weapon row wrapping), the bars' scale, the colors' thresholds, the preview's figures, the stepper's limits, and Queue with no Shipyard.
- **Verify:** CI; **owner run**, side by side with the mockup. The owner's adjustments to colors and sizes close H6 and are recorded in design §11.

### 9.4 — Research and production as windows

- **Goal:** design §12. The Research Lab's research and a structure's production queue move from their panels into floating windows.
- **Scope:** both open from their structure's selection panel and from a key, take the mockup's look, and keep what the panels do today (ADR-017, ADR-015).
- **Acceptance:** `HudTests` cover both.
- **Verify:** CI; **owner run.**

---

## Milestone 10 — Tiers

### 10.1 — Research tiers: the schema and the 17 topics

- **Gate:** H2, decided.
- **Goal:** design §6.
- **Scope:**
  - **The tuning data** learns a topic's tier, a gateway topic with no effect of its own, and the new effects: an ore reserve, structure hit points, a structure weapon's fire rate, every ship's speed, and the Constructor's rates. Upgrades of one stat add their percentages.
  - **The 17 topics** in `Tuning.json` as H2 decides them.
  - **`Tools/BattleModel.py`** reads the new effects it can model and ignores, by name, those it cannot.
  - **The research window** (9.4) groups topics by tier and shows a gateway as one.
- **ADR:** a new one: the tiers, gateways and the stacking of upgrades, beside ADR-017.
- **Acceptance:** `ResearchTests` and `TuningTests` cover each new effect, stacking, a gateway gating its tier, and a lab's queue across tiers.
- **Verify:** CI.

### 10.2 — The Pulse Drive, the Flak Battery and the Rail Cannon

- **Gate:** H1, decided.
- **Goal:** design §5, in the game and in the model.
- **Scope:** the three components in `Tuning.json` with their abbreviations; the Flak Battery's splash through the Missile Rack's (ADR-014); their exhaust color (the Pulse Drive's) and their shots, as presentation data (ADR-019). The Pulse Drive's exhaust color is the owner's to pick at the run.
- **Acceptance:** `CombatTests` and `DesignTests` cover each.
- **Verify:** CI; **owner run**: whether the Pulse Drive's exhaust and the two weapons' shots read.

### 10.3 — The Q2 check per tier, and tuning against it

- **Gate:** H4, decided.
- **Goal:** design §7. P3.
- **Scope:** `Q2Check` gains the stages of design §7: starting, tier 1, tier 2 and tier 3, each with its components and budgets, and (d) per tier. Run it, tune the tier-2 and tier-3 numbers against it as B.1 tuned §12, and record each number that moved and why in design §5 and §6. Tier 1's numbers move only if a later tier's result requires it, and the owner decides that.
- **Acceptance:** `Q2CheckTests.TheFullCheck` passes all four criteria at every stage, in the Linux container; `TheRecordedCountersHold` gains a counter per new component.
- **Verify:** CI; the owner runs the check of record in Release|ARM64 on the development machine, and its time is recorded.

### 10.4 — The AI on tiers and new designs

- **Goal:** design §13: the AI researches the 25 topics and counters the new designs. It is not made harder to rush (design §3).
- **Scope:** `Opponent.json`: its research order through all three tiers, and counters for and with the new designs, from 10.3's results. `AiSettingsTests` checks every identifier against `Tuning.json`, as now.
- **Acceptance:** `AiPlayerTests`: the AI reaches tier 3 against a passive player, and answers a Flak, a Rail and a Pulse design.
- **Verify:** CI.

---

## Milestone 11 — The map

### 11.1 — Ore reserves and depletion

- **Gate:** H3, decided.
- **Goal:** design §8, ore that runs out.
- **Scope:** each ore asteroid's reserve and the trickle in the map data and the tuning data; a rig drawing its reserve down; Improved Extraction draining faster and Deep Core Survey adding to what is left; the reserve in the snapshot for an asteroid in sight, and remembered otherwise (ADR-024); the figure in the selection panel and a mark on the minimap for an exhausted asteroid.
- **ADR:** a new one: depletion and what the snapshot carries of it.
- **Acceptance:** `EconomyTests` cover a reserve running out to the trickle, both upgrades, and the fog's remembered figure.
- **Verify:** CI; **owner run** of the panel and the minimap.

### 11.2 — The 5 km map

- **Gate:** H3, decided.
- **Goal:** design §8, the map.
- **Scope:** `Map.json` at 5,000 m with the four rings, point-symmetric, every passage at least the minimum gap. `MapTests` check both. The fog of war's grid, the minimap and the path graphs at the new size are checked against Q4: a `--measure --load` run on the new map is part of the owner run.
- **Acceptance:** `MapTests`; `PathfinderTests` on the new map.
- **Verify:** CI; **owner run**: the layout confirmed by the owner, as MVP task 2.3's was, and its first matches decide H8.

### 11.3 — The AI follows the ore

- **Goal:** design §13: the AI builds on the nearest asteroids with ore left and moves on when one runs dry.
- **Scope:** `AiPlayer` and `Opponent.json`: rings rather than a fixed count of home and contested asteroids.
- **Acceptance:** `AiPlayerTests`: an AI whose home reserves run out builds further out.
- **Verify:** CI.

---

## Milestone 12 — The win condition

### 12.1 — Losing all production

- **Gate:** H5, decided.
- **Goal:** design §4.
- **Scope:** the rule in `Simulation`: a player loses when it has no Command Station and no finished Shipyard. H5's answers: whether a lost Command Station stays lost, and the reveal of the last Shipyards through fog of war. The AI attacks production first (design §13). The banner is unchanged.
- **ADR:** a new one, superseding ADR-020's decision 8.
- **Acceptance:** `MatchOutcomeTests`: losing the Command Station alone does not end the match; losing it and the last finished Shipyard does; a Shipyard under construction does not count; the reveal. `AiPlayerTests`: the attack group goes for a Shipyard first.
- **Verify:** CI; **owner run.**

---

## Milestone 13 — Measuring Phase 1

### 13.1 — The match log for Phase 1

- **Goal:** the figures P1, P2 and P4 need (design §2, §10).
- **Scope:** `MatchLog` records each gateway topic as it finishes, each side's warship count every 30 s and its peak, and each asteroid as it runs dry. `Tools/MatchLog.py` reports the match's length against 45–60 minutes, the designs built in each tier, and the peak ship count. A switch runs 10 seeded AI-against-AI matches on the real server headlessly and summarizes their lengths.
- **Acceptance:** `MatchLogTests`.
- **Verify:** CI.

### 13.2 — P1–P5

- **Goal:** answer design §2's questions and record them there. A failed answer is still a result.
- **Scope:** the owner plays matches without rushing the AI; the AI-against-AI run; the Q4 measurement on the 5 km map at the peak ship count 13.1 reports; the owner's judgement of the designer, the windows and the banking.
- **Verify:** **owner run.** The answers decide whether and how Phase 2 goes on to two hours (design §1).
