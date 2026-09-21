# ADR-012 — A shader is compiled into a header, not shipped as a file

**Status:** Proposed — drafted 2026-09-21 against `Plan/README.md` F1, which reserved this number and
stated the recommendation. **The owner has not ruled**, and `Design/ADR/README.md` says a proposed ADR is
not something to write code against: M0.15 cites this record and waits on that word.
**Date:** 2026-09-21
**Owner:** Stefan Zwaal

## Context

R13's scaled present is a full-screen blit, so **M0.15 is the first HLSL in this tree** and there is no
shader build path anywhere in it. R14 closes the dependency list at the Windows SDK, the MSVC standard
library and `Microsoft.Windows.CppWinRT`; `fxc` and `dxc` both ship with the SDK, so **the compiler is not
the question.** What nothing answers is how the compiled result reaches the binary.

**The structural fact that decides it is a project-layout one, and it is easy to miss.** `NeuronClient` is
a **static library with no package of its own**. `OutpostCommander` is the only project in the tree that
produces a package. So a `.cso` on disk has to be declared as package content by a project the library
that uses it cannot see — the shader would live in `NeuronClient`, and the thing that carries it would be
an entry in `OutpostCommander.vcxproj` that nobody editing the renderer has any reason to look at. A
header has no such problem: it is linked into the library, and the library is linked into the package.

[`ADR-005`](ADR-005-meshes-are-generated-in-code.md) drew this same line one subsystem over and is worth
reading beside this one. It has no mesh file, no loader and no asset build step, for the same reason and
with the same trade: a geometry change is a code change the compiler checks.

**This is taken before the shader rather than after it.** The plan is explicit that the second shader is
the point of no return — by then the path is whatever the first one happened to do.

## Decision

**Shaders are compiled at build time by `fxc`, through MSBuild's `FxCompile` item, and the output is a C
header rather than a `.cso` on disk.** Concretely:

- **`HeaderFileOutput`** (`/Fh`), never `ObjectFileOutput` (`/Fo`). The generated header is `#include`d by
  the translation unit that creates the pipeline state, and the bytes are in the static library.
- **`VariableName`** (`/Vn`) is set explicitly, to `g_<shaderName>` — `AGENTS.md` §1's convention for a
  global. Left unset, `fxc` invents its own spelling from the entry point, and the one identifier in this
  arrangement that a human has to type would be the one nobody chose.
- **Shader Model 5.0**, which is what a blit needs and what `fxc` is for. See *Consequences* for the
  ceiling that accepts.
- **The generated header goes to `$(IntDir)` and is never checked in.** It is build output that happens to
  be text.
- **One entry point per `.hlsl` file**, named for what it is, so that a file's name and its shader's name
  are the same fact.
- **`.hlsl` files are project items of the library that uses them** — `NeuronClient` — with the
  `.vcxproj` and `.filters` entries `AGENTS.md` §2 requires of any file. `.editorconfig` already has an
  `[*.hlsl]` section waiting.

## Consequences

**The decisive gain is that no project has to know about a file it cannot see.** A shader is added by
adding two files to `NeuronClient`, and the package picks it up because it picks up the library. Nothing
in `OutpostCommander.vcxproj` changes, ever, for a shader.

**The gain nobody weighs until it bites is that there is no file to read at device creation.** A `.cso`
inside a package is reached through `Package.Current.InstalledLocation`, and **that API is asynchronous.**
Device creation happens on the frame thread, which is an ASTA, where blocking on an asynchronous
operation is a deadlock rather than a delay — the trap `NeuronClient/DatagramTransport.h` already names
for the socket and which would be met a second time here. So the `.cso` path does not merely cost a
packaging entry; it costs either an asynchronous state machine in the middle of device creation or a
worker thread and a handoff, for a sequence of bytes that was known at build time. **That is a great deal
of machinery to arrive at where `/Fh` starts.**

**A missing or broken shader becomes a compile error.** For a tree whose `AGENTS.md` demands the build
gate everything, a renderer that cannot start because a file was not packaged is exactly the failure this
avoids — and it is the failure that appears only on somebody else's machine.

**What it costs, and the first two are real:**

- **Changing a shader rebuilds every translation unit that includes its header, and relinks the package.**
  There is no hot reload and no swapping a shader on a deployed device. At the MVP's shader sizes that is
  seconds, but it is the iteration loop, and iteration loops are what people actually feel.
- **The bytes are in the binary.** A blit is nothing. A large permutation set would argue the other way,
  and that is one of the two things below that reopen this.
- **Shader Model 5.0 is a ceiling, and it is `fxc`'s.** `fxc` is in maintenance; Shader Model 6 and DXIL
  need `dxc`, which MSBuild's `FxCompile` item does not drive — reaching it means a custom build step.
  Nothing in the MVP's renderer wants a wave intrinsic or a 16-bit type, and D3D12 accepts DXBC from 5.x
  without complaint, so this is a ceiling the MVP does not touch. It is named because a ceiling nobody
  wrote down is one somebody walks into.
- **A generated header is not a tree file.** It is not in any `ClInclude` list, `.clang-tidy` never sees
  it, and `AGENTS.md` §1's naming table does not reach the symbol inside it. That is why `/Vn` is set
  rather than defaulted: it is the one place the convention has to be applied by hand.

**What reopens it:**

1. **A shader that needs Shader Model 6** — wave intrinsics, 16-bit types, anything DXIL-only. The answer
   is `dxc` behind a custom build step, and it is a second ADR rather than an edit to this one, because it
   changes the tool rather than the destination.
2. **Enough shader permutations that baking them all is a binary-size problem.** Then a `.cso` and a
   loader start to pay for themselves, and the packaging entry this record avoids becomes worth its cost.
3. **Wanting to iterate on a shader without a rebuild.** That is a real want on a project with an artist
   on it, and this record forecloses it for as long as it stands.

## Measurements

**One is owed, at M0.15**, and it is cheap: **the compiled size of the present pass's vertex and pixel
shaders**, from the generated headers. The binary-cost argument above is the only quantitative claim this
record makes, and it is currently an expectation rather than a figure — "a blit is nothing" should be a
number before the second reader has to take it on faith. `AGENTS.md` §6 asks for exactly that.
