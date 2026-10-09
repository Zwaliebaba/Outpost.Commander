# ADR-066 — The HUD says what production and research are doing

Status: **accepted** · 2026-10-05

## Context

The review of 2026-10-03 found that nothing outside the windows says a Shipyard or the Research Lab is idle. The last three of its screenshots banked 4,292–5,136 Ore while the queues they showed, research's and Shipyard 05's, stood at 0 / 5. Only a structure's window showed its queue, one producer at a time, and a line under the Ore named the topic under way only while there was one.

Phase 1 design §12 kept a selected structure's panel to its name, hit points, construction and window buttons, and moved the queues into the windows (owner, 2026-10-03). Task 15.1 of the interface plan (`GameDesign/ImplementationPlan-Interface.md`) proposed a panel under the Ore and a line on the selection panel. The owner decided gate K2 on 2026-10-03 as proposed. This ADR changes what Phase 1 design §12 says the selection panel shows.

The second interface review of 2026-10-08 (`GameDesign/ImplementationPlan-Interface2.md`, finding 5) found the Shipyards' line a warning when nothing could be done, the same when something could, and leading nowhere near the remedy: at the fleet cap, IDLE stood in the warning's color while only the Command Station's next level could help. The same review found the column under the Ore ragged and its state written as sentences (finding 7): the status panel's edge moved with its text, and research read "Researching Reinforced Structures, 91% (+3 queued)". The owner decided gate V5 as proposed on 2026-10-08. Task UI3.1 rewrote decisions 1 and 2, and task UI3.2 decisions 1 and 3.

## Decision

1. **A status panel stands under the Ore, in place of the research line.** It has a line for each of these, and each line is a place to click:
   - **The Research Lab**, once the player has a finished one. Researching, the topic's name over a bar of how far it has come, with how many more are queued at the bar's end, "+2", and " · waiting for Ore" after the name while the front topic waits. Idle, "Research Lab IDLE" while a topic is open to it, or "Research: every open topic done" when none is. A topic is open while it is not researched, its tier is one the Lab has opened, and every prerequisite is researched; its Ore is not asked. A click opens the research window.
   - **The Shipyards**, once the first is finished, first whether the fleet cap holds them back: a Shipyard's front warship waits for the cap, or the fleet has room for none of the player's saved designs. Then the line reads "Shipyards at the fleet cap · Station L4: +10", with what the Command Station's next level adds to the cap; "Shipyards at the fleet cap · Station upgrading, 62%" while that level is being built; and "Shipyards at the fleet cap" at the station's top level. A click selects the Command Station and moves the camera to it (`ActionKind::Select`, `PlayerControls::SelectAlone`), where its panel offers the upgrade ([ADR-064](ADR-064-structure-upgrades.md)). The atlas has no arrow glyph, so the level reads "L4", not "L3 → L4".
   - **Otherwise the Shipyards' counts**, a short label and a figure each, every one in its place, "Shipyards · BUILDING 3 · WAITING 1 · IDLE 1". A click opens the production window at the first idle Shipyard by number, or at the first Shipyard when none is idle.
   - **The fleet**, under the Shipyards' line while the match sets a cap: "Fleet" beside a bar of the command points its warships take against the cap, with "29 / 30" at the bar's end. Its click is the Shipyards' line's.
   - A Shipyard waits for Ore when its queue holds a job and the front job has made no progress, as the production window counts it.
2. **IDLE is a warning chip only when something could be done now** ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1): the Lab's while a topic is open to it, and the Shipyards' when an idle Shipyard could start a ship, that is, a saved design of a hull it builds fits under the cap and the player has its Ore. Otherwise idle Shipyards are counted plainly, "IDLE 2", and the line at the cap is plain text, since its remedy is a level and not a queue. The rest of a line is in the text's color.
3. **The column under the Ore is one width** (task UI3.2): the Ore, status, territory and alerts panels are as wide as the widest of the alerts' 380 units, the longest research topic's name with room for "+99" at its end, and the status panel's widest lines, measured on fixed text: "Shipyards · BUILDING 99 · WAITING 99 · IDLE 99" with a chip's tag, "Shipyards at the fleet cap · Station upgrading, 100%" and "Research: every open topic done". What the panel holds never moves the column's edge, and a line that will not fit is cut short. The panel's place is kept for its three lines and research's bar, whether it has them or not, so that the territory and the alerts under it do not move as a Lab or a Shipyard is finished. The production and research windows open under that place, at 178 units from the top where they opened at 148.
   - A bar under its line's words is 4 units thick in a row of 8, across the panel; a bar beside them, 6 units thick, runs from 8 units after them to 8 units short of its figure. The track is `BAR_TRACK_COLOR` and the fill, for its share, `BAR_FILL_COLOR`, as the windows' bars are.
   - The column's foot is left free: Phase 5's orders and away panels are floating windows ([ADR-080](ADR-080-events-and-scheduled-orders.md)), not panels of the column.
4. **The selection panel of the player's own finished Shipyard, Command Station or Research Lab gains a line**, after its hit points and before its next level:
   - "Building Swarm · 62% · +2 queued", or "Building Swarm · waiting for Ore", with the front job's name and how many more are queued.
   - "Researching Hull Plating · 62%" for the Lab.
   - "Idle" when its queue is empty.
   - An enemy structure's queue stays unshown (task 9.4).
5. **Alerts are not this.** An alert ([ADR-059](ADR-059-alerts-and-standing-orders.md)) is a message about an event that fades. The status panel shows a standing state, and stays.

## Consequences

- **Phase 1 design §12 changes.** A selected structure's panel keeps its name, hit points, construction and window buttons, and now also says what the structure is doing.
- **The Command Station has no line on the status panel.** It builds Constructors, which a player queues on purpose, and an idle station is not the friction the review found. Its selection panel says what it is doing.
- **A Lab or Shipyard under construction has no line.** It cannot be idle, since it cannot yet take work.
- **The click is the line's row across the panel**, the first and the last row reaching to the panel's edge, so a click on the panel's padding still opens a window.
- **The tests.**
  - `HudTests.ShowsWhatProductionAndResearchAreDoing` checks the panel with no Lab and no Shipyard; a Lab researching, waiting, idle with a topic open and with none open; Shipyards building, waiting and idle, with a chip when an idle one could start a ship, and plainly without the Ore or with Ore only for a hull no idle Shipyard builds; and a click on each line.
  - `ShowsTheFleetAgainstItsCap` checks the line at the cap with a level to buy, upgrading and at the top level, idle with no room, and the counts again with room, each with its click's target.
  - `KeepsTheColumnOneWidth` checks the Ore, status, territory and alerts panels at one width, no narrower than 380 units, across four status panels, and research's and the fleet's bars at nothing, half and the whole, each in its place.
  - `PlayerControlsTests.SelectsAStructureFromTheHud` checks that the HUD's selection takes the place of the old and drops an armed placement.
  - `SaysWhatASelectedProducerIsDoing` checks the selection panel's line for a Shipyard building, waiting and idle, the Command Station and the Lab, and none on an enemy's.

## What this forecloses

- A line per Shipyard on the HUD: the panel counts them, and the production window shows each.
