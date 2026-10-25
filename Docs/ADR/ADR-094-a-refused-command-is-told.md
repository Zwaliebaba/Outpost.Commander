# ADR-094 — A refused command is told to the player who gave it, as an event, in words

Status: **accepted** · 2026-10-25
Owns: command-refusals, refusal-words

## Context

The server checks every command against the rules and refuses the ones that break them (`CommandResult`). It told nobody: `InProcessServer` dropped what `Simulation::Tick` returned, and the client could not tell a refusal from an order that had not arrived yet. A Shipyard ordered outside a held sector, a design saved twice and a scheduled order with a trigger in no sector all came to nothing, without a word. The owner decided that the server answers a refusal with its reason and the HUD shows it (owner, 2026-10-25).

Every connection already gets its player's events in the snapshot of their tick ([ADR-080](ADR-080-events-and-scheduled-orders.md) decision 1), and the client reads every snapshot in order for its alerts ([ADR-059](ADR-059-alerts-and-standing-orders.md)). A connection speaks for one player, and the server sets each command's player from the connection it came on (ADR-002).

## Decision

1. **A refusal is an event.** When `Simulation::Tick` refuses a command, it raises `EventKind::CommandRefused` for the command's player, with the reason in `EventView::refusal`, in the order the commands were given. So it reaches that player's snapshot of the tick, and only that player's: the client that sent the command, and no other. A command that is applied raises nothing. No second message is added to the protocol, and `PROTOCOL_VERSION` stays 1 (AGENTS.md R18).
2. **`CommandResult` is the protocol's.** It moves from `GameLogic/Simulation.h` to `GameProtocol/CommandResult.h`, unchanged, so that a client can read a refusal. It crosses the wire as its byte, and a value past its last is refused as any enumeration's is ([ADR-060](ADR-060-quic-in-process.md) decision 4).
3. **A refusal has no place.** It names no sector and no position. The server does not give it the sector its zero position lies in, as it does another event without one.
4. **The client says it in words, as an alert** ([ADR-059](ADR-059-alerts-and-standing-orders.md) decision 1). `DescribeRefusal` gives each reason the player's words: "Not enough Ore", "Not in a held sector", "At the node cap: upgrade the Command Station". A reason it has no words for cannot be added without them, since `AlertsTests` asks words of every reason.
   - The alert has no place, so it has no mark on the minimap, and Space moves the camera to the newest alert that has one.
   - A refusal told again takes the older one's place at once, as the newest alert, so that each refusal is seen as it happens. Two refusals of different words are both listed.
5. **A scheduled order refused when it is given is told the same way,** since it is a command ([ADR-080](ADR-080-events-and-scheduled-orders.md) decision 5). One that fires and is refused is still its order-fired event's to tell.
6. **What else hears of it.** A hosted AI and a deputy get their own refusals in their snapshots, and ignore them. The report of a time away leaves them out, and no scheduled order fires on one.

## Consequences

- **The layout of what a universe saves changes,** since a report of a time away keeps whole events (`AwayReport::ordersFired`), so `UNIVERSE_STATE_VERSION` 1's hash is replaced (AGENTS.md R18).
- **Tests:**
  - `WireFormatTests.CarriesEveryFieldOfASnapshot` carries a refusal with the last reason.
  - `InProcessServerTests.TellsARefusalToItsPlayerAlone`: two refusals reach the connection that sent them, in order, in the snapshot of their tick, and the other player's connection hears of none; a scheduled order with a trigger in no sector is one of them.
  - `AlertsTests.TellsARefusalInWords` and `SaysEveryRefusalInWords`: the words, no place, the key going past a refusal, a refusal told again in its older one's place, and words of their own for every reason.
  - `HudTests.ListsTheAlerts`: an alert with no place has no mark.

## What this forecloses

- A command whose refusal is told to anyone but the player who gave it.
- A refusal told with a place the server made up, or one marked on the minimap.
