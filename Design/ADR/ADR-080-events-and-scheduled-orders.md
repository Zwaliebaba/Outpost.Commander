# ADR-080 — The server raises each player's events, which its alerts read

Status: **accepted** · 2026-10-08

## Context

Phase 5 design §7 (gates H4, H5) has scheduled orders fire on the server, on events, and says that the client's alerts read the same events, so that an order's trigger and the alert its player sees are one event. [ADR-059](ADR-059-alerts-and-standing-orders.md) decision 1 made the alerts the client's: `Alerts` compared each snapshot with the one before. The server, which keeps the orders, needs the events itself, and so does the report of what happened while a player was away (design §11).

## Decision

1. **The simulation raises each player's events in the tick they happen,** in the order the tick comes to them, and the player's snapshot of that tick carries them (`Snapshot::events`, `EventView`). They are not state: a save does not hold them, and a tick begins with none. They raised `PROTOCOL_VERSION` to 13.
2. **The events, and where in the tick each is raised:**
   - **Relay under attack:** once a tick for each Relay an enemy's shot hits, naming whose shot hit it first; while the fight is settled.
   - **Ship lost, structure lost:** for each of the player's ships and structures destroyed, as the fight removes them.
   - **Ship going back to be repaired:** when one of the player's ships starts to ([ADR-075](ADR-075-repair-and-retreat.md)); not again while it goes on.
   - **Structure built:** when a site of the player's is finished. **Ship built:** when one of its producers delivers a ship, a Constructor among them.
   - **Sector gained, sector lost, Relay suppressed, pirates cleared:** when the territory is worked out at the end of the tick ([ADR-056](ADR-056-territory.md)), from how each sector stood before it. A Relay is suppressed when its sector, held by the same player, was not. A sector the pirates guarded and no longer do tells every player ([ADR-073](ADR-073-pirates.md)).
   - **Enemy entered:** once the tick's vision is known, for each sector the player holds in which it sees an enemy or pirate warship and saw none at the end of the tick before, naming the first such warship and its owner. Each player's sectors with warships it saw are state (`PlayerState::enemySectors`), so that a world restored from a save raises the same events as one that never stopped, which raised `WORLD_STATE_VERSION` to 3 (AGENTS.md R18).
   - Each names its sector: the one it gives, or the one its position is in, or none.
3. **The client's alerts read the events** (ADR-059 decision 1): a Relay suppressed or under attack, a Mining Rig lost, enemy or pirate ships entering a held sector, a ship going back to be repaired, and pirates cleared. What the player built and lost besides a rig, and the sectors it gained and lost, raise no alert.

## Consequences

- **Tested in the Linux container.** `EventTests`, on the repository's map with fog of war: an enemy entering and a Relay suppressed, each once and only for its holder; a Relay hit once a tick, naming whose shot; a Constructor and a Relay built, the sector gained in the Relay's tick, and both lost again; a ship going back once, and a ship lost; and a camp cleared, told to both players once. `AlertsTests` read each alert from its event. `WireFormatTests` carry every field of an event.
- **An enemy is seen entering only where its player sees it.** A suppressed sector is not seen whole (ADR-056), so a warship that suppresses a Relay from beyond the Relay's own sight enters unseen, as it did for the client's alerts.

## What this forecloses

- Alerts that a client makes by comparing its snapshots, without a new decision.
- An event that only some clients see: each player's events are its own, the same for every client and hosted player that plays its seat.
