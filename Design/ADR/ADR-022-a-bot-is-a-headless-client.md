# ADR-022 — A bot is a headless client, and one process runs many of them to load the host

**Status:** Accepted — ruled 2026-09-23 by the owner: the placement, the purpose (stress testing the host from one machine) and a rule-based policy, on the record as proposed.
**Date:** 2026-09-23
**Owner:** Stefan Zwaal

## Context

Every client in this tree is a person with a finger on a packaged application. **Nothing can load the
host.** The join, the update and the command are each pinned by a socket-free suite (M1.4). But no run
puts many real clients on one host's wire at once. So there has been no measurement of what the host
does under a full complement of seats, under join churn, or under traffic that is malformed or hostile.
The owner's need is exactly that: **stress-test the server by running many clients from one location.**

A packaged client can't provide this. Each instance needs a visible window or Windows suspends it
(`Plan/M1-the-fleet.md` M1.15). Each one also needs the loopback exemption, which is already strained
(ADR-008). And it takes one process per seat, driven by a finger.

**There is no bot logic in the tree to reuse.** The tick's comment lists an AI phase, but it is not
written. The stub AI is M3's and the AI of `GameDesign.md` §8 is M4.5's. Whether M4.5's AI is written so a
bot can share it is [`OpenQuestions.md`](../OpenQuestions.md) Q48, and it is not decided here.

## Decision

**A bot is a client with no window, and `Bot` is an unpackaged desktop console application written in
C++/WinRT that runs many of them.** Each bot has its own socket and endpoint, joins as the packaged client
does (a `Join`, a slot and a token, ADR-013), receives updates and sends commands. The host cannot tell
a bot from a person. **Nothing changes on the host or the wire for the bot's sake.** The player count it
needs past four is [`ADR-023`](ADR-023-the-player-count-is-configurable.md)'s.

**One process, many clients, one thread for the loop.** A run is described on the command line: the host
address, how many of each role, a seed, and a length in ticks. The process starts one client per
requested bot and drives them all from one loop. Each client drains its own queue, advances its own join
state and policy, and sends. **A thousand clients should not cost a thousand processes**, and a single
loop makes the harness's own scheduling deterministic, so a run is repeatable apart from the network.
Datagram arrival is still asynchronous, one `DatagramSocket` per bot delivering into that bot's
`PacketQueue`, exactly as the packaged client works.

**Bots come in three roles, because a host has more to survive than players:**

| Role | What it does | What it loads |
|---|---|---|
| **Player** | Takes a seat and plays the rule-based policy below | The command path, the per-client accumulator and its egress ([`ADR-024`](ADR-024-replication-is-prioritized-records.md)) |
| **Churner** | Takes a seat, drops its socket, and rejoins with its token on a new endpoint, at a seeded interval | ADR-013's rejoin, endpoint rebinding, the session table |
| **Flooder** | Never seated. Sends joins after the match is full, commands from an endpoint the host never seated, and malformed datagrams: truncated, wrong version, impossible counts | Every refusal path, and that the host stays up and keeps its tick |

**It reuses the client libraries, not copies of them.** `Bot` references `GameClient` and `NeuronClient`,
the same pair the two client suites link, and for the same reason: a desktop binary can link Windows Store
static libraries (`AGENTS.md` §3). Players and churners use `DatagramTransport` and `PacketQueue`,
`JoinState`, and `ClientFrame` with its `ReplicaStore`. So a bot and a person cannot disagree about the
protocol, because there is one client-side implementation of it. **Flooders deliberately don't use it.**
They write bytes the encoder would refuse, which is their job. It links no `GameLogic`, so R19 holds.

**Being unpackaged, it can't use the three facilities that need package identity, and doesn't try:**

| Packaged client | Bot |
|---|---|
| Host address from `LocalState` (`ReadHostAddress`, ADR-008) | From the command line, parsed by `HostAddressFromFileContents` so there is still one parser |
| Session token in `LocalState\session.txt`, or `session-<n>.txt` per instance slot (`ReadSessionToken`) | Held in memory per bot for the life of the process |
| Instance slot claimed across processes (`InstanceSlot.h`) | Not needed. Every bot presents its own token |

**A seat is never given back, so the harness must never waste one.** ADR-013 holds a slot indefinitely
and forgets an endpoint but never a slot. A bot that restarts without its token is a new player and takes
a second seat, and a stress run that restarts bots would exhaust the match in a few cycles. So **a churner
always rejoins with the token it holds**, and a process that exits takes its seats with it. **To run
again, restart the host.** The harness says so when it is refused, and doesn't treat `MatchFull` as a
flake.

