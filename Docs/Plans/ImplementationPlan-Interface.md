# Outpost Commander — Interface Implementation Plan

Status: **open** · accepted by the owner on 2026-10-03, with K1–K7 as proposed · From a UI/UX review of six screenshots of a match, made on 2026-10-03 against the interface Phase 1 built, which [the living design](../OutpostCommander.md) now describes (Game §interface)

[The living design](../OutpostCommander.md) says what the interface is (Game §interface), AGENTS.md says how code is written, and `Docs/ADR/` records the engineering decisions. This plan puts the review's recommendations in order, as a queue of tasks. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. The Phase 1 plan is closed, and what it learned is in [the post-mortems](../Records/PostMortems.md). Two things Phase 2 built meet this plan, the designer's fourth row, for modules ([ADR-058](../ADR/ADR-058-modules.md)), and alerts ([ADR-059](../ADR/ADR-059-alerts-and-standing-orders.md)), and the milestone order says how.

---

## How an agent uses this plan

The rules are these:

1. **Read AGENTS.md, then Game §interface and Game §ships.designer, then the ADRs the task names.** The review is not in the repository; what it found is summarized below, with how each figure was found.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in milestone order.
3. **A gate is an owner decision.** The task does not start until the owner has answered, and the answer is written into the gates table, with its date, first. An answer that changes what the interface shows is recorded in the task's ADR, naming what it changes, as ADR-043, ADR-046 and ADR-047 recorded the owner's reviews of Phase 1's screen.
4. **ADRs are edited in place**, as AGENTS.md §6 has it: a decision that changes is rewritten where it stands, and a new ADR takes the next free number, ADR-061 onward.
5. **Every task is presentation.** It is built, its tests run, and the game is run and looked at, at 1920×1080 and at 2880×1920 as [ADR-030](../ADR/ADR-030-typography-and-sprites.md)'s typography was (AGENTS.md §3). The owner's run accepts it.
6. **No task lands with a text outside its panel or over another**, once task 14.1's tests exist.

Task numbers continue Phase 1's milestones, so that a number names one task across every plan. Gates are lettered K.

---

## What the review found

| Finding | Priority | Task |
|---|---|---|
| A topic's two prerequisites run past its card into the next: Relay Archives' "NEEDS · IMPROVED EXTRACTION + HULL PLATING" into Pulse Drive. A weapon's splash note touches its lock line on the Missile Rack and the Flak Battery. | P0 | 14.1 |
| The figures and labels a player decides by — a part's numbers, a topic's effect and prerequisites, the per-Ore figures — are set at 11–13 px at 1080p, where Xbox Accessibility Guideline 101 asks 18 px on PC. A locked card's text stands at 3.6–4.0:1 against its card, under 4.5:1. The interface scales only with the screen, so a player cannot enlarge it. | P0 | 14.2, 17.1 |
| Nothing outside the windows says a Shipyard or the Research Lab is idle. The last three screenshots bank 4,292–5,136 Ore while the queues they show, research's and Shipyard 05's, stand at 0 / 5. | P1 | 15.1 |
| D, P and R open the windows, and no button says so. A, S, Alt, the control groups and the camera's keys are written nowhere, and the menu is Start skirmish and Quit. | P1 | 15.2, 16.4 |
| Hovering a part replaces each of the designer's figures with the previewed one, and says better or worse by its color alone. | P1 | 15.3 |
| The research window opens at its slot beside production's, mid-screen, even when production is closed. | P1 | 15.4 |
| The selection ring is a band 15% of its radius wide: about 15 px of saturated green under a Shipyard, in a scene of lines a pixel wide. | P1 | 15.5 |
| "QUEUE · 0 BUILT" reads as one statement. The production window's arrows sit by its small label and change the big title below it. Research gives no page position. A window's body is 97% opaque, so bright shapes show through its near-black. | P2 | 16.1 |
| The gateway topic's gold edge reads as a pick or a hover. | P2 | — (Phase 3 removes the gateways: ADR-033 decision 2) |
| On the minimap, enemy marks and ore are both warm and differ only by brightness and size. Fields stand at 1.3:1 against the map and dry asteroids at 2.5:1, and the minimap is 260 units for a 5 km map. | P2 | 16.2 |
| A production card shows a name, a code and a cost: not the build time, nor what the design is good against. | P2 | 16.3 |
| The starting designs' names repeat their codes, "Small+Ion+Mass Driver" beside "S·I·MD", and are cut short in the chips. | P2 | 16.5 |
| The brightest stars, drawn as crosses, read as waypoints on the playfield. | P2 | 16.6 |

