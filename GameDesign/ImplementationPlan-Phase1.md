# Outpost Commander — Phase 1 Implementation Plan

Status: **open** · 2026-10-02 · Derived from [the Phase 1 design](OutpostCommander-Phase1.md)

The Phase 1 design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks. Phase 2, territory, is [a draft design](OutpostCommander-Phase2.md) with no plan yet; its one effect here is task 11.2's layout. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. [The MVP plan](ImplementationPlan.md) is closed, and its rules for how an agent works carry over unchanged.

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
| 7.1 | Ships bank in their turns | — | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 8.1 | Measure where an order tick's time goes | — | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 8.2 | Order ticks within 5 ms | 8.1 | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run on the development machine |
| 9.1 | Typography: two faces, several sizes, sprites | — | H7 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 9.2 | Floating windows | 9.1 | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 9.3 | The designer window after the mockup | 9.2 | H6 | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 9.4 | Research and production as windows | 9.2 | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 10.1 | Research tiers: the schema and the 17 topics | — | H2 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run; its last four topics came with 10.2 |
| 10.2 | The Pulse Drive, the Flak Battery and the Rail Cannon | 10.1 | H1 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); built and tested in CI (Debug|x64), not yet run |
| 10.3 | The Q2 check per tier, and tuning against it | 10.2 | H4, H9, H10 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); the check passes in the container, not yet on the development machine |
| 10.4 | The AI on tiers and new designs | 10.3 | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49) |
| 11.1 | Ore reserves and depletion | — | H3 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); not yet run |
| 11.2 | The 5 km map | 11.1 | H3 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); the layout awaits the owner run |
| 11.3 | The AI follows the ore | 11.2, 10.4 | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49) |
| 12.1 | Losing all production | — | H5 decided | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); not yet run |
| 13.1 | The match log for Phase 1 | 12.1, 11.3 | — | in review, [#49](https://github.com/Zwaliebaba/Outpost.Commander/pull/49); the switch has not run on Windows |
| 13.2 | P1–P5 | 13.1, 9.3, 9.4, 8.2 | — | todo |

### Milestone order

7, 8, 9, 10, 11, 12, 13. Milestones 7 and 9 are presentation and can be run by the owner as soon as they land. Milestone 8 goes before 10 and 11 because they raise the load the order ticks carry. Milestone 13 measures what the others built, so it comes last.

---

## Gates

Each is an owner decision, from design §15. H1–H5 and H7 were decided on 2026-10-02; H6 and H8 are decided at owner runs; H9 and H10 came out of task 10.3's first runs and were decided on 2026-10-03.

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
| H9 | Whether Q2's (b) at tiers 2 and 3 asks every component to be worth building, or only those the tier adds. **Decided on 2026-10-03:** only those the tier adds. | design §7, §15 | — |
| H10 | What the Flak Battery is for, beyond the swarm. **Decided on 2026-10-03:** a role against the Medium hull too, tuned in 10.3. | design §5, §15 | — |

---

## Milestone 7 — Banking

### 7.1 — Ships bank in their turns

- **Goal:** design §9. Ships lean into their turns, drawn by the client only.
- **Scope:**
  - **The angle.** In `GameApp`, from the interpolated snapshots: the ship's sideways acceleration, its heading's rate of turn times its speed, mapped to a bank angle, clamped to the hull's limit, and smoothed by a critically damped spring. The rate comes from the two snapshots the interpolator is between, not from frame-to-frame differences, so it does not depend on the frame rate.
  - **The data.** Each hull's and the Constructor's maximum bank and response time in `OutpostCommander/Assets/Models.json`, beside its length (ADR-011, ADR-018). The design's starting values: Small about 35°, Medium about 22°, Large about 12°, the Constructor about 15°, each settling in about a quarter of a second.
  - **What rolls.** The ship's world transform, and so its faces and crease lines (ADR-027); the gun and exhaust hardpoints (ADR-019, `Hardpoints.cpp`); and the starting pose of an explosion's shards (ADR-026). Not the footprint, the selection ring, the health bar or picking.
  - **No change** to `GameProtocol`, `GameLogic` or the snapshot.
- **ADR:** [ADR-029](../Design/ADR/ADR-029-ship-banking.md): how the bank is derived and smoothed, and that the simulation knows nothing of it.
- **Acceptance:** `GameAppTests` cover the angle without a GPU: none flying straight, none turning on the spot, into the turn on both sides, clamped, smoothed, the same at 30 and 120 frames a second, and a hardpoint rolled with its hull.
- **Verify:** CI; **owner run**: whether the bank reads from the RTS camera at the default view, and that the exhaust stays on the hull.
- **As built:** [ADR-029](../Design/ADR/ADR-029-ship-banking.md).
  - **The motion.** `SnapshotInterpolator::Motions` gives each entity's speed and turn rate between the two snapshots around the view.
  - **The bank.** `Outpost::TargetBankRadians` and `Outpost::ShipBanking` in `ShipBanking.h`, a critically damped spring solved exactly over each frame. `GameClient::UpdateBanking` runs it each frame, and `PlaceModel` puts the bank in a ship's pose.
  - **The pose.** `ModelPose::bankRadians`, and `Outpost::PoseMatrix` moved to `Hardpoints.h` from `GameClient.cpp`, so that the hull, its crease lines, its hardpoints and its shards are placed by one rule.
  - **The data.** An optional `bank` on each hull in `Models.json` and a `constructorBank`; ADR-029 gives the first values and why.
  - **Tests.** `ShipBankingTests`, and new cases in `SnapshotInterpolatorTests`, `HardpointsTests` and `ModelCatalogTests`.
  - **Not built or run in the container**, which has no Windows: the banking math and the roll's direction were checked there against a standalone build and the DirectXMath matrix convention. CI builds Debug|x64 and runs the suites; the owner's run judges the look.

---

## Milestone 8 — The order ticks

### 8.1 — Measure where an order tick's time goes

- **Goal:** design §10. Know what costs 5.7–20.7 ms before changing it.
- **Scope:** time the parts of an order tick inside `Simulation` and `InProcessServer`: building the path graphs, the group's route, the per-ship searches, laying out the slots, the parting of ships, and the rest of the step. Log them with `--measure --load` alongside the tick times, and extend `Tools/FrameTimes.py` to summarize them. The timing is compiled into every build, as the tick timings are, and costs a few clock reads a tick.
- **Acceptance:** the summary lists each part's mean and worst over a run.
- **Verify:** CI; **owner run** of `--measure --load` in Release|ARM64 on the development machine. The breakdown is recorded in design §10 with how it was measured.
- **As built:**
  - **The parts.** `TickPart` and `TickTiming` in `GameProtocol/Server.h`: commands, and inside them each order's group route and ship paths; graph builds, wherever they happen; then fight, targets, economy, move, separate and vision in the simulation's order, and the snapshots. `Server::TakeTickTimings` replaces `TakeTickDurations` and gives each tick's total and its parts.
  - **No clock in the simulation.** ADR-009 keeps wall time out of `Simulation`, so `Simulation::Tick` takes an optional `TickObserver` and tells it where each part begins and ends, through `ObservedPart`, and the pathfinder tells it when it builds a graph. The observer is set for that one tick and cleared after, so no copy of a simulation holds one. `InProcessServer` passes its `TickProfiler`, which reads `std::chrono::steady_clock`, and times the snapshots itself. ADR-009 needs no change: nothing the observer learns reaches the state.
  - **The log and the summary.** `--measure` writes the part names once and a `tick_parts_ns` line after each `tick_ns`. `Tools/FrameTimes.py` prints each part's mean and worst over every tick and over the ticks over 5 ms.
  - **Tests.** `MovementTests.TellsItsObserverEachPartOfATick` checks the parts and their nesting with a recording observer, and that only the observed tick tells it; `MeasurementLoadTests` checks that every tick's snapshots and an order tick's ship paths are timed, nested inside their tick.
  - **Not built or run in the container**: the observer and the summary tool were checked there in a standalone build and on a sample log.

### 8.2 — Order ticks within 5 ms

- **Goal:** Q4's tick half met with the orders included: no tick over 5 ms at 200 ships and 40 structures on the development machine.
- **Scope:** what 8.1 finds. Design §10 names the ways open to it. Any part of an order that moves to later ticks is done within Q5's 150 ms, which Q5 is measured again to show.
- **ADR:** a new one: the change, superseding the parts of ADR-010 it changes.
- **Acceptance:** `MovementTests` and `PathfinderTests` still pass, and the Q2 check's tier-1 stage still gives the MVP's report: a change to when ships start moving can change a battle, and if it does, the change is reported and the owner decides.
- **Verify:** CI; **owner run** of `--measure --load` and Q5's measurement on the development machine. The figures go to design §10 and the MVP design's Q4 row is noted as superseded.
- **As built:** [ADR-032](../Design/ADR/ADR-032-order-ticks.md), which records the measurement and the figures.
  - **The measurement.** 8.1's parts, timed in a Linux build of the simulation with clang 18 and counted with callgrind, on task 2.7's load. The owner's breakdown from the development machine was not waited for: the container's order ticks are about a third of the MVP's on both the first and the later ticks, which is enough to say where the time goes. The breakdown is in ADR-032 and design §10.
  - **The graphs** are built when match setup is over (`InProcessServer::PreparePathfinding`, now called by `CreateInProcessServer`), and again on quiet ticks, one a tick, after a structure drops them (`Pathfinder::PrepareNext`).
  - **A large order** of more than `Simulation::SPLIT_ORDER_SHIPS`, 32, plans every other ship in its own tick and the rest in the next, and sets off then (`Simulation::PlannedOrder`, `Plan`, `PlanPaths`, `SetOff`, `FinishPlannedOrders`; `GroupRoutes` carries its routes over with `TakeRoutes`).
  - **A search** sorts the corners the start may see 64 at a time instead of heaping them all, and a ship passes over a route that cannot come within the detour limit (`GroupRoutes::ShortestJoinMeters`). Both are checked to give the same paths: the load ends in the same state, bit for bit, with and without them.
  - **Tests.** `MovementTests`: a large order planned over two ticks, held, then set off at one pace and arrived, a copy part-way equal to its original; a ship ordered again leaving its planning group; dropped graphs built on the quiet ticks after a structure, and an order then finding its graph built. Every `GameLogicTests` suite was also run in the container against a stand-in for the test framework: 164 passed.
  - **The Q2 check** was run in full in the container before and after, and passes all four criteria both times. Large armies set off a tick later, so the shares of what is worth building move by a few points; ADR-032 lists the two that move more. The tier-1 stage's verdicts are the MVP's; whether its shares may move is the owner's to decide.
  - **Not built or run on Windows in the container.** CI builds Debug|x64 and runs the suites. The owner's `--measure --load` run in Release|ARM64 is what closes the task and puts the development machine's figures in design §10.

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
- **ADR:** [ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md), superseding ADR-015's one size and ASCII-only text. It keeps ADR-015's atlas, quads, one draw call, and no shipped font.
- **Acceptance:** `GlyphAtlasTests` cover several faces and sizes in one atlas, the two non-ASCII characters, tracking, and the sprites.
- **Verify:** CI; **owner run**: every existing panel still reads, and text is sharp at 1920×1080 and 2880×1920.
- **As built:** [ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md).
  - **The atlas.** `Neuron::RasterizeFont`, `Neuron::DrawSprite` and `Neuron::PackGlyphs` make one texture of every font and sprite; `NextCodePoint` reads UTF-8. `UiPipeline` takes a list of fonts and sprites and a scale, and draws text in a font with tracking, sprites mirrored or not, and hatched rectangles, which the pixel shader stripes.
  - **The game's.** `Hud::Typefaces` and `Hud::Sprites`; `Hud::Text` names its typeface and tracking, Body by default, so the HUD draws as it did. In Debug the game writes to the debugger which face the figures found (H7).
  - **Hatching is a shader branch, not an atlas tile**, as this plan first said: a tile is a quad per tile, and the sampler clamps.
  - **Tests.** `GlyphAtlasTests` rewritten for several fonts and sprites, UTF-8, tracking and the family fallback; `HudTests.NamesItsTypefacesAndSprites`.
  - **Not built or run in the container**: the packing, UTF-8, tracking and sprites were checked there in a standalone build. DirectWrite and the shader are CI's and the owner's.

### 9.2 — Floating windows

- **Goal:** design §12.
- **Scope:** in `GameApp`, a window manager the HUD lays out through: windows with a title bar, a front-to-back order, dragging by the title bar, clamping to the screen with the title bar always reachable, bringing to the front on a click, closing with × and Esc, and each window's position kept in memory for as long as the process lives. Input focus tests the windows front to back, then the anchored HUD, then the world (ADR-015's rectangle test). Positions are held in reference units and clamped again when the screen's size changes.
- **ADR:** [ADR-031](../Design/ADR/ADR-031-floating-windows.md): the window manager and its focus order.
- **Acceptance:** `HudTests` cover dragging, clamping after a resize, the order of focus, Esc closing the front window, and a window reopening where it was closed.
- **Verify:** CI; **owner run.**
- **As built:** [ADR-031](../Design/ADR/ADR-031-floating-windows.md).
  - **The state.** `Outpost::WindowManager`: open windows front to back, where each was left, and the drag.
  - **The layout.** `Hud::Lay` takes the manager and lays each open window out last, back to front, kept on the screen by `Hud::KeepOnScreen`; the layout is drawn and clicked in layers, so a window covers what is behind it and a HUD button under one takes no click.
  - **The designer is the first window**, laid out as before inside a frame with a hatched title bar, a close box and corner brackets. Until 9.3 adds its button and key, selecting a built Shipyard opens it once.
  - **Esc** closes the front window and goes no further, so with a window open it no longer cancels a placement first; the designer's name field still has it first while typing.
  - **Tests.** `WindowManagerTests`; `HudTests` for the designer's window, its layers, a HUD button under it, and clamping at 1920×1080 and 1280×720. Esc and dragging by hand are the owner's run.
  - **Not built or run in the container**: the manager was checked there in a standalone build.

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
- **As built:**
  - **The snapshot.** A finished Shipyard takes its owner's next number and counts the warships it delivers (`EntityView::shipyardNumber`, `shipsBuilt`, the owner's only); a research topic names the hull, drive or weapon it unlocks (`ResearchTopicView::unlocksHull`, `unlocksDrive`, `unlocksWeapon`).
  - **The designer.** `Designer` keeps its target Shipyard, the lowest numbered by default and stepped by number round the end; the count, 1 to the target's free slots; and the first chip shown. `Load` picks a saved design's components. ×N sends N `QueueShipCommand`s in one frame, or, for picks that are no saved design, one save and N queues once it is saved (ADR-023).
  - **The content.** `Hud::DesignerPanel` is the mockup's content, described whenever GameClient gives a designer, which it does while the window is open: the header, the name and Save or Saved, the chips, a `SlotRow` of `PartCard`s per slot, six `StatBar`s against the best any combination of components reaches, a `DamageCard` per hull rated Good from two thirds of the best any design does to it and Fair from a third, the hint, Rename, the stepper and Queue. A hovered card's design fills the bars' and the damage cards' previews, each number marked better, the same or worse; lower is better for cost and build time.
  - **The layout.** `LayDesigner` in `Hud.cpp`, at the mockup's size in reference units: 728 wide and 704 tall with three weapons, a card line taller with five. The hatched header is 72 units, of which the top 36 are the title bar it is dragged by (ADR-031 unchanged); the Shipyard's arrows sit in that bar, and a press on a button there presses it rather than starting a drag. Figures against a right edge are placed by an estimated advance per character, because the layout is made without the fonts. A chip's name is cut to 20 characters.
  - **The look.** The mockup's colors, sampled from it and made linear, as the render target encodes to sRGB, for the designer and for every window's frame, title bar, close box and corners. Ore stays the HUD's gold outside the windows until 9.4. A seventh typeface, `Detail`, Cascadia Mono or Consolas at 11 units, sets a card's numbers.
  - **Opening it.** A selected finished Shipyard of the player's offers "Ship designer", which opens the window aimed at it; **D** opens or closes it, aimed at the selected Shipyard if one is. Selecting a Shipyard while it is open aims it there. The selection panel's Queue buttons per design stay until 9.4.
  - **The abbreviations are derived, not stored.** `Outpost::Abbreviation` in `GameProtocol` takes the capital initial of each word of a component's name, which gives exactly design §5's eleven, so that the designer and the match log read one rule and the abbreviation cannot drift from the name. This departs from the scope's field in `Tuning.json`; a component whose initials would clash or read badly gets the field then.
  - **Not done:** a locked card takes no click, so it is not under the pointer as a button is, and hovering it previews nothing.
  - **Tests.** `HudTests`: the mockup's content, figure by figure; the preview; ×N, Queue saving first, and Queue with no Shipyard; the layout with two parts locked, with every part unlocked and five weapons wrapping, with every part locked, and with more chips than fit; the open button. `DesignerTests`: the target, the count, loading and scrolling. `ProductionTests` and `ResearchTests` for the snapshot's new fields.
  - **Not built or run in the container**, which has no Windows. The HUD's and the designer's tests ran there in a standalone build against a stand-in for the test framework, and the layout was drawn from `Hud::Lay`'s output with substitute fonts and compared with the mockup. CI builds Debug|x64 and runs the suites; the owner's run closes H6.

