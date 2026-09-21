# ADR-005 — Meshes are generated in code; the MVP has no content pipeline

**Status:** Accepted — **amended 2026-09-21 by
[`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md)**, which makes the backdrop near-black with a
stated luminance ceiling rather than an absence, and **by
[`ADR-015`](ADR-015-the-base-is-built-from-modules.md)**, which added a sixth thing to draw after this
record said there were five. The shape list in *Context* is corrected below and the decision is untouched.
Originally ruled 2026-09-20 following an adversarial review, **with changes**: the silhouette consequence
is named, and the mesh function must be parameterised for divergent proportion rather than only for size.
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

`AGENTS.md` R14 closes the dependency list at the Windows SDK, the MSVC standard library and
`Microsoft.Windows.CppWinRT`, and says plainly that a second package is a decision rather than a
convenience. **A mesh format is the single most common place a graphics project reaches past that line
without noticing** — glTF wants a JSON parser, FBX wants the SDK, and every D3D12 sample in existence
pulls DirectXTK12 for the loader it happens to ship with.

The MVP needs five distinct shapes. **This record originally listed them as three ship hulls, a station
and an asteroid, and that was wrong in both directions within a day.**
[`ADR-015`](ADR-015-the-base-is-built-from-modules.md) made a module a separate destroyable entity, which
has to be drawn; and `GameDesign.md` §10 cut the `Cruiser` from the MVP, so nothing builds it and nothing
draws it. **The five are `Scout`, `Frigate`, `ModuleFrame`, the station and the asteroid** — still five,
and not the same five. The `Cruiser` stays a parameterisation the shared function must reach at M4 rather
than a shape the MVP emits.

## Decision

**Meshes are functions.** A hull is a function that emits a few dozen triangles — a fuselage, an engine
block, wings — parameterised so that **every hull in the catalog shares the code that shapes them**,
including the two the MVP never draws. The asteroid is the
same function driven by the match PRNG so that no two rocks are identical.

**Normals are baked per face onto split vertices.** That is flat shading, which is what a low-polygon
faceted look wants; there is no smoothing group to decide, no tangent basis to get wrong and no vertex
cache to optimize at this scale.

**Team color is a vertex attribute** selecting between a hull palette and the owner's color, so one
instanced draw covers every ship of a hull regardless of owner.

There is **no mesh file, no loader, no asset build step and no texture** in the MVP.

## Consequences

**What this buys:** no third-party dependency, no file format to version, no asset step in CI, no
importer to debug, and a geometry change that is a code change the compiler checks. For a tree whose
`AGENTS.md` demands the build gate everything, that last one is worth more than it looks.

**The sharpest cost is a silhouette problem, and it lands on the feel the design sells.**
`Design/GameDesign.md` §1 promises "ships that bank as they turn and read as silhouettes", and
`Design/Interface.md` §5 couples pitch to zoom so the tactical view is near top-down. **Two hulls emitted
by one shared parameterised function will tend to be the same shape at two scales**, which is exactly
unreadable at the zoom where identification matters most. The function must therefore be parameterised
for *divergent proportion* — a wide flat hull against a long narrow one — and not merely for size. If that
fails at M2 the answer is a shape-coded overlay, not more triangles.

**Beyond that it costs how the game looks.** A ship will look like a few dozen triangles because it is a
few dozen triangles, and no amount of care in the function changes that ceiling. The MVP's job is to prove
the loop, and a faceted low-polygon fleet in silhouette against black is at least a coherent look rather
than an apologetic one — but it is a ceiling and it is low.

**That backdrop is no longer literally black** ([`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md)),
and this paragraph is why that ADR carries a luminance ceiling rather than a taste. Black was doing work
here: it is what lets a few dozen triangles read as deliberate. The sky is therefore capped at **12% of
full white over any large area**, with only the brightest eight stars reaching 45% over a few dozen
pixels. If the fleet stops reading at the tactical zoom, that ceiling is the first number to move — not
the triangle count.

It also means **there is no way for anyone but a programmer to change a ship's shape**, which forecloses
art as a parallel activity for as long as it stands.

**What reopens it:** the first ship that needs to look like something a function cannot describe. The
answer then is a small hand-rolled binary format — a vertex count, an index count and two arrays — written
by a script in `Build/` and read by about forty lines in `GameClient`. **That is still no dependency**, and
it should be reached for before any library is.

## Measurements

None, and none is owed. This is a decision about where the dependency line sits, and R14 already drew it.
