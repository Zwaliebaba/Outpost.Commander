# ADR-001 — WinUI 3 is the application shell (an exception to R14)

Status: **accepted** · 2026-09-29 · Packaging decided and the template brought into line 2026-09-30

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
- CI restores the NuGet packages in a step of its own before the Windows build: `nuget restore` for each `packages.config`, into `packages/`. A build on a clean clone does not work without it. The step was missing when this ADR was accepted, so CI stayed red on the missing packages until the milestone 1 scaffold added it.
- **Composition adds latency.** A `SwapChainPanel` is presented through DWM composition, which typically adds a frame compared with a flip-model swap chain on an `HWND` in exclusive-like mode. For an RTS this is acceptable. For a twitch game it would not be. This figure is not measured here. Measure it when the renderer exists if it becomes a concern.
- Windows App SDK versions move quickly. Updating is a deliberate change, done for all the packages above together, never one at a time.

## Brought into line with the milestone 1 scaffold (2026-09-30)

When this ADR was accepted, the template project still broke other rules in AGENTS.md. The milestone 1 scaffold is the first real source in the project, and it fixed them:

- **x64 only.** The `Win32` and `ARM64` configurations are gone from the project, and the `ARM64` and `x86` platforms from `OutpostCommander.slnx` (§3).
- **Toolset and standard.** The toolset is `v145` with no `v143` fallback, and `/std:c++latest` replaces `stdcpp20`/`stdcpp17` (§3).
- **Compiler settings.** `TreatWarningAsError`, `ConformanceMode` (`/permissive-`), `/fp:precise` and `/arch:AVX2` are stated once in the project, for both configurations (§3, R16).
- **Windows macros.** `pch.h` includes `framework.h`, the one header that owns the Windows macro family, instead of `<windows.h>` (§4).
- **Filters.** They are functional: `Shell`, `Platform` and `Package`. The `Project Files` filter is gone, and so are the `wil` natvis entries that pointed outside the repository (§2).
- **Entry point.** `App.xaml` and a `MainWindow` holding the `SwapChainPanel` give the executable the entry point the XAML compiler generates. Before this, it did not link (`LNK2019: WinMain`).

## What the shell brings into the tree

- **Two non-C++ source kinds, in the executable project only.** These are `.xaml` markup and `.idl` runtime-class definitions. Code-behind is named `<Type>.xaml.h` and `<Type>.xaml.cpp`, because the files the XAML compiler generates include those names. R7's `.h`/`.cpp` rule governs C++ source, and no library project has these files.
- **Three namespaces that C++/WinRT requires by name:** `winrt`, `implementation` and `factory_implementation`. They keep the SDK's spelling (R4), and `.clang-tidy` exempts exactly these three from the CamelCase namespace rule.
- **The executable is registered in `.clang-tidy`'s header filter.** The filter admits `.xaml.h`, so the code-behind headers are linted like any other.

## Packaged, decided 2026-09-30

The owner kept MSIX packaging (`AppxPackage`, `EnableMsixTooling`) for day-to-day development.

- **What it buys:** the game runs as it will ship, with package identity, and the Windows App SDK runtime arrives as a framework-package dependency with no bootstrapper.
- **What it costs:** a packaged app must be registered before it runs. Visual Studio's F5 registers the layout, and from the command line `Add-AppxPackage -Register <layout>\AppxManifest.xml` does. Either way, the edit–run loop is slower than an unpackaged executable run straight from `x64/Debug`.
- CI builds the executable. It does not register or run the package.

## What this forecloses

- A game that runs on anything but Windows 10 1809 (`10.0.17763`) or later with the Windows App SDK runtime installed.
- Exclusive full-screen presentation. The game runs in a window, borderless or maximised.
- Treating R14 as absolute. It now has one named exception, and the table above is its complete scope.