### 9.4 — Research and production as windows

- **Goal:** design §12. The Research Lab's research and a structure's production queue move from their panels into floating windows.
- **Scope:** both open from their structure's selection panel and from a key, take the mockup's look, and keep what the panels do today (ADR-017, ADR-015).
- **Acceptance:** `HudTests` cover both.
- **Verify:** CI; **owner run.**
- **As built:** the owner's answers of 2026-10-03 are in design §12.
  - **The production window** shows one producer, held by `ProductionTarget`: the one selected when it opens, another selected while it is open, or the next or previous by its arrows, among the player's finished Command Stations and then Shipyards by number. It shows the queue as five rows, the front job's progress or its wait for Ore, and a card per thing it builds: the Constructor, or each saved design with its abbreviation. `Hud::DescribeProduction`.
  - **The research window** shows the player's Research Lab, its queue, and a card per topic not researched or queued yet with its effect, cost and time; one whose prerequisites are neither is hatched, dim, and names them. Ten cards show at once, two to a row, and it scrolls by a row (`Hud::StepTopics`), which 10.1's 25 topics need. `Hud::DescribeResearch`.
  - **The selection panel** keeps a structure's name, hit points and construction, and its buttons open the windows: Production on the Command Station and a Shipyard, Ship designer on a Shipyard, Research on the Research Lab. The queue's lines and the queue buttons are the windows' alone, so an enemy structure's queue, which the panel showed without fog of war, is no longer shown.
  - **Keys:** P and R open and close the windows, as D does the designer; opened by key, a window shows the structure selected, if it can.
  - **The look** is the designer's: the window helpers `LayDesigner` had are a `Painter` all three share, with the header band and the Ore box. Production stands at first under the Ore, 480 units wide, and research beside it, 560 wide, clear of the designer.
  - **Tests.** `HudTests`: the panel's open buttons, the production window at a Shipyard, at the Command Station and with neither, the research window with a topic blocked and with no Lab, and the three windows' layout and scrolling. `ProductionTargetTests`.
  - **Not built or run in the container**: the HUD's tests ran there in a standalone build, and the two windows were drawn from `Hud::Lay`'s output with substitute fonts.

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
- **As built:** [ADR-033](../Design/ADR/ADR-033-research-tiers.md).
  - **The schema.** A topic's `"tier"`, required; a gateway's `"effect": { "opensTier": n }`, read as `GatewayEffect`; and five targets with their one rate each: `allStructures` hit points, `structureWeapon` fire rate, `allShips` speed, `constructors` build rate and `asteroids` ore reserve. The loader checks the tiers: a gateway opens its own tier, a tier has one, every other topic of a later tier requires it, and no topic requires one of a later tier.
  - **Stacking.** `UpgradesFrom` adds each rate's percentages; the MVP's topics never stack, so tier 1 is unchanged.
  - **The simulation.** Structures are placed with their owner's hit points and raised, keeping their share, when the topic finishes; a structure weapon fires at its owner's rate; a design's speed and every ship's follow the speed upgrade; the Constructors build and repair at their owner's rate. The ore reserve waits in `Upgrades` for task 11.1.
  - **13 of the 17 topics** are in `Tuning.json`: 9, 12–18, 20, 21 and 23–25. Topics 10, 11, 19 and 22 name the Pulse Drive, the Flak Battery and the Rail Cannon, which the loader refuses before they exist, so they arrive with them in 10.2.
  - **The research window** lists topics tier by tier, shows each card's tier, and edges a gateway in gold.
  - **The Q2 checks.** Both the C++ check and `Tools/BattleModel.py` add percentages, read the new effects and leave out by name those a clump's battle cannot feel; their (d) stays over tier 1's topics until 10.3.
  - **Tests.** `TuningTests`: the new effects load, each tier rule is refused when broken, and the repository file's every member loads, `tier` included. `ResearchTests`: stacking of hulls and of a weapon's rate, and every new factor; a gateway gating its tier, queued with a topic of its tier, and changing no rate; Reinforced Structures on standing, new, enemy and test structures; Drive Harmonics on standing and new warships and Constructors, and Rapid Construction building faster. `HudTests`: the topics tier by tier and the gateway's card.
  - **Built and tested in CI** on Debug|x64; the container first ran the suites against a stand-in for the test framework. Not yet run.

