# ADR-006 — The renderer's shape: borderless full screen with a windowed toggle, two frames in flight, fatal device loss

Status: **accepted** · 2026-09-30

## Context

The renderer is hand-written D3D12 on a Win32 window (ADR-001, R12, R14). Before the first frame is drawn, it needs a few decisions that are expensive to change later (implementation plan, gate G1): how the game occupies the screen, how many frames the CPU may record ahead of the GPU, how it presents, what happens when the window or display changes, what happens when the GPU is lost, and which failed `HRESULT`s are handled rather than thrown.

Two design targets decide most of them (design §3):

- **Q4:** 99% of frames within 16.7 ms at 1920×1080, with the HUD drawn.
- **Q5:** at most 150 ms from an order to the ship visibly responding. The design builds that from ADR-002's 50–100 ms for the tick and interpolation, plus about two frames for input and presentation. So presentation has roughly two frames of latency to spend, and a deeper frame queue would spend more than that on its own.

The design also pans the camera by edge scroll (design §4), which only works when the game owns the screen edges.

The owner took these decisions on 2026-09-30.

## Decision

1. **The game starts borderless full screen, and Alt+Enter toggles a window.** At start the window is a `WS_POPUP` that exactly covers the primary monitor, in physical pixels, because the process is per-monitor DPI aware (`app.manifest`). Alt+Enter switches to a resizable `WS_OVERLAPPEDWINDOW`, and again back to borderless over whichever monitor the window is then on. The first time the game goes windowed, the window has a 1280×720 client area, centered on its monitor's work area. After that it returns to the frame it last had as a window, for the rest of the session; nothing is saved between runs. The toggle behaves the same in Debug and Release. The game handles Alt+Enter itself, in `Window::ProcessMessages`, and DXGI's own handling of it is switched off (`DXGI_MWA_NO_ALT_ENTER`), because DXGI's would switch to exclusive full screen. Exclusive full screen (`SetFullscreenState`) is not used. A flip-model swap chain that covers the output gets independent flip from DWM, so borderless has the same presentation path and latency, without display mode changes or fragile Alt+Tab. When the display changes (`WM_DISPLAYCHANGE`), a full-screen window covers its monitor again and a windowed one keeps its frame.
2. **Two frames in flight, on a waitable swap chain.** The swap chain has two back buffers and the renderer has one command allocator per back buffer (`Renderer::FRAME_COUNT`). The swap chain is created with `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` and `SetMaximumFrameLatency(1)`. The main loop waits on that object before it reads input, so the input a frame is built from is as fresh as the frame it is shown in. The count is one constant, so that the Q4 and Q5 measurements can try three if two turns out to leave too little slack.
3. **Presentation is flip model with vsync on.** The swap effect is `DXGI_SWAP_EFFECT_FLIP_DISCARD`, presented with a sync interval of 1. The swap chain is created with `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` whenever the system supports it. That flag can only be set at creation, and costs nothing when unused, so a later setting can turn vsync off and allow tearing without recreating anything.
4. **The back buffer is the native resolution.** Its size is the window's client area: the monitor in full screen, the window's inside otherwise. It is resized when that size changes, with the GPU idle. While a window is being dragged to a new size, Windows runs its own modal loop and the game's loop waits; the last frame is stretched to fit (`DXGI_SCALING_STRETCH`) until the drag ends and the back buffers follow. While the window is minimized, nothing is rendered and the loop sleeps until a message arrives. The swap chain's format is `DXGI_FORMAT_R8G8B8A8_UNORM`, and the render target view is `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`, so shaders write linear colors and the hardware encodes them. There is no HDR.
5. **The interface is laid out in a fixed reference frame and scaled to the back buffer.** Because the back buffer follows the monitor, UI positions are never written in back-buffer pixels. They are written in reference units on a 1920×1080 frame, the resolution Q4 is judged at, and each element is anchored to a corner, an edge or the center of the screen. At runtime there is one uniform scale, `min(width / 1920, height / 1080)`. On a 16:9 monitor the reference frame fills the screen at any resolution. On any other aspect ratio, such as a 16:10 or 3:2 laptop panel, the whole reference frame still fits, and the spare width or height goes between the anchors instead of pushing elements off the screen. Text is rasterized at its final size in pixels rather than scaled as a bitmap, so it stays sharp at every scale; how text is drawn is gate G7's decision. Mouse input reaches the interface through the inverse of the same transform, so hit testing uses the reference units too. The scene is independent of all this: it is drawn at the back buffer's resolution through the camera.
6. **The adapter is the high-performance hardware adapter at feature level 11_0 or above.** Adapters come from `IDXGIFactory6::EnumAdapterByGpuPreference` with `DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE`, and software adapters are skipped. Nothing in the MVP needs a feature level above 11_0, and the lowest floor runs on the most hardware. If no adapter qualifies, the game reports it and exits.
7. **Losing the device is fatal.** When `Present`, a queue signal or `ResizeBuffers` returns `DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_RESET` or `DXGI_ERROR_DEVICE_HUNG`, the renderer throws `winrt::hresult_error` carrying `GetDeviceRemovedReason()`. The game reports it and exits. It does not recreate the device.
8. **Everything else fails loudly, in one place.** Every other failed `HRESULT` is thrown by `winrt::check_hresult` (R12). `Present`'s success status codes, such as `DXGI_STATUS_OCCLUDED`, are not failures and are not thrown. `wWinMain` catches `winrt::hresult_error` once, shows its message and code in a message box, and exits with that code, so a failure at startup or a lost device is never a silent exit.
9. **Debug builds turn on the diagnostics.** In Debug, the D3D12 debug layer and DRED (auto-breadcrumbs and page-fault reporting through `ID3D12DeviceRemovedExtendedDataSettings`) are enabled, and the DXGI factory is created with `DXGI_CREATE_FACTORY_DEBUG`. All three need the Windows *Graphics Tools* optional feature. Without it, the game runs without them rather than failing. Release enables none of them.
10. **PIX markers name the frame's regions** (ADR-005): the frame on the CPU, the command list's work on the GPU, and the submission on the queue. They are compiled in for Debug only.

