# ADR-003 — Replication is full snapshots, and commands are made reliable by them

**Status:** Proposed
**Date:** 2026-09-20
**Owner:** proposed by the design; not yet ruled

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

An entity record is **nine bytes**: identity 2, position 4 as two `std::int16_t` quantized over the play
area, heading 1, hull remaining 1, flags 1. A snapshot header carries the protocol version, the snapshot
sequence, the tick, the entity count, each player's credits and **each player's last applied command
sequence**, in about 36 bytes.

**Snapshots go out at 10 Hz**, every second tick.

**Fragments reassemble all-or-nothing.** A snapshot larger than the 1,200-byte safe payload is split under
one sequence number with an index and a count; an incomplete set is discarded rather than rendered.

**The host serialises a per-player entity set, not the world.** The MVP has no fog of war and that set is
everything, but it is a list the host builds, so adding visibility later changes one function and not the
wire format. The client never assumes the set is complete.

**Commands are made reliable by the snapshot.** A command carries a per-player sequence number and is
repeated in every outgoing packet until the `lastCommandSeqApplied` field of an arriving snapshot reaches
it. The host applies commands in sequence order and ignores anything at or below what it has applied.
There is no general reliability layer, no second timer and no separate acknowledgement packet.

## Consequences

**What this buys is the elimination of an entire class of bug.** There is no divergence, because there is
no accumulated state to diverge; there is no baseline to expire; there is no recovery path to get wrong.
This is the largest single simplification in the MVP and it is why the MVP is reachable.

**What it costs is bandwidth, and the cost is stated rather than waved at.** 204 records at nine bytes
plus the header is **1,872 bytes a snapshot**, which is two fragments, **18.7 KB/s to each client and
75 KB/s — 600 kbit/s — out of the host**. Delta encoding would cut that by most of itself, because an
asteroid never moves and an idle ship changes nothing.

**The sharper cost is fragment loss.** With two fragments and all-or-nothing reassembly, a snapshot is
lost at about twice the packet loss rate, and a lost snapshot at 10 Hz is a **200-millisecond gap** the
interpolator must cover. On a LAN — the MVP's target — that is nothing. On a congested wireless link it is
the first thing that will look wrong.

**The two levers, in order:** raise the rate to 20 Hz, which halves the gap and doubles the bandwidth and
is one constant; then add delta encoding, which is where the complexity declined here is waiting.

**What would reopen it:** a measured entity count materially above 204 — a later milestone raising the
fleet cap, or structures, or projectiles as entities — or measured loss on a real wireless link that 20 Hz
does not rescue.

## Measurements

None yet. Three are owed before this moves from Proposed to Accepted, and the third is owed at **M0**,
which exists largely to obtain it:

1. **The encoded size of a full snapshot at 204 entities**, from the encoder rather than from this table.
2. **The cost of encoding and sending four of them**, against the tick's 50 ms budget.
3. **Loss and jitter on a real wireless link between two machines**, which is what decides whether 10 Hz
   and two-fragment snapshots survive contact.

Every figure above is **arithmetic on the design's own numbers**, not a measurement: 204 is 4 × 50 + 4;
1,872 is 204 × 9 + 36; 18.7 KB/s is 1,872 × 10; 600 kbit/s is that times four clients times eight. The
nine-byte record is a proposed layout and the 1,200-byte payload is the conventional safe figure for UDP
over Ethernet, not something measured here.
