# Outpost Commander — Console Plan: The Bottom of the Screen as One Console

Status: **open** · accepted by the owner on 2026-10-28, with gates B1–B5 decided that day · From the owner's request of 2026-10-28 to make the build window look natural, with two screenshots of a match at 1920×1080 and 100%, one with a Constructor selected and one without, and the owner's choice the same day of StarCraft II's console over a bar across the screen as Homeworld 2 has (gate B1) · Written against `main` at `c7652a9`

The bottom of the screen is where the player picks what to do, and a window is for what needs managing. Today the bottom holds the minimap at the left and, in the middle, the selection with its buttons against its right edge, while the Constructors' structures stand in a floating window of their own at the bottom right. This plan makes the bottom of the screen one console in StarCraft II's shape: the minimap in the bottom-left corner, the selection in the middle, and a command card in the bottom-right corner, the three on one baseline. The structures become a mode of the command card, and the build window goes. Production, research, the designer, the orders and the Controls window stay windows, since each is managed rather than picked from.

It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then [ADR-088](../ADR/ADR-088-selection-and-commands.md), [ADR-031](../ADR/ADR-031-floating-windows.md), [ADR-043](../ADR/ADR-043-hud-in-the-windows-look.md), [ADR-070](../ADR/ADR-070-interface-scale.md), [ADR-099](../ADR/ADR-099-interface-settings-kept.md) and [ADR-100](../ADR/ADR-100-the-canvas.md), Game §interface.hud and [the Style Guide](../StyleGuide.md).**
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gates are decided.** One PR per milestone (owner, 2026-09-30).
3. **A gate is an owner decision.** The task does not start until the owner has answered, and the answer is written into the gates table, with its date, first.
4. **ADRs are edited in place** (AGENTS.md §6). This plan reserves no number.
5. **The interface plans' guards hold in every mode of the card, at every step the interface takes:** no text outside its panel or over another (`KeepsEveryTextInsideItsPanel`, `OverlapsNoTwoTexts`), and every text at 4.5:1 against its panel (`SetsEveryTextAtFourAndAHalfToOne`).
6. **The task changes the screen, so it is presentation.** It is built, its tests run, and the game is run and looked at, at 1920×1080 and at 2880×1920. The owner's run accepts it. An agent in a cloud container has no Windows, no MSBuild and no GPU, so the PR says which ran where.

Task numbers carry the prefix CN, since UI, CV, LV, SP, NW, UN, RF and OV are taken. Gates are lettered B.

---

## What the review found

Each figure is computed from `Hud.cpp`'s constants on `main` at `c7652a9`, or measured on the owner's screenshots, as its row says. A screenshot is 1917 by 1078 pixels cut from a 1920×1080 screen at 100%, where a reference unit is a pixel, and its panels' edges stand at the pixels `Hud.cpp`'s constants give them. A width measured on it is the span of the columns whose pixels are brighter than the panel's, within 2 pixels.

| # | Finding | Task |
|---|---|---|
| 1 | **The Build button moves with the selection.** The selection and its buttons are centered as a pair (ADR-088 decision 2), so the buttons' panel's left edge stands at 770 units plus half the selection's width: 910 beside the narrowest selection, 280 units, and 1,050 beside the widest, 560, at 100% on a 16:9 screen. Every command the player reaches for moves by up to 140 units as what it has selected changes. Computed. | CN1.1 |
| 2 | **The pair's tops do not line up.** The two panels are bottom-aligned, and each is as tall as what it holds. A Constructor's selection is 90 units tall and its buttons' panel 118, so their tops stand 28 units apart: 974 and 946 pixels on the screenshot with a Constructor. Computed, and measured. | CN1.1 |
| 3 | **The build window outlives its Constructor.** It never opens or closes by itself (gate C7 of [the canvas plan](ImplementationPlan-Canvas.md)), so with no Constructor selected it stands at the bottom right, 380 by 294 units, six dim buttons each saying NO CONSTRUCTOR. The 294 is `WindowsFit`'s; the rest is the screenshot without a Constructor. | CN1.2 |
| 4 | **Build is said twice, apart.** The Build button stands in the buttons' panel, and the window it opens at the bottom right: for a lone Constructor at 100%, 234 units apart, from the panel's right edge at 1,290 to the window's left at 1,524. The button does not show that the window is open. Computed, and measured. | CN1.2 |
| 5 | **One context, two looks.** The selection and its buttons are HUD panels, framed with corner brackets (ADR-043 decision 1); the window that holds what the same Constructor builds has a hatched title bar and a close box (ADR-031 decision 3). | CN1.2 |
| 6 | **The structures' words do not fit two to a row on one line.** On the screenshot with a Constructor: Shipyard 63 pixels, Research Lab 96, Mining Rig 72, Defence Platform 126, Relay 40 and Repair Bay 78; a cost such as "♦ 300" 36; ONE PER PLAYER 99; and a button's words 13 pixels in from its left edge and 12 from its right. Two to a row in a panel of 380 units, a cell is 175 units wide and holds 150 of words. Defence Platform with its cost needs 170, its 126, a gap of 8 and 36, and the Research Lab with ONE PER PLAYER needs 203. On two lines, a name over its cost, the widest line is Defence Platform's 126. Measured. | CN1.2 |

