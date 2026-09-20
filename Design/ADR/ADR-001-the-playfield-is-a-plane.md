# ADR-001 — The playfield is a plane

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

*Outpost Commander* takes its feel from *Homeworld*, whose defining property is movement through a
three-dimensional volume. It also takes `AGENTS.md` R21: **touch is the only input**, the vocabulary is
`Tapped`, `Holding` and a manipulation's translate, scale and rotate, and there is no keyboard, no
modifier and no second pointer button anywhere.

**This ADR originally argued that a tap is a ray and a ray has no depth, and that argument does not
survive scrutiny.** A ray has the depth of *the camera's current focus plane* — which is precisely what
*Homeworld*'s move disk is, with the vertical drag as a rarely used modifier rather than the primary
interaction. Under R21 one could legitimately build a volume where a tap places on the focus plane and
changing altitude means moving the camera, which pinch and orbit already do. **No fourth gesture is
required, so the stated reason was refutable.** It is replaced here rather than left standing, because a
refutable justification on an irreversible decision is a decision that gets reopened by whoever spots the
hole.

**The plane wins on two other grounds, and they hold.**

**Implementation cost.** Three-dimensional spatial indexing, three-dimensional separation — which is
already an unbudgeted cost in two dimensions, see `Design/TechnicalDesign.md` §2 — three-dimensional AI
target selection, and six bytes of position instead of four, against one developer and no code at all.

**The camera's gesture budget, which is the decisive one.** On a plane, pitch can be coupled to zoom
(`Design/Interface.md` §5), which frees a degree of freedom outright. **In a volume the player needs
independent pitch to read depth**, so the fourth degree of freedom comes back — not for the move order,
which the focus plane solves, but for the camera, where R21 has nothing left to give.

The question had to be answered before anything was written, because it decides the spatial index, the
collision test, the pathfinding, the camera, the AI's target selection and the wire format — every one of
which is cheaper to write once than to convert.

## Decision

**The simulation is two-dimensional.** Every entity's position is a point on one plane; there is no third
coordinate anywhere in `GameCore` or `GameLogic`, and none on the wire.

**The camera is not.** It always looks at a focus point *on* the plane, orbits around that point and
zooms; **pitch is coupled to zoom rather than separately controlled, and it never rolls**
(`Design/Interface.md` §5). That is four degrees of freedom — focus, heading, distance — and it is
deliberately not a free camera: it is enough to read scale and silhouette, and no more. Asteroids,
wrecks, debris and effects may be *drawn* above and below the plane so the space reads as a volume; none
of it is simulated and the host does not know it exists.

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
possible — four bytes of position rather than six, and an MVP snapshot of 1,056 bytes rather than about
1,260, which is the difference between one datagram and two —
is in `Design/TechnicalDesign.md` §4 and is arithmetic on the design's own entity counts, not a
measurement.
