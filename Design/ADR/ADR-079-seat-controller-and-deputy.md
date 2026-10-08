# ADR-079 — A seat is its player's or its deputy's, and the server plays its deputies and AI empires on its own thread

Status: **accepted** · 2026-10-08

## Context

Phase 5 design §6 (gates H2 and H3) has a world keep running while its players are away. A seat's deputy keeps its player's empire running from 60 s after the player's connection has gone, and an AI empire plays a seat of its own, to win. Both run on the server's thread, and a world with them still replays from its seed and its log. [ADR-025](ADR-025-server-thread.md) kept the AI off the server's thread until clients and server were separate processes, which they now are ([ADR-078](ADR-078-dedicated-server.md)).

The design leaves two things open, which the owner decided on 2026-10-08: how many Constructors a deputy keeps, and what its defenders do once an attack is over.

## Decision

1. **A player the server plays itself is a `HostedPlayer`**, declared in `GameProtocol`. After a tick in which the seat is its, it is handed its player's snapshot, and `Play` returns the orders to apply. After a tick in which its player plays the seat, `Watch` gives it the snapshot to learn from, and it gives no order. `Server::Host` hands one to the server before the server starts. `GameLogic` never includes `Opponent`: `OutpostServer` makes the hosted players and hands them over.
2. **Hosted players play after each tick, on the server's thread.** Once every player has been sent its snapshot, each hosted player is handed its own. Its orders wait in its connection's queue, and the next tick applies and logs them as it does a client's. So a world replays from its seed and its log without them ([ADR-009](ADR-009-deterministic-core.md)), and nothing of them is saved. Their time is the tick's `Hosted` part, `hosted` in the measurement log.
3. **A hosted player without a seat is the player: an AI empire.** `AiEmpire`, in `Opponent`, plays its seat with `AiPlayer`, which starts from any state of its player ([ADR-020](ADR-020-ai-and-match-flow.md) decision 16). It has a loopback connection of its own, and its snapshots are handed to it rather than queued.
4. **A hosted player with a seat is the seat's deputy, and `SeatController` says who plays the seat:**
   - the deputy, until the player first takes the seat;
   - the player, while its newest connection is open, and for `DEPUTY_DELAY_SECONDS` (60 s) of ticks after the server sees that connection gone (`QuicChannel::HasGone`);
   - the deputy after that, until the player takes the seat again, which hands it back at once.

   A connection that closes is seen gone at once. One whose client vanished is seen gone when MsQuic's idle timeout ends it, 30 s by MsQuic's default.
5. **A snapshot shows its owner each ship's order** (`EntityView::order`; `ShipOrder` moved to `GameProtocol`), which is how a deputy tells an idle ship from a busy one. `PROTOCOL_VERSION` is 12. A save keeps the enemy entities each player remembers as `EntityView`s, so the state's layout changed with it: `WORLD_STATE_VERSION` is 2 (AGENTS.md R18), and a world saved at version 1 is refused ([ADR-077](ADR-077-world-state-on-disk.md)).
6. **The deputy is a keeper** (`Deputy`, in `Opponent`; design §6, gate H3). It decides once a second while it plays, and uses the Normal AI's settings for its research order and how long a defence holds.
   - **Research.** A Lab with a slot free takes the next topic of the AI's research order, then any topic it can take.
   - **Production.** A finished Shipyard whose queue is empty is given the design it last queued, which the deputy learns from the queue whether it plays or watches. A Shipyard it never saw build, as after the server restarted, gets the design most of its player's warships are of. It keeps one job queued, and the job waits for the Ore and the fleet cap as any job does. When the cap holds a Shipyard's next ship back, the deputy upgrades the Command Station once the Ore is there, as the AI does.
   - **Constructors.** It keeps as many as its player had when it took the seat (owner, 2026-10-08), counting those the Command Station has queued.
   - **Rigs.** It rebuilds a Mining Rig it saw stand, on an asteroid that still holds ore, in a sector its player holds and the pirates do not guard, with the two nearest idle Constructors.
   - **Repairs.** Each damaged structure, and each site with nobody on it, gets the nearest idle Constructor.
   - **Defence.** What draws its idle warships is enemy warships, the pirates' among them, in a sector its player holds, taking the sector with the most of them; or whatever fires on one of its player's structures in such a sector. Idle means no order, no standing order and not retreating. They attack-move there. The defence ends `defenseHoldSeconds` (10 s) after the attack was last seen, or at once when the sector is lost, and each defender then moves back to where it stood (owner, 2026-10-08).
   - **What it never does.** It never attacks, raids, claims a sector, clears pirates or salvages. It leaves alone a ship that retreats, or that has an order or a standing order.
   - **Each turn starts afresh.** Its Constructor count, repairs and defenders belong to the turn. What it learned while watching, each Shipyard's last design and where the rigs stood, it keeps.
