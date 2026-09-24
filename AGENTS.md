# AGENTS.md — Engineering Rules for *Outpost Commander*

Operating instructions for every agent (and human) writing code in this repository. **Read this before generating a single line.**

*Outpost Commander* is a multiplayer-only real-time strategy game and a hobby project with one developer: a packaged UWP client and an authoritative Win32 host, built on Windows with MSVC. This file is about **how code is written here** — naming, layout, build settings and the standing rules of the codebase.

**All six libraries hold real code.** (Where each milestone stands, and what it owes, is stated once, in [`Design/Plan/README.md`](Design/Plan/README.md) *Where it stands* — `OpenQuestions.md` Q72.) `NeuronCore` has the byte codec, the packet header, fixed point, the vector, the sine table and the PRNG; `NeuronServer` the Winsock endpoint and the tick schedule; `NeuronClient` the `DatagramSocket` endpoint, the packet queue, the window metrics and fit transforms, the Direct3D 12 device and swap chain, the scene target and the scaled present, the interface pass, the gesture seam that is the only way in, the CMO reader and the instanced mesh pass, the star field and its point sprites, the glyph atlas and the text renderer; `GameCore` the entity, the component catalog, the design table and the derived-stat function, the wire records and the update, the command codec, the join, the starting layout, the field generator, and the one rule for where a module may go; `GameLogic` the world, the tick, the state hash, command intake, ring slot assignment, the uniform grid, the mining system and its unload target, the economy, the build system and what modules do, the replication accumulator, the session table and the host loop; `GameClient` the replica store and interpolation clock, the camera and its gesture, the tap resolver, selection and group selection, the derived field and its baked asteroids, the hull and module meshes, module placement, the order markers, the four panels and the credit flash, and the stress harness's policy, schedules and report.

**`GameClient` was the last one holding a function that returns its own name**, so that every edge of the build — every reference, every include path, every link — was exercised by something rather than asserted. It has real declarations now and the placeholder is gone.

**There is a game in it, and an interface over it.** Nothing below is a target to migrate towards — it describes the code as it must be written.

**Where these rules come from.** Naming, formatting and the compiler settings are carried over from two sibling repositories, `Outpost.Warzone` and `Nomad-Commander`, where they were measured against a large tree. That lineage is why [`.clang-format`](.clang-format) and [`.clang-tidy`](.clang-tidy) are what they are, and it is why code can move between the trees without a rename or a reflow pass. **What did not come across is the other trees' design or their decisions.**

**What is authoritative, in order:** this file, then [`Design/`](Design/README.md), then the surrounding code. For anything none of them covers, match the file you are editing.

**This file says *how* code is written; `Design/` says *what* is being built.** A design document never overrides an engineering rule and an engineering rule never decides a game mechanic. Where a design decision has to constrain the shape of code it becomes a rule here citing the design section that is its source — R22 to R24 are the three that have. Decisions taken while building are ADRs under [`Design/ADR/`](Design/ADR/README.md); questions the design has not answered go on [`Design/OpenQuestions.md`](Design/OpenQuestions.md) **before** the code that needs them is written, not after.

**Start at [`Design/README.md`](Design/README.md)**, then the milestone you are working on in `Design/GameDesign.md` §10.

If a rule here conflicts with a habit from another codebase, this file wins. If you think a rule is wrong or your task cannot be done without deviating, **say so in your report — never deviate silently.**

---

## 1. Naming convention (normative — no exceptions)

| Kind | Convention | Example |
|---|---|---|
| Type (class, struct, enum, concept, alias) | `PascalCase` | `SwapChainTarget` |
| Function, method | `PascalCase` | `PresentFrame()` |
| Member variable | `m_camelCase` | `m_deviceRemoved` |
| Static member (mutable) | `sm_camelCase` | `sm_activeDevice` |
| Global | `g_camelCase` | `g_instance`, `g_frameCount` |
| Parameter | `_camelCase` | `_fileName`, `_fleetId` |
| Local | `camelCase` | `shadedColor` |
| Compile-time constant | `UPPER_CASE` | `WIDTH_PIXELS`, `CELL_PIXELS` |
| Enumerator | `PascalCase` | `DeviceLost`, `OutOfVideoMemory` |
| Macro | `UPPER_CASE` | `OUTPOST_ASSERT` |
| Namespace | `PascalCase` | `Outpost` |
| File | `PascalCase.cpp` / `.h` | `SwapChainTarget.cpp` |

**Note the split that catches people out: a `constexpr` is `UPPER_CASE`, an enumerator is `PascalCase`.** They are both compile-time and they are spelled differently on purpose — an enumerator is a *value of a type* and reads as one at the use site (`PageFault::OutOfVideoMemory`), while a constant is a number with a name and is meant to look like one. [`.clang-tidy`](.clang-tidy) is the single source of truth for the option values; this document states the rules in prose and does not repeat the settings, so there is nothing to drift.

### The rules behind the table

**R1 — The leading underscore on parameters is deliberate.** It is legal C++: the reserved forms are `_Uppercase`, anything containing `__`, and `_lowercase` **at global scope**. A parameter is never at global scope, so `_fileName` is safe. Never introduce a reserved form — no `_Impl`, no `__helper`, no file-scope `_cache` (use `g_cache` in an anonymous namespace).

**R2 — A type name carries no prefix or affix, and that includes abstract ones.** An interface is `Transport`, not `ITransport`. A base class is not `BaseTransport` or `AbstractTransport`. PascalCase means the name and nothing else. This bans `CFoo`, `SFoo`, `EFoo`, `IFoo`, `FooBase`, `FooAbstract`, `FooImpl` and `_t` suffixes. Name the concept and let the concrete types say what they are:

```
Transport             ← the concept
├── UdpTransport      ← a socket-backed one
└── LoopbackTransport ← in-process, for tests
```

That tree is an illustration of the rule, not a description of anything. A base class for one derived class is ceremony: name the concept, and add the layer when a second thing needs it.

**R3 — Compile-time constants are `UPPER_CASE`.** `constexpr`, `inline constexpr` and `static constexpr` members: `WIDTH_PIXELS`, `TICKS_PER_SECOND`, `CELL_PIXELS`. `sm_` is reserved for *mutable* statics, which are rare and must document their thread-safety.

**R4 — Acronyms capitalize as words**: `HlslSource`, `DxgiFactory`, `UdpTransport` — never `HLSLSource`. Identifiers from an external SDK keep that SDK's spelling (`ID3D12Device`, `DXGI_FORMAT`, `HRESULT`, `IDXGISwapChain4`) and are never renamed to fit.

