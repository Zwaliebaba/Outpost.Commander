# Outpost Commander — Play Over a Real Network

Status: **open** · gates M1–M11 open, for the owner · From the owner's request of 2026-10-26 for a plan from the review of the game's network code: the transport, the server's tick, the snapshot and the client's view, as they stand at `efbfbf2`

The game's server model stands: one authoritative server, fog of war applied there, commands up and snapshots down, and the AI as a player like any other (ADR-002). This plan is about the transport under it. Every skirmish crosses QUIC on the loopback (ADR-060 decision 1), which runs the transport's code in every match but never meets what a real network does: no loss, no jitter, no limit on bandwidth, and a round trip of nothing. A universe has yet to be played from another machine: the [Phase 5 plan's](ImplementationPlan-Phase5.md) tasks 36.1 and 36.3 await the owner's run, and ADR-004 says that latency over a real network is not measured. The review found that what the loopback hides is where the transport is weakest (§1).

This plan measures first, then changes what the measurement shows needs changing, in order. It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the ADRs and the finding the task names.**
2. **Take the lowest-numbered task whose status is `todo` and whose dependencies are `done`.** One PR per milestone (owner, 2026-09-30), in the milestone order below.
3. **ADRs are edited in place** (AGENTS.md §6), in the same commit as the change that implements them.
4. **A task that touches rendering, input or presentation is run, not only built**, and the owner's run accepts it.
5. **QUIC is checked in CI's Debug|x64 run only** (ADR-060), since the Linux container has no Schannel. A task that changes `QuicChannel`, `QuicTransport`, the listener or the server's seats is verified there, and says so; the container runs the rest.
6. **A change to the messages changes `WireLayoutHash`,** so a server refuses a game of the build before it (ADR-060 decision 5). A new message is added at the end of `Message`, so that a command's kind, which begins every record of a universe's command log, stays what it is (ADR-077 decision 7).
7. **A figure is measured, and says how** (AGENTS.md §6). The figures §1 marks as computed are worked out from the wire format and the shipped data; NW2 and NW3.1 measure them, and each measurement replaces its computed figure.

Task numbers carry the prefix NW, and gates are lettered M.

---

## 1. What the review found

Each finding cites the code as it stands at `efbfbf2`.

1. **Every message rides one stream.** A connection has one bidirectional stream: commands go up it and snapshots come down it, reliably and in order (ADR-060 decision 2; `NeuronCore/QuicChannel.cpp:560`). ADR-004 chose QUIC because a lost packet on one stream does not stall the others. With one stream, a lost packet holds back every snapshot behind it until it is sent again, and they then arrive together.
2. **Nothing limits what waits to be sent.** The server sends each seat its snapshot every tick, whatever its connection still has to deliver (`GameLogic/InProcessServer.cpp:551`), and the channel hands every send to MsQuic, which queues it (`NeuronCore/QuicChannel.cpp:412`). A client whose link carries less than a snapshot a tick falls further behind for as long as it plays, and the server's memory grows with the queue. Nothing reports it: the connection is alive, so no timeout ends it. The cause is the one ADR-060's first foreclosure names: the shots, destructions, events and the report of a time away ride in the snapshot, so no snapshot may be dropped.
3. **The view buffers one tick, and follows when snapshots arrive.** `SnapshotInterpolator` shows the simulation one tick, 50 ms at 20 Hz, behind the newest snapshot, measured from when that snapshot arrived, and jumps once it is 2 ticks out (ADR-013 decision 3; `GameApp/SnapshotInterpolator.h:24` and `:28`). A stall longer than 50 ms holds the view on the newest snapshot, since it never extrapolates (ADR-002 decision 5), and the burst after the stall jumps the view on.
4. **Most of a snapshot is what never changes.** Every snapshot carries the player's catalogue of hulls, drives, weapons, modules and research topics with their names and effects, the structure types, all 25 sectors with their names and borders, and the map's 86 asteroids and fields, which every player always sees (`GameLogic/Simulation.cpp:243` and `:1004`). Computed, not measured: an entity is 122 bytes on the wire, the asteroids and fields 10.5 KB, the catalogue 2.7 KB and the sectors 1.4 KB, so a snapshot is about 14.6 KB before a single ship, about 2.3 Mbit/s a player at 20 Hz. With 170 ships and structures in view beside them a snapshot is about 35 KB, some 30 packets of 1,200 bytes, and at 1% loss about one snapshot in four holds a lost packet, each of which stalls the stream (finding 1). The budgets' 8,065 bytes predates the 10 km map ([the budgets](../Budgets.md)).
5. **The listener keeps every connection it was ever given.** Each connection is added to the listener's list and taken off only when the listener goes (`NeuronCore/QuicChannel.cpp:528`), its MsQuic connection's handle held until then: every reconnect, every refused hello, and every stranger that reaches an open port. A universe runs for weeks.
6. **A connection that has not said hello is held to nothing.** The 64 MiB limit on a message (`NeuronCore/QuicChannel.cpp:17`) holds before the hello as after it, and the channel takes every byte it is sent at once, so flow control does not stop a sender: a peer that never takes a seat can make the server keep 64 MiB for each connection it opens. Nor does the server close a connection that sends no hello (`GameLogic/InProcessServer.cpp:240`).
7. **A seat's commands are not bounded.** A seated client's every command is queued (`GameLogic/InProcessServer.cpp:274`) and applied in the next tick (`GameLogic/InProcessServer.cpp:445`). A client that, by a bug, sends an order every frame puts several fleets' path plans into one tick, whose budget is 5 ms ([the budgets](../Budgets.md)).
8. **Nothing reads MsQuic's statistics.** The channel reports no round trip, loss, congestion window or backlog, and ADR-086 forecloses a round trip on the tag for that reason. The universe log records each seat's growth every hour (ADR-082 decision 3), and nothing of its connection.
9. **Every QUIC test runs on the loopback,** and no test impairs a link, so CI cannot see findings 1 to 3.
10. **A connection lost in play goes back to the menu** (ADR-078 decision 10). In a universe, where the newest connection with the seat's token takes the seat at once (ADR-078 decision 4), a player whose link dropped for a moment could be joined again without being asked.
11. **An order shows nothing until its snapshot arrives.** Over the loopback an order's response is a mean of 48 ms against its 150 ms target ([the budgets](../Budgets.md)). Over a network the round trip adds to it, and every tick the view has to buffer to ride out jitter (finding 3) adds 50 ms more. The client marks nothing as the order is given, which is what a game that predicts nothing can still show at once (ADR-002 decision 5). And a refusal names no command, so a client cannot tell which of its orders a refusal answers (ADR-094 decision 1).

## 2. What it keeps, and what it does not take on

- **The server model** (ADR-002): no lockstep, and no prediction of what a ship does. Whether play over a real network needs an acknowledgement or a prediction is still decided from an order's response measured over one (ADR-002 decision 5). NW6.4 makes that measurement, and gate M11 puts the decision to the owner.
- **QUIC through MsQuic** (ADR-004). The findings are in how the game uses QUIC, not in QUIC: raw UDP with hand-written reliability, which ADR-004 forecloses, would buy nothing here.
- **Bandwidth, as gate F5 of [the Horizon plan](ImplementationPlan-Horizon.md) decided it on 2026-10-25:** the snapshots as they are, measured against the budgets, and deltas or datagrams only if the measurement says so. NW2 is that measurement. NW3 and NW4 are neither deltas nor datagrams: they answer the stall and the backlog of findings 1 and 2, and they go ahead where NW2's figures miss gate M1's targets. Deltas are NW7, which waits on F5's condition. If the owner reads F5 as covering NW3 and NW4 too, gates M3 and M4 are where that is said.
- **The trust of a circle of friends** (ADR-078): tokens in plain text and a pinned certificate. NW1's limits are against a broken client and a stranger on an open port, not against a friend.
- **Two seats.** A universe has a seat for each of the map's starts (Game §universe.seats). Seats for more players are Phase 7's, [the Horizon plan's](ImplementationPlan-Horizon.md) milestone 48 and its gate F1. More seats multiply findings 2 and 4 on the server's uplink, which is why NW7 stays in view.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| NW1.1 | The listener lets go of a connection that has gone | — | — | todo |
| NW1.2 | A connection is held to its hello | — | — | todo |
| NW1.3 | A seat's commands are bounded | — | M9 | todo |
| NW2.1 | The connection's figures | — | — | todo |
| NW2.2 | The impaired link | NW2.1 | M1 | todo |
| NW2.3 | The connection's figures over a real network | NW2.1 | M1 | todo; an owner run |
| NW3.1 | What does not change crosses once | NW2.2 | M2, M3 | todo |
| NW3.2 | A command carries its number | NW3.1 | M7 | todo |
| NW4.1 | Events and state cross apart, and the newest state wins | NW3.1 | M4 | todo |
| NW5.1 | The view's delay follows the link | NW4.1 | M5 | todo |
| NW6.1 | An order is marked as it is given | NW3.2 | M6 | todo |
| NW6.2 | A universe's lost connection is joined again | NW2.2 | M8 | todo |
| NW6.3 | The tag gives the round trip | NW2.1 | M10 | todo |
| NW6.4 | Order to response over a real network, again | NW5.1, NW6.1 | M11 | todo; an owner run |
| NW7.1 | State crosses as what changed | NW4.1 | F5 of the Horizon plan | todo; only if F5's measurement asks for it |

### Milestone order

NW1, NW2, NW3, NW4, NW5, NW6, NW7.

- **NW1 changes no message,** so a universe made before it runs on after it. A universe that runs a week with its port forwarded is what findings 5 to 7 meet, so NW1 is meant to land before the owner's week, the Phase 5 plan's task 39.4; when the week runs is the owner's.
- **NW2 changes nothing a player sees.** Its figures are the measurement gate F5 asks for, and they set gate M1's targets. A target NW2 shows held needs no change: NW3 and NW4 are for the targets it shows missed.
- **NW3.2 changes what a universe saves** (AGENTS.md R18), so a universe of the build before it is refused. It lands after the owner's week, or the week's universe is made again. NW3.1 changes no save.
- **NW4 before NW5.** The view's delay is set from the jitter it meets, and until NW4 the stall of finding 1 is most of that jitter.
- **NW6 after NW5,** so that the owner's runs over a real network judge the whole: the streams, the view and the mark together. NW6.2 and NW6.3 depend on less, and could go sooner, in a milestone of their own, if the owner asks.
- **NW7 only if F5's measurement asks for it,** most likely with seats for more players.

---

## Gates

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| M1 | **The links a change is measured over, and what it is held to.** Proposed, three profiles of `ImpairedLink`, each way: *near*, a 20 ms round trip, ±5 ms of jitter and 0.1% loss; *Wi-Fi*, 40 ms, ±15 ms and 1%; *hotspot*, 80 ms, ±30 ms, 2%, and 4 Mbit/s to the client, below the snapshot rate under load (§1 finding 4). Proposed targets, to be rows of [the budgets](../Budgets.md) once NW2 has measured against them: on every profile, no more than two ticks of a seat's state waiting on the server; the newest state a client holds more than two ticks behind the server in at most 1% of samples on near and Wi-Fi, and 5% on hotspot; an order's response at most 150 ms plus the link's round trip, at the 95th percentile; and no target for bandwidth, which F5 has measured, not held. | this plan, §1 findings 1 to 4 | NW2.2 and NW2.3, and whether NW3 and NW4 go ahead | open |
| M2 | **The version of a changed protocol.** F5 has deltas or datagrams come "as a new protocol version", while `PROTOCOL_VERSION` stays 1 until the game is live (AGENTS.md R18; owner, 2026-10-09) and the layout's hash refuses a game of another build (ADR-060 decision 5). Proposed: until the game is live, a change to the messages is told by the hash alone, and F5's new version is read as that. | this plan, §2 | NW3.1 | open |
| M3 | **What crosses once.** Proposed: on the wire only, so that `Snapshot` stays what every reader reads: the client, the AI, a deputy, the match log and the universe log. The server's end of a QUIC seat keeps the catalogue it last sent that seat (the hulls, drives, weapons, modules, research topics, designs and structure types as the player's research has them, the sectors' names and borders, the map's size, the Constructor's cost and time, the tickets and their drain, and the asteroid fields), and sends it whole when the seat is taken and before the snapshot of any tick in which it changed; the snapshot on the wire leaves it out. `QuicTransport` keeps the last catalogue and puts it back, so that every snapshot it hands on is the snapshot the server built. The server finds a change by comparing, so `Simulation` holds nothing new (AGENTS.md R18). The sectors' holders, suppression, cut-off and guard stay in the snapshot, and so do the ore asteroids, whose reserves change as they are mined. | this plan, §1 finding 4 | NW3.1 | open |
| M4 | **The streams.** Proposed: commands go up the bidirectional stream the client opens, as now, and each tick's shots, destructions, events and report of a time away come down it, reliably and in order. Each tick's state comes down a unidirectional stream of its own, which the server opens and ends with the state. When the server sends a seat a state, it aborts every older state stream of that seat not yet acknowledged, so that a seat has only the newest state on its way and no backlog; an abort loses only a state not yet whole at the client. The events go first when both have something to send. `QuicTransport` hands on a snapshot for each state it receives, with the events of every tick since the last it handed on, each shot and destruction carrying its tick, so that an explosion still plays where it happened. The alternatives: QUIC datagrams for state, which at 1,200 bytes would have to be cut up and put back together, reliability written by hand under another name; and one stream on which state is skipped while a seat's backlog is over a snapshot, which bounds memory and latency in a day's work but keeps the stall of finding 1. | this plan, §1 findings 1 and 2; ADR-060's first foreclosure | NW4.1 | open |
| M5 | **The view's delay.** Proposed: the view runs behind the newest state by what the link needs rather than by one tick: by how late states have arrived over the last 10 s, at their 95th percentile, at least 1 tick and at most 4, 200 ms at 20 Hz. Its clock follows the server's ticks, as an offset from the frame's clock smoothed over many arrivals, rather than the arrival of the newest. It still never extrapolates. On the loopback it stays at 1 tick, so a skirmish shows what it shows now. | this plan, §1 finding 3 | NW5.1 | open |
| M6 | **The order's mark.** Proposed: as the player gives a move, an attack-move, a hold of a sector or a patrol, its point is marked on the ground at once, and an attack's target, by a ring in the color of the selection's ring (ADR-067) that shrinks to nothing over 0.4 s; a refusal of that order (gate M7) turns its mark the warning color (ADR-085 decision 1). It predicts nothing: the ships move when a snapshot says they do. | this plan, §1 finding 11 | NW6.1 | open |
| M7 | **A command's number.** Proposed: the client numbers its commands, and a refusal names the number of the command it refuses (ADR-094 decision 1); the server trusts the number for nothing (ADR-002 decision 2). The order's mark reads it now (gate M6), and an acknowledgement or a prediction would need it later (gate M11). `EventView` carries it, so the layout of what a universe saves changes and version 1's hash is replaced (AGENTS.md R18), with what that does to a running universe, as the milestone order says. | this plan, §1 finding 11 | NW3.2 | open |
| M8 | **A universe's lost connection.** Proposed: in a universe, a connection lost for any reason but the server's own close (`Finished`, `WrongVersion`, `SeatRefused` or `SeatTaken`), or silent for 10 s, is joined again with the same join file, at once and then every 2 s for 30 s, while the match stays on screen and the tag says "Reconnecting · 5 s"; after that, the menu, as now. 30 s is half the deputy's minute, so a link that drops for a moment never hands the seat to the deputy (ADR-079 decision 4). A skirmish, whose server is in the game, goes back to the menu as now. | this plan, §1 finding 10 | NW6.2 | open |
| M9 | **A seat's commands.** Proposed: a seated client's message is at most 64 KiB, which holds a command naming every ship a fleet cap allows many times over; a seat's commands are applied at most 32 a tick, and the rest wait for the next tick, in the order given; and a connection with more than 1,200 waiting, nearly two seconds behind at 32 a tick, is closed as `MalformedMessage`. Every connection is held to them, the loopback's and the hosted players' too, so that what the tests play is what a match plays; NW1.3 shows that no AI meets them. | this plan, §1 finding 7 | NW1.3 | open |
| M10 | **The round trip on the tag.** Proposed: in a universe the tag gives the connection's smoothed round trip beside the address, in whole milliseconds, "Server 203.0.113.5:4433 · 42 ms"; a skirmish's tag stays "Local skirmish". It lifts ADR-086's foreclosure of a round trip on the tag. | this plan, §1 finding 8 | NW6.3 | open |
| M11 | **Acknowledgement or prediction** (ADR-002 decision 5), from NW6.4's measurement over a real network. Proposed: neither, while the order's mark and the measured response hold gate M1's target; if the response misses it, which of the two is a plan of its own. | this plan, §2 | A later plan, if any | open |

---

## Milestone NW1 — A server that runs for weeks

### NW1.1 — The listener lets go of a connection that has gone

- **Finding:** 5.
- **Scope:**
  - The listener lets go of a channel once its connection has gone and no seat holds it. It does so from a thread of the server's and never from MsQuic's, since `QuicChannel::Close` waits for MsQuic's last callback and is never called from one (ADR-060 decision 7).
  - `QuicListener` says how many channels it holds, for the tests.
- **Not in scope:** the limits on what a connection sends, which are NW1.2's and NW1.3's.
- **ADR:** [ADR-060](../ADR/ADR-060-quic-in-process.md) decision 7, which says who closes what, and when.
- **Acceptance:** `QuicTransportTests.LetsGoOfConnectionsThatHaveGone`: 200 connections refused for a wrong token, and 200 that take a universe's seat in turn and close, leave the listener holding only the connections still open; a connection whose seat a newer one took is let go too. The tests of a seat taken again and of the deputy's hand-over pass as they are.
- **Verify:** the checkers; CI's Debug|x64 run.

### NW1.2 — A connection is held to its hello

- **Finding:** 6.
- **Scope:**
  - Before its hello, a connection may send one message of at most 256 bytes; a hello is 33. A longer one closes it with `Neuron::FRAMING_ERROR_CODE`, as a message over the limit does now.
  - A connection that has sent no hello 5 s after its handshake is closed with `CloseReason::MalformedMessage`.
  - The channel takes the first message's limit from whoever reads it, the listener's `Accept` giving it with the receiver, so that the engine still knows nothing of a hello (R9).
- **ADR:** [ADR-060](../ADR/ADR-060-quic-in-process.md) decisions 2 and 5.
- **Acceptance:** `QuicTransportTests.RefusesAFirstMessageLongerThanAHello` and `ClosesAConnectionThatSaysNoHello`; every test of the hello passes as it is.
- **Verify:** the checkers; CI's Debug|x64 run.

### NW1.3 — A seat's commands are bounded

- **Finding:** 7. **Gate:** M9.
- **Scope:** gate M9's three limits: the seated client's message in the channel, and the commands a tick applies and the commands that may wait in the server's tick, for every connection.
- **ADR:** [ADR-009](../ADR/ADR-009-deterministic-core.md) decision 6, the order commands are applied in, which the commands that wait keep; [ADR-002](../ADR/ADR-002-authoritative-server.md) decision 2.
- **Acceptance:**
  - `InProcessServerTests.AppliesASeatsCommandsAtItsShare`: 100 orders sent in one tick are applied 32, 32, 32 and 4 over four ticks, in the order given, each logged with the tick it was applied at, and the log replays to an equal simulation.
  - `QuicTransportTests.ClosesASeatThatFloodsIt`: a client that has more commands waiting than gate M9 allows is closed, and told why.
  - `--ai-matches` over seeds 1 to 40 plays the same matches with the limits as without, its match log the same line for line, so no AI meets them; the most commands any seat gave in one tick is recorded in [the balance log](../Records/BalanceLog.md).
- **Verify:** the checkers; CI; the container's `--ai-matches`, with the limits and without.

---

## Milestone NW2 — The network, seen

### NW2.1 — The connection's figures

- **Finding:** 8.
- **Scope:**
  - `QuicChannel::Statistics` reads MsQuic's figures for the connection: the smoothed and the least round trip, the packets sent and those suspected lost, the congestion window, the bytes in flight, and the channel's backlog, the bytes it has handed MsQuic that the peer has not yet acknowledged.
  - For the backlog, a send completes only once the peer has it, so MsQuic's send buffering is turned off (`SendBufferingEnabled`). The task checks that this is what MsQuic 2.6.3 does with it, and what it costs a snapshot's send.
  - The server takes each seat's figures once a second. `--measure` logs them beside the ticks, and `Tools/FrameTimes.py` summarizes them. The universe log's hour record gains each seat's median and 95th percentile round trip, its loss and its largest backlog, and `Tools/UniverseLog.py` reports them. The client logs its own connection's figures in `--measure`.
- **ADR:** [ADR-004](../ADR/ADR-004-quic-transport.md), a decision of its own for what the channel reports; [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 3, the hour's record. ADR-086's foreclosure of a round trip on the tag stands until gate M10.
- **Acceptance:** `QuicTransportTests.ReportsItsConnection`: a round trip is reported once a snapshot has crossed, the packets sent count up, and a seat's backlog comes back to nothing once its client has read every snapshot. The universe log's tests read an hour's record with the figures, and `Tools/UniverseLog.py` still reads a log written before them.
- **Verify:** the checkers; CI; a skirmish with `--measure` on the development machine, its loopback figures recorded.

### NW2.2 — The impaired link

- **Finding:** 9. **Gate:** M1.
- **Scope:**
  - `ImpairedLink` in `GameLogicTests`: a UDP relay on the loopback between a `QuicTransport` and the server's listener, written with Winsock (R14: the Windows SDK), that drops, delays and holds to a rate each direction's datagrams, from a seed, by gate M1's profiles. It is the test project's own helper ([ADR-089](../ADR/ADR-089-test-dll-per-library.md) decision 3).
  - The tests CI runs ask only what holds by a wide margin on any runner: through the relay with no impairment every snapshot of a match arrives, and under each profile a seat is taken, an order reaches the simulation, and a deputy takes the seat and hands it back. They time nothing tightly, since a runner's timing is not the relay's.
  - `ImpairedLinkTests.MeasuresTheProfiles`, run only with `OUTPOST_NETWORK_MEASURE` set, as `UniverseFolderTests.MeasuresALateSave` is run only with its variable, plays two minutes of the measurement load (`ServerDesc::measurementLoad`), ordered across the map as `--load` orders it, through each profile. It reports how late each state arrives, at the median and the 95th and 99th percentiles; the longest gap between two arrivals; the share of 60 Hz samples at which the newest state the client holds is more than two ticks behind the server; the seat's largest backlog, from NW2.1; and an order's response, from its send to the first snapshot in which an ordered ship has moved.
- **ADR:** none of its own, since it is a test project's helper; [ADR-060](../ADR/ADR-060-quic-in-process.md)'s Consequences name the tests.
- **Acceptance:** the tests CI runs pass. The measuring test's figures for the transport as it stands are recorded in [the balance log](../Records/BalanceLog.md); they are the baseline that gate M1's targets are set against, and they decide whether NW3 and NW4 go ahead.
- **Verify:** the checkers; CI; `OUTPOST_NETWORK_MEASURE` on the development machine.

### NW2.3 — The connection's figures over a real network

- **Finding:** 9. **Gate:** M1.
- **Scope:** an owner run, and no code: the Phase 5 plan's runs of tasks 36.1 and 36.3, made on a build with NW2.1 so that their logs carry the figures. The owner's universe runs on the owner's PC and is joined from a second machine on another network, over Wi-Fi and over a phone's hotspot. The same links serve [the budgets'](../Budgets.md) run of an order's response over a real network, which this plan holds.
- **Acceptance:** the figures recorded in [the balance log](../Records/BalanceLog.md), and the latest in their rows of the budgets; gate M1's profiles checked against what the real links measured, and changed where they differ.
- **Verify:** the owner's run.

---

## Milestone NW3 — What does not change crosses once

### NW3.1 — What does not change crosses once

- **Finding:** 4. **Gates:** M2, M3.
- **Scope:**
  - A message for gate M3's catalogue, added at the end of `Message`. The server's end of a QUIC seat sends it when the seat is taken, before the first snapshot, and again before the snapshot of any tick in which it changed. The snapshot on the wire leaves out what it carries.
  - `QuicTransport` keeps the last catalogue and puts it back into every snapshot it hands on, with the asteroid fields among the entities in identifier order, as `SnapshotInterpolator` pairs them.
  - The loopback, the hosted players, the match log and the universe log are untouched: they read `Snapshot`, which does not change.
- **Not in scope:** the streams, which are NW4.1's, and deltas, which are NW7.1's.
- **ADR:** [ADR-060](../ADR/ADR-060-quic-in-process.md) decision 4, the wire format, with a decision of its own for what crosses once. [ADR-002](../ADR/ADR-002-authoritative-server.md) decision 4 stands, since a snapshot for each player is still what a client reads.
- **Acceptance:**
  - `WireFormatTests`: a snapshot with every field set crosses with its catalogue and is put back equal to the snapshot sent; every truncation of the catalogue's message is refused, as a snapshot's is.
  - `QuicTransportTests.HandsOnTheSnapshotsTheServerBuilt`: two minutes of a match with research finished in them, read through QUIC, equal the snapshots the server built, tick by tick; a seat taken again gets its catalogue before its first snapshot.
  - The 20th tick's snapshot of a match, and one late in a universe, measured on the wire before and after by [the budgets'](../Budgets.md) method for a snapshot's size, with the bandwidth a player takes; recorded in the balance log, and the latest in the budgets.
- **Verify:** the checkers; CI's Debug|x64 run; the container's measurement of a snapshot's size.

### NW3.2 — A command carries its number

- **Finding:** 11. **Gate:** M7.
- **Scope:**
  - The client numbers its commands, and a refusal's event names the number of the command it refuses. The words of a refusal and its alert are unchanged (ADR-094 decision 4).
  - `EventView` carries the number, so `UNIVERSE_STATE_VERSION` 1's hash is replaced (AGENTS.md R18). A command's layout changes too, and with it the wire and the command log's records (ADR-077 decision 7). A universe of the build before is refused, as R18 has it until the game is live.
- **ADR:** [ADR-094](../ADR/ADR-094-a-refused-command-is-told.md) decision 1; [ADR-060](../ADR/ADR-060-quic-in-process.md) decision 4.
- **Acceptance:** `InProcessServerTests.TellsARefusalToItsPlayerAlone` names the numbers of the two commands refused; `WireFormatTests` carries a refusal with its number; `UniverseStateTests.TheLayoutIsTheVersions` holds the new hash.
- **Verify:** the checkers; CI.

---

## Milestone NW4 — State that may be dropped

### NW4.1 — Events and state cross apart, and the newest state wins

- **Findings:** 1 and 2. **Gate:** M4.
- **Scope:**
  - Gate M4's streams: commands up the client's stream and each tick's events down it, each tick's state down a stream of its own, and an older state stream not yet acknowledged aborted by a newer one.
  - The client lets the server open state streams, which MsQuic does not let a peer do unless it is told (`PeerUnidiStreamCount`); the listener still lets a client open one stream (ADR-060 decision 2).
  - A state the server cannot open a stream for, because the client has as many open as it allows, is not sent: the next tick's is.
  - `QuicTransport` hands on a snapshot for each state, with every tick's events since the last. Each shot and destruction carries its tick, and the client plays each from one tick before its own, where the view shows it, rather than from the snapshot's.
  - A seat whose connection has gone is sent no more state, rather than having it queued.
- **Not in scope:** the view's delay, which is NW5.1's.
- **ADR:** a new ADR that takes the streams from [ADR-060](../ADR/ADR-060-quic-in-process.md) decision 2, which keeps the framing. ADR-060's first foreclosure goes, since its condition, state split from events, is met. [ADR-025](../ADR/ADR-025-server-thread.md) decision 4 and [ADR-060](../ADR/ADR-060-quic-in-process.md) decision 7 for who touches what on which thread; [ADR-078](../ADR/ADR-078-dedicated-server.md) decision 4 for what a seat taken again misses.
- **Acceptance:**
  - `ImpairedLinkTests`: on gate M1's hotspot profile, a seat's backlog stays at two states or fewer over a minute of the measurement load; on its Wi-Fi profile, every tick's events arrive, once and in order.
  - Through the relay with no impairment every tick's state arrives, so a skirmish loses nothing.
  - The tests of the deputy's hand-over, a seat taken again, a refusal told and the silence's warning pass as they are.
  - `MeasuresTheProfiles` run again, its figures recorded beside NW2.2's.
- **Verify:** the checkers; CI's Debug|x64 run; `OUTPOST_NETWORK_MEASURE` on the development machine.

---

## Milestone NW5 — The view rides out the jitter

### NW5.1 — The view's delay follows the link

- **Finding:** 3. **Gate:** M5.
- **Scope:** `SnapshotInterpolator` as gate M5 has it: the delay from how late states arrive, between 1 and 4 ticks; the clock from the server's ticks; and still no extrapolation. A ship's banking, which reads the interpolator's motions ([ADR-029](../ADR/ADR-029-ship-banking.md)), and the effects, which play from their ticks, follow the view as they do now.
- **ADR:** [ADR-013](../ADR/ADR-013-client-view-and-controls.md) decision 3.
- **Acceptance:** `SnapshotInterpolatorTests`: states that arrive every tick keep the delay at 1 tick and the view where it is now; arrivals with the jitter of each of gate M1's profiles hold the view still in no more 60 Hz frames than gate M1's target allows; a burst after a stall moves the view on smoothly, never more than a tick in a frame; and the delay comes back down within 10 s once the jitter ends.
- **Verify:** the checkers; CI and the container; run over Wi-Fi and a phone's hotspot against the owner's universe; the owner's run.

---

## Milestone NW6 — What the player sees of the network

### NW6.1 — An order is marked as it is given

- **Finding:** 11. **Gate:** M6.
- **Scope:** gate M6's mark, drawn over the ground with the other marks in the world (Game §interface.marks); a refusal that names the order's number (NW3.2) turns it.
- **ADR:** [ADR-013](../ADR/ADR-013-client-view-and-controls.md) decision 5, which turns events into commands; the mark's look as a decision of its own beside the selection's ring ([ADR-067](../ADR/ADR-067-selection-ring-at-a-fixed-width.md)); Game §interface.marks.
- **Acceptance:** `PlayerControlsTests`: a move, an attack-move, a hold of a sector and a patrol each mark their point as the order is given, and an attack its target; a refusal of the order's number turns its mark; a mark is gone after 0.4 s. `GameClient`'s drawing needs D3D12 and Windows, so it has no test; CI builds it.
- **Verify:** CI; run in a skirmish and against a dedicated server; the owner's run.

### NW6.2 — A universe's lost connection is joined again

- **Finding:** 10. **Gate:** M8.
- **Scope:** gate M8's rejoin, made on a thread of the shell's as a join is now (ADR-078 decision 9), with the match on screen meanwhile; the tag's "Reconnecting" and its seconds; and the menu with its notice after 30 s, or at once on the server's own close.
- **ADR:** [ADR-078](../ADR/ADR-078-dedicated-server.md) decision 10; [ADR-086](../ADR/ADR-086-where-the-match-runs.md) decision 2, the tag's warning; Game §session.join.
- **Acceptance:** `QuicTransportTests.TakesItsSeatAgainAfterItsLinkWent`: through `ImpairedLink`, a client whose link carries nothing for 10 s connects again with its seat's address and token and takes its seat, and the server closes its old connection with `SeatTaken`; a client refused with `SeatTaken`, `SeatRefused` or `WrongVersion` does not try again. `HudTests.ShowsWhereTheMatchRuns` sees the tag's "Reconnecting · 30 s" at its widest. The shell's retries need Windows; CI builds them.
- **Verify:** CI; against the owner's universe, with the network cable pulled for 10 s and for 90 s; the owner's run.

### NW6.3 — The tag gives the round trip

- **Finding:** 8. **Gate:** M10.
- **Scope:** gate M10's round trip on a universe's tag, from NW2.1's figures, once a second.
- **ADR:** [ADR-086](../ADR/ADR-086-where-the-match-runs.md) decision 1, whose foreclosure of a round trip goes; Game §interface.hud.tag.
- **Acceptance:** `HudTests.ShowsWhereTheMatchRuns` with the round trip at its widest, 999 ms; the whole HUD's contrast and overlap tests see it through `LongestContent`, as ADR-086's Consequences have them.
- **Verify:** CI; run against a dedicated server; the owner's run.

### NW6.4 — Order to response over a real network, again

- **Gate:** M11.
- **Scope:** an owner run, and no code: [the budgets'](../Budgets.md) method for an order's response, over the loopback and over NW2.3's real links, on a build with NW4 to NW6.
- **Acceptance:** the figures recorded against gate M1's target, and gate M11 put to the owner with them.
- **Verify:** the owner's run.

---

## Milestone NW7 — State as what changed, if the measurement asks

### NW7.1 — State crosses as what changed

- **Gate:** F5 of [the Horizon plan](ImplementationPlan-Horizon.md).
- **Outline, not scoped:** only if F5's measurement says bandwidth is what binds, most likely with seats for more players. The client acknowledges, on its stream, the newest state it holds. The server sends each entity's changed fields against that state, and a whole state when a seat is taken or the state acknowledged is too old. All of it is in the transport, as NW3.1 is, so that `Snapshot` is still what every reader reads. A moving ship changes about 12 of its 122 bytes a tick (§1 finding 4, computed).