7. **`OutpostServer` hosts them.**
   - A seat in `World.json` is a player's, with a token, or an AI empire's, with its difficulty: Easy, Normal or Hard (ADR-065). `--new-world --ai <player>[:<difficulty>]` makes one.
   - An AI seat is hosted as an `AiEmpire` with its difficulty's settings, and gets no seat to take and no join file.
   - A player's seat is opened as before, and hosted with a `Deputy`.

## Consequences

- **Tested in the Linux container.**
  - `SeatControllerTests`: the hand-over and the hand-back, tick by tick.
  - `DeputyTests`, in `OpponentTests` ([ADR-075](ADR-075-test-dll-per-library.md)), on snapshots built by hand: each of decision 6's keeper rules, and a turn beginning afresh.
  - `InProcessServerTests.HostsAiEmpiresWhoseOrdersReplayFromTheLog`: a world of two AI empires on the server's thread, three minutes of it replayed from the command log to an equal simulation; and `HostsAPlayerOnceAndBeforeItStarts`.
  - `WorldSettingsTests`: AI seats, and `--new-world --ai` run.
- **Passed in CI's Debug|x64 run on Windows.** `QuicTransportTests.ADeputyPlaysItsSeatWhileItsPlayerIsAway` checks the hand-over over a real connection: the deputy plays an untaken seat, watches while its player plays, takes the seat exactly 60 s of ticks after the connection went, and hands it back when the player returns. QUIC does not run in the container (ADR-078).
- **W3, measured in the container** (`DeputyPlayTests.KeepsAnEmpireRunningAsTheAiKeepsItsOwn`, in `GameLogicTests`). Two Normal AIs played for 10 minutes, then blue's seat was its deputy's for 10 more. Over those 10 minutes, the deputy's Shipyards stood idle with Ore to spend 0.8–1.4% of their time, against 0.0–5.4% for the AI's, and its Lab 0.2%, against 0.0–0.2%, over seeds 1 to 4. "Idle with Ore to spend" means a finished Shipyard with an empty queue while its player could pay for a ship that the cap had room for, or a Lab with nothing to research while a topic it could take was paid for. The deputy's first version queued a ship only once the Ore and the cap allowed it, and its Shipyards stood idle 50.6% of the time, which is why it now keeps a job queued.
- **W2, measured in the container** (`InProcessServerTests.MeasuresTheHostedPlayersTicks`, with `OUTPOST_WORLD_MEASURE`): an hour of world with two Normal AI empires on the server's thread, clang 18 at `-O2`, seeds 1 to 3.

  | Seed | Tick, 99th percentile | Tick, longest | Hosted part, 99th percentile | Hosted part, longest |
  |---|---|---|---|---|
  | 1 | 0.35 ms | 7.08 ms | 0.09 ms | 0.61 ms |
  | 2 | 0.40 ms | 8.64 ms | 0.09 ms | 0.36 ms |
  | 3 | 0.25 ms | 6.08 ms | 0.08 ms | 3.20 ms |

  The figure that counts, Release on the development machine, is the owner's run at milestone 39.
- **A deputy's defence is blunt.** Any enemy warship in a held sector, a passing scout among them, draws every idle warship, so one sector may be stripped to answer another. Whether that is good enough is the owner's week to judge.
- **A Shipyard idle at a restart builds the fleet's commonest design**, since the deputy learns a Shipyard's last design only from its queue, and nothing of it is saved.
- **A hand-over is not reported yet.** The world log, at milestone 39, records each one, and the client's panel saying what happened while the player was away needs the server's events of milestone 38.
- **An AI empire made afresh plays on, but not as it would have.** A world with AI empires replays from its log, not from its AIs.

## What this forecloses

- A deputy that attacks, raids, claims or clears pirates without a scheduled order (milestone 38).
- Hosted players off the server's thread, or answering a tick before every player has been sent its snapshot.
- A hosted player's state in a save.
