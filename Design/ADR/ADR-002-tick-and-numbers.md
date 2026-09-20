# ADR-002 — The tick, and the numbers the simulation is made of

**Status:** Proposed
**Date:** 2026-09-20
**Owner:** proposed by the design; not yet ruled

## Context

`AGENTS.md` R16 requires the simulation to be deterministic and states the half of it the build enforces:
`/fp:precise` everywhere, stated rather than inherited, identical in Debug and Release. It also states
what the code must do — integers and fixed point, no wall clock, a pinned PRNG — and leaves every actual
number to be decided.

Those numbers have to exist before the first line of movement code, because every one of them is in a
type that appears in the entity, in the wire format and in every test.

`/arch:AVX2` on x64 makes this load-bearing rather than tidy. It lets MSVC contract `a*b+c` into an FMA
even under `/fp:precise`, and contract differently at different optimisation levels. A `float` in the
simulation is therefore a Debug and a Release that disagree — and, since ARM64 sets no such switch, an x64
and an ARM64 that disagree, on two platforms CI builds every push.

## Decision

**Twenty ticks a second.** Fifty milliseconds. The tick is the clock; wall time converts to ticks at one
seam and the two meet nowhere else.

**Position and velocity are `std::int32_t` with 8 fractional bits** — one unit is 1/256 of a world unit,
about four millimetres at the design's nominal metre. The 16,384-unit play area spans ±2,097,152, leaving
three orders of magnitude of headroom. Multiplication is `(std::int64_t(a) * b) >> 8`; the 64-bit
intermediate is not optional.

**Angles are `std::uint16_t` binary angles**, 65,536 to a turn, so addition wraps without a modulus and
the difference of two headings is a subtraction rather than a special case. **Sine is a 4,096-entry table
of `std::int16_t` in Q1.15**, indexed by `angle >> 4`, with no interpolation: eight kilobytes, identical
on every platform, 0.088° of resolution.

**Distances are compared squared, in `std::int64_t`.** An integer square root exists for the rare case
that needs a magnitude. The renderer is not the simulation and uses floats freely.

**One PRNG, written out in `NeuronCore`, seeded from the match.** Never `std::random_device`, never a hash
of an address, never a second generator somewhere convenient. The standard library's *distributions* are
not portable between implementations and none is used.

**Candidate order is a correctness property.** Entities live in a `std::vector` with a free list and the
tick iterates it in index order. The uniform grid is a candidate structure only: **every query whose result
reaches an outcome sorts its candidates by entity identity first**, and every tie — two targets at equal
distance — breaks on identity, never on which cell was visited first.

## Consequences

Twenty hertz is a movement update every 50 ms and a fighter moving seven position units a tick. The client
interpolates between snapshots and never sees it (`Design/TechnicalDesign.md` §6). Anything needing finer
temporal resolution than 50 ms — a weapon's exact impact moment, say — does not get it, and
[`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md) is downstream of that.

Fixed point costs care at every multiplication and a class of overflow bug that floats do not have. It
buys a simulation that is bit-identical across Debug, Release, x64 and ARM64, which is what makes the
determinism test in `GameLogicTests` worth anything.

The sorting requirement costs real time in the hot path of target selection. It is not optional: an
unordered tie-break is a desynchronisation that appears once an hour and cannot be reproduced.

**What would reopen it:** a measured tick cost that does not fit 50 ms at the design's entity count, which
argues for 10 Hz rather than for floats; or a play area that outgrows `int32` at 1/256, which it is nowhere
near.

## Measurements

None yet, and two are owed before this moves from Proposed to Accepted:

1. **The cost of an empty tick and of a full one** at 204 entities on the host, against the 50 ms budget.
2. **The determinism test passing across all four configuration and platform pairs** — the same seed and
   the same scripted orders producing the same state hash on x64 and ARM64, Debug and Release. Until that
   runs, every claim in this ADR about bit-identical behaviour is an argument rather than a fact.

The figures in the Decision are definitions and arithmetic on them, not measurements: ±2,097,152 is
16,384 × 256 ÷ 2, 0.088° is 360 ÷ 4,096, and eight kilobytes is 4,096 × 2.
