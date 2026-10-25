# Outpost Commander — Findings of the Design Reference

Status: **open** · gates F1–F16 decided by the owner on 2026-10-25, F16 carried from the reconciliation's gate R7 · From the as-built Game Design Reference of 2026-10-25, which describes the game at `a053fe2`. The reference is tagged [`design-reference-2026-10-25`](https://github.com/Zwaliebaba/Outpost.Commander/tree/design-reference-2026-10-25/DesignReference) and published as a Claude Design canvas.

The reference read every design document, ADR, data file and the code, and drew every screen from `Hud.cpp`'s layout. It found 68 conflicts and 79 gaps. Most conflicts are an earlier document that a later one already overrides, or an ADR that should say what the code does; they need no decision. Fifteen needed the owner. This plan writes those decisions down before the code that follows them, and puts the work in order.

This plan is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed. The evidence for each finding is in the reference: [`conflicts.md`](https://github.com/Zwaliebaba/Outpost.Commander/blob/design-reference-2026-10-25/DesignReference/_work/conflicts.md) (C-nnn) and [`gaps.md`](https://github.com/Zwaliebaba/Outpost.Commander/blob/design-reference-2026-10-25/DesignReference/_work/gaps.md) (G-nnn).

---

## How an agent uses this plan

1. **Read AGENTS.md, then the ADRs and the finding the task names.**
2. **Take the lowest-numbered task whose status is `todo` and whose dependencies are `done`.** One PR per milestone (owner, 2026-09-30), in the milestone order below.
3. **ADRs are edited in place** (AGENTS.md §6), in the same commit as the change that implements them.
4. **A task that touches rendering, input or presentation is run, not only built**, and the owner's run accepts it.

Task numbers carry the prefix RF, and gates are lettered F.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| RF1.1 | The decisions that need no code, in their ADRs | — | F4, F6, F7, F8 | done in the PR that adds this plan |
| RF2.1 | The designer reserves a row for each bar it draws | — | F3 | merged, [#116](https://github.com/Zwaliebaba/Outpost.Commander/pull/116); awaiting the owner's run |
| RF2.2 | The Controls window says what a click selects | — | F9 | merged, [#116](https://github.com/Zwaliebaba/Outpost.Commander/pull/116); awaiting a run |
| RF2.3 | The Orders window's Give lights under the pointer | — | F13 | merged, [#116](https://github.com/Zwaliebaba/Outpost.Commander/pull/116); awaiting a run |
| RF2.4 | A click selects another player's structure, for its panel | — | F16 | merged, [#116](https://github.com/Zwaliebaba/Outpost.Commander/pull/116); awaiting the owner's run |
| RF3.1 | The HUD draws each side in its own color | — | F1 | todo |
| RF4.1 | A Research Lab and a Defence Platform need a held sector | — | F5 | todo |
| RF5.1 | A refused command is told to the player | — | F10 | todo |
| RF5.2 | A waiting scheduled order can be cancelled from the Orders window | RF5.1 | F11 | todo |
| RF5.3 | No away report on a seat's first join | — | F12 | todo |
| RF5.4 | Joining a world says it is connecting | — | F14 | todo |

The owner's track, beside the plan:

| Item | What | Gate | Status |
|---|---|---|---|
| RF-O1 | Rework the Human Command Station's L1–L5 art toward a square plan, and bake it again | F2 | done from the owner's model sheet; awaiting the owner's run |
| RF-O2 | Remove the Mining Rig's `gun` hardpoint, and bake it again | F8 | todo |
| RF-O3 | Look at the designer's Large hull card at 1080p with Cascadia Mono installed | F15 | todo |

### Milestone order

RF1, RF2, RF3, RF4, RF5.

- **RF1 changes no code.** It lands with this plan. F11's rule needs no edit, because ADR-080 decision 5 already states it.
- **RF2 is four small interface fixes,** each a test and a few lines.
- **RF3 changes what every minimap shows,** so it is a milestone of its own and an owner run.
- **RF4 changes a rule the AI plays by.** The AI must place by it too.
- **RF5 is the protocol and the world:** a refusal message, a cancel command, the first join and an asynchronous join. `PROTOCOL_VERSION` stays 1 until the game is live (AGENTS.md R18).

---

## Gates

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| F1 | **The HUD's side colors.** The minimap's marks, washes and outlines, and the territory panel's figures, are drawn in fixed own and enemy colors (`Hud.cpp:26-30`). The world draws each side in its `Models.json` color by player number. In a world, seat 2 can be a human's, and that player sees its own ships red-orange in the world and blue on the minimap. **Decided on 2026-10-25:** the HUD follows the sides. Each side keeps its `Models.json` color everywhere. | C-001 | RF3.1 | decided 2026-10-25 |
| F2 | **The Human Command Station's size.** Its art is about twice as deep as it is long. Level 1 is fitted by its length alone, so it is drawn about 90 m by 181 m on its 90 m footprint at every level. [ADR-064](../ADR/ADR-064-structure-upgrades.md) assumed a square level 1. **Decided on 2026-10-25:** the owner reworks the mesh. No code change. | C-002 | RF-O1 | decided 2026-10-25 |
| F3 | **The designer's seventh bar.** `ExtentOf` reserves six bar rows (`Hud.cpp:1087`), but the designer draws seven whenever a module has sight, and the Sensor Array always has. So the Build row runs about 13 units into the footer. This is derived from the code; nobody has seen it on screen. **Decided on 2026-10-25:** reserve a row for each bar drawn. | C-003 | RF2.1 | decided 2026-10-25 |
| F4 | **A lit footprint ring's color.** The code draws it in the side's ship-line color. [ADR-042](../ADR/ADR-042-faint-footprint-lines.md) decision 2 says the structure's own line color. **Decided on 2026-10-25:** the code wins, and ADR-042 is rewritten in place. | C-004 | RF1.1 | decided 2026-10-25 |
| F5 | **Where a Research Lab and a Defence Platform may stand.** A Shipyard and a Repair Bay need a sector the player holds. A Research Lab and a Defence Platform need nothing, so Defence Platforms can creep into an enemy's sectors. **Decided on 2026-10-25:** both need a held sector. | G-005 | RF4.1 | decided 2026-10-25 |
| F6 | **A Mining Rig in a suppressed sector.** It may be placed, and earns nothing until the suppression lifts. **Decided on 2026-10-25:** it may, and [ADR-056](../ADR/ADR-056-territory.md) says so. | G-004 | RF1.1 | decided 2026-10-25 |
| F7 | **The Shipyard's L4–L5 and the Research Lab's L5 art.** The data stops at L3 and L4 by Phase 3's gate K3, so these levels are loaded and never drawn. **Decided on 2026-10-25:** keep them for content that needs them, and [ADR-064](../ADR/ADR-064-structure-upgrades.md) says so. | G-014 | RF1.1 | decided 2026-10-25 |
| F8 | **Gun hardpoints with no gun.** The Mining Rig's mesh carries a `gun` hardpoint and the rig has no weapon. The Defence Platform carries six for its one Defence gun. **Decided on 2026-10-25:** the rig's is removed in the art. The platform's six are its muzzles, because a shot leaves from the gun hardpoint nearest its target (`Hardpoints.cpp:206-220`); [ADR-018](../ADR/ADR-018-nmf-and-hardpoints.md) says so. | G-015 | RF1.1, RF-O2 | decided 2026-10-25 |
| F9 | **Selecting an enemy's ship.** The Controls window reads "Select a ship, or a structure of yours", but a click picks only the player's own ships. **Decided on 2026-10-25:** the player's own only, and the line is reworded. | G-023 | RF2.2 | decided 2026-10-25 |
| F10 | **A refused command.** The server tells the player nothing when it refuses a command (`InProcessServer.cpp:482`). A scheduled order refused when given never appears. **Decided on 2026-10-25:** the server answers a refusal with its reason, and the HUD shows it. | G-025 | RF5.1 | decided 2026-10-25 |
| F11 | **Cancelling a scheduled order.** A ship waits on at most one scheduled order, and any direct order takes it out of that order. An order with no ships left is deleted (`Simulation.cpp:3240-3253, 3305-3320`), so the orders are bounded by the ships. [ADR-080](../ADR/ADR-080-events-and-scheduled-orders.md) decision 5 already says so, which the reference's gap G-024 missed. The Orders window has no way to cancel one. **Decided on 2026-10-25:** no new cap. The window gains a cancel and says that a direct order also leaves a scheduled one. | G-024 | RF5.2 | decided 2026-10-25 |
| F12 | **An away report on the first join.** A seat nobody has taken is its deputy's from tick 0, so a player's first join of a new world opens a report of everything since the world began. **Decided on 2026-10-25:** no report on the first join. | G-028 | RF5.3 | decided 2026-10-25 |
| F13 | **Hover in the windows.** Only the HUD's own controls light under the pointer ([ADR-088](../ADR/ADR-088-selection-and-commands.md)). The Orders window's Give is drawn like a HUD button but never lights. Nothing has a pressed state. **Decided on 2026-10-25:** Give lights. The windows otherwise stay as ADR-088 says, with no pressed state. | G-036, G-039 | RF2.3 | decided 2026-10-25 |
| F14 | **A loading screen and a connecting sign.** The window is hidden until the game has loaded, as [ADR-049](../ADR/ADR-049-startup-in-parallel.md) chose. Join world freezes the menu for up to 10 s with no sign. **Decided on 2026-10-25:** keep the hidden startup. The join becomes asynchronous, with a connecting line on the menu. | G-034 | RF5.4 | decided 2026-10-25 |
| F15 | **The Large hull card's numbers.** "1,200 HP · ARM 14 · 25m/s" may be cut short at 1080p in Cascadia Mono 13, against [ADR-062](../ADR/ADR-062-type-scale-and-contrast.md) decision 4. It is unproven, because the metric could not be checked without the font. **Decided on 2026-10-25:** the owner checks it on Windows. A fix is a task only if the run shows it. | C-005 | RF-O3 | decided 2026-10-25 |
| F16 | **Another player's structure, selected.** `Hud::Describe` lays out a remembered structure's panel, "Last seen 4:12 ago" and what was seen then, but `PlayerControls` picks only the player's own ships and structures (`PlayerControls.cpp:263-275`), so no click opens it ([ADR-081](../ADR/ADR-081-territory-and-memory-over-the-fog.md) decision 4). Gate V3 of [the second interface plan](ImplementationPlan-Interface2.md) put the age on the panel. **Decided on 2026-10-25,** as the reconciliation's gate R7: the panel is made reachable. A click on another player's structure, seen or remembered, selects it alone, for a panel with no buttons that takes no orders. | the reconciliation, R7 | RF2.4 | decided 2026-10-25 |

---

## Milestone RF1 — The decisions that need no code

### RF1.1 — The decisions that need no code, in their ADRs

- **Gates:** F4, F6, F7, F8.
- **Scope:** each ADR is edited in place.
  - [ADR-042](../ADR/ADR-042-faint-footprint-lines.md) decision 2: a lit ring takes the side's ship-line color.
  - [ADR-056](../ADR/ADR-056-territory.md): a Mining Rig may be placed in a held sector while it is suppressed, and earns nothing until the suppression lifts.
  - [ADR-064](../ADR/ADR-064-structure-upgrades.md): the Shipyard's L4–L5 and the Lab's L5 art are loaded and never drawn, and wait for content.
  - [ADR-018](../ADR/ADR-018-nmf-and-hardpoints.md): a model's gun hardpoints are its muzzles, and a shot leaves from the nearest. An unarmed model carries none, so the Mining Rig's goes (RF-O2).
- **Verify:** the checkers; no build is needed.

---

## Milestone RF2 — Four small interface fixes

### RF2.1 — The designer reserves a row for each bar it draws

- **Gate:** F3.
- **Scope:** `ExtentOf` counts the bars `DescribeDesigner` makes, so the footer starts below the last. With Tuning.json the window grows from 913 to 938 units. It still fits a 1080-line screen and still reaches 110 % ([ADR-070](../ADR/ADR-070-interface-scale.md)).
- **ADR:** [ADR-058](../ADR/ADR-058-modules.md) decision 7 and [ADR-062](../ADR/ADR-062-type-scale-and-contrast.md)'s figures, edited in place.
- **Acceptance:** `HudTests`: no bar row meets the footer, with and without a module that has sight.
- **Verify:** CI; run; owner run.

### RF2.2 — The Controls window says what a click selects

- **Gate:** F9, and F16, whose click it names.
- **Scope:** `KeyBindings.h:95` says that a click selects one of the player's own ships, or any structure, its own or another player's (RF2.4), in words to that effect.
- **Verify:** CI; the Controls window in a run.

### RF2.3 — The Orders window's Give lights under the pointer

- **Gate:** F13.
- **Scope:** `Hud.cpp:1979` passes the hovered flag, as the HUD's own buttons do.
- **ADR:** [ADR-088](../ADR/ADR-088-selection-and-commands.md)'s Consequences name Give.
- **Acceptance:** `HudTests`: Give is drawn lit when its action is under the pointer.
- **Verify:** CI; run.

### RF2.4 — A click selects another player's structure, for its panel

- **Gate:** F16.
- **Scope:**
  - A left-click on none of the player's own ships or structures, but on another player's structure, seen or remembered, selects that structure alone (`PlayerControls.cpp:263-275`). A box still takes the player's own ships only, and Shift adds none of another player's structures to a selection.
  - Its panel is the one `Hud::Describe` lays out: a seen structure's kind, level and hit points; a remembered one's "Last seen 4:12 ago" and what was seen then. It offers no buttons, opens no window, and gives no production line ([ADR-066](../ADR/ADR-066-production-status-on-the-hud.md) decision 4).
  - A right-click with it selected gives no order, and it takes none.
  - It leaves the selection when it is destroyed while seen, or its memory is forgotten.
- **ADR:** [ADR-013](../ADR/ADR-013-client-view-and-controls.md) decision 5 and [ADR-081](../ADR/ADR-081-territory-and-memory-over-the-fog.md) decision 4, edited in place, and Game §interface.hud.selection.
- **Acceptance:** `PlayerControlsTests`: a click on another player's structure, seen or remembered, selects it alone, and a box does not; `HudTests`: its panel offers no button.
- **Verify:** CI; run, selecting an enemy's seen and remembered structures; owner run.

---

## Milestone RF3 — Each side in its own color

### RF3.1 — The HUD draws each side in its own color

- **Gate:** F1.
- **Scope:**
  - The minimap's marks, sector washes, outlines and hatching, its remembered crosses, and the territory panel's figures take each side's color from `Models.json`, through what the client already loads (`ModelCatalog`).
  - The pirates and the derelicts stay in their own colors.
  - Every figure keeps at least ADR-062's contrast on the window's navy.
  - The scene's colors do not change.
- **ADR:** [ADR-043](../ADR/ADR-043-hud-in-the-windows-look.md) decision 7 and [ADR-085](../ADR/ADR-085-one-meaning-per-color.md), edited in place; a new decision if the side colors need a HUD shade of their own to keep the contrast.
- **Acceptance:** `HudTests`:
  - the marks of player 1 and player 2 are drawn in their sets' colors, whichever seat views them;
  - every figure's contrast is at least ADR-062's floor.
- **Verify:** CI; run as seat 1 and as seat 2 of a world; owner run.

---

## Milestone RF4 — Where a Research Lab and a Defence Platform may stand

### RF4.1 — A Research Lab and a Defence Platform need a held sector

- **Gate:** F5.
- **Scope:**
  - The server refuses a Lab or a Defence Platform outside a sector the player holds, as it refuses a Shipyard and a Repair Bay (`Simulation.cpp:2113-2133`).
  - The placement ghost shows it invalid.
  - The AI and the deputy place them only where they hold ([ADR-041](../ADR/ADR-041-ai-plays-a-longer-match.md), [ADR-076](../ADR/ADR-076-ai-plays-phase-4.md)).
  - On a map without sectors, nothing changes.
- **ADR:** [ADR-056](../ADR/ADR-056-territory.md), edited in place.
- **Acceptance:**
  - `GameLogicTests`: a Lab and a Defence Platform are refused outside a held sector and placed inside one.
  - The AI-against-AI matches of [ADR-065](../ADR/ADR-065-ai-difficulty.md) still end within their bounds.
- **Verify:** CI; `--ai-matches` against the last figures; owner run.

---

## Milestone RF5 — The protocol and the world

### RF5.1 — A refused command is told to the player

- **Gate:** F10.
- **Scope:**
  - The server answers a command it refuses with the refusal's reason, to the client that sent it.
  - The HUD shows it, as an alert or a hint, in words a player reads: "Not enough Ore", "Needs Shipyard L2", "Not in a held sector", and so on.
  - A scheduled order refused when given is told the same way.
- **ADR:** a new one, for the message and the words. It rewrites what [ADR-060](../ADR/ADR-060-quic-in-process.md) and [ADR-080](../ADR/ADR-080-events-and-scheduled-orders.md) say of refusals.
- **Acceptance:** `GameProtocolTests` for the message, and `GameLogicTests`: each refusal reaches the client that sent it, and only that one.
- **Verify:** CI; run against a dedicated server.

### RF5.2 — A waiting scheduled order can be cancelled from the Orders window

- **Gate:** F11.
- **Scope:**
  - A command cancels a seat's scheduled order by its id.
  - The Orders window gives each listed order a way to cancel it, and says that giving its ships an order also takes them out.
- **ADR:** [ADR-080](../ADR/ADR-080-events-and-scheduled-orders.md), edited in place.
- **Acceptance:** `GameLogicTests`: a cancelled order is gone and its ships are free; `HudTests`: the control is laid out for each listed order.
- **Verify:** CI; run.

### RF5.3 — No away report on a seat's first join

- **Gate:** F12.
- **Scope:** the away report starts only once a player has held the seat (`AwayReports.cpp`, `SeatController.cpp`).
- **ADR:** [ADR-079](../ADR/ADR-079-seat-controller-and-deputy.md), edited in place.
- **Acceptance:** `GameLogicTests`: a seat's first join carries no report, and a later return does.
- **Verify:** CI; a new world joined, left and joined again.

### RF5.4 — Joining a world says it is connecting

- **Gate:** F14.
- **Scope:**
  - Join world no longer blocks the frame loop (`WinMain.cpp:349-358`, `QuicTransport`).
  - The menu shows a line such as "Connecting to {server}…" until the welcome comes or the join fails, which keeps today's notice.
  - The hidden startup is unchanged.
- **ADR:** [ADR-078](../ADR/ADR-078-dedicated-server.md) decision 9 and [ADR-086](../ADR/ADR-086-where-the-match-runs.md), edited in place.
- **Acceptance:** `GameAppTests`: the menu lays out the connecting line.
- **Verify:** CI; run against a dedicated server that answers and one that does not.

---

## Not in this plan

- **The other conflicts.** Of the 68 conflicts, 32 are an ADR or a document that should say what the code does, and 28 are already superseded. Correcting the ADRs is documentation, not a decision; it can be its own PR.
- **The other gaps.** Most of the 79 are facts no document states, which the reference states from the code. The world's bank sink (G-003) stays Phase 6's, as gate H9 left it.
- **The reference itself.** It is a snapshot of `a053fe2`, kept on its tag, and is not merged.
