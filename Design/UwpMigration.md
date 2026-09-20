# Migrating to UWP and the core window

**What this document is.** *Outpost Commander* is a hard fork of *Frontier Commander*, taken at its M1 state on 2026-09-20, and the fork exists for one reason: the game is to run as a **packaged UWP application whose view is a `CoreWindow`**, where the tree it came from runs as a Win32 desktop executable over an `HWND`. This is the plan for getting from one to the other — what changes, what does not, in what order, and what could still go wrong.

The decisions it implements are [`ADR-013`](ADR/ADR-013-uwp-application-model.md) to [`ADR-019`](ADR/ADR-019-the-client-never-simulates.md), taken by the owner on 2026-09-20. The work is [`tasks/p1-uwp-shell.yaml`](../tasks/p1-uwp-shell.yaml), eleven tasks in seven waves. This document is the narrative those are the decisions and the schedule of; where they disagree with it, they win.

**Read the honesty note first.** Nothing about the UWP half of this plan has been built. The tree has no Windows machine in reach, so every platform claim here is read from Microsoft's documentation and **none is confirmed against a compiler**. That is why the first task measures instead of building, why five of its acceptance lines are numbers rather than behaviours, and why five of the seven ADRs carry a **Measurements** section that says, in as many words, that it has nothing in it yet. `AGENTS.md` §3 asks for the difference between "builds clean, not run" and "builds and runs" to be stated. For the shell this is the third case: **neither, yet**. For the library restructure of [`ADR-018`](ADR/ADR-018-client-server-libraries.md), which has landed, it is a fourth: **checked but not compiled** — `Build/CheckProjectFiles.py` verifies every edge, every reference, every include directory and every quoted include against the table, and no compiler has seen any of it.

---

## 1. The surface, measured

The Win32 coupling in this tree is **four files deleted outright and a handful of named call sites**. `NeuronClient/Window.{h,cpp}` and `NeuronClient/RawMouse.{h,cpp}` are 457 lines by `wc -l`; the rest is the swap chain's creation call, two calls in `App.cpp`, `Main.cpp` entire (33 lines) and three calls in `NeuronCore/Paths.cpp` (113 lines). Everything else — the fourteen-stage tick, the pathing, the replication protocol, all seven render passes, the interface — names no windowing API at all.

| What it does today | Where | What replaces it |
|---|---|---|
| `WNDCLASSEXW`, `CreateWindowExW(WS_POPUP)`, `PeekMessageW` loop, `WM_*` to `InputEvent` | `NeuronClient/Window.{h,cpp}`, 387 lines | `IFrameworkViewSource` + `IFrameworkView` in the packaged executable; `CoreDispatcher::ProcessEvents(ProcessAllIfPresent)` in place of `Window::Pump`; `CoreWindow::KeyDown`/`KeyUp`/`CharacterReceived`/`PointerPressed`/`PointerReleased`/`PointerMoved`/`PointerWheelChanged` in place of the window procedure's switch |
| `RegisterRawInputDevices`, `GetRawInputData` on `WM_INPUT` | `NeuronClient/RawMouse.{h,cpp}`, 70 lines | `MouseDevice::GetForCurrentView().MouseMoved`, whose `MouseDelta` is the same relative count with the same guarantees — unaccelerated, unclamped by the screen edge. **The closest mapping in the whole migration** |
| `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` | `NeuronClient/Window.cpp` | `ApplicationViewScaling::TrySetDisableLayoutScaling(true)`, falling back to converting `CoreWindow::Bounds` from DIPs ([`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md)) |
| `CreateSwapChainForHwnd`, `MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER)` | `NeuronClient/SwapChain.cpp` | `CreateSwapChainForCoreWindow`; the association goes, there being no `HWND` and no DXGI full-screen toggle |
| `ShowCursor(TRUE/FALSE)` for aim mode | `App.cpp` | `CoreWindow::PointerCursor = nullptr` and back, which is the documented pairing with `MouseMoved` |
| `SetWindowTextW` — the frame-time readout | `App.cpp` | The HUD. There is no title bar ([`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md)) |
| `wWinMain` + `CommandLineToArgvW` | `OutpostCommander/Main.cpp` | `CoreApplication::Run(ViewSource())`; arguments from `LaunchActivatedEventArgs::Arguments()` ([`ADR-016`](ADR/ADR-016-package-identity-and-launch.md)) |
| `GetModuleFileNameW`, `GetEnvironmentVariableW(L"LOCALAPPDATA")` | `NeuronCore/Paths.cpp` | Nothing: `Paths` becomes two roots each executable sets for itself, so `NeuronCore` names no path API at all ([`ADR-016`](ADR/ADR-016-package-identity-and-launch.md)) |

`OutputDebugStringA` in `NeuronCore/Log.cpp` and `NeuronCore/Assertion.cpp` stays — it is in the UWP surface.

**Nothing in the game layer appears in that table**, and that is not luck: `ADR-001` forbade the portable libraries a platform header and `Build/CheckProjectFiles.py` has been failing the build over it since M0. The rule was written for portability between layers and it has just paid for a platform migration.

---

## 2. The tree, before and after

