# Outpost Commander — Second Interface Plan: The Screen Shows the Game

Status: **draft** · proposed on 2026-10-08, awaiting the owner's acceptance and gates V1–V8 · From a UI/UX review of five screenshots of a match against the AI on the 10 km map, made on 2026-10-08 against [the Phase 5 design](OutpostCommander-Phase5.md), the designs it amends, and what [the first interface plan](ImplementationPlan-Interface.md) built

[The first interface plan](ImplementationPlan-Interface.md) made the text legible and the HUD say what production and research are doing. This review found the next layer. The main view does not show the state a match is decided by: territory, sight, and what the player only remembers. The HUD reports state in sentences, and in a warm color that also means the enemy.

This plan puts the review's recommendations in order, as a queue of tasks. It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the ADRs the task names, then the Phase 5 design's §11** for what the client gains beside this plan. The review is not in the repository; what it found is summarized below, with how each figure was found.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone (owner, 2026-09-30), in the milestone order below, and where V1 places it against Phase 5.
3. **A gate is an owner decision.** The task does not start until the owner has answered, and the answer is written into the gates table, with its date, first. An answer that changes what a design or an ADR says the interface shows is recorded in the task's ADR, naming what it changes.
4. **ADRs are edited in place** (AGENTS.md §6). A new one takes the next free number when its task lands; Phase 5's milestones 38 and 39 take numbers too, so this plan reserves none.
5. **Every task is presentation.** It is built, its tests run, and the game is run and looked at, at 1920×1080 and at 2880×1920, as the first plan's tasks were. The owner's run accepts it. An agent in a cloud container has no Windows, no MSBuild and no GPU: the HUD's tests run there in a standalone build with substitute fonts (ADR-043), and the rest is CI's and the owner's. The PR says which ran where.
6. **The first plan's guards hold for every task:** no text outside its panel or over another (`KeepsEveryTextInsideItsPanel`, `OverlapsNoTwoTexts`), and every text at 4.5:1 against its panel (`SetsEveryTextAtFourAndAHalfToOne`).
7. **Where a task rewrites what the first plan built** and the owner has not yet run, such as its 15.1 and 16.2, the owner's run of the rewrite closes both.

Task numbers carry the prefix UI, as the self-play plan's carry SP, since [the horizon plan](ImplementationPlan-Horizon.md) has given milestones 40–51 to Phases 6 and 7. Gates are lettered V.

---

## What the review found

