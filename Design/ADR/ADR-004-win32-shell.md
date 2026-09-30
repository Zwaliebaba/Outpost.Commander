# ADR-004 — A plain Win32 shell, packaged as MSIX

Status: **accepted** · 2026-09-30 · Supersedes ADR-001

## Context

ADR-001 made the game executable a WinUI 3 app. D3D12 was to render into a `SwapChainPanel` with XAML drawn over it, and the Windows App SDK's NuGet packages came in as R14's one exception. The owner has since taken the XAML shell out (`App`, `MainWindow`). The executable is a bare `wWinMain` over NeuronCore and the libraries of ADR-002, and nothing in the tree uses XAML.

What ADR-001 bought, a UI toolkit, is no longer used. What it cost is still being paid:

- eleven NuGet packages, restored on every clean build and required, by ADR-003, to ship ARM64 binaries;
- WinRT headers in the executable's `pch.h`, and WinRT and XAML allowances in `.clang-tidy`;
- composition, which ADR-001 notes typically presents a frame later than a flip-model swap chain on an `HWND`. Nobody has measured that here.

The owner chose a plain Win32 shell, still packaged.

## Decision

1. **The game executable is a plain Win32 application.** It owns an `HWND`, and D3D12 presents to it through a flip-model swap chain (`CreateSwapChainForHwnd`, `DXGI_SWAP_EFFECT_FLIP_DISCARD`). There is no WinRT and no XAML anywhere in the tree.
2. **The UI is drawn by the game**, over the D3D12 scene: the HUD, the ship designer, menus and text. How it is drawn is a decision for the renderer's own ADR.
3. **The executable stays MSIX-packaged** (`AppxPackage`, `EnableMsixTooling`), as the owner decided under ADR-001, so it runs as it ships, with package identity.
4. **R14's exception shrinks to the packaging tools.** This table is the complete list:

   | Package | Why |
   |---|---|
   | `Microsoft.Windows.SDK.BuildTools` | The SDK tools the MSIX build runs, such as `makeappx` and `makepri` |
   | `Microsoft.Windows.SDK.BuildTools.MSIX` | The single-project MSIX build targets |

   Both are build-time tools. The game compiles and links nothing from them. The Windows App SDK packages, WinUI, CppWinRT, WIL, WebView2 and DWrite are removed.
5. **The project keeps the template's project type** (`ApplicationType` `Windows Store`, `DesktopCompatible`, not an app container), because it is what the MSIX tooling builds a packaged desktop app from. `CompileAsWinRT` is stated `false` rather than inherited.
6. **The manifest names its entry point**, `Windows.FullTrustApplication`, instead of the `$targetentrypoint$` token, whose substitution came with the removed packages.

## Consequences

- **The UI is ours to build.** The MVP's HUD and ship designer become in-game panels (design §9). This is the largest cost of the decision.
- **The composition frame can go.** A flip-model swap chain on an `HWND` can present without DWM composition's extra frame, by independent flip when the window covers the output. Nothing is measured yet: the design's Q4 and Q5 measure frame pacing and order-to-response latency on this shell.
- **Packaging costs are unchanged from ADR-001.** A packaged app is registered before it runs: Visual Studio's F5 does it, and so does `Add-AppxPackage -Register <layout>\AppxManifest.xml`. The edit–run loop stays slower than an unpackaged executable's.
- **ADR-003's rule on packages now covers two build-time tools.** Nothing the game links comes from a package, so no package has to ship ARM64 binaries for the game to run on ARM64.
- **CI still restores `packages.config`**, which now lists the two tools.
- **`.clang-tidy` loses the allowances ADR-001 added:** `.xaml.h` headers in the header filter, and the three namespaces C++/WinRT required.
- **ADR-001 stays in the tree as history,** marked superseded.

## What this forecloses

- **XAML, and WinRT APIs in general.** Input, audio and windowing use the Win32 and DirectX APIs the Windows SDK ships, such as XInput. A WinRT-only API, for example `Windows.Gaming.Input`, needs a new ADR.
- **An unpackaged build** for day-to-day development, without a new ADR.
- **Any package beyond the two tools above**, without a new ADR.
