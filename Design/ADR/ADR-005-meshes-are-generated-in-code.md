# ADR-005 — Meshes are generated in code; the MVP has no content pipeline

**Status:** Proposed
**Date:** 2026-09-20
**Owner:** proposed by the design; not yet ruled

## Context

`AGENTS.md` R14 closes the dependency list at the Windows SDK, the MSVC standard library and
`Microsoft.Windows.CppWinRT`, and says plainly that a second package is a decision rather than a
convenience. **A mesh format is the single most common place a graphics project reaches past that line
without noticing** — glTF wants a JSON parser, FBX wants the SDK, and every D3D12 sample in existence
pulls DirectXTK12 for the loader it happens to ship with.

The MVP needs five distinct shapes: three ship hulls, a station and an asteroid. That is the whole of it.

## Decision

**Meshes are functions.** A hull is a function that emits a few dozen triangles — a fuselage, an engine
block, wings — parameterised so that the three hulls share the code that shapes them. The asteroid is the
same function driven by the match PRNG so that no two rocks are identical.

**Normals are baked per face onto split vertices.** That is flat shading, which is what a low-polygon
faceted look wants; there is no smoothing group to decide, no tangent basis to get wrong and no vertex
cache to optimise at this scale.

**Team colour is a vertex attribute** selecting between a hull palette and the owner's colour, so one
instanced draw covers every ship of a hull regardless of owner.

There is **no mesh file, no loader, no asset build step and no texture** in the MVP.

## Consequences

**What this buys:** no third-party dependency, no file format to version, no asset step in CI, no
importer to debug, and a geometry change that is a code change the compiler checks. For a tree whose
`AGENTS.md` demands the build gate everything, that last one is worth more than it looks.

**What it costs is how the game looks.** A ship will look like a few dozen triangles because it is a few
dozen triangles, and no amount of care in the function changes that ceiling. The MVP's job is to prove
the loop, and a faceted low-polygon fleet in silhouette against black is at least a coherent look rather
than an apologetic one — but it is a ceiling and it is low.

It also means **there is no way for anyone but a programmer to change a ship's shape**, which forecloses
art as a parallel activity for as long as it stands.

**What reopens it:** the first ship that needs to look like something a function cannot describe. The
answer then is a small hand-rolled binary format — a vertex count, an index count and two arrays — written
by a script in `Build/` and read by about forty lines in `GameClient`. **That is still no dependency**, and
it should be reached for before any library is.

## Measurements

None, and none is owed. This is a decision about where the dependency line sits, and R14 already drew it.
