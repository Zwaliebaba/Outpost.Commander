# ADR-015 — C++/WinRT arrives as a NuGet package: the first exception to R14 that is a package

**Status:** Accepted; amends [`AGENTS.md`](../../AGENTS.md) R14 and §2
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

`AGENTS.md` R14 is a closed list: the Windows SDK, the MSVC standard library, and `d3dx12.h` as a single pinned file with its licence beside it. §2 says it in the negative — "**There are no vendored SDKs and no package manager**" — and §5 goes further and names the C++/WinRT NuGet package by name among the things a D3D12 sample reaches for that this tree does not take, on the ground that `winrt/base.h` is SDK content and needs none of it.

That last sentence was true for the Win32 tree and it stops being true here, but **not for the reason it looks like**. The Windows SDK does ship the full C++/WinRT Windows projection — `winrt/Windows.ApplicationModel.Core.h`, `winrt/Windows.UI.Core.h` and the rest are under `Include\<version>\cppwinrt\winrt\`, and a UWP app could in principle be written against them with no package at all. **What the SDK does not ship is the build integration**: `cppwinrt.exe`, the MSBuild props and targets that run it, and the generated-projection machinery that a project needs the moment it consumes a `.winmd` that is not the Windows one. The SDK's copy is also pinned to the SDK's version, which moves when the SDK moves and not when the tree decides.

**The owner's decision of 2026-09-20 is that the package is included.** This ADR records what that buys, what it costs, and the three settings it fights with — because two of those three are settings `AGENTS.md` §3 says to stop and report rather than change.

## Decision

**`Microsoft.Windows.CppWinRT` is a dependency of `OutpostCommander` and of no other project.** It is referenced by the packaged executable alone, pinned to an exact version in that project's `packages.config` or `PackageReference`, and restored by the build. The six libraries and the three console executables reference no package and keep R14 as written.

**The version is pinned and the pin is stated in this ADR when `P1` fixes it**, in the same way `ADR-004` pins `d3dx12.h` to a DirectX-Headers release. A floating version is not a dependency, it is a moving one.

**The three settings the package changes are overridden back, in the project file, with the override commented.** The package's props are documented to set the C++ language standard to `/std:c++17`, to add `/bigobj`, and to add `/await`. Against this tree:

| What the package sets | What this tree requires | Resolution |
|---|---|---|
| `LanguageStandard` = `stdcpp17` | `/std:c++latest` (`AGENTS.md` §3, "the compiler settings are the settings") | **Overridden back to `stdcpplatest` in the project file**, after the package's props import so the override wins |
| `/await` | Nothing; and the switch is **deprecated as of Visual Studio 2026 (18.0)** and slated for removal, with `/await:strict` or standard C++20 coroutines named as the replacements | **Removed.** The game uses no `co_await`; if one ever appears, it is a C++20 coroutine and needs no switch |
| `/bigobj` | Nothing it conflicts with | **Kept.** The generated projection headers genuinely produce large objects, and the switch changes no code generation |

**A fourth collision is expected and is not yet resolved.** The generated `winrt/*.h` headers under `/W4` with **warnings as errors** are not known to be clean, and `AGENTS.md` §4 forbids silencing a diagnostic with a pragma to make a build pass. If they warn, the answer is `/external:I` over the generated-projection directory with `/external:W0` — treating them as third-party headers, which they now are — and **not** a lowered warning level, a `/wd` and not a pragma. `P1` finds out which, and this ADR is amended with the answer.

## Consequences

**§2's flat statement is now false and is amended**, not quietly: the tree has a package manager, it has exactly one package, and the sentence becomes a rule about which project may hold one.

**The build gains a network dependency it did not have.** `nuget restore` runs before the compile, and a restore that cannot reach the feed fails the build. The Win32 tree could be built from a clean clone with no network at all; this one cannot, for the one project. CI already has network, so this costs CI nothing and costs an offline developer the package cache.

**R14's argument is weakened and is worth restating rather than eroding.** The rule's point was never that dependencies are bad — it is that a dependency is a decision, that a closed list makes each one visible, and that "the sample does it" is not an argument. `d3dx12.h` came in as a pinned file with a hash. This one comes in as a versioned package with build integration, which is a larger thing, and it is admitted because `ADR-013`'s application model needs the toolchain and not because it was convenient. **The list is still closed.** A second package is a second ADR.

**What would reopen it.** The SDK shipping the build integration, which would make the package redundant; or the package's props becoming unoverridable on the language standard, which would put it in direct conflict with a rule `AGENTS.md` §3 says to stop and report over rather than bend. In that second case the answer is to drop the package and write against the SDK's projection headers directly, accepting the SDK-pinned version — which is why `P1` proves the override works before anything else in the plan is built.

**What it does not change.** `winrt::com_ptr` and `winrt::check_hresult` are what `AGENTS.md` R12 and R14 already require for COM lifetimes, and they came from the SDK's `winrt/base.h`. Every project keeps using them from the SDK; only the packaged executable gets the projection through the package.

## Measurements

**Nothing here is measured, and the decision is taken without measurement.** Every claim about what the package sets is read from the *Introduction to C++/WinRT* documentation and the package's own readme; the `/await` deprecation is from the Visual Studio 2026 (18.0) release notes, "C++ feature deprecations and removals". **None is confirmed against a build on this tree.**

`p1-uwp-shell/P1` measures all of it, and until it has run this ADR should be read as a decision with its arithmetic owed:

- the exact package version, and whether it carries `cppwinrt.exe` for the pinned Windows SDK;
- whether `LanguageStandard` set after the package's props import survives to the compiler command line, checked by reading `/std:` out of the build log rather than by the build succeeding;
- whether `/await` is still added when the property is cleared, and whether its removal is clean under `v145`;
- whether the generated projection headers compile at `/W4 /WX` and, if not, the exact warning numbers, so that the `/external:` answer is scoped to them and not to a directory of unknown contents.