**R5 — Template parameters are PascalCase**: `T`, `Fn`, `BlockBytes`, `Ts...`.

**R6 — Units belong in names; types do not.** `fuelPerJump`, `upkeepCreditsPerDay`, `arrivalTick`, `confidencePercent` are encouraged — a simulation measured in ticks, credits and distances makes unit ambiguity a real defect class, and it is one the compiler cannot catch for you. Never encode the type: no `iCount`, `pFleet`, `strName`.

**R7 — A file is named for its primary type**, PascalCase, `.h` / `.cpp` only. `.hpp`, `.cc` and `.inl` are not used; template implementations live in the header. The exceptions, because MSBuild and the Visual Studio wizards spell them this way: `pch.h`, `pch.cpp`, `Resource.h`.

**R8 — `m_` marks encapsulated state, not every field.** A `class` with invariants prefixes private members `m_`. A public aggregate — a `Desc` config struct, a wire record, a POD handed to the renderer — uses plain `camelCase` fields so brace initialization reads naturally.

**R9 — One namespace per layer, and the engine does not know the game.** Engine code is `Neuron`; game code is `Outpost`. The split is a rule rather than a filing preference: if an engine type has to know a game concept by name in order to do its job, it is in the wrong layer. Test suites use `namespace <Project>Tests`.

**R10 — No `using namespace` at file scope in a header.** It leaks into every translation unit that includes it, and the failure it causes appears somewhere else. In a `.cpp` it is allowed for the unit-test framework and nothing else; otherwise qualify the name or write a local alias.

**R11 — One spelling per family, and it is the SDK's.** `color`, `initialize`, `serialize`, `normalize`, `quantize`, `synchronize`, `behavior`, `neighbor`, `center`, `gray`, `canceled`. Neither spelling is wrong English; the defect is a tree where a reader has to know which half they are in and a grep for one finds half the uses. `D3D12_CLEAR_VALUE::Color` settles which half wins.

**This now covers prose as well, and it did not used to.** R11 originally exempted documents — *"a document may spell `flavour` and `harbour`"* — on the grounds that only identifiers are grepped. That was wrong here for a reason specific to this tree: **the design documents quote identifiers constantly**, so the exemption put both spellings inside single sentences. `Design/Interface.md` said "the recognizer emits `Tapped`" three times over, naming a type that is spelled `GestureRecognizer`. A reader still has to know which half they are in; the boundary just moved from the file to the paragraph. **US spelling everywhere**, prose and identifiers alike. **Nothing sweeps it** — the script that did was deleted on 2026-09-22 (§2) — so R11 is review's, in prose and in identifiers.

**It is not a gate, and that was decided on 2026-09-22 after it had been one.** It walks every file in the tree — 3,940 of them, including the generated C++/WinRT projection headers — and takes **minutes** where every other check here takes about a second, which is long enough that people stop running it locally and it becomes a thing CI says no to after the fact. It is also mostly noise for the same reason: **101 of its last 105 findings were inside `Generated Files\winrt\`**, which is build output nobody here wrote and nobody can fix. So it is run occasionally and deliberately, by the owner, rather than on every push. **R11 is unchanged** — it is a rule, and a rule whose checker is not in CI is exactly the situation the rest of §1's table already describes for R2, R4, R6, R7, R9 and R10. **What would make it a gate again is teaching it to skip generated output**, at which point it would be both fast and quiet.

### Worked example — this is the target style

```cpp
// NeuronClient/SceneTarget.h
#pragma once

#include <cstdint>

namespace Neuron
{

// R3: constant → UPPER_CASE. R6: the unit is in the name.
inline constexpr std::uint32_t SCREEN_WIDTH_PIXELS = 1920;
inline constexpr std::uint32_t SCREEN_HEIGHT_PIXELS = 1080;

// Enumerator → PascalCase, unlike the constants above.
enum class TargetFault : std::uint8_t
{
  DeviceRemoved,
  BadFormat,
  OutOfVideoMemory
};

/// The color framebuffer the game draws into, and the depth buffer that goes with it.
/// R2: no prefix on the type. R8: private state carries m_.
class SceneTarget
{
public:
  struct Desc                                            // R8: aggregate → plain fields
  {
    std::uint32_t widthPixels;                           // R6: unit in the name
    std::uint32_t heightPixels;
    DXGI_FORMAT colorFormat;                             // R4: SDK spelling kept as-is
  };

  [[nodiscard]] static bool Create(ID3D12Device* _device,        // R1: _ on parameters
                                   const Desc& _desc,
                                   SceneTarget& _outTarget) noexcept;

