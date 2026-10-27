# Outpost Commander — Canvas Plan: One Toolkit for the Interface

Status: **open** · accepted by the owner on 2026-10-27, with gates C1–C7 decided that day · From the owner's request of 2026-10-27 for a generic canvas layer that gives the game its windows, buttons, drop-downs and other components, and a review of the interface's code made that day against `main` at `94d6aa0`

The interface is drawn well and tested well, and costs too much to add to. Every window is placed by hand in reference units, works out its own size apart from its layout, and threads hover and focus through its parameters. This plan puts a canvas under the interface: a layout that measures, interaction that every widget shares, and a set of widgets in the windows' look. The windows and the HUD are then moved onto it one at a time, each without changing what the screen shows. Before the canvas, window places start to last across restarts and the Constructors' build buttons become a window of their own; after it, the designer scrolls and the orders form takes drop-downs.

It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then [ADR-015](../ADR/ADR-015-ui-drawing.md), [ADR-030](../ADR/ADR-030-typography-and-sprites.md), [ADR-031](../ADR/ADR-031-floating-windows.md), [ADR-043](../ADR/ADR-043-hud-in-the-windows-look.md), [ADR-061](../ADR/ADR-061-measured-text.md), [ADR-062](../ADR/ADR-062-type-scale-and-contrast.md), [ADR-070](../ADR/ADR-070-interface-scale.md) and [ADR-088](../ADR/ADR-088-selection-and-commands.md), Game §interface and [the Style Guide](../StyleGuide.md).**
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in the milestone order below.
3. **A gate is an owner decision.** The task does not start until the owner has answered, and the answer is written into the gates table, with its date, first.
4. **ADRs are edited in place** (AGENTS.md §6). A new one takes the next free number when its task lands; this plan reserves none.
5. **A port changes nothing on the screen.** The port milestones, CV4, CV5, CV6 and CV8, move a window or a panel onto the canvas and do nothing else. The PR keeps the old layout function until a temporary test has compared the old and the new display lists item by item, in order, within a hundredth of a pixel, with the longest content, at 1920×1080 and 1280×720 and at every step the windows fit, and found them equal. The same PR then deletes the old function and the test. A difference the port cannot avoid is named in the PR, and the owner's run accepts it.
6. **The interface plans' guards hold for every task:** no text outside its panel or over another (`KeepsEveryTextInsideItsPanel`, `OverlapsNoTwoTexts`), and every text at 4.5:1 against its panel (`SetsEveryTextAtFourAndAHalfToOne`).
7. **Every task that changes the screen is presentation.** It is built, its tests run, and the game is run and looked at, at 1920×1080 and at 2880×1920. The owner's run accepts it. An agent in a cloud container has no Windows, no MSBuild and no GPU, so the PR says which ran where.

Task numbers carry the prefix CV, since UI, LV, SP, NW, UN and RF are taken. Gates are lettered C.

---

## What the review found

Every figure was counted with grep or computed from the layout's own arithmetic on `main` at `94d6aa0`. Nothing was measured on a screen.

| # | Finding | Task |
|---|---|---|
| 1 | **The windows already move; the HUD does not, by decision.** The designer, production, research, Controls, orders, away and leave windows are dragged by their title bars, come to the front and are kept on the screen ([ADR-031](../ADR/ADR-031-floating-windows.md)). The Ore column, the minimap, the selection panel and its buttons are anchored ([ADR-043](../ADR/ADR-043-hud-in-the-windows-look.md) decision 1). What reads as "the windows are fixed" is the HUD, and places lost when the game closes. | C1, CV1 |
| 2 | **A window's place and the interface's scale last until the game closes,** by the owner's decision of 2026-10-02 (ADR-031 decision 1; ADR-070 decision 5). A player who arranges the windows does so again every session. | CV1 |
| 3 | **The Constructors' build buttons are the HUD's, not a window.** They stand in the buttons' panel against the selection panel's right edge, after any warship's orders, and an order folds them into one Build button (ADR-088 decision 2), so they cannot be moved. Selected alone, a Constructor's panel holds the six structures it builds and the retreat row: 318 units tall and 380 wide, the bottom 31% of a 16:9 screen, right of its middle, where the ships the player is working with stand. Beside warships, their four orders join on a map with sectors: 478 units, 46%. Only an order folds it, to 118 units. | C7, CV2 |
| 4 | **Every window is placed by hand.** `Hud.cpp` is 3,863 lines and holds 159 `constexpr float`, most of them a place in reference units from a window's corner, which `Painter` scales. A change of one row's height moves every row under it by hand. | CV3.2, CV4–CV8 |
| 5 | **A window's size is worked out apart from its layout.** Seven functions (`ExtentOf`, `QueueWindowHeight`, `ProductionHeight`, `TopicHeight`, `ControlsHeight`, `AwayHeight`, `LeaveHeight`) and one constant (`ORDERS_WINDOW_HEIGHT`) repeat their layout's arithmetic, and `WindowsFit` uses them a third time. Nothing but the layout tests holds the two in step. | CV3.2 |
| 6 | **A new window touches eight places:** `WindowKind` and `WINDOW_KINDS`, its optional panel in `Hud::Content`, its `Describe`, its `Lay`, its height, its first place in `Hud::Lay`, `WindowsFit`, and its `ActionKind`s with their cases in `GameClient` (41 kinds, 41 cases). The orders window's names stand 3 times in `Hud.h`, 14 in `Hud.cpp` and 5 in `GameClient.cpp`. | CV4.1 |
| 7 | **Interaction is threaded by hand.** The pointer's hover reaches a window's layout as a parameter (`_hovered`, `_giveHovered`). The one element that takes keys, the designer's name, reads its own Esc, Backspace and Enter in `Designer.cpp` ([ADR-017](../ADR/ADR-017-research-and-the-designer.md) decision 8). | CV3.3 |
| 8 | **Nothing scrolls, and nothing stands above a window.** The layers are the HUD and one per window (ADR-031 decision 4), so a list that opens over its window has nowhere to go. The designer, 938 units tall at its most, holds 16:9 screens at 110% (ADR-070), and the Controls window, 806 units for its 33 lines, holds every screen at 125%. The orders form steps a sector, a target, an hour and a minute one press at a time. | CV7.1, CV7.2 |

