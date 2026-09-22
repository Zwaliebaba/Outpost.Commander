# ADR-021 — Content ships with the package; the line was drawn against dependencies, not against files

**Status:** Accepted — ruled 2026-09-22.
**Date:** 2026-09-22
**Owner:** Stefan Zwaal
**Updates:** [`ADR-005`](ADR-005-meshes-are-generated-in-code.md), which no longer claims the MVP has no
content pipeline, and [`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md), which no longer rests on
a painted cubemap being unloadable. **Both were edited rather than superseded**, under the rule the same
ruling set: through the MVP a conflicting record is updated in place, because two records that disagree
cost more than the history of the disagreement buys (`ADR/README.md`, *Superseding, and the MVP
exception*).

## Context

ADR-005 was titled "Meshes are generated in code; **the MVP has no content pipeline**" and ended on "There
is no mesh file, no loader, no asset build step and no texture in the MVP", while
[`TechnicalDesign.md`](../TechnicalDesign.md) §7 restated it under the heading "Content, and why there is
no content pipeline". Read together, those were taken as a standing prohibition on shipping any file at
all — everything in the binary, nothing beside it. **Both have been corrected as part of this ruling**;
what follows is why they were wrong to read that way in the first place.

**Neither of them argued that.** ADR-005's whole case is R14's dependency line: a mesh format is "the
single most common place a graphics project reaches past that line without noticing — glTF wants a JSON
parser, FBX wants the SDK, and every D3D12 sample in existence pulls DirectXTK12 for the loader it happens
to ship with." Its Measurements section says so outright: *"This is a decision about where the dependency
line sits, and R14 already drew it."* It even named its own exit — a small hand-rolled binary format
written by a script in `Build/` and read by about forty lines in `GameClient` — and said of it, in bold,
"**That is still no dependency**."

**And R14 never banned content.** It says the reverse in as many words: *"Third-party content is a
different question and it is the owner's: art, fonts and sound are content, not dependencies."* The
owner's ruling was owed from the day that sentence was written, and this record is it.

**There was never a packaging obstacle either.** `OutpostCommander` is a packaged application: a file
declared in the project installs with it. `Server` is an ordinary Win32 executable and reads from its own
directory. Neither has ever lacked a way to carry a file; each simply had no file to carry.

So what was being obeyed is a prohibition nobody wrote. It felt like a rule because two documents state
the *absence* of a pipeline as though it were a constraint, when it was a description of what the MVP
happened to contain.

## Decision

**Content files ship.** Meshes, textures, fonts, audio and any other presentation content may be authored
as files, carried in the package, and loaded at runtime.

**Three boundaries do not move, and each is a different rule than the one being lifted.**

**1. R14 still closes the dependency list.** A content *file* is not a dependency; a *loader library* is.
No glTF loader, no FBX SDK, no DirectXTK12, no DirectXTex. A format used here is one whose reader is
written here, or one the Windows SDK already reads — WIC for images, Media Foundation for audio, both
inside R14 already. ADR-005's hand-rolled "a vertex count, an index count and two arrays" is still the
cheapest thing that works and is still the first thing to reach for.

**2. Nothing the simulation reads becomes a file.** The component catalog, the designs and the damage
table stay `constexpr` tables in `GameCore` (`TechnicalDesign.md` §7). A table the host and the client can
disagree about is a desync with a file format in front of it, and R16 and R23 are what it would break.
**Presentation may be data; rules stay code.** Moving that line is a separate decision and this is not it.

**3. Shaders stay headers** ([`ADR-012`](ADR-012-a-shader-is-compiled-into-a-header.md)). Nothing here
touches its reasoning: `NeuronClient` is a static library with no package of its own, so its `.cso` would
have to be declared by a project its owner never opens, and a packaged `.cso` is read through an
asynchronous API on the ASTA. That record was ruled on its own merits and stands.

**Content is not loaded on the frame thread, and this is the trap in this decision.**
`Package.Current.InstalledLocation` is **asynchronous** and the frame thread is an ASTA, where blocking on
an asynchronous operation is a deadlock rather than a delay. ADR-012 records that as observed rather than
reasoned about, and `NeuronClient/DatagramTransport.h` already names it for the socket. Content is
therefore loaded **before the frame loop begins, or on a worker thread with a handoff** — never by
blocking a frame. Whoever writes the first loader meets this before they meet anything else.

**A licensed asset travels with its license**, which R14 already requires: the owner approves it before it
lands, and the license text sits beside the bytes.

## Consequences

**Art becomes a parallel activity, and that is the whole of why this is worth taking.** ADR-005 named the
cost it was accepting — *"there is no way for anyone but a programmer to change a ship's shape, which
forecloses art as a parallel activity for as long as it stands."* This buys that back.

**Nothing changes today.** Meshes are still generated in code, the sky is still generated from the seed,
and there is no loader to write against. **This record removes a prohibition; it does not order a
pipeline.** Until somebody writes the first loader, ADR-005's surviving half describes the tree exactly.

**Two arguments elsewhere stopped resting on this and now rest on themselves, which is why both records
were edited rather than annotated.** ADR-005's sharpest cost — that one shared function tends to emit the
same shape at two scales, unreadable at the zoom where identification matters most — is now answerable
with a modeled hull rather than only with a shape-coded overlay. And
[`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md) claimed a painted cubemap "could not be loaded
even if one existed"; that decision survives on its other grounds, which are that a seeded sky costs no
wire bytes, cannot desync anything, never updates, and saves 6.3 MB in the package.

**What it costs is the failure ADR-012 declined, and it is accepted here rather than waved at.** A file
format to version, an asset step somebody has to keep green in CI, an importer to debug — and a class of
failure that appears on somebody else's machine instead of at compile time, which is a file that did not
get packaged. That argument was right for shaders and it is a real cost for content; the difference is
that content is what a non-programmer changes, and shaders are not.

**What would reopen it:** nothing likely. A prohibition removed is not one to reinstate — if content costs
more than it buys, the answer is to ship less of it rather than to ban files again.

## Measurements

None, and none is owed. This is a decision about where a line sits, and the two lines it does not move —
R14's dependency list and R16's determinism — are drawn elsewhere and are unchanged by it.
