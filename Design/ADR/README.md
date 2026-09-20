# Architecture Decision Records

One file per engineering decision, numbered in order (`AGENTS.md` §6). **ADR-001 to ADR-012 arrived with the code, from *Frontier Commander*, when this tree was forked on 2026-09-20**; the UWP migration begins at ADR-013. The last section says why the numbering continues rather than restarting.

| ADR | Decision | Date |
|---|---|---|
| [`ADR-001`](ADR-001-solution-layout.md) | The solution and project layout: the eight projects, their edges and namespaces, the settings every project carries, the test-project shape | 2026-09-17 |
| [`ADR-002`](ADR-002-tick-and-numbers.md) | The 20 Hz tick, the 1/256 position unit and the fixed-point formats, the state hash's order, the order record; the empty tick measured | 2026-09-17 |
| [`ADR-003`](ADR-003-snapshot-and-replay-formats.md) | The snapshot and replay formats: the stream layout, its digest, and the one-version rule until the first save ships | 2026-09-17 |
| [`ADR-004`](ADR-004-renderer-foundation.md) | The renderer foundation: 1920×1080 authored, a borderless window over the primary monitor, a 4× multisampled scene target resolved and presented scaled; the `HRESULT` policy; `d3dx12.h` pinned at DirectX-Headers v1.606.3; shader model 6.0 through `FXCompile` and `dxc` | 2026-09-17 |
| [`ADR-005`](ADR-005-fog-and-lighting.md) | Fog and lighting: the Species lighting unchanged; team-colour slots neither lit nor fogged; distance desaturation over the Species fog scaled to the landscape, chosen on two captured frames, the owner confirming or overriding at T22; the far plane scaling with the landscape and the reversed depth that follow | 2026-09-17 |
| [`ADR-006`](ADR-006-content-format.md) | The content format: one plain row aggregate per table with integer fields in the unit their names say, a version per file refused by name, fail-fast loading and exhaustive validation, a line on every diagnostic including the ones that span files, `OutpostHost --validate` as CI's gate, and `DesignStats` as the one derivation | 2026-09-17 |
| [`ADR-007`](ADR-007-view-budget.md) | The view budget: the fog range absolute in world units with a ceiling on the desaturation, superseding ADR-005's fractions on the owner's frame; terrain chunk residency bounded by radius with the coarsest level resident everywhere, which holds for the four shipping size classes and is stated as O(area) that a quadtree must replace beyond them; the renderer's distances explicitly not line of sight | 2026-09-18 |
| [`ADR-008`](ADR-008-fog-grid-encoding.md) | The fog grid, the first O(area) state a snapshot carries, is run-length encoded; ADR-003's refusal of compression superseded for that one section on the measurement it demanded, every other section unchanged | 2026-09-18 |
| [`ADR-009`](ADR-009-content-in-the-simulation.md) | The simulation reads the content tree, taken by reference at construction and never written; a snapshot carries the digest of the tables it was written against and refuses any other, so reloading against different rules is a refusal rather than a silent divergence | 2026-09-18 |
| [`ADR-010`](ADR-010-species-content.md) | The Species-derived content: the owner's acceptance of the provenance risk, including distribution in M3 and inside mods; the inventory of every file that has come across and the tool that converted it; the exclusions enforced in `Tools/ImportSounds.py` rather than remembered | 2026-09-18 |
| [`ADR-011`](ADR-011-model-normals.md) | Model normals baked by the loader onto a split vertex rather than taken from the pixel shader's derivatives, because the colour already forces the split and the derivative's sign has no answer on a model; the outward normal is the SDK's `cross(c − a, b − a)`, pinned by a test and checked by a winding count; the captured frame K1 asked for is owed once G2 lands | 2026-09-18 |
| [`ADR-012`](ADR-012-replication-protocol.md) | The replication protocol: the interest set as a security boundary, delta frames against a 32-frame history, positions quantized to a quarter of a world unit by floor division, 1,200-byte fragments, and the bytes a frame takes at 100 and 600 visible objects, measured | 2026-09-19 |
| [`ADR-013`](ADR-013-uwp-application-model.md) | The application model: an MSIX-packaged UWP app whose view is a `CoreWindow` driven by `IFrameworkView`, no XAML anywhere, `TryEnterFullScreenMode` in place of the borderless popup and `CoreApplication::Exit` in place of Alt+F4; supersedes ADR-004 on the window | 2026-09-20 |
| [`ADR-014`](ADR-014-platform-boundary.md) | The platform boundary: exactly one packaged project holding Windows Runtime glue and nothing else, over a desktop tree that keeps its six libraries, its six test suites and its console executables; the `Game` library and `OutpostCapture` that makes possible, and the compile-time enforcement given up to get it | 2026-09-20 |
| [`ADR-015`](ADR-015-cppwinrt-dependency.md) | `Microsoft.Windows.CppWinRT` as a pinned NuGet dependency of the packaged executable alone: the first package in the tree, the amendment to R14 and §2 it forces, and the three settings it changes that are overridden back | 2026-09-20 |
| [`ADR-016`](ADR-016-package-identity-and-launch.md) | Package identity: `Paths` reduced to two roots each executable supplies for itself so that `Core` names no path API, `GameData` shipped in the package, `Mods` moved to the user's directory, launch options from the activation arguments, and the capture gate moved to `OutpostCapture` with its four assertions unchanged | 2026-09-20 |
| [`ADR-017`](ADR-017-core-window-pixels-and-lifetime.md) | The core window's pixels and its lifetime: the swap chain sized in physical pixels through one tested conversion, raw-pixel scaling asked for first, suspend and resume handled, device removal recovered, and the frame-time readout moved out of the title bar the app no longer has | 2026-09-20 |
| [`ADR-018`](ADR-018-client-server-libraries.md) | Six libraries on two axes -- layer and side: NeuronCore, NeuronClient and NeuronServer for the engine, GameShared, GameClient and GameLogic for the game; the edges between them; Tests/IntegrationTests as the one suite allowed both sides; and the five boundary defects the split found. Supersedes ADR-001's project table | 2026-09-20 |
| [`ADR-019`](ADR-019-the-client-never-simulates.md) | The client links no simulation and there is no local host, so ADR-012's interest set is enforced by the linker; the cost is that a player with one machine cannot play, because loopback is isolated for a packaged app against an unpackaged host | 2026-09-20 |