  [[nodiscard]] std::uint32_t WidthPixels() const noexcept { return m_widthPixels; }

private:
  ID3D12Resource* m_depthTarget = nullptr;
  std::uint32_t m_widthPixels = 0;
  bool m_deviceRemoved = false;
};

} // namespace Neuron
```

### What is enforced, and what is not

| Rule | Enforced by |
|---|---|
| Layout and formatting (§4) | [`.clang-format`](.clang-format), **gated in CI** on a pinned version |
| Debug and Release agreeing (§3) | [`Scripts/CheckProjectFiles.py`](Scripts/CheckProjectFiles.py), **gated in CI** — the settings only. CI still builds `Debug\|x64` alone, so nothing compiles Release |
| Every library having a suite that runs (§2) | The CI test steps, which fail on a suite that did not build |
| The naming table, R1, R3, R5, R8 | [`.clang-tidy`](.clang-tidy) — **driven, reported, not gated**; see below |
| R2, R4, R6, R7, R9, R10, R11 | Review. Check your own diff against the table before handing it back. |

**`.clang-tidy` now has a driver, and it reports rather than gates.** [`Scripts/RunClangTidy.ps1`](Scripts/RunClangTidy.ps1) reads each project for its sources and include directories, follows the `.vcxitems` it imports, and runs clang-tidy in clang-cl driver mode. **CI runs it weekly and on demand — the `naming` job, Mondays at 06:00 UTC or from the Actions tab — and not on your push.** It was a step on the Windows build until 2026-09-22 and came off it for three reasons that are written at the job: it could not fail, its output was buried in the SDK-header warnings it analyzes but does not report, and being slow inside a job with a 30-minute timeout it could turn main red in the one way it was configured not to. That is deliberate and not timidity: **clang is not MSVC**, `/std:c++latest` is ahead of what clang implements, and the C++/WinRT headers lean on MSVC extensions — so a parse error there is a clang limitation, not a defect in this tree. Read its output — and read it in the weekly run, since nothing puts it in front of you at review time any more; treat a finding as a finding and a crash as a note to whoever next touches the script. **Until it has run clean on the runner once and been switched to `-Gate`, naming is still review's problem** — do not assume a green build says anything about it, and note that your push never ran it at all.

**What stands between it and `-Gate` is the noise, not the findings.** `.clang-tidy`'s `HeaderFilterRegex` limits what is **reported** and not what is **analyzed**, so every run walks the Windows SDK headers and generates tens of thousands of warnings it then discards. That is why it is slow and why its real diagnostics are hard to see. Quiet it and the same work makes it fast, readable and gate-able at once.

---

## 2. Repository shape

`OutpostCommander.slnx` at the root. **Six libraries on two axes, layer and side** — the prefix says which layer, the suffix says which side:

| | shared | client only | server only |
|---|---|---|---|
| **engine**, namespace `Neuron` | `NeuronCore` | `NeuronClient` | `NeuronServer` |
| **game**, namespace `Outpost` | `GameCore` | `GameClient` | `GameLogic` |

Three executables over them: **`OutpostCommander`**, the game — a packaged UWP application whose view is a `CoreWindow`, built from `GameClient` and `NeuronClient`; **`Server`**, the authoritative host — an ordinary Win32 console executable, built from `GameLogic` and `NeuronServer`; and **`Bot`**, the stress harness — an unpackaged desktop console executable that runs many headless clients from one process, built from the same `GameClient` and `NeuronClient` as the game ([`Design/ADR/ADR-022`](Design/ADR/ADR-022-a-bot-is-a-headless-client.md)). Each in a flat directory of its name, and one suite per library under `Tests/<Library>Tests`.

**The client links no simulation.** `GameLogic` is not in `OutpostCommander`'s reference set and its headers are not on the client's include path, so what a client may know is a boundary the linker keeps rather than a convention inside one address space. The game is multiplayer only and a player with one machine cannot play it; that is the cost, and it was taken deliberately.

**The edges run one way, and a layer never reaches sideways.** Engine code is built on by game code and never the reverse (R9), and two libraries at the same level share what is below them rather than each other. An edge that only exists "for now" is an edge, and it is the one that will be impossible to remove later.

### The two shared libraries are shared-items projects, and this is why

`NeuronCore` and `GameCore` are `.vcxitems`, not `.vcxproj`. They produce no `.lib` of their own: their sources are compiled **into** each project that imports them, with that project's settings.

That is not a filing preference. `NeuronClient` and `GameClient` are compiled for the **Windows Store** application type — AppContainer, `WINAPI_FAMILY_APP`, a restricted view of the Windows SDK — and `NeuronServer`, `GameLogic` and every suite are ordinary desktop builds. One static library cannot be both, and the direction that fails is exactly the one that matters: a desktop-compiled library linked into an AppContainer package compiles, links, and is refused at certification. A shared-items project compiles the shared code twice, correctly, once per side.

**Import each `.vcxitems` exactly once per link closure, or the same symbols arrive twice.** Who imports what:

| Project | imports | references |
|---|---|---|
| `NeuronClient` | `NeuronCore` | — |
| `GameClient` | `GameCore` | `NeuronClient` |
| `NeuronServer` | `NeuronCore` | — |
| `GameLogic` | `GameCore` | `NeuronServer` |
| `OutpostCommander` | — | `GameClient`, `NeuronClient` |
| `Server` | — | `GameLogic`, `NeuronServer` |
| `Bot` | — | `GameClient`, `NeuronClient` |
| `Tests/NeuronCoreTests` | `NeuronCore` | — |
| `Tests/GameCoreTests` | `GameCore`, `NeuronCore` | — |
| `Tests/NeuronClientTests` | — | `NeuronClient` |
| `Tests/GameClientTests` | — | `GameClient`, `NeuronClient` |
| `Tests/NeuronServerTests` | — | `NeuronServer` |
| `Tests/GameLogicTests` | — | `GameLogic`, `NeuronServer` |

`GameCoreTests` imports two because nothing below a suite over `GameCore` provides `NeuronCore` to it. Every other row imports at most one.

### Every library has a master include, and a pch that includes it

`NeuronClient.h` for `NeuronClient`, `GameCore.h` for `GameCore`, and so on. **A master include includes the master include of everything its library is built on**, so a consumer includes one file and gets the whole chain:

```
NeuronCore.h  ←  NeuronClient.h  ←  GameClient.h  ←  OutpostCommander/pch.h, Bot/pch.h
       ↑              ↑                  ↑
       └── GameCore.h ┘                  │
       ↑                                 │
       └── NeuronServer.h ← GameLogic.h ← Server/pch.h
```

Each project's `pch.h` includes **its own** master include and nothing else; `pch.cpp` includes `pch.h` and is the translation unit that creates the precompiled header. A suite's `pch.h` includes the master include of the library under test, and `<CppUnitTest.h>`.

**The two shared-items libraries carry no precompiled header.** Their `.cpp` files are compiled into six different projects, each with a pch of its own, and there is no one header to share between them. Those files are marked `PrecompiledHeader: NotUsing` in the `.vcxitems` and include their own master header first instead. That is the one exception to "`pch.h` comes first", and it is stated in the files themselves.

### Include directories

A project lists the directories of the **other** projects it reaches into, as `$(SolutionDir)<Project>`. A `.vcxitems` lists its own directory relative to itself, because it is evaluated inside whichever project imported it.

**A project does not put its own directory on the include path.** `cl.exe` already searches the directory of the including file first for a quoted include, so `#include "GameClient.h"` from a `.cpp` in the same folder resolves without help.

**No header is named like a C runtime or SDK header.** The other projects' directories sit on the include path ahead of the SDK, MSVC searches them for an angled include too, and it matches case-insensitively — a `NeuronCore/Assert.h` is what DirectXMath's `<assert.h>` would find.

**No identifier is spelled like a Windows SDK macro.** `<windows.h>` is in scope throughout this tree, and the preprocessor rewrites `near`, `far`, `pascal`, `cdecl`, `interface`, `small`, `hyper`, `IN`, `OUT`, `OPTIONAL`, `CONST`, `VOID`, `PURE`, `DELETE` and `IGNORE` before the compiler sees them.

