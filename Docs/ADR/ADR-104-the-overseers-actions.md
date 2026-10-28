# ADR-104 — The overseer asks the server for actions on its channel, which the server's thread carries out between two ticks

Status: **accepted** · 2026-10-28
Owns: overseer-actions

## Context

The overseer watches a universe over a channel of its own ([ADR-101](ADR-101-the-overseers-channel.md)) and acts on the server but never on the simulation (owner, 2026-10-27). [The overseer's proposal](../Plans/UniverseOverseer.md) names its actions: Save now, Stop, Drop link and New token. The owner decided that each is written to the universe log, and that Stop asks to be confirmed in a window where Save now does not (owner, 2026-10-28). This ADR records how an action crosses, how the server carries out Save now and Stop, and how the overseer asks for them.

## Decision

1. **An action crosses as one message of the overseer's own, and its answer as another** (`OverseerProtocol.h`): `ActionMessage` names the action (`OverseerAction`: `SaveNow`, `Stop`, `DropLink` or `NewToken`), its seat for Drop link and New token, no player for the others, and for Drop link whether the seat's deputy takes it at once; `ActionResultMessage` names the action and its seat, whether the server did it or refused it, and the server's words for it. One message with a kind rather than one an action, since the four share every field but the deputy's choice. They are the overseer's messages, so its layout and hash change and an overseer of another build is refused (ADR-101 decision 4); the game's messages do not change.
   - **The link takes an action from the overseer that holds the channel,** and closes one that names what it may not, a seat for Save now or Stop, or no player's seat for Drop link or New token, with `MalformedMessage` (ADR-101 decision 3). MsQuic's thread keeps it with the address it came from (`OverseerLink::TakeActs`).
   - **The server's thread carries it out at its next tick,** after the simulation's tick and never inside it, as it saves (`InProcessServer::Act`), and answers the overseer that holds the channel then (`OverseerLink::SendResult`). No action reaches the simulation, its state or its command log.
2. **Save now saves the universe at that tick, as the save each minute is** ([ADR-077](ADR-077-universe-state-on-disk.md) decision 7): the server's thread encodes it, the folder's thread writes it, and the answer comes once the folder says it is written, "Saved at tick N, written in X ms.". One asked for while another asked for is still being written, or a second asked for at the same tick, is refused, naming the tick of the one being written. A save due at that tick is the same save.
3. **Stop stops the server as Ctrl+C does.** The server's thread marks it asked (`InProcessServer::StopAsked`) and answers it at once; `OutpostServer`'s main thread, which looks each `CHECK_INTERVAL`, 1 s, closes the server as it does on Ctrl+C, and `Closing` says its owner stopped it (ADR-101 decision 8). The universe comes back from its folder when the server is started again on its machine, from its last save with the commands logged since; nothing on the overseer starts it.
4. **The universe log writes each action** as an `overseer` record, at the tick the server carried it out, with the address it came from, whatever came of it, as it writes the overseer's comings and goings ([ADR-082](ADR-082-universe-log-and-universe-run.md) decision 1): `save`, `stop`, `drop player <p> deputy <now or later>` and `token player <p>`.
5. **The overseer asks for an action only of a running server.** The header's Save now and Stop stand beside its Server button, dimmed and taking no press unless the header says Running, since a Stalled server's thread may never carry one out and none is asked of a server it has no word from ([ADR-103](ADR-103-the-overseer-application.md) decision 7). Save now asks at once. Stop opens the confirm window (`OverseerWindow::Confirm`), in the middle of the screen, which says what stopping does: its Stop asks for it and closes the window, dimmed as the header's is, and Keep running or its close box closes it asking nothing.
   - **What came of the latest action is told under the header's state** while the server is Running or Stalled, with how long ago, wrapped, since the server's words may run long, and a refusal in the warning's color as a refused text is ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1). The model keeps it until a newer connection's welcome (`OverseerModel::LatestResult`).

## Consequences

- **An action waits for the server's thread.** A Stalled server carries out none until its tick runs again, and a stuck one is stopped from Windows, as it was before the overseer had actions.
- **An answer goes to the overseer that holds the channel when the server gives it,** which is a newer one than asked when the channel was taken meanwhile.
- **A save asked for is a save like any other:** the folder keeps the newest of them ([ADR-077](ADR-077-universe-state-on-disk.md)), so an owner who asks often drops the older saves sooner.
- **Tests.**
  - `OverseerProtocolTests`: an action and its answer cross as they were sent. `UniverseLogTests.WritesTheOverseersActions`: each action's record.
  - `OverseerLinkTests`, over a real connection: Save now answered once written, with its record, and `Closing`'s last save that save; a second save refused while the first is written, from a server stepped by hand; Stop asked, answered and written; and an action that names what it may not closed as malformed. QUIC runs in CI's Windows run only.
  - `OverseerModelTests.KeepsTheLatestActionsResult`. `OverseerScreenTests`: Save now and Stop from the header, Stop's window and its two answers, the result under the header, and the actions asked of no server that does not run.
