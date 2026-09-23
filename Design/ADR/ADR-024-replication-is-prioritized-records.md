# ADR-024 — Replication is prioritized absolute-state records, one datagram at a time

**Status:** Accepted — ruled 2026-09-23 by the owner on the architecture as proposed, with no backward
compatibility owed to the format it replaces. **The figures are the budget script's arithmetic**; the
measurements this record owes are listed at the end and none is taken yet.
**Date:** 2026-09-23
**Owner:** Stefan Zwaal

## Context

[`ADR-003`](ADR-003-the-record-and-the-command.md) replicated the world as a full, self-contained snapshot
in one datagram: no delta, no acknowledgment, no ordering, and a lost packet costs one frame. That is the
right property and the wrong unit. Its unit of delivery is *the world at tick T*, which fits one datagram
only while the world is small: 110 entities in the MVP, with 95 bytes to spare. The owner set a target
this cannot meet — **a replication design that keeps every datagram whole, needs no resend and no ordering,
and supports 100 players** — and asked that the design be challenged rather than stretched.

The arithmetic ends the stretch. 100 players at the design's fleet is 5,500 entities, which at the old
ten-byte record is 55 KB a snapshot: 45 datagrams per client per tick, 110 MB/s of host egress, and a
36% chance of losing every snapshot at 1% packet loss once the fragments must all arrive. Fragmentation
(M4.2), and [`ADR-023`](ADR-023-the-player-count-is-configurable.md)'s first draft, which grouped records by
owner to fit a fifth player into the snapshot, were both answers to the four-to-eight-player question.
Neither reaches a hundred, and the second one was dropped in this record's favor the day it was proposed.

Three other things in the old design fail at scale, and each is a decision here:

- **The header carried one block per player.** At 100 players that is 800 of the 1,232 bytes before a
  single entity.
- **A death was sent once**, in a removal list. That held only because every snapshot repeated the world.
- **The identity index was ten bits.** 1,024 live entities, against 5,500.

## Decision

**The unit of replication is the entity record, not the snapshot.** A record is a self-contained fact:
*this entity, at this tick, was here, facing this way, at this hull, of this design, owned by this
player.* Nothing in a record depends on any earlier record. A datagram is a bag of such facts at one tick,
and any subset of the bag is meaningful on its own. That is the whole of what makes the properties below
hold, and it is the property [`ADR-003`](ADR-003-the-record-and-the-command.md) already had in its record;
this record keeps the record and drops the snapshot around it.

**The record is twelve bytes:** identity 3 (a 16-bit index and an 8-bit generation), owner 1, position 4
as two `std::int16_t` over the play area, heading 1, hull 1, design identity 1, flags 1 (state 3, cargo 2,
three spare). The identity grew because a match can hold thousands of entities and the generation has to
survive index reuse under loss; the owner is a byte because a record must say whose it is without the
datagram being grouped or complete. Everything else is ADR-003's, including the hull's one-percent floor.

**The datagram is an update, and its header does not scale with the player count.** Transport 4 (version,
type, a sequence the receiver uses only to count loss); tick 4; live entity count 2; **the recipient's own
player block** — credits 4, last command applied 2, building design 1, progress 1 — and nothing about
anyone else's; record count 1; removal count 1; fire count 1. **Twenty-one bytes** at any player count.
What the client draws about other players is on their entities: a rival's station has a hull on its record.

**The host fills one update per client per tick from a priority accumulator.** Each client has a score
per live entity. Every tick the score grows by that entity's relevance to this client, and sending the
entity resets it to zero. The host takes the highest scores that fit — **99 records** at the pinned payload
with three removals and two fire events riding along — and sends them. Relevance is an integer sum of:
a base of one, so every entity accumulates; a term for being inside the client's view; a term for having
moved or changed since it was last sent to this client; and a term for being the client's own. The
weights are constants in `GameLogic` beside the accumulator, with the reason written beside each, and
they are [`OpenQuestions.md`](../OpenQuestions.md) Q49's to tune by playing.

**The sweep is a guarantee, not a hope.** Any entity unsent to a client for **⌈live entities ÷ records per
tick⌉ ticks** — the sweep — goes into the next update ahead of every score. So relevance decides *how often*
an entity is refreshed, and the sweep bounds *how long* it can go without being. The client knows the
sweep because the header carries the live entity count, and **it forgets an entity that has gone three
sweeps without a record.** That is the only way an entity leaves a client without a removal, and it is what
makes rejoining correct: a client that was away misses the removals of everything that died meanwhile,
and those entities are simply never refreshed again.

**A client may receive more than one update a tick, each of them whole.** When more records are due than
one update holds, the host sends a second, up to a per-client cap of **two**. Each update is complete and
separately renderable — the same test that separates rate separation from fragmentation in the budget
skill — so this is not fragmentation: losing one loses those records' refresh, not the frame. At the MVP's
110 entities a second update is what keeps every entity refreshed every tick, at 49.3 KB/s per client.
At 100 players the cap is what bounds bandwidth, and the accumulator ranks what fits.

**Ordering is a per-entity tick comparison.** Every update carries the tick it describes. The client keeps,
per entity, the tick of the newest sample it holds, and drops any record whose tick is not newer. There is
no sequence window, no reassembly and no reorder buffer. The interpolation clock of
`TechnicalDesign.md` §6 is unchanged, except that the two samples it interpolates between are per
entity rather than per snapshot; an entity whose newest sample is older than the render time holds
its last position and never extrapolates (R19).