| # | Finding | Priority | Task |
|---|---|---|---|
| 1 | **Territory does not show in the main view.** `GameClient` draws no sector border and no node. The minimap washes and outlines the held and pirate-guarded sectors only, never a neutral sector or a node. "Cut off, it earns half" is a line on a selected Relay's or rig's panel (`Hud.cpp`, `DescribeSelection`) and nowhere else. A player finds a node by arming a Relay and sweeping the cursor. | P1 | UI1.1, UI1.4 |
| 2 | **A remembered structure looks like a seen one.** `GameClient.cpp` never reads `EntityView::remembered`. A remembered structure keeps its last-seen health and build bars, and its only mark is the fog's darkening, which darkens the space around it as much. Shot 5's base, 0–5% built, is a memory, and nothing says how old. | P1 | UI1.2 |
| 3 | **The edge of sight barely shows.** The fog is a black mask over a black scene: a grid line peaks at 27 of 255 in sight and at 15 where the player has seen before, and the stars dim by the same share. | P1 | UI1.3 |
| 4 | **The warning is the enemy's color.** `WARNING_COLOR` stands at an sRGB hue of 17.5° and `ENEMY_COLOR` at 14.6°. In shot 1 "3 IDLE" stands directly over the enemy's "2" and "642", in what reads as the same salmon. | P1 | UI2.1 |
| 5 | **The Shipyards' line is a warning in all five shots.** In shots 1–4 the fleet is at its cap, 29 / 30 with a Medium hull taking 2, so the idle Shipyards could add one Small hull at most. In shot 5 four stand idle with room for 11 points and 25,586 Ore banked, and the line looks the same. The cap is raised by the Command Station's level ([ADR-071](../Design/ADR/ADR-071-fleet-cap.md)), but a click on the line opens production. | P1 | UI3.1 |
| 6 | **The build bar is the player's blue**, at an sRGB hue of 202.5° against the player's 207.8°, on the enemy's structures too (shot 5). | P2 | UI2.2 |
| 7 | **The status panel's edge moves.** It is as wide as its longest line, 515 px in shot 1 and 322 px in shot 5, between two panels 260 units wide, so its edge moves as its text does. Its state is four facts in one sentence. | P2 | UI3.2 |
| 8 | **The territory panel needs working out.** "3 / 10 : 2" puts the cap between the sides. Tickets show what is left, not the drain: at 3 nodes to 2 the enemy loses 1 ticket every 3 s and is out in 32:06; in shot 5, at 3 to 1, it loses 2 every 3 s and is out in 12:09. | P2 | UI3.3 |
| 9 | **Bars on the ground turn with the camera.** Health and build bars are strips laid along world x on the world −z side of their entity (`GameClient.cpp`, `DrawHealthBars`). After a half-turn of the camera (Q, E) every bar stands above its unit and fills from the right; after a quarter-turn it stands on end. The pitch foreshortens its thickness, to 0.64 at 40°. In shots 3–4 the bars sit inside the selection rings, in nearly the same green. | P2 | UI4.1 |
| 10 | **Warships get one button.** For warships the command panel holds one button, "Retreat at 25%", 420 px from the selection it acts on, and A, H, T and S have no button. The retreat button steps through three states and shows one. | P2 | UI4.2, UI4.3 |
| 11 | **No HUD control answers the pointer.** The hovered action reaches only the designer's preview (`Hud::Describe`), so the status panel's lines, which take clicks (ADR-066), look like labels. | P2 | UI4.4 |
| 12 | **The selection panel repeats itself and hides who is hurt.** "7 ships" over "7 × Medium+Ion+Lance+Sensor Array" says the count twice and leads with the components' name, the designer's default (`Designer::Name`). "3,599 / 4,095" hides which ships are near their retreat threshold. | P2 | UI4.5 |
| 13 | **The rocks outweigh the fleet.** In shot 1 the two rock fields cover 15,400 and 22,465 lit pixels, against 851 for the group of six ships at the top. The ships are twice as bright a pixel; the rocks are 18–26 times larger. | P2 | UI5.1 |
| 14 | **Every field is the same rosette:** a rock and a ring of six at fixed shares and a fixed angle (`GameClient.cpp`, `FIELD_RING_ROCKS`). The repetition shows in shots 1 and 5, and the gaps look passable where the server blocks the whole circle. | P2 | UI5.2 |
| 15 | **A blast throws amber diamonds,** the shape and color of Ore's glyph (shot 4). | P3 | UI2.3 |
| 16 | **The minimap's view is the wrong shape.** It is the axis-aligned box around the view's ground corners (`Hud::Lay`, "the view's outline"), so it overstates the near side and does not turn with the camera. | P3 | UI5.3 |

**How the figures were found:**

- **The screenshots** are 1917 × 1078 PNGs of a 1080p screen.
- **Brightness** is the Rec. 709 luma of the sRGB pixels.
- **A grid line's brightness** is the median of each line's peak along a row: 27.3 in shots 1–3, which are in sight, and 15.4 in shot 5. ADR-024's seen-before shade of 0.55, applied in linear light to 27.3, gives 15.6.
- **Lit pixels** are those above 35, in a box around each field and around the ships.
- **A panel's width** is the distance between its brackets.
- **Hues** are those of `Hud.cpp`'s and `GameClient.cpp`'s linear colors, encoded to sRGB.
- **The ticket times** follow ADR-057's rule with `Tuning.json`'s numbers: the drains needed, rounded up, times 3 s.
- **What the client draws and does not** was read from the code.
- **Motion, flashes and the camera's feel** cannot be judged from stills.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| UI1.1 | Sectors and nodes in the world | — | V2 | todo |
| UI1.2 | Memories that look like memories | — | V3 | todo |
| UI1.3 | Sight that shows | UI1.2 | V2 | todo |
| UI1.4 | The minimap's territory | UI1.1 | V2 | todo |
| UI2.1 | A warning is a chip | — | V4 | todo |
| UI2.2 | Build progress in a neutral color | — | V4 | todo |
| UI2.3 | The blast and Ore's glyph | — | V4 | todo |
| UI3.1 | What holds the Shipyards back, and what frees them | UI2.1 | V5 | todo |
| UI3.2 | One column, and state in bars | UI3.1 | V5 | todo |
| UI3.3 | Nodes and tickets that read | UI2.1 | V5 | todo |
| UI4.1 | Bars on the screen | UI2.2 | V7 | todo |
| UI4.2 | Commands beside the selection | — | V6 | todo |
| UI4.3 | Retreat as three choices | UI4.2 | V6 | todo |
| UI4.4 | Controls that answer the pointer | — | — | todo |
| UI4.5 | One count, one bar per ship, a short name | UI3.1 | V6 | todo |
| UI5.1 | Rocks that stand back | — | V8 | todo |
| UI5.2 | Fields of their own | — | V8 | todo |
| UI5.3 | The minimap's view as its four corners | — | — | todo |

### Milestone order

UI1, UI2, UI3, UI4, UI5.

