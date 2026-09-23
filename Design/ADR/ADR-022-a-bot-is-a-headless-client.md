# ADR-022 — A bot is a headless client, and it is a fixture rather than an opponent

**Status:** Proposed
**Date:** 2026-09-23
**Owner:** the owner — placement, purpose and the policy's kind were chosen on 2026-09-23; the record's text has not been ruled on

## Context

Every client in this tree is a person with a finger on a packaged application. That leaves three things
nothing can do today:

- **Fill a second seat without a second person.** M1.15 needs two clients on one host, and its one-machine
  route runs into the fact that Windows suspends a packaged application that is not visible
  (`Plan/M1-the-fleet.md` M1.15, `Plan/README.md` F5). That gate is about two people playing, so a bot does
  not close it. What it does is make "a second client is connected and issuing orders" something a bot
  can provide, so one person can check everything else about a two-client match.
- **Exercise the protocol end to end with nobody watching.** Every property of the join, the snapshot and
  the command is pinned by a socket-free suite (M1.4). Nothing puts a real host and a real client on the
  same wire unattended: no soak test, no reconnect check across a real socket, and no run of four seats
  when M4 opens them.
- **Test the command path from outside the host.** `GameDesign.md` §8 says an AI "issues the same orders a
  human does, through the same command path" and "is not given information a human in its position would
  not have". The host-side AI of §8 meets that by discipline. A client meets it by construction, because
  it holds no simulation (R19) and knows only what it was sent.

`GameDesign.md` §2 already expects this: *"Four AI players and no human is not a game; it is a test
fixture, and it is worth having for exactly that."* It never says where such a fixture lives. This record
says where.

## Decision

**A bot is a third executable, `Bot`: an unpackaged desktop console application written in C++/WinRT.**
It connects to the host exactly as `OutpostCommander` does. It sends a `Join`, is given a slot and a token
(ADR-013), receives snapshots and sends commands, and the host cannot tell it apart from a person. **Nothing
changes on the host and nothing changes on the wire.**

**It reuses the client libraries instead of copying them.** `Bot` references `GameClient` and
`NeuronClient`, the same pair the two client suites link, and for the same reason: a desktop binary can
link Windows Store static libraries (`AGENTS.md` §3). It uses `DatagramTransport` and `PacketQueue` for
the socket, `JoinState` for the handshake, and `ClientFrame` and `ReplicaStore` to turn snapshots into
state. So a bot and a person cannot disagree about the protocol, because there is only one client-side
implementation of it. It links no `GameLogic`, which keeps R19 true.

**Being unpackaged, it cannot use the three facilities that need package identity, and it does not
try:**

| Packaged client | Bot |
|---|---|
| Host address from `LocalState` (`ReadHostAddress`, ADR-008) | From the command line, parsed by `HostAddressFromFileContents` so there is still one parser |
| Session token in `LocalState\session.txt`, or `session-<n>.txt` per instance slot (`ReadSessionToken`) | Kept in memory for the life of the process. A restarted bot is a new player, which is right for a fixture |
| Instance slot claimed across processes (`InstanceSlot.h`) | Not needed. Two bots are two tokens without any coordination |

**It needs no loopback exemption**, because an unpackaged process is not in an AppContainer. That exemption
is what makes same-machine play strain ADR-008. The bot is unaffected by it, so a bot and a host on one
machine is an arrangement that works from a shell on any Windows box with the build, a CI runner included.

**The policy is rule-based, it lives in `GameClient`, and it is deterministic given the snapshots.** The
executable holds the Windows Runtime glue and the loop, and nothing else (R20). The decision half is
`BotPolicy`. It is a pure function of the replica store's newest snapshot, the bot's player, and a
`Neuron::Pcg32` seeded from the command line, and it returns the commands to send, or none. It keys its
pacing on the snapshot's tick, never on the wall clock, so a suite can feed it snapshots and assert what
comes out. R16 does not bind a client, but a fixture that cannot be replayed from its inputs cannot be
debugged from a report of what it did. The one thing outside its control is which snapshots arrive, and
that is the network's.

**At M1 the policy only uses what M1 has**: it builds what it can afford, cancels occasionally, and moves
selections of its own ships to points on the plane. **It gains `Attack` when M3 does**, and nothing else
in it has to change for that.

**It is not the AI of `GameDesign.md` §8, and it does not replace M4.5.** §8's AI runs on the host, on the
tick, under R16, and can take over an abandoned slot from arbitrary mid-match state (M4.6). A bot can do
none of that: it is a process that has to be started, and a slot it leaves is a slot it leaves.
**The two exist for different reasons.** §8's AI is how one person gets an opponent. The bot is how the
protocol gets exercised by something that is not a person. If the bot's policy ever grows into something
worth playing against, that is the point to revisit this record, not a reason to grow it.

## Consequences

**This adds a project row to `AGENTS.md` §2**, and it is the first executable that references the client
libraries without being packaged. The import table gains `Bot | — | GameClient, NeuronClient`, the
executable list becomes three, and `packages.config` is on a sixth C++/WinRT project at the same pinned
version. That is a line and not a decision (R14). **R20 applies to it as it does to the other two:** a
policy decision found in `Bot/` is a defect.

**What it forecloses:** a bot that reads simulation state it was not sent. It cannot, because it links
nothing that holds any, and that is the property worth keeping. A bot that "just peeks" at `GameLogic` for
a better decision is R19 broken with extra steps.

**What it costs:** a third executable to build on four configuration pairs, only one of which CI builds
(`AGENTS.md` §6). It also depends on the client libraries staying linkable into an unpackaged process.
**Today three functions in `NeuronClient` call `ApplicationData::Current()`**: `ReadSessionToken`,
`WriteSessionToken` and `ReadHostAddress`. The bot calls none of them. A client-library function that
reaches package identity on a path the bot does use (`ClientFrame`, `JoinState`, `ReplicaStore`,
`DatagramTransport`) is a regression against this record. The bot notices it at run time, as a
`winrt::hresult_error` at start-up, not at link time.

**What it enables and does not include:** a CI step that starts `Server` and two bots on the Windows runner,
asserts that both are seated and that a command round-trips, and fails if not. That is the first check in
this tree that puts two real endpoints on one wire. It is a separate step because it changes the workflow,
and `AGENTS.md` §6 says a prose copy of the workflow is not where decisions go.

**What would reopen it:** a client library that can no longer be linked outside a package, which ends the
reuse and turns this into a second client implementation. That is a worse trade than the fixture is worth,
and the right response is to fix the library. The other trigger is the policy growing past a fixture, as
above.

## Measurements

None yet, and none of this record's claims is a figure. **Owed at the step that builds it**
(`Plan/M1-the-fleet.md` M1.14b), on the machine that builds it:

- **That an unpackaged process can use `DatagramSocket` against a host on loopback with no exemption.**
  This is the claim everything above rests on. It is documented Windows behavior, but not yet observed in
  this tree.
- **That the bot is seated, that its token survives a rejoin, and that a command it sends is acknowledged
  by a later snapshot**, all against the real `Server`. These are the same properties M1.4's suites pin
  without a socket, observed here with one.