---

## The canvas

What the gates and the tasks build, as proposed. The ADR of each task records what it decides.

### What stays

- **The pipeline stays as it is.** `Hud::Describe` and the window panels describe what to show from the snapshot. The layout turns that into a display list of panels, lines, sprites and texts in layers. `GameClient` draws the list in one draw call ([ADR-015](../ADR/ADR-015-ui-drawing.md) decision 2) and hit-tests it. Nothing is retained between frames but interaction state and window places, so there is no widget tree to keep in step with the simulation.
- **The display list stays the contract.** `HudTests` read it, and it is what the layout and contrast guards check. The canvas emits it; it does not replace it.
- **An action stays a value.** A press resolves to a `Hud::Action`, as a key does, which is how a button does what its key does (ADR-088 decision 2) and how a test asserts on what a click means. The canvas knows no action: each hit region carries a payload, an index the game maps to its `Action`.
- **The layout stays a pure function** of the content, the fonts, the screen and the interaction state, so its tests need no GPU, no window and no input.
- **The input rules stay:** a press on the interface is the interface's, and a release always reaches the controls ([ADR-015](../ADR/ADR-015-ui-drawing.md) decision 5).

### What it adds

- **A display list in the engine.** Panels (solid, hatched or an image), lines, sprites and texts, each list divided by layer, and hit regions that carry a widget's identifier, its layer, its payload and whether it answers hover only, as a locked part card does. `LayerAt`, `HitAt` and `Covers` are the engine's. The game's own part of today's `Hud::Layout`, the minimap's square and its mapping, stays in `GameApp`.
- **A layout that measures.** Each frame the build records a tree of nodes: columns, rows, grids, stacks, an anchor to a corner, an edge or the middle of the screen or of its parent, spacers, and leaves. Each node has padding, a gap, alignment, and a fixed, least or most size. A first pass measures from the leaves up, wrapping text to the width it is given, and a second places from the root down. Sizes are in reference units and become pixels where they are emitted, as `Painter` does now, so that a port can reproduce today's places exactly. A window's size is what its measure returns, so the height functions go.
- **One leaf the game draws itself.** The minimap draws its marks, its fog and its segments into the rectangle the layout gives it.
- **Interaction every widget shares.** A widget's identifier is a hash of its window's identifier and its own label or index, pushed and popped as the build descends; a duplicate in one frame is a failed assertion ([ADR-093](../ADR/ADR-093-a-failed-assertion-ends-the-process.md)). The canvas keeps what is hot (under the pointer), what is active (pressed, and holding the pointer until release) and what has the keyboard, and a small store per identifier for a scroll's offset, an open drop-down and a caret. Input is resolved against the last frame's display list, as `GameClient` resolves it now.
- **Layers.** The bars over the entities, then the HUD, then each window back to front, then an overlay for an open drop-down. The overlay is drawn last and hit-tested first; a press outside it closes it and goes no further.
- **Keys in one order.** Esc reaches, in turn, the field that has the keyboard, an open drop-down, the front window, and then the controls (ADR-031 decision 5). While a field has the keyboard, no order, group or camera key reads it (ADR-017 decision 8).
- **A theme.** The Style Guide's roles as one struct: the colors of a window's body, an edge, a hover, a pick, a card, a field, the text, a label, locked text, the warning, a bar's track and fill; which font each role takes; and the paddings, gaps and the title bar's height. `GameApp` fills it from the mockup's colors in `Hud.cpp`. A rule of the Style Guide becomes a rule of the API where it can: a chip is the only widget that sets text on the warning's color ([ADR-085](../ADR/ADR-085-one-meaning-per-color.md) decision 1).
- **Widgets in the windows' look:** a window's frame, hatched title bar, close box and corners (ADR-031 decision 3); a HUD panel's frame and corners (ADR-043 decision 1); a label that fits or wraps (ADR-061 decision 3); a button as a card, with its cost, its reason or its key's cap (ADR-043 decision 4); a small arrow or close button; a stepper, `< SHIPYARD 05 >`; a row of cells, one lit as the pick (ADR-088 decision 3); a checkbox; a progress bar; a chip; a key cap; a text field; and, as CV7 needs them, a scroll view and a drop-down.
- **What stays the game's:** the part card, the damage card, a queue's option and its rows, a topic card, a status line, Ore's gem and figure. Each is built from the widgets above.
- **A window is a row of a table.** Its identifier, its title, the key that toggles it, where it first opens, whether `WindowsFit` counts it and whether its place is kept. Where it first opens is an anchor to a corner, an edge or the middle, kept clear of the HUD's anchored panels at their largest, so that a window not yet moved never opens over them nor moves as they change. Adding one is its panel, its `Describe`, its build, its row and its actions.

