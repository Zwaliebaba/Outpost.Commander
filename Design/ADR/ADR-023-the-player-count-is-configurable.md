# ADR-023 — The host's player count is configurable past four, for stress runs

**Status:** Accepted — ruled 2026-09-23 by the owner. **Cut down the same day:** this record first
also reshaped the snapshot to group records by owner, so that a fifth player fit the wire. That half was
made pointless by [`ADR-024`](ADR-024-replication-is-prioritized-records.md), which put the owner on
every record and removed the snapshot the grouping was for. What is left is the half about the host.
**Date:** 2026-09-23
**Owner:** Stefan Zwaal

## Context

[`ADR-022`](ADR-022-a-bot-is-a-headless-client.md)'s harness exists to load the host with many clients
from one machine, and [`ADR-024`](ADR-024-replication-is-prioritized-records.md) exists so the wire can
carry a hundred. **The host itself still seats at most four.** `MAX_PLAYERS = 4` is compiled in
`GameCore/Entity.h`, the MVP host hard-codes two (`GameLogic/Host.cpp`), and a client past the last slot
gets `MatchFull` (ADR-013). Four things are sized to four, and only one of them is a constant:

- **The fixed-size arrays** in `CommandIntake`, `Sessions` and `BuildSystem`, all sized from `MAX_PLAYERS`.
- **The start layout**: four fixed anchors related by quarter turns on integers (`GameCore/Layout.h`),
  and that exactness is why the starts are fair. There is no fifth anchor.
- **The client's palette** has four team colors (`GameClient/Panels.cpp`).
- **Ownership on the wire** was two team bits. ADR-024 made it a byte, so this one is answered there.

## Decision

**The player count is a host argument, not a constant.** `PlayerId` stays one byte, so the ceiling is
254, because zero is `NO_PLAYER`. **`MAX_PLAYERS` becomes that capacity, 254, and the game's four becomes
`MATCH_PLAYERS`** (`GameCore/Entity.h`), where one constant had been doing both jobs. The live entity count
is bounded separately, by ADR-024's 16-bit index: the store has 65,536 slots and refuses a creation past
them rather than wrapping an identity.

**Amended while building (M1.14c): the per-player tables are sized to the capacity, not at `Begin`.**
This record first had them sized when the match starts. The command intake and the build system keep an
array entry per possible player instead -- 255 entries, a few kilobytes -- because an array needs no count
threaded into a constructor, no "was `Begin` called" to assert, and cannot be indexed past by any
`PlayerId` there is. What a match's count decides is how many slots `Sessions` hands out and how many
stations the layout places; nothing else reads it.

**Four is still the game's number.** `GameDesign.md` §2's "a match is four slots" does not move. The host
refuses a count above four unless it is started in a **stress configuration**, named on its command line,
so a real match can never be configured past the design by accident: `Server --players N --stress`, checked
by `PlayerCountAllowed` in `GameLogic/Host.h`, which a suite pins.

**Above four, the start layout does not claim to be fair.** Two and four keep the quarter-turn anchors,
exact and symmetric, and three keeps its admitted unfairness. Above four, player *k* of *N* starts at
binary angle `k × 65536 / N` on the anchor radius, placed through `NeuronCore`'s sine table and facing
the center. Integer arithmetic on a pinned table satisfies R16, and both sides compute it identically
(R23). It is not exactly symmetric, and the stress configuration exists to load the host, not to be
played.

**A client names a player past the palette with the last team color**, in the HUD and on the hulls. The
packaged client can join a stress run and draw it. It will not draw it well, and it doesn't have to.

**One limitation is known and left.** The packaged client recenters its camera on `StartAnchor(2, player)`
when it is seated, because the join does not tell it the player count. In a stress run its camera opens in
the wrong place, and a pan fixes it. The harness has no camera, so this costs a stress run nothing; putting
the count in the join reply is the fix if a person ever plays one.

## Consequences

**What it costs:** a second code path in `GameCore/Layout.h` that only stress runs exercise, which is the
usual fate of test-only branches, so it gets a test; and arrays sized at run time where they were
`std::array`, which is a `std::vector` and an assertion that `Begin` came first.

**What it forecloses:** nothing about the game. A stress run is not a match, and the switch that permits
it is the line between the two.

**What would reopen it:** a design that wants more than four *real* players, at which point the fair
layout above four is a design question (`GameDesign.md` §3) and not this record's.

## Measurements

None of this record's claims is a figure. **Owed at `Plan/M1-the-fleet.md` M1.14c:** that eight harness
players (ADR-022) seat on one host started with `--players 8 --stress`, and that a two-player match's
layout is unchanged bit for bit against M1.5's pinned output.
