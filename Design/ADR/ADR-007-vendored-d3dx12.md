# ADR-007 — d3dx12.h, vendored in NeuronClient

Status: **accepted** · 2026-09-30

## Context

R14 allows the Windows SDK, the MSVC standard library and the packages in its table, and named `d3dx12.h` as excluded: it is not SDK content but Microsoft's open-source helper header, shipped with the DirectX Agility SDK and in the DirectX-Headers repository. Without it, resource barriers, descriptor handles and the other D3D12 description structs are written out by hand.

The owner added `d3dx12.h` to `NeuronClient`, with `DirectXHelper.h` around it, on 2026-09-30, and chose to keep it as a vendored file rather than take it from a package or drop it.

## Decision

1. **`NeuronClient/d3dx12.h` is third-party source kept in the tree.** It is Microsoft's, under the MIT licence, whose notice is at the top of the file. It is kept as committed on 2026-09-30 and updated only by replacing it whole with a newer upstream copy. It is never edited here.
2. **It is reached only through `NeuronClient/DirectXHelper.h`**, which defines `D3DX12_NO_STATE_OBJECT_HELPERS` before including it. The state-object helpers are the one part of the header that needs `<wrl/client.h>` and `Microsoft::WRL::ComPtr`, which R12 rules out, so they stay compiled out. `DirectXHelper.h` also defines `IID_GRAPHICS_PPV_ARGS(p)`, which is `IID_PPV_ARGS` for a `winrt::com_ptr` (R12): `__uuidof(p), (p).put_void()`.
3. **No checker applies to it.** `Build/CheckProjectFiles.py` lists it in `VENDORED_FILES`: it must be registered in `NeuronClient` like any file, but R2 (its `CD3DX12_` types), R7 (its lower-case name), R11 and R12 are not checked in it. `Build/CheckFormat.py` leaves its layout alone, and `.clang-tidy`'s `ExcludeHeaderFilterRegex` excludes it. A fix to it would be lost at the next update, so none is made.
4. **Code in this tree may use its helpers.** `CD3DX12_RESOURCE_BARRIER::Transition` and `CD3DX12_CPU_DESCRIPTOR_HANDLE` replace the hand-written barrier and descriptor arithmetic in the renderer. The helpers' names are upstream's and keep their spelling (R4).

## Consequences

- **It reaches every client layer.** `NeuronClient.h` includes `DirectXHelper.h`, so `GameApp` and the executable see d3dx12's names too. They are D3D12 types and functions only, and the server layers cannot include `NeuronClient` (ADR-002).
- **It follows the SDK's `d3d12.h`.** Parts of it are guarded on `D3D12_SDK_VERSION`, and the Windows SDK's `d3d12.h` is what defines that here. Features that need the Agility SDK stay compiled out.
- **An update is a whole-file replacement,** checked by a Debug|x64 build and clang-tidy like any change.

## What this forecloses

- Editing `d3dx12.h` in place.
- The state-object helpers, and with them any WRL, without changing this ADR.
- Other vendored third-party source, each of which needs its own ADR and its own entry in `VENDORED_FILES`.