- **UI1 goes first.** It is what the review found most, and V1 asks for it before Phase 5's week.
- **UI2 goes before UI3,** since the status panel's warning is UI2.1's chip.
- **UI4's bars take UI2.2's neutral build bar.**
- **UI5 is looks,** item by item as V8 accepts them. UI5.3 can go at any time.

A task whose gate the owner turns down is dropped, and its milestone ships without it.

---

## Gates

Each is an owner decision, proposed here and in the task it gates. None is decided yet.

| Gate | Decision | Proposed in | Blocks |
|---|---|---|---|
| V1 | **Where this plan stands against Phase 5.** Proposed: UI1, UI2.1 and UI3.1 land before Phase 5's 39.3, the owner's week. A world played for a week is where the map and the memories matter most, and W6 judges the rhythm by it. The rest goes beside Phase 5 or after it. Phase 5's 38.1 moves the alerts' source to the server, and its 38.3 adds an orders window. UI2.1 restyles the alerts' newest line, and UI4.2 the selection's commands; whichever lands second fits around the other. | this plan | the order |
| V2 | **Territory and sight in the world** (UI1.1, UI1.3, UI1.4). Sector borders as one-pixel lines in the holder's color. A neutral border brighter than the grid and never wider, the way back ADR-028 left for major lines. A mark on every node, and a node the player could claim now in the player's color. A suppressed and a cut-off border, each told apart from a held one and from each other by pattern, not color. The borders drawn over the fog, as the minimap's are. The same lattice, nodes and states on the minimap. And a seen-before shade of about 0.8 on the ground, up from 0.55, which takes the grid outside sight from 15 of 255 to about 7, computed, against 27 in sight and about 4 where never seen. It changes ADR-028 decision 1's "all alike" and ADR-024 decision 9's shade. | UI1.1, UI1.3 | UI1.1, UI1.3, UI1.4 |
| V3 | **Memories** (UI1.2). A remembered entity drawn after the fog, as lines without faces and without bars, with its age on hover and on its selection panel. And where the age comes from. Proposed: the server keeps the tick each remembered structure was last seen, so the age survives a save and a seat taken again (Phase 5). It adds to what `Simulation` holds, so `WORLD_STATE_VERSION` and `PROTOCOL_VERSION` rise (AGENTS.md R18). The other way, the client noting when a structure turned remembered, changes neither, but knows no age after a join. | UI1.2 | UI1.2 |
| V4 | **One meaning per color** (UI2.1–UI2.3). A warning as a chip: the word, dark, on a filled tag of the warning's color, never plain text in that color. It marks an idle line's IDLE, an income of nothing and the newest alert; the enemy's figures stay plain text. Build progress in a neutral light gray. For the blast and the glyph, one of three: Ore's glyph becomes another CPU-drawn shape in the same gold (proposed); the particles become round spots, which reverses the owner's choice in ADR-026; or both stay. | UI2.1 | UI2.1, UI2.2, UI2.3 |
| V5 | **The status and territory panels** (UI3.1–UI3.3). The Shipyards' line reports what binds them: the cap and the station level that lifts it, Ore, or idle with room. It warns only when an idle Shipyard could start a ship. A click at the cap selects the Command Station. The column's panels take one fixed width, with research and the fleet as bars. The nodes read "3 : 2", with "cap 10 of 25" as a label, under a drain row saying who loses how much a minute and when they reach nothing. The drain row needs the drain's two numbers in the snapshot, so `PROTOCOL_VERSION` rises. | UI3.1 | UI3.1, UI3.2, UI3.3 |
| V6 | **Selection and commands** (UI4.2, UI4.3, UI4.5). The command panel stands against the selection panel's right edge, the pair centered at the bottom, so that the two read as one. Attack-move, Hold sector, Patrol and Stop buttons, each with its key's cap, for a selection with a warship. The retreat as three choices. One bar per ship, up to 24, a click on one selecting it. A new design's default name: where its hull, drive and weapon are a starting design's, that design's name with its module's code, "Lancer+SA". | UI4.2 | UI4.2, UI4.3, UI4.5 |
| V7 | **Bars on the screen** (UI4.1). Health and build bars drawn by the HUD above their entity, 32 × 5 reference units at any zoom, filling left to right however the camera has turned, under every panel and window. ADR-047's least size becomes the size. It rewrites ADR-028 decision 7's place and ADR-047 decision 4. | UI4.1 | UI4.1 |
| V8 | **The scene's weight** (UI5.1, UI5.2). Rock faces at a shade of their own, about half the 0.3 they share with the ships now, with their lines kept. Every field's rocks laid out from its identifier. The gaps between a field's rocks filled with smaller ones, so that the field looks as solid as the circle the server blocks. | UI5.1 | UI5.1, UI5.2 |

