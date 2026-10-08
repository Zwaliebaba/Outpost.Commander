# ADR-080 — The server raises each player's events; its alerts read them and its scheduled orders fire on them

Status: **accepted** · 2026-10-08

## Context

Phase 5 design §7 (gates H4, H5) has scheduled orders fire on the server, on events, and says that the client's alerts read the same events, so that an order's trigger and the alert its player sees are one event. [ADR-059](ADR-059-alerts-and-standing-orders.md) decision 1 made the alerts the client's: `Alerts` compared each snapshot with the one before. The server, which keeps the orders, needs the events itself, and so does the report of what happened while a player was away (design §11).

The design leaves three things open, which the owner decided on 2026-10-08: whose clock a time of day is, which sector an event trigger watches, and whether the report of a time away survives a restart.

## Decision

1. **The simulation raises each player's events in the tick they happen,** in the order the tick comes to them, and the player's snapshot of that tick carries them (`Snapshot::events`, `EventView`). They are not state: a save does not hold them, and a tick begins with none.
2. **The events, and where in the tick each is raised:**
   - **Relay under attack:** once a tick for each Relay an enemy's shot hits, naming whose shot hit it first; while the fight is settled.
   - **Ship lost, structure lost:** for each of the player's ships and structures destroyed, as the fight removes them.
   - **Ship going back to be repaired:** when one of the player's ships starts to ([ADR-075](ADR-075-repair-and-retreat.md)); not again while it goes on, even when it turns for another repairer.
   - **Structure built:** when a site of the player's is finished. **Ship built:** when one of its producers delivers a ship, a Constructor among them.
   - **Sector gained, sector lost, Relay suppressed, pirates cleared:** when the territory is worked out at the end of the tick ([ADR-056](ADR-056-territory.md)), from how each sector stood before it. A Relay is suppressed when its sector, held by the same player, was not. A sector the pirates guarded and no longer do tells every player ([ADR-073](ADR-073-pirates.md)).
   - **Enemy entered:** once the tick's vision is known, for each sector the player holds in which it sees an enemy or pirate warship and saw none at the end of the tick before, naming the first such warship and its owner. Each player's sectors with warships it saw are state (`PlayerState::enemySectors`), so that a world restored from a save raises the same events as one that never stopped.
   - **Order fired:** when a scheduled order fires (decision 5).
   - Each names its sector: the one it gives, or the one its position is in, or none.
3. **The client's alerts read the events** (ADR-059 decision 1): a Relay suppressed or under attack, a Mining Rig lost, enemy or pirate ships entering a held sector, a ship going back to be repaired, pirates cleared, and an order fired, held back or refused, which is told every time. What the player built and lost besides a rig, and the sectors it gained and lost, raise no alert.
4. **A scheduled order** (`ScheduleOrderCommand`) is one trigger, one action and at most one condition, given to a selection (design §7):
   - **Triggers:** a time of day; or, in one sector the player picks (owner, 2026-10-08), enemy ships entering it, a Relay of the player's suppressed or under attack there, or a Mining Rig of the player's lost there. An event trigger fires on an event raised after the order was given: enemy ships already in the sector do not fire it until they come again.
   - **Actions:** move, attack-move, attack, hold a sector and patrol, which its warships are given as the player gives those orders; and a Mining Rig built on the asteroid at its point, which its Constructors are given. Its target is fixed when it is given (owner, 2026-10-08).
   - **The condition:** the most command points the enemy and pirate warships the player sees in the action's sector may take; more, and the ships hold the sector they are in, the middle of them, instead.
   - It is refused as `NoShips` without a warship, or without a Constructor for a rig (`NotAConstructor` for another ship); `NoSector` for a trigger's sector that is not the map's, or a hold outside every sector; `InvalidPosition`, `UnknownTarget` and `NotAnEnemy` as the orders of its actions are; `InvalidCondition` for fewer than no points; and `NotOwned` for another player's ship.
5. **The server keeps it as it keeps a standing order** (ADR-059 decision 3), as state, with the ships that wait on it, and fires it once:
   - At the end of each tick, once the tick's events are raised, each order whose trigger fired gives its ships its action through the path a player's order takes, and is dropped. Its event says what it did: as given, held back by its condition, or refused, as when an attack's target is gone or a rig's sector is not held.
   - A ship waits on one order at a time. Any order the player gives it takes it out of the one it waits on, as an order ends a standing order, and so does a new scheduled order. Setting its retreat does not. An order none waits on any more is dropped, and so is one whose ships are all gone.
   - Only its owner sees it: `Snapshot::scheduled`, and on each ship the order it waits on (`EntityView::scheduledOrder`).
   - A deputy leaves alone a ship that waits on one ([ADR-079](ADR-079-seat-controller-and-deputy.md)), since an order of its own would take the ship out of it.
