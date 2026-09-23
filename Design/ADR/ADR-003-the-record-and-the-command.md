# ADR-003 — The entity record, the pinned payload, and commands made reliable by the state that acknowledges them

**Status:** Accepted — ruled 2026-09-20 following an adversarial review, **with changes**: 20 Hz rather than
10, a ten-byte record with the design identity its own byte, an explicit removal list, host-side command
validation, and figures stated for both player counts. **Amended 2026-09-21:** the payload is pinned at
1,232 rather than 1,200 — stated under *Decision* — and the command packet is filled oldest-first so its
size is bounded by construction (`TechnicalDesign.md` §5).
**Cut down 2026-09-23 by [`ADR-024`](ADR-024-replication-is-prioritized-records.md), which replaced the
full snapshot with prioritized per-entity records.** This file is edited in place rather than superseded
(README, *the MVP exception*), and its title and slug changed with it, because a file asserting a decision
it no longer takes is the defect that rule exists to prevent. **The table at the top of *Decision* says
which paragraphs are still live**; the replaced ones are kept below it, marked, because the latency and
loss arguments in them are why 20 Hz and 75 ms were chosen and those two figures did not move.
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

`AGENTS.md` R19 settles the architecture before this ADR starts: `OutpostCommander` links no `GameLogic`,
so **the client cannot simulate and lockstep is not available.** The host simulates and sends state.

What is left to decide is the shape of that state on the wire, and it has to be decided before anything is
written because the answer determines whether the host keeps per-client history, whether the client
accumulates, and whether either side needs an acknowledgment path.

The standard answer in this genre is delta encoding against an acknowledged baseline, which costs a
history ring per client, an acknowledgment channel, and a failure mode where a client whose baseline has
aged out must be recovered. The design's entity count — four players at fifty ships plus four stations,
204 records (`Design/GameDesign.md` §10) — is small enough to ask whether that is worth paying for.

## Decision

| Still decided here | Replaced by ADR-024 |
|---|---|
| The entity record's **fields and their semantics**: position as two `std::int16_t` over the play area, heading in 256 steps, hull as a percent with the one-percent floor, the design identity as its own byte, cargo in two flag bits | The record's **width and identity**: it is twelve bytes with a three-byte identity and an owner byte, not ten with team bits |
| **The payload pinned at 1,232 bytes**, and why it is not 1,200 | **The snapshot as a unit**: there is no self-contained world-at-a-tick; the datagram is an update filled by a priority accumulator |
| **20 Hz and the 75-millisecond interpolation delay**, and the latency table that chose them | **The header**: it carries the recipient's own block and no per-player list |
| **Commands made reliable by `lastCommandSeqApplied`**, applied in order, oldest-first fill | **The removal list sent once**: removals and fire events are repeated facts |
| **Host-side validation as correctness**, all five checks | **Fragmentation and the two-slot reassembler**: removed from the transport |
| **Delta encoding declined**, and the reason | **"The host serializes a per-player entity set"**: it does, and now it is a different set per tick |

**Replaced by ADR-024 — Every snapshot is self-contained.** No baselines, no acknowledgments, no per-client history, no
accumulated client state. A client that misses a packet misses one frame of animation and is fully correct
on the next one.

**Widths replaced by ADR-024; semantics live.** An entity record was **ten bytes**: identity 2, position 4 as two `std::int16_t` quantized over the play
area, heading 1, hull remaining 1, **design identity 1**, flags 1.

**ONE PERCENT IS THE FLOOR, NOT ZERO, FOR ANYTHING STILL ALIVE.** The byte is a percentage and the
rounding is to nearest, which means a `Station` on one of its eight thousand points quantizes to 0 — and
0 is the value a client draws as destroyed. An empty bar over a base that is still firing is a worse lie
than the whole percent the floor tells instead, and it is the kind that reads as a bug in the renderer.
Both ends are therefore exact by construction: zero means dead, a hundred means undamaged, and everything
between is nearest. `GameCore/EntityRecord.cpp` is where that lives and `GameCoreTests` pins it.

**The design identity gets its own byte, and that is a correction.** It was packed into the flags byte
alongside team and state, which left two bits — room for exactly four designs, forever. R24 and
[`ADR-006`](ADR-006-a-ship-is-a-composition.md) rest on a design being an *identity* that research and a
designer extend; a two-bit cap is a hardcoded limit wearing the costume of an identity, and the wire
format would have contradicted the rule at the moment the designer arrived. One byte, +1.1%.

**Replaced by ADR-024.** A snapshot header carries the protocol version, the type, the snapshot sequence, the tick, the entity
count, **the player count**, **the removal count**, and then one block per player: credits, last applied
command sequence, and the **currently building design and its progress**. **Thirty bytes at two players,
forty-six at four** — the per-player block is sized by the count in the header, so growing the match from
two players to four is a runtime value and not a format change.

