# ADR-002 — An authoritative server, in-process for the MVP

Status: **accepted** · 2026-09-29

## Context

The owner's direction: the game will be **server-authoritative** in the future, and for the MVP the server runs inside the client process. The MVP design is in `GameDesign/Archive/OutpostCommander-MVP.md`, where §9 describes what this means for players.

The two usual RTS network models make different demands on the code:

- **Lockstep** sends only commands. Every peer simulates, so the simulation must be bit-for-bit deterministic across machines: fixed-point or strictly controlled floats, ordered iteration, and a seeded PRNG (AGENTS.md R16). One divergence desynchronises the match.
- **Authoritative server** simulates in one place and sends state to the clients. The simulation does not need to be deterministic across machines, because only one machine runs it. The costs are bandwidth for state, and latency between an order and its visible effect.

The owner chose the second. The decision to record is how the code is shaped so that moving the server to another process later is a transport change and not a rewrite.

## Decision

1. **The simulation is a separate layer with no knowledge of the client.** It owns all game state and rules. It includes no renderer, XAML, WinRT API or window headers, only the Windows SDK and the standard library at most (R9, R14 as written). It builds as a static library that the client executable links today and a server executable can link later.
2. **State changes only through commands.** A client sends a `Command` — move, attack, build, queue, research, save design and so on — tagged with its player. The server validates it against ownership, cost and legality, and applies it at the start of a tick, or rejects it. Nothing outside the server mutates simulation state. This holds for the human player, the AI and any debug tool.
3. **The server ticks at a fixed rate** (20 Hz at first, a tuning value). A tick is the simulation's clock. Wall-clock time enters only where the host decides how many ticks to run.
4. **Clients receive snapshots, addressed per player.** After each tick the server produces a snapshot for each player, holding what that player may see. Fog of war is applied there, on the server only ([ADR-024](ADR-024-fog-of-war.md)).
5. **The client renders from snapshots, never from server state.** It interpolates between the two snapshots around the moment it shows ([ADR-013](ADR-013-client-view-and-controls.md)). The client is always about one tick behind the server, which is 50 ms at 20 Hz. Client-side prediction is not needed for the MVP.
6. **Commands and snapshots cross a `Transport`.** The command and snapshot types are plain data — no pointers into server state, entities referred to by ID — so that a transport can serialise them without changing their meaning. The shell's matches cross a `QuicTransport`: QUIC through MsQuic (ADR-004), to the in-process server on the loopback address ([ADR-060](ADR-060-quic-in-process.md)). The headless runs and the tests that step the server use a `LoopbackTransport`, in-process queues with no serialisation.
7. **The AI is a client.** It gets its player's snapshot and sends commands through the same transport. It has no access to server internals, and its library's include path is what enforces that (see Layer shape).
8. **Floats are allowed in the simulation.** No cross-machine determinism is required. Two things are still required, because they are cheap now and expensive later: the fixed tick (3), and **all simulation randomness from one seeded PRNG** owned by the server, never `std::random_device` or anything address-dependent. That keeps a match reproducible from a seed and a command log on the same build, which is what debugging and bug reports need.

## Consequences

- There are two copies of state: server state and the client's snapshot view. Code that needs a value for display reads it from the snapshot. Anything that shows up only on the client is presentation, such as selection, the camera and effects.
- Every player action has a one-tick delay before it takes effect, plus the interpolation delay. At 20 Hz that is 50–100 ms in-process. This is normal for an RTS. Before a real network is added, measure it and decide on command acknowledgement or local prediction then.
- Selection, camera, UI state and ghost structures are client state. They are not commands and the server never sees them.
- Snapshots of 200 ships and 40 structures at 20 Hz are small in-process. Delta compression is a problem for the network transport, not the MVP.
- Testing gets easier: the simulation library can be driven by command scripts in a test project with no window and no GPU.

## Layer shape

The engine is split three ways, into what both sides share, what only the client needs and what only the server needs. The game is split the same way, with one shared library in the middle.

