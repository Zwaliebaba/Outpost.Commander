# ADR-001 — A plain Win32 shell, packaged as MSIX

Status: **accepted** · 2026-09-30

## Context

The game executable needs a window to present D3D12 into, and a UI: the HUD, the ship designer, menus and text. There were two ways to get them.

- **WinUI 3.** D3D12 renders into a `SwapChainPanel` and XAML draws the UI over it. What it buys is a UI toolkit the game does not have to write. What it costs is eleven NuGet packages (the Windows App SDK, WinUI, CppWinRT, WIL, WebView2 and DWrite), each of which must also ship ARM64 binaries (ADR-003). It also brings WinRT, XAML, `.idl` files and C++/WinRT's namespaces into the tree. And presentation goes through DWM composition, which typically adds a frame of latency compared with a flip-model swap chain on an `HWND`.
- **Plain Win32.** The executable owns an `HWND` and the game draws its own UI. This stays within R14, apart from packaging.

The project started from the WinUI 3 template. The owner has chosen Win32.

## Decision

1. **The game executable is a plain Win32 application.** It owns an `HWND`, and D3D12 presents to it through a flip-model swap chain (`CreateSwapChainForHwnd`, `DXGI_SWAP_EFFECT_FLIP_DISCARD`). There is no WinRT and no XAML anywhere in the tree.
2. **The UI is drawn by the game**, over the D3D12 scene: the HUD, the ship designer, menus and text. How it is drawn is a decision for the renderer's own ADR.
3. **The executable is MSIX-packaged** (`AppxPackage`, `EnableMsixTooling`) for day-to-day development, so it runs as it ships, with package identity.
4. **R14's one exception is the packaging tools.** This table is the complete list:

   | Package | Why |
   |---|---|
   | `Microsoft.Windows.SDK.BuildTools` | The SDK tools the MSIX build runs, such as `makeappx` and `makepri` |
   | `Microsoft.Windows.SDK.BuildTools.MSIX` | The single-project MSIX build targets |

   Both are build-time tools, restored through `packages.config` into `packages/`, which is not committed. The game compiles and links nothing from them, so no package has to ship ARM64 binaries for the game to run on ARM64. Only the executable project references them.
5. **The project keeps the template's project type** (`ApplicationType` `Windows Store`, `DesktopCompatible`, not an app container), because it is what the MSIX tooling builds a packaged desktop app from. `CompileAsWinRT` is stated `false` rather than inherited, and the manifest names its entry point, `Windows.FullTrustApplication`. The package requests only `runFullTrust`.

## Consequences

- **The UI is ours to build.** The MVP's HUD and ship designer are in-game panels (design §9). This is the largest cost of the decision.
- **No composition frame.** A flip-model swap chain on an `HWND` can present without DWM composition's extra frame, by independent flip when the window covers the output. Nothing is measured yet: the design's Q4 and Q5 measure frame pacing and order-to-response latency on this shell.
- **A packaged app is registered before it runs.** Visual Studio's F5 registers the layout, and so does `Add-AppxPackage -Register <layout>\AppxManifest.xml`. The edit–run loop is slower than an unpackaged executable's. CI builds the executable but does not register or run it.
- **CI restores `packages.config`** in a step of its own before the Windows build. A clean clone does not build without it.
- **R14's Direct3D list is unchanged:** no Agility SDK, no `d3dx12.h`, no DirectXTK.

## What this forecloses

- **XAML, and WinRT APIs in general.** Input, audio and windowing use the Win32 and DirectX APIs the Windows SDK ships, such as XInput. A WinRT-only API, for example `Windows.Gaming.Input`, needs this ADR changed.
- **An unpackaged build** for day-to-day development, and any package beyond the two tools above, without changing this ADR.
- **Windows before 10 1809** (`10.0.17763`), the package's minimum version.
