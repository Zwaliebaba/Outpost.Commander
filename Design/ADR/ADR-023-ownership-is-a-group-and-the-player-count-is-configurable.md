# ADR-023 — Ownership is a group in the snapshot, and the host's player count is configurable past four

**Status:** Proposed
**Date:** 2026-09-23
**Owner:** the owner. On 2026-09-23 they chose a configurable player count larger than four for stress runs. This record's shape for it has not been ruled on.

## Context

[`ADR-022`](ADR-022-a-bot-is-a-headless-client.md)'s bot exists to stress the host with many clients from one
machine. **The host seats at most four.** `MAX_PLAYERS = 4` is compiled in `GameCore/Entity.h`, the MVP
host hard-codes two (`GameLogic/Host.cpp`), and a client past the last slot gets `MatchFull` (ADR-013).
The owner chose a configurable count larger than four over the two cheaper answers: keep four seats and
stress with unseated traffic, or stop at eight by spending the flags byte's spare bit.

**Four things are sized to four today, and only one of them is a constant:**

- **Ownership on the wire** is the flags byte's two team bits (`GameCore/EntityRecord.h`,
  `FLAGS_TEAM_MASK = 0x03`), which can name four players and no more.
- **The start layout** is four fixed anchors related by quarter turns on integers (`GameCore/Layout.h`),
  and that exactness is why the starts are fair. There is no fifth anchor.
- **The fixed-size arrays** in `CommandIntake`, `Sessions` and `BuildSystem`, all sized from
  `MAX_PLAYERS`.
- **The client's palette** has four team colors (`GameClient/Panels.cpp`).

**The obvious fix is wrong, and the budget says so.** Giving ownership a byte of its own on every record
is the simple way to name any number of players. `Scripts/DatagramBudget.py --add-bytes 1` puts the
**two-player MVP snapshot at 1,247 B against the pinned 1,232, so it fragments**. It is 1,137 B and one
datagram today. ADR-003's central property would be lost in every real match so that a test harness can
seat a fifth client. That is out.

## Decision

**Ownership is not a per-entity field. The host sends records grouped by owner, and each player's block
carries how many records are that player's.** Records are in owner order: player 1's first, then player
2's, and so on. Any records after the last player's group are unowned, which is where M2's asteroids go.
Within a group, records are in ascending identity index. That order costs nothing, and it is the order the
budget's identity-delta lever (lever 0) wants if it is ever taken. The per-player block grows from eight
bytes to ten, gaining a two-byte `entityCount`. **The two team bits leave the flags byte** and become spare.

This is lever 0 of the datagram budget: the question is whether a field needs to be per-entity at all. A
fleet of fifty repeats one owner fifty times.

| `DatagramBudget.py` | Today | This record | An owner byte instead |
|---|---|---|---|
| 2 players × 55 (`--add-header 4`) | 1,137 B, one datagram, 95 B free | **1,141 B, one datagram, 91 B free** | 1,247 B, **fragments** |
| 4 players × 55 (`--players 4 --add-header 8`) | 2,253 B, two | 2,261 B, two | — |

**The player count becomes a host argument, not a constant.** `PlayerId` stays one byte, so the count's
ceiling is 254 because zero is `NO_PLAYER`. The per-player arrays become sized at `Sessions::Begin`, and
`MAX_PLAYERS` stops being a capacity. **Four is still the game's number.** `GameDesign.md` §2's "a match is
four slots" does not move. The host refuses a count above four unless it is started in a **stress
configuration**, named on its command line, so a real match can never be configured past the design by
accident.

**Above four, the start layout does not claim to be fair.** Two and four keep the existing quarter-turn
anchors, exact and symmetric (and three keeps its admitted unfairness). Above four, player *k* of *N*
starts at binary angle `k × 65536 / N` on the same anchor radius, placed through `NeuronCore`'s sine table
and facing the center. That is integer arithmetic on a pinned table, so it satisfies R16 and both sides
compute it identically (R23). It is not exactly symmetric, and the stress configuration exists to load the
host, not to be played.

**A client names a player past the palette with the last team color.** The packaged client can join a
stress run and draw it. It will not draw it well, and it doesn't have to.

**Until fragmentation exists, a stress run is limited to what fits in one datagram.** Reassembly is M4.2.
Until then the host's encoder refuses a snapshot past 1,232 B, so a run must choose its size:

| `DatagramBudget.py` with this record | Snapshot | Datagrams |
|---|---|---|
| 8 players × (5 ships + station + 4 modules) | 901 B | one |
| 8 × (20 + 5) | 2,101 B | two |
| 16 × (5 + 5) | 1,781 B | two |
| 16 × (50 + 5) | 8,981 B | eight |

**When M4.2 lands it reassembles as many fragments as the header can count (255), not two.** ADR-003
scoped reassembly to the fourth player's two datagrams. A stress run is exactly the case that sends more,
and the host's per-client egress at sixteen full fleets is 179.6 KB/s.

**The binding ceiling is the entity index, not the player count.** Ten bits of index is 1,024 live
entities (`WIRE_INDEX_BITS`). At fifty-five a player that is eighteen full fleets. A stress run past it is
a run the host must refuse at `Begin`, not one that wraps identities.

## Consequences

**The format changes before the first build is handed to anyone, and that deadline is the reason to do it
now and not at M4.** The budget skill says that freedom to reshape the record expires at the first build a
second person runs. After that, this is a coordinated release of two executables. The stress half, meaning
the host argument, the layout and the arrays, could wait. The format half should not.

**What it costs:**

- **Four bytes at two players**, which is 4 of the 95 B of headroom.
- **An ordering constraint on the encoder.** Records must be emitted grouped and sorted, and the decoder
  must reject a snapshot whose group counts don't sum to at most the entity count. That's a new `SnapshotFault`.
- **Ownership is no longer on the record.** `GameClient/HitTest.cpp`'s one line that reads it from
  `flags` becomes a lookup the replica store fills while decoding.
- **A second code path in `GameCore/Layout.h`** that only stress runs exercise. That is the usual fate of
  test-only branches, so it gets a test.

**What it forecloses:** a record that is meaningful without its snapshot. A record no longer says who owns
it, so any future split of a snapshot must keep a group intact within one datagram, or carry the group
counts in each fragment. Rate separation (budget lever 5) already had to keep identities together, and
this adds ownership to the same side.

**What it reopens:** two spare bits in the flags byte, giving three counting the existing one. Nothing
takes them here.

**What would reopen it:** fog of war (lever 6). Once each client receives a different entity set, the
groups are per recipient, which the host builds per player anyway. The other trigger is a design that
lets ownership change mid-match, since this record, like ADR-003, assumes ownership is set once at
creation.

## Measurements

Every figure above is `Scripts/DatagramBudget.py` on the design's arithmetic, not a measurement. It was run
on 2026-09-23 with `--add-header 2×N` to model the ten-byte block, and the rows give the flags. **Owed at
the step that builds it** (`Plan/M1-the-fleet.md` M1.14c):

- The encoder's own `EncodedSize` at two and at four players, replacing the arithmetic above, as M0.9 did
  for ADR-003.
- `DatagramBudget.py`'s `HEADER_PER_PLAYER` gains the field and its audit gains the two freed bits, so the
  script models this record and stops needing `--add-header`.
