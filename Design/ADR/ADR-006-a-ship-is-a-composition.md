# ADR-006 — A ship is a hull, a drive and its slots, from the first line

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

The brief asks for two things that look like they are in tension at MVP scope: a station that builds **a
miner, a fighter and a battleship**, and *Warzone 2100*'s system where **a unit is a composition** a player
designs rather than a type they pick. Three fixed ships need no component model; the *Warzone* system is
meaningless without the research that feeds it, and research is explicitly future work
(`Design/GameDesign.md` §9).

So the honest question is not whether the MVP needs the designer — it does not — but **when the model
underneath it has to exist**, given that the expensive part is not building it but retrofitting it.

Retrofitting it is not one change. A ship's damage output, its armour, its cost, its build time, its mass,
its speed and its turn rate would each move from a constant on a type to a derivation over a set, and each
of those appears in the simulation, in the build queue, in the AI's evaluation and in the wire format. It
is a change that touches everything at once, which is the definition of a change that does not get made.

## Decision

**A ship is a hull, a drive and a component in each of the hull's slots.** Nothing in the simulation knows
what a "fighter" is. It knows a **design**, and a design is a composition referred to by identity.

**Every stat is derived**, by one pure integer function in `GameCore`, with a test suite over every
combination in the catalog. Mass is the hull plus its contents; speed is thrust over mass; cost and build
time are sums. **A cruiser with four plasma cannons is slower than an empty one because of arithmetic, not
because anyone wrote it down.**

**The MVP ships two designs and no designer.** Miner and Fighter are *rows in a table*, not types in the
code, and so is the station:

| Design | Hull | Drive | Slots |
|---|---|---|---|
| Miner | `Scout` | `IonDrive` | 1× `MiningLaser` |
| Fighter | `Frigate` | `BurnDrive` | 2× `MassDriver` |
| Station | `Station` | *(none)* | 2× `PointDefence` |

A heavy design on the `Cruiser` hull was cut from the MVP on 2026-09-20 for reasons of economy rather than
model (`Design/GameDesign.md` §6, §10). **The hull stays in the catalog and the derivation tests still
cover it**, which is the point: reinstating it at M4 is a table row and nothing else, which is the claim
this ADR exists to make.

**A component is referred to by identity and never hardcoded**, which is the single property research
needs: research adds an availability gate over a set of component identities and changes nothing else.

## Consequences

**The miner is the evidence.** It is not a ship type — it is a `Scout` hull with a mining tool where a
weapon would go, and the station is a hull with two point-defence mounts and no drive. That three things
which look like three kinds of object fall out of one model with no special case anywhere is what says
the model is right, and it costs nothing today.

**The wire format contradicted this rule and has been corrected.** The design identity was packed into a
flags byte alongside team and state, leaving two bits — room for exactly four designs, permanently. A
two-bit cap is a hardcoded limit wearing the costume of an identity, and it would have been discovered
while building the designer this ADR exists to make cheap.
[`ADR-003`](ADR-003-replication-is-full-snapshots.md) now gives the design identity its own byte.

**The designer and research become additive.** The designer is a screen that writes a row into a table the
simulation already reads. Research is a gate over identities the code already treats as data. Neither
needs the simulation changed, which is the entire point.

**What it costs now** is a level of indirection the MVP does not visibly need: a catalog, a design table,
a derivation function and its tests, where three structs with baked numbers would have run today. That is
a few hundred lines and a suite, spent deliberately.

**What it costs later, and is worth naming:** every stat being derived means **no design can be tuned
directly.** Balancing a fighter means changing a component that other designs also use, and the second
design that uses `MassDriver` moves with it. That is *Warzone 2100*'s actual balance problem and this
inherits it.

**What would reopen it:** nothing short of abandoning the *Warzone* half of the design, at which point
three baked types would be correct and this would be ceremony.

## Measurements

None. The claim this rests on — that retrofitting the model later is the expensive path — is an argument
about the blast radius of a change, stated in the Context, not a figure.
