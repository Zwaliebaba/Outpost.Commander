# ADR-014 — The platform boundary: one packaged executable over a desktop tree

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

`ADR-013` makes the game a packaged UWP application. It does not say **how much of the tree is packaged**, and that is a separate decision with a much larger blast radius, because MSBuild's `ApplicationType` of `Windows Store` is not a packaging switch — it defines `WINAPI_FAMILY_APP`, and the Windows SDK headers then **hide the declarations of every API outside the UWP surface**. A project built that way cannot call `GetModuleFileNameW` or `CreateWindowExW` because those functions do not exist as far as the compiler is concerned.

That is a genuinely useful property and it is not free. Three arrangements were available:

- **The whole tree packaged.** Every project carries `ApplicationType`, and the app-container boundary is enforced by the compiler on the first line rather than by review or by store certification. The cost lands on everything that is not the game: the six native MSTest DLLs of `Tests/` become UWP test app packages, deployed and run through an app container; `OutpostHost`, a console executable with no console under UWP, cannot exist at all; and the CI capture gate — the strongest quality mechanism this tree has — has to be re-plumbed through `IApplicationActivationManager::ActivateApplication`, package deployment and a scrape of the package's `LocalState`, on a hosted runner, with every one of those a new way for a green build to go red for reasons that are not the code's.
- **Libraries built both ways.** One source list, a desktop configuration and a UWP configuration per library. It buys compile-time enforcement *and* working test suites, and it costs a doubled build matrix in a CI job that is already the slow half of the pipeline, plus a second axis for `Build/CheckProjectFiles.py` to hold the projects to.
- **One packaged executable over a desktop tree.** Only `OutpostCommander` carries `ApplicationType`; everything it links stays a desktop static library. The tests, the validator and the capture keep working unchanged. The boundary is not enforced by the compiler.

**The thing that settles it is the capture gate.** `.github/workflows/build.yml` runs a 9,000-tick scripted match on WARP and then asserts, from the run's log, that a shot was in flight in at least one frame, that no frame drew fewer than a hundred interface quads, that a device was selected, and that the scripted commander researched something. Those four assertions are what stand between this tree and a green build over a game that does not work, and `AGENTS.md` §3 says so in as many words. **That path has no window and no swap chain** — `App::RunCapture` writes the resolved scene target to disk and never presents — so packaging it buys nothing and risks all four.

## Decision

**Exactly one project is packaged: `OutpostCommander`.** It carries `ApplicationType` `Windows Store`, the `AppxManifest.xml` and the package assets. Every other project in the solution is a desktop project, built with the same toolset, the same `/std:c++latest`, `/permissive-`, `/W4 /WX`, `/fp:precise` and `/arch:AVX2` the tree has always used.

**Everything in `OutpostCommander` is Windows Runtime glue and nothing else.** The view source, the framework view, the event subscriptions, the package paths and the entry point. It holds no game logic, no arithmetic and no decision a test could pin. The rule is checked rather than remembered: `Build/CheckProjectFiles.py` gains a rule that refuses any `.cpp` in the packaged project that does not name a `winrt::` type or `CoreApplication`, and a size ceiling on the project.

**Two projects are added to make that possible.**

| Project | Kind | Namespace | Holds |
|---|---|---|---|
| `Game` | static library | `Outpost` | What the executable holds today: the application, the frame assembly, the camera controller, the HUD, the match, the local host, the operator and the starting base — everything that was unreachable from a test because it lived in an Application |
| `OutpostCapture` | executable, console | `Outpost` | The headless path: `--warp --capture <landscape> <ticks> <directory>`, and nothing else. What CI drives |

`Game` gets `Tests/GameTests`, which is the reason to want it independent of this migration: `Hud.cpp` is 70 kB and `Operator.cpp` 18 kB of code that no suite has ever been able to reach, and `NeuronClient/PointerMode.h` already carries a comment explaining that its contents were pushed down into a library for exactly this reason.

