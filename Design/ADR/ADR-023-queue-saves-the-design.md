# ADR-023 — Queue on a design not yet saved saves it, and the queue follows once the server has it

Status: **accepted** · 2026-10-02 · Supersedes the "New picks" part of [ADR-017](ADR-017-research-and-the-designer.md) decision 7

## Context

ADR-017 decision 7 gave the designer two separate steps for new picks. The panel offered Save once every pick was unlocked, and Queue only when the picks matched a saved design. A new combination therefore showed a dim Queue button until the player pressed Save, and nothing on the panel said so.

In the owner's first match (design §3), the player researched all eight topics and never built the Large hull, the Fusion Drive or the Missile Rack. The owner reported on 2026-10-02 that the Large hull could be picked but Queue stayed disabled. Research and the server had unlocked it correctly. The panel stopped the player, and that weakens what the first match says about Q2 and Q3.

The server queues only saved designs (`QueueShipCommand` names a `DesignId`), and the server assigns that identifier when it applies the save. The client cannot queue a design it has not yet seen in a snapshot.

## Decision

1. **Queue is offered for new picks too.** When the picks match no saved design, the Queue button carries their cost. It is enabled when Save would be, the Shipyard is built with room in its queue, and the Ore covers the cost: the same Ore rule as for a saved design. Its action is `Hud::ActionKind::SaveAndQueue`.
2. **Pressing it sends the save, and the queue waits in the designer.** `Designer::SaveAndQueue` returns the `SaveDesignCommand` to send and records a waiting queue: the Shipyard, the picks and the tick of the newest snapshot. A second press before the design comes back adds another waiting queue but sends no second save, so each press queues one ship.
3. **The queue is sent when the design comes back.** Every frame, `Designer::TakeQueueCommands` looks for each waiting queue's picks among the newest snapshot's designs. When it finds them, it returns a `QueueShipCommand` for that design and that Shipyard, which `GameClient` gives to the controls. In process, that is the snapshot after the save.
4. **A save that does not come back is dropped.** If the design is not in a snapshot within `Designer::SAVE_WAIT_TICKS`, 40 ticks or two seconds, the server refused the save, and its waiting queues are forgotten. A new match starts with a new designer, so nothing waits across matches.
5. **Save design stays.** It saves without queuing, as before. The name saved is the one typed, or the components' names.

## Consequences

- A new design is one click from the first ship, as a saved one is.
- The protocol and the server do not change. The server still refuses to queue a design that is not saved, and still checks the save.
- The first ship of a new design is queued one snapshot after the click, 50 ms in process. Over a network it is one round trip.
- Queuing a new design always saves it, so the player's design list grows with every combination they build.
- `GameAppTests` checks it: `DesignerTests.QueuesADesignOnceItIsSaved` and the designer part of `HudTests.ShowsTheDesignerBesideAShipyard`.

## What this forecloses

- Queuing by components on the server. A ship is still built from a saved design, so its components stay fixed (ADR-017).
- Queuing a design that the player has not saved. The click saves it.
