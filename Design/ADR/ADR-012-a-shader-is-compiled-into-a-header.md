# ADR-012 — A shader is compiled into a header by `dxc` at Shader Model 6.7

**Status:** Accepted — ruled 2026-09-21. The recommendation in `Plan/README.md` F1 was `fxc` to a `/Fh`
header; **the ruling kept the header and changed the compiler**, taking Shader Model 6.7 and therefore
`dxc`, and fixed the file layout and the symbol name.
**Date:** 2026-09-21
**Owner:** Stefan Zwaal

## Context

R13's scaled present is a full-screen blit, so **M0.15 is the first HLSL in this tree** and there was no
shader build path anywhere in it. R14 closes the dependency list at the Windows SDK, the MSVC standard
library and `Microsoft.Windows.CppWinRT`; `fxc` and `dxc` both ship with the SDK, so **the compiler was
never the question.** What nothing answered is how the compiled result reaches the binary.

**The structural fact that decides that is a project-layout one, and it is easy to miss.** `NeuronClient`
is a **static library with no package of its own**. `OutpostCommander` is the only project in the tree
that produces a package. So a `.cso` on disk has to be declared as package content by a project the
library that uses it cannot see — the shader would live in `NeuronClient`, and the thing carrying it would
be an entry in `OutpostCommander.vcxproj` that nobody editing the renderer has any reason to open.

[`ADR-005`](ADR-005-a-mesh-is-a-cmo-file.md) drew this same line one subsystem over and is worth
reading beside this one: it keeps R14 closed against a mesh *library* while ruling that a mesh is a CMO
*file* with a reader written here. **It used to argue the opposite** — a mesh was a function, and a
geometry change was a code change the compiler checked — and this record is unaffected either way, because
the argument below is about a static library with no package and an asynchronous read on the ASTA, neither
of which is a question about whether files are allowed. **The two records diverge deliberately and both
say why:** content is what a non-programmer changes, and a shader is not.

## Decision

**Shaders are compiled at build time into a C header, by `dxc`, at Shader Model 6.7, through MSBuild's
`FxCompile` item.** The Visual Studio project system drives it; there is no custom build step and no
script. Concretely:

