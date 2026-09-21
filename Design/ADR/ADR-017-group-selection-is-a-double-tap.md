# ADR-017 — Group selection is a double tap, and `Holding` is freed

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal — on a touch review against the target device
**Amends:** [`ADR-010`](ADR-010-selection-is-proximity-and-design.md). Its selection *rule* — same design,
screen-space circle, camera as the group-size control — is untouched. Only the verb that triggers it
changes.

## Context

**ADR-010 rejected hold-then-drag partly for a cost its own decision then paid.** It records that
hold-then-drag was wrong because it meant "two meanings on one finger, **about 300 ms of latency before
the band appears**, and a gesture that is not self-evident" — and then adopted a plain `Holding`, which
fires no faster. Windows' hold threshold for touch is in the region of half a second. The latency
objection survived the decision it was used to justify.

That matters because of *when* group selection happens. It is not a setup action taken at leisure; it is
what a player does repeatedly while under fire, and holding a finger motionless for half a second is a
long time to stand still in a real-time game. It is also the one gesture in this design where the finger
must not move, on a handheld device, which is the hardest thing to ask of a hand.

**The design is also short of verbs and has already paid for it.** `Interface.md` §7 states plainly that
**order queueing has no gesture and is out of the MVP because of it**, and that `Holding` over empty space
is being banked against a future need. A vocabulary that has already refused a feature for want of a verb
is one worth adding to if the addition is free.

## Decision

**A tap on your own ship selects it. A second tap that resolves to the same ship, within 300 milliseconds,
expands the selection to every ship of that design within ADR-010's circle.**

**The first tap fires immediately and is never deferred.** This is the whole of why double tap is
affordable here. The usual objection — that enabling double tap delays every single tap by the
double-tap window while the client waits to see whether a second one arrives — does not apply, because
nothing waits. The single-tap action happens at once and the second tap *upgrades* the result. Tapping
a ship and tapping it again is select-then-expand, not a deferred decision between two outcomes, so **no
tap anywhere in this game gets slower.**

**The second tap is matched by entity identity, not by screen distance.** It counts if it resolves to the
same ship, which is robust to the ship having moved between the two taps — a screen-distance threshold
would fail exactly when the fleet is moving, which is when this is used.

**It arrives on the event this design already uses.** `GestureRecognizer` delivers it through `Tapped`
with `TappedEventArgs.TapCount`, under `GestureSettings::DoubleTap`. It is a field on a verb R21 already
names, not a fourth verb.

**`Holding` is now unassigned everywhere**, not just over empty space.

## Consequences

**It is faster where speed matters.** Two taps land in roughly 300 ms against a hold's 500 ms or more, and
the intermediate state is useful: one ship is already selected, so a gesture abandoned halfway has cost
nothing and taken no time.

**It is the idiom.** Double-click selects all of type in StarCraft, in Age of Empires and in most things
since. A player who has touched an RTS before already knows this, which is not true of a hold.

**The gesture budget goes from one spare verb to two.** `Holding` is free over ships as well as over empty
space. **What to spend it on is deliberately not decided here** — order queueing is the standing candidate
(`Interface.md` §7), a deselect and a stop-and-hold-position are the others, and the reason this ADR does
not pick one is that ADR-010's instinct to bank a verb rather than spend it on something marginal was
right. It is now banking two.

**What it costs is a double tap issued by accident.** Tapping two different ships in quick succession is
not affected — the identity match makes that two single taps. The real case is a player tapping the same
ship twice meaning to re-select it, and getting the group. That is a recoverable mistake and a cheap one:
the fix is a third tap.

**ADR-010's circle is untouched.** Screen-space, 192 authored pixels, drawn during the gesture, centered on
the ship, own ships only, same design only, fixed radius. **The camera is still the group-size control.**
The wedge that ADR-010 records at a raking camera is still there and is still M1's to watch — though
`Interface.md` §5's pitch floor now bounds how bad it can get, which is a bound ADR-010 did not have.

## Measurements

None yet. Two are owed at **M1.16**, and both are settled by playing rather than arguing:

1. **Whether 300 milliseconds is the right window** — long enough to be reachable under pressure, short
   enough that a deliberate re-tap does not expand by accident.
2. **Whether accidental expansion is actually annoying**, which is the only cost this decision has and the
   one thing a test cannot report.

The 300 ms figure is a starting point taken from common practice, not a measurement, and is stated as one.
