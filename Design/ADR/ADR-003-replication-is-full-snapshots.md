# ADR-003 — Replication is full snapshots, and commands are made reliable by them

**Status:** Accepted — ruled 2026-09-20 following an adversarial review, **with changes**: 20 Hz rather than
10, a ten-byte record with the design identity its own byte, an explicit removal list, host-side command
validation, and figures stated for both player counts.
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

`AGENTS.md` R19 settles the architecture before this ADR starts: `OutpostCommander` links no `GameLogic`,
so **the client cannot simulate and lockstep is not available.** The host simulates and sends state.

What is left to decide is the shape of that state on the wire, and it has to be decided before anything is
written because the answer determines whether the host keeps per-client history, whether the client
accumulates, and whether either side needs an acknowledgement path.

The standard answer in this genre is delta encoding against an acknowledged baseline, which costs a
history ring per client, an acknowledgement channel, and a failure mode where a client whose baseline has
aged out must be recovered. The design's entity count — four players at fifty ships plus four stations,
204 records (`Design/GameDesign.md` §10) — is small enough to ask whether that is worth paying for.

## Decision

**Every snapshot is self-contained.** No baselines, no acknowledgements, no per-client history, no
accumulated client state. A client that misses a packet misses one frame of animation and is fully correct
on the next one.

An entity record is **ten bytes**: identity 2, position 4 as two `std::int16_t` quantized over the play
area, heading 1, hull remaining 1, **design identity 1**, flags 1.

**The design identity gets its own byte, and that is a correction.** It was packed into the flags byte
alongside team and state, which left two bits — room for exactly four designs, forever. R24 and
[`ADR-006`](ADR-006-a-ship-is-a-composition.md) rest on a design being an *identity* that research and a
designer extend; a two-bit cap is a hardcoded limit wearing the costume of an identity, and the wire
format would have contradicted the rule at the moment the designer arrived. One byte, +1.1%.

A snapshot header carries the protocol version, the type, the snapshot sequence, the tick, the entity
count, **the player count**, **the removal count**, and then one block per player: credits, last applied
command sequence, and the **currently building design and its progress**. **Thirty bytes at two players,
forty-six at four** — the per-player block is sized by the count in the header, so growing the match from
two players to four is a runtime value and not a format change.

**A removal list closes the snapshot.** One count byte in the header, then two bytes per removed entity
identity. Typically zero to three per snapshot.