**Replaced by ADR-024, which repeats each removal for ten ticks.** A removal list closes the snapshot: one count byte in the header, then two bytes per removed entity
identity. Typically zero to three per snapshot.

**The payload is pinned at 1,232 bytes, and that is the IPv6 minimum-MTU payload exactly.** Every
conformant IPv6 path must carry 1,280 bytes unfragmented; less the 40-byte IPv6 header and the 8-byte UDP
header, that is 1,232 — the largest figure that needs no path-MTU discovery, no probing and no fallback
path in the code. The conventional alternative is QUIC's 1,200, which is this same number with 32 bytes
held back for an IPv6 extension header or a tunnel, and **that reserve is what this pin declines.**

The cost is named rather than implied: a path that adds encapsulation — an IPv6-in-IPv4 tunnel, IPsec, a
corporate VPN — fragments where 1,200 would have fitted, and fragmentation is precisely the property this
ADR exists to keep. `GameDesign.md` §2 puts the game on a LAN, where none of those is present, so the pin
is defensible today and is **a decision rather than a default**. The day the game leaves a LAN this is the
first number to revisit, and the fallback is 1,200: 32 bytes, which is three entity records.

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

**Replaced by ADR-024, which removed fragmentation from the transport.** Fragments reassemble all-or-nothing, under one sequence number with an index and a count; an
incomplete set is discarded. **The reassembler holds partial sets for the two most recent sequences**, not
one: with a single slot, any cross-snapshot reorder discards a snapshot whose fragments had all arrived.
**The MVP does not fragment at all** — see below.

**Replaced by ADR-024, whose accumulator builds a different set every tick.** The host serializes a per-player entity set, not the world. The MVP has no fog of war and that set is
everything, but it is a list the host builds, so adding visibility later changes one function and not the
wire format.

**Commands are made reliable by the snapshot.** A command carries a per-player sequence number and is
repeated in every outgoing packet until the `lastCommandSeqApplied` field of an arriving snapshot reaches
it. The host applies commands in sequence order and ignores anything at or below what it has applied.
There is no general reliability layer, no second timer and no separate acknowledgment packet.

**The host validates every command, and that is correctness rather than security.**
`Design/TechnicalDesign.md` §5 declines authentication and any defense against a hostile client; that
exclusion silently covered ownership, bounds and generation checks too, which is a different category.
A 1,232-byte command packet holds **608 entity identities against a peak of 110** — a 5.5× amplification
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

**What it cost was bandwidth, stated for both configurations. Historical since ADR-024**, whose per-client cost does not depend on the entity count:

| | entities | header | snapshot | fragments | per client @20 Hz | host egress |
|---|---|---|---|---|---|---|
| **MVP — 2 players × (50 ships + station + 4 modules)** | 110 | 30 B | **1,137 B** | **one** | 22.7 KB/s | 23 KB/s (182 kbit/s) |
| Post-M2 — 4 players, same per player | 220 | 46 B | 2,253 B | two | 45.0 KB/s | 180 KB/s (1.44 Mbit/s) |

This ADR originally quoted only the four-player figure, against a configuration
`Design/GameDesign.md` §2 says the MVP cannot run: **the MVP is one human against one AI, so there is one
client.** Designing the replication budget against a load the project has no way to generate is what made
10 Hz look like a saving.

**The MVP does not fragment, and that removes its sharpest cost outright.** At 1,137 bytes a snapshot is
one datagram, so snapshot loss equals packet loss rather than roughly twice it, and a lost snapshot is a
**50-millisecond gap inside a 75-millisecond buffer** — covered without extrapolating. Two consecutive
losses are needed to show anything, which at 2% packet loss is once every two minutes. The two-fragment
path returns with the fourth player and is where the analysis above applies.

**Delta encoding stays declined, and the reason is better than the one given.** This ADR originally
declined it for costing "a history ring per client, an acknowledgment channel, and a failure mode where
a client whose baseline has aged out must be recovered". Those costs are overstated: a 32-frame ring
across four clients is 240 KB, the acknowledgment channel already exists in `lastCommandSeqApplied`, and
baseline recovery is a full snapshot, which is the thing already built. **It loses on its merits instead:
delta saves most when nothing is moving and least when everything is**, so it optimizes the idle case and
degenerates to a full snapshot plus a bitmask during the battle that is the only time the budget is under
pressure.

**What reopened it was exactly this:** the owner set a 100-player target on 2026-09-23, which is an entity count far above 204, and ADR-024 is the answer. The clause as written was: a measured entity count materially above 204 — a later milestone raising the
fleet cap, or structures, or projectiles as entities — or measured loss on a real wireless link that
neither the single fragment nor the 75-millisecond buffer rescues.

## Measurements

None yet. Four are owed, and three of them at **M0**, which exists largely to obtain them:

