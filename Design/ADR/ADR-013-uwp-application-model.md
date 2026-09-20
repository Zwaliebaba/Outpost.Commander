# ADR-013 — The application model: a packaged UWP app over a CoreWindow

**Status:** Accepted; supersedes [`ADR-004`](ADR-004-renderer-foundation.md) on the window and on how the game is left, and nothing else in it. **Corrected on 2026-09-20 by [`ADR-020`](ADR-020-central-server-no-lobby-no-pause.md)**: Escape never exits — it peels and then swaps the pointer's mode, as `Design/Interface.md` §7 and `NeuronClient/PointerMode.h` have it — and F10's quit menu is the way out. **Amended on 2026-09-20 by [`ADR-021`](ADR-021-touch-is-the-only-input.md)** on what device the client runs on and what it takes as input: the packaged application is for a **touchscreen**, a gesture is the only way into the input queue, and the keyboard is not subscribed — which overtakes the correction above a second time, because Escape is no longer a key at all and the quit menu is reached by a gesture.
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

*Outpost Commander* is a hard fork of *Frontier Commander* taken at its M1 state, and the reason the fork exists is the application model: the game is to run as a **packaged Universal Windows Platform application whose view is a `CoreWindow`**, where the tree it came from runs as a Win32 desktop executable over an `HWND`. Everything else about the two trees is the same game.

`ADR-004` settled the window for the Win32 tree: a borderless `WS_POPUP` covering the primary monitor, per-monitor-V2 DPI awareness set before the window exists, Escape and Alt+F4 answered in the window procedure because a popup has no close box, and DXGI's Alt+Enter toggle disabled. **None of that survives packaging, and each clause has to be replaced rather than dropped**, because each was answering a real question that the new model asks again in a different form.

Three application models can carry a Direct3D 12 renderer on Windows today, and the choice among them is not a matter of taste:

- **WinUI 3 / Windows App SDK**, which Microsoft names as its recommendation for new general-purpose desktop apps. It has **no `CoreWindow`** and no equivalent of the no-XAML "Core App" shape: a D3D swap chain reaches the screen through a `SwapChainPanel` inside a XAML tree, or through an `HWND`, which is the model this fork exists to leave.
- **XAML-hosted UWP**, a `SwapChainPanel` inside a UWP XAML page. It brings the XAML framework, its layout pass and its compositor throttling into a frame loop that has no other use for any of them, and `CompositionTarget::Rendering` is exactly the kind of external timing source that made `--novsync` necessary in the first place (`ADR-004`; `m0-foundation/T22`).
- **`CoreWindow` UWP**, the model Visual Studio's "Core App (C++/WinRT)" template produces: an `IFrameworkViewSource` handed to `CoreApplication::Run`, an `IFrameworkView` that receives the window, and a `Run` method that owns the loop. Microsoft's own documentation is explicit that **there is no WinUI 3 equivalent of this template**.

The third is the only one whose shape matches what the renderer already is: one thread, one loop, one swap chain, nothing between the game and the present.

**UWP's standing.** Microsoft's position, as of Visual Studio 2026, is that UWP is *not* deprecated — VS 2026 ships `Microsoft.VisualStudio.ComponentGroup.UWP.VC`, "C++ Universal Windows Platform tools (Latest MSVC)" — but that WinUI 3 is the recommendation for new general-purpose desktop apps, with Xbox, HoloLens and Surface Hub named as the app models that still require UWP. **This game is none of those**, and the owner's reason is the app model itself: package identity, the app container, and a `CoreWindow` with nothing between it and the swap chain. That is a deliberate choice against the recommendation, and it is recorded here as one so that nobody re-derives it as an oversight.

## Decision

**The game is an MSIX-packaged UWP application.** `OutpostCommander` becomes a `ConfigurationType` of `Application` with `ApplicationType` `Windows Store`, an `AppxManifest.xml`, package identity, and no entry point other than `CoreApplication::Run`. It is never run unpackaged, and nothing in the tree depends on it being runnable that way.

**The client runs on a touchscreen device** — a tablet, a Surface, or a desktop with a touch panel — and the host is elsewhere on the network ([`ADR-019`](ADR-019-the-client-never-simulates.md), [`ADR-021`](ADR-021-touch-is-the-only-input.md)). “Desktop, packaged” in this ADR means the *device family*, `Windows.Desktop`, and not a machine with a mouse.

**Its view is a `CoreWindow`, driven by `IFrameworkView`.** One type implements `IFrameworkViewSource` and returns one implementing `IFrameworkView`; `Initialize`, `SetWindow`, `Load`, `Run` and `Uninitialize` are the five methods, `SetWindow` is where the window's events are subscribed, and `Run` owns the frame loop and calls `CoreDispatcher::ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent)` once per frame. **That call replaces `Window::Pump` exactly**: it is the one drain of the queue per frame, and the events it dispatches enqueue into the same `Neuron::InputQueue` the window procedure enqueued into, for the same reason — a message becomes a record and the frame decides what it meant (`TechnicalDesign.md` §6.5).

**There is no XAML anywhere in the tree**, no `SwapChainPanel`, and no XAML compiler step in any project. A `.xaml` file appearing in a project file is a defect.