```
OutpostCommander (exe, Win32)    ── the shell: WinMain, the window, MSIX packaging (ADR-001). Wires the pieces together
 ├── GameApp      (static lib)    ── client game: presentation, selection, camera, UI state. → NeuronClient, GameProtocol
 ├── Opponent     (static lib)    ── the AI player, and a seat's deputy (ADR-079). → GameProtocol only
 ├── GameLogic    (static lib)    ── the server: state, rules, LoopbackTransport, its QUIC seats. → NeuronServer, GameProtocol
 ├── GameProtocol (static lib)    ── commands, snapshots, entity IDs, their wire format, QuicTransport, the in-process server's factory. → NeuronCore
 ├── NeuronClient (static lib)    ── client engine: D3D12, input, audio, PIX markers (ADR-005). → NeuronCore
 ├── NeuronServer (static lib)    ── server engine: the tick host and the pinned PRNG (ADR-009). → NeuronCore
 └── NeuronCore   (static lib)    ── engine code shared by client and server, including QUIC (ADR-004)
OutpostServer (exe, console)     ── the dedicated server: runs a world from its folder ([ADR-078](ADR-078-dedicated-server.md)), its AI empires and deputies ([ADR-079](ADR-079-seat-controller-and-deputy.md)). → GameLogic, NeuronServer, Opponent
GameLogicTests (test DLL)         ── drives GameLogic through GameProtocol. The Q2 battles run here from milestone 3, and
                                     the AI plays the real server here from milestone 6 (ADR-020)
GameAppTests (test DLL)           ── drives GameApp's camera math, model data and interface without a GPU
NeuronCoreTests, NeuronServerTests, NeuronClientTests, GameProtocolTests, OpponentTests (test DLLs)
                                  ── each drives its own library, with only what that library builds on (ADR-075)
```

Each library has a master header named after it, and its `pch.h` includes that header. `NeuronClient.h`, `NeuronServer.h` and `GameProtocol.h` include `NeuronCore.h`. `Opponent.h` includes `GameProtocol.h`. `GameLogic.h` includes `NeuronServer.h` and `GameProtocol.h`, and `GameApp.h` includes `NeuronClient.h` and `GameProtocol.h`.

**The `Neuron*` libraries know no game concept (R9).** A command or a snapshot is a game concept, so the types that cross the boundary live in `GameProtocol` and not in `NeuronCore`. They cannot live in `GameLogic` either, because the client would then have to include server headers, and they cannot live in `GameApp`, because the server cannot see it.

**Three projects reference packages, each for a reason recorded in an ADR.** The executable restores the MSIX packaging tools (ADR-001). `NeuronCore` references MsQuic, because the client and the dedicated server both need QUIC (ADR-004). `NeuronClient` references the PIX event runtime, which the server never links (ADR-005). No other library references a package, and no project includes XAML or a WinRT API. So the server can still be linked into a Windows Server container later: MsQuic's Schannel build needs Windows Server 2022 or later. A package's include directory is on its own project's include path only, so `msquic.h` and `pix3.h` never appear in a master header. `NeuronCore.h` in particular includes no XAML, WinRT API or package header. It does include C++/WinRT's `<winrt/base.h>` from the Windows SDK, so every layer shares one COM pointer and one `HRESULT` check (AGENTS.md R12, ADR-001). It is also the one header that owns the Windows macro family (AGENTS.md §4).

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

Neither `GameApp`, `Opponent` nor the executable lists `GameLogic` or `NeuronServer`, so a client or AI file that includes a server header does not compile. This was checked when the projects were created. Adding `#include "GameLogic.h"` to `GameApp` and to `Opponent` fails with C1083. A quoted include is also resolved relative to the including file, so `#include "../GameLogic/Server.h"` would slip past the include path. `Build/CheckProjectFiles.py` therefore rejects any include that climbs out of its own project. Together, these make the build, not review, answer the design's Q5. The executable still links `GameLogic` and `NeuronServer`, because the in-process server has to be in the executable. It gets the server through the factory declared in `GameProtocol` and defined in `GameLogic`. A test project for `GameLogic` may list `GameLogic` as well, because it is a test and not a client. `GameLogicTests` also lists `Opponent`, so that the AI plays the real server headlessly (ADR-020), and so does `OutpostServer`, which hands the server its AI empires and deputies through `HostedPlayer`, declared in `GameProtocol` ([ADR-079](ADR-079-seat-controller-and-deputy.md)); the AI's own files still cannot include `GameLogic`, and `GameLogic` cannot include `Opponent`.

`NeuronClient` and `NeuronServer` do not reference each other, and neither do `GameApp` and `GameLogic`. `Opponent` and `GameLogic` share only `GameProtocol`. The dedicated server, `OutpostServer`, is a second executable that links `GameLogic`, `NeuronServer` and `Opponent` and none of the client libraries, and the client side changes only the address its `QuicTransport` connects to (ADR-060, [ADR-078](ADR-078-dedicated-server.md)).

## What this forecloses

- **Lockstep multiplayer.** A later switch to lockstep would need a deterministic simulation (fixed-point, R16's deterministic-core rules) and is a new ADR, not a flag.
- Client code that reaches into simulation objects "just for now". That is exactly the edge (AGENTS.md §2) that makes a dedicated server impossible later.
- Client-authoritative shortcuts, such as the client deciding a hit or a build completion.