### 10.2 — The Pulse Drive, the Flak Battery and the Rail Cannon

- **Gate:** H1, decided.
- **Goal:** design §5, in the game and in the model.
- **Scope:** the three components in `Tuning.json` with their abbreviations; the Flak Battery's splash through the Missile Rack's (ADR-014); their exhaust color (the Pulse Drive's) and their shots, as presentation data (ADR-019). The Pulse Drive's exhaust color is the owner's to pick at the run.
- **Acceptance:** `CombatTests` and `DesignTests` cover each.
- **Verify:** CI; **owner run**: whether the Pulse Drive's exhaust and the two weapons' shots read.
- **As built:** [ADR-034](../Design/ADR/ADR-034-shot-looks.md).
  - **The tuning data.** The Pulse drive (3), the Flak Battery (4) and the Rail Cannon (5), with design §5's numbers, and the four topics that waited for them: 10 Pulse Drive, 11 Flak Battery, 19 Rail Cannon and 22 Proximity Fuses. All 25 topics are in. Their abbreviations, **P**, **FB** and **RC**, come from their names as every component's do. The Flak Battery splashes through the Missile Rack's mechanism (ADR-014), which needed no change.
  - **The looks.** `Models.json` gives each weapon whose shot is not a tracer its look, the Lance's beam and the Rail Cannon's new slug, a white line that joins gun and target and lingers; the Flak Battery fires tracers with its splash ring. The Pulse Drive's exhaust is a provisional lime green, for the owner to pick at the run.
  - **The Q2 checks** stay on tier 1's components, the MVP's 18 designs, in both the C++ check, through `PartsThrough`, and `Tools/BattleModel.py`, until 10.3 gives tiers 2 and 3 their stages.
  - **Tests.** `DesignTests.DerivesThePhaseOneComponents`: each one's derived stats, its lock until its topic, its name and abbreviation. `CombatTests.FlakSplashesItsHits` and `ARailCannonOutrangesTheLance`, which also breaks a Pulse raider in one hit. `Q2CheckTests.FieldsTheGamesDesigns`: 45 designs, 18 through tier 1 and 36 through tier 2. `ModelCatalogTests.GivesEachWeaponItsShot` and the Pulse exhaust; `CombatEffectsTests.ASlugJoinsGunAndTargetAndLingers` and `AWeaponNotListedFiresTracers`.
  - **Design §6's tier 2 is 890 s, not 900 s**, by its own table, so the total is 2,590 s; still about 43 minutes. §6 now says so; the table's times are unchanged.
  - **Built and tested in CI** on Debug|x64; the container first ran the suites against a stand-in for the test framework. Not yet run.

### 10.3 — The Q2 check per tier, and tuning against it

- **Gate:** H4, decided.
- **Goal:** design §7. P3.
- **Scope:** `Q2Check` gains the stages of design §7: starting, tier 1, tier 2 and tier 3, each with its components and budgets, and (d) per tier. Run it, tune the tier-2 and tier-3 numbers against it as B.1 tuned §12, and record each number that moved and why in design §5 and §6. Tier 1's numbers move only if a later tier's result requires it, and the owner decides that.
- **Acceptance:** `Q2CheckTests.TheFullCheck` passes all four criteria at every stage, in the Linux container; `TheRecordedCountersHold` gains a counter per new component.
- **Verify:** CI; the owner runs the check of record in Release|ARM64 on the development machine, and its time is recorded.
- **As built so far:**
  - **The staged check.** `RunQ2Check` plays design §7's stages: tier 1 at 2,000–12,000 Ore, the starting components at 2,000–4,500, tier 2 at 4,500–12,000 and tier 3 at 6,000–12,000, each with the components through its tier and no upgrades. (d) runs per tier: a topic of tier 1 against the starting designs at the starting budgets, as the MVP's; a topic of a later tier at its tier's budgets against every design and upgrade of the tier before, the researching side having those too. Only the designs a topic adds or changes are tested, since one the other side has with the same numbers is not the topic's. (b) reports the Pulse Drive when no design worth building uses it and does not fail on it. `CheckOptions::lastTier` limits the stages; `Tools/BattleModel.py` runs the same stages.
  - **What it finds with the 10.2 numbers**, run in the container at 20 battles a pairing without (c):
    - **(a) holds** at tiers 2 and 3: every design has a counter.
    - **(b) fails on the Flak Battery**, at every budget of tiers 2 and 3. It is all or nothing: it beats every Small design 20–0 and loses 0–20 to every Medium and Large one, and a Small design is never most of the mix. Its 20 m splash also reaches nobody: the formation stands Small hulls 24 m apart (three footprint radii, ADR-010). A 26 m splash, or a hit of 12 or 14 at 60 Ore, still loses 0–20 to every Medium design and is still never worth building.
    - **(b) fails on the Mass Driver** at tier 3 in both fire modes and at tier 2 under focus fire. This is not new: with the MVP's numbers no design with a Mass Driver was worth building under focus fire at 4,500 Ore or more, and under spread fire only S+I+MD at 7% at 12,000. Tier 1 passes (b) on it at its smaller budgets, which tiers 2 and 3 do not have.
    - **(d) fails on Ablative Armour**: M+I+La with +45% hit points against the tier 2 designs with +30% has no answer better than 37% at 6,000 Ore under focus fire.
  - **The owner's answers** (2026-10-03): H9, (b) at tiers 2 and 3 judges only the components the tier adds, which both checks now do; H10, the Flak Battery gets a role against the Medium hull, which the tuning gives it.
  - **The tuning**, recorded with its reasons in design §5 and §6: the Flak Battery to 20 a hit, 200 m, a 26 m splash and 60 Ore, and Ablative Armour to +5%. Run in the container at 12 battles a pairing without (c), (a) and (b) then hold at tiers 2 and 3, the Flak Battery and the Rail Cannon each worth building at every one of their budgets; and at +5% the picket answers the plated Lance line in 95–100% of battles at every tier 3 budget.
  - **The recorded counters** gain one per new component, at its tier's smallest budget: Medium+Ion+Flak beats the swarm at 4,500 Ore, a Pulse picket beats the heavy brawler at 4,500, and a Rail Cannon heavy line beats the heavy Lance line at 6,000.
  - **The check of record in the container passes all four criteria at every stage**: 60 battles a pairing, up to 480 when uncertain, with (c)'s 3,604 counter checks under one-number changes of ±5%, in 69 minutes on four threads. The owner's run in Release|ARM64 on the development machine, and its time, remain (design §7).

### 10.4 — The AI on tiers and new designs

- **Goal:** design §13: the AI researches the 25 topics and counters the new designs. It is not made harder to rush (design §3).
- **Scope:** `Opponent.json`: its research order through all three tiers, and counters for and with the new designs, from 10.3's results. `AiSettingsTests` checks every identifier against `Tuning.json`, as now.
- **Acceptance:** `AiPlayerTests`: the AI reaches tier 3 against a passive player, and answers a Flak, a Rail and a Pulse design.
- **Verify:** CI.
- **As built:**
  - **The research order** runs through all 25 topics: tier 1 as before, then Relay Archives and the Flak Battery first, the Pulse Drive, the upgrades of the weapons it fields and the structures; then Precursor Vault and the Rail Cannon first. Every topic comes after its prerequisites, which `AiSettingsTests` checks against `Tuning.json`.
  - **The counters**, from task 10.3's battles: the Flak Battery is the first answer to the swarm, the brawler, the picket and every Pulse raider once the AI has it; the Rail Cannon to the heavy Mass Driver and Lance lines. The Medium Missile Rack answers the Small Flak Battery, the heavy Lance line the Medium and Large ones, and the picket every Rail Cannon design. An answer the AI has not unlocked falls through to the MVP's, as before, so nothing changes until tier 2.
  - **Tests.** `AiPlayerTests.AnswersThePhaseOneDesigns`, and `ReachesTierThree`: against a player who does nothing, the AI opens tier 3 at tick 37,200, 31 minutes in, in the Linux container. It is the longest test in CI, about four minutes in Debug, and stays there (owner, 2026-10-03): it is what caught depletion starving the AI's research.

---

## Milestone 11 — The map

### 11.1 — Ore reserves and depletion

- **Gate:** H3, decided.
- **Goal:** design §8, ore that runs out.
- **Scope:** each ore asteroid's reserve and the trickle in the map data and the tuning data; a rig drawing its reserve down; Improved Extraction draining faster and Deep Core Survey adding to what is left; the reserve in the snapshot for an asteroid in sight, and remembered otherwise (ADR-024); the figure in the selection panel and a mark on the minimap for an exhausted asteroid.
- **ADR:** a new one: depletion and what the snapshot carries of it.
- **Acceptance:** `EconomyTests` cover a reserve running out to the trickle, both upgrades, and the fog's remembered figure.
- **Verify:** CI; **owner run** of the panel and the minimap.
- **As built:** [ADR-035](../Design/ADR/ADR-035-ore-depletion.md).
  - **Data.** `Map.json`'s ore asteroids each state a `"reserve"`, required: 7,500 Ore at home and 12,000 contested on today's map, as design §8's rings. `Tuning.json`'s rules give the trickle, `"exhaustedYieldPercent": 20`.
  - **The draw.** A built rig draws from its asteroid what it earns, divided by its owner's reserve factor, carried in hundredths as income is; a dry asteroid's rig earns a fifth of its rate and draws nothing. An asteroid placed without a reserve, as tests place them, never runs out.
  - **What a player sees.** Under fog of war each player keeps every asteroid's reserve as it last saw it; the snapshot gives it on the asteroid and on the rig standing on it. A rig's panel reads "Ore left 3,420", or "Ore run out: it earns a trickle", and the minimap draws a dry asteroid in a dark rust.
  - **Tests.** `EconomyTests.AnAsteroidRunsDryToATrickle`, `ResearchChangesHowFastAReserveDrains` and `RemembersTheReserveItLastSaw`; `MapTests` and `TuningTests` for the new members; `HudTests.ShowsTheOreLeft`.

### 11.2 — The 5 km map

- **Gate:** H3, decided.
- **Goal:** design §8, the map.
- **Scope:** `Map.json` at 5,000 m with the four rings, point-symmetric, every passage at least the minimum gap. `MapTests` check both. The layout is in sectors for Phase 2 (design §8; owner, 2026-10-03): about nine, each with a node site, and the map data names them, their node sites and their adjacency, which the game reads nothing of yet. `MapTests` check that adjacency is symmetric, every asteroid lies in one sector and every sector can be reached from both starts. The fog of war's grid, the minimap and the path graphs at the new size are checked against Q4: a `--measure --load` run on the new map is part of the owner run.
- **Acceptance:** `MapTests`; `PathfinderTests` on the new map.
- **Verify:** CI; **owner run**: the layout confirmed by the owner, as MVP task 2.3's was, and its first matches decide H8.
- **As built:** [ADR-036](../Design/ADR/ADR-036-map-sectors.md).
  - **The map.** 5,000 m, the starts at (−1,750, −1,750) and (1,750, 1,750), 4.95 km apart. Nine sectors on a grid of thirds: each home in its corner with its three home asteroids; each player's flanks with its four near ones, two in each; the six contested round the center; two rich in each empty corner, one nearer each start. 22 asteroid fields stand on the sector borders and at their junctions. 24 asteroids, 285,000 Ore, as design §8.
  - **Rings and sectors in the data.** The yields `"near"` and `"rich"` join `"home"` and `"contested"`, with their rates in `Tuning.json`. `"sectors"` names each sector's rectangle, node and adjacency, which the loader checks and Phase 1 does not read.
  - **The client** lets the camera's focus go 2,500 m out instead of 1,000, and draws the ground's grid over 5 km.
  - **The AI on the bigger map.** Its attack-move left its fleet standing in formation out of its guns' reach of the Command Station it had come for, so its attack group's ships within 500 m of their target structure are ordered to attack it. It then beats a player who does nothing at 7.8 minutes, where it took 5.4 on the 2 km map.
  - **Tests.** `MapTests`: the repository map's rings and reserves (`TheRepositoryMapHasPhaseOnesShape`), its sectors (`TheRepositoryMapIsLaidOutInSectors`), and the loader's refusals of broken sectors; the reachability, symmetry and pathing tests run on the new map unchanged. `AiPlayerTests`' figures follow the new map's rings, and the defence test is placed from the AI's start.

### 11.3 — The AI follows the ore

- **Goal:** design §13: the AI builds on the nearest asteroids with ore left and moves on when one runs dry.
- **Scope:** `AiPlayer` and `Opponent.json`: rings rather than a fixed count of home and contested asteroids.
- **Acceptance:** `AiPlayerTests`: an AI whose home reserves run out builds further out.
- **Verify:** CI.
- **As built:**
  - **The AI keeps its rigs on ore.** `homeAsteroids` and `contestedAsteroids` now count together the rigs it keeps on asteroids with ore left. When one runs dry, it plans a rig on the nearest asteroid it knows still holds ore, not the enemy's home, with a Defence Platform beside it, as a contested one; the dry rig stays for its trickle. An asteroid it has not seen counts as holding ore.
  - **Tests.** `AiPlayerTests.FollowsTheOre`: with its home reserves at 200 Ore, after eight minutes the AI keeps three dry rigs and six on ore, each away from home with a platform. `ReachesTierThree` depends on it: without it the AI's income fell to the trickle at minute 21 and its Shipyards took every Ore the lab waited for.

---

## Milestone 12 — The win condition

### 12.1 — Losing all production

- **Gate:** H5, decided.
- **Goal:** design §4.
- **Scope:** the rule in `Simulation`: a player loses when it has no Command Station and no finished Shipyard. H5's answers: whether a lost Command Station stays lost, and the reveal of the last Shipyards through fog of war. The AI attacks production first (design §13). The banner is unchanged.
- **ADR:** a new one, superseding ADR-020's decision 8.
- **Acceptance:** `MatchOutcomeTests`: losing the Command Station alone does not end the match; losing it and the last finished Shipyard does; a Shipyard under construction does not count; the reveal. `AiPlayerTests`: the attack group goes for a Shipyard first.
- **Verify:** CI; **owner run.**
- **As built:** [ADR-037](../Design/ADR/ADR-037-losing-all-production.md). It supersedes ADR-020's decision 9, the match's end, which this entry named as decision 8, and the attack in ADR-024's decision 8.
  - **The rule.** A player whose base was placed loses when it has neither a Command Station nor a finished Shipyard. A lost Command Station needed no change to stay lost: the tuning data gives it no cost, so no Constructor can build one.
  - **The reveal.** Under fog of war, each finished Shipyard of a player without a Command Station is kept among its opponents' remembered structures, refreshed every tick, so they see it and can order an attack on it.
  - **The AI.** Its attack group goes for Shipyards, then the Command Station, then the rest, nearest first in each, and turns to a Shipyard that comes to light. Without its Command Station it plays on, apart from queuing Constructors. It still beats a player who does nothing at 7.8 minutes, and one who holds the middle at 9.0.
  - **Tests.** `MatchOutcomeTests.TheLastShipyardKeepsAPlayerIn`, `AShipyardSiteDoesNotCount` and `ALostCommandStationIsLostForGood`; `AiPlayerTests.AttacksProductionFirst` and `PlaysOnWithoutItsStation`.

---

## Milestone 13 — Measuring Phase 1

### 13.1 — The match log for Phase 1

- **Goal:** the figures P1, P2 and P4 need (design §2, §10).
- **Scope:** `MatchLog` records each gateway topic as it finishes, each side's warship count every 30 s and its peak, and each asteroid as it runs dry. `Tools/MatchLog.py` reports the match's length against 45–60 minutes, the designs built in each tier, and the peak ship count. A switch runs 10 seeded AI-against-AI matches on the real server headlessly and summarizes their lengths.
- **Acceptance:** `MatchLogTests`.
- **Verify:** CI.
- **As built:** [ADR-038](../Design/ADR/ADR-038-phase-one-match-log.md).
  - **The log** adds a `tier` record after each gateway, each player's `fleet` every 30 seconds from its own snapshots, a `dry` record as each asteroid runs out, and each player's `peak` as the match ends or is left.
  - **The switch** is the executable's `--ai-matches`, since only the executable may include both the AI and the log. It steps the server a tick at a time through a new `Server::Step`, so each match reproduces from its seed, plays seeds 1 to 10 on every core to their end or 120 minutes, and writes `OutpostCommander-ai-matches.log` in the temporary folder. `Tools/MatchLog.py --ai-matches` gives each match and their lengths' median and spread; for every log it gives the tiers, the peak, and the warships built in each tier.
  - **What it found in the container**, for 13.2 to weigh: two AIs end a match in 22 minutes, the median of 40 seeds, from 19:37 to 32:18, and none in P1's 45 to 60. They open tier 2 at 15:04 and never reach tier 3. Player 1 wins 30 of the 40 with the same AI in both seats; the cause is not the order the server applies their commands, and is not yet known.
  - **Tests.** `MatchLogTests.RecordsTiersFleetsAndDryAsteroids`, with the two older tests' expected logs extended; `InProcessServerTests.StepsOneTickAtOnce`.

### 13.2 — P1–P5

- **Goal:** answer design §2's questions and record them there. A failed answer is still a result.
- **Scope:** the owner plays matches without rushing the AI; the AI-against-AI run; the Q4 measurement on the 5 km map at the peak ship count 13.1 reports; the owner's judgement of the designer, the windows and the banking.
- **Verify:** **owner run.** The answers decide whether and how Phase 2 goes on to two hours (design §1).