Two decisions shape it. [`ADR-014`](ADR/ADR-014-platform-boundary.md) puts exactly one project inside the package and leaves everything else a desktop project. [`ADR-018`](ADR/ADR-018-client-server-libraries.md) splits the six libraries by **side** as well as by layer, and [`ADR-019`](ADR/ADR-019-the-client-never-simulates.md) then takes the simulation out of the client's binary altogether.

```
              before                                  after
  Core        static lib                NeuronCore    engine, shared
  Content     static lib                NeuronClient  engine, client
  Sim         static lib                NeuronServer  engine, server
  Net         static lib                GameShared    game,   shared
  Replica     static lib                GameClient    game,   client
  Client      static lib                GameLogic     game,   server

  OutpostCommander  exe, Windows subsystem, links everything
                 -> OutpostCommander  packaged UWP, links no simulation
  OutpostHost       exe, console               unchanged
                 -> OutpostCapture    exe, console: the capture CI drives, both sides in one process

  six suites -> seven: one per library, plus Tests/IntegrationTests, the only
                       suite allowed to see both halves
```

**The split is what makes the security boundary real.** `ADR-012` calls the interest set "a security boundary", but until now `OutpostCommander` linked `Sim`, because `LocalHost.cpp` ran a host in-process for single player — so the boundary was a convention inside one address space, which for an RTS is the oldest cheat there is. Now `GameLogic` is not in the client's reference set, `Sim.h` is not on its include path, and the checker fails the build if either changes.

**It cost single player.** Loopback is isolated for a packaged application, and an unpackaged host cannot be exempted in any shipping configuration, so the game is multiplayer only and a player with one machine cannot play it. [`ADR-019`](ADR/ADR-019-the-client-never-simulates.md) states the price in full and the two alternatives that were refused.

**The split also found five defects nobody had seen**, because a boundary expressed as a project is checked and a boundary expressed as a rule is remembered: the renderer was built on the game's content tables (an R9 break `ADR-001`'s table permitted); `Victory.h` declared two functions over a `Sim&` while its enum travelled on the wire; `ClientHistory.h` was the server's and sat with the client's; `LandscapeGenerator` is shared, because the client generates terrain from the definition it receives; and `Movement` is not vocabulary at all. `ADR-018` has the detail and the measurement.

**What it has not fixed yet.** `GameShared` holds **19 headers that came out of `Sim`**, `World.h` and `StateHash.h` among them, because `Placement.h` takes a `World&` and `Landscape.h` includes `StateHash.h`. Those two includes drag six more behind them. `P2` removes all three and sends eight headers back; until it lands, a `GameClient` translation unit can name `World`.

---

## 3. The order, and why it is that order

Seven waves. Every edge is a thing that genuinely cannot start until another finishes, which is the rule `ImplementationPlan.md` §3 puts on the task graph.

**`P1` — prove the ground, and alone.** One throwaway UWP project that opens a core window, makes a D3D12 device, creates a swap chain for it, clears to a colour and presents. It answers five questions, and **the plan changes shape if any answers wrong**: whether `v145` exists as a UWP toolset and survives the tree's setting block; whether the `Microsoft.Windows.CppWinRT` package's `/std:c++17` stays overridden, read off the compiler command line rather than inferred from success; whether the generated `winrt/*.h` headers compile at `/W4 /WX`; whether `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` is accepted at *creation* on a core-window chain; and whether the CI runner carries the UWP C++ workload at all. It also confirms the loopback isolation `ADR-019` rests on — **if a packaged client can reach an unpackaged host on one box, that ADR is reopened at once**, because the cost it accepts exists only if the constraint is real.

**`P2` — tighten the boundary.** Pure desktop refactor, no UWP, runs concurrently with `P1`. `Placement` stops taking a `World&`, the landscape's hashing moves to the side that hashes, `App.cpp` loses its `Movement.h`, and eight headers go back to `GameLogic`.

**`P3`, `P4` — the split and the harness.** The presentation layer comes out of the executable into `GameClient` with the first tests it has ever had; `LocalHost` is deleted; `OutpostCapture` takes the headless path and CI's capture step changes one word. **Both are pure desktop refactors**, reviewable against the old tree line by line, and when they land CI is green in exactly the shape it is green in today — so that when something breaks later, the refactor is not a suspect.

**`P5`, `P6` — the pieces a test runner can hold.** The DIP-to-pixel conversion and the key and pointer translation go into `NeuronClient` as pure functions with suites; `SwapChain` gains its core-window constructor. None needs a package to build or a window to test, and separating them from the part that does is deliberate.

**`P7`, `P8` — the shell.** `Paths` reduced to two roots; then the framework view, the manifest, package identity, and the deletion of `Window` and `RawMouse`. This is where the game first runs as a package, and every task in it ends with the owner running it and looking at it — because here, for the first time in this tree's life, a green build says nothing about whether it *starts*.

**`P9`, `P10`, `P11` — pixels, lifetime, and closing.** The physical-pixel path measured at three scale factors; suspend, resume and device removal; then the package as a CI gate and the closing grep. **That grep is written and run on the day `P1` is written**, per `ImplementationPlan.md` §3 — a closing criterion is worth the most when it can fail early and the least when it fails last.