**The swap chain is created for the core window**, `IDXGIFactory2::CreateSwapChainForCoreWindow`, and the window is put into full screen by `ApplicationView::TryEnterFullScreenMode()` rather than by sizing a borderless window to the monitor. `MakeWindowAssociation` and `DXGI_MWA_NO_ALT_ENTER` go: there is no `HWND` to associate and no DXGI full-screen toggle to suppress.

**Escape does not leave; the quit menu does, and it calls `CoreApplication::Exit()`.** `ADR-004` gave the window procedure both Escape and Alt+F4 because a `WS_POPUP` has no close box. A `CoreWindow` is not in that position: the shell owns Alt+F4 and the app's close, and `CoreWindow::Closed` arrives whether the game asked for it or not. So **Escape keeps exactly the meaning `Design/Interface.md` §7 gives it** — peeled, not swallowed, and the last press swaps the pointer's mode rather than exiting, which is what `NeuronClient/PointerMode.h`'s `EscapePeel` enumerates and what `NeuronClientTests` pins. The way out is F10's menu (`Interface.md` §10), and `CoreWindow::Closed` is honoured wherever it comes from.

## Consequences

**What this forecloses.** WinUI 3 and the Windows App SDK are out of reach for as long as the view is a `CoreWindow`, because there is no `CoreWindow` in that framework: a later move there is a rewrite of the shell, not a retarget. So is any XAML overlay, any second window, and any in-process tooling window — a debug view is drawn by the game into its own scene target or it does not exist. Running the game from a command line as a plain executable is gone, and with it every workflow that assumed it; `ADR-016` says what replaces the ones the tree actually used.

**What it costs that the Win32 tree did not pay.** Package identity means deployment before launch, and deployment on a developer machine means developer mode. Suspend and resume become real states the renderer has to survive rather than conditions that never occur. The app container means the game can read its own install directory and write its own app data and nothing else, which `ADR-016` turns into a rule. And the "launch it and look at it" clause of `AGENTS.md` §3 now costs a package build and a deploy rather than pressing F5 on an executable.

**What would reopen it.** A frame the compositor will not present without tearing on a variable-refresh display, if the creation flag turns out to be refused on a core-window chain (see **Measurements**, which does not yet have the answer); or a `CoreWindow` deprecation with a date on it. Neither is a reason to move to XAML — the answer to both is `CreateSwapChainForComposition` under a `Windows.UI.Composition` visual, which keeps the no-XAML property.

**What does not change.** Everything `ADR-004` settled other than the window: 1920×1080 authored, the 4× multisampled scene target resolved and presented scaled, the `FRAMES_IN_FLIGHT` = 3 frame loop, the three-buffer flip-discard swap chain, the `HRESULT` policy, `d3dx12.h` at its pin, and shader model 6.0 through `FXCompile`. The scale rule of `AGENTS.md` §5 also stands, but the number it reads changes units — see `ADR-016` on device-independent pixels, which is the one place this migration can silently produce a worse picture.

## Measurements

**None of the figures this ADR would want have been measured, and the decision is taken without them.** This is a departure from the ADR standard of `Design/ADR/README.md`, and it is stated rather than papered over: the tree has no Windows machine in reach at the time of writing, so every platform claim below is read from Microsoft's documentation and **none is confirmed against a build**. `p1-uwp-shell/P1` exists to measure them before any other task in the plan starts, and this ADR is amended with what it finds.

| Claim | Source | Status |
|---|---|---|
| A no-XAML UWP C++ app is a supported template shape and has no WinUI 3 equivalent | *Introduction to C++/WinRT*, "Core App (C++/WinRT)" | Documented, unverified here |
| VS 2026 ships a latest-MSVC UWP C++ toolset | VS 2026 component directory, `Microsoft.VisualStudio.ComponentGroup.UWP.VC`, 18.0.11123.170 | Documented; that it accepts `v145` with this tree's settings is `P1`'s to prove |
| A UWP app in `TryEnterFullScreenMode` may present with `DXGI_PRESENT_ALLOW_TEARING` | *DXGI\_PRESENT* constants, the `DXGI_PRESENT_ALLOW_TEARING` row | Documented for the **present** flag; that `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` is accepted at **creation** on a core-window chain is `P1`'s to prove |
| `CoreDispatcher::ProcessEvents(ProcessAllIfPresent)` drains without blocking | *The app object and DirectX* | Documented, unverified here |

The one figure that is not a documentation claim is arithmetic on this tree: **the Win32 surface this ADR has to replace is four files deleted outright and three named call sites**. The four are `NeuronClient/Window.h` (71 lines), `NeuronClient/Window.cpp` (316), `NeuronClient/RawMouse.h` (21) and `NeuronClient/RawMouse.cpp` (49) — **457 lines**, by `wc -l` at `ec702e2`. The three are the `CreateSwapChainForHwnd` and `MakeWindowAssociation` pair in `NeuronClient/SwapChain.cpp`, the `ShowCursor` and `SetWindowTextW` calls in `App.cpp`, and `wWinMain` with `CommandLineToArgvW` in `Main.cpp` (33 lines whole). Everything else in `Client`, and all of `Core`, `Content`, `Sim`, `Net` and `Replica`, names no windowing API at all.