### The rest of the shape

**Project directories are flat.** C++ source lives directly in its project's folder. This is not taste: `.clang-tidy`'s `HeaderFilterRegex` matches headers exactly one level in, so **a header in a subdirectory is silently unchecked** the day that gate is switched on. `OutpostCommander/Assets/` is the one subdirectory in the tree, and it holds package artwork rather than code.

**The project files are part of the source.** Adding, removing or moving a file means editing the owning `.vcxproj` or `.vcxitems` **and** its `.filters`. A file that compiles locally but is missing from the project fails only in CI — or worse, links a stale object nobody notices.

**A filter says what a file is for, never what its extension is.** There is no `Source Files` and no `Header Files` in any `.filters` in this tree, and there will not be one: a header and the `.cpp` that implements it are two halves of one idea and belong together, under the name of the thing they are. Sorting by extension separates them and says nothing a glance at the name would not have said. So a project starts with **no filters at all** — its files sit at the project root, which for a library holding one idea is the honest picture — and a filter arrives the first time there is a functional group worth naming. `OutpostCommander`'s `Assets` is the only one in the tree today; it names the package artwork, which is what those files are for, and it also has to exist because they live in a subdirectory.

**Visual Studio recreates `Source Files` and `Header Files` on its own**, every time *Add New Item* is used. Delete them again and drop the `<Filter>` element from the item; a file with none sits at the root, which is what is wanted. Nothing checks this for you.

**There is one package, and no vendored SDKs.** `Microsoft.Windows.CppWinRT`, at a pinned version, referenced through a `packages.config` by the **six projects on the C++/WinRT side**: `NeuronClient`, `GameClient`, `OutpostCommander`, `Bot`, and the two suites that link the first two. The Windows SDK ships the projection headers — a Windows Store project reaches `<winrt/Windows.UI.Core.h>` with no package at all, which is worth knowing before adding one — but not `cppwinrt.exe` or its MSBuild targets, which is what a project needs to author a runtime class of its own from IDL.

**Every packages.config in the tree pins the same version.** Six configs that disagree is a restore that fetches two copies of one package and a build that links whichever import path was written last. CI restores them by finding them, not by naming them, so a seventh project does not mean remembering to edit the workflow. Every other project depends on the Windows SDK and the MSVC standard library and on nothing else. A second package is a decision, not a convenience; see R14.

**`Scripts/` holds the checks, and the split in it is deliberate.** A **gate** is pass/fail, runs in CI and is named `Check*`: [`CheckProjectFiles.py`](Scripts/CheckProjectFiles.py) (§3's table), [`CheckDeterminism.py`](Scripts/CheckDeterminism.py) (R16), [`CheckDesign.py`](Scripts/CheckDesign.py) (figures and citations across `Design/`), [`CheckSuites.py`](Scripts/CheckSuites.py) (a suite that ran no test) and [`CheckHudGeometry.py`](Scripts/CheckHudGeometry.py) (the HUD's tiers, clear space and mirror, and the figures its `geometry.json` copies out of `Design/Interface.md` §1 — which `CheckDesign.py` does not reach). A **calculator** answers a question and has no verdict: [`DatagramBudget.py`](Scripts/DatagramBudget.py) costs a wire-format proposal. [`RunClangTidy.ps1`](Scripts/RunClangTidy.ps1) is neither yet — it reports, weekly and on demand rather than on a push (§1).

**`Check*` means gate again, with no exception to remember.** `CheckSpelling.py` was the one, de-gated and then deleted on 2026-09-22: it took minutes where these take a second, and 101 of its last 105 findings were in generated C++/WinRT output, so it was mostly reporting Microsoft's spelling back to us. R11 did not go anywhere (§1) — its checker did. A replacement is welcome and must skip generated output, or it will be deleted again for the same reason.

They live here rather than beside the skills under `.claude/` for one reason: **a build must not break because a skill was moved.** The skills hold the judgment — when a lever is worth its cost, where a decision goes, what a sweep cannot see — and point at these. Run them before you push; they are seconds, and they are what CI runs.

**Build and IDE output is never committed** — `x64/`, `ARM64/`, `AppPackages/`, `Generated Files/`, `packages/`, `.vs/`, `*.user`, and anything a build step generates.

---

## 3. Build and verify

**x64 and ARM64 are the platforms; there is no 32-bit anything.** No Win32/x86 configuration in any project or in the solution; do not add one, and do not write code that only works at 32 bits. `EnableEnhancedInstructionSet` is the one compiler setting that belongs to the platform rather than to the configuration — `AdvancedVectorExtensions2` on x64, `NotSet` on ARM64 — and it is stated per platform in every project file rather than inherited, because an MSVC default is not a decision.

**Debug and Release are aligned by rule, not by luck.** Every setting that is not *about* optimization reads identically in both — and in the project files it is written once, in one unconditioned `ItemDefinitionGroup`, so there is no second copy to drift. The two configurations differ in exactly this and nothing else:

| | Debug | Release |
|---|---|---|
| `UseDebugLibraries` | `true` | `false` |
| `WholeProgramOptimization` | — | `true` |
| `LinkIncremental` | `true` | `false` |
| `Optimization` | `Disabled` | `MaxSpeed` |
| `FunctionLevelLinking`, `IntrinsicFunctions` | — | `true` |
| preprocessor | `_DEBUG` | `NDEBUG` |
| `EnableCOMDATFolding`, `OptimizeReferences` | — | `true` |

Each of those lives in a `Condition="'$(Configuration)'=='Debug'"` or `'Release'` group, and **nothing else may.** If you find yourself adding a setting to one of those groups, you are either adding an optimization switch or breaking this rule.

**The compiler settings are the settings.** Toolset `v145` (Visual Studio 2026), `/std:c++latest`, `/permissive-`, `/W4` with **warnings as errors**, `/fp:precise`, SDL checks on. There is no CMake. If a build error tempts you to change the toolset, lower the language standard, turn off `/permissive-` or silence a warning — **stop and report instead.**