**`Game` does not name a window.** `RunWindowed` takes a presentation seam — the client size in physical pixels, a way to say the title has changed, and a pump that returns false when the user has left — and the packaged executable supplies it over a `CoreWindow`. Nothing above that seam knows which application model it is running under, which is what makes `OutpostCapture` possible at all.

**The three edges are one way, as `AGENTS.md` §2 requires.** `Game` builds on all six libraries; `OutpostCommander` and `OutpostCapture` build on `Game`; nothing builds on either executable. `OutpostHost` is unchanged and still builds on `Core`, `Content`, `Sim` and `Net` alone.

## Consequences

**What this costs, stated plainly: the UWP API boundary is not enforced by the compiler anywhere in this tree.** A library can call `GetModuleFileNameW` and it will compile, link into the package, and fail or be refused at certification rather than at build. That is the whole of the price, it is a real one, and three things are put against it rather than pretending otherwise:

1. **`Core` is the only library that ever called one.** `Neuron::Paths` used `GetModuleFileNameW` and `%LOCALAPPDATA%`; `ADR-016` removes both and leaves `Core` naming no path-producing API at all. After that task, a `grep` over the six libraries for the Win32 process, window and environment entry points returns nothing, and `Build/CheckProjectFiles.py` keeps it returning nothing.
2. **The existing `platform-header` rule already covers four of the six.** `Sim`, `Content`, `Net` and `Replica` include no Windows, D3D, socket or WinRT header at all, checked in CI since `ADR-001`. Only `Core` and `Client` can reach the platform, and `Client` reaches only D3D12 and DXGI, which are in the UWP surface entire.
3. **The package build is a CI gate.** Building the `.msix` is what catches an import the app container forbids, and it is cheap relative to the capture job.

**What it forecloses.** A second packaged executable — a separate tool app, a launcher — would need its own manifest and identity, and the rule that the packaged project holds only glue means such a tool cannot grow logic of its own. It also forecloses ever moving the capture into the package as a debug mode, because the capture is now a different executable; if that is ever wanted, it is a new decision.

**What would reopen it.** Store certification refusing the package over an import from a library, which would make the compile-time boundary worth its cost; or a second packaged target, which would make the build-both-ways arrangement cheaper than maintaining two glue layers.

**What this makes better, unrelated to UWP.** `Tests/GameTests` is new coverage over 90 kB of code that has never had any, and `OutpostCapture` gives the capture gate an executable whose only job is the capture — today the same binary carries the whole game, and a crash in the windowed path can take the gate with it.

## Measurements

**Arithmetic on this tree, not a measurement of a build.** The code that moves into `Game` is seven `.cpp`/`.h` pairs, counted with `stat` over `OutpostCommander/` at `ec702e2`: `App` (73.9 kB), `Hud` (80.2), `Operator` (27.0), `Match` (18.8), `LocalHost` (12.2), `StartingBase` (10.4) and `CameraController` (6.3) — **228.8 kB over 14 files**. What stays in the packaged executable is `Main.cpp` (1.1 kB today) and `pch.{h,cpp}` (0.4 kB), plus the framework view, which is new.

**The Win32 calls the six libraries make**, counted by `grep` over `Core`, `Content`, `Sim`, `Net`, `Replica` and `Client` at `ec702e2` for the Win32 process, window, environment and raw-input entry points: **three in `Core`** (`GetModuleFileNameW`, `GetEnvironmentVariableW` twice, and `OutputDebugStringA` twice, which is in the UWP surface and stays), and **the window and raw-input files in `Client`**, which `ADR-013` deletes. `Sim`, `Content`, `Net` and `Replica`: none.

The claim that `ApplicationType` `Windows Store` sets `WINAPI_FAMILY_APP` and hides non-UWP declarations is documented (MSB8020, *Modify Visual Studio*) and **unverified on this tree**; nothing in this ADR depends on it being true, because this ADR is the decision not to rely on it.
