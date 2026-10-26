# ADR-079 — A seat is its player's or its deputy's, and the server plays its deputies and AI empires on its own thread

Status: **accepted** · 2026-10-08
Owns: hosted-players, seat-controller, deputy, ai-empire

## Context

A universe keeps running while its players are away. A seat's deputy keeps its player's empire running from 60 s after the player's connection has gone, and hands the seat back when the player takes it again; an AI empire plays a seat of its own, to win (owner, 2026-10-08). A universe's clients are other processes than its server ([ADR-078](ADR-078-dedicated-server.md)), so the deputies and the AI empires run on the server's thread ([ADR-025](ADR-025-server-thread.md) decision 6), and a universe with them must still replay from its seed and its log.

A deputy is a keeper, not a player (owner, 2026-10-08): it keeps as many Constructors as its player had, and its defenders go back to where they stood once an attack is over.

## Decision

1. **A player the server plays itself is a `HostedPlayer`**, declared in `GameProtocol`. After a tick in which the seat is its, it is handed its player's snapshot, and `Play` returns the orders to apply. After a tick in which its player plays the seat, `Watch` gives it the snapshot to learn from, and it gives no order. `Server::Host` hands one to the server before the server starts. `GameLogic` never includes `Opponent`: `OutpostServer` makes the hosted players and hands them over.
2. **Hosted players play after each tick, on the server's thread.** Once every player has been sent its snapshot, each hosted player is handed its own. Its orders wait in its connection's queue, and the next tick applies and logs them as it does a client's. So a universe replays from its seed and its log without them ([ADR-009](ADR-009-deterministic-core.md)), and nothing of them is saved. Their time is the tick's `Hosted` part, `hosted` in the measurement log.
3. **A hosted player without a seat is the player: an AI empire.** `AiEmpire`, in `Opponent`, plays its seat with `AiPlayer`, which starts from any state of its player ([ADR-020](ADR-020-ai-and-match-flow.md) decision 16). It has a loopback connection of its own, and its snapshots are handed to it rather than queued.
4. **A hosted player with a seat is the seat's deputy, and `SeatController` says who plays the seat:**
   - the deputy, until the player first takes the seat, with no report of a time away for it ([ADR-080](ADR-080-events-and-scheduled-orders.md) decision 7);
   - the player, while its newest connection is open, and for `DEPUTY_DELAY_SECONDS` (60 s) of ticks after the server sees that connection gone (`QuicChannel::HasGone`);
   - the deputy after that, until the player takes the seat again, which hands it back at once.

   A connection that closes is seen gone at once. One whose client vanished is seen gone when MsQuic's idle timeout ends it; the game sets no idle timeout of its own.
5. **A snapshot shows its owner each ship's order** (`EntityView::order`; `ShipOrder`, in `GameProtocol`), which is how a deputy tells an idle ship from a busy one. A save keeps the enemy entities each player remembers as `EntityView`s, so they are part of the state's layout (AGENTS.md R18, [ADR-077](ADR-077-universe-state-on-disk.md)).
6. **The deputy is a keeper** (`Deputy`, in `Opponent`; owner, 2026-10-08). It decides once a second while it plays, and uses the Normal AI's settings for its research order and how long a defence holds.
   - **Research.** A Lab with a slot free takes the next topic of the AI's research order, then any topic it can take.
   - **Production.** A finished Shipyard whose queue is empty is given the design it last queued, which the deputy learns from the queue whether it plays or watches. A Shipyard it never saw build, as after the server restarted, gets the design most of its player's warships are of. It keeps one job queued, so that the Shipyard starts its next ship as soon as the Ore and the fleet cap allow, as any job waits for them. When the cap holds a Shipyard's next ship back, the deputy upgrades the Command Station once the Ore is there, as the AI does.
   - **Constructors.** It keeps as many as its player had when it took the seat (owner, 2026-10-08), counting those the Command Station has queued.
   - **Rigs.** It rebuilds a Mining Rig it saw stand, on an asteroid that still holds ore, in a sector its player holds and the pirates do not guard, with the two nearest idle Constructors.
   - **Repairs.** Each damaged structure, and each site with nobody on it, gets the nearest idle Constructor.
   - **Defence.** What draws its idle warships is enemy warships, the pirates' among them, in a sector its player holds, taking the sector with the most of them; or whatever fires on one of its player's structures in such a sector. Idle means no order, no standing order, not retreating and waiting on no scheduled order. They attack-move there. The defence ends `defenseHoldSeconds` after the attack was last seen, or at once when the sector is lost, and each defender then moves back to where it stood (owner, 2026-10-08).
   - **What it never does.** It never attacks, raids, claims a sector, clears pirates or salvages, except by a scheduled order its player gave, which the server fires whoever plays the seat ([ADR-080](ADR-080-events-and-scheduled-orders.md)). It leaves alone a ship that retreats, that has an order or a standing order, or that waits on a scheduled order.
   - **Each turn starts afresh.** Its Constructor count, repairs and defenders belong to the turn. What it learned while watching, each Shipyard's last design and where the rigs stood, it keeps.
