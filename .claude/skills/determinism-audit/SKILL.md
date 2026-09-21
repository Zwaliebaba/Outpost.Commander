---
name: determinism-audit
description: Audit simulation code in Outpost Commander against R16, so a match reproduces from its seed on every machine and platform. Use this skill whenever code is written, reviewed or changed in GameCore or GameLogic — a tick system, movement, combat resolution, the mining loop, build queues, the procedural generator, target selection, any sort or comparator, anything drawing from the PRNG, and any fixed-point arithmetic. Use it proactively on a design or plan change that adds simulation state even if nobody mentions determinism, and whenever someone asks "why did the two clients diverge", "why will this replay not reproduce", "is this safe to sort", "can I use a float here" or "does this need to be fixed point". Running `Scripts/CheckDeterminism.py` is a precondition of touching the simulation, not advice: R16 is a rule no build can check and no reviewer reliably sees, the sweep costs a second, and a violation surfaces months later as an x64-against-ARM64 desync that cannot be debugged from a report of what happened.
---

# The determinism audit

R16 exists because **a simulation that cannot reproduce from its seed cannot be replayed, cannot be
debugged from a report of what happened, and cannot be measured twice.** Everything below serves that
one property.

What makes it worth a skill rather than a paragraph is the *shape of the failure*. A float in
`GameCore` compiles. It passes every test. It works perfectly on the machine that wrote it. It surfaces
as two clients disagreeing about where a fleet is, on someone else's hardware, weeks later — and by then
the code that caused it is load-bearing. **CI builds `Debug|x64` only** (`AGENTS.md` §6), so nothing
automated compares two platforms, and **ARM64 is the platform the game is actually for**.

## Run the sweep. Every time, before anything else

```bash
python3 Scripts/CheckDeterminism.py              # GameCore and GameLogic
python3 Scripts/CheckDeterminism.py --review     # + the judgement calls
python3 Scripts/CheckDeterminism.py --also NeuronCore   # the PRNG lives there
```

It strips comments and string literals before matching, so its output is signal rather than a wall to
skim past. A violation is a violation: floats, unordered containers, wall-clock reads, `rand`,
`random_device`, `default_random_engine`, CRT transcendentals, DirectXMath. `--review` adds the
constructs that are *legal and load-bearing* — sort comparators, distributions, pointer-keyed
containers, mutable statics — each of which needs an answer rather than a fix.

**A clean sweep is half the audit and the cheaper half.** The rest is below, and it is the half that
actually bites.

## What no sweep can see

Six things. Each has the same signature: perfectly ordinary code, no forbidden identifier anywhere,
and a divergence that only appears with two machines and enough time.

**1. A tie broken by position rather than by identity.** Every "nearest target", "cheapest option",
"first available slot" is a selection over candidates, and the spatial grid hands them over in cell
order. `TechnicalDesign.md` §2 already states the rule — *every query that reaches an outcome sorts its
candidates by entity identity before using them* — and a sweep cannot enforce it, because what it sees
is an ordinary `std::min_element`. **Ask: if two candidates compare equal, what decides?** If the answer
is anything other than entity identity, it is the container's insertion history, which is not a
specified quantity.

**2. Free-list reuse order.** Entities live in a `std::vector` with a free list, identified by index and
generation, and the tick iterates in index order. So **the order in which recycled indices are handed
out decides the iteration order of everything**. Two machines that free the same slots in a different
sequence get different indices for the same ships, and from then on every index-ordered pass diverges —
while the sweep sees a `std::vector`, which is as ordered as a container gets. Ask: is the free list
driven purely by the deterministic death order, taken consistently from one end?

**3. Two consumers of one PRNG stream.** R23 has **both sides running the generator** — the host to
populate the match, the client to draw the same asteroids. If the generator draws from the same engine
the simulation uses, the client advances a stream the host also advances, and the two fall out of step
at the first generated thing. The generator needs **its own engine**, seeded from the match seed by a
derivation that is written down. This one is invisible to every tool and obvious in hindsight.

**4. A fixed-point intermediate that overflows.** `(std::int64_t(a) * b) >> 8` is documented and is
correct — *for one multiply*. A chain is not: three values at 8 fractional bits each accumulate 24 bits
of fraction before the shift, and the 64-bit intermediate runs out sooner than intuition says. The type
is not the check; **the bound is**. Also: **Q1.15 spans [−1, +0.999969]**, so `sin(90°)` is 32,768 and
saturates ([`ADR-002`](../../../Design/ADR/ADR-002-tick-and-numbers.md)) — a multiply by "one" that
quietly is not.

**5. Wall time leaking past the seam.** The sweep catches `std::chrono` written *inside* the simulation.
It cannot catch a duration handed in as a parameter from the seam and used to scale something. Mapping
wall time to ticks at the boundary is the design; a system that knows how *long* anything took is not.

**6. A shared rule reading unshared state.** R19 has the client previewing an order against the same
`GameCore` rule the host validates with. If that rule reads anything the host does not have — a
selection, a camera, an interpolated position rather than a replicated one — the two answers differ by
construction. Not a desync in the classic sense, the same class of defect, and the same fix: the rule
takes replicated state as arguments and reads nothing else.

## The formats are settled; do not re-derive them

Position and velocity `std::int32_t` at 8 fractional bits, angles `std::uint16_t` binary, sine from the
4,096-entry Q1.15 table indexed by `angle >> 4`, distances compared squared in `std::int64_t`, one PRNG
in `NeuronCore` seeded from the match. [`ADR-002`](../../../Design/ADR/ADR-002-tick-and-numbers.md) and
`TechnicalDesign.md` §2 are authoritative and carry the arithmetic behind each. **The standard
library's engines are specified; its distributions are not portable**, so a value is derived from raw
engine output with arithmetic written here.

If a quantity seems to need a float, it needs a named unit instead: hold hundredths and say so in the
name (R6). `confidencePercent`, `fuelPerJump`, `upkeepCreditsPerDay` — the unit in the name is what
makes the integer readable, and it is the thing that stops the next person reaching for a double.

## The other half is a test, and it is not this

`GameLogicTests`' determinism test runs a fixed tick count from a seed against a scripted order list and
asserts the state hash — `TechnicalDesign.md` §8 calls it the most valuable test in the tree and it is.
But it catches a divergence **only on a path it happens to exercise**, and only where it runs: today
that is `Debug|x64`, on one machine, which is the one comparison that proves least. ADR-002 owes the
real one — the same seed across all four configuration and platform pairs.

So the three do different jobs and none substitutes for another: the sweep catches what is written, the
blind-spot list catches what is designed, the test catches what is executed. **Run all three.**

## What to hand back

```
Sweep:        CheckDeterminism.py <flags>, N violations, M judgement calls   ← empty means the work is not done
Judgement:    <each --review hit, and the answer: total order on what, which engine, which key>
Blind spots:  <of the six, which the change touches, and what was checked>
Formats:      <every new quantity, its type, its scale, and the unit in its name>
Bounds:       <every fixed-point expression added, and its worst-case intermediate>
Test:         <what the determinism test now covers that it did not>
```

Then say plainly which of the six you did **not** need to check and why — a blind spot skipped silently
is indistinguishable from one that was never noticed, and this is the rule where that difference is the
whole game.
