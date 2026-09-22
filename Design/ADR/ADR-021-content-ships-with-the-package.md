# ADR-021 — Content ships with the package; the line was drawn against dependencies, not against files

**Status:** Accepted — ruled 2026-09-22.
**Date:** 2026-09-22
**Owner:** Stefan Zwaal
**Updates:** [`ADR-005`](ADR-005-a-mesh-is-a-cmo-file.md), which no longer claims the MVP has no
content pipeline, and [`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md), which no longer rests on
a painted cubemap being unloadable. **Both were edited rather than superseded**, under the rule the same
ruling set: through the MVP a conflicting record is updated in place, because two records that disagree
cost more than the history of the disagreement buys (`ADR/README.md`, *Superseding, and the MVP
exception*).

**Later the same day the owner went further with `ADR-005` and reversed it**: a mesh is a CMO file, and
that record was replaced in place rather than superseded. **This one is unaffected** — it removed a
prohibition and ADR-005 is what spends it — but every sentence below that describes ADR-005 describes the
record as it stood on the morning of 2026-09-22, and says so where it matters.

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
to ship with." Its Measurements section said so outright — *"This is a decision about where the
dependency line sits, and R14 already drew it"* — a sentence the replaced record no longer carries,
because the record that replaced it takes a different decision on the same line. It even named its own
exit — a small hand-rolled binary format
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
inside R14 already. **That boundary is what makes CMO affordable**: ADR-005 now rules that a mesh is a
CMO file *and* that its reader is written in this tree, because CMO's only reader in the wild is the
DirectXTK12 this paragraph closes. Its hand-rolled "a vertex count, an index count and two arrays"
remains the cheapest thing that works and is now the fallback rather than the first reach — ADR-005 says
what would send the tree back to it.

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

**This record removes a prohibition; it does not order a pipeline** — and it was written expecting that
nothing would change for a while. **[`ADR-005`](ADR-005-a-mesh-is-a-cmo-file.md) ordered one the same
day**, ruling that a mesh is a CMO file, so the first loader is owed at M1.9 rather than whenever somebody
wanted one. The sky is still generated from the seed and
[`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md) is unmoved.

**Two arguments elsewhere stopped resting on this and now rest on themselves, which is why both records
were edited rather than annotated.** ADR-005's sharpest cost — that one shared function tends to emit the
same shape at two scales, unreadable at the zoom where identification matters most — became answerable
with a modeled hull rather than only with a shape-coded overlay, **and that is the opening ADR-005 then
took**. And
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

**The line itself needs none** — it is a decision about where a boundary sits, and the two boundaries it
does not move, R14's dependency list and R16's determinism, are drawn elsewhere and unchanged by it.

**What it does owe is the check it names**, and M1.9 is when content first had to be read at runtime:

### The package carries them — 2026-09-22, M1.9

The thirteen `.cmo` files and `manifest.json` are declared as package content and appear in the build's
own `.appxrecipe`, each mapped from its source path to `Assets\Meshes\<name>.cmo` inside the package.
The registered install location carries all fourteen, **444 KiB**, and the client reads three of them and
draws them.

**THIS IS NOT THE CHECK THIS RECORD ASKED FOR AND THAT IS STATED PLAINLY.** ADR-021's named failure is
**a clean install on a machine that did not build it**, and this was a loose-file registration of a build
output on the machine that produced it — the one arrangement in which a missing payload declaration
cannot show. What has been proved is that the declaration exists and that the runtime path finds the
files; what has not is the thing the record was written about.

**What makes the failure survivable rather than silent** is that a hull with no mesh draws M0.21b's
generated arrow and the client logs the mesh by name. An install that did not carry the files shows a
field of arrows and a log that says which ones, instead of a window that never appears.