---

## The console

What the gates decided and the tasks build. The ADRs the tasks rewrite record what they decide.

- **Three panels on one baseline.** The minimap in the bottom-left corner, as now; the selection in the middle of the screen; the command card in the bottom-right corner, where the build window first opens now, its left edge at 1,524 units at 100% on 16:9. Each stands with its foot at the margin, 16 units above the screen's foot, and each is as tall as its own content asks: the minimap 300 units, the card its fixed height, the selection what it holds. The three stand apart, so no two edges are asked to meet.
- **The selection** is the selection panel as it is, moved (B4): in the middle of the screen, and moved only as far as it must to stand a gap of 8 units clear of the minimap and of the card. Its widest is 560 units, or the room between them where that is less: 552 at 150% on a 16:9 screen.
- **The command card** is the buttons' panel made a grid of fixed cells (B2): 380 units wide, two columns of cells 175 units wide, three rows, and the retreat at its foot for ships. Each command has its own cell and keeps it, empty when the selection does not offer it, so that a command never moves. It is 209 units tall in every selection and in both modes. With nothing selected, or a selection that offers nothing, such as another player's structure, it is not drawn, as the buttons' panel is not now.
- **Build is a mode of the card** (B3). B, or the Build button in its cell, turns it on: the card's cells give way to the structures, two to a row, each its name over its cost or the reason it is dim. B again, or Esc, turns it off. The build window and every rule of its own go.
- **Windows stay windows.** The designer, production, research, orders, Controls, away and leave windows float, move and are kept as now (gates C1 and C2 of the canvas plan), and stand over the console as they stand over the rest of the HUD (B5).

### The card's cells

As B2 decides. A structure is selected alone and ships never with one (Game §orders.selection), so the rows hold either a selection of ships' cells or a structure's.

| Row | Left cell | Right cell |
|---|---|---|
| 1 | Attack-move (A); or a structure's first window: Production (P), or Research (R) on a Research Lab | Hold sector (H); or the Shipyard's Ship designer (D) |
| 2 | Patrol (T) | Stop (S) |
| 3 | Build (B) | empty |
| 3, a structure that grows | Upgrade, across both cells, with its level, time, cost and progress (ADR-064 decision 7) | |
| Foot | RETREAT over its three cells, for a selection of ships (ADR-088 decision 3) | |

Hold sector is offered only on a map with sectors, and its cell is empty without them. A lone Constructor's card holds Build and the retreat, its first two rows empty.

**Its height,** computed from `Hud.cpp`'s constants: a padding of 12, a button of 34 with 6 between, and the retreat's label of 20 over its cells of 34. Three rows of buttons with the retreat under them are 198 units. In build mode a label row of 20, BUILD with Esc's key cap at its right, stands over three rows of two-line cells, each 51 units (6, a name's line of 22, a cost's line of 17, and 6), with 6 between: 209 units. The card stands 209 units tall in both modes, so that turning build mode on moves nothing, and in the cells' mode the 11 units to spare stand over the retreat.

### Build mode

As B3 decides.

