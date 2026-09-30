# ADR-005 — PIX event markers in the client, in Debug

Status: **accepted** · 2026-09-30

## Context

The renderer is hand-written D3D12 (R12, R14), and Q4 is a frame-time and tick-time target (design §3). PIX on Windows is Microsoft's debugger and profiler for D3D12. A GPU capture records a frame to debug, and a timing capture shows where the time goes on each thread and on the GPU. Both are much easier to read when the game names its own regions, such as the scene, the HUD and the present, with event markers.

The Windows SDK's own `pix.h` can name GPU regions on a command list or a queue. Its CPU markers, though, are empty functions. `pix_win.h` in SDK 10.0.28000.0 says "Not implemented on Windows". CPU regions, and captures started from code, need the PIX event runtime: `pix3.h` and `WinPixEventRuntime.dll`. That runtime is a NuGet package. R14 as written rules it out, and this ADR is the exception to it.

The owner has added the runtime to `NeuronClient`, and chosen to have markers in Debug only.

## Decision

1. **Event markers come from the PIX event runtime**: the package `WinPixEventRuntime` 1.0.240308001, restored through `NeuronClient/packages.config` into `packages/`, which is not committed.
2. **It lives in `NeuronClient`, and only there.** The package adds its include directory to `NeuronClient`'s include path and no other. So `pix3.h` is included only from `NeuronClient`'s `.cpp` files. Client code outside `NeuronClient` marks its work through `NeuronClient`, not by including `pix3.h`.
3. **Markers are compiled in for Debug only.** This is `pix3.h`'s own default. It defines `USE_PIX` when `_DEBUG` is defined, and in Release every marker is an empty inline function. No project or header defines `USE_PIX`, `USE_PIX_RETAIL` or `PROFILE`, because any of them would turn markers on in Release. The markers therefore follow `_DEBUG` against `NDEBUG`, which is one of the four things AGENTS.md §3 allows to differ.
4. **`pix3.h` is included between `#pragma warning(push)` and `#pragma warning(pop)`.** When markers are compiled out, as in Release, the header disables C4548 and C4555 for the rest of the translation unit. Without the push and pop, every file that includes it would be checked less strictly in Release than in Debug. The push and pop contain the header's own disables rather than adding one, so AGENTS.md §4's rule against silencing a warning holds.
5. **The server does not mark its work.** `NeuronServer` and `GameLogic` cannot include `NeuronClient` (ADR-002), so the tick host has no markers. The tick time for Q4 comes from the server's own timer. If the server's work should show as named regions, the package moves to `NeuronCore` and this ADR changes.

## Consequences

- **Release captures have no named regions.** Q4 is measured on Release (implementation plan 3.7), and Release has no markers. A PIX timing capture of Release still shows the threads, the GPU work and the presents, but not the engine's own regions. So the Q4 figures come from the game's own per-frame timings, written to a file, and a Debug capture is where the regions are read.
- **`NeuronClient` carries the runtime to whatever links it.** The package's targets add `WinPixEventRuntime.lib` to the linker of the project that imports them, but `NeuronClient` is a static library and has no link step. So `NeuronClient` merges `WinPixEventRuntime.lib` into `NeuronClient.lib` (`Lib` `AdditionalDependencies`), and the executable names no package path. A Debug object with a CPU event and a command-list event references `PIXEventsReplaceBlock` and `PIXGetThreadInfo`. The same object built for Release references neither (checked with `dumpbin /symbols` on 2026-09-30). So a Debug executable imports `WinPixEventRuntime.dll` once a marker exists, and a Release one never does.
- **The DLL reaches the package.** The targets add `WinPixEventRuntime.dll` as content, and the build copies it through the project reference into the executable's package. The targets set no configuration condition, so the Release package carries the DLL too, although nothing in Release imports it.
- **The wiring was checked end to end on 2026-09-30,** in Debug and Release on both platforms. A temporary CPU event in a `NeuronClient` `.cpp` made the Debug executable import `WinPixEventRuntime.dll` and left the Release one without it. Both ran from their package layouts. The event was then removed.
- **Both platforms are covered.** The package ships x64 and ARM64 binaries. The copies from the Debug|x64 and Debug|ARM64 builds of 2026-09-30 are `8664` and `AA64` by `dumpbin /headers` (ADR-003).
- **`pix3.h` builds under the project's settings.** This was checked on 2026-09-30 for x64, in Debug and in Release, with `/std:c++latest /permissive- /W4 /WX`. The test translation unit had the Windows macro family, `<windows.h>`, `<d3d12.h>`, and `pix3.h` inside the push and pop, plus one CPU event and one command-list event.
- **The licence travels with the DLL.** The event runtime is MIT-licensed (`license.txt` in the package). `NeuronClient` lists that file as content, and the MSIX package carries it as `Licenses\WinPixEventRuntime.txt`.
- **The PIX tool is not a dependency.** It is installed on the developer's machine, and the game neither links it nor ships it. A GPU capture does not need markers; they only name its regions.

## What this forecloses

- Markers in Release, without changing this ADR. That includes a third build configuration for profiling, which ADR-003's Debug and Release also rule out.
- Code outside `NeuronClient` that calls the event runtime, `pix3.h` in any header, and a package path in any other project.
- Another profiling or capture library, such as Tracy or Optick, without an ADR of its own.