---

## Milestone UI1 — What the world shows

### UI1.1 — Sectors and nodes in the world

- **Gate:** V2.
- **Goal:** the review's first finding. The match is won on nodes, and rigs and Shipyards work only in held sectors (ADR-056 decision 5), but the main view shows no sector and no node.
- **Scope:** as V2 answers. The proposal:
  - **Every sector's border is drawn** from `Snapshot::sectors`, as one-pixel lines with `MeshPipeline::DrawLines`, as the grid is (ADR-028 decision 1):
    - a held sector's in its holder's color, inset a few meters, so that where two holders' sectors meet both colors show side by side, as a front line;
    - a neutral sector's brighter than the grid, never wider;
    - a pirate-guarded sector's in the pirates' color;
    - a suppressed sector's and a cut-off sector's in patterns of their own, so that neither depends on color. The ADR picks the patterns.
  - **Every node is marked** with a small ring at `SectorView::node`:
    - neutral while free, and in its holder's color when held;
    - in the player's color when the player could claim it now: free, not guarded, next to a sector the player holds, and under its node cap.
    - The rule is the one the Relay's ghost uses (`PlaceGhost`, ADR-056 decision 11). It moves to where both read it, so that the mark and the ghost cannot disagree.
  - **The borders and nodes are drawn after the fog mask.** Every player sees the territory, fog or not (ADR-056 decision 10), as the minimap draws its outlines over the fog.
  - **The lines are worked out again only when a sector's holder or state changes,** not every frame. How the mesh reaches the GPU is the ADR's: a static upload (ADR-048) or a per-frame one, as `DrawTriangles` does (ADR-026 decision 5).
  - **The grid stays,** its color and spacing unchanged.
- **ADR:** a new one, territory drawn in the world. It rewrites ADR-028 decision 1's "all alike", and gives ADR-056 decision 11 the world's part.
- **Acceptance:** `GameAppTests`, for what is pure:
  - the border segments of a held, a neutral, a guarded, a suppressed and a cut-off sector, and of two holders' sectors meeting;
  - the node marks: a claimable node marked exactly where the Relay's ghost would be green, and none at the node cap;
  - the lines rebuilt on a change and not otherwise.
- **Verify:** CI; run, at the default zoom and the widest; **owner run.**

### UI1.2 — Memories that look like memories

- **Gate:** V3.
- **Goal:** the review's second finding. A remembered structure is drawn as a seen one, with last-seen bars, and nothing says how old it is.
- **Scope:** as V3 answers. The proposal:
  - **A remembered entity is drawn as a memory.** That is `EntityView::remembered`: an enemy structure or a derelict, ADR-024 and ADR-074.
    - It is drawn after the fog mask, which then no longer darkens it, as lines without faces, at a fixed strength, in its side's color.
    - It has no health bar, no build bar and no resting ring.
  - **Its age is shown.** Hovering it shows "Last seen 4:12 ago". Its selection panel opens with the same line, then what was seen, such as "34% built when seen".
  - **The age comes from the server.** `EntityView` gains the tick it was last seen. The server keeps it with each remembered structure in `Simulation`'s remembered views (ADR-024), so it survives a save and a seat taken again.
    - `WORLD_STATE_VERSION` rises, and the new layout's hash is recorded beside the old (AGENTS.md R18).
    - `PROTOCOL_VERSION` rises.
    - If V3 chooses the client's way instead, `GameClient` notes the tick each entity turned remembered, and an entity it never saw turn shows no age.
  - **On the minimap,** a remembered structure's mark differs from a seen one's by more than its color, and from an ore asteroid's outline by its shape. The ADR picks how.
- **ADR:** rewrites ADR-024 decision 9's "what stands in the fog is darkened with it" for remembered entities, and its list of what a snapshot carries. With the server's way, ADR-077 for the state's new version.
- **Acceptance:**
  - `GameAppTests`: a remembered structure is drawn as lines, with no faces and no bar, after the mask; the hover text and the panel's line, with an age.
  - `HudTests`: the minimap's mark.
  - With the server's way:
    - `GameLogicTests`: the tick is the last one in sight, it stands while the structure is remembered, and it survives a save and a load;
    - `WorldStateTests`: the new version's hash;
    - `WireFormatTests`: the field.
- **Verify:** the container's `GameLogicTests` where the server changes; CI; run; **owner run.**

### UI1.3 — Sight that shows