1. **The encoded size of a full snapshot** at the MVP's 110 entities and at 220, from the encoder rather
   than from this table — and specifically that the MVP's really is one datagram.
2. ~~**Tap-to-visible latency on real hardware**, against the 152 ms this ADR now predicts.~~ —
   **MEASURED ON LOOPBACK at M0.23, and it is not yet the figure this owes.** See below.
2. **The cost of encoding and sending four of them**, against the tick's 50 ms budget.
3. **Loss and jitter on a real wireless link between two machines**, which is what decides whether 10 Hz
   and two-fragment snapshots survive contact.


### Tap-to-visible, measured on loopback — 2026-09-22

**76 ms mean, 46 to 98, median 82, over nine taps**, every one of them with the playout clock
interpolating rather than starved. Two further taps were discarded by the instrument because the
ship was still moving from the previous order, which is what it is supposed to do with them.

| | |
|---|---|
| Device | Surface Pro, ARM64, fullscreen, 2880 × 1920 |
| Build | **Debug** |
| Host | **the same machine** — loopback |
| Input | **injected touch**, not a finger |
| World scale | 1:1 |

**THIS IS NOT THE MEASUREMENT THIS ADR OWES AND IT IS RECORDED AS A STAGE RATHER THAN AN ANSWER.**
Three of its conditions are wrong for the question. The host is on the same machine, so the network
term — the thing a wireless link actually costs — is absent entirely. It is a Debug build. And the
touch is injected at the operating system rather than pressed onto the digitizer, so the panel's own
input latency is not in it either. **M0.23 asks for a host on another machine**; that run is still
owed and this figure will move when it happens.

**What it does establish is that the chain works end to end and where the remaining terms are.**

**THE PREDICTION AND THE MEASUREMENT ARE NOT THE SAME QUANTITY, and that is the finding worth
keeping.** The 152 ms above is a *settle* time: send wait, host tick, snapshot wait, and then the
full 75 ms interpolation delay before the client is drawing the state the host reached. What the
gate measures — `Design/Plan/M0-the-wire.md` M0.23's own words — is *"the first frame in which the
drawn position differs"*, which is the **onset** of the movement and arrives much earlier.

The onset is early because of how interpolation works rather than in spite of it. A snapshot becomes
the far end of the straddling pair about **25 ms** after it arrives, not 75; from that moment the
client is blending toward it and the drawn position starts to move. So the onset is roughly one host
tick plus that, which is where 76 ms comes from, while the full 75 ms delay is still there and is
what the ship takes to *catch up*.

**Both numbers are real and they answer different questions.** A player feels the onset — the thing
that says "it heard me" — and the settle is when the picture is true. **This ADR predicted only the
second and the interface was designed against it**, so the 152 ms is not wrong; it is answering "when
is the ship where the host says" where §4's own argument about a dead interface is about "when does
anything happen at all". The second number is the better one for that argument and it did not exist
until now.

### What the same run turned up

**About 1.4% of frames fall through the playout buffer** — nine starved frames in 21 seconds of
steady running, three of them at startup before three snapshots have arrived and six scattered
through the rest. This ADR says a lost snapshot is "a 50-millisecond gap inside a 75-millisecond
buffer, covered without extrapolating"; on this evidence the margin is thinner in practice than the
arithmetic makes it look. One loopback run on one machine — it wants repeating before anybody acts
on it.

**A reconnecting client was mute, and this ADR's own claim is what hid it.** "Suspend and resume cost
nothing structurally … on resume the client reconnects and the first self-contained snapshot restores
everything" is true of **state** and was not true of **commands**: `CommandIntake` refuses a command
whose sequence is not newer than the last it applied for that player and remembers that for the whole
match, so a client that relaunched and counted from one again had every order discarded until it
caught up. On the device that was thirty-eight seconds of tapping a ship that would not move. The
client now adopts `lastCommandSequenceApplied` from the first snapshot naming it — a field that has
been on the wire since M0.9 for exactly this and that nothing read until M0.23 needed it.

**The snapshot size is now measured, and it was one byte short.** M0.9's encoder produces **1,137 bytes**
at 110 entities and **2,253** at 220; `Tests/GameCoreTests/SnapshotTests.cpp` pins both and writes them into
every CI log. This ADR's own arithmetic — 110 × 10 + 30 + three removals — omitted the **fire-event count
byte** that [`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md) places after the removal list, which is
a field this document specifies and its table then left out. Headroom against the 1,232-byte payload is
**95 bytes**, still nine entity records.

The rest remains **arithmetic on the design's own numbers**: 110 is 2 × (50 + 1 + 4), and 22.7 KB/s is
1,137 × 20. The 1,232-byte payload is the IPv6 minimum decided above rather than anything observed, and the
latency table sums four design constants — none of the four has been observed.
