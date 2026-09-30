# ADR-001 — A plain Win32 shell, packaged as MSIX

Status: **accepted** · 2026-09-30

## Context

The game executable needs a window to present D3D12 into, and a UI: the HUD, the ship designer, menus and text. There were two ways to get them.

- **WinUI 3.** D3D12 renders into a `SwapChainPanel` and XAML draws the UI over it. What it buys is a UI toolkit the game does not have to write. What it costs is eleven NuGet packages (the Windows App SDK, WinUI, CppWinRT, WIL, WebView2 and DWrite), each of which must also ship ARM64 binaries (ADR-003). It also brings WinRT, XAML, `.idl` files and C++/WinRT's namespaces into the tree. And presentation goes through DWM composition, which typically adds a frame of latency compared with a flip-model swap chain on an `HWND`.
- **Plain Win32.** The executable owns an `HWND` and the game draws its own UI. This stays within R14, apart from packaging.

The project started from the WinUI 3 template. The owner has chosen Win32.

## Decision

1. **The game executable is a plain Win32 application.** It owns an `HWND`, and D3D12 presents to it through a flip-model swap chain (`CreateSwapChainForHwnd`, `DXGI_SWAP_EFFECT_FLIP_DISCARD`). There is no XAML and no WinRT API anywhere in the tree. The one C++/WinRT header in use is `<winrt/base.h>`, for `winrt::com_ptr` and `winrt::check_hresult` on classic COM such as D3D12 (AGENTS.md R12). It comes with the Windows SDK, not in a package, and it brings in no `Windows.*` namespace, no `.idl` and no `cppwinrt.exe`.
2. **The UI is drawn by the game**, over the D3D12 scene: the HUD, the ship designer, menus and text. How it is drawn is a decision for the renderer's own ADR.
3. **The executable is MSIX-packaged** (`AppxPackage`, `EnableMsixTooling`) for day-to-day development, so it runs as it ships, with package identity.
4. **The packaging tools are one of R14's exceptions.** This table is the complete list of them:

   | Package | Why |
   |---|---|
   | `Microsoft.Windows.SDK.BuildTools` | The SDK tools the MSIX build runs, such as `makeappx` and `makepri` |
   | `Microsoft.Windows.SDK.BuildTools.MSIX` | The single-project MSIX build targets |

   Both are build-time tools, restored through `packages.config` into `packages/`, which is not committed. The game compiles and links nothing from them, so they need no ARM64 binaries. Only the executable project references them. R14's other exceptions are libraries the game does link, and each has an ADR of its own: MsQuic in `NeuronCore` (ADR-004) and the PIX event runtime in `NeuronClient` (ADR-005).
5. **The project keeps the template's project type** (`ApplicationType` `Windows Store`, `DesktopCompatible`, not an app container), because it is what the MSIX tooling builds a packaged desktop app from. `CompileAsWinRT` is stated `false` rather than inherited, and the manifest names its entry point, `Windows.FullTrustApplication`. The package requests only `runFullTrust`.

## Consequences

- **The UI is ours to build.** The MVP's HUD and ship designer are in-game panels (design §9). This is the largest cost of the decision.
- **No composition frame.** A flip-model swap chain on an `HWND` can present without DWM composition's extra frame, by independent flip when the window covers the output. Nothing is measured yet: the design's Q4 and Q5 measure frame pacing and order-to-response latency on this shell.
- **A packaged app is registered before it runs.** Visual Studio's F5 registers the layout, and so does `Add-AppxPackage -Register <layout>\AppxManifest.xml`. The edit–run loop is slower than an unpackaged executable's. CI builds the executable but does not register or run it.
- **CI restores `packages.config`** in a step of its own before the Windows build. A clean clone does not build without it.
- **R14's Direct3D list is unchanged:** no Agility SDK, no `d3dx12.h`, no DirectXTK.
- **COM through `<winrt/base.h>` links three system libraries.** `NeuronCore.h` includes the header and names `ole32.lib`, `oleaut32.lib` and `runtimeobject.lib` with `#pragma comment(lib)`, so every binary that includes it links them. In Debug the header alone references `IIDFromString` (ole32), for its debugger support. `check_hresult` references `GetErrorInfo`, `SysFreeString` and `SysStringLen` (oleaut32), and `RoOriginateLanguageException` (runtimeobject). This was checked on 2026-09-30 in Debug and Release on both platforms, compiled with `/std:c++latest /permissive- /W4 /WX`. A test program caught `check_hresult(E_INVALIDARG)` as `0x80070057`, and so did a temporary call in the executable. With nothing calling it yet, the executable imports only `ole32.dll`, and only in Debug.
- **There is one C++/WinRT, the SDK's.** Both installed SDKs carry version 2.0.250303.5. The header's `detect_mismatch` makes mixing two versions in one binary a link error, so the `Microsoft.Windows.CppWinRT` package stays out. The Windows Store project type adds the executable's own folder, `Generated Files\` and its intermediate folder to the executable's include path. On 2026-09-30, a C++/WinRT 3.0 left in `Generated Files\` by the WinUI template broke the link that way. So the executable does not inherit `%(AdditionalIncludeDirectories)`, and its include path is exactly its row in ADR-002.

## What this forecloses

- **XAML, and WinRT APIs in general.** Input, audio and windowing use the Win32 and DirectX APIs the Windows SDK ships, such as XInput. A WinRT-only API, for example `Windows.Gaming.Input`, needs this ADR changed.
- **`Microsoft::WRL::ComPtr`, and the C++/WinRT package.** COM uses the SDK's `<winrt/base.h>` (AGENTS.md R12).
- **An unpackaged build** for day-to-day development, and any packaging tool beyond the two above, without changing this ADR. Any other package needs an ADR of its own, as ADR-004 and ADR-005 are.
- **Windows before 10 1809** (`10.0.17763`), the package's minimum version.
