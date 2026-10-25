# ADR-023 — Queue on a design not yet saved saves it, and the queue follows once the server has it

Status: **accepted** · 2026-10-02
Owns: save-and-queue

## Context

The server queues only saved designs (`QueueShipCommand` names a `DesignId`), and the server assigns that identifier when it applies the save. The client cannot queue a design it has not yet seen in a snapshot. So a new combination of picks cannot be queued until it is saved, and without help from the designer the player would have to press a Save that nothing on the panel asks for before Queue could work.

## Decision

1. **Queue is offered for new picks too.** When the picks match no saved design, the Queue button carries their cost. It is enabled when Save would be, the Shipyard is built with room in its queue, and the Ore covers the cost: the same Ore rule as for a saved design. Its action is `Hud::ActionKind::SaveAndQueue`. When the hull needs a higher level than the target Shipyard has, Queue is dim and says "Needs Shipyard L<n>", as it is for a saved design ([ADR-064](ADR-064-structure-upgrades.md) decision 9).
2. **Pressing it sends the save, and the queue waits in the designer.** `Designer::SaveAndQueue` returns the `SaveDesignCommand` to send and records a waiting queue for each ship the designer's count asks for ([ADR-017](ADR-017-research-and-the-designer.md) decision 7): the Shipyard, the picks and the tick of the newest snapshot. A second press before the design comes back adds its own waiting queues but sends no second save.
3. **The queue is sent when the design comes back.** Every frame, `Designer::TakeQueueCommands` looks for each waiting queue's picks among the newest snapshot's designs. When it finds them, it returns a `QueueShipCommand` for that design and that Shipyard, which `GameClient` gives to the controls. In process, that is the snapshot after the save.
4. **A save that does not come back is dropped.** If the design is not in a snapshot within 40 ticks (`Designer::SAVE_WAIT_TICKS`), the server refused the save, and its waiting queues are forgotten. A new match starts with a new designer, so nothing waits across matches.
5. **Save design saves without queuing.** The name saved is the one typed, or the one the designer gives new picks ([ADR-069](ADR-069-starting-design-names.md) decision 5).

## Consequences

- A new design is one click from the first ship, as a saved one is.
- The server refuses to queue a design that is not saved, and checks the save.
- The first ship of a new design is queued one snapshot after the click, one tick in process: 50 ms at 20 ticks a second, by count. Over a network it is one round trip.
- Queuing a new design always saves it, so the player's design list grows with every combination they build.
- `GameAppTests` checks it: `DesignerTests.QueuesADesignOnceItIsSaved` and `HudTests.QueuesNShipsAtTheTarget`.

## What this forecloses

- Queuing by components on the server. A ship is still built from a saved design, so its components stay fixed (ADR-017).
- Queuing a design that the player has not saved. The click saves it.
