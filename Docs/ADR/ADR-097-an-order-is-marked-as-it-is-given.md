# ADR-097 — An order is marked on the ground as it is given

Status: **accepted** · 2026-10-26
Owns: order-marks

## Context

An order reaches the server at its next tick, and the player sees it obeyed only once a snapshot shows the ships move: a tick and the view's delay on the loopback ([ADR-013](ADR-013-client-view-and-controls.md) decision 3), and a round trip more on a universe's link. Until then nothing on the screen says the click was taken, and a player who sees nothing clicks again. The client shows only what the snapshots say and predicts nothing ([ADR-002](ADR-002-authoritative-server.md) decision 5), so what it can show at once is the order itself, not its effect.

A refused order is told to its player as an event that names the order's number ([ADR-094](ADR-094-a-refused-command-is-told.md) decision 1), so the client knows which order a refusal is of.

## Decision

1. **A move, an attack-move, a hold of a sector and a patrol mark their point, and an attack its target, as the order is given** (owner, 2026-10-26). `PlayerControls` keeps the mark of each order it numbers that has a point or a target (`OrderMark`), whether the order came from a click, a key or the HUD's minimap, and `GameClient` draws it. A stop, a build, a repair, a salvage and every order a window gives mark nothing.
2. **A mark is a ring as wide as the selection's, in its color, that shrinks to nothing over `PlayerControls::MARK_SECONDS`, 0.4 s** (owner, 2026-10-26). Around a target it starts as the target's selection ring would be, 1.3 times its footprint, and around a point at `MARK_POINT_UNITS`, 16 of the HUD's reference units in radius at any zoom ([ADR-067](ADR-067-selection-ring-at-a-fixed-width.md) decision 1). `GameClient` ages the marks by each frame's wall time (`AgeMarks`) and draws them with the selection's rings.
3. **A refusal of the order turns its mark the warning's color** while it still shows (`PlayerControls::Refuse`), `REFUSED_MARK_COLOR`, the HUD's `WARNING_COLOR` ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1). A refusal that comes after the mark has gone is told by its alert alone ([ADR-094](ADR-094-a-refused-command-is-told.md)).
4. **A mark predicts nothing.** It says where the order goes and nothing of where the ships are; they move when a snapshot says they do ([ADR-002](ADR-002-authoritative-server.md) decision 5).

## Consequences

- **A click is answered on the frame it is read,** whatever the link, and the ships' own answer still waits for the server.
- **A refusal over a link whose round trip passes 0.4 s turns no mark,** since the mark has gone by then; the alert still tells it.
- **Tests.** `PlayerControlsTests.MarksEachOrderAsItIsGiven` gives a move, an attack, an attack-move, a hold, a patrol and a stop, and checks that each but the stop marks its point or target under its order's number, that a refusal of the attack's number turns its mark and no other, and that the marks are there short of 0.4 s and gone after it. `GameClient`'s drawing needs D3D12 and Windows, so it has no test; CI builds it.

## What this forecloses

- **Moving a ship on the screen before the server does.** That is prediction, which ADR-002 decision 5 rules out and which a decision of its own would have to bring in.