---

## 4. The risks, ranked

Ranked by what it costs to be wrong, not by how likely each is.

**1. The loopback constraint might not be what the documentation says.** `ADR-019` gives up single player on one clause of *Interprocess communication (IPC)*. If a packaged client turns out to reach an unpackaged host on `127.0.0.1` — or if some future Windows release admits it — the price was paid for nothing. *Answered by `P1`, which is why it is in `P1` and not in `P8`.*

**2. The CI runner has no UWP C++ workload.** If `windows-latest` cannot build a `Windows Store` project, the packaging gate of `ADR-016` cannot exist and the package is only ever built on the owner's machine. Everything else survives — the capture gate is on a desktop executable by design, precisely so this risk cannot reach it. *Answered by `P1`; the fallbacks are a workload install step, a pinned image, or saying plainly in `AGENTS.md` §3 that the package is a local gate.*

**3. The C++/WinRT package will not give up `/std:c++17`.** `AGENTS.md` §3 says that if a build error tempts you to lower the language standard, stop and report. If the package's props cannot be overridden that is exactly the position, and the answer is to drop the package and write against the Windows SDK's own projection headers, at the price of being pinned to the SDK's version. *Answered by `P1`. The risk that would most change what the tree looks like, and it costs a day to answer.*

**4. The picture gets quietly worse.** [`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md) names it in full: a swap chain created at DIP size on a scaled display renders at the wrong resolution and the compositor scales it again, so the point-sampled integer-multiple path of `AGENTS.md` §5 never runs and the interface arrives soft. **Nothing fails, nothing logs, and no test catches it** — which is why `P9`'s acceptance is four numbers and a screenshot at each of 100%, 150% and 200%.

**5. The restructure does not compile.** `ADR-018` moved roughly 300 files and rewrote fifteen project files with no compiler in reach. What is verified is every edge, every reference, every include directory and every quoted include, plus the formatter and the plan validator; **a link error is still possible and a compile error is not ruled out**. `P2` is the first task that builds it, and it is in wave 1 for that reason.

**6. Suspend, resume and device removal are new code, not a port.** A `grep` over `NeuronClient/` for `DXGI_ERROR_DEVICE_REMOVED` returns nothing. A desktop game that runs until the player leaves can get away with that; a packaged app the system parks and wakes cannot. The largest piece of genuinely *new* work in the plan, and the only one whose failure mode is a crash rather than a wrong picture.

**7. UWP is not where Microsoft is investing.** It is not deprecated — Visual Studio 2026 ships a latest-MSVC UWP C++ toolset, and the no-XAML "Core App" shape has no WinUI 3 equivalent at all — but the recommendation for new desktop apps is WinUI 3, and the app models Microsoft names as still requiring UWP are Xbox, HoloLens and Surface Hub, none of which this game targets. **The owner's decision of 2026-09-20 is taken with that known.** The mitigation is structural rather than hopeful: `ADR-014` keeps the packaged project down to glue, so the day this has to move, what moves is one project and not a tree.

---

## 5. What does not change

Worth stating, because the list is longer than the one above.

The simulation, entire: the 20 Hz tick, the fixed-point positions, the state hash, snapshots and replays, pathing, visibility, combat, the AI. The content format and its loaders. The replication protocol and both endpoints. All seven render passes, the scene target, the 4× resolve, the shaders and the `FXCompile` step. The interface layer, its panels and its minimap. The authored resolution of 1920×1080 and the scale rule over it. `/fp:precise` and `/arch:AVX2` and everything `AGENTS.md` R16 rests on them for. The naming convention, the formatter, the linter, and all three checkers. Six task plans and nineteen ADRs. **And not one line of C++ changed for the library restructure** — the directories are flat, so a quoted include resolves by filename and only the include *paths* in the project files moved.

**And the capture gate.** Its four assertions — a shot in flight, a hundred interface quads a frame, a device selected, something researched — are the strongest quality mechanism this tree has, and `ADR-014` is in substantial part a decision to keep them running unchanged. They move to a different executable and they do not otherwise move.

---

## 6. CI, after

```
  check the build shape        CheckProjectFiles.py        unchanged
  build Debug|x64              msbuild over the solution   + the package, if the runner can (risk 2)
  run the suites               vstest over *Tests.dll      seven suites now, discovered by glob
  capture 9,000 ticks on WARP  OutpostCapture.exe          one word changed
  validate the content         OutpostHost.exe --validate  unchanged
  clang-tidy                   RunClangTidy.py             unchanged
  format                       CheckFormat.py, Linux job   unchanged
  the plans validate           CheckTaskDag.py             unchanged
```

**The workflow needed no change for the restructure**, because it discovers projects by glob and the solution by extension rather than naming either. That was not an accident when it was written, and it paid for itself the first time the tree moved.

**The package is built and not run.** Running it needs deployment and developer mode on the runner, and `AGENTS.md` §3 already holds that a green build says nothing about whether the game draws. What a build gate can honestly catch is a manifest regression and an import the app container forbids, and that is what it is asked to catch.