- **Gate:** V2.
- **Depends on:** UI1.2. A memory is drawn after the mask, so a deeper shade does not bury it.
- **Goal:** the review's third finding. Outside sight the grid falls only from 27 of 255 to 15.
- **Scope:** as V2 answers. The proposal:
  - **The ground's seen-before shade goes from 0.55 to about 0.8.** The grid and the stars outside sight fall to about 7 of 255, so the edge of sight is where they go out. Never seen stays 0.9.
  - **The minimap keeps 0.55,** where telling explored from never seen matters for scouting. The ground and the minimap read one texture (ADR-052), so either the ground's mask maps the stored shade or the minimap's draw does. The ADR picks which.
- **ADR:** rewrites ADR-024 decision 9's shades, and ADR-052 if the mapping lives in a shader.
- **Acceptance:** `FogOfWarTests` with the shades.
- **Verify:** CI; run, measuring the grid's brightness in and out of sight as the review measured it; **owner run.**

### UI1.4 — The minimap's territory

- **Gate:** V2.
- **Depends on:** UI1.1, for the claim rule.
- **Goal:** the minimap half of the review's first finding.
- **Scope:** as V2 answers. The proposal:
  - **Every neutral sector is outlined faintly,** so that the lattice shows.
  - **Every node is a dot,** in its holder's color or neutral.
  - **A node the player could claim is ringed in its color,** by UI1.1's rule.
  - **A cut-off sector is marked apart from a suppressed one,** whose hatching stays, by pattern rather than color.
  - **The marks' contrast** is checked as task 16.2 checks the minimap's marks.
- **ADR:** UI1.1's. ADR-056 decision 11 and ADR-068's minimap rewritten in place.
- **Acceptance:** `HudTests`: the lattice; the nodes; a claimable node by the same rule as the ghost; a cut-off sector apart from a suppressed one; the marks' contrast.
- **Verify:** CI; run; **owner run.**

---

## Milestone UI2 — One meaning per color

The milestone's tasks share one ADR: one meaning per color.

### UI2.1 — A warning is a chip

- **Gate:** V4.
- **Goal:** the review's fourth finding. The warning's salmon is the enemy's, so the player's own problem reads in the enemy's color.
- **Scope:** as V4 answers. The proposal:
  - **The HUD gains a chip:** a word in the windows' dark navy on a filled tag of `WARNING_COLOR`, set inside a line of text. Nothing else in the HUD is drawn that way, so a warning differs from the enemy's figures by shape and fill, not by a hue 3° away.
  - **It marks three things:**
    - a status line's IDLE (ADR-066 decision 2);
    - an income of nothing, "+0/s" (ADR-047 decision 5);
    - the newest alert (ADR-059 decision 2), whose line otherwise takes the text's color.
  - **The enemy's figures stay plain text** in `ENEMY_COLOR`. A refused name in the designer keeps its color, since it stands in its own field, away from any enemy figure.
  - **Phase 5's 38.1** moves the alerts' source to the server's events. The chip is the HUD's either way.
- **ADR:** the milestone's. It rewrites ADR-066 decision 2, ADR-047 decision 5 and ADR-059 decision 2 in place.
- **Acceptance:** `HudTests`: each of the three drawn as a chip, with a panel under its word; no text in `ENEMY_COLOR` has one; 14.2's contrast test with the chip's text against its fill.
- **Verify:** CI; run; **owner run.**

### UI2.2 — Build progress in a neutral color

- **Gate:** V4.
- **Scope:**
  - **`BUILD_BAR_COLOR` becomes a neutral light gray** on every side's structures, in place of a blue one shade from the player's.
  - **The HUD's own progress fills** (`BAR_FILL_COLOR`) stand on the player's own buttons and stay.
  - **UI4.1 carries the color** to the bars on the screen.