## When to write one

A decision is anything a future reader would otherwise re-litigate: a file format, a wire protocol, a subsystem's shape, a project added, an exception to a rule in `AGENTS.md`, a figure the design left to measurement (the tick rate, the authored resolution). It is written **in the same commit as the change that implements it**, and a design document the decision settles is updated in that commit to cite it. `TechnicalDesign.md` §12 lists the ones the work will meet, numbered as they are written.

Not every choice is a decision. A local naming choice, a refactor that changes no boundary, and anything `AGENTS.md` already settles need none.

## The file

`ADR-<nnn>-<slug>.md`: three digits and a kebab-case slug of the decision, `ADR-001-solution-layout.md`. Written in the voice of `AGENTS.md`: dated, owned, and stating what it forecloses as plainly as what it chooses. Figures are measured, not estimated, and say how they were measured.

## Template

```markdown
# ADR-<nnn> — <title>

**Status:** Accepted | Superseded by ADR-<nnn>
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

The figures the decision rests on, each with how it was measured: the command, the build, the
machine. If a figure is arithmetic rather than a measurement, say what it is arithmetic on.
```

## Superseding

A decision is never edited into a different decision. A new ADR supersedes it, the old one's status line points forward, and the old file stays: the history of why is the point.

## Where ADR-001 to ADR-012 came from

*Outpost Commander* is a hard fork of *Frontier Commander*, taken at its M1 state on 2026-09-20. **ADR-001 to ADR-012 came across with the code they describe**, renamed but otherwise unaltered, because a tree that copies the code and drops its decisions is a tree whose every boundary is about to be re-litigated by whoever forgot it. They are this repository's decisions now and are superseded here, not there.

Two consequences follow and are stated rather than left to be noticed. **The measurements in them were made in the other tree** — on its CI runs, its `windows-latest` images and the owner's machine — and where one cites a CI artefact the link points at *Frontier Commander*, deliberately, because that is where the evidence is. And **the numbering does continue that tree's**, which is the one place this repository departs from `AGENTS.md` §6: ADR-013 follows ADR-012 because the code arrived with twelve decisions already taken, and renumbering them from one would have broken every citation in the design documents and the task plans for no gain.

The UWP migration begins at ADR-013. `Design/UwpMigration.md` is the plan those five ADRs are the decisions of.
