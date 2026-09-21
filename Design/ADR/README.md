# Architecture Decision Records

One file per engineering decision, numbered in order (`AGENTS.md` §6). Numbering starts at `ADR-001`:
this tree has taken no decisions before these.

| ADR | Decision | Status | Date |
|---|---|---|---|
| [`ADR-001`](ADR-001-the-playfield-is-a-plane.md) | The simulation is two-dimensional and the camera is not, because a tap is a ray and a ray has no depth | Accepted | 2026-09-20 |
| [`ADR-002`](ADR-002-tick-and-numbers.md) | The 20 Hz tick, the 1/256 position unit, the 16-bit binary angle over a 4,096-entry sine table, the pinned PRNG, and candidate ordering as a correctness property | Accepted | 2026-09-20 |
| [`ADR-003`](ADR-003-replication-is-full-snapshots.md) | Full self-contained snapshots at 20 Hz, no delta and no acknowledgement; a ten-byte record, an explicit removal list, host-side command validation, and commands made reliable by a sequence the snapshot already carries | Accepted | 2026-09-20 |
| [`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md) | No projectile entities in the MVP: damage lands on the tick a weapon fires and the client draws an event | Accepted | 2026-09-20 |
| [`ADR-005`](ADR-005-meshes-are-generated-in-code.md) | No mesh format, no loader and no asset build step in the MVP | Accepted | 2026-09-20 |
| [`ADR-006`](ADR-006-a-ship-is-a-composition.md) | A ship is a hull, a drive and its slots from the first line, with every stat derived by one tested pure function | Accepted | 2026-09-20 |
| [`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md) | The authored frame is 1440 × 960, which is an exact 2× point-sampled fit on the Surface Pro, and the 48-pixel touch target that follows | Accepted | 2026-09-20 |
| [`ADR-008`](ADR-008-the-host-address-is-configuration.md) | The host address is a configuration file with a compiled-in default and no discovery; the loopback exemption is a development arrangement and not a shipping one | Accepted | 2026-09-20 |
| [`ADR-009`](ADR-009-text-is-directwrite-into-an-atlas.md) | Text is DirectWrite rasterised into a Direct3D 12 atlas we own — no Direct2D and no D3D11On12, which R12 bans, and no dependency | Accepted | 2026-09-20 |
| [`ADR-010`](ADR-010-selection-is-proximity-and-design.md) | A tap selects one ship and a hold selects the same design within a screen-space circle; no band select, so one-finger drag is unconditionally panning. Supersedes `Design/Interface.md` §3's hold-then-drag | Accepted | 2026-09-20 |
| [`ADR-015`](ADR-015-the-base-is-built-from-modules.md) | The base is built from modules, and a module is a separate destroyable entity placed by tap inside the point-defence radius; each upgrade level is its own component identity | Accepted | 2026-09-21 |
| [`ADR-011`](ADR-011-the-interface-draws-after-the-scale.md) | The interface draws after the scale at physical resolution, in authored coordinates through the transform R13 already computes. Breaks R13's letter, keeps its intent; amends ADR-007 to bind the world only and supersedes ADR-009's pixel doubling | Accepted | 2026-09-20 |

**Accepted** means the owner decided it. **Proposed** means the design takes it and the owner has not yet
ruled; a proposed ADR is not something to write code against. **All eleven are now Accepted** — ADR-002
to ADR-005 were ruled on 2026-09-20 following an adversarial review, three of them with changes.

**ADR-012, ADR-013 and ADR-014 do not exist yet and their numbers are reserved.** The implementation plan
met three questions the design does not answer and named a number for each rather than answering them in
passing: the shader build path (`Plan/README.md` F1, due at M0.14), the join record (F2, M1.4), and the
firing interval with its integer rounding (F3, M3). **A gap in this list is a reservation, not a lost
file.**

## When to write one

A decision is anything a future reader would otherwise re-litigate: a file format, a wire protocol, a
subsystem's shape, a project added, an exception to a rule in `AGENTS.md`, a figure the design left to
measurement. It is written **in the same commit as the change that implements it**, and the design document
it settles is updated in that commit to cite it.

Not every choice is a decision. A local naming choice, a refactor that changes no boundary, and anything
`AGENTS.md` already settles need none.

## The file

`ADR-<nnn>-<slug>.md`: three digits and a kebab-case slug. Written in the voice of `AGENTS.md` — dated,
owned, and stating what it forecloses as plainly as what it chooses. Figures are measured, not estimated,
and say how they were measured; a figure that is arithmetic on the design's own numbers says so.

## Template

```markdown
# ADR-<nnn> — <title>

**Status:** Proposed | Accepted | Superseded by ADR-<nnn>
**Date:** <YYYY-MM-DD>
**Owner:** <who decided>

## Context

What question the work met, and why it had to be answered now. Cite the design section
(`Design/<Document>.md §n`) or the `AGENTS.md` rule that raised it.

## Decision

What was decided, precisely enough to check code against it.

## Consequences

What this forecloses, what it costs, and what would reopen it.

## Measurements

The figures the decision rests on, each with how it was measured: the command, the build, the machine.
If a figure is arithmetic rather than a measurement, say what it is arithmetic on.
```

## Superseding

A decision is never edited into a different decision. A new ADR supersedes it, the old one's status line
points forward, and the old file stays: the history of why is the point.