- **B, or the Build button, turns build mode on** for a selection that holds a Constructor, a mixed one included, and B or the button again turns it off. Without a Constructor, B does nothing.
- **In build mode the card holds the label row and the structures,** in the order the build window holds them now, two to a row: Shipyard and Research Lab, Mining Rig and Defence Platform, Relay and Repair Bay. Each cell shows the structure's name over its cost, or over the reason it cannot be built, ONE PER PLAYER or NODE CAP, dim as the build window's buttons are when the Ore is short (ADR-043 decision 4). On a map without sectors the Relay's cell is empty.
- **Picking a structure arms its placement,** as its button in the build window does now; Shift keeps it armed for another, and build mode stays on, so that another can be picked once it is placed (gate C7 of the canvas plan).
- **Esc, in turn,** cancels an armed placement, turns build mode off, closes the front window, and in a universe opens the leave window (ADR-031 decision 5; Game §interface.windows.leave). A right-click cancels a placement and leaves build mode on.
- **A selection without a Constructor turns build mode off,** and a Constructor selected again shows the card's cells: build mode is not kept between selections.
- **The orders and the retreat are not shown in build mode;** the orders' keys still give them.
- **No structure takes a key of its own.** StarCraft II's B and then a letter would make S, A and the other order keys name a structure while build mode is on and an order otherwise; that is a decision of its own, after this plan.

### What it costs

- **It overturns three decisions:** gate C7 of the canvas plan, the pair of ADR-088 decision 2 and ADR-100 decision 8's first place beside the selection, and with them CV2.1's window and its move onto the canvas in CV4.4.
- **A card is often mostly empty.** A lone Constructor's card is 209 units tall where its buttons' panel is 118, with two empty rows; a Command Station's holds two buttons. That is the price of commands that never move.
- **The eye travels farther** between what is selected and what it can do: from the middle of a 280-unit selection to the middle of the card, 754 units at 100% on a 16:9 screen, and 1,074 on a 2560×1080 screen. Computed.
- **The designer covers the card's top two rows at its tallest.** With every part of the match and the Sensors bar it is 938 units tall from the top margin, so at 100% on 16:9 its foot stands at 954 units, 99 below the card's top at 855. Today it stands over the buttons' panel of a lone Constructor by 8 units. It keeps standing over, rather than scroll its part rows from 100% (B5).
- **Build mode hides a mixed selection's orders and retreat** until Esc or B, though their keys work throughout.

### Scale

