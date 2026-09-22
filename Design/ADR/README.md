# Architecture Decision Records

One file per engineering decision, numbered in order (`AGENTS.md` §6). Numbering starts at `ADR-001`:
this tree has taken no decisions before these.

| ADR | Decision | Status | Date |
|---|---|---|---|
| [`ADR-001`](ADR-001-the-playfield-is-a-plane.md) | The simulation is two-dimensional and the camera is not, because a tap is a ray and a ray has no depth | Accepted | 2026-09-20 |
| [`ADR-002`](ADR-002-tick-and-numbers.md) | The 20 Hz tick, the 1/256 position unit, the 16-bit binary angle over a 4,096-entry sine table, the pinned PRNG, and candidate ordering as a correctness property | Accepted | 2026-09-20 |
| [`ADR-003`](ADR-003-replication-is-full-snapshots.md) | Full self-contained snapshots at 20 Hz, no delta and no acknowledgment; a ten-byte record, an explicit removal list, host-side command validation, and commands made reliable by a sequence the snapshot already carries | Accepted | 2026-09-20 |
| [`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md) | No projectile entities in the MVP: damage lands on the tick a weapon fires and the client draws an event | Accepted | 2026-09-20 |
| [`ADR-005`](ADR-005-a-mesh-is-a-cmo-file.md) | A mesh is a CMO file, authored as content, with the reader written here — R14 closes a list of libraries and not a list of files. **Replaced 2026-09-22**, having decided the opposite: a mesh was a function | Accepted | 2026-09-22 |
| [`ADR-006`](ADR-006-a-ship-is-a-composition.md) | A ship is a hull, a drive and its slots from the first line, with every stat derived by one tested pure function | Accepted | 2026-09-20 |
| [`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md) | The world is authored at a scale of the panel, **defaulting to 1:1 since [`ADR-016`](ADR-016-the-world-resolution-is-a-scale.md)**, and the 48-pixel touch target that follows from the panel rather than from it | Accepted | 2026-09-20 |
| [`ADR-008`](ADR-008-the-host-address-is-configuration.md) | The host address is a configuration file with a compiled-in default and no discovery; the loopback exemption is a development arrangement and not a shipping one | Accepted | 2026-09-20 |
| [`ADR-009`](ADR-009-text-is-directwrite-into-an-atlas.md) | Text is DirectWrite rasterised into a Direct3D 12 atlas we own — no Direct2D and no D3D11On12, which R12 bans, and no dependency | Accepted | 2026-09-20 |
| [`ADR-010`](ADR-010-selection-is-proximity-and-design.md) | A tap selects one ship and a hold selects the same design within a screen-space circle; no band select, so one-finger drag is unconditionally panning. Supersedes `Design/Interface.md` §3's hold-then-drag | Accepted | 2026-09-20 |
| [`ADR-013`](ADR-013-a-client-is-told-which-player-it-is.md) | A client learns its player from a `Join` the host answers with a slot, a session token and the match seed; the host assigns the slot, a token is a name rather than a credential, and a timeout forgets an endpoint but never a slot | Accepted | 2026-09-22 |
| [`ADR-015`](ADR-015-the-base-is-built-from-modules.md) | The base is built from modules, and a module is a separate destroyable entity placed by tap inside the point-defense radius; each upgrade level is its own component identity | Accepted | 2026-09-21 |
| [`ADR-011`](ADR-011-the-interface-draws-after-the-scale.md) | The interface draws after the scale at physical resolution, in authored coordinates through the transform R13 already computes. Breaks R13's letter, keeps its intent; amends ADR-007 to bind the world only and supersedes ADR-009's pixel doubling | Accepted | 2026-09-20 |
| [`ADR-012`](ADR-012-a-shader-is-compiled-into-a-header.md) | A shader is compiled by `dxc` at Shader Model 6.7, through `FxCompile`, into a checked-in header rather than a `.cso` on disk — `NeuronClient` is a static library with no package of its own, and a packaged `.cso` is read through an asynchronous API on the ASTA. Raises the minimum Windows to 10.0.22621.0 | Accepted | 2026-09-21 |
| [`ADR-016`](ADR-016-the-world-resolution-is-a-scale.md) | The world's resolution is a scale defaulting to 1:1, and the interface gets its own transform rather than borrowing the world's | Accepted | 2026-09-21 |
| [`ADR-017`](ADR-017-group-selection-is-a-double-tap.md) | Group selection is a double tap rather than a hold — the first tap acts at once and the second upgrades it — which frees `Holding` | Accepted | 2026-09-21 |
| [`ADR-018`](ADR-018-the-camera-is-anchored-to-the-plane.md) | The camera is ray-anchored to the plane and one solve drives pan, zoom and orbit; no inertia, and a hold on empty space recenters | Accepted | 2026-09-21 |
| [`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md) | The sky is a baked galaxy cubemap plus seeded instanced stars, modeled on the real magnitude and color distributions and capped at 12% large-area luminance | Accepted | 2026-09-21 |
| [`ADR-020`](ADR-020-damage-offscreen-is-announced-at-the-edge.md) | Damage off screen shows as a directional indicator at the edge, derived from data the client already has and tappable to recenter | Accepted | 2026-09-21 |
| [`ADR-021`](ADR-021-content-ships-with-the-package.md) | Content files ship with the package; the line R14 drew is against dependencies, and R16's is against simulation data becoming files | Accepted | 2026-09-22 |

**Accepted** means the owner decided it. **Proposed** means the design takes it and the owner has not yet
ruled; a proposed ADR is not something to write code against. **All twenty are Accepted** — ADR-002 to
ADR-005 were ruled on 2026-09-20 following an adversarial review, three of them with changes; ADR-012
on 2026-09-21 with the compiler changed from the one the plan recommended; ADR-021 on 2026-09-22, which
also settled that a conflicting record is updated in place through the MVP rather than superseded; and
**ADR-005 again on 2026-09-22, reversed outright** — a mesh is a CMO file where that record had ruled a
mesh was a function. That last one is the exception below being stretched from contradictions to
decisions, and the owner stretched it.

**ADR-014 does not exist yet and its number is reserved.** The implementation plan met three questions
the design does not answer and named a number for each rather than answering them in passing: the shader
build path (`Plan/README.md` F1) became ADR-012, the join record (F2, M1.4) became ADR-013, and **the
firing interval with its integer rounding (F3, M3) is still owed**. **A gap in this list is a reservation,
not a lost file.**

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

## Superseding, and the MVP exception

**Through the MVP, a conflicting ADR is updated in place.** The owner ruled this on 2026-09-22: while the
design is still being settled, two records that disagree cost more than the history of the disagreement
buys, and a reader who has to work out which of two paragraphs is live is the failure this directory
exists to prevent. So an ADR that has been overtaken is **edited until it is true**, its status line says
what changed and when, and the git history carries the rest.

**`ADR-005` is the worked example, and now the limit case as well.** It was first updated rather than
half-superseded, alongside `ADR-021`. Then on 2026-09-22 the owner **reversed its decision outright** — a
mesh is a CMO file where it had ruled a mesh was a function — and ruled it replaced in place rather than
superseded by a twenty-second record, on the same ground: nothing has shipped, and a second record
arguing with the first is the cost this exception exists to avoid. **Its slug changed with its title**,
because a filename asserting a decision the file no longer takes is the same defect one directory out.

**After the MVP this inverts, and the older rule takes over:** a decision is never edited into a different
decision, a new ADR supersedes it, the old one's status line points forward, and the old file stays —
because once something has shipped, why it used to be another way is a question people actually ask.

**A new decision is usually still a new record, and stretching that is the owner's call rather than an
author's.** This exception is about removing contradictions, not about folding every change into whatever
record is nearest: `ADR-021` is its own file because it decides something, and `ADR-019` was edited
because it merely disagreed with it. **`ADR-005` is the one case where a new decision replaced an old
record instead**, and the reason is that it *reversed* that record rather than adding to it — a
twenty-second ADR would have left the fifth asserting the opposite for the whole of the MVP.