**How the figures were found.** Sizes are the typefaces' sizes in `Hud::Typefaces`, in reference units, which are pixels at 1080p. Contrast is WCAG's ratio between the relative luminances of the linear colors in `Hud.cpp`, which the render target encodes to sRGB. The ring's width and the Ore figures were read off the screenshots. What hovering does was read from the code, not seen, and motion and flashing cannot be judged from stills.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 14.1 | Text measured with its fonts, and kept inside its card | — | — | merged with 14.2, [#65](https://github.com/Zwaliebaba/Outpost.Commander/pull/65); awaiting the owner's run |
| 14.2 | A type scale that reads | 14.1 | K1 | merged, [#65](https://github.com/Zwaliebaba/Outpost.Commander/pull/65); awaiting the owner's run |
| 15.1 | What production and research are doing, on the HUD | 14.2 | K2 | merged, [#77](https://github.com/Zwaliebaba/Outpost.Commander/pull/77); awaiting the owner's run |
| 15.2 | Keys on the buttons | 14.2 | — | merged, [#77](https://github.com/Zwaliebaba/Outpost.Commander/pull/77); awaiting the owner's run |
| 15.3 | The designer's preview in figures | 14.2 | — | merged, [#77](https://github.com/Zwaliebaba/Outpost.Commander/pull/77); awaiting the owner's run |
| 15.4 | Windows open where there is room | — | — | merged, [#77](https://github.com/Zwaliebaba/Outpost.Commander/pull/77); awaiting a run |
| 15.5 | The selection ring at a fixed width | — | K3 | merged, [#77](https://github.com/Zwaliebaba/Outpost.Commander/pull/77); awaiting the owner's run |
| 15.6 | A locked part previews what its topic buys | 15.3 | K8 | todo |
| 16.1 | The windows' headers and bodies | 14.2 | — | merged, [#78](https://github.com/Zwaliebaba/Outpost.Commander/pull/78); awaiting the owner's run |
| 16.2 | The minimap's marks and size | — | K5 | merged, [#78](https://github.com/Zwaliebaba/Outpost.Commander/pull/78); awaiting the owner's run |
| 16.3 | What a production card says | 14.2 | K5 | merged, [#78](https://github.com/Zwaliebaba/Outpost.Commander/pull/78); awaiting the owner's run |
| 16.4 | A controls window | 15.2 | K4 | merged, [#78](https://github.com/Zwaliebaba/Outpost.Commander/pull/78); awaiting the owner's run |
| 16.5 | Short names for the starting designs | — | K6 | merged, [#78](https://github.com/Zwaliebaba/Outpost.Commander/pull/78); awaiting a run |
| 16.6 | The sky's crosses | — | K5 | done: kept as they are (owner, 2026-10-05) |
| 17.1 | An interface scale the player sets | 14.2 | K7 | merged, [#79](https://github.com/Zwaliebaba/Outpost.Commander/pull/79); awaiting the owner's run |

### Milestone order

14, 15, 16, 17. Milestone 14 goes first because every later task lays out text: 14.1's tests are what make 14.2's sizes safe, and they guard everything after. It also goes before Phase 2's plan, so that if Phase 2's modules (ADR-058) give the designer its fourth row, the row is laid out once, at the sizes it keeps. Milestone 15 is what a player is missing in play. Milestone 16 is polish, item by item as K5 accepts it. Milestone 17 waits for K7, and may be dropped. A task whose gate the owner turns down is dropped, and its milestone ships without it.

---

## Gates

Each is an owner decision, proposed in the task it gates, or, for K8, in what this plan leaves out. Where a proposal left a choice open, the task that meets it makes the choice and records it under its "As built".

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| K1 | The type scale, the locked text's color and the room they take, as 14.2 proposes. With them the designer grows from 728 × 790 units with five weapons, 38% of a 1080p screen's width and 73% of its height, to about 816 × 840, 43% and 78%; the mockup, with three weapons, took 38% and 65%. It changes the mockup's sizes and colors, which the owner accepted as built on 2026-10-03; [ADR-062](../ADR/ADR-062-type-scale-and-contrast.md) records what changes. As proposed (owner, 2026-10-03). What the proposal had not foreseen (owner, 2026-10-04): the three texts the contrast test found besides those it names are raised, the Queue button by darkening its green; and production narrows to 478 units so that research stays 560 and both stand clear of the designer. | 14.2 | — | decided 2026-10-04 |
| K2 | The HUD says what production and research are doing, as 15.1 proposes: a status panel under the Ore, and a line on a producer's or the Lab's selection panel, which then held only its name, hit points, construction and window buttons (Game §interface.hud.selection). As proposed. | 15.1 | — | decided 2026-10-03 |
| K3 | The selection ring a fixed width on the screen, about 3 px at 1080p at any zoom, in place of the band ADR-042 decision 3 kept. As proposed. | 15.5 | — | decided 2026-10-03 |
| K4 | How the keys with no button are taught: a Controls window on F1, and whether the main menu says so, which then held only Start skirmish and Quit (Game §session.menu). As proposed (owner, 2026-10-03), with the menu's line left to 16.4's owner run; the menu is unchanged (owner, 2026-10-05). | 16.4 | — | decided 2026-10-05 |
| K5 | The review's smaller looks, each yes or no: the gateway's gold stripe (16.1); the minimap's outlined ore, lighter fields and 300 units (16.2); a production card's build time and strength against each hull (16.3); the brightest stars kept as ADR-028's crosses, shortened, or drawn as dots (16.6). Yes to each, as proposed (owner, 2026-10-03), with 16.6's choice among its three made at the owner's run: ADR-028's crosses are kept (owner, 2026-10-05). The gateway's stripe lapsed: Phase 3 removes the gateway topics ([ADR-033](../ADR/ADR-033-research-tiers.md) decision 2), and the owner chose Phase 3 over 16.1 (owner, 2026-10-04). | 16.2, 16.3, 16.6 | — | decided 2026-10-05 |
| K6 | Short names for the four starting designs, as 16.5 proposes from the designs' own nicknames (Game §ships.designs.starting). As proposed (owner, 2026-10-03), with Line or Lancer left to 16.5 to put to the owner; Lancer (owner, 2026-10-05). | 16.5 | — | decided 2026-10-05 |
| K7 | Whether the player can scale the interface beyond the screen's fit (ADR-006), how and how far, or not now. As proposed: Ctrl+= and Ctrl+- as far as the windows fit. | 17.1 | — | decided 2026-10-03 |
| K8 | **Previewing a locked part.** A locked card takes no click, so hovering it previews nothing. A preview would show what a topic buys before it is researched. Whether it joins 15.3's preview. It was not in the review. **Decided on 2026-10-25:** it joins it, as task 15.6. | this plan's "Not in this plan" | 15.6 | decided 2026-10-25 |

---

## Milestone 14 — Legibility

### 14.1 — Text measured with its fonts, and kept inside its card

- **Goal:** the review's two overflows, and the cause under them. The layout is made without the fonts and guesses a line's width as its characters times a share of its face's size (ADR-043, under its consequences), so a line can run past its card and no test notices.
- **Scope:**
  - **Widths are measured.** `Hud::Lay` takes each typeface's advances in reference units, and every width now guessed from `MONO_ADVANCE`, `CONDENSED_ADVANCE` or `NAME_ADVANCE` is measured instead: a figure against a right edge, a panel fitted to its lines, a name cut short. `GameClient` gives the advances of the atlas it draws with (`UiPipeline::TextWidth`); the tests make them with `Neuron::RasterizeFont`, which needs no GPU. The layout still holds no GPU state (ADR-015).
  - **A name is cut to its card's width**, not to a count of characters: the chips' `CHIP_NAME_CHARACTERS` and the production card's `nameCharacters` go.
  - **A topic's prerequisites go one to a line**, each with its checkbox: "NEEDS · IMPROVED EXTRACTION", then "+ HULL PLATING". The topic cards grow to hold the most any topic has.
  - **A part card's splash note and lock line get a line each**, and the weapon row grows to hold them.
  - Sizes and colors stay as they are; they are 14.2's.
- **First, the CI runner's fonts.** The tests need Bahnschrift, Cascadia Mono or Consolas, and Segoe UI. If the runner lacks one, the font fails as ADR-030 has it fail, and the task reports it: the tests never fall back to a guess.
- **ADR:** a new one: the HUD measures its text with its fonts, and its tests measure with the same faces. It supersedes the estimate ADR-043 records.
- **Acceptance:** two new `HudTests`, over the HUD, every window, the menu and the banner, at 1920×1080 and 1280×720, with the real fonts and the longest content the game makes: a 32-character design name, every part locked and every part unlocked, the topic with the most prerequisites, a full queue, a selection of six designs.
  - `KeepsEveryTextInsideItsPanel`: every text ends inside the smallest panel it starts in.
  - `OverlapsNoTwoTexts`: no two texts' line boxes meet within a layer.
  - The first fails at Relay Archives before the fix. The existing `HudTests` pass, with any position that measuring moves updated, and the PR says which.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-04):** [ADR-061](../ADR/ADR-061-measured-text.md), which rewrote ADR-043's estimate in place. The client measures with `UiPipeline::Fonts`, after `UiPipeline::UseScale`, rather than `UiPipeline::TextWidth`. The tests rasterize with `Neuron::RasterizeUiAtlas`, which packs what `Neuron::RasterizeFont` draws. The territory panel and the alerts, which came after the review, are measured too. CI compiled it and ran its tests on 2026-10-04; `OverlapsNoTwoTexts` found two pairs of lines that met at 1280×720, a damage card's hull and figure and the Queue button's label and detail, which were spaced apart. The layout tests name every offender rather than the first. Not yet seen on screen.

### 14.2 — A type scale that reads

- **Gate:** K1.
- **Goal:** the review's legibility finding: the figures and labels a player decides by are 11–13 px at 1080p, and a locked card's text is 3.6–4.0:1 against its card.
- **Scope:** as K1 answers. The proposal:
  - **`Hud::Typefaces`:** Label 12 → 13; Figure 13 → 14, at weight 600, which Cascadia Mono has and Consolas meets with its bold; Detail 11 → 13. Name 16, Title 22 and LargeFigure 28 stay.
  - **Locked text** to about (0.20, 0.26, 0.35) in linear color: 4.6:1 on a locked card's hatching and 5.1:1 on its body, still under a third the luminance of live text. The labels stay, at 4.8–5.3:1.
  - **One place sets a face's size.** The sizes written beside calls, such as `TITLE_FACE_UNITS`, `NAME_FACE_UNITS` and the 13.0f that sizes Ore's diamond on a card, come from `Hud::Typefaces`.
  - **The windows grow to fit**, as 14.1's tests require: part cards about 220 units wide instead of 190, so the designer about 816 wide, and its card lines about 12 units taller; the research window's cards taller. Their default places move with them.
- **What the contrast test will also find:** the name field's character count, "21/32", at 3.5:1, and the labels on a window's hatched header, such as "SHIPYARD 05 · DESIGNER", at 3.6:1 on its stripes. Each is raised with the locked text, or named in the test as decorative, as K1 answers.
- **ADR:** a new one: the type scale, the locked text's color, and the room they take. The owner accepted the mockup's sizes and colors as built on 2026-10-03, and they live in `Hud.cpp` rather than an ADR, so the new ADR records what changes from what was accepted.
- **Acceptance:** `HudTests.NamesItsTypefacesAndSprites` with the new sizes; 14.1's tests; and `SetsEveryTextAtFourAndAHalfToOne`, which computes each text's contrast against the panel it starts in, as the review did, and lists the texts K1 exempts.
- **Verify:** CI; run, the designer beside the mockup; **owner run**, which closes K1's sizes.
- **As built (2026-10-04):** [ADR-062](../ADR/ADR-062-type-scale-and-contrast.md), which rewrote ADR-043's window widths in place. The sizes and the locked text are as proposed, set in `Hud::FaceUnits`. The name's count takes the labels' color and the title bar's labels the row labels'; nothing is named decorative, and the contrast test exempts no text. The owner's answers of 2026-10-04 raised a shown chip's code, the worse figure's red and the Queue button's cost, and narrowed production to 478. The part cards are 220 wide and the designer 818, but a card stays 80 units tall: measured, the larger faces need only 2 units more in a row with notes, not about 12. The designer is 818 × 810 with three weapons and 818 × 913 with five and the module row; K1's 840 predates the module row. Topic lines are 18 units apart, and a slot row's label and pick 18 units apart, which `OverlapsNoTwoTexts` found meeting at 1280×720 on the module row. CI green on 2026-10-04. Not yet seen on screen.

---

## Milestone 15 — What the player needs to see

### 15.1 — What production and research are doing, on the HUD

- **Gate:** K2.
- **Goal:** the review's main friction. A Shipyard or the Research Lab can stand idle with Ore banked, and only its window says so, one producer at a time.
- **Scope:** as K2 answers. The proposal:
  - **A status panel under the Ore**, in place of the research line (`ResearchLine`), with a line for each of:
    - the Lab: its front topic and how far it has come, waiting for Ore, or IDLE; no line before the player has a Lab.
    - the Shipyards: how many are building and how many are idle, "SHIPYARDS · 3 BUILDING · 1 IDLE"; no line before the first is finished.
  - **An idle line** is in the warning's color and says IDLE in words, so it reads without its color.
  - **A click on a line opens its window:** research, or production at the first idle Shipyard, or at the first Shipyard if none is idle.
  - **A line on the selection panel** of the player's own finished producer or Lab: "Building Small+Ion+Mass Driver · 62% · +2 queued", "Waiting for Ore", or "Idle". An enemy structure's queue stays unshown (ADR-066 decision 4).
  - The windows' default top moves down if the panel needs the room.
  - Phase 2's alerts ([ADR-059](../ADR/ADR-059-alerts-and-standing-orders.md)) are messages about events. This panel shows a standing state, and stays when they come.
  - Phase 3 gives the same selection panel the structure's level and an Upgrade button ([ADR-064](../ADR/ADR-064-structure-upgrades.md) decision 7, Game §structures.levels.panel). The line is laid out to leave room for both, and whichever lands second fits around the first.
- **ADR:** a new one: what the HUD reports of production, and what it adds to the selection panel (Game §interface.hud.selection).
- **Acceptance:** `HudTests`: the panel with no Lab and no Shipyard; a Lab researching, waiting for Ore and idle; Shipyards building and idle; a click on each line; the selection panel's line for a Shipyard building, waiting and idle, and none on an enemy's.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** [ADR-066](../ADR/ADR-066-production-status-on-the-hud.md). The Shipyards' line counts those waiting for Ore apart from those building, "Shipyards: 2 building, 1 waiting for Ore, 1 IDLE", since a Shipyard that waits is neither. The Lab's line comes once the Lab is finished, as the Shipyards' does. A waiting producer's selection line names its job, "Building Swarm · waiting for Ore". The Command Station gets the selection line and no status line. The panel's place is kept for two lines, so the windows open at 148 units from the top, where they opened at 128. Written without MSVC; not yet seen on screen.

### 15.2 — Keys on the buttons

- **Goal:** D, P and R open the windows (Game §interface.windows.behavior), and no button says so.
- **Scope:**
  - **A button that opens a window shows its key** at its right end, as a cap: an outline, and the letter in the label face. Production shows P, Ship designer D and Research R. A Build button, which has its cost there and no key, is unchanged.
  - **The window keys move out of `GameClient.cpp`** into a header the HUD reads too, so a cap cannot disagree with its key. Task 16.4 gathers the other keys there.
  - No sprite or character is added (ADR-030).
- **ADR:** none.
- **Acceptance:** `HudTests`: each window button shows its key, and a Build button none; 14.1's tests.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** the keys are in `GameApp/KeyBindings.h`. A cap is 20 units square, outlined and lettered in the row labels' color. Written without MSVC; not yet seen on screen.

### 15.3 — The designer's preview in figures

- **Goal:** while a part is hovered, each of the designer's figures is replaced by the previewed one in green or red (Game §ships.designer.figures), so the current figure is gone and the direction is told by color alone.
- **Scope:**
  - **A bar's figure** reads the previewed value and its change as a signed number, "242 (+44)", colored as now.
  - **A damage card** adds its change, "+4.5", where it fits at 14.2's sizes.
  - **In ASCII.** The atlas holds printable ASCII, · and × (ADR-030), so a minus sign or an arrow would show as "?".
  - The sign says which way a number moved, and the color whether that is better. Cost and build time, where lower is better, read right without the color.
- **ADR:** none: Game §ships.designer.figures already asks for the change against the design picked.
- **Acceptance:** `HudTests`: the figures for a part that raises, lowers and keeps a number, cost and build time included; 14.1's tests with the widest preview.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** a number that keeps reads alone, "120", with no "(+0)". The change is the difference of the two figures as written, rounded first. To fit "1,680 (+1,482)", the bars narrow from 116 units to 90 and the figures end at 316, where they ended at 290. A damage card's change stands at its right, on the large figure's line. Written without MSVC; not yet seen on screen.

### 15.4 — Windows open where there is room

- **Goal:** the research window opens at its slot beside production's, mid-screen, even when production is closed.
- **Scope:** a window not yet moved takes the first of the two slots under the Ore that no open window holds, production's and then research's. The designer keeps its top-right corner, and a moved window stays where it was left. `Hud::Lay` without a manager lays the windows out as it does now, which the HUD's tests rely on (ADR-031).
- **ADR:** none: where a window first stands is the HUD's (ADR-031, decision 2).
- **Acceptance:** `HudTests`: research alone at the first slot; production then research, and research then production, side by side in the order they opened; a moved window kept.
- **Verify:** CI; run.
- **As built (2026-10-05):** `WindowManager` keeps which slot each window took as it opened, and frees it when the window closes or is moved, so a window keeps its place while the other opens or closes. The second slot stands beside where the other window's width would end, so research in the first and production in the second still clear the designer. [ADR-031](../ADR/ADR-031-floating-windows.md) is rewritten for it. Written without MSVC; not yet seen on screen.

### 15.5 — The selection ring at a fixed width

- **Gate:** K3.
- **Goal:** a selected structure's ring is a band 15% of its radius wide, which ADR-042 decision 3 kept so that a selection stays the strongest mark on the ground. Under a Shipyard that is about 15 px of saturated green among lines a pixel wide.
- **Scope:** as K3 answers. The proposal: the selection's green ring, and attack-move's amber one, is a band about 3 px wide at 1080p at any zoom, scaled with the HUD's scale, at the radius it has now, and still the strongest mark on the ground. The placement ghost keeps its band, and a rig's selection ring stays its asteroid's (ADR-042 decision 4). How it is drawn is the ADR's: the footprint ring's one-pixel lines, several a pixel apart, or a band whose width follows the distance.
- **ADR:** a new one, superseding ADR-042 decision 3 for the selection.
- **Acceptance:** `GameAppTests` for what is pure: the band's width in meters, at a distance from the camera, comes to the same pixels near and far.
- **Verify:** CI; run, near and far; **owner run.**
- **As built (2026-10-05):** [ADR-067](../ADR/ADR-067-selection-ring-at-a-fixed-width.md), which rewrites ADR-042 decision 3 in place. The ring is one-pixel lines a pixel apart, as many as 3 units at the HUD's scale, from `Camera::MetersPerPixelAt`. Written without MSVC; not yet seen on screen.

### 15.6 — A locked part previews what its topic buys

- **Gate:** K8.
- **Goal:** hovering a locked card previews the design it would make, as hovering an open one does (15.3), so the player sees what a topic buys before researching it.
- **Scope:**
  - A locked card answers the pointer: its figures replace the bars' and the damage card's with the previewed ones and their signed changes, as 15.3 lays them out, at the player's research as it stands.
  - It still takes no click, and its hatching and "RESEARCH · LARGE HULL" line stay. It does not light as a pickable card does (ADR-088).
  - The help line names the topic while it is hovered, so the preview is not read as a pick.
- **ADR:** [ADR-017](../ADR/ADR-017-research-and-the-designer.md) decision 7, edited in place; Game §ships.designer.figures.
- **Acceptance:** `HudTests`: hovering a locked hull, drive, weapon and module previews its figures; a click on it changes nothing; 14.1's tests with a locked card hovered.
- **Verify:** CI; run; **owner run.**

---

## Milestone 16 — Polish

The milestone's looks, 16.1–16.4 and 16.6, share one ADR, as the owner's second look at Phase 1's screen shared ADR-046. Task 16.5 changes the tuning data and has its own.

### 16.1 — The windows' headers and bodies

- **Gate:** none.
- **Scope:**
  - **The designer's header** reads "QUEUE 0 / 5" over its slots and "BUILT 0" apart, as the other windows write a queue (`QueueRows`).
  - **Arrows flank what they step:** "< SHIPYARD 05 >". The production window's move from beside its "PRODUCTION" label to either side of the producer's name, and the designer's to either side of the Shipyard's. They grow from 18 units square to 24 (`SMALL_BUTTON_UNITS`), and the research window's scroll arrows with them.
  - **Research says where its page is:** "TOPICS · 1-10 OF 21", with an ASCII hyphen (ADR-030).
  - **A window's body is opaque:** `WINDOW_COLOR`'s alpha 0.97 → 1.
  - **The gateway's mark is left as it is.** Phase 3 removes the gateway topics ([ADR-033](../ADR/ADR-033-research-tiers.md) decision 2, [ADR-064](../ADR/ADR-064-structure-upgrades.md) decision 10), and their gold outline goes with them (owner, 2026-10-04).
- **ADR:** the milestone's.
- **Acceptance:** `HudTests`: the headers; the arrows' places and size; the page line on the first, a middle and the last page.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** [ADR-068](../ADR/ADR-068-interface-polish.md). The ships built read "BUILT · 4", in the labels' grammar, left of the slots. The designer's title bar reads "< SHIPYARD 05 > · DESIGNER". The saved-design chips narrow from 220 to 216 so their arrows, now 24 square, clear the third chip. Written without MSVC; not yet seen on screen.

### 16.2 — The minimap's marks and size

- **Gate:** K5.
- **Scope:**
  - **An ore asteroid is an outlined square** in Ore's darkened gold, not a filled one, so that it differs from an enemy's mark in shape as well as brightness. This changes ADR-043 decision 7's fill, and keeps its colors.
  - **A field stands at 2:1 or more** against the map, from 1.3:1, and a dry asteroid at 3:1, from 2.5:1.
  - **The minimap is 300 units square**, from 260: about 18 m a unit on the 5 km map, from 20.
- **ADR:** the milestone's.
- **Acceptance:** `HudTests`: an ore mark outlined; `MapPointAt` and `MinimapPixelOf` at the new size; the marks' contrast against the map, computed as 14.2's test computes it.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** [ADR-068](../ADR/ADR-068-interface-polish.md). The outline is 2 units wide, and a dry asteroid is outlined too. A field is 2.1:1 and a dry asteroid 3.1:1, both computed. Written without MSVC; not yet seen on screen.

### 16.3 — What a production card says

- **Gate:** K5.
- **Scope:** a Shipyard's card adds its design's build time, and three short bars marked S, M and L for how it does against each hull. A bar's length follows the designer's rating against that hull, three segments for Good, two for Fair and one for Poor, so that the bars read without their colors. The Constructor's card adds its build time. Phase 3 dims a card whose hull is above the Shipyard's level, with the reason ([ADR-064](../ADR/ADR-064-structure-upgrades.md) decision 9); the card is laid out to leave room for it.
- **ADR:** the milestone's.
- **Acceptance:** `HudTests`: the time and the bars of two designs that differ; 14.1's tests.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** [ADR-068](../ADR/ADR-068-interface-polish.md). The bars are marked with each hull's initial from the snapshot. The Constructor's time was not in the snapshot, so `Snapshot::constructorBuildSeconds` carries it, and the protocol version goes from 5 to 6. A card is 74 units tall, where it was 48. Written without MSVC; not yet seen on screen.

### 16.4 — A controls window

- **Gate:** K4.
- **Scope:** as K4 answers. The proposal:
  - **F1 opens and closes a Controls window** in the windows' look, a fourth `WindowKind`. It lists every key and mouse action the game reads: selecting, orders, control groups, the camera, Alt's health bars, the windows' keys and Esc.
  - **Its lines come from the keys themselves.** The keys the input code reads, in `Camera`, `PlayerControls` and `GameClient`, join 15.2's header, and the window reads them from there, so the two cannot disagree.
  - **The main menu** is unchanged, or gains a line "F1 · CONTROLS" under its buttons, as K4 answers.
- **ADR:** the milestone's.
- **Acceptance:** `HudTests`: the window names every key in the header; `WindowManagerTests` with a fourth kind; 14.1's tests.
- **Verify:** CI; run; **owner run.**
- **As built (2026-10-05):** [ADR-068](../ADR/ADR-068-interface-polish.md). The window opens in the middle of the screen, takes no slot under the Ore, and lists 26 actions under three headings. The menu is unchanged (owner, 2026-10-05). The name field's typing keys are text entry and are not listed. Written without MSVC; not yet seen on screen.

### 16.5 — Short names for the starting designs

- **Gate:** K6.
- **Scope:** as K6 answers. The four starting designs take names given in the tuning data. The proposal is the designs' own nicknames: Swarm for Small+Ion+Mass Driver, Picket for Small+Ion+Lance, Brawler for Medium+Ion+Mass Driver, and Line, or the mockup's Lancer, for Medium+Ion+Lance. A design without one keeps its components' name (`DesignName`), as the AI's designs do. The match log records components beside a name (ADR-038), and the balance check names designs by their codes, so neither changes.
- **ADR:** a new one: where a starting design's name lives in the tuning data (ADR-008).
- **Acceptance:** `TuningTests` read the names and refuse an invalid one; `DesignTests.StartingDesignsAreTheFourOfTheFirstMinutes`, and every test that names a starting design, follow; the chips show the names whole.
- **Verify:** CI; run.
- **As built (2026-10-05):** [ADR-069](../ADR/ADR-069-starting-design-names.md). `Tuning.json` gains an optional `startingDesigns` list: Swarm, Picket, Brawler and Lancer (owner, 2026-10-05). Match setup saves them under those names. The match log now names them so too, beside their components. Written without MSVC; not yet seen on screen.

### 16.6 — The sky's crosses

- **Gate:** K5.
- **Scope:** as K5 answers, one of: keep ADR-028's crosses; shorten them, to 3–8 px in radius from 6–16; or draw the brightest 25 stars as dots, `StarPipeline`'s `Spot` (ADR-022). The owner chose the crosses on 2026-10-02 (ADR-028) and their colors in the second look at Phase 1's screen (ADR-028 decision 3). The review's point is only that, on the playfield, they read as markers.
- **ADR:** the milestone's, superseding ADR-028 decision 3 if they change.
- **Acceptance:** `StarfieldTests` for what changes.
- **Verify:** run; **owner run.**
- **As built (2026-10-05):** nothing changes. The owner chose to keep ADR-028's crosses on 2026-10-05, and [ADR-068](../ADR/ADR-068-interface-polish.md) records it.

---

## Milestone 17 — The interface's own scale

### 17.1 — An interface scale the player sets

- **Gate:** K7.
- **Goal:** Xbox Accessibility Guideline 101 asks that text can be enlarged to 200% of its 18 px, and the interface scales only with the screen (ADR-006).
- **Scope:** as K7 answers. The proposal:
  - **Ctrl+= and Ctrl+- step a factor** on `Hud::Scale`: 100%, 110% and 125%, and further only while every window still fits the screen.
  - **The factor lasts until the game closes**, as a window's place does, and nothing is written to disk (Game §interface.windows.behavior).
  - **The atlas is drawn again** at the new scale, as it already is when the scale moves a font by a whole pixel (ADR-030).
  - **What it cannot reach:** after 14.2 the designer, with five weapons, fills a 1080p screen's height at about 125%. 200% would need the designer to scroll or reflow, which is not proposed.
- **ADR:** a new one, beside ADR-006's scale.
- **Acceptance:** 14.1's and 14.2's tests at every step, at 1920×1080 and 1280×720; `HudTests` that a moved window stays on the screen across a step.
- **Verify:** CI; run at each step; **owner run.**
- **As built (2026-10-05):** [ADR-070](../ADR/ADR-070-interface-scale.md). The steps are 100%, 110%, 125%, 150%, 175% and 200%. A step up is taken only while the designer, laid out for the match's components, and the Controls window fit between the screen's margins. That is 110% in a match on any 16:9 screen, since the designer with five weapons and the module row is 913 units tall, and 125% on 2880×1920. A new screen size, or a match starting, takes the factor down to the largest step that fits. The factor scales the HUD, the windows and the menu, not the world's marks. 14.1's and 14.2's tests run at every step the longest content fits. Written without MSVC; not yet seen on screen.

---

## Not in this plan

- **What the enemy's fleet is armed with.** The review asked whether a player can tell from a battle. That is what scouting shows, with the Sensor Array (Game §ships.modules), not the HUD's.
- **Flashes and motion.** Stills cannot show them. A battle at the peak ship counts, watched for flashing, belongs in an owner run, and what it finds becomes a task.
- **The rest of the warm colors.** Gold, amber, the Fair rating, hurt health and the attack-move ring are all warm, but each is used consistently; only the gateway's edge misled, and Phase 3 removes the gateways.
- **Keys for building.** No Build button has a key. Giving them keys is a design question, not a finding of the review.