**The Windows SDK is pinned to `10.0.26100.0`, and the pin is not "whatever is newest here".** It is the SDK the CI runner has, and a developer machine with a newer one cannot see that: a pin of `10.0.28000.0` — the newest on the machine that wrote these files — failed every project on GitHub's image with `MSB8036` before a single file compiled, because `windows-latest` carries Visual Studio 2026 and exactly one SDK. **Raising this means checking the runner image first**: `actions/runner-images`, `images/windows/Windows2025-VS2026-Readme.md`, its Windows SDK list. The toolchain step of the workflow reads the pinned value back out of the project files and fails by name when the runner lacks it, so the next mismatch is one line in a log rather than a hunt. `WindowsTargetPlatformMinVersion` is a different thing — the oldest Windows the package will install on — and is `10.0.17763.0`.

**`OutpostCommander` is a packaged application and `Server` and `Bot` are not.** The client carries `ApplicationType` `Windows Store`, a `Package.appxmanifest` and package identity; the host is an ordinary console executable. **`Bot` is a desktop console executable with no manifest and no package identity** that links the two Windows Store client libraries, which works for the reason the client suites do (below). Having no identity, it cannot call the three `NeuronClient` functions that read `LocalState` — `ReadHostAddress`, `ReadSessionToken`, `WriteSessionToken` — and it needs no loopback exemption, so it runs against a host on the same machine from any shell (ADR-022). `NeuronClient` and `GameClient` are Windows Store static libraries, compiled against the app family; `NeuronServer`, `GameLogic` and every suite are desktop builds. **A Windows Store static library carries no Windows Runtime metadata here** — `GenerateWindowsMetadata` is `false` in both, because no library in this tree declares a runtime class, and the C++/WinRT package would otherwise feed mdmerge a `.winmd` that nothing writes.

**Running the game costs a deploy.** A packaged application is not started from a shell: it is built, deployed and launched, and in Visual Studio that is F5 on `OutpostCommander` with developer mode on. **A packaged client cannot reach a host on the same machine without a loopback exemption, which Visual Studio grants silently on every F5** — so same-machine play works on the box that built it and nowhere else. **Before you conclude otherwise, remove the exemption and try again.** [`Design/ADR/ADR-008`](Design/ADR/ADR-008-the-host-address-is-configuration.md) has the mechanics and why this is never a shipping configuration.

**Build through the solution, never a `.vcxproj` directly.** Output paths and cross-project include directories are anchored on `$(SolutionDir)`, and MSBuild defines `SolutionDir` only for a solution build. Building a project file directly resolves every one of those paths against the *project* folder instead. **It does not fail — that is the problem.** Output lands in the wrong folder, so the next solution build links against whichever copy is staler, and every cross-project include path becomes a directory that does not exist. To build one project, use `/t:<ProjectName>` on the solution.

```powershell
# From the repository root. /t:<Project> builds one project, still through the solution.
msbuild OutpostCommander.slnx /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo

# The other three pairs, which NOBODY ELSE BUILDS — CI builds Debug|x64 and compiles two suites for
# Debug|ARM64 (§6), so most breaks in these reach main green and are found by whoever ships. Run all three before you do.
foreach ($c in 'Release|x64','Debug|ARM64','Release|ARM64') { $p = $c -split '\|'
  msbuild OutpostCommander.slnx /p:Configuration=$($p[0]) /p:Platform=$($p[1]) /m /v:minimal /nologo }
```

**Run the tests**, through `vstest.console.exe`, over every suite the build produced. Every suite is an ordinary desktop test DLL and lands beside the other output at `<Platform>\<Configuration>\<Suite>.dll`, so running them costs no deploy:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vstest = & $vswhere -latest -products * -find '**\vstest.console.exe' | Select-Object -First 1
$suites = Get-ChildItem -Recurse -Filter '*Tests.vcxproj' | ForEach-Object { "x64\Debug\$($_.BaseName).dll" }
& $vstest $suites /Platform:x64 /Logger:"console;verbosity=normal"
```

**The two client suites are desktop test DLLs that link Windows Store libraries.** That direction works — the libraries were compiled against the narrower app family, so everything they call is available to a desktop host — and it is what buys a suite that runs in CI without an appx deploy, a developer-mode runner and a signing certificate. What it costs is stated plainly: **those suites do not compile or run under the UWP API family**, so they cannot catch an app-family violation. Nothing in this tree can; what stands in for it is the client libraries themselves being compiled for Windows Store, and the package build in CI.

**vstest reports "no tests found" as a pass.** An empty suite is therefore worse than no suite: it is a green check mark over a library nobody exercised. A new test project ships a placeholder `SuiteSmoke` for exactly this reason; delete it when the first real test lands, never before. **All six have reached that point** and none carries one now — `GameClientTests` was the last, and lost it when `GameClient` stopped being a shell.

**Check formatting before you push.** It is seconds, and it is what CI runs:

```powershell
python -m pip install clang-format==23.1.1     # the version CI pins
git ls-files '*.cpp' '*.h' | ForEach-Object { clang-format --dry-run --Werror $_ }
# clang-format -i <file> rewrites an offender.
```

**A green build says nothing about whether the game draws.** For anything touching rendering, input, audio or presentation, launch the executable and look at it.

**Report what you actually did.** "Builds clean, not run" and "builds and runs" are different claims. Never imply the second when you only did the first, and say which configurations you built.

---

## 4. Layout and formatting

[`.clang-format`](.clang-format) is the authority for C++ layout: 2-space indent, 140 columns, Allman braces, pointer and reference bound left, includes never reordered. [`.editorconfig`](.editorconfig) covers everything clang-format does not — CRLF, UTF-8, final newline, trailing whitespace, and the non-C++ formats — and repeats the two numbers an editor needs before the first save. [`.gitattributes`](.gitattributes) settles line endings per extension, so a Windows checkout and the Linux format job read the same bytes.

**This tree is formatted, and CI keeps it that way.** A whole-tree format check here is a no-op. Format what you write; if the check fires, run `clang-format -i` and commit the result rather than arguing with it.

- **Do not reformat what your task did not touch.** The check being green tree-wide means a drive-by reformat produces pure churn and buries your actual change.
- **Include order is load-bearing and grouped by hand**, which is why `SortIncludes` is `Never`: `pch.h` first, then this project's own headers, then other projects' master includes, then the SDK, then the standard library. A formatter reordering these behind a change's back is a correctness risk, not a style preference.
- **One header owns the Windows macro family, and nothing else defines any of it.** `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `NODRAWTEXT`, `NOBITMAP`, `NOMCX`, `NOSERVICE` and `NOHELP` are set in [`NeuronCore/NeuronCore.h`](NeuronCore/NeuronCore.h), before `<windows.h>`, and the project files deliberately define none of them. Two owners of one macro is C4005, and `/WX` makes that fatal — `/D` spells a bare macro as `1` where a `#define` spells it as nothing, so the collision is guaranteed rather than possible. If you need `<windows.h>`, you already have it: every `pch.h` in the tree reaches that header.
- Do not silence a diagnostic with `#pragma warning(disable: ...)` to make a build pass. Fix the cause, or report it.

