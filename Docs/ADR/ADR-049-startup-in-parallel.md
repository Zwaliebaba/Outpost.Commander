# ADR-049 — Startup is measured, and its CPU work overlaps the device behind a hidden window

Status: **accepted** · 2026-10-04
Owns: startup-stages, parallel-startup, hidden-startup-window, shared-dwrite-factory, unzeroed-render-targets

## Context

Startup makes the first match's server, which parses the tuning and the map and builds the pathfinding graphs; the AI's settings; the window; the device and the swap chain; and the client, with its pipeline states, the interface's fonts, every mesh and its crease lines, a sky of 50,000 stars and the particle sprite. Done one after another on one thread, each step waits for the one before, and a window shown full screen meanwhile is black and processes no messages until the first frame.

Most of that is CPU work with no use for the device: parsing, mesh building, rasterizing glyphs and generating stars. Creating the device is the one part of startup the game cannot make cheaper, so the rest can be done while it runs.

Two costs are avoidable whatever the order:

- **An isolated DirectWrite factory** gets the system's font collection without DirectWrite's shared, cross-process font cache. Microsoft's documentation of `DWRITE_FACTORY_TYPE` says the shared factory is the one that takes "advantage of cross process font caching components for better performance".
- **The OS zeroes a render target and a depth buffer when they are created.** The multisampled scene target and its depth buffer are cleared before every use, so the zeroing buys nothing.

`--measure` times frames, ticks and responses; startup needs stages of its own to say where its time goes on a real machine.

## Decision

1. **`--measure` logs startup's stages.** Each `startup_ns <stage> <ns>` line is the time the stage was done, counted from the start of `wWinMain`, so the process's own loading before it is not counted. The stages are `window`, `device`, `server`, `assets`, `interface`, `client` (pipelines built and uploads recorded), `uploads` (the batch of ADR-048 done) and `first_frame` (its `Present` returned). `Tools/FrameTimes.py` prints them.
2. **The CPU work runs on threads of its own while the window and the device are created.** `WinMain` starts three tasks with `std::async`:
   - the first match's server, together with the AI's settings at each difficulty ([ADR-065](ADR-065-ai-difficulty.md));
   - the client's assets: `Outpost::LoadClientAssets` reads the catalog, the camera's settings, every mesh with its pieces and crease lines, the starfield and the particle sprite;
   - the interface's atlas: `Outpost::RasterizeInterface`, at the scale of the window's client area, which is known before the device exists.

   `GameClient` is then built from what they made, inside one batch of uploads. A task's exception reaches the main thread when its result is taken, and is reported there.
3. **The window is created hidden and shown once everything is loaded**, just before the first frame. A failure while loading is reported before the screen goes full screen. A hidden window cannot be marked "Not Responding" while the main thread is busy.
4. **DirectWrite's factory is the shared one.** Its objects may be used from any thread, so the atlas can be made on another one.
5. **Render targets and depth buffers are created with `D3D12_HEAP_FLAG_CREATE_NOT_ZEROED`.** This applies at startup and on every resize. Even so, the OS may zero memory that comes from another process. A runtime older than Windows 10 version 2004 refuses the flag, and the target is created again without it, and the debug layer reports the refused call once per target. The package's minimum is Windows 11 ([ADR-060](ADR-060-quic-in-process.md) decision 8), so no runtime it installs on refuses the flag, and the second attempt is not reached.

## Consequences

- The `startup_ns` lines say where startup's time goes on a real machine. A task's stage time is when it finished, so the overlap shows as stages done before `device`.
- **Pipeline states are created one after another** on the main thread, inside the client's construction, and the `client` stage holds them. Creating them in parallel means restructuring each pipeline class, so it waits until that stage is measured as worth it. The driver's shader cache makes a warm start cheaper than a cold one.
- Loading code must not touch anything that is not thread-safe. `FileSys`'s home directory is set before the first task starts, and is only read after.
- **It leaves open** a loading screen, which would need the device first and could then draw while the tasks finish.

## What this forecloses

- A window that shows anything before the game has loaded.