**This exists because absence is ambiguous and the original design leaned on it.** A dead ship simply
vanished from the entity list, so the client learned of death by *inference*. That works only while the
interest set is everything — and this ADR claims that a per-player set makes fog of war additive. **It is
additive for the protocol and breaking for the client**: the day visibility ships, absence means "died" or
"left my view", and every wreck ([`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md)), every
selection eviction and every death effect fires wrongly. The removal list is what makes the additive claim
true rather than merely stated.

**Snapshots go out at 20 Hz**, every tick.

**This was 10 Hz, and the change is about latency rather than loss.** This ADR originally priced
bandwidth, fragment loss and a 200-millisecond gap, and never computed the number that decides how the
game feels:

| tap → visible response | send wait | host tick | snapshot wait | interpolation | **total** |
|---|---|---|---|---|---|
| at 10 Hz, 150 ms buffer — average | 25 | 25 | 50 | 150 | **252 ms** |
| at 10 Hz, 150 ms buffer — worst | 50 | 50 | 100 | 150 | **352 ms** |
| **at 20 Hz, 75 ms buffer — average** | 25 | 25 | 25 | 75 | **152 ms** |
| **at 20 Hz, 75 ms buffer — worst** | 50 | 50 | 50 | 75 | **227 ms** |

**On a touchscreen there is no cursor, no hover and no click feedback: the tap is the only signal the
player gets that anything happened.** A quarter of a second of nothing is a dead interface. 20 Hz and a
75-millisecond interpolation delay — one snapshot interval plus a jitter margin — buy a hundred
milliseconds of that back, and what they cost is bandwidth the MVP has in abundance.

**Fragments reassemble all-or-nothing**, under one sequence number with an index and a count; an
incomplete set is discarded. **The reassembler holds partial sets for the two most recent sequences**, not
one: with a single slot, any cross-snapshot reorder discards a snapshot whose fragments had all arrived.
**The MVP does not fragment at all** — see below.

**The host serialises a per-player entity set, not the world.** The MVP has no fog of war and that set is
everything, but it is a list the host builds, so adding visibility later changes one function and not the
wire format.

**Commands are made reliable by the snapshot.** A command carries a per-player sequence number and is
repeated in every outgoing packet until the `lastCommandSeqApplied` field of an arriving snapshot reaches
it. The host applies commands in sequence order and ignores anything at or below what it has applied.
There is no general reliability layer, no second timer and no separate acknowledgement packet.

**The host validates every command, and that is correctness rather than security.**
`Design/TechnicalDesign.md` §5 declines authentication and any defence against a hostile client; that
exclusion silently covered ownership, bounds and generation checks too, which is a different category.
A 1,200-byte command packet holds **592 entity identities against a peak of 102** — a 5.8× amplification
into a single-threaded host loop, reachable from an ordinary bug or a reordered packet with no attacker
anywhere. The host therefore rejects entities the sender does not own, bounds the selection at the
sender's own entity count, rejects stale generations, clamps target points to the play area, and handles
`uint16` sequence wraparound explicitly rather than letting the "at or below" comparison invert.

## Consequences

**What this buys is the elimination of an entire class of bug.** There is no divergence of simulation
state, because there is no accumulated simulation state on the client; there is no baseline to expire and
no recovery path to get wrong. This is the largest single simplification in the MVP and it is why the MVP
is reachable.

**The claim is narrower than it was stated, and the narrowing matters.** The client *does* accumulate —
the selection set, order markers in flight, and wrecks — and all three are derived from snapshots by
inference. "Nothing can diverge" is true of positions and was never true of the client as a whole. The
removal list above is what makes the derived state correct; without it the sentence was doing more work
than it could carry.

**What it costs is bandwidth, stated for both configurations rather than only the larger one.**

| | entities | header | snapshot | fragments | per client @20 Hz | host egress |
|---|---|---|---|---|---|---|
| **MVP — 2 players × 50 ships** | 102 | 30 B | **1,056 B** | **one** | 21.1 KB/s | 21 KB/s (169 kbit/s) |
| Post-M2 — 4 players × 50 ships | 204 | 46 B | 2,092 B | two | 41.8 KB/s | 167 KB/s (1.34 Mbit/s) |

This ADR originally quoted only the four-player figure, against a configuration
`Design/GameDesign.md` §2 says the MVP cannot run: **the MVP is one human against one AI, so there is one
client.** Designing the replication budget against a load the project has no way to generate is what made
10 Hz look like a saving.

**The MVP does not fragment, and that removes its sharpest cost outright.** At 1,056 bytes a snapshot is
one datagram, so snapshot loss equals packet loss rather than roughly twice it, and a lost snapshot is a
**50-millisecond gap inside a 75-millisecond buffer** — covered without extrapolating. Two consecutive
losses are needed to show anything, which at 2% packet loss is once every two minutes. The two-fragment
path returns with the fourth player and is where the analysis above applies.

**Delta encoding stays declined, and the reason is better than the one given.** This ADR originally
declined it for costing "a history ring per client, an acknowledgement channel, and a failure mode where
a client whose baseline has aged out must be recovered". Those costs are overstated: a 32-frame ring
across four clients is 240 KB, the acknowledgement channel already exists in `lastCommandSeqApplied`, and
baseline recovery is a full snapshot, which is the thing already built. **It loses on its merits instead:
delta saves most when nothing is moving and least when everything is**, so it optimises the idle case and
degenerates to a full snapshot plus a bitmask during the battle that is the only time the budget is under
pressure.

**What would reopen it:** a measured entity count materially above 204 — a later milestone raising the
fleet cap, or structures, or projectiles as entities — or measured loss on a real wireless link that
neither the single fragment nor the 75-millisecond buffer rescues.

## Measurements

None yet. Four are owed, and three of them at **M0**, which exists largely to obtain them:

1. **The encoded size of a full snapshot** at the MVP's 102 entities and at 204, from the encoder rather
   than from this table — and specifically that the MVP's really is one datagram.
2. **Tap-to-visible latency on real hardware**, against the 152 ms this ADR now predicts. Owed at M0: it
   is the number that decides how the game feels, and every other decision here is cheap beside it.
2. **The cost of encoding and sending four of them**, against the tick's 50 ms budget.
3. **Loss and jitter on a real wireless link between two machines**, which is what decides whether 10 Hz
   and two-fragment snapshots survive contact.

Every figure above is **arithmetic on the design's own numbers**, not a measurement: 102 is 2 × 50 + 2;
1,056 is 102 × 10 + 30 + three removals; 21.1 KB/s is 1,056 × 20. The ten-byte record is a proposed
layout, the 1,200-byte payload is the conventional safe figure for UDP over Ethernet, and the latency
table sums four design constants — none of the four has been observed.