## Consequences

- **Q4 frame time is measured as work, not as the present interval.** With vsync on, a frame that takes 5 ms of work still presents every 16.7 ms at 60 Hz, and one that takes 17 ms presents at 33.3 ms. Task 3.7 therefore records the CPU and GPU time of each frame, and states the development machine's refresh rate: a 120 Hz panel halves the vsync interval.
- **Edge scroll needs the cursor held wherever the screen edge is not the window's edge.** In a window, and on a shared edge between monitors, the cursor leaves the game before it reaches the edge. Task 1.5, the camera, decides whether the cursor is clipped to the window while the game is active, and what edge scroll does in a window.
- **Debugging can happen beside a window.** Alt+Enter before a breakpoint, or at it, leaves the debugger visible on a single-screen machine.
- **Alt+F4 closes the game** until the menu of task 6.2 exists.
- **A driver reset ends the match.** Recovering from a lost device would mean every GPU resource can be rebuilt from CPU-side data, which constrains the whole renderer. Moving to recovery later means refactoring the renderer.
- **The interface has one layout.** It is designed once, at 1920×1080 reference units, and every monitor shows the same layout at a different scale. An aspect ratio narrower than 16:9 shows it smaller, with space above and below.

## What this forecloses

- Exclusive full screen, without changing this ADR.
- Starting on a monitor other than the primary one, and remembering the windowed frame or the mode between runs.
- More than one monitor for the game at once.
- UI positions in back-buffer pixels.
- HDR output.
- Recovering from a lost device.
- A render resolution lower than the display's. If Q4 needs one, the scene can be drawn to a smaller target and scaled, and that is a change to this ADR.
