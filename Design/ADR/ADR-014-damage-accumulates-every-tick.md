# ADR-014 — Damage accumulates exactly every tick; deaths wait for every weapon

**Status:** Accepted — ruled 2026-09-24 by the owner, to the recommendation of `OpenQuestions.md` Q66, which
came from the mid-implementation review's M2. **Nothing implements it yet**: M3.1 builds the table and M3.2
the weapons pass, both to this record.
**Date:** 2026-09-24
**Owner:** Stefan Zwaal

## Context

`GameDesign.md` §7 states damage **per second**, per mount, and computes every row of its raid table from
that. [`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md) applies damage on the tick a weapon fires, and
the simulation ticks twenty times a second in integers (ADR-002). `Plan/README.md` F3 named the gap: nothing
said how often a weapon fires or where the integer division lands, and M3.0 was the gate that had to say.

**The review found the question narrower and sharper than F3 put it.** Any rule that applies whole points of
damage every tick makes stations and modules **immune to every ship weapon**: a MassDriver against a Heavy
target is 25 × 100 ÷ 400 = 6.25 a second, or 0.3125 a tick, which is zero whether it truncates or rounds. A
rule of whole points per shot at 1 Hz lands within +4% to +9% of every §7 row, so it would quietly move the
table. And the wire bounds the cadence of *events*, separately from damage: an update carries at most
`MAX_FIRES_PER_UPDATE` (40) fire events, each repeated `FIRE_REPEAT_TICKS` (3) times, so M mounts firing an
event every tick need 3M sends a tick. At 100 mounts that backs up 52,000 sends in 200 ticks, measured on the
unmodified accumulator.

Two orderings were unpinned as well. Deaths applied inside the weapons pass, in slot order, hand the lower
slot the first shot in every symmetric fight, which is a player-1 edge by construction. And with two
shooters on one target, overkill was unspecified.

## Decision

**Every weapon accumulates damage every tick, in ten-thousandths of a point.** A mount in range of its target
adds `dps × modifierPercent × 100 ÷ 20` a tick, which is `dps × modifierPercent × 5` and **a whole number for
every weapon and size class there is**: 25 dps at 70% against Small is 8,750 a tick, 0.875 of a point, and
PointDefense's 60 at 90% against Medium is 27,000, 2.7 points. **Whole points are applied to hull, and the
remainder is kept on the mount** against the same target.

*The unit is corrected from the ruling's wording.* Q66's recommendation, and the review's M2 before it, say
"hundredths of a point", but hundredths leave a fraction a tick (25 × 70% is 87.5 hundredths); the review's
own formula, `dps × modifier × 100 / 20`, is ten-thousandths. The ruling's intent, §7 reproduced exactly with
no rounding anywhere, needs this unit, so this record states it. A MassDriver therefore takes a station's hull down at exactly 6.25 a second,
and §7's rows hold exactly rather than within a tolerance.

**Deaths are applied at the deaths step, after every weapon has fired.** No weapon's damage depends on the
slot order of the weapons before it, and two stations reduced to zero on one tick are both dead, which is
`OpenQuestions.md` Q65's draw. **Overkill is allowed**: two shooters on a target with 10 points left both
apply their damage that tick.

**A fire event is emitted at most once per shooter per ten ticks**, whatever the damage cadence. It exists to
draw a tracer and to feed [`ADR-020`](ADR-020-damage-offscreen-is-announced-at-the-edge.md)'s alert, and
neither needs twenty a second. At that rate 104 mounts are 31 sends a tick against the cap of 40. **The
repeat drops from three to two while a client's pending fire events exceed 40**, so a brawl costs tracers
and never datagrams.

**§7's rows are pinned as tests at M3.1**, with one tick's damage of tolerance: time to kill for each row of
the raid table, computed from the catalog through this rule.

## Consequences

**The station is killable by ships**, which §5's whole standoff argument assumed and a per-tick integer rule
would have silently denied.

**A remainder is state, and state is hashed.** Each mount's accumulated hundredths live in `GameLogic` beside
its target, reset when the target changes, and M3.2 folds them into the state hash (ADR-002, as widened on
2026-09-24), so two builds that round differently diverge in the hash on the tick they do.

**The alert and the tracer see a thinned stream**, one event per shooter per half second. That is what the
review's sixth experiment measures before M3.11's question 0 is asked: alerts a minute in a scripted fight.

**What it forecloses:** a weapon with a visible reload that matters to play, since damage no longer arrives in
volleys. A weapon that should hit in volleys is a different row with its own rule, written when one exists.

## Measurements

**Owed at M3.1 and M3.2, and M3.1's is taken:**

1. ~~**Each §7 row reproduced by a test within one tick.**~~ — **DISCHARGED at M3.1, 2026-09-24**, by
   `Tests/GameCoreTests/DamageTableTests.cpp` on all four pairs. Every row is exact rather than within a
   tick: 258, 400, 1,280, 12,800, 2,400 and 800 ticks. The three-against-six row is exact at the pooled
   rate, 515 ticks, which is §7's 25.7 seconds. It is 516 ticks, 25.8 seconds, when the miners are killed
   one at a time, because this record allows overkill and every kill ends partway through a tick.
   `Plan/M3-the-fight.md` M3.1 has the table.
2. **The fire events a tick in the scripted fight at 110 entities, under 40.** Owed at M3.2.
3. **The four-pair determinism run with the remainder in the hash.** Owed at M3.2.
