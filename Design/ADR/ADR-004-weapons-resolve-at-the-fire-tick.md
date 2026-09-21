# ADR-004 — Weapons resolve at the fire tick; the MVP has no projectiles

**Status:** Accepted — ruled 2026-09-20 following an adversarial review, **with changes**: the fire event is
now a specified wire record rather than a name in prose, and the decision's effect on the speed counter is
stated.
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

`Design/GameDesign.md` §7 gives every armed ship a weapon that fires roughly once a second, and
`Design/GameDesign.md` §10 caps the field at about 200 ships. A projectile with a travel time of around a
second would therefore put **as many projectiles in flight as there are ships**.

[`ADR-003`](ADR-003-replication-is-full-snapshots.md) makes every entity a per-snapshot cost, so doubling
the entity count doubles the replication budget outright. It also doubles the simulation's per-tick
integration and collision work, and adds a spawn-and-destroy churn that the entity storage would feel
every tick rather than occasionally.

The question is whether the MVP's combat needs projectiles to be *entities*.

## Decision

**A weapon applies its damage on the tick it fires.** There is no projectile in the simulation, none in
the entity store and none on the wire. Range and firing arc are checked at the fire tick; if the target is
in range, the damage lands.

**The host emits a fire event** — shooter 2, target 2, weapon 1 — appended to the snapshot after the
removal list, behind a count byte. It is unreliable in the sense that it is not retransmitted: a lost
snapshot costs a missing tracer and nothing else, which is why it is not worth a reliability path of its
own. **This ADR originally referred to a fire event and a death event without specifying either**, which
left two wire records named in prose and defined nowhere. The death event is the removal list
[`ADR-003`](ADR-003-replication-is-full-snapshots.md) now carries; this is the other.

**The client draws it.** A tracer, a beam or a muzzle flash, entirely client-side, on a client-side timer.
The same applies to wrecks: a destroyed ship leaves debris the client spawns from a death event and decays
on its own, and **the host never knows a wreck exists**.

## Consequences

**This removes roughly 200 entities from the simulation and the wire**, which is the whole reason for it.
It also removes projectile collision, projectile lifetime, and the interaction between a projectile and a
target that dies before it arrives.

**It quietly strengthens the counter this design depends on, and neither document said so.**
`Design/GameDesign.md` §7 claims the counter to a heavier ship is speed — strike craft pick the fight and
leave. **Instant resolution is what makes disengaging actually work**: a ship that leaves weapon range
takes zero further damage, where with travel time the shots already fired still land. The two decisions
are coupled and were taken independently; the coupling is in this ADR's favor and is recorded here so
that reopening either one is known to move the other.

**What it costs is a class of gameplay.** Nothing can be intercepted, nothing can miss by flying past,
there is no lead-the-target, and a fleeing ship cannot outrun a shot already fired. Combat resolves as
attrition arithmetic between things in range, which is closer to *Warzone 2100*'s feel than to
*Homeworld*'s, and it is not what a player watching a tracer will expect.

**There is a presentation seam here and it should be honest about itself.** A client drawing a tracer that
always hits is drawing a lie about a shot that has already resolved. That is fine for a tracer and it is
not fine for anything a player is meant to react to.

**What reopens it is precise and worth stating:** the first weapon whose travel time is supposed to
*matter* — a torpedo you are meant to be able to shoot down, a missile you are meant to be able to outrun
— is a real entity and cannot be anything else. At that point projectiles arrive as a second entity kind
with their own replication budget, and this ADR is superseded rather than amended.

## Measurements

None. The figure this rests on — about 200 projectiles in flight, doubling both the entity count and the
replication budget — is **arithmetic** on `Design/GameDesign.md`'s fleet cap of 50 ships per player, four
players, a firing interval of about one second and a travel time of about one second. It is not a
measurement and no measurement is owed before accepting it, because the decision is about what combat
should *feel* like at least as much as about the budget.