**It needs no loopback exemption**, because an unpackaged process isn't in an AppContainer. The host and
the harness on one machine work from a shell on any Windows box with the build, a CI runner included.

**The policy is rule-based, lives in `GameClient`, and is deterministic given the snapshots.** The
executable holds the Windows Runtime glue, the loop and the command-line parsing, and nothing else (R20).
The player's decision half is `BotPolicy`. It is a pure function of that bot's replica store, its
player, and a `Neuron::Pcg32` seeded from the run seed and the bot's index, and it returns the commands to
send, or none. The churner's and flooder's schedules are the same kind of function. **All three key their
pacing on the newest update's tick, or on a tick counted by the harness loop, never on the wall clock**, so a
suite can feed them inputs and assert what comes out. R16 doesn't bind a client, but a stress run that
can't be replayed from its inputs can't be debugged from a report of what it did.

**At M1 the player policy uses only what M1 has.** It builds what it can afford, cancels occasionally, and
moves subsets of its own ships to points on the plane. **It never commands an entity it doesn't own**; the
flooder is the role that sends refusable commands, deliberately and counted. It gains `Attack` when M3 does.

**The harness reports, and it reports what it saw on the wire, not what the host says.** Per role it
counts: seats taken and refused, updates received and lost (the transport sequence is for exactly this),
the gap between consecutive update ticks (a host that falls behind shows up here first), **the refresh
interval per entity**, which is ADR-024's figure and the one a stress run exists to measure, commands sent
and acknowledged, and the time from sending a command to the update acknowledging it. **The host's own tick cost is the host's to report.** It is
M2.10's measurement, and the harness doesn't try to infer it from outside.

**It is not the AI of `GameDesign.md` §8 and it doesn't replace M4.5.** §8's AI runs on the host, on the
tick, under R16, and can take over an abandoned slot mid-match (M4.6). The bot is how the host gets loaded
by something that isn't a person. Whether the two share their decision code is Q48.

## Consequences

**This adds a project row to `AGENTS.md` §2**, and `Bot` is the first executable that references the
client libraries without being packaged. The import table gains `Bot | — | GameClient, NeuronClient`, the
executable list becomes three, and `packages.config` goes on a sixth C++/WinRT project at the same pinned
version. That is a line, not a decision (R14). **R20 applies:** a decision found in `Bot/` is a defect.

**What it forecloses:** a bot that reads simulation state it wasn't sent. It links nothing that holds any.

**What it costs:**

- **A third executable to build on four configuration pairs**, only one of which CI builds (`AGENTS.md` §6).
- **The client libraries must stay linkable into an unpackaged process.** Today `ReadSessionToken`,
  `WriteSessionToken` and `ReadHostAddress` call `ApplicationData::Current()`, and the bot calls none of
  them. A client-library function that reaches package identity on a path the bot does use is a
  regression against this record. It shows up at run time as a `winrt::hresult_error`, not at link time.
- **One socket per bot.** The harness's ceiling is the machine's, not the protocol's: ephemeral ports,
  handles, and the thread pool that delivers `MessageReceived`. **The first run measures where that
  ceiling is**, and the harness refuses to start more bots than it has been shown to sustain, so a
  harness bottleneck isn't reported as a host result.
- **Hosting the harness and the host on one machine measures them together.** The one-location setup is
  what the owner asked for, and it is right for finding failures. **For a figure about the host's
  capacity, the harness runs on a second machine.**

**What it enables and doesn't include:** a CI step that starts `Server` and a small harness on the Windows
runner, asserts that every player is seated, that a command round-trips and that the host survives the
flooder, and fails otherwise. It is a separate step because it changes the workflow.

**What would reopen it:** a client library that can no longer be linked outside a package. That ends the
reuse and turns this into a second client implementation, and the right response is to fix the library.

## Measurements

None yet. **Owed at the step that builds it** (`Plan/M1-the-fleet.md` M1.14b), on the machine that
builds it:

- **That an unpackaged process can use `DatagramSocket` against a host on loopback with no exemption.**
  Everything above rests on this. It is documented Windows behavior, not yet observed in this tree.
- **That players are seated, that a churner keeps its seat across a rejoin on a new endpoint, that a
  command is acknowledged by a later update, and that the host keeps ticking under the flooder.** All
  against the real `Server`.
- **How many bots one harness process sustains** before its own update tick gap degrades with the
  host idle. That is the harness's ceiling, and it is stated here once measured. **Until then the harness
  refuses more than 128** (`HARNESS_BOT_CEILING`, `GameClient/StressReport.h`), a provisional bound and not
  a measured one.
- **The refresh interval per entity at every seated count the host allows**, against ADR-024's sweep,
  which is the measurement that record cannot take without this harness.
