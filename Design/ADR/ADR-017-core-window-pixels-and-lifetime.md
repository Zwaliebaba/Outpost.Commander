# ADR-017 — The core window's pixels and its lifetime

**Status:** Accepted; supersedes [`ADR-004`](ADR-004-renderer-foundation.md) on DPI awareness and nothing else in it
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

Two things a `CoreWindow` does that an `HWND` did not, and both reach the picture.

**It measures itself in device-independent pixels.** `CoreWindow::Bounds` is a `Rect` of `float` in DIPs, not a `RECT` of physical pixels, and `pixels = dips × dpi ÷ 96`. `ADR-004` handled this on the Win32 side with one line — `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` before any window exists — and its comment says exactly why: "a 4K desktop at 150% is 3840 wide, not a virtualised 2560, and the integer-multiple case of `AGENTS.md` §5 can happen at all." That line does not exist under UWP and the problem it solved does.

**This is the one place in the migration that can silently produce a worse picture.** `AGENTS.md` §5 fixes the present: 1:1 and unfiltered when the client area already matches the authored 1920×1080, point sampling at an exact integer multiple, bilinear otherwise. A swap chain created at `Bounds` on a 3840×2160 panel at 150% would be 2560×1440, `FitAuthored` would compute a bilinear 1.33× from a 1920×1080 scene target, and the compositor would then scale that 2560×1440 surface up to 3840×2160 — **two resamples where the rule asks for a point-sampled 2×**. Nothing fails, nothing logs, and the dense interface full of small type that `ADR-004` says is the real cost of the whole arrangement arrives soft. A green build says nothing about it, which is precisely the failure mode `AGENTS.md` §3 warns about.

**It has a lifetime the desktop window did not.** A packaged app is suspended when it loses visibility and may be terminated while suspended; it is resumed, sometimes onto a different adapter. `NeuronClient/GraphicsDevice` has **no device-removal handling at all** today — a `grep` for `DXGI_ERROR_DEVICE_REMOVED` and `GetDeviceRemovedReason` over `Client/` returns nothing — which was survivable for a desktop game that runs until the player leaves and is not survivable for one the system parks.

## Decision

**The swap chain is created and resized in physical pixels, always, and the conversion is a pure function with a test.** `Neuron::PhysicalPixels(dipsBounds, logicalDpi)` lives in `Client`, names no Windows Runtime type, rounds as the platform does (`floor(dips × dpi ÷ 96 + 0.5)`), and `ClientTests` pins it at 96, 120, 144 and 192 DPI against the four panel sizes the game is likely to meet. **It is a pure function in a library for the same reason `Neuron::AimedBy` is** (`NeuronClient/PointerMode.h`): it is one rounding away from a blurry game, it compiles either way, and no compiler will ever notice.

**The game asks for raw pixels first and converts second.** `ApplicationViewScaling::TrySetDisableLayoutScaling(true)` is called before the first frame; where it succeeds the window reports raw pixels and the conversion is the identity, and where it fails the conversion carries it. **Both paths produce the same number**, and the log says which one ran and what it produced — a line the owner can read off a running build, which is how a wrong answer here gets caught at all.

**`DisplayInformation::GetForCurrentView()` supplies the DPI**, and `DpiChanged` and `DisplayContentsInvalidated` are subscribed alongside `CoreWindow::SizeChanged`. All three do the same thing: recompute the physical size and, if it changed, resize the swap chain — which is the `Window::TakeResized` path unchanged, reached by three events instead of one.

**The manifest declares landscape orientations only**, and the renderer implements no pre-rotation. A `DXGI_MODE_ROTATION` path and the matrices that go with it are a real piece of work for a device this game does not target, and taking it now would be building for a case that has no design behind it.

**The frame loop stops when the window is not visible, and the device is trimmed when the app suspends.** `CoreWindow::VisibilityChanged` and `CoreWindow::Activated` gate the loop; `CoreApplication::Suspending` takes its deferral, waits for the GPU to go idle, calls `IDXGIDevice3::Trim()` and completes the deferral; `CoreApplication::Resuming` does nothing but let the loop start again. A suspended app that still holds its heaps is a certification finding, and one that presents while invisible is burning a laptop's battery to draw nothing.

**Device removal is handled, and this is new behaviour rather than a port.** `Present` and `BeginFrame` check for `DXGI_ERROR_DEVICE_REMOVED` and `DXGI_ERROR_DEVICE_RESET`, log `GetDeviceRemovedReason`, and rebuild the device, the swap chain, the scene target and every pipeline state. **The match is not lost** — the simulation holds no graphics state, which is a property `ADR-001`'s edges already guarantee and which is worth having said out loud, because it is the reason this is recoverable at all.

**The frame time leaves the title bar, because there is no title bar.** `m1-vertical-slice/K5` put it in the window caption on the stated ground that it was "the one place a number can be read off a running build without a pass to draw text". A full-screen `CoreWindow` has no caption, and `ApplicationView::Title` reaches the task switcher and not the screen. The ground has also expired: `m1-vertical-slice/K4` landed the interface passes, so there *is* a pass to draw text. The readout moves into the HUD, and the log keeps carrying it for the runs nobody is watching.

## Consequences

**What this forecloses.** Portrait and rotating displays, until somebody writes the pre-rotation path and a design that wants it. Any assumption anywhere that the window's reported size is in pixels — which is why the conversion is one function and not an expression at each call site.

**What it costs.** Device-removal recovery is a real rebuild path through `GraphicsDevice`, `SwapChain`, `SceneTarget` and seven pipeline states, and it is a path nobody exercises by accident; it needs a way to be provoked deliberately, which is what the `--warp` device and the D3D12 debug layer's device-removal injection are for. Suspend handling puts a deferral and a GPU wait on a code path that previously did not exist.

**What would reopen it.** `TrySetDisableLayoutScaling` failing on every machine the owner has, which would make the conversion the only path and the "ask first" half dead weight; or a design that wants the game on a tablet, which brings rotation back.

## Measurements

**Not measured.** The conversion formula is documented (*Supporting screen orientation*, `ConvertDipsToPixels`; *DPI and DIPs*), the DIP-ness of `CoreWindow::Bounds` is documented in the same place, and neither is confirmed on this tree. The claim in this ADR most worth testing early is not the formula but `TrySetDisableLayoutScaling`: `p1-uwp-shell/P8` runs the game at 100%, 150% and 200% on one panel and records, for each, what the window reported, what the swap chain was created at, which scaling path `FitAuthored` took, and whether a screenshot of the interface is pixel-exact at 1:1. **Four numbers and one picture, per scale factor** — and until they exist, the picture this migration produces is unverified.

The one figure from this tree: `Client/` contains **no** reference to `DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_RESET` or `GetDeviceRemovedReason`, counted by `grep` at `ec702e2`. The device-removal path is written from nothing, not ported.
