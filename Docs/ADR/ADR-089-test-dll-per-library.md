# ADR-089 — Each library has a test DLL of its own, which can include only that library and what it builds on

Status: **accepted** · 2026-10-07
Owns: test-dll-per-library, test-placement, test-helpers

## Context

[ADR-002](ADR-002-authoritative-server.md) holds that a boundary between layers is enforced by include paths and not by review (R9). A test DLL can include every header its include path reaches. A test of an engine library in a DLL that also reaches the game could lean on a game concept, and only review would stop it. The boundary holds for the tests too only when each library's tests sit in a DLL whose include path is that library's and what it builds on (owner, 2026-10-07).

## Decision

1. **Each library has one native unit-test DLL of its own** (owner, 2026-10-07): `NeuronCoreTests`, `NeuronServerTests`, `NeuronClientTests`, `GameProtocolTests`, `OpponentTests`, `GameLogicTests` and `GameAppTests`. Each includes and links its own library and what that library builds on, and the unit-test framework's folder. Each is a row in ADR-002's table, in AGENTS.md §2 and in `Build/CheckProjectFiles.py`.
2. **A test lives in the lowest DLL that can include everything it needs.** For example:
   - `NeuronCoreTests`: JSON and its reader, `FileSys`, the server certificate, the QUIC channel and the assertions.
   - `NeuronServerTests`: the tick host and `Random`.
   - `NeuronClientTests`: the mesh and texture readers, the glyph atlas, the bloom and the screen's line segments, with no window and no GPU.
   - `GameProtocolTests`: identifiers, the wire format, placement and the away report.
   - `OpponentTests`: the AI's settings, the `--ai-matches` options, the deputy and the matchup player.
   - `GameLogicTests`: what needs the server. That is the AI playing it ([ADR-020](ADR-020-ai-and-match-flow.md)), `QuicTransport` against a real server, the server made from the packaged data, and the check of the AI's identifiers against the tuning data, in `AiPlayerTests`.
   - `GameAppTests`: GameApp's, among them the checks of every shipped model, which read the models through GameApp's catalog in `ModelCatalogTests`.
3. **A test project's helpers are its own.** A project cannot include another project's headers (ADR-002, `CheckProjectFiles.py`). So `OpponentTests/RepositoryData.h`, `NeuronCoreTests/TemporaryHomeDirectory.h` and `NeuronClientTests/RepositoryAssets.h` copy what they need from `GameLogicTests/RepositoryData.h` and `GameAppTests/RepositoryAssets.h`.
4. **CI runs the DLL of every `*Tests.vcxproj` in the tree**, and fails when one was not built (`.github/workflows/build.yml`).

## Consequences

- **The boundary holds for the tests.** Adding `#include "GameProtocol.h"` to a test in `NeuronCoreTests` fails with C1083.
- **Each test DLL compiles a precompiled header of its own.**
- **`NeuronCoreTests` imports `msquic.dll`**, as `GameLogicTests` does ([ADR-004](ADR-004-quic-transport.md)). It finds it in the shared output folder, where `NeuronCore` copies it.
- **The home-directory helpers exist in three copies**, in `GameLogicTests`, `OpponentTests` and `NeuronCoreTests`. A change to one is made to all three.

## What this forecloses

- A test of an engine library that uses a game type: it does not compile.
- A test of `GameProtocol` or `Opponent` that reaches the server or the client.
- A test project that includes another test project's helpers.
