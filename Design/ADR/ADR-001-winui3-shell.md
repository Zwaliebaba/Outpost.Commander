# ADR-001 — WinUI 3 is the application shell (an exception to R14)

Status: **accepted** · 2026-09-29

## Context

AGENTS.md R14 allows the Windows SDK and the MSVC standard library and nothing else, and §2 says there is no package manager. The game project `OutpostCommander` was created from the WinUI 3 template. It depends on the Windows App SDK and CppWinRT, which ship **only as NuGet packages**. The template brought in 17 packages through `packages.config`.

The alternative was a plain Win32 window that owns an `HWND` and a DXGI swap chain, which is within R14. The owner chose WinUI 3. What it buys is a real UI toolkit — the HUD, the ship designer, menus and text — without writing one. For an RTS, which is UI-heavy, that is a large amount of work saved.

## Decision

1. **The game executable is a WinUI 3 app.** D3D12 renders into a `SwapChainPanel` (swap chain created with `CreateSwapChainForComposition`). XAML draws the HUD and dialogs over it.
2. **R14 gains one exception**: the NuGet packages that WinUI 3 needs, restored through `packages.config` into `packages/` (not committed). Each package is listed below with the reason it is there. **Nothing else comes in through this door.** A new package is a new ADR.

   | Package | Why |
   |---|---|
   | `Microsoft.WindowsAppSDK.WinUI` | WinUI 3 / XAML |
   | `Microsoft.WindowsAppSDK.Foundation`, `.InteractiveExperiences`, `.Base`, `.Runtime` | Required by WinUI (dependencies in its nuspec), plus the runtime framework package |
   | `Microsoft.WindowsAppSDK.DWrite` | Text rendering used by XAML |
   | `Microsoft.Web.WebView2` | Required by `Microsoft.WindowsAppSDK.WinUI` (nuspec dependency). The game does not use WebView2 |
   | `Microsoft.Windows.CppWinRT` | C++/WinRT projection and code generation for XAML |
   | `Microsoft.Windows.ImplementationLibrary` (WIL) | Used by the template's `pch.h` (`wil/cppwinrt_helpers.h`). Header-only |
   | `Microsoft.Windows.SDK.BuildTools`, `.MSIX` | The SDK build tools and MSIX packaging that the Windows App SDK build targets use |

3. **Removed in the same change:** the `Microsoft.WindowsAppSDK` metapackage and the `Widgets`, `AI`, `ML` and `Search` component packages, and `Microsoft.Windows.AI.MachineLearning`. None of them is used. The metapackage exists only to pull in all components. Its props file adds only a project capability, and its targets file only checks that every component is referenced. The ML package was injecting `WindowsMLAutoInitializer.cpp` into our build.
4. **The exception is limited to the application shell.** The engine and simulation libraries stay within R14 as written: Windows SDK and standard library only, no WinRT, no XAML and no NuGet includes. Only the executable project references the packages. This keeps the simulation portable to a dedicated server (ADR-002) and keeps the engine free of the UI framework.
5. **D3D12 is still the renderer, and R14's list for Direct3D is unchanged:** no Agility SDK, no `d3dx12.h`, no DirectXTK. `SwapChainPanel` needs only `dxgi1_6.h` and `microsoft.ui.xaml.media.dxinterop.h`, which the WinUI package supplies.

## Consequences

- AGENTS.md R14 and §2 point to this ADR as the one sanctioned exception.
- CI has to restore NuGet packages before the Windows build (`nuget restore` or `msbuild /t:restore /p:RestorePackagesConfig=true`). A build on a clean clone does not work without it.
- **Composition adds latency.** A `SwapChainPanel` is presented through DWM composition, which typically adds a frame compared with a flip-model swap chain on an `HWND` in exclusive-like mode. For an RTS this is acceptable. For a twitch game it would not be. This figure is not measured here. Measure it when the renderer exists if it becomes a concern.
- Windows App SDK versions move quickly. Updating is a deliberate change, done for all the packages above together, never one at a time.

## Not yet brought into line (known, deliberate follow-ups)

The template project still violates other rules in AGENTS.md. Fixing these is project setup, and it happens when the first real source file lands, not in this change:

- `Win32` and `ARM64` configurations in the project, and `ARM64`/`x86` platforms in `OutpostCommander.slnx` — §3 requires x64 only.
- The `v143` fallback toolset, and `LanguageStandard` `stdcpp20`/`stdcpp17` — §3 requires `v145` and `/std:c++latest`.
- `TreatWarningAsErrors`, `/permissive-`, `/fp:precise` and `/arch:AVX2` are not set explicitly — §3, R16.
- `pch.h` includes `<windows.h>` itself instead of through the one header that owns the Windows macros (§4).
- The `.filters` file has a `Project Files` filter and a `wil` natvis entry pointing outside the repository.
- No `App.xaml` or entry point exists yet, so the project does not link (`LNK2019: WinMain`), before and after this change.

## Open

**Packaged or unpackaged?** The template is MSIX-packaged (`AppxPackage`, `EnableMsixTooling`). Packaged apps must be deployed before they run, which slows the edit–run loop. `WindowsPackageType=None` (unpackaged, using the bootstrapper) runs straight from `x64/Debug`. Decide when milestone 1 is built.

## What this forecloses

- A game that runs on anything but Windows 10 1809 (`10.0.17763`) or later with the Windows App SDK runtime installed.
- Exclusive full-screen presentation. The game runs in a window, borderless or maximised.
- Treating R14 as absolute. It now has one named exception, and the table above is its complete scope.