---

## 5. Rules for this codebase

These bind code as it is written from here. Several describe subsystems that do not exist yet; they are here so that the first version of each is right rather than retrofitted.

**R12 — Graphics is Direct3D 12 only.** No D3D11, no D3D11On12, no immediate-mode helper layers. COM lifetimes are RAII from the first line — a raw `AddRef`/`Release` pair in new code is a defect, not a style. `winrt::com_ptr` is the smart pointer; WRL's `ComPtr` is not used.

**R13 — The client draws into a scene target and presents that, scaled.** Every pass draws into an off-screen color target at the resolution the game is authored for, and the frame ends by presenting that target into the swap chain's back buffer, fitted to the window's client area with the aspect ratio preserved: **1:1 and unfiltered when the client area already matches, point sampling at an exact integer multiple, bilinear otherwise, letterboxed.** Exactly one place asks the window how big it is, and that is it; every layout, every glyph and every integer position behind it is unconditional. A pass that branches on the window size has misunderstood this rule.

Two things bind anyone changing the authored resolution or the sample count:

- **A back buffer cannot be multisampled** — D3D12 supports only the flip-model swap effects and DXGI will not multisample a flip-model back buffer, so `SampleDesc.Count` must be 1. A *scene* target may be, and is resolved before the present step scales it. That is the main thing the indirection buys.
- **A scale is not free, and text is what it costs** — a glyph baked to an exact pixel height reaches the glass resampled unless the scale is exactly 1, which is why the 1:1 and exact-multiple paths exist and are worth keeping common.

Both are settled for this game in [`Design/ADR/ADR-007`](Design/ADR/ADR-007-the-authored-frame-is-1440x960.md), as amended by [`ADR-016`](Design/ADR/ADR-016-the-world-resolution-is-a-scale.md) — **the world's resolution is a scale of the panel, not a constant, and it defaults to 1:1.** Do not re-derive either here.

**One departure from "every pass" is recorded and is not a violation.** The *interface* draws after the scale, straight into the back buffer at physical resolution, laid out in authored coordinates through **a fit transform of its own** — so no pass branches on the window size and no layout number becomes conditional. The world still draws into the scene target. **There are two fit transforms and one place that computes them** ([`ADR-016`](Design/ADR/ADR-016-the-world-resolution-is-a-scale.md)): the world's maps the scene target into the back buffer and the interface's maps authored layout space into it. Sharing one value couples the interface's scale to the world's resolution, which is a defect and not a simplification — at a 1:1 world scale the shared value is identity and the whole interface lands at half size in a corner. [`ADR-011`](Design/ADR/ADR-011-the-interface-draws-after-the-scale.md) states why this rule's intent survives its letter being broken, and what would put the interface back.

**R14 — Two dependencies, both named, and the list is closed.** The Windows SDK and the MSVC standard library — with one named exception: **`Microsoft.Windows.CppWinRT`, a NuGet package**, pinned to an exact version, referenced by the six C++/WinRT projects §2 lists and by no others. The rule is one *package*, not one project: adding it to a seventh project on that side is a line in `packages.config` and four in the `.vcxproj`, while adding a second package is a decision. If you believe one is unavoidable, propose it in your report with what it buys and what it costs — do not add it.

For Direct3D that list means what the Windows SDK installs: `d3d12.h`, `dxgi1_6.h`, `DirectXMath.h`, `winrt/base.h` and the `fxc`/`dxc` compilers. It excludes what a D3D12 sample reaches for by reflex, because each is NuGet or GitHub content and not SDK content: the DirectX Agility SDK, DirectX-Headers, `d3dx12.h`, DirectXTK12, DirectXTex and the DirectX Shader Compiler as a redistributable.

**It binds what the executable is built from, not what a development tool needs.** A script that never ships and never links — clang-format itself, say — does not reopen this rule. **Third-party *content* is a different question and it is the owner's**: art, fonts and sound are content, not dependencies, and anything under a license needs the owner's approval before it lands, with the license text traveling with the bytes. **The owner has ruled, and the ruling is [`Design/ADR/ADR-021`](Design/ADR/ADR-021-content-ships-with-the-package.md): content files ship with the package.** What that does not move is this rule — a content *file* is not a dependency and a *loader library* is — and it does not move R16: simulation data stays `constexpr` in `GameCore` rather than becoming a file the two sides can disagree about.

**R15 — Memory is plain C++.** `new`/`delete` where it must be, RAII everywhere, standard containers by default. No pool, slab or free-list allocator without a decision recorded and a measurement behind it.

**R16 — Determinism is a property of the simulation, and it is built, not hoped for.** Every project compiles `/fp:precise`, stated explicitly rather than inherited, and identically in Debug and Release — which is the half of this that actually protects a replay.

**`/arch:AVX2` on x64 has a cost, and it is named rather than waved at.** It sets an AVX2 floor — Intel Haswell (2013) and AMD Excavator (2015); an older CPU meets an illegal instruction, not a message. That is the real cost of the switch and it is the whole of it.

**The simulation holds no floats, for three reasons:**

- **`sinf`, `cosf`, `sqrtf` and friends are CRT implementations and are not correctly-rounded.** Nothing specifies them bit-identical between x64 and ARM64, or across CRT versions. That alone ends cross-platform float determinism and no compiler switch reaches it.
- **ARM64 always has FMA**; x64 has it only under `/arch:AVX2`. Any explicit `std::fma`, or any library that contracts, differs by architecture whatever `/fp` says.
- **`/fp:precise` rounds to source precision at four named points** — assignments, typecasts, arguments passed, values returned — and explicitly permits *"intermediate computations ... at machine precision"* in between. Register allocation therefore reaches the result, and register allocation is what an optimization level changes.

Integers are reached by none of the three. Floats live in the renderer, where nothing is replayed. **[`Scripts/CheckDeterminism.py`](Scripts/CheckDeterminism.py) sweeps `GameCore` and `GameLogic` for what this rule forbids and is gated in CI**; it catches what is *written*, never what is *designed*, and the `determinism-audit` skill carries the six blind spots no sweep can see. **This rule once rested on FMA contraction under `/fp:precise`, which this toolset does not do**; [`Design/ADR/ADR-002`](Design/ADR/ADR-002-tick-and-numbers.md) records the correction and the citation.

