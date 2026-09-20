# ADR-001 — The playfield is a plane

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

*Outpost Commander* takes its feel from *Homeworld*, whose defining property is movement through a
three-dimensional volume. It also takes `AGENTS.md` R21: **touch is the only input**, the vocabulary is
`Tapped`, `Holding` and a manipulation's translate, scale and rotate, and there is no keyboard, no
modifier and no second pointer button anywhere.

Those two do not fit. **A tap is a ray, and a ray has no depth.** Specifying a point in a volume needs two
independent inputs, which is why *Homeworld* invented the move disk and why the move disk needed a mouse
and a held key to drive it. On a tablet gripped in two hands, a two-stage depth gesture competes with the
camera for the same fingers, at the moment a player is least able to spare attention.

The question had to be answered before anything was written, because it decides the spatial index, the
collision test, the pathfinding, the camera, the AI's target selection and the wire format — every one of
which is cheaper to write once than to convert.

## Decision

**The simulation is two-dimensional.** Every entity's position is a point on one plane; there is no third
coordinate anywhere in `GameCore` or `GameLogic`, and none on the wire.

**The camera is three-dimensional** and orbits, pitches and zooms freely over that plane
(`Design/Interface.md` §5). Asteroids, wrecks, debris and effects may be *drawn* above and below the plane
so the space reads as a volume; none of it is simulated and the host does not know it exists.

A move order is therefore one tap, resolved by intersecting one ray with one plane.

## Consequences

**What is lost is the tactical z-axis**: attacking from above, hiding beneath the plane, and the vertical
envelopment *Homeworld* is remembered for. This is a real loss and it is taken deliberately rather than
worked around.

**What is gained is everything downstream.** Two-dimensional spatial indexing, collision and separation;
one ray-plane intersection instead of a depth-placement interaction; an AI that reasons in two dimensions;
four bytes of position on the wire instead of six. The MVP is reachable largely because of this.

What survives of the lineage is fleets rather than armies, strike craft against capitals, collectors
feeding a base, the sense of scale and the silhouette. None of those need the third axis.

**What would reopen it:** an input device that gives a second axis honestly. It would not be a small
change — it is the spatial index, the collision test, the camera, the AI and the wire format at once — and
anyone reopening it should expect to rewrite rather than extend.

## Measurements

None. This is a decision about what is expressible with a finger, not a quantity. The arithmetic it makes
possible — four bytes of position rather than six, and a snapshot of 1,872 bytes rather than about 2,280 —
is in `Design/TechnicalDesign.md` §4 and is arithmetic on the design's own entity counts, not a
measurement.
