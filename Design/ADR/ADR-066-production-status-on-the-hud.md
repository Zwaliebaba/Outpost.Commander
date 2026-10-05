# ADR-066 — The HUD says what production and research are doing

Status: **accepted** · 2026-10-05

## Context

The review of 2026-10-03 found that nothing outside the windows says a Shipyard or the Research Lab is idle. The last three of its screenshots banked 4,292–5,136 Ore while the queues they showed, research's and Shipyard 05's, stood at 0 / 5. Only a structure's window showed its queue, one producer at a time, and a line under the Ore named the topic under way only while there was one.

Phase 1 design §12 kept a selected structure's panel to its name, hit points, construction and window buttons, and moved the queues into the windows (owner, 2026-10-03). Task 15.1 of the interface plan (`GameDesign/ImplementationPlan-Interface.md`) proposed a panel under the Ore and a line on the selection panel. The owner decided gate K2 on 2026-10-03 as proposed. This ADR changes what Phase 1 design §12 says the selection panel shows.

## Decision

1. **A status panel stands under the Ore, in place of the research line.** It has a line for each of these, and each line is a place to click:
   - **The Research Lab**, once the player has a finished one: "Researching Hull Plating, 62%", with "(+2 queued)" after it when more are queued; "Researching Hull Plating, waiting for Ore"; or "Research Lab IDLE". A click opens the research window.
   - **The Shipyards**, once the first is finished: how many are building, how many wait for Ore and how many stand idle, "Shipyards: 3 building, 1 waiting for Ore, 1 IDLE". A count of nothing is left out, apart from building's. A click opens the production window at the first idle Shipyard by number, or at the first Shipyard when none is idle.
   - A Shipyard waits for Ore when its queue holds a job and the front job has made no progress, as the production window counts it.
2. **An idle line says IDLE in words, in the warning's color**, so that it reads without its color. A line that reports nothing idle is in the text's color.
3. **The panel is as wide as its longest line**, from 260 units to 560, and a longer line is cut short. Its place is kept for two lines whether it has them or not, so that the territory and the alerts under it do not move as a Lab or a Shipyard is finished. The production and research windows open under that place, at 148 units from the top where they opened at 128.
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
- **The test.** `HudTests.ShowsWhatProductionAndResearchAreDoing` checks the panel with no Lab and no Shipyard, a Lab researching, waiting and idle, Shipyards building, waiting and idle, and a click on each line. `SaysWhatASelectedProducerIsDoing` checks the selection panel's line for a Shipyard building, waiting and idle, the Command Station and the Lab, and none on an enemy's.

## What this forecloses

- A line per Shipyard on the HUD: the panel counts them, and the production window shows each.