- **ADR-070's ceilings stay:** 150% in a match on 16:9, 16:10 and 3:2 screens, 175% on 21:9, and 200% on the menu, which has no console.
- **The bottom row at its widest** is the minimap's 324 units with its margin and gap, the selection, a gap of 8, the card's 380 and the right margin's 16: 1,288 units with the selection at 560, past a 16:9 screen's 1,280 at 150%. Holding the selection's widest to the room, 552 at 150%, keeps the row within the screen and the ceiling where it is. Counting the selection at its least instead, 280, would let the row reach 175%, where the minimap's top would stand at 301 units, over the column under the Ore, which runs to 350 with its longest content ([#154](https://github.com/Zwaliebaba/Outpost.Commander/pull/154)'s measure); so the selection is counted at its widest.
- **The build window no longer counts** in `WindowsFit`, which it never decided.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| CN1.1 | The selection in the middle and the command card in the corner | — | B1, B2, B4, B5 | in review, [#158](https://github.com/Zwaliebaba/Outpost.Commander/pull/158) |
| CN1.2 | Build mode in the command card, and the build window goes | CN1.1 | B1, B3 | in review, [#158](https://github.com/Zwaliebaba/Outpost.Commander/pull/158) |

### Milestone order

One milestone, CN1, in one pull request, CN1.1 and then CN1.2, so that the build window is never given a place of its own beside a card that is about to replace it. A task whose gate the owner turns down is dropped, and the milestone ships without it.

---

## Gates

Each is an owner decision, proposed here and in the task it gates, and all five are decided.

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| B1 | **The bottom of the screen is a console of three panels,** in StarCraft II's shape: the minimap in the bottom-left corner, the selection in the middle and a command card in the bottom-right corner, on one baseline, with the gaps between them open to the battlefield. The Constructors' structures are a mode of the card, not a window; what needs managing stays a window. Chosen over a bar across the bottom as Homeworld 2 has, which on this camera would cover the nearest ground for good, and over a card that stays against the selection (owner, 2026-10-28). It overturns gate C7 of the canvas plan. | this plan | CN1.1, CN1.2 | decided 2026-10-28 |
| B2 | **The card's cells.** Two columns and three rows of fixed cells, each command in its own as "The card's cells" lists them, empty where it is not offered, and the card 209 units tall in every selection and mode, so that a command never moves. Over cells filled from the top in the order the buttons stand in now, with the card as tall as what it holds, 118 to 209 units, which would move a command's cell and the card's top with the selection. As proposed (owner, 2026-10-28). | this plan | CN1.1 | decided 2026-10-28 |
| B3 | **Build mode,** as "Build mode" says: B and the Build button turn it on and off; the structures on two lines, two to a row; picking leaves it on; Esc cancels a placement, then turns it off, before it closes a window; a selection without a Constructor ends it and it is not kept; and no structure takes a key of its own, which StarCraft II's B and then a letter would need a decision of its own for. As proposed (owner, 2026-10-28). | this plan | CN1.2 | decided 2026-10-28 |
| B4 | **Where the selection stands.** In the middle of the screen, moved only to stand a gap clear of the minimap and the card, at most 560 units wide or the room between them, 552 at 150% on 16:9. Over the middle of the room between the minimap and the card, which is 40 units left of the screen's middle at 100% on 16:9. As proposed (owner, 2026-10-28). | this plan | CN1.1 | decided 2026-10-28 |
| B5 | **Windows over the console.** A window stands over the console as it stands over the rest of the HUD, so that the designer at its tallest covers the card's top 99 units at 100% on 16:9 until the player moves it. Over each window first opening clear of the card where it fits at its least, which would scroll the designer's part rows from 100%, by 107 units with every part and the Sensors bar. As proposed (owner, 2026-10-28). | this plan | CN1.1 | decided 2026-10-28 |

---

## Milestone CN1 — The console

### CN1.1 — The selection in the middle and the command card in the corner

- **Gates:** B1, B2, B4, B5.
- **Goal:** the review's first two findings. A command stands in one place whatever is selected, and no edge at the bottom of the screen almost meets another.
- **Scope:**
  - **The command card** takes the buttons' panel's place: anchored to the bottom-right corner at the margins, 380 by 209 units, its cells as B2 decides, the retreat at its foot. `Hud::Content` says which cell each button takes, rather than listing the buttons in order.
  - **The selection** stands as B4 decides, its height and width worked out as now, its widest held to the room.
  - **`WindowsFit`** counts the bottom row as "Scale" says.
  - **Until CN1.2, in the same pull request,** the Build button still opens the build window, which still first opens at the bottom right, over the card.
- **ADR:** ADR-088 decision 2 rewritten in place for the card, its cells and its place, and the selection's place; and ADR-070 decision 3 and its consequences for the bottom row's figures. ADR-100 decision 8's first places stay, as B5 decides. Game §interface.hud, Game §interface.hud.selection, Game §interface.hud.buttons and Game §interface.scale follow.
- **Acceptance:** `HudTests`:
  - the card's place and size at 100% and 150% on 16:9, at 2880×1920 and at 2560×1080;
  - every command in its cell for warships, a Constructor, a mixed selection, a Command Station, a Shipyard and a Research Lab, with Hold sector's cell empty on a map without sectors and the Upgrade across both cells;
  - the retreat at the card's foot, and the card as tall for every selection;
  - no card with nothing selected or with another player's structure;
  - the selection in the middle, moved clear of the minimap and the card, at most 552 units wide at 150% on 16:9;
  - the feet of the minimap, the selection and the card on one line;
  - `StepsTheInterfaceAsFarAsItsWindowsFit` with the ceilings unchanged, and the guards at every step.
- **Verify:** CI; run at 1920×1080 and at 2880×1920, with warships, a Constructor, both, and each structure selected; **owner run.**
- **As built (2026-10-28):** ADR-088 decision 2, [ADR-070](../ADR/ADR-070-interface-scale.md) decision 3, [ADR-015](../ADR/ADR-015-ui-drawing.md) decision 4 and ADR-100 decision 7.
  - **The selection's widest is 552 units at every scale,** not 560 or the room between the minimap and the card. With 560 and the gap before the card, the bottom row is 1,288 units, and `WindowsFit` would stop a 16:9 screen at 125%. Counting the room instead would let a match reach 175%, which "Scale" turns down. 552 keeps the row at 1,280 and the ceiling at 150%; at smaller steps a selection's widest line is cut 8 units sooner than before.
  - **The selection stands clear of the card's place whether or not the card is drawn,** so that it does not move as the card comes and goes.
  - `HudTests` ran in the Linux container against stand-ins for the Windows parts and a generated font: 99 of the 100 the harness can run pass, the one failure the known font artefact in `OffersToLeaveAUniverse`. `PlayerControls`, `GameClient` and the shell are CI's to build first. Not yet run on Windows.

### CN1.2 — Build mode in the command card, and the build window goes

- **Gates:** B1, B3.
- **Goal:** the review's third to sixth findings. What a Constructor builds stands where its other commands stand, in the HUD's look, and only while a Constructor is selected.
- **Scope:**
  - **Build mode** as B3 decides. Whether it is on is the client's, kept as an armed placement is, and `Hud::Describe` takes it as it takes the armed order; the ADR says where it is kept.
  - **The structures' cells** are `Hud::BuildPanel`'s buttons, each on two lines, in the order and the cells "Build mode" gives. NO CONSTRUCTOR goes with the window.
  - **The build window goes:** its row in the windows' table, `WindowKind::Build`, `FirstPlace::BesideTheSelection`, `LayBuild`, the window `ActionKind::OpenBuild` toggles, and its count in `WindowsFit`. Its name in the interface's file, `build`, is never given to another window, and a file that names it still loads and keeps the rest, as ADR-099 decision 3 has it for a window a build does not have.
  - **Esc's order** as B3 decides.
  - **The Controls window's line for B** says what B does now.
- **ADR:** ADR-088 decision 2 takes build mode, and what ADR-031 decisions 5 and 6 say of the build window moves to it; ADR-031 decision 2's first places, ADR-068 decision 4's line for B, ADR-099 decision 3's names, ADR-100 decisions 7 and 8, and ADR-070 decision 3 lose the build window. the build window's section of the design goes, its key on the "Retired keys" line, and Game §interface.hud.buttons, Game §interface.windows.behavior, Game §interface.controls and Game §orders.constructors say where the structures stand.
- **Acceptance:**
  - `HudTests`: B and the Build button turn build mode on for a Constructor, a mixed selection included, and off again, and do nothing without one; each structure's cell, cost, reason and dim state, in the cells' order, the Relay's cell empty without sectors; the card the same size in both modes, with the label row and Esc's cap; Esc's order with a placement armed, with build mode on, with a window open, and in a universe; a right-click that leaves build mode on; a selection without a Constructor that ends it, and a Constructor selected again that shows the cells; the guards in build mode at every step.
  - `WindowManagerTests` without the build window, and `InterfaceSettingsTests` with a file that names `build`.
  - `PlayerControlsTests`, if build mode is kept there.
- **Verify:** CI; run, a base built from the card, two structures placed with Shift and two without, Esc in each state, at 1920×1080 and at 2880×1920; **owner run.**
- **As built (2026-10-28):** ADR-088 decision 2, with what [ADR-031](../ADR/ADR-031-floating-windows.md) decisions 5 and 6 said of the build window; ADR-099 decision 3 and ADR-100 decisions 6 to 8; the build window's section of the design retired.
  - **BUILD's cap is B's, not Esc's.** A key's cap holds a letter, and B turns build mode off as Esc does.
  - **Build mode is the controls'** (`PlayerControls::IsBuilding`), beside the armed placement. It is checked before a frame's input and after it, so that a click that selects ships without a Constructor ends it in the same frame.
  - **A structure's cell is a button on two lines,** which the canvas's button now draws (`Neuron::ButtonContent::stacked`), tested by `CanvasTests.StacksAButtonsFigureUnderItsLabel`.
  - **NO CONSTRUCTOR goes:** `DescribeBuild` without a Constructor dims every structure, which build mode never shows. A seventh structure would not be offered, since the card has six cells.
  - **The Controls window's line for B** moves from WINDOWS to SELECTING AND ORDERS, its words as they were.
  - The engine's tests ran in the container under g++ and clang++ with `-Werror`, 51 of 51; `HudTests` as CN1.1 says.

---

## After this plan

- **Words, not icons.** StarCraft II's cells are icons with a key's letter, and so are small. The interface's atlas holds its fonts and three sprites ([ADR-030](../ADR/ADR-030-typography-and-sprites.md)), so the cells hold words, and the card is as wide as the longest word lets it be. Icons for the commands and the structures would let the card shrink, and are a plan of their own.
- **Keys in build mode,** as B3 says.