### Where it lives

- **The canvas is in `NeuronClient`, in `Neuron`** (C4): the display list, the layout, the interaction state, the window stack (the order, the drag and the clamp of `WindowManager` and `Hud::KeepOnScreen`), the theme's struct and the widgets. `Hud::TextMetrics` moves with it, naming a font by its index, since only its `Typeface` names are the game's. `NeuronClientTests` tests all of it without a GPU.
- **The game's look and content stay in `GameApp`:** the theme's values, the typefaces, the composites, the windows' table, the places production and research first take under the Ore (ADR-031 decision 2), and every `Describe`.
- **`Neuron::Window` is the Win32 window already** (`NeuronClient/Window.h`), so the canvas's window takes another name, which the ADR picks.
- **No third-party toolkit.** Dear ImGui, RmlUi and Noesis are packages R14 does not list, and none draws the mockup's look without being rebuilt to it.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| CV1.1 | Window places and the scale kept on disk | — | C2 | merged, [#142](https://github.com/Zwaliebaba/Outpost.Commander/pull/142) |
| CV2.1 | The build window | CV1.1 | C7 | merged, [#144](https://github.com/Zwaliebaba/Outpost.Commander/pull/144) |
| CV3.1 | The display list in the engine | — | C4 | merged, [#145](https://github.com/Zwaliebaba/Outpost.Commander/pull/145) |
| CV3.2 | A layout that measures | CV3.1 | C4 | merged, [#145](https://github.com/Zwaliebaba/Outpost.Commander/pull/145) |
| CV3.3 | Interaction every widget shares | CV3.1 | C4 | merged, [#145](https://github.com/Zwaliebaba/Outpost.Commander/pull/145) |
| CV3.4 | The theme and the widgets | CV3.2, CV3.3 | C4 | merged, [#145](https://github.com/Zwaliebaba/Outpost.Commander/pull/145) |
| CV4.1 | The windows' table, and the leave and away windows | CV3.4 | — | todo |
| CV4.2 | The Controls window | CV4.1 | — | todo |
| CV4.3 | The orders window | CV4.1 | — | todo |
| CV4.4 | The build window on the canvas | CV2.1, CV4.1 | — | todo |
| CV5.1 | The production window | CV4.1 | — | todo |
| CV5.2 | The research window | CV5.1 | — | todo |
| CV6.1 | The designer | CV5.1 | — | todo |
| CV7.1 | Scrolling, and how far the interface scales | CV4.2, CV4.3, CV6.1 | C3, C5 | todo |
| CV7.2 | Drop-downs in the orders form | CV4.3 | C6 | todo |
| CV8.1 | The column under the Ore | CV3.4 | C1 | todo |
| CV8.2 | The selection, its buttons and its retreat | CV8.1 | C1 | todo |
| CV8.3 | The minimap, the tag, the hint, the banner and the menu | CV8.2 | C1 | todo |

### Milestone order

CV1 to CV8, in their numbers' order.

- **CV1 goes first.** It needs nothing of the canvas, it is what the owner meets every session, and it is small.
- **CV2 goes next, on today's code** (C7). The owner wants the build window before the canvas is built, and CV4.4 moves it onto the canvas with the other small windows.
- **CV3 changes nothing on the screen.** It is the engine's, and its tests are `NeuronClientTests`, plus `HudTests` changed only to read the moved types.
- **The ports go from the smallest window to the largest,** so that the canvas meets the leave window's two buttons before the designer's mockup.
- **CV7 goes before CV8,** since scrolling needs the designer and the Controls window on the canvas, and nothing of the HUD. CV7.2 can go as soon as CV4.3 is done.
- **CV8 goes last.** The HUD's panels are anchored and do not move (C1), so porting them buys least. Once it lands, `Painter`, `OpenWindow`, `Frame` and `AddButton` are deleted.

A task whose gate the owner turns down is dropped, and its milestone ships without it.

---

## Gates

Each is an owner decision, proposed here and in the task it gates.

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| C1 | **What the player moves.** The floating windows, as now; the HUD's panels stay anchored, with no presets. A player finds the minimap and the selection by where they always are, and a panel dragged over the battlefield hides it. The Constructors' build buttons leave the HUD for a window (C7). As proposed (owner, 2026-10-27). | this plan | CV8.1, CV8.2, CV8.3 | decided 2026-10-27 |
| C2 | **Window places and the interface's scale are kept on disk.** Each window's place, once the player has moved it, and the interface's step, kept across restarts. On a different screen they are clamped as now (`KeepOnScreen`, `StepInterface`). It reverses the owner's decision of 2026-10-02, in ADR-031 decision 1 and ADR-070 decision 5. As proposed (owner, 2026-10-27). | this plan | CV1.1 | decided 2026-10-27 |
| C3 | **The designer's part rows scroll.** Its title bar, header, name and saved designs above them, and its bars, damage cards and footer below them, stay put, so that hovering a part still previews it in the bars. As proposed (owner, 2026-10-27). | this plan | CV7.1 | decided 2026-10-27 |
| C4 | **The canvas is in `NeuronClient`** from its first task, in `Neuron`, and the game's look and content stay in `GameApp`. The layout needs no game concept, so R9 puts it in the engine, and its tests are `NeuronClientTests`. As proposed (owner, 2026-10-27). | this plan | CV3.1, CV3.2, CV3.3, CV3.4 | decided 2026-10-27 |
| C5 | **How far the interface scales.** ADR-070's steps stay, and `WindowsFit` counts the HUD's bottom row as well as the windows of a fixed size. At its widest the row needs 1,280 units: the minimap's 300 with its margin and gap, 324, the selection panel's 560, the buttons' 380 and the right margin (`Hud.cpp`, where the pair is placed). A 16:9, 16:10 or 3:2 screen is 1,920 units wide at 100%, so the row holds it at 150%; at 175% it would run 183 units past the edge. A 21:9 screen reaches 175%, where its height stops the designer. At 150% the smallest text, 13 units, is 19.5 px at 1080p, past Xbox Accessibility Guideline 101's 18 px. The designer, 938 units at its tallest, scrolls its part rows (C3), which get 219 units at 150% on 16:9: what of it cannot scroll is 469 units, 168 above the rows and 301 below them with the Sensors bar. The Controls window, 806 units, scrolls too, since without it 16:9 and 16:10 stop at 125%. The orders and away windows, 560 and 530 units, fit at 150% and do not scroll. The bottom row does not stack to reach 175%: a taller column would cover more of the battlefield, which the build window (C7) is there to clear. Production and research keep growing and paging as now. As proposed (owner, 2026-10-27). | this plan | CV7.1 | decided 2026-10-27 |
| C6 | **Where a drop-down is used.** A choice among a few is a row of cells, so that the player sees them all and picks in one press (ADR-088 decision 3); a drop-down hides its choices and takes two presses. Proposed: a drop-down only where the choices run long, which is the orders form's sectors, its target, its hour and its minute ([ADR-080](../ADR/ADR-080-events-and-scheduled-orders.md) decision 9). The trigger, the action and the condition keep their steppers. Nothing else in the game takes a drop-down. As proposed (owner, 2026-10-27). | this plan | CV7.2 | decided 2026-10-27 |
| C7 | **The build window.** The Constructors' build buttons become a floating window of their own, built on today's code right after CV1, and moved onto the canvas in CV4.4. It opens and closes on demand, with B or with one Build button that takes the structures' place beside the selection, and never by itself. Esc cancels an armed placement before it closes a window, so that the first Esc cancels a placement armed from the build window. Picking a structure leaves the window open. It first opens at the bottom right. The owner's answers of 2026-10-27, revised the same day to these: at first the window opened by itself as a Constructor was selected, and Esc closed it before it canceled a placement, which left the placement armed. | this plan | CV2.1, CV4.4 | decided 2026-10-27 |

---

## Milestone CV1 — Places that last

### CV1.1 — Window places and the scale kept on disk

- **Gate:** C2.
- **Goal:** the review's second finding. A player arranges the windows once.
- **Scope:**
  - **What is kept:** the interface's step, and each window's place once the player has moved it, by the window's name rather than its enumerator's number. A window not yet moved is not kept, and opens where it opens now, production and research in their places under the Ore (ADR-031 decision 2).
  - **Where:** `Outpost Commander\Interface.json` under the local application data, which a packaged game's Windows redirects to the package's own data, as it does any application data (ADR-078). The shell finds the folder, as it finds the join file's, and hands `GameClient` the path; `GameApp` reads and writes it with `Neuron::ParseJson` and `Neuron::QuoteJson`.
  - **When:** read at startup, before the first layout. Written when a drag ends, when the step changes and when the game closes, to a file beside it that then replaces it, so that a crash never leaves half a file.
  - **What fails:** a file that is not there, cannot be read, is not JSON or names another version is ignored, and the game starts as it does now. In Debug it says so to the debugger. It never stops the game, unlike a bad join file.
  - **On another screen:** a place is clamped by `KeepOnScreen` and the step comes down through `StepInterface` with no step, as now (ADR-070 decision 4). The ADR decides whether a place is kept from the screen's top-left corner, as now, or from the corner nearest the window. Proposed: the nearest corner, so that a window left against the right edge stays there on a wider screen.
- **ADR:** a new one, for the file, its place and its format. ADR-031 decision 1 and ADR-070 decision 5 are rewritten in place, with Game §interface.windows.behavior and Game §interface.scale.
- **Acceptance:** `GameAppTests`: a round trip of places and step; a missing, truncated, foreign and other-version file each ignored; a place kept from the nearest corner, read back on a wider and a narrower screen and clamped; `WindowManagerTests` for places loaded before a window opens.
- **Verify:** CI; run, move the windows, step the scale, close and open the game; then at 2880×1920; **owner run.**
- **As built (2026-10-27):** [ADR-099](../ADR/ADR-099-interface-settings-kept.md).
  - A place is kept from the corner nearest the window's middle, and stood again every frame of a match, so that a moved window keeps to its corner within a session too, across a step of the scale or a new size of screen.
  - Only a step the player takes is kept: not a step down that a screen or a match makes, nor a step up the screen has no room for.
  - `InterfaceSettings` and `WindowManager` with their tests ran in the Linux container against stand-ins for the Windows parts, built with g++ 13 and clang 18 and checked by clang-tidy 18 with the repository's checks. `GameClient` and the shell are CI's to build first. Not yet run on Windows.

---

## Milestone CV2 — The build window

### CV2.1 — The build window

- **Gate:** C7.
- **Goal:** the review's third finding. The player puts the Constructors' build buttons where it wants them, and they no longer cover the bottom of the screen.
- **Scope:** as C7 answers:
  - **An eighth window kind,** in the windows' look (ADR-031 decision 3), dragged and kept on the screen as the others are, its place kept by CV1.1 under the name `build`.
  - **It holds a button for each structure the Constructors build,** as the buttons' panel holds them now: its cost, or its reason in its cost's place, such as ONE PER PLAYER or NODE CAP, dim when the player cannot afford it (ADR-043 decision 4). A press arms the structure's placement, as the button does now, and Shift keeps it armed for another. With no Constructor selected, every button is dim and says NO CONSTRUCTOR, so that the window keeps its size.
  - **B (`KEY_BUILD`) opens and closes it, and so does the Build button,** which stands in the buttons' panel for a selection that holds a Constructor, with B's cap (ADR-088 decision 2). It never opens by itself.
  - **Picking a structure leaves the window open.**
  - **Esc cancels an armed placement before it closes the front window;** with nothing being placed, it closes the front window as now (ADR-031 decision 5).
  - **The buttons' panel keeps the warships' orders and the retreat row.** The structures are never in it, so ADR-088 decision 2's fold goes, with `PlayerControls`' part in it. A Constructor's panel falls from 318 units to 118, and beside warships from 478 to 278, computed with its Build button.
  - **It first opens at the bottom right,** beside the selection and its buttons where they leave room at their widest, as they do at 100%, the pair's right edge at 1,430 units and the window's left at 1,524, and at 110%, by 7 units. Where they do not, from 125% on, it first opens at the right edge with its foot level with the minimap's top, 316 units above the screen's foot, which the bottom row stands under: the buttons' panel at its tallest is 278 units, and the selection panel of 24 ships of six designs about 240. That place does not move as the selection changes.
  - **The Controls window lists B,** a line more: 828 units, which still fits 16:9 at 125%, with 4 units to spare.
  - **`WindowsFit` counts it,** 294 units tall with the six structures the tuning data has.
- **ADR:** ADR-031 decisions 2, 5 and 6 for the new window, Esc and how it opens, ADR-088 decision 2 for the buttons' panel, ADR-068 decision 4 for its key and ADR-099 decision 3 for its name in the file, rewritten in place; Game §interface.windows.behavior and Game §interface.hud.buttons.
- **Acceptance:** `GameAppTests`: the Build button with B's cap for a selection with a Constructor, a mixed one included, and none without; the window's buttons, each structure's cost, reason and dim state as the buttons' tests have them now, and NO CONSTRUCTOR without one; its first place at 100% and at 125%; the buttons' panel without structures; Esc's order with a placement armed; `WindowManagerTests` for the eighth kind; B in the Controls window; the layout and contrast guards with the window open.
- **Verify:** CI; run, a base built from the window, the window dragged, closed and opened again with B and with Build, and Esc with a placement armed; **owner run.**
- **As built (2026-10-27):** ADR-031 decisions 2, 5 and 6 and ADR-088 decision 2.
  - The window stands over the bottom row from 110%, not 125%: at 110% it would clear the selection and its buttons at their widest by 6.7 units, short of the 8 units of `PANEL_GAP` that stand between the HUD's panels.
  - `InterfaceSettings` and `WindowManager` with their tests ran in the Linux container, as CV1.1's did. `Hud`, `GameClient` and `PlayerControls` are CI's to build first. Not yet run on Windows.

---

## Milestone CV3 — The canvas

Nothing on the screen changes in this milestone.

### CV3.1 — The display list in the engine

- **Gate:** C4.
- **Goal:** the ground every later task builds on, moved without a change.
- **Scope:**
  - `Hud::Rect`, `Text`, `SpriteMark`, `Line`, `Span` and the layer bookkeeping of `Hud::Layout` become the engine's display list, with `LayerAt`, `Covers` and the hit regions; a text names its font by index. `Hud::Fill::Fog` becomes an image fill, which knows no fog.
  - `GameClient::DrawHud`'s loop becomes an engine function that draws a display list through `UiPipeline`, in the same order.
  - `ClipToRect` moves to the engine beside `ScreenSegment`.
  - `Hud::Layout` holds a display list, the minimap's square and mapping, and the actions its regions' payloads name. `ActionAt` and `HoverAt` keep their meaning.
- **ADR:** a new one, the canvas. It takes over from ADR-015 decision 4 and ADR-031 decision 4 what moves to the engine, and those decisions are rewritten as AGENTS.md §6 says. CV3.2 to CV3.4 add to it.
- **Acceptance:** `NeuronClientTests` for the layers, the hit regions and the clip; `HudTests` change only where they name the moved types, and pass.
- **Verify:** CI; run, every window open; **a run** by whoever can.
- **As built (2026-10-27):** [ADR-100](../ADR/ADR-100-the-canvas.md) decision 1; ADR-015 decisions 1 and 4, ADR-031 decisions 3 and 4 and ADR-052 decision 4 rewritten.
  - `Hud::Layout` is a `Neuron::DrawList` with the game's lists beside it. A press and the pointer find a region, whose payload indexes `actions` or, for one that answers the pointer only, `previews`. `Hud::Window::firstAction` stays for `Layout::ActionsOf`, which `HudTests` read.
  - A region also carries the widget it is and whether it scrolls, and a layer whether it is an overlay, which CV3.3 needs; a region that names nothing is `NO_PAYLOAD`.

### CV3.2 — A layout that measures

- **Gate:** C4.
- **Goal:** the fourth and fifth findings. A place follows from what stands before it, and a size from what it holds.
- **Scope:**
  - The nodes and the two passes of "The canvas", in reference units, with text measured by `TextMetrics`, which moves to the engine.
  - A place is emitted as units times the scale from its window's corner, as `Painter` emits it now, and glyphs stay on whole pixels in `UiPipeline` (ADR-015 decision 3), so that a port can match today's display list.
  - Width flows down and height up: a label wraps to the width it is given (`TextMetrics::Wrap`) or is cut short with three dots (`TextMetrics::Fit`), and its parent is as tall as its lines.
- **ADR:** the canvas's.
- **Acceptance:** `NeuronClientTests`: rows, columns, grids and stacks with padding and gaps; least and most sizes; an anchor to each corner, edge and the middle; text wrapped and cut; a parent measured from its children; the same tree at two scales.
- **Verify:** CI.
- **As built (2026-10-27):** ADR-100 decision 2. `Neuron::LayoutTree` arranges in four passes, widths up and then down, heights up and then places down. A text cut short within a most is measured as it is set, so that a stepper's right arrow stands just after the dots, as `Painter::Stepped` puts it. `TextMetrics::Wrap` takes a tracking.

### CV3.3 — Interaction every widget shares

- **Gate:** C4.
- **Goal:** the seventh finding. Hover, press, drag and the keyboard reach a widget by its identifier, not by its parameters.
- **Scope:**
  - Widget identifiers, the hot, active and focused widget, and the store per identifier, as "The canvas" has them.
  - The window stack: the order front to back, grabbing a title bar, dragging, releasing, and the clamp of `KeepOnScreen`, taken from `WindowManager`, which keeps which windows the game has and the places under the Ore (ADR-031 decision 2).
  - Input: the pointer, its buttons, the wheel, characters and keys, resolved against the last frame's list. A press on the interface is the interface's and a release reaches the controls (ADR-015 decision 5). The wheel is the interface's only over a scroll view, and the camera's everywhere else (CV7.1 adds the first).
  - Keys in the order of "The canvas", Esc first to the field that has the keyboard.
- **ADR:** the canvas's; ADR-031 decisions 1 and 5 rewritten for what moves.
- **Acceptance:** `NeuronClientTests`: hot and active across frames; a press held through a move off and back; a drag clamped at each edge; the front layer taking a press over a back one; the wheel over and off a scroll region; focus taken and given back, and Esc's order; a duplicate identifier caught. `WindowManagerTests` for what stays the game's.
- **Verify:** CI.
- **As built (2026-10-27):** ADR-100 decisions 3 and 4; ADR-031 decision 1 rewritten.
  - `Neuron::WindowStack` is a template over a window's name and their count, its places in a fixed array as `WindowManager`'s were, so that `Place` and `Settle` stay `noexcept`. `WindowManager` keeps its interface, and `WindowManagerTests` did not change.
  - A field's characters and keys reach it as one stream (`FieldStroke`), since a Backspace between two characters takes back the first.
  - ADR-031 decision 5 is unchanged: `GameClient` still resolves the windows' input, since no window is on the canvas yet.

### CV3.4 — The theme and the widgets

- **Gate:** C4.
- **Goal:** the windows' look as widgets, so that a window is built and not drawn.
- **Scope:** the theme's struct, and the widgets "The canvas" lists, but the scroll view and the drop-down, which CV7 adds with their first use. Each draws into the display list exactly what `Painter`, `OpenWindow`, `Frame`, `AddButton` and `RetreatRow` draw now for the same thing. The text field takes ADR-017 decision 8's characters, Backspace, Enter and Esc.
- **ADR:** the canvas's, and the Style Guide's rules that the API now holds.
- **Acceptance:** `NeuronClientTests` on each widget's display list, in a theme of the tests' own; a chip the only way onto the warning's color; a button dim and taking no press when disabled, with its reason in its cost's place.
- **Verify:** CI.
- **As built (2026-10-27):** ADR-100 decisions 5 to 7.
  - A window's title bar and close box are regions of its own, which `OpenWindow` does not add; a port names them.
  - The checkbox has no counterpart in today's windows.
  - The game's own theme is not filled yet; the first port fills it from `Hud.cpp`'s values.
  - The engine's canvas and its tests ran in the Linux container, built with g++ 13 and clang 18 against stand-ins for `DirectXMath` and the test framework: 35 tests pass and clang-tidy 18 finds nothing. Fifteen bugs planted in the layout, the interaction and the widgets each fail a test: four survived at first, and the tests were strengthened until none did. `WindowManagerTests` ran there against the new `WindowManager`. `Hud`, `GameClient` and `HudTests` are CI's to build first. Not yet run on Windows.

---

## Milestone CV4 — The small windows

Each task is a port (rule 5).

### CV4.1 — The windows' table, and the leave and away windows

- **Goal:** the sixth finding, and the first windows on the canvas.
- **Scope:** the windows' table of "The canvas", replacing what is spread across `WindowKind`'s uses, each window's first place an anchor kept clear of the HUD's anchored panels, as CV2.1's is; the leave window and the away window built on the canvas. `LeaveHeight` and `AwayHeight` go, and `WindowsFit` measures the away window instead.
- **ADR:** the canvas's; ADR-031 decision 2's first places move to the table.
- **Acceptance:** the port's comparison; `HudTests` and `WindowManagerTests` as they stand.
- **Verify:** CI; run, both windows open and dragged.
- **As built (2026-10-27):** [ADR-100](../ADR/ADR-100-the-canvas.md) decisions 7 and 8, and decisions 2, 5 and 6 rewritten; ADR-031 decisions 2 and 3, ADR-070 decision 3 and ADR-099 decisions 1 and 3 rewritten. The table is `Outpost::WINDOWS`, with each window's title, and `WindowsFit` measures the windows its rows count. `GameClient` keeps the canvas's interaction and moves it over the HUD drawn last. The theme's title is the windows' header, in the label face and its own color, so that a window's title bar on the canvas is drawn as `OpenWindow` drew it with its header. The comparison found four things in the engine, fixed in it: a window's body did not stretch across it, an anchored node larger than its box stood off its start, a text of nothing took no line, and a disabled button's reason stood only beside a figure, so that Give's was lost. A button on the canvas is lit as the one under the pointer, where the old windows lit every button whose action was the one under it; no two of a window's buttons share an action, so that the game shows no difference, and the comparison points at buttons with the actions `Describe` gives them.

### CV4.2 — The Controls window

- **Scope:** the Controls window on the canvas, its lines still made by `KeyBindings()` (ADR-068 decision 4). `ControlsHeight` goes.
- **Acceptance:** the port's comparison; `ListsEveryKeyInTheControlsWindow`.
- **Verify:** CI; run, on the menu and in a match.
- **As built (2026-10-27):** ADR-100 decision 7. Each key stands in a column of its own, cut short a gap before it, and what it does takes the rest of the line.

### CV4.3 — The orders window

- **Scope:** the orders window on the canvas, its form's rows as steppers. `ORDERS_WINDOW_HEIGHT` goes.
- **Acceptance:** the port's comparison; the orders window's tests as they stand.
- **Verify:** CI; run, an order given and canceled.
- **As built (2026-10-27):** ADR-100 decision 7. The scheduled orders and the form's rows stand in room for their most, so that the window keeps its height; each cancel button stands in the middle of its line. The arrows' actions are made before the stepper, in the order they are drawn, since a call's arguments are made in any order.

### CV4.4 — The build window on the canvas

- **Scope:** CV2.1's window on the canvas, its buttons the canvas's, its row in the windows' table.
- **Acceptance:** the port's comparison; CV2.1's tests as they stand.
- **Verify:** CI; run, a structure placed from it.
- **As built (2026-10-27):** ADR-100 decisions 7 and 8. Its first place is `FirstPlace::BesideTheSelection`, an anchor at the bottom right, and `BuildCorner` goes.

---

## Milestone CV5 — Production and research

### CV5.1 — The production window

- **Scope:** production on the canvas: its header's stepper, its cards two to a row, kept still as the queue grows (ADR-043 decision 6), each card a composite of the game's, and the queue under them. `ProductionHeight` and `QueueWindowHeight` go.
- **Acceptance:** the port's comparison; `KeepsTheCardsStillAsTheQueueGrows`, `SaysWhatAProductionCardBuilds`.
- **Verify:** CI; run, a queue filled and emptied.

### CV5.2 — The research window

- **Scope:** research on the canvas, its paging by a row as now, every topic card as tall as the most lines any shown topic needs (ADR-061 decision 4). `TopicHeight` goes.
- **Acceptance:** the port's comparison; research's tests as they stand.
- **Verify:** CI; run, paged to the end and back.

---

## Milestone CV6 — The designer

### CV6.1 — The designer

- **Goal:** the largest window, and the one fixed to the owner's mockup, on the canvas.
- **Scope:** the designer on the canvas: its header, name field as the canvas's text field, saved designs, part rows, bars, damage cards and footer, at [ADR-062](../ADR/ADR-062-type-scale-and-contrast.md) decision 4's widths. `ExtentOf` goes, and `WindowsFit` measures the designer. `Designer.cpp` stops reading keys of its own.
- **Acceptance:** the port's comparison, with parts locked, unlocked and previewed; `LaysTheDesignerOutAsAWindow`, `FitsTheLargeHullsNumbersInCascadiaMono`.
- **Verify:** CI; run, a design named, saved, renamed and queued; **owner run**, since it is the mockup's.

---

## Milestone CV7 — What the canvas adds

### CV7.1 — Scrolling, and how far the interface scales

- **Gates:** C3, C5.
- **Goal:** the eighth finding's first half. A window of a fixed size no longer holds the interface's scale under the screen's room.
- **Scope:** as C3 and C5 answer:
  - **A scroll view:** its content clipped to it, moved by the wheel over it, and a thumb that is dragged. Clipping is on the CPU in `UiPipeline`: a quad cut to the clip, its texture coordinates with it, so the interface stays one draw call. A hatched quad's stripes are laid on the screen's pixels (ADR-030 decision 6) and need no change. Lines are never clipped by a scroll view; only the minimap draws them.
  - **The designer's part rows scroll,** and nothing above or below them. The thumb stands in the 14 units right of the cards (ADR-062 decision 4). The view shows at least one line of cards.
  - **The Controls window's lines scroll,** with a least height. The orders and away windows do not (C5).
  - **`WindowsFit` counts each fixed window at its least height, and the HUD's bottom row at its widest** (C5).
- **ADR:** the canvas's for the scroll view; ADR-070 decision 3 and its consequences rewritten with the new ceilings, measured from the canvas's measure; Game §interface.scale and Game §interface.windows.designer.
- **Acceptance:** `NeuronClientTests` for the clip of a solid, a hatched, a sprite and a text quad, and for the scroll view; `HudTests.StepsTheInterfaceAsFarAsItsWindowsFit` with 150% on 16:9, 16:10 and 3:2 and 175% on 21:9; the layout and contrast guards at every new step, which is where the column under the Ore would meet the minimap, a fit this plan has not computed.
- **Verify:** CI; run at every step on 1920×1080 and 2880×1920, a part hovered at the top and the bottom of the rows, a selection at its widest; **owner run.**

### CV7.2 — Drop-downs in the orders form

- **Gate:** C6.
- **Goal:** the eighth finding's second half. A sector or an hour is picked, not stepped to.
- **Scope:** as C6 answers: a drop-down widget whose list opens in the overlay layer, under its field or over it where the screen ends, scrolling when it runs past the screen; picked by a press, closed by Esc or a press outside it, which goes no further. The orders form's sector, target, hour and minute fields take it; their arrows stay beside it.
- **ADR:** the canvas's for the drop-down; [ADR-080](../ADR/ADR-080-events-and-scheduled-orders.md) decision 9 for the form; a Style Guide line beside ADR-088 decision 3's: a choice among many is a drop-down.
- **Acceptance:** `NeuronClientTests` for opening, picking, Esc, a press outside and the flip at the screen's foot; the orders window's tests with a field picked.
- **Verify:** CI; run, an order scheduled with every field picked; **owner run.**

---

## Milestone CV8 — The HUD and the menu

The HUD's panels stay where they are (C1). Each task is a port (rule 5).

### CV8.1 — The column under the Ore

- **Scope:** the Ore, the status, territory and alerts panels as anchored panels on the canvas, the column's width kept for its widest fixed text ([ADR-066](../ADR/ADR-066-production-status-on-the-hud.md) decision 3).
- **Acceptance:** the port's comparison; the column's tests as they stand.
- **Verify:** CI; run.

### CV8.2 — The selection, its buttons and its retreat

- **Scope:** the selection panel, as wide as its longest line between its least and most (ADR-043 decision 5), its ship bars, the buttons against its right edge, and the retreat's row of cells (ADR-088 decisions 2 to 4).
- **Acceptance:** the port's comparison; the selection's tests as they stand.
- **Verify:** CI; run, a structure, a ship and a fleet selected.

### CV8.3 — The minimap, the tag, the hint, the banner and the menu

- **Scope:** the minimap's frame, with its marks, fog and view drawn by the game into its leaf; the tag at the top right; the hint; the match's end; the main menu. `Painter`, `OpenWindow`, `Frame` and `AddButton` are deleted.
- **Acceptance:** the port's comparison; the minimap's and the menu's tests as they stand.
- **Verify:** CI; run, the menu and a match to its end.

---

## Not in this plan

- **Moving the HUD's panels, and presets for them.** C1 keeps them anchored.
- **Docking, resizing and minimizing a window.** ADR-031 forecloses them. The canvas would make resizing cheap, but nothing asks for it.
- **Tooltips.** A button says why it cannot be pressed on itself, not on hover (ADR-043 decision 4). The overlay layer could carry one later.
- **Moving between widgets with the keyboard or a gamepad.**
- **Scrolling production and paging research differently.** C5 leaves them as they are.
- **A bottom row that stacks or narrows at large steps.** C5 holds the scale where the row fits.
- **Animation and transitions.**
- **Text shaping and right-to-left text.** ADR-030 forecloses them.