**Removals and fire events are repeated facts.** A removal — the three-byte identity, generation included
— rides every update to a client for **ten consecutive ticks** after the death; the client ignores one it
has already applied. A fire event ([`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md), now shooter 3,
target 3, weapon 1) rides for **three**. Losing a death now needs ten consecutive losses, which at 5%
packet loss is once in 10¹³ updates; losing a tracer needs three.

**The client tells the host what it is looking at.** The command packet's header gains the view center
(4 B) and view radius (2 B), so it is twelve bytes. It is sent as often as it was, it is unreliable as it
was, and a lost one leaves the host scoring against the last view it saw. A view is not state and reveals
nothing; when fog of war arrives, relevance switches to distance from owned entities and this field stays.

**Fragmentation is gone from the transport, not deferred.** The two fragment fields M0.2 reserved in the
packet header are removed, and with them M4.2's reassembler before it was written. **Delta encoding stays
declined** on the ground ADR-003 gave: it saves least when everything moves.

**Updates go out at 20 Hz** and the client renders **75 milliseconds** behind, unchanged. The update
rate and the tick rate remain the same number.

## Consequences

**What it buys is the property the owner asked for, at any player count.** Every datagram is whole and
meaningful alone; nothing is resent, acknowledged or reordered; a lost update delays some entities'
refresh by a tick and breaks nothing; and every client converges on the same absolute state, because no
client ever computes any. Per-client bandwidth is **24.6 KB/s per update per tick** (197 kbit/s) whatever
the entity count, and host egress is that times the client count: **2.46 MB/s at 100 clients**, or 19.7
Mbit/s. The host's accumulator is 100 clients × 5,500 scores a tick, updated and partially sorted — on the
order of 10⁷ integer operations a second, which is nothing beside the simulation.

**What a client sees is a refresh rate, and it degrades with what is on screen rather than failing.** At
the MVP, everything every tick. With 220 entities in view, every 3 ticks at one update or 2 at the cap.
With 1,000 in view — a 100-player match at tactical zoom, drawing 3.5-pixel silhouettes — each refreshes
every 11 ticks, 550 ms, and holds between; at that size the hold is not visible. **5,500 in view is the
true ceiling of the cap**: each refreshes every 2.8 seconds, the nearest and moving ones far more often,
and that is visible. A match that wants a whole 100-player war on one screen at once needs a higher cap
and the bandwidth that goes with it. The budget script states all of these with `--in-view`.

**What it costs:**

- **The MVP pays two updates a tick where one snapshot did**, 49.3 KB/s against 22.7. Bandwidth the MVP
  has in abundance, and the honest price of a twelve-byte record with a three-byte identity.
- **Resume is a sweep, not a snapshot.** ADR-003 restored a resumed client with one datagram. Now the
  host resets that client's scores so everything is sent within one sweep — two ticks at the MVP, 56 at
  5,500 entities, 2.8 seconds — and the client clears its store on rejoin so nothing stale survives. The
  reconnecting overlay (`Interface.md` §7) stays up until the first update after the rejoin, as it does
  now.
- **Per-client host state.** A score per entity per client, and a last-sent tick per entity per client.
  At 100 × 5,500 that is a few megabytes, and it is *not* simulation state: it is never hashed, never
  replayed and never reaches the tick's outcome. `Scripts/CheckDeterminism.py` sweeps `GameLogic` and
  will see integer code either way; the `determinism-audit` skill's judgment is that this is the seam
  where per-client state may live because nothing downstream of it is simulation.
- **The interpolation store is per entity.** `ReplicaStore` stops holding snapshots and holds two samples
  per entity, which is what a bag of facts needs and what a snapshot ring cannot give it.
- **A fire event costs seven bytes, not five**, and a removal three, not two. Both ride along for several
  ticks. At the defaults that is 23 bytes of the update, and it is in the script.
- **A three-byte identity upstream too.** A command at the peak 110-identity selection is 338 B, so
  three fit in a packet where five did. `TechnicalDesign.md` §4's oldest-first fill already bounds this.

**What it forecloses:** anything that needs the world at one tick as an object — a state hash on the
client, a replay built from datagrams, a "snapshot" a test can compare whole. None of those exists and
R19 forbids the first. The determinism test hashes the host's `World` and is untouched.

**What is unchanged, and worth saying:** the simulation. Nothing in `GameLogic`'s tick knows that
replication is per client, and the state hash of M1.7 must come out the same after this change as before
it. That is the strongest test of whether the seam was cut in the right place.

**What would reopen it:** a measured refresh at the tactical zoom that a player notices at the MVP's
entity count with the cap at two; a host whose accumulator cost shows in the tick at 100 clients, which
is the one figure above that is an estimate of CPU rather than of bytes; or a fog-of-war design whose
interest set is so small that the sweep guarantee becomes the wrong bound.

## Measurements

All figures above are `Scripts/DatagramBudget.py`, run 2026-09-23, on the design's arithmetic; the flags
are `--players`, `--ships`, `--in-view`, `--datagrams` and `--upstream`. **Owed at
`Plan/M1-the-fleet.md` M1.14c**, on the machine that builds it:

1. **The encoder's own `EncodedSize` of a full update**, replacing the 1,232 and 99 above with what
   `GameCoreTests` measures, as M0.9 did for the snapshot.
2. **That M1.7's determinism hash is unchanged**, `0x37f846ed90b74ca1` on all four pairs, which is the
   proof that nothing simulated moved.
3. **Refresh interval per entity at the MVP with the cap at two**, observed from the client's store over
   a two-minute match: every entity every tick is the prediction.
4. **The accumulator's cost per client per tick** at 110 and, with ADR-022's harness, at every player
   count the host will seat, against the 50 ms tick.
5. **Tap-to-visible on loopback**, re-run against ADR-003's measured 76 ms, because the path from a
   command to the update that shows it has changed shape and the figure must not have.