Inside the simulation, additionally: no `float` where a fixed-point or integer quantity will do (hold a fraction as integer hundredths and say so in the name, R6), no iteration over an unordered container whose order reaches the outcome, and **no wall-clock time — the tick is the clock.** Wall time maps to ticks at the seam, and that is the only place the two meet. Randomness is a pinned PRNG seeded from the match — never `std::random_device`, never a hash of an address. None of this is taste: a simulation that cannot reproduce from its seed cannot be replayed, cannot be debugged from a report of what happened, and cannot be measured twice.

**R17 — A string you do not write is `const`.** `/permissive-` turns on `/Zc:strictStrings`: a literal is `const char[N]` and will not bind to `char*`. The fix is `const` on the signature, never a cast at the call site — a `const_cast` here is a lie about a literal that lives in a read-only section, and writing through it is a real crash rather than a theoretical one.

**R18 — The view is a `CoreWindow` and the loop owns the frame.** The client is a packaged UWP application driven by `IFrameworkView`, and one drain of the dispatcher per frame is the whole of its event handling. **There is no XAML anywhere in this tree** — no `SwapChainPanel`, no `.xaml` in any project, no XAML compiler step. The window's size is in device-independent pixels and reaches the swap chain through one conversion, which is a pure function in `NeuronClient` with a suite over it, because it is the one place this application model can silently produce a worse picture.

**R19 — The client never simulates, and nothing in it may.** `OutpostCommander` links no `GameLogic`. Client-side code interpolates records it was *sent*, and previews against rules that live in `GameCore` — the same rules the host validates with. If a client feature seems to need the simulation, it needs a record it is not being sent, and the answer is in the protocol, never a second copy of a rule.

**R20 — The packaged project holds Windows Runtime glue and nothing else.** No game logic, no arithmetic, no decision a test could pin lives in `OutpostCommander`; anything testable is pushed down into a library, which is what `GameClient` and `NeuronClient` are for. The same goes for `Server` and `Bot`. A thing an executable holds is a thing no suite can reach.

**R21 — Touch is the only input, and a gesture is the only way in.** `Windows::UI::Input::GestureRecognizer` is the one path from the `CoreWindow` into the game, and a `PointerPoint` whose `PointerDeviceType` is not `Touch` is dropped at the seam, at exactly one site. Keyboard events are not subscribed. Mouse and pen have no compatibility path, deliberately: an interaction is named in the recognizer's vocabulary — `Tapped`, `Holding`, and a manipulation's translate, scale and rotate — or it is not an interaction. **A double tap is `Tapped` carrying a count** (`TappedEventArgs.TapCount`, under `GestureSettings::DoubleTap`) rather than a fourth verb, and is used for group selection ([`Design/ADR/ADR-017`](Design/ADR/ADR-017-group-selection-is-a-double-tap.md)); it is affordable only because the first tap acts immediately and the second upgrades the result, so no tap ever waits to find out what it is. A key, a hover, a second button and a wheel are four things a finger does not have, and a feature that needs one needs a different design rather than a different device. **The client requires a touchscreen**, which is a requirement and not an oversight. The arithmetic under a gesture is a pure function in `NeuronClient` with a suite over it, for the same reason R20 gives: the sign of a pinch is a thing a package can hide and a test cannot.

**R22 — The simulation is two-dimensional.** Every entity's position is a point on one plane. There is no third coordinate in `GameCore` or `GameLogic`, none in any wire record, and none in any test fixture. The camera has three dimensions and the renderer may *draw* asteroids, wrecks and effects above and below the plane; nothing the simulation owns has a height, and the host does not know that anything is drawn off it.

This is not a preference about space games. **A tap is a ray, and a ray has no depth** — R21 has already refused the second input that would give it one, so a move order is one ray-plane intersection and cannot be anything else. A feature that needs a third coordinate needs a different input device, which R21 has settled. Source: [`Design/ADR/ADR-001`](Design/ADR/ADR-001-the-playfield-is-a-plane.md).

**R23 — The client derives the world from the seed; nobody sends it one.** The procedural generator lives in `GameCore` and both sides run it: the host to populate the match, the client to draw the same asteroids in the same places. No map is ever transmitted. **Its inputs are the seed and the player count** — a half is copied at two players and a quarter at four — and both arrive on the join reply and nowhere else ([`Design/ADR/ADR-013`](Design/ADR/ADR-013-a-client-is-told-which-player-it-is.md), amended at M2.3).

That is not the client simulating (R19). **A generator is a rule, and `GameCore` is where the rules both sides evaluate live** — the same arrangement that lets a client preview an order against the rules the host validates with. What the simulation *owns*, such as how much ore is left in an asteroid, is replicated like any other state. A generator that reaches a different answer on the two sides is a defect of the same class as a desynchronisation, so it obeys R16 in full: integers, fixed point, no unordered iteration, and the match's pinned PRNG. Source: [`Design/TechnicalDesign.md`](Design/TechnicalDesign.md) §3.

**R24 — A built thing is a composition, and every stat is derived.** Nothing in the simulation knows a ship type, a station or a base module by name. It knows a **design** — a hull, an optional drive, and a component in each of the hull's slots — referred to by identity. A station is a hull with no drive; a base module is a hull with no drive and one slot ([`Design/ADR/ADR-015`](Design/ADR/ADR-015-the-base-is-built-from-modules.md)). Every stat is computed by **one pure integer function in `GameCore`** with a suite over it: mass is the hull plus its contents, speed is thrust over mass, cost and build time are sums.

**A number baked onto a type is the defect this rule exists to prevent.** It is the one that makes research and a design interface impossible to add later without moving damage, cost, build time, mass, speed and the wire format in a single change — which is the definition of a change that never gets made. Source: [`Design/ADR/ADR-006`](Design/ADR/ADR-006-a-ship-is-a-composition.md).

**R25 and up are reserved.** A rule with no source behind it is a rule nobody can settle an argument with; do not invent one and do not import one from another tree. R22 to R24 each cite the design document or decision record that is their source, and that citation is what makes them rules rather than opinions — see §6.

---

## 6. Working rules

**Stay in scope.** Do what the task asks. Adjacent code that offends you is not part of the task — note it in your report and move on. Unrequested "while I was in there" changes are the main way a young tree acquires regressions it cannot bisect.

