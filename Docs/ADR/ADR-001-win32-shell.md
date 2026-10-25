# ADR-001 — A plain Win32 shell, packaged as MSIX

Status: **accepted** · 2026-09-30
Owns: win32-shell, msix-packaging, winrt-base-for-com

## Context

The game executable needs a window to present D3D12 into, and a UI: the HUD, the ship designer, menus and text. There are two ways to get them.

- **WinUI 3.** D3D12 renders into a `SwapChainPanel` and XAML draws the UI over it. What it buys is a UI toolkit the game does not have to write. What it costs is a set of NuGet packages, the Windows App SDK, WinUI, CppWinRT, WIL, WebView2 and DWrite among them, each of which must also ship ARM64 binaries ([ADR-003](ADR-003-x64-and-arm64.md)). It also brings WinRT, XAML, `.idl` files and C++/WinRT's namespaces into the tree. And presentation goes through DWM composition, which typically adds a frame of latency compared with a flip-model swap chain on an `HWND`.
- **Plain Win32.** The executable owns an `HWND` and the game draws its own UI. This stays within R14, apart from packaging.

## Decision

1. **The game executable is a plain Win32 application** (owner, 2026-09-30). It owns an `HWND`, which D3D12 presents to ([ADR-006](ADR-006-renderer-shape.md) decision 3). There is no XAML and no WinRT API anywhere in the tree. The one C++/WinRT header in use is `<winrt/base.h>`, for `winrt::com_ptr`, `winrt::check_hresult` and the other COM and error helpers it holds, on classic COM such as D3D12 (AGENTS.md R12). It comes with the Windows SDK, not in a package, and it brings in no `Windows.*` namespace, no `.idl` and no `cppwinrt.exe`.
2. **The UI is drawn by the game**, over the D3D12 scene: the HUD, the ship designer, menus and text. How it is drawn is [ADR-015](ADR-015-ui-drawing.md)'s, and [ADR-006](ADR-006-renderer-shape.md) decision 5 fixes the reference frame it is laid out in.
3. **The executable is MSIX-packaged** (`AppxPackage`, `EnableMsixTooling`) for day-to-day development, so it runs as it ships, with package identity.
4. **The packaging tools are one of R14's exceptions.** This table is the complete list of them:

   | Package | Why |
   |---|---|
   | `Microsoft.Windows.SDK.BuildTools` | The SDK tools the MSIX build runs, such as `makeappx` and `makepri` |
   | `Microsoft.Windows.SDK.BuildTools.MSIX` | The single-project MSIX build targets |

   Both are build-time tools, restored through `packages.config` into `packages/`, which is not committed. The game compiles and links nothing from them, so they need no ARM64 binaries. Only the executable project references them. R14's other exceptions are libraries the game does link, and each has an ADR of its own: MsQuic in `NeuronCore` ([ADR-004](ADR-004-quic-transport.md)) and the PIX event runtime in `NeuronClient` ([ADR-005](ADR-005-pix-markers.md)), and one vendored header, `d3dx12.h` in `NeuronClient` ([ADR-007](ADR-007-vendored-d3dx12.md)).
5. **The project's type is the Windows Store project type** (`ApplicationType` `Windows Store`, `DesktopCompatible`, not an app container), because it is what the MSIX tooling builds a packaged desktop app from. `CompileAsWinRT` is stated `false` rather than inherited, and the manifest names its entry point, `Windows.FullTrustApplication`. The package requests only `runFullTrust`.

## Consequences

- **The UI is ours to build.** The HUD, the windows and the menus are in-game panels. This is the largest cost of the decision.
- **No composition frame.** A flip-model swap chain on an `HWND` can present without DWM composition's extra frame, by independent flip when the window covers the output. Frame pacing and order-to-response latency on this shell are budgets ([`Docs/Budgets.md`](../Budgets.md)).
- **A packaged app is registered before it runs.** Visual Studio's F5 registers the layout, and so does `Add-AppxPackage -Register <layout>\AppxManifest.xml`. The edit–run loop is slower than an unpackaged executable's. CI builds the executable but does not register or run it.
- **CI restores `packages.config`** in a step of its own before the Windows build. A clean clone does not build without it.
- **R14's Direct3D list stands, apart from `d3dx12.h`,** vendored under [ADR-007](ADR-007-vendored-d3dx12.md): no Agility SDK, no DirectXTK.
- **COM through `<winrt/base.h>` links three system libraries.** `NeuronCore.h` includes the header and names `ole32.lib`, `oleaut32.lib` and `runtimeobject.lib` with `#pragma comment(lib)`, so every binary that includes it links them. In Debug the header alone references `IIDFromString` (ole32), for its debugger support. `check_hresult` references `GetErrorInfo`, `SysFreeString` and `SysStringLen` (oleaut32), and `RoOriginateLanguageException` (runtimeobject).
- **There is one C++/WinRT, the SDK's.** The header's `detect_mismatch` makes mixing two versions in one binary a link error, so the `Microsoft.Windows.CppWinRT` package stays out. The Windows Store project type adds the executable's own folder, `Generated Files\` and its intermediate folder to the executable's include path, and a C++/WinRT left in `Generated Files\` would be found before the SDK's. So the executable does not inherit `%(AdditionalIncludeDirectories)`, and its include path is exactly its row in [ADR-002](ADR-002-authoritative-server.md).

## What this forecloses

- **XAML, and WinRT APIs in general.** Input, audio and windowing use the Win32 and DirectX APIs the Windows SDK ships, such as XInput. A WinRT-only API, for example `Windows.Gaming.Input`, needs this ADR changed.
- **`Microsoft::WRL::ComPtr`, and the C++/WinRT package.** COM uses the SDK's `<winrt/base.h>` (AGENTS.md R12).
- **An unpackaged build** for day-to-day development, and any packaging tool beyond the two above, without changing this ADR. Any other package needs an ADR of its own, as [ADR-004](ADR-004-quic-transport.md) and [ADR-005](ADR-005-pix-markers.md) are.
- **Windows before 11**, the package's minimum ([ADR-060](ADR-060-quic-in-process.md) decision 8).