6. **A time of day is the player's clock** (owner, 2026-10-08). The client turns the hour and minute the player picks into the next such moment on the player's clock and sends it as seconds since 1970 UTC (`ScheduledTrigger::utcSeconds`). The host makes it a tick as the command arrives (`TickOfMoment`): the first tick at or after the moment at the server's rate, by the host's wall clock, which is where wall time becomes ticks ([ADR-009](ADR-009-deterministic-core.md) decision 3). A moment past is the next tick, and one more than a week ahead a week ahead. The tick a client writes is never trusted, and the log keeps the host's, so a world replays. A pause while the server is down pushes the order late: it fires at its tick.
7. **The report of a time away** (`AwayReport`, design §11) is the host's (`AwayReports`). While a seat's deputy plays it, each tick's events go to the seat's report, from the tick the deputy first played: ships and structures built and lost, the sectors gained and lost as the player would come back to them, a sector gained and lost again being in neither, and the last 16 orders fired. The player's first snapshot after it takes the seat again carries it (`Snapshot::away`), once. **A save keeps the reports beside the state** (owner, 2026-10-08), so a restart while the player is away loses nothing of it ([ADR-077](ADR-077-world-state-on-disk.md)).
8. **The protocol and the save.** `PROTOCOL_VERSION` is 13. The players' sectors with warships seen, the scheduled orders and the seats' reports changed what a save holds, and `WORLD_STATE_VERSION` is 4 (AGENTS.md R18).

## Consequences

- **Tested in the Linux container.**
  - `EventTests`, on the repository's map with fog of war: an enemy entering and a Relay suppressed, each once and only for its holder; a Relay hit once a tick however many shots hit it, naming whose shot; a Constructor and a Relay built, the sector gained in the Relay's tick, and both lost again; a ship going back once, and not again when its Repair Bay falls; a ship lost; and a camp cleared, told to both players once.
  - `ScheduledOrderTests` (W5): a time of day fires on its tick and not before; enemy ships fire it in its sector only; a Relay hit and a rig lost fire theirs; each action gives its order; the condition holds back from three Small warships at two points and not at three; a rig refused when it fires is told so; another order, or a new scheduled order, takes a ship out, and a retreat set does not; the refusals; and a saved world fires its order on the same tick as the world that never stopped.
  - `InProcessServerTests`: a moment's tick, rounded up and held to a week, and the host's tick in place of the client's, in the log. `AwayReportsTests` and `AwayReportTests`: a report kept while the deputy plays and handed over once, and what it counts. `WorldStateTests`: a save keeps the seats' reports. `DeputyTests`: a ship waiting on an order is no defender. `AlertsTests`: each alert from its event. `WireFormatTests`: every field of the command, the events, the orders and the report.
  - Each was run against a deliberately broken build and failed: an event raised again, or not at all; an order a tick late, in another sector, at the condition's limit, or kept by a ship given another order; a moment not rounded up; the client's tick trusted; a report handed over twice; and a deputy that sends a waiting ship.
- **Passed in CI's Debug|x64 run on Windows** once it runs: `QuicTransportTests.ADeputyPlaysItsSeatWhileItsPlayerIsAway` checks that the player's first snapshot after it takes the seat again, over a real connection, carries the report from the tick the deputy took it.
- **An enemy is seen entering only where its player sees it.** A suppressed sector is not seen whole (ADR-056), so a warship that suppresses a Relay from beyond the Relay's own sight enters unseen; the Relay's suppression is its event then.
- **A time of day is a moment, not a recurring hour.** An order for 02:00 fires once, at the next 02:00; another night needs another order.
- **The report is of what the seat's events tell:** what the deputy built and lost and the orders that fired, not what the enemy did beyond it.

## What this forecloses

- Alerts that a client makes by comparing its snapshots, without a new decision.
- A scheduled order that fires more than once, nests another, or names more than one trigger, action or condition (design §7, gate H5).
- A tick for a time of day that a client chooses.
