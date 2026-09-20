# Migrating to UWP and the core window

**What this document is.** *Outpost Commander* is a hard fork of *Frontier Commander*, taken at its M1 state on 2026-09-20, and the fork exists for one reason: the game is to run as a **packaged UWP application whose view is a `CoreWindow`**, where the tree it came from runs as a Win32 desktop executable over an `HWND`. This is the plan for getting from one to the other — what changes, what does not, in what order, and what could still go wrong.

The decisions it implements are [`ADR-013`](ADR/ADR-013-uwp-application-model.md) to [`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md), taken by the owner on 2026-09-20. The work is `tasks/p1-uwp-shell.yaml`. This document is the narrative those two are the decision and the schedule of; where they disagree with it, they win.

**Read the honesty note first.** Nothing in this plan has been built. The tree has no Windows machine in reach, so every platform claim here is read from Microsoft's documentation and none is confirmed against a compiler. That is why the first task in the plan measures instead of building, why five of its acceptance lines are numbers rather than behaviours, and why four of the five ADRs carry a **Measurements** section that says, in as many words, that it has nothing in it yet. `AGENTS.md` §3 asks for the difference between "builds clean, not run" and "builds and runs" to be stated. This is the third case: **neither, yet**.

---

## 1. The surface, measured

The Win32 coupling in this tree is **four files deleted outright and a handful of named call sites**: `NeuronClient/Window.{h,cpp}` and `NeuronClient/RawMouse.{h,cpp}` are 457 lines by `wc -l` at `ec702e2`, and the rest is the swap chain's creation call, two calls in `App.cpp`, `Main.cpp` entire (33 lines) and three calls in `NeuronCore/Paths.cpp` (113 lines). Counted by grep over `Core`, `Content`, `Sim`, `Net`, `Replica`, `Client` and the two executables at `ec702e2` for the Win32 window, process, environment and raw-input entry points. Everything else — the fourteen-stage tick, the pathing, the replication protocol, all seven render passes, the interface — names no windowing API at all.

| What it does today | Where | What replaces it |
|---|---|---|
| `WNDCLASSEXW`, `CreateWindowExW(WS_POPUP)`, `PeekMessageW` loop, `WM_*` to `InputEvent` | `NeuronClient/Window.{h,cpp}`, 387 lines | `IFrameworkViewSource` + `IFrameworkView` in the packaged executable; `CoreDispatcher::ProcessEvents(ProcessAllIfPresent)` in place of `Window::Pump`; `CoreWindow::KeyDown`/`KeyUp`/`CharacterReceived`/`PointerPressed`/`PointerReleased`/`PointerMoved`/`PointerWheelChanged` in place of the window procedure's switch |
| `RegisterRawInputDevices`, `GetRawInputData` on `WM_INPUT` | `NeuronClient/RawMouse.{h,cpp}`, 70 lines | `MouseDevice::GetForCurrentView().MouseMoved`, whose `MouseDelta` is the same relative count with the same guarantees — unaccelerated, unclamped by the screen edge. **The closest mapping in the whole migration** |
| `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` | `NeuronClient/Window.cpp` | `ApplicationViewScaling::TrySetDisableLayoutScaling(true)`, falling back to converting `CoreWindow::Bounds` from DIPs ([`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md)) |
| `CreateSwapChainForHwnd`, `MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER)` | `NeuronClient/SwapChain.cpp` | `CreateSwapChainForCoreWindow`; the association goes, there being no `HWND` and no DXGI full-screen toggle |
| `ShowCursor(TRUE/FALSE)` for aim mode | `Game/App.cpp` (today `OutpostCommander/App.cpp`) | `CoreWindow::PointerCursor = nullptr` and back, which is the documented pairing with `MouseMoved` |
| `SetWindowTextW` — the frame-time readout | `Game/App.cpp` | The HUD. There is no title bar ([`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md)) |
| `wWinMain` + `CommandLineToArgvW` | `OutpostCommander/Main.cpp` | `CoreApplication::Run(ViewSource())`; arguments from `LaunchActivatedEventArgs::Arguments()` ([`ADR-016`](ADR/ADR-016-package-identity-and-launch.md)) |
| `GetModuleFileNameW`, `GetEnvironmentVariableW(L"LOCALAPPDATA")` | `NeuronCore/Paths.cpp` | Nothing: `Paths` becomes two roots each executable sets for itself, so `Core` names no path API at all ([`ADR-016`](ADR/ADR-016-package-identity-and-launch.md)) |

`OutputDebugStringA` in `NeuronCore/Log.cpp` and `NeuronCore/Assertion.cpp` stays — it is in the UWP surface.

**Nothing in `Sim`, `Content`, `Net` or `Replica` appears in that table**, and that is not luck: `ADR-001` forbade those four a platform header and `Build/CheckProjectFiles.py` has been failing the build over it since M0. The rule was written for portability between layers and it has just paid for a platform migration.

---

## 2. The tree, before and after

`ADR-014` puts exactly one project inside the package and leaves everything else a desktop project. Two projects are added to make that work.

```
                     before                          after
  Core                 static lib                      unchanged
  Content              static lib                      unchanged
  Sim                  static lib                      unchanged
  Net                  static lib                      unchanged
  Replica              static lib                      unchanged
  Client               static lib                      minus Window and RawMouse,
                                                       plus the DIP conversion and the
                                                       core-window swap chain constructor
  Game                 —                          NEW  static lib: what the executable held
  OutpostCommander     exe, Windows subsystem     UWP  packaged; WinRT glue and nothing else
  OutpostCapture       —                          NEW  exe, console: the headless capture CI drives
  OutpostHost          exe, console                    unchanged
  Tests/GameTests      —                          NEW  the first coverage over 229 kB of game code
```

**`Game` is the load-bearing move, and it is worth being clear that it is not bookkeeping.** `OutpostCommander/` holds **228.8 kB over 14 files** — `App` at 73.9 kB, `Hud` at 80.2, `Operator` at 27.0 — and none of it has ever been reachable from a test, because it lives in an Application. `NeuronClient/PointerMode.h` already carries a comment explaining that its own contents were pushed down into a library for exactly that reason. Splitting the shell from the game is what lets the packaged executable be small enough to hold no logic, and it hands `Tests/GameTests` 229 kB that no suite has ever touched.

**The seam between them is three things and no window type.** `Game::RunWindowed` is handed the client size in physical pixels, a way to say the frame time changed, and a pump that returns false when the user has left. The packaged executable supplies those over a `CoreWindow`. Nothing above the seam knows what application model it is under, which is the property that makes `OutpostCapture` a different executable over the same code rather than a copy of it.

---

## 3. The order, and why it is that order

Five phases. The order is not narrative — every edge is a thing that genuinely cannot start until another finishes, which is the rule `ImplementationPlan.md` §3 puts on the task graph.

**Phase 0 — prove the ground (`P1`).** One throwaway UWP project that opens a core window, makes a D3D12 device, creates a swap chain for it, clears to a colour and presents. It answers five questions, and **the plan changes shape if any of them answers wrong**:

1. Does `v145` exist as a UWP toolset on the installed Visual Studio, and does the tree's setting block — `/std:c++latest`, `/permissive-`, `/W4 /WX`, `/fp:precise`, `/arch:AVX2` — survive `ApplicationType` `Windows Store`?
2. Does the `Microsoft.Windows.CppWinRT` package's `/std:c++17` stay overridden, read off the compiler command line in the build log rather than inferred from the build succeeding?
3. Do the generated `winrt/*.h` projection headers compile at `/W4` with warnings as errors, and if not, exactly which warnings?
4. Is `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` accepted at *creation* on a core-window chain, not merely at present?
5. **Does the CI runner image carry the UWP C++ workload at all?** If `windows-latest` has no `Microsoft.VisualStudio.ComponentGroup.UWP.VC`, the package cannot be built in CI and the packaging gate needs a workload install step, a different image, or a self-hosted runner.

That is a day of work that can save a fortnight, and it is why nothing else in the plan may start until it lands.

**Phase 1 — the split (`P2`, `P3`).** `Game` comes out of the executable with `Tests/GameTests`; `OutpostCapture` takes the headless path and CI's capture step changes one word. **Both are pure desktop refactors**: they touch no Windows Runtime API, they can be reviewed against the old tree line by line, and when they land CI is green in exactly the shape it is green in today. Doing them before any UWP code exists means that when something breaks in Phase 2, the refactor is not a suspect.

**Phase 2 — the pieces with tests (`P4`, `P5`).** The DIP-to-pixel conversion and the key and pointer translation go into `Client` as pure functions with suites, and `SwapChain` gains its core-window constructor. None of this needs a package to build or a window to test. It is the part of the migration a compiler and a test runner can hold, and it is deliberately separated from the part they cannot.

**Phase 3 — the shell (`P6`, `P7`, `P8`, `P9`).** The packaged executable: the view source and framework view, the manifest and assets, package identity and the two roots, the physical-pixel path, suspend and resume, device removal. This is where the game first runs as a package. Every task in it ends with the owner running it and looking at it, because `AGENTS.md` §3 is explicit that a green build says nothing about whether the game draws — and here, for the first time in this tree's life, a green build says nothing about whether it *starts*.

**Phase 4 — closing (`P10`, `P11`).** The package build becomes a CI gate; `Window.{h,cpp}` and `RawMouse.{h,cpp}` are deleted and a grep over the tree for the Win32 window entry points returns nothing. The closing grep is written and run on the day `P1` is written, per `ImplementationPlan.md` §3 — a closing criterion is worth the most when it can fail early and the least when it fails last.

---

## 4. The risks, ranked

Ranked by what it costs to be wrong, not by how likely each is.

**1. The CI runner has no UWP C++ workload.** If `windows-latest` cannot build a `Windows Store` project, the packaging gate of `ADR-016` cannot exist and the package is only ever built on the owner's machine. Everything else in the plan survives — the capture gate is on a desktop executable by design, precisely so that this risk cannot reach it — but the tree loses its only automated check that the package is buildable at all. *Answered by `P1`; if it answers wrong, the fallbacks are a workload install step in the job, a pinned older image, or accepting that the package is a local gate and saying so in `AGENTS.md` §3.*

**2. The C++/WinRT package will not give up `/std:c++17`.** `AGENTS.md` §3 says that if a build error tempts you to lower the language standard, stop and report. If the package's props cannot be overridden, that is exactly the position, and the answer is to drop the package and write against the Windows SDK's own projection headers — which ship the full `winrt/Windows.*` namespace and need no package, at the price of being pinned to the SDK's version. *Answered by `P1`. This is the risk that would most change what the tree looks like, and it costs a day to answer.*

**3. The picture gets quietly worse.** [`ADR-017`](ADR/ADR-017-core-window-pixels-and-lifetime.md) names it in full: a swap chain created at DIP size on a scaled display renders at the wrong resolution and the compositor scales it again, so the point-sampled integer-multiple path of `AGENTS.md` §5 never runs and the interface arrives soft. **Nothing fails, nothing logs, and no test catches it** — which is why `P8`'s acceptance is four numbers and a screenshot at each of 100%, 150% and 200%, and why the conversion is a pure function in `Client` with a suite rather than an expression at the call site.

**4. Suspend, resume and device removal are new code, not a port.** A `grep` over `Client/` for `DXGI_ERROR_DEVICE_REMOVED` returns nothing at `ec702e2`. A desktop game that runs until the player leaves can get away with that; a packaged app that the system parks and wakes cannot. This is the largest piece of genuinely *new* work in the plan, and the only one whose failure mode is a crash rather than a wrong picture.

**5. Tearing may not be available.** `m0-foundation/T22` established that the frame time this renderer reports is meaningless without an unlocked present, and `m1-vertical-slice/K5` built `--novsync` to get one. The documentation is explicit that a UWP app in `TryEnterFullScreenMode` may *present* with `DXGI_PRESENT_ALLOW_TEARING`; it is less explicit about the *creation* flag on a core-window chain. If creation refuses it, the frame time has to come from GPU timestamp queries instead, which is better measurement anyway and more work. *Answered by `P1`.*

**6. UWP is not where Microsoft is investing.** It is not deprecated — Visual Studio 2026 ships a latest-MSVC UWP C++ toolset, and the no-XAML "Core App" shape has no WinUI 3 equivalent at all — but the recommendation for new desktop apps is WinUI 3, and the app models Microsoft names as still requiring UWP are Xbox, HoloLens and Surface Hub, none of which this game targets. **The owner's decision of 2026-09-20 is taken with that known.** The mitigation is structural rather than hopeful: `ADR-014` keeps the packaged project down to glue, so the day this has to move, what moves is one project and not a tree.

---

## 5. What does not change

Worth stating, because the list is longer than the one above.

The simulation, entire: the 20 Hz tick, the fixed-point positions, the state hash, snapshots and replays, pathing, visibility, combat, the AI. The content format and its loaders. The replication protocol and both endpoints. All seven render passes, the scene target, the 4× resolve, the shaders and the `FXCompile` step. The interface layer, its panels and its minimap. The authored resolution of 1920×1080 and the scale rule over it. `/fp:precise` and `/arch:AVX2` and everything `AGENTS.md` R16 rests on them for. The naming convention, the formatter, the linter, and all three checkers. Five task plans and twelve ADRs.

**And the capture gate.** Its four assertions — a shot in flight, a hundred interface quads a frame, a device selected, something researched — are the strongest quality mechanism this tree has, and `ADR-014` is in substantial part a decision to keep them running unchanged. They move to a different executable and they do not otherwise move.

---

## 6. CI, after

```
  check the build shape        CheckProjectFiles.py        unchanged
  build Debug|x64              msbuild over the solution   + the package, if the runner can (risk 1)
  run the suites               vstest over *Tests.dll      + GameTests
  capture 9,000 ticks on WARP  OutpostCapture.exe          one word changed
  validate the content         OutpostHost.exe --validate  unchanged
  clang-tidy                   RunClangTidy.py             unchanged
  format                       CheckFormat.py, Linux job   unchanged
  the plans validate           CheckTaskDag.py             unchanged
```

**The package is built and not run.** Running it needs deployment and developer mode on the runner, and `AGENTS.md` §3 already holds that a green build says nothing about whether the game draws. What a build gate can honestly catch is a manifest regression and an import the app container forbids, and that is what it is asked to catch.
