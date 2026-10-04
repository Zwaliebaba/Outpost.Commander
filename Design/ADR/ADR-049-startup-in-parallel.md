# ADR-049 — Startup is measured, and its CPU work overlaps the device behind a hidden window

Status: **accepted** · 2026-10-03

## Context

Startup ran each step after the one before, on one thread:

1. the first match's server, which parses the tuning and the map and builds the pathfinding graphs;
2. the AI's settings;
3. the window, shown full screen and black, with no messages processed until the first frame;
4. the device and the swap chain;
5. the client.

The client's own construction, in turn, did these one after another:

- the pipeline states;
- seven fonts, each through a DirectWrite factory of its own;
- 21 meshes and their crease lines;
- a sky of 50,000 stars;
- the particle sprite.

Most of that is CPU work with no use for the device: parsing, mesh building, rasterizing glyphs and generating stars. The device creation it waited behind is the one part of startup the game cannot make cheaper.

Two of the costs were avoidable whatever the order.

- **`RasterizeFont` created an isolated DirectWrite factory for each font**, and an isolated factory gets the system's font collection without DirectWrite's shared, cross-process font cache. Microsoft's documentation of `DWRITE_FACTORY_TYPE` says the shared factory is the one that takes "advantage of cross process font caching components for better performance".
- **The multisampled scene target and depth buffer were zeroed by the OS when created.** They are cleared before every use, so the zeroing buys nothing. At 4K the two hold about 266 MB.

Nothing here was measured on Windows. `--measure` could time frames, ticks and responses, but not startup, so this ADR also adds that.

## Decision

1. **`--measure` logs startup's stages.** Each `startup_ns <stage> <ns>` line is the time the stage was done, counted from the start of `wWinMain`, so the process's own loading before it is not counted. The stages are `window`, `device`, `server`, `assets`, `interface`, `client` (pipelines built and uploads recorded), `uploads` (the batch of ADR-048 done) and `first_frame` (its `Present` returned). `Tools/FrameTimes.py` prints them.
2. **The CPU work runs on threads of its own while the window and the device are created.** `WinMain` starts three tasks with `std::async`:
   - the first match's server, together with the AI's settings;
   - the client's assets: `Outpost::LoadClientAssets` reads the catalog, the camera's settings, every mesh with its pieces and crease lines, the starfield and the particle sprite;
   - the interface's atlas: `Outpost::RasterizeInterface`, at the scale of the window's client area, which is known before the device exists.

   `GameClient` is then built from what they made, inside one batch of uploads. A task's exception reaches the main thread when its result is taken, and is reported as before.
3. **The window is created hidden and shown once everything is loaded**, just before the first frame. A failure while loading is still reported before the screen goes full screen, which is why the server used to be made before the window. A hidden window cannot be marked "Not Responding" while the main thread is busy.
4. **DirectWrite's factory is the shared one.** Its objects may be used from any thread, so the atlas can be made on another one.
5. **Render targets and depth buffers are created with `D3D12_HEAP_FLAG_CREATE_NOT_ZEROED`.** This applies at startup and on every resize. The OS may still zero memory that comes from another process. A runtime older than Windows 10 version 2004 refuses the flag, and the target is created again without it, and the debug layer reports the refused call once per target. The package's minimum is Windows 11 since ADR-060, so no runtime it installs on refuses the flag, and the second attempt is not reached.

## Consequences

- The `startup_ns` lines say where startup's time goes on a real machine. A task's stage time is when it finished, so the overlap shows as stages done before `device`.
- **Pipeline states are still created one after another** on the main thread, inside the client's construction, and the `client` stage holds them. Creating them in parallel means restructuring each pipeline class, so it waits until that stage is measured as worth it. The driver's shader cache makes a warm start cheaper than a cold one.
- Loading code must not touch anything that is not thread-safe. `FileSys`'s home directory is set before the first task starts, and is only read after.
- **Forecloses** a window that shows anything before the game has loaded. **Leaves open** a loading screen, which would need the device first and could then draw while the tasks finish.