- **ADR:** the milestone's.
- **Acceptance:** the color named once and used for every side.
- **Verify:** run, on an enemy structure under construction (shot 5's case) and an own one.

### UI2.3 — The blast and Ore's glyph

- **Gate:** V4.
- **Goal:** a blast's amber diamonds are Ore's glyph in shape and color.
- **Scope:** as V4 answers, one of three:
  - **Ore's glyph becomes another shape** (proposed), drawn by `Neuron::DrawSprite` on the CPU (ADR-030 decision 5), in `GOLD_COLOR`, everywhere Ore is written (ADR-043 decision 3). The owner picks it at the run from two or three drawn.
  - **The particles become round spots,** the `Spot` the exhausts are drawn with. That reverses the texture the owner chose in ADR-026.
  - **Nothing changes,** and the task closes as task 16.6 did.
- **ADR:** the milestone's. It rewrites ADR-043 decision 3's glyph or ADR-026 decision 1.
- **Acceptance:** `NeuronClientTests`: the new sprite's shape. `HudTests`: `WritesOreOneWay` with it.
- **Verify:** CI; run; **owner run.** The task may be dropped.

---

## Milestone UI3 — The column under the Ore

### UI3.1 — What holds the Shipyards back, and what frees them

- **Gate:** V5.
- **Depends on:** UI2.1.
- **Goal:** the review's fifth finding. The line is a warning when nothing can be done, looks the same when something can, and does not lead to the remedy.
- **Scope:** as V5 answers. The proposal:
  - **The Shipyards' line says what binds them,** first case first:
    - **At the cap.** That is, a Shipyard waits for the fleet cap, or the fleet has room for none of the player's saved designs (`DesignView::hull`, `HullView::commandPoints`).
      - It reads "Fleet 29 / 30, at the cap · Station L3 → L4: +10", with the next level's cost, from `StructureTypeView::levels`.
      - While the station is upgrading it reads "upgrading, 62%". At the station's top level it reads "Fleet 50 / 50, at the cap".
      - It is plain text, since its remedy is a level, not a queue.
    - **Otherwise,** the counts, as now: building, waiting for Ore and idle.
  - **IDLE is a chip only when an idle Shipyard could start a ship now:** the fleet has room for one of the player's saved designs, and the player has the Ore for it. An idle Shipyard at the cap, or without the Ore, is counted plainly.
  - **The Lab's IDLE is a chip only while a topic is open to it,** by its tier and prerequisites in the snapshot. With every open topic done, the line reads "Research: every open topic done", plainly.
  - **A click on the line at the cap selects the Command Station** and moves the camera to it, where its panel offers the upgrade (ADR-064). Otherwise the click opens production, as now.
    - A new `ActionKind::Select` names an entity, and `GameClient` selects it through `PlayerControls` and centers the camera on it.
    - UI4.5's per-ship bars use the same action.
- **ADR:** ADR-066 rewritten in place.
- **Acceptance:** `ShowsWhatProductionAndResearchAreDoing`, extended, covers:
  - at the cap with a level to buy, at the top level, and upgrading;
  - idle with room and Ore (a chip), idle at the cap and idle without Ore (no chip);
  - the click's target in each case;
  - the Lab with nothing open.
- **Verify:** CI; run; **owner run.**

### UI3.2 — One column, and state in bars

- **Gate:** V5.
- **Depends on:** UI3.1.
- **Goal:** the review's seventh finding. The status panel's edge moves with its text, and its state is a sentence.
- **Scope:** as V5 answers. The proposal:
  - **The Ore, status, territory and alerts panels take one fixed width,** so the column's edge stands still. It is the wider of the alerts' 380 units and the measured line of the longest topic's name. This rewrites ADR-066 decision 3's fitted width.
  - **Research reads as its topic's name over a progress bar,** with the queued count at the bar's end, "+3", in place of "Researching Reinforced Structures, 91% (+3 queued)".
  - **The Shipyards read as their counts,** a short label and a figure each, "BUILDING 0 · WAITING 2 · IDLE 3". The fleet reads as a bar against its cap, with "29 / 30".
  - **The column leaves its foot free** for what Phase 5 adds: §11's panel of what happened while away, and 38.3's orders window.
- **ADR:** ADR-066's.
- **Acceptance:** `HudTests`: the panels' widths equal and unchanged across every status content; bars at 0, 50 and 100%; 14.1's and 14.2's tests.
- **Verify:** CI; run; **owner run.**

### UI3.3 — Nodes and tickets that read

- **Gate:** V5.
- **Depends on:** UI2.1.
- **Goal:** the review's eighth finding. "3 / 10 : 2" needs decoding, and the drain the match is played for is left as arithmetic.
- **Scope:** as V5 answers. The proposal:
  - **The nodes read as the tickets do,** the player's against the enemy's, "3 : 2", with "cap 10 of 25" in the labels' color beside "Nodes".
  - **A third row says who is draining, and how fast:**
    - "Enemy −20 a minute · out in 32:06", in the text's color;
    - when the player is the one behind, the row is a warning chip;
    - level on nodes, it reads "No drain".
  - **The drain's two numbers join the snapshot's rules,** as `constructorBuildSeconds` did (ADR-068): `drainIntervalSeconds` and `drainTicketsPerNodeDifference` (ADR-057 decision 1). `PROTOCOL_VERSION` rises.
    - The time is the drains needed at the current nodes, rounded up, times the interval.
    - The next drain may be up to an interval away, which the row does not try to show.
  - **A world has no domination** (Phase 5 §8), so it has no ticket rows, as now.
- **ADR:** ADR-057 decisions 7 and 8 rewritten in place, and ADR-056 decision 11's panel line.
- **Acceptance:**
  - `HudTests`: ahead, level and behind; the times at shot 1's figures, 32:06, and at shot 5's, 12:09.
  - `WireFormatTests`: the fields.
  - `GameLogicTests`: the snapshot carries the tuning data's numbers.
- **Verify:** the container's `GameLogicTests`; CI; run; **owner run.**

---

## Milestone UI4 — Selection and commands

The milestone's looks share one ADR: selection and commands.

### UI4.1 — Bars on the screen

- **Gate:** V7.
- **Depends on:** UI2.2.
- **Goal:** the review's ninth finding. A bar on the ground turns with the camera and is foreshortened by its pitch.
- **Scope:** as V7 answers. The proposal:
  - **`GameClient` works out each bar's place on the screen,** from `Camera::PixelOf` of its entity's position, raised above the footprint's top on the screen by its projected radius.
  - **The HUD lays the bars out,** so `Hud::Lay` still holds no camera (ADR-015). `Hud::Content` gains the bars: each with its place in pixels, its health share, its build share and its side.
  - **A bar is 32 × 5 reference units** at `Hud::Scale`. Whether the player's factor (ADR-070) scales it too, as it scales the HUD and not the world's marks, is the ADR's.
  - **It fills left to right however the camera has turned:**
    - health over build;
    - the back in the side's color at 0.3 (ADR-028 decision 7);
    - the health fill green, amber or red as now, and the build fill UI2.2's gray.
  - **Bars are drawn first,** so every panel and window covers them. A remembered entity has none (UI1.2).
  - **When a bar shows does not change:** hurt, under construction, or any while Alt is held (ADR-047).
  - **`DrawHealthBars` and its strips on the ground go.**
  - **The cost** is four quads a bar, against the interface's 8,192 a frame (`UiPipeline::MAX_QUADS`): Alt over 300 entities is 1,200.
- **ADR:** the milestone's. It rewrites ADR-028 decision 7's place, 1.5 m off the footprint, and ADR-047 decision 4, whose least size becomes the size.
- **Acceptance:**
  - `GameAppTests`: a bar's place above its entity with the camera turned 0°, 90°, 180° and 270°, its fill growing to the right in each.
  - `HudTests`: bars under the panels; none for a remembered entity; the size at 1080p and 2160p.
- **Verify:** CI; run, turning the camera; **owner run.**

### UI4.2 — Commands beside the selection

- **Gate:** V6.
- **Goal:** the review's tenth finding. A warship's orders have no buttons, and the one button there is stands apart from the selection it acts on.
- **Scope:** as V6 answers. The proposal:
  - **The command panel stands against the selection panel's right edge,** bottom-aligned, the pair centered at the bottom, so that the two read as one. This rewrites ADR-043 decision 1's anchor for the buttons.
  - **A selection with a warship offers Attack-move, Hold sector, Patrol and Stop.** Each shows its key's cap from `KeyBindings.h` (task 15.2).
  - **A button does what its key does.**
    - Attack-move, Hold sector and Patrol arm the next left-click, and the armed one shows lit until it is given or canceled. Stop gives its order at once.
    - `PlayerControls` gains the calls the HUD makes, so the key and the button go one way, and four action kinds name them.
  - **The Constructors' build buttons and the retreat stay in the panel** (UI4.3).
  - **Phase 5's 38.3 names a ship's pending order on its panel.** The selection panel leaves room for that line.
- **ADR:** the milestone's. ADR-059 decision 7 gains the buttons.
- **Acceptance:**
  - `HudTests`: the four, with their caps, for warships; none for a selection of only Constructors; an armed one lit; the two panels touching at every width the selection panel takes.
  - `PlayerControlsTests`: each button arms or orders as its key does.
- **Verify:** CI; run; **owner run.**

### UI4.3 — Retreat as three choices

- **Gate:** V6.
- **Depends on:** UI4.2.
- **Scope:**
  - **The retreat button becomes a row of three,** "25%", "50%" and "Never", under the label RETREAT.
    - The selection's own setting is lit.
    - A mixed selection has none lit, and MIXED beside the label.
    - A press sets that setting; `ActionKind::SetRetreat` already carries it.
  - **The designer's retreat takes the same row** where 14.1's tests find room. Otherwise it keeps its stepper, and the task reports so.
- **ADR:** ADR-075 decision 10 rewritten in place.
- **Acceptance:** `HudTests`: each setting lit; mixed; a press on each cell; 14.1's tests.
- **Verify:** CI; run; **owner run.**

### UI4.4 — Controls that answer the pointer

- **Gate:** none.
- **Goal:** the review's eleventh finding.
- **Scope:**
  - **`Hud::Lay` takes the hovered action,** as `Hud::Describe` does.
  - **It lights what the pointer is over:** the hovered button's edge, and the row of a clickable status line, on the cards' color.
  - **Text the player cannot click is unchanged.**
  - **`Lay` without a hovered action lays out as now,** which the existing tests rely on.
- **ADR:** the milestone's.
- **Acceptance:** `HudTests`: a hovered button; a hovered status line; nothing else changed.
- **Verify:** CI; run.

### UI4.5 — One count, one bar per ship, a short name

- **Gate:** V6.
- **Depends on:** UI3.1, for `ActionKind::Select`.
- **Goal:** the review's twelfth finding.
- **Scope:** as V6 answers. The proposal:
  - **A selection of one design is titled once,** "7 × Lancer+SA". A selection of several designs reads "12 ships" over a line per design, as now.
  - **Up to 24 ships, a small bar per ship stands under the lines.**
    - Each is green, amber or red by its share, as the bar over the ship is.
    - A retreating ship's bar is marked by its outline.
    - A click on a bar selects that ship alone.
    - Past 24 ships, the selection's single bar stands as now.
  - **A new design's default name.** Where its hull, drive and weapon are a starting design's, it is that design's name with its module's code, "Lancer+SA" (`Designer::Name`, ADR-069). Otherwise it is the components' name, as now. A typed name wins, as now.
- **ADR:** rewrites ADR-046 decision 6 and ADR-069.
- **Acceptance:**
  - `HudTests`: the titles; seven bars, with one hurt and one retreating; a click on a bar; 25 ships falling back to the single bar.
  - The designer's tests: "Lancer+SA", and a typed name kept.
- **Verify:** CI; run; **owner run.**

---

## Milestone UI5 — The scene's weight

### UI5.1 — Rocks that stand back

- **Gate:** V8.
- **Goal:** the review's thirteenth finding. At the default zoom the rocks outweigh the fleet.
- **Scope:** as V8 answers. The proposal:
  - **Rocks get a face shade of their own,** about half the `FILL_SHADE` of 0.3 they share with the ships now.
  - **`ROCK_EDGE_BRIGHTNESS` stays,** and `FIELD_SHADE` stays relative to the new shade.
  - **The review's measure is repeated at the owner's run:** a field's lit pixels and brightness against a group of ships, at the default zoom.
- **ADR:** ADR-040 and ADR-028 decision 4 rewritten in place.
- **Acceptance:** none pure.
- **Verify:** CI; run, with the measure; **owner run.**

### UI5.2 — Fields of their own

- **Gate:** V8.
- **Goal:** the review's fourteenth finding. Every field is one rosette, and its gaps look passable.
- **Scope:** as V8 answers. The proposal:
  - **Each field's rocks are laid out from its identifier** with `EffectRandom` (ADR-026 decision 7), the same in every frame and on every machine:
    - five to eight ring rocks;
    - their distances, sizes and turns varied within the field's circle.
  - **Smaller rocks fill the gaps,** so that no gap along the ring is wider than a Small hull's footprint and the field reads as solid as the circle the server blocks.
  - **The rocks are instanced** as they are now (ADR-053). Nothing on the server changes.
- **ADR:** UI5.1's.
- **Acceptance:** `GameAppTests`: one field's layout the same twice; two fields' layouts different; every rock inside its circle; no gap wider than the bound.
- **Verify:** CI; run; **owner run.**

### UI5.3 — The minimap's view as its four corners

- **Gate:** none.
- **Goal:** the review's sixteenth finding.
- **Scope:**
  - **`Neuron::UiPipeline` gains a quad laid along a segment,** at any angle, drawn in the same call as the rest (ADR-015). It knows no game concept (R9).
  - **`Hud::Layout` gains lines.**
  - **The minimap draws the view as its four ground corners joined,** so that it shows the near and the far side and turns with the camera.
- **ADR:** ADR-015 rewritten for the segment, and ADR-043 decision 7's "the camera's view an outline".
- **Acceptance:** `NeuronClientTests`: a segment's corners at 0°, 45° and 90°. `HudTests`: the view's corners with the camera turned two ways.
- **Verify:** CI; run, turning the camera.

---

## Not in this plan

- **The sky's crosses.** The owner kept them on 2026-10-05 (task 16.6). This review saw two beside the fleet in shots 3–4, where a move marker would stand, and does not reopen the decision.
- **The bank.** The shots hold 15,387–25,586 Ore. Phase 5's H9 keeps Phase 4's economy, and the sink is Phase 6's (gate G3). UI3.1 shows what holds the Shipyards back; it changes nothing they cost.
- **Green for the selection, health, the drag box and Good.** It stays. Where it misled was the health bars inside the selection rings, and UI4.1 moves the bars above the units.
- **Selection rings chaining in a tight formation,** at 1.3 times the footprint (ADR-067). They are looked at again at UI4.1's run, once the bars are out of them.
- **Motion and flashes.** Stills cannot show them. The beam's 0.25 s, the blasts and the camera's feel belong to an owner run, and what it finds becomes a task.
- **A strategic view.** It is the horizon's gate G6.
