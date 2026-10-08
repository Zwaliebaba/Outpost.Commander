# ADR-075 — Each library has a test DLL of its own, which can include only that library and what it builds on

Status: **accepted** · 2026-10-07

## Context

[ADR-002](ADR-002-authoritative-server.md) placed two test DLLs: `GameLogicTests` and `GameAppTests`. The tests of the other five libraries lived in whichever of the two could reach them:

- `JsonTests`, `RandomTests`, `TickHostTests`, `WireFormatTests` and `IdTests` were in `GameLogicTests`.
- `MeshDataTests`, `TextureDataTests`, `GlyphAtlasTests` and `PlacementTests` were in `GameAppTests`.

A test of the engine there could include every game header its DLL could. Only review stopped a test of `NeuronCore` from leaning on a game concept, while ADR-002 holds that a boundary is enforced by include paths and not by review (R9).

The owner asked for a test DLL per library on 2026-10-07.

## Decision

1. **Five more native unit-test DLLs:** `NeuronCoreTests`, `NeuronServerTests`, `NeuronClientTests`, `GameProtocolTests` and `OpponentTests`. Each includes and links its own library and what that library builds on, and the unit-test framework's folder. Each is a row in ADR-002's table, in AGENTS.md §2 and in `Build/CheckProjectFiles.py`.
2. **A test lives in the lowest DLL that can include everything it needs.**
   - `NeuronCoreTests`: JSON and its reader, `FileSys`, the server certificate and the QUIC channel.
   - `NeuronServerTests`: the tick host and `Random`.
   - `NeuronClientTests`: the mesh and texture readers and the glyph atlas, with no window and no GPU.
   - `GameProtocolTests`: identifiers, the wire format and placement.
   - `OpponentTests`: the AI's settings and the `--ai-matches` options.
   - `GameLogicTests` keeps what needs the server. That is the AI playing it ([ADR-020](ADR-020-ai-and-match-flow.md)), `QuicTransport` against a real server, the server made from the packaged data, and the check of the AI's identifiers against the tuning data, which moved into `AiPlayerTests`.
   - `GameAppTests` keeps GameApp's tests. The checks of every shipped model read the models through GameApp's catalog, so they moved from `MeshDataTests` into `ModelCatalogTests`.
3. **A test project's helpers are its own.** A project cannot include another project's headers (ADR-002, `CheckProjectFiles.py`). So `OpponentTests/RepositoryData.h`, `NeuronCoreTests/TemporaryHomeDirectory.h` and `NeuronClientTests/RepositoryAssets.h` copy what they need from `GameLogicTests/RepositoryData.h` and `GameAppTests/RepositoryAssets.h`.
4. **CI is unchanged.** It runs the DLL of every `*Tests.vcxproj` in the tree, and fails when one was not built (`.github/workflows/build.yml`).

## Consequences

- **The boundary was checked.** Adding `#include "GameProtocol.h"` to a test in `NeuronCoreTests` fails with C1083 (2026-10-07).
- **Measured on 2026-10-07, counting `TEST_METHOD` in each project:** 37 in `NeuronCoreTests`, 11 in `NeuronServerTests`, 32 in `NeuronClientTests`, 16 in `GameProtocolTests`, 22 in `OpponentTests`, 258 in `GameLogicTests` and 235 in `GameAppTests`. That makes 611, the same set of test names as the two DLLs held before the split.
- **Each DLL compiles a precompiled header of its own**, so CI's Debug|x64 build does five more.
- **`NeuronCoreTests` imports `msquic.dll`**, as `GameLogicTests` does ([ADR-004](ADR-004-quic-transport.md)). It finds it in the shared output folder, where `NeuronCore` copies it.
- **The home-directory helpers exist in three copies**, in `GameLogicTests`, `OpponentTests` and `NeuronCoreTests`. A change to one is made to all three.

## What this forecloses

- A test of an engine library that uses a game type: it does not compile.
- A test of `GameProtocol` or `Opponent` that reaches the server or the client.
- A test project that includes another test project's helpers.