| | |
|---|---|
| **Source** | `$(ProjectDir)Shaders\` — `NeuronClient\Shaders\PresentVS.hlsl` |
| **One file per stage** | `<name>VS.hlsl` and `<name>PS.hlsl`. The stage is in the file name, not a second entry point |
| **Entry point** | `main`, in every file. The file says which shader it is; the entry point never has to |
| **Target** | Shader Model 6.7 — `vs_6_7`, `ps_6_7` |
| **Output** | `HeaderFileOutput` = `$(ProjectDir)CompiledShader\%(Filename).h`, **and that header is checked in** |
| **Symbol** | `VariableName` = `g_p%(Filename)`, so `PresentVS.hlsl` yields `g_pPresentVS` |
| **`ObjectFileOutput`** | **empty, deliberately** — see below |

**`ObjectFileOutput` must be suppressed explicitly, and this is the trap in the decision.** Left at its
default, `FxCompile` emits a `.cso` *as well as* the header **and the build copies it into the package
layout** — the exact path this record declines, arriving silently alongside the one it chose. That was
observed rather than reasoned about: the first build produced `PresentVS.cso` in
`x64\Debug\OutpostCommander\`, and clearing the property made the next build delete it.

**Shaders are compiled identically in every configuration** — no embedded debug information, optimizations
on, in Debug as in Release. **This follows from the header being checked in at a configuration-independent
path.** `$(ProjectDir)CompiledShader\PresentVS.h` has no `$(Configuration)` in it, so a Debug build and a
Release build write *the same file*; if they wrote different bytes the file would flip back and forth with
whatever was built last, and whichever landed in a commit would be an accident. Compiling identically
makes the checked-in bytes one fact. **Verified:** Debug and Release produce byte-identical headers.

**`WindowsTargetPlatformMinVersion` is raised to `10.0.22621.0`**, in all three C++/WinRT projects and in
`Package.appxmanifest`'s `TargetDeviceFamily`. Shader Model 6.7 shipped in-box with Windows 11 22H2; the
projects previously claimed `10.0.17763.0`, which promised machines the shaders cannot run on.

**`NeuronClient\CompiledShader\.clang-format` sets `DisableFormat: true`.** The generated headers are
tracked, so `AGENTS.md` §3's format gate walks them, and no `dxc` output will ever satisfy a style. The
guard lives beside the files rather than as an exclusion every caller of the gate has to remember.

## Consequences

**No project has to know about a file it cannot see.** A shader is added by adding one file to
`NeuronClient\Shaders` and one item to its `.vcxproj`. Nothing in `OutpostCommander.vcxproj` changes,
ever, for a shader.

**There is no file to read at device creation, and that is worth more than the packaging entry.** A `.cso`
inside a package is reached through `Package.Current.InstalledLocation`, and **that API is asynchronous**,
while device creation happens on the frame thread — an ASTA, where blocking on an asynchronous operation
is a deadlock rather than a delay. `NeuronClient/DatagramTransport.h` already names that trap for the
socket, and the `.cso` path would meet it a second time. It would cost an asynchronous state machine in
the middle of device creation, or a worker thread and a handoff, for bytes that were known at build time.

**A missing or broken shader is a compile error.** The failure a packaged `.cso` produces — a renderer
that will not start because a file was not packaged — appears only on somebody else's machine.

**Shader Model 6.7 is a high floor, and it was measured rather than assumed.** The development machine is
a Snapdragon X with an Adreno X1-85 — the same GPU family as the Surface Pro 11 this game targets — and
`CheckFeatureSupport` reports **Shader Model 6.7** on it. That is the evidence the floor is safe on the
hardware that matters. What it costs is everything below Windows 11 22H2, which the raised minimum now
states honestly instead of leaving as a contradiction between the manifest and the shaders.

**The checked-in headers are large text.** `PresentVS.h` is 52 KB of source for 3,036 bytes of DXIL;
`PresentPS.h` is 60 KB for 4,056. Every shader edit is therefore a large diff of hex and disassembly
comments, and a reviewer reads the `.hlsl` rather than the header. In exchange the compiled bytes are in
the repository: a build needs no `dxc`, and what shipped can be read without rebuilding it.

**There is no hot reload.** Changing a shader rebuilds every translation unit that includes its header and
relinks the package. At these sizes that is seconds — but it is the iteration loop, and iteration loops
are what people actually feel.

**A generated header is not an ordinary tree file.** It is not in any `ClInclude` list, `.clang-tidy`'s
`HeaderFilterRegex` does not reach one folder down, and `AGENTS.md` §1's naming table does not govern the
symbol inside it. That is why `VariableName` is set rather than defaulted: it is the one place the naming
convention has to be applied by hand.

**What reopens it:**

1. **A shader large enough, or a permutation set wide enough, that baking it into the binary is a size
   problem.** Then a `.cso` and a loader start to pay for themselves.
2. **Wanting to iterate on a shader without a rebuild**, which is a real want the day anyone but a
   programmer touches one.
3. **Hardware that matters and does not reach Shader Model 6.7.** The floor is defensible because the
   target device clears it; a second target that does not would move this, and the answer is a lower
   `ShaderModel` rather than a different destination.

## Measurements

**Taken, and this record states no figure it did not measure:**

1. **Shader Model 6.7 is available on target-class hardware** — Adreno X1-85 on a Snapdragon X, reported
   by `D3D12_FEATURE_DATA_SHADER_MODEL`. `NeuronClient/GraphicsDevice.h` exposes it so the claim stays
   checkable rather than becoming folklore.
2. **The compiled sizes**, from the generated headers. `PresentVS` 3,036 bytes and `PresentPS` 4,056
   bytes of DXIL: the binary cost of baking a blit in is four kilobytes, so "a blit is nothing" is now a
   figure. **M1.9 added a third pair** and it is the largest so far — `ShipVS` **5,068 bytes** and
   `ShipPS` **4,924** — for the instanced hull draw with its two-segment palette ramp and two lights.
   **Eight stages come to about 32 KiB of DXIL across four checked-in header pairs**, against headers
   that are 144 KiB of source text; the text is the cost of this decision and the DXIL is the thing.
3. **Debug and Release produce byte-identical headers**, which is what makes checking them in coherent.
4. **A pipeline state built from this DXIL is created on the device, and draws.** Taken at **M0.15** on the
   development machine — the Snapdragon X and Adreno X1-85 of measurement 1 — where
   `NeuronClient/PresentStep.cpp` built a graphics pipeline state from both checked-in headers and the
   packaged client then presented frames continuously from it. Compiling at 6.7 and the device reporting
   6.7 were two facts; **the driver accepting the blob was the third and it is no longer owed.** What is
   still unmeasured is any other machine, which is what the raised
   `WindowsTargetPlatformMinVersion` is for.