**Record decisions where the next person will read them.** An engineering decision — a file format, a wire protocol, a subsystem's shape, an exception to a rule here — belongs in writing, in the same commit as the change that implements it, and this file is where the standing ones live. Figures are measured, not estimated: if you quote one, say how you measured it. A decision nobody wrote down gets re-litigated every few months by whoever forgot it.

**What CI gates: `Debug|x64`, the format check, and a compile-only `Debug|ARM64` of the two simulation suites, and nothing else.** [`.github/workflows/build.yml`](.github/workflows/build.yml) is the definition and is not restated here — a prose copy of a workflow drifts, and this one already did. What matters is not the steps but **what they leave open**, which no file states for you:

- **Release.** Nothing compiles it, so a Release that quietly lost an include directory, sat on an older language standard, or breaks only under optimization reaches `main` green. §3's table is the rule it is still expected to obey; the only thing that checks it is you, before you ship.
- **ARM64.** Since `Design/OpenQuestions.md` Q76 (ruled by the owner 2026-09-24), CI **compiles** `GameCoreTests` and `GameLogicTests` for `Debug|ARM64`, which is where the determinism pins live — compile only, because the runner is x64. Nothing compiles the client, the host or `Release|ARM64`, and nothing **runs** anything on ARM64, so an ARM64-only break outside those two suites, or one that only shows when the code runs, still reaches `main` green — most plausibly something assuming x86-family intrinsics, since `EnableEnhancedInstructionSet` is the one setting that differs by platform. **The target device is an ARM64 part** (`Design/ADR/ADR-007`), so this is the platform the game is for, and the four-pair run by hand is still what checks it.

The owner decided this scope: the Windows build is the slow half of the pipeline and each extra pair roughly doubles it. **The cheap half of both gaps is now closed by a static check rather than a second build.** [`Scripts/CheckProjectFiles.py`](Scripts/CheckProjectFiles.py) reads every `.vcxproj` and asserts §3's table in about a second, gated in CI, so configuration drift — a setting that wandered into a conditioned group, a lowered language standard, a platform inheriting `EnableEnhancedInstructionSet`, an SDK pin past the runner's — is caught here.

**It compiles nothing, and that is the whole of what it does not do.** A Release that breaks only under optimization and an ARM64 that assumes an x86 intrinsic reach `main` green exactly as before. §7 still carries that, and the pull request template still asks whether you built the other three pairs; answer it honestly.

**A red toolchain step is not something to work around.** CI fails by name when the runner lacks the pinned toolset or SDK, deliberately: lowering the pin to get past it is not a fix (§3).

**Commits and PRs.** Branch off `main`; small, focused commits with an imperative subject describing the change, not the process. One change per PR. CI must be green. **A WIP commit is squashed before merge** — a commit named for the process rather than the change cannot be bisected to (`OpenQuestions.md` Q72). Never commit build output, `.vs/`, `packages/` or `.user` files.

---

## 7. Before you hand work back

**Always:**

- [ ] Naming conforms to §1 — `_` on parameters, `m_` on class state, `UPPER_CASE` constants, `PascalCase` enumerators, no `I`/`C`/`Base` affixes. **Nothing checks this for you.**
- [ ] Only the lines the task required were changed; no reformatting, no drive-by fixes.
- [ ] No new third-party dependency and no second NuGet package (R14); every `packages.config` still pins one version.
- [ ] The format check passes, and so do the gates: `python3 Scripts/CheckProjectFiles.py`, `Scripts/CheckDeterminism.py`, `Scripts/CheckDesign.py`, `Scripts/CheckHudGeometry.py`. They are seconds and they are what CI runs. **There is no spelling check to run** (§1, §2); spell what you write correctly, US spelling in prose and identifiers alike.
- [ ] It builds `Debug|x64`, and every test suite runs and passes.
- [ ] **CI builds nothing else and runs nothing on ARM64** (§6), so `Release|x64`, `Debug|ARM64` and `Release|ARM64` were built and their suites run locally — or your report says plainly that they were not.
- [ ] Your report states what you verified, what you assumed, and any rule here you had to bend.

**If you touched `Design/` or this file:**

- [ ] `Scripts/CheckDesign.py` is clean — figures agree across every document that states them, citations resolve, links resolve.
- [ ] Each decision went to exactly one of: a rule here (citing its source), an ADR, `Design/OpenQuestions.md`, or nowhere. The `design-consistency` skill has the four-way.

**If you added, removed or moved a file:**

- [ ] It is in the owning `.vcxproj` or `.vcxitems` **and** its `.filters`.
- [ ] No `.filters` gained a `Source Files` or `Header Files` filter — Visual Studio adds them back on its own (§2).

**If you added a library:**

- [ ] It has a master include, a `pch.h` that includes it, a suite under `Tests/`, and **a name in [`.clang-tidy`](.clang-tidy)'s `HeaderFilterRegex`** — that pattern names each library explicitly, so a library missing from it has every header silently unchecked.
- [ ] A new `.vcxitems` import was added to exactly one project per link closure (§2).

**If you touched a project file:**

- [ ] No `ConformanceMode`, `LanguageStandard`, `WarningLevel` or `TreatWarningAsError` changed, and no warning silenced with a pragma.
- [ ] Debug and Release still differ in exactly the rows of §3's table and nothing else — `Scripts/CheckProjectFiles.py` asserts this, so run it rather than reading.

**If you touched the simulation or the wire format:**

- [ ] No third coordinate reached either (R22); no map was transmitted that the seed already derives (R23); no ship stat was baked onto a type rather than derived (R24).
- [ ] Nothing in the client links the simulation (R19), and nothing was put in an executable that a suite could have covered (R20).
- [ ] **`Scripts/CheckDeterminism.py` is clean, including `--review`**, and the judgment calls it reports were answered rather than dismissed — a sort's comparator is a total order on entity identity, a draw comes from the match's engine. A clean sweep is half of R16; the `determinism-audit` skill has the other half.
- [ ] **If a datagram moved, `Scripts/DatagramBudget.py` was run** and its figures — not estimates — are in the report and in `Design/TechnicalDesign.md` §4. The headroom is double digits and nothing in the build fails when it is gone.

**If you touched rendering, input, audio or presentation:**

- [ ] It was **run**, not just built — which for the client means deployed as a package and launched. Input is touch, because there is no other kind (R21); on a machine without a touchscreen, say what stood in.