7. **`OutpostServer` hosts them.** An AI seat of `Universe.json` ([ADR-078](ADR-078-dedicated-server.md) decision 2) is hosted as an `AiEmpire` with its difficulty's settings ([ADR-065](ADR-065-ai-difficulty.md)), and gets no seat to take and no join file. A player's seat is opened for its token ([ADR-078](ADR-078-dedicated-server.md) decision 3) and hosted with a `Deputy`.

## Consequences

- **Tests.**
  - `SeatControllerTests`: the hand-over and the hand-back, tick by tick.
  - `DeputyTests`, in `OpponentTests` ([ADR-089](ADR-089-test-dll-per-library.md)), on snapshots built by hand: each of decision 6's keeper rules, and a turn beginning afresh.
  - `InProcessServerTests.HostsAiEmpiresWhoseOrdersReplayFromTheLog`: a universe of two AI empires on the server's thread, three minutes of it replayed from the command log to an equal simulation; and `HostsAPlayerOnceAndBeforeItStarts`.
  - `UniverseSettingsTests`: AI seats, and `--new-universe --ai` run.
  - `QuicTransportTests.ADeputyPlaysItsSeatWhileItsPlayerIsAway` checks the hand-over over a real connection: the deputy plays an untaken seat, watches while its player plays, takes the seat exactly `DEPUTY_DELAY_SECONDS` of ticks after the connection went, and hands it back when the player returns.
  - `DeputyPlayTests.KeepsAnEmpireRunningAsTheAiKeepsItsOwn`, in `GameLogicTests`: two Normal AIs play for 10 minutes, then blue's seat is its deputy's for 10 more. The deputy's Shipyards and Lab stand idle with Ore to spend no more than 5 points of their time beyond the AI's, and it attacks, claims and salvages nothing and builds nothing but a lost rig. "Idle with Ore to spend" means a finished Shipyard with an empty queue while its player could pay for a ship that the cap had room for, or a Lab with nothing to research while a topic it could take was paid for.
  - `InProcessServerTests.MeasuresTheHostedPlayersTicks`, run with `OUTPOST_UNIVERSE_MEASURE`, times an hour of a universe with two Normal AI empires on the server's thread, the tick and its hosted part, for seeds 1 to 3.
- **A deputy's defence is blunt.** Any enemy warship in a held sector, a passing scout among them, draws every idle warship, so one sector may be stripped to answer another.
- **A Shipyard idle at a restart builds the fleet's commonest design**, since the deputy learns a Shipyard's last design only from its queue, and nothing of it is saved.
- **What happened while the player was away** is the host's report, kept from the events of the snapshots the deputy plays and handed to the player when it takes the seat again ([ADR-080](ADR-080-events-and-scheduled-orders.md)). A hand-over itself is in the universe log ([ADR-082](ADR-082-universe-log-and-universe-run.md)).
- **An AI empire made afresh plays on, but not as it would have.** A universe with AI empires replays from its log, not from its AIs.

## What this forecloses

- A deputy that attacks, raids, claims or clears pirates without a scheduled order of its player's.
- Hosted players off the server's thread, or answering a tick before every player has been sent its snapshot.
- A hosted player's state in a save.
