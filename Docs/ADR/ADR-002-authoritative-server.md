# ADR-002 — An authoritative server that clients reach only through commands and snapshots

Status: **accepted** · 2026-10-08
Owns: authoritative-server, command-and-snapshot-boundary, transport-interface, project-layers, include-path-boundary

## Context

The two usual RTS network models make different demands on the code:

- **Lockstep** sends only commands. Every peer simulates, so the simulation must be bit-for-bit deterministic across machines: fixed-point or strictly controlled floats, ordered iteration, and a seeded PRNG (AGENTS.md R16). One divergence desynchronises the match.
- **Authoritative server** simulates in one place and sends state to the clients. The simulation does not need to be deterministic across machines, because only one machine runs it. The costs are bandwidth for state, and latency between an order and its visible effect.

The game is **server-authoritative**. Its server runs inside the game's process for a skirmish and in a process of its own for a world ([ADR-078](ADR-078-dedicated-server.md)), so the code is shaped for the server to sit in either place: the boundary between it and its clients is a transport, and where the server runs is a transport's concern, not the game's.

## Decision

1. **The simulation is a separate layer with no knowledge of the client.** It owns all game state and rules. It includes no renderer, XAML, WinRT API or window headers, only the Windows SDK and the standard library at most (R9, R14). It builds as a static library, `GameLogic`, which the game executable links for its in-process server and the dedicated server links for a world ([ADR-078](ADR-078-dedicated-server.md) decision 1).
2. **State changes only through commands.** A client sends a `Command` — move, attack, build, queue, research, save design and so on — tagged with its player. The server validates it against ownership, cost and legality, and applies it at the start of a tick, or rejects it. Nothing outside the server mutates simulation state. This holds for the human player, the AI and any debug tool.
3. *(retired)*
4. **Clients receive snapshots, addressed per player.** After each tick the server produces a snapshot for each player, holding what that player may see. Fog of war is applied there, on the server only ([ADR-024](ADR-024-fog-of-war.md)).
5. **The client renders from snapshots, never from server state.** It shows the world a tick behind the newest snapshot, interpolated between the two around that moment ([ADR-013](ADR-013-client-view-and-controls.md) decision 3). It predicts nothing: an order shows its effect once the snapshot of the tick that applied it arrives.
6. **Commands and snapshots cross a `Transport`.** The command and snapshot types are plain data, with no pointers into server state and entities referred to by ID, so that a transport can serialize them without changing their meaning. Which runs cross a `QuicTransport` and which the in-process `LoopbackTransport` is [ADR-060](ADR-060-quic-in-process.md) decision 1's, and a joined world's connection [ADR-078](ADR-078-dedicated-server.md) decision 9's.
7. **The AI reaches the server as a player does, never through its internals.** In the game it is a client: it gets its player's snapshot and sends commands through a `Transport`. On a world's server it is a `HostedPlayer`, handed its player's snapshot after each tick and returning orders that the next tick applies as a connection's ([ADR-079](ADR-079-seat-controller-and-deputy.md) decisions 1 to 3). Either way it has no access to server state, and its library's include path is what enforces that (decision 9).
8. *(retired)*
9. **The layers, and the include paths that enforce them.** The engine is split three ways, into what both sides share, what only the client needs and what only the server needs. The game is split the same way, with one shared library in the middle.

   ```
   OutpostCommander (exe, Win32)    ── the shell: WinMain, the window, MSIX packaging (ADR-001). Wires the pieces together
    ├── GameApp      (static lib)    ── client game: presentation, selection, camera, UI state. → NeuronClient, GameProtocol
    ├── Opponent     (static lib)    ── the AI player, and a seat's deputy (ADR-079). → GameProtocol only
    ├── GameLogic    (static lib)    ── the server: state, rules, LoopbackTransport, its QUIC seats. → NeuronServer, GameProtocol
    ├── GameProtocol (static lib)    ── commands, snapshots, entity IDs, their wire format, QuicTransport, the in-process server's factory. → NeuronCore
    ├── NeuronClient (static lib)    ── client engine: D3D12, input, audio, PIX markers (ADR-005). → NeuronCore
    ├── NeuronServer (static lib)    ── server engine: the tick host and the pinned PRNG (ADR-009). → NeuronCore
    └── NeuronCore   (static lib)    ── engine code shared by client and server, including QUIC (ADR-004)
   OutpostServer (exe, console)     ── the dedicated server: runs a world from its folder (ADR-078), its AI empires and deputies (ADR-079). → GameLogic, NeuronServer, Opponent
   GameLogicTests (test DLL)         ── drives GameLogic through GameProtocol: the balance check's battles, and the AI playing the real server (ADR-020)
   GameAppTests (test DLL)           ── drives GameApp's camera math, model data and interface without a GPU
   NeuronCoreTests, NeuronServerTests, NeuronClientTests, GameProtocolTests, OpponentTests (test DLLs)
                                     ── each drives its own library, with only what that library builds on (ADR-089)
   ```

   Each library has a master header named after it, and its `pch.h` includes that header. `NeuronClient.h`, `NeuronServer.h` and `GameProtocol.h` include `NeuronCore.h`. `Opponent.h` includes `GameProtocol.h`. `GameLogic.h` includes `NeuronServer.h` and `GameProtocol.h`, and `GameApp.h` includes `NeuronClient.h` and `GameProtocol.h`.

   **The `Neuron*` libraries know no game concept (R9).** A command or a snapshot is a game concept, so the types that cross the boundary live in `GameProtocol` and not in `NeuronCore`. They cannot live in `GameLogic` either, because the client would then have to include server headers, and they cannot live in `GameApp`, because the server cannot see it.

   **Three projects reference packages, each for a reason recorded in an ADR.** The executable restores the MSIX packaging tools ([ADR-001](ADR-001-win32-shell.md)). `NeuronCore` references MsQuic, because the client and the dedicated server both need QUIC ([ADR-004](ADR-004-quic-transport.md)). `NeuronClient` references the PIX event runtime, which the server never links ([ADR-005](ADR-005-pix-markers.md)). No other library references a package, and no project includes XAML or a WinRT API. What Windows the dedicated server needs for MsQuic's Schannel build is [ADR-078](ADR-078-dedicated-server.md)'s. A package's include directory is on its own project's include path only, so `msquic.h` and `pix3.h` never appear in a master header. `NeuronCore.h` in particular includes no XAML, WinRT API or package header. It does include C++/WinRT's `<winrt/base.h>` from the Windows SDK, so every layer shares one COM pointer and one `HRESULT` check (AGENTS.md R12, [ADR-001](ADR-001-win32-shell.md)). It is also the one header that owns the Windows macro family (AGENTS.md §4).

   **The boundary is enforced by include paths, not by review.** A project's include path lists only the projects it may include (AGENTS.md §3):

   | Project | Include path | Links |
   |---|---|---|
   | NeuronCore | — | — |
   | NeuronClient, NeuronServer, GameProtocol | NeuronCore | — |
   | Opponent | NeuronCore, GameProtocol | — |
   | GameLogic | NeuronCore, NeuronServer, GameProtocol | — |
   | GameApp | NeuronCore, NeuronClient, GameProtocol | — |
   | OutpostCommander | NeuronCore, NeuronClient, GameProtocol, Opponent, GameApp | all seven libraries |
   | OutpostServer | NeuronCore, NeuronServer, GameProtocol, GameLogic, Opponent | NeuronCore, NeuronServer, GameProtocol, GameLogic, Opponent |
   | GameLogicTests | NeuronCore, NeuronServer, GameProtocol, GameLogic, Opponent, and the unit-test framework's folder in the Visual Studio install | NeuronCore, NeuronServer, GameProtocol, GameLogic, Opponent |
   | GameAppTests | NeuronCore, NeuronClient, GameProtocol, GameApp, and the unit-test framework's folder | NeuronCore, NeuronClient, GameProtocol, GameApp |
   | NeuronCoreTests | NeuronCore, and the unit-test framework's folder | NeuronCore |
   | NeuronServerTests, NeuronClientTests, GameProtocolTests | NeuronCore and its own library, and the unit-test framework's folder | the same |
   | OpponentTests | NeuronCore, GameProtocol, Opponent, and the unit-test framework's folder | the same |

   Neither `GameApp`, `Opponent` nor the executable lists `GameLogic` or `NeuronServer`, so a client or AI file that includes a server header does not compile: adding `#include "GameLogic.h"` to `GameApp` or to `Opponent` fails with C1083. A quoted include is also resolved relative to the including file, so `#include "../GameLogic/Server.h"` would slip past the include path. `Build/CheckProjectFiles.py` therefore rejects any include that climbs out of its own project. Together, these make the build, not review, hold the server boundary. The executable still links `GameLogic` and `NeuronServer`, because the in-process server has to be in the executable. It gets the server through the factory declared in `GameProtocol` and defined in `GameLogic`. A test project for `GameLogic` may list `GameLogic` as well, because it is a test and not a client. `GameLogicTests` also lists `Opponent`, so that the AI plays the real server headlessly ([ADR-020](ADR-020-ai-and-match-flow.md)), and so does `OutpostServer`, which hands the server its AI empires and deputies through `HostedPlayer`, declared in `GameProtocol` ([ADR-079](ADR-079-seat-controller-and-deputy.md)); the AI's own files still cannot include `GameLogic`, and `GameLogic` cannot include `Opponent`.

   `NeuronClient` and `NeuronServer` do not reference each other, and neither do `GameApp` and `GameLogic`. `Opponent` and `GameLogic` share only `GameProtocol`. The dedicated server, `OutpostServer`, is a second executable that links `GameLogic`, `NeuronServer` and `Opponent` and none of the client libraries, and the client side changes only the address its `QuicTransport` connects to ([ADR-060](ADR-060-quic-in-process.md), [ADR-078](ADR-078-dedicated-server.md)).

## Consequences

- There are two copies of state: server state and the client's snapshot view. Code that needs a value for display reads it from the snapshot. Anything that shows up only on the client is presentation, such as selection, the camera and effects.
- Every player action has a one-tick delay before it takes effect, plus the interpolation delay. This is normal for an RTS. What it costs is a budget ([`Docs/Budgets.md`](../Budgets.md)).
- Selection, camera, UI state and ghost structures are client state. They are not commands and the server never sees them.
- Testing gets easier: the simulation library can be driven by command scripts in a test project with no window and no GPU.

## What this forecloses

- **Lockstep multiplayer.** A switch to lockstep would need a deterministic simulation (fixed-point, R16's deterministic-core rules) and is a new ADR, not a flag.
- Client code that reaches into simulation objects "just for now". That is exactly the edge (AGENTS.md §2) that would put server state on the client.
- Client-authoritative shortcuts, such as the client deciding a hit or a build completion.
