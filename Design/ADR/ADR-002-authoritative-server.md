# ADR-002 — An authoritative server, in-process for the MVP

Status: **accepted** · 2026-09-29 · Layer shape amended 2026-09-30

## Context

The owner's direction: the game will be **server-authoritative** in the future, and for the MVP the server runs inside the client process. The MVP design is in `GameDesign/OutpostCommander-MVP.md`, where §9 describes what this means for players.

The two usual RTS network models make different demands on the code:

- **Lockstep** sends only commands. Every peer simulates, so the simulation must be bit-for-bit deterministic across machines: fixed-point or strictly controlled floats, ordered iteration, and a seeded PRNG (AGENTS.md R16). One divergence desynchronises the match.
- **Authoritative server** simulates in one place and sends state to the clients. The simulation does not need to be deterministic across machines, because only one machine runs it. The costs are bandwidth for state, and latency between an order and its visible effect.

The owner chose the second. The decision to record is how the code is shaped so that moving the server to another process later is a transport change and not a rewrite.

## Decision

1. **The simulation is a separate layer with no knowledge of the client.** It owns all game state and rules. It includes no renderer, WinRT, XAML or window headers, only the Windows SDK and the standard library at most (R9, R14 as written). It builds as a static library that the client executable links today and a server executable can link later.
2. **State changes only through commands.** A client sends a `Command` — move, attack, build, queue, research, save design and so on — tagged with its player. The server validates it against ownership, cost and legality, and applies it at the start of a tick, or rejects it. Nothing outside the server mutates simulation state. This holds for the human player, the AI and any debug tool.
3. **The server ticks at a fixed rate** (20 Hz at first, a tuning value). A tick is the simulation's clock. Wall-clock time enters only where the host decides how many ticks to run.
4. **Clients receive snapshots, addressed per player.** After each tick the server produces a snapshot for each player. In the MVP every player sees everything, but the snapshot is still built per player. That is the seam where fog of war goes later, on the server only.
5. **The client renders from snapshots, never from server state.** It keeps the last two snapshots and interpolates between them. The client is always about one tick behind the server, which is 50 ms at 20 Hz. Client-side prediction is not needed for the MVP.
6. **Commands and snapshots cross a `Transport`.** For the MVP it is a `LoopbackTransport`: in-process queues, with no serialisation. The command and snapshot types are nevertheless plain data — no pointers into server state, entities referred to by ID — so that a network transport can serialise them later without changing their meaning.
7. **The AI is a client.** It gets its player's snapshot and sends commands through the same transport. It has no access to server internals, and its library's include path is what enforces that (see Layer shape).
8. **Floats are allowed in the simulation.** No cross-machine determinism is required. Two things are still required, because they are cheap now and expensive later: the fixed tick (3), and **all simulation randomness from one seeded PRNG** owned by the server, never `std::random_device` or anything address-dependent. That keeps a match reproducible from a seed and a command log on the same build, which is what debugging and bug reports need.

## Consequences

- There are two copies of state: server state and the client's snapshot view. Code that needs a value for display reads it from the snapshot. Anything that shows up only on the client is presentation, such as selection, the camera and effects.
- Every player action has a one-tick delay before it takes effect, plus the interpolation delay. At 20 Hz that is 50–100 ms in-process. This is normal for an RTS. Before a real network is added, measure it and decide on command acknowledgement or local prediction then.
- Selection, camera, UI state and ghost structures are client state. They are not commands and the server never sees them.
- Snapshots of 200 ships and 40 structures at 20 Hz are small in-process. Delta compression is a problem for the network transport, not the MVP.
- Testing gets easier: the simulation library can be driven by command scripts in a test project with no window and no GPU.

## Layer shape (proposal, fixed when the projects are created)

```
OutpostCommander (exe, WinUI 3)  ── client: UI, input, camera, presentation
 ├── Engine     (static lib)      ── D3D12 renderer, mesh loading, math. Knows no game concepts (R9)
 ├── Protocol   (static lib)      ── commands, snapshots, entity IDs, Transport, the in-process server's factory
 ├── Opponent   (static lib)      ── the AI player. Builds on Protocol only
 └── Simulation (static lib)      ── the server: state, rules, LoopbackTransport. Builds on Protocol
SimulationTests (test DLL)        ── drives Simulation through Protocol. The Q2 battles run here from milestone 3
NeuronCore (static lib)           ── below all of the above: diagnostics, files, Win32 handles. Knows no game concepts (R9)
```

**The boundary is enforced by include paths, not by review.** A project's include path lists only the projects it may include (AGENTS.md §3). The client lists `Engine` and `Protocol`, and `Opponent` lists `Protocol`. Neither lists `Simulation`, so a client or AI file that includes a server header does not compile. A quoted include is also resolved relative to the including file, so `#include "../Simulation/Server.h"` would slip past the include path. `Build/CheckProjectFiles.py` therefore rejects any include that climbs out of its own project. Between them, that is how the design's Q5 is answered by the build rather than by review. The client still links `Simulation`, because the in-process server has to be in the executable. It gets the server through a factory declared in `Protocol` and defined in `Simulation`. `SimulationTests` may list `Simulation` as well, because it is a test and not a client.

`Engine` and `Simulation` do not reference each other, and `Opponent` and `Simulation` share only `Protocol`. Every project may build on `NeuronCore`, which builds on nothing but the Windows SDK and the standard library. It is the one project that exists today besides the executable, and `framework.h`, the one header that owns the Windows macros, lives in it. The client links all four libraries and translates snapshots into draw calls. The final layout is recorded in AGENTS.md §2 when the first of these projects is created.

## What this forecloses

- **Lockstep multiplayer.** A later switch to lockstep would need a deterministic simulation (fixed-point, R16's deterministic-core rules) and is a new ADR, not a flag.
- Client code that reaches into simulation objects "just for now". That is exactly the edge (AGENTS.md §2) that makes a dedicated server impossible later.
- Client-authoritative shortcuts, such as the client deciding a hit or a build completion.
