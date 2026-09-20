# ADR-018 — Six libraries on two axes: layer and side

**Status:** Accepted; supersedes [`ADR-001`](ADR-001-solution-layout.md) on the project table, its edges and its test-project mapping, and nothing else in it
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

`ADR-001` gave the tree six libraries — `Core`, `Content`, `Sim`, `Net`, `Replica`, `Client` — split by *subject*: the simulation here, the protocol there, the renderer over there. That split is orthogonal to the one the game actually has, which is **which machine the code runs on**, and the cost of the mismatch is `Net`: one library holding the wire format, the host endpoint *and* the client endpoint, so that any executable wanting one gets all three.

The mismatch reaches further than tidiness. [`ADR-012`](ADR-012-replication-protocol.md) calls the interest set **"a security boundary"** — the client is sent only what its commander can see, and that is what stops a modified client reading the fog. But `OutpostCommander` links `Sim`, because `LocalHost.cpp` runs the host in the same process for single player. **The boundary is a convention inside one address space**, which for a real-time strategy game is the oldest cheat there is.

[`ADR-013`](ADR-013-uwp-application-model.md) then makes the client a packaged UWP application and the host a Win32 console executable, and those two want different APIs for the same job: `Windows.Networking.Sockets` on one side, Winsock on the other. A `Transport` interface with two implementations is the obvious answer, and there is no library either implementation belongs in.

## Decision

**Six libraries on two axes: which layer the code is (engine or game) and which side it runs on (both, client, or server).** The prefix says the layer; the suffix says the side. The namespaces are unchanged — `Neuron` for the engine, `Outpost` for the game (`AGENTS.md` R9) — so the project name and the namespace still agree.

| | shared | client only | server only |
|---|---|---|---|
| **engine**, `Neuron` | `NeuronCore` | `NeuronClient` | `NeuronServer` |
| **game**, `Outpost` | `GameShared` | `GameClient` | `GameLogic` |

- **`NeuronCore`** — arithmetic, fixed point, binary angles, hashing, randomness, the slot map, the byte stream, JSON, bitmaps, textures, waves, logging, paths, the `Transport` interface and `LoopbackTransport`; and the aggregates the renderer consumes without knowing the game: `RenderView`, `HeightView`, `InterfaceDesc`, `ModelDesc`.
- **`NeuronClient`** — Direct3D 12, the seven passes, the scene target, the swap chain, the camera, input, the interface primitives, fonts, the scale rule. The Windows Runtime datagram transport joins it in M3.
- **`NeuronServer`** — `TickPacer`, which is where wall time meets the tick (`AGENTS.md` R16) and therefore a thing only a host does. The Winsock transport joins it in M3.
- **`GameShared`** — the content tables and their loaders; the wire format (`Records`, `Messages`, `Fragmenter`, `Reassembler`, `ReliableStream`); and **the vocabulary both sides agree on**: object ids, orders, designs, devices, structures, deposits, plans, features, seats, the fog grid, the landscape and its generator, and `VictoryState`.
- **`GameClient`** — the replica, interpolation, selection, picking, order input, the placement preview, the render-view builder, and the client endpoint.
- **`GameLogic`** — the simulation entire, the host endpoint, the interest set, and the client history the encoder keeps.

**The edges, which are the whole of what a project may reference, put on its include path, or include with a quoted include:**

```
NeuronCore   ← nothing
NeuronClient ← NeuronCore
NeuronServer ← NeuronCore
GameShared   ← NeuronCore
GameClient   ← NeuronCore, GameShared
GameLogic    ← NeuronCore, GameShared
```

**`GameClient` is not built on `NeuronClient`, and that is deliberate.** `ADR-001` put the render-view and height-view aggregates in `Core` so that the replica could produce them and the renderer consume them with no edge between the two; that property survives here and is worth keeping. `GameClient` gains the edge only when the presentation layer moves down out of the executable, and that task says so.

**The executables:**

| Executable | Kind | Built on |
|---|---|---|
| `OutpostCommander` | packaged UWP | `NeuronCore`, `NeuronClient`, `GameShared`, `GameClient` — **and, until `p1-uwp-shell/P2` splits it, `NeuronServer` and `GameLogic` too, because the game and the host still share one `App.cpp`** |
| `OutpostHost` | Win32 console | `NeuronCore`, `NeuronServer`, `GameShared`, `GameLogic` |
| `OutpostCapture` | Win32 console, arriving with `P3` | everything: it is a harness, not the game |

**One suite per library, and one more.** `Tests/<Library>Tests` for each of the six, derived by name as before. **`Tests/IntegrationTests` is the exception and is written into `BUILT_ON` by hand**, because the split leaves no project above both `GameClient` and `GameLogic`, and the three tests that prove the two halves agree — convergence, the host endpoint, the interest set — have nowhere else to live. It is a harness, like `OutpostCapture`; nothing ships from either.

## Consequences

**Five boundary defects the split found, each of which was real and none of which was visible before.** The checker found them by refusing the edges, which is the argument for expressing a boundary as a project rather than as a review rule:

1. **`NeuronClient` was built on the game.** `GeometryPass.h`, `ModelBuffers.h` and `UiDraw.h` included `InterfaceDesc.h` and `ModelDesc.h`, so the renderer depended on `Content` — an R9 break that `ADR-001`'s table permitted by listing `Client` as built on `Content`. Both are aggregates the renderer consumes and neither names a game concept, so they join `RenderView` and `HeightView` in `NeuronCore`, and `NeuronClient` is built on `NeuronCore` alone.
2. **`Victory.h` was not shared.** Its two functions, `Annihilated` and `CheckVictory`, both take a `Sim&`. What the wire carries is the *state*, so `VictoryState` becomes `GameShared/VictoryState.h` and the evaluation stays in `GameLogic`.
3. **`ClientHistory.h` was on the wrong side of its name.** It is the *server's* record of what a client has acknowledged, read only by `FrameEncoder`. It is `GameLogic`'s.
4. **`LandscapeGenerator` is shared, not server-only.** The client receives a landscape *definition* on the wire and generates the terrain in order to render it, so generation is something both sides do from the same seed — which is also what makes it worth being deterministic.
5. **`Movement` is not vocabulary.** It reaches `Production`, `Sim` and `Steering`; only one stray include in `App.cpp` made it look shared.

**What this does not fix yet, and the measurement that says so.** `GameShared` holds **19 headers that came out of `Sim`**, among them `World.h` and `StateHash.h`, and the client has no business with either. They are there because `Placement.h` takes a `World&` and `Landscape.h` includes `StateHash.h` — two includes that drag six more headers behind them. The split is therefore *correct at the project level and loose at the header level today*, and three tasks tighten it: `Placement` takes what it needs rather than the whole world, the landscape's hashing moves to the side that hashes, and `App.cpp` loses its `Movement.h`. Until those land, a `GameClient` translation unit can name `World`. **That is stated here rather than discovered later.**

**What it costs.** Fifteen projects where there were fourteen, a seventh test suite, and a `NeuronServer` that holds one header until M3 brings the Winsock transport — a slot reserved with a genuine occupant rather than a placeholder. Every document written before 2026-09-20 names the old six projects in prose; every *path* in the tree was rewritten by lookup against the new layout, so a dead path is a defect, but a sentence saying "Sim" means `GameLogic` and a sentence saying "Net" means whichever of the three the context is about.

**What would reopen it.** A seventh library, if the replica and the presentation layer turn out to want separating once both are in `GameClient` — the `ADR-001` property they inherit is a convention there rather than a structure, and if it erodes, the answer is a project and not a rule.

## Measurements

**Counted on this tree at `ec702e2`, by script, not estimated.**

- **The closure that fixes the boundary.** Starting from every file that will live in `GameClient` or in the shared wire format and following quoted includes transitively, the client side reaches **21 of the 43 `Sim` headers**. Following the same closure from `Net/Records.h` alone reaches **8**. The difference is the five defects above plus the two include chains named under **Consequences**: `World.h ← Placement.h ← RenderViewBuilder.cpp` and `StateHash.h ← Landscape.h ← RenderViewBuilder.h`, which between them account for `World`, `StateHash`, `Seat`, `GhostStore`, `Projectile`, `Wreck` and `Feature`.
- **Where the files went**: `NeuronCore` 39, `NeuronClient` 87, `NeuronServer` 3, `GameShared` 61, `GameClient` 24, `GameLogic` 51, by `find | wc -l` after the move.
- **`Sim`, `Content`, `Net` and `Replica` named no platform header**, checked in CI since M0, which is why none of them needed one line changed to move.

**Not compiled.** There is no MSVC in reach; what is verified is `Build/CheckProjectFiles.py` clean over all fifteen projects — which checks every edge, every reference, every include directory and every quoted include against the table above — its self-test green on all 27 rules against fixtures renamed to the new table, `Build/CheckFormat.py` clean over 395 files, and all five task plans validating. **A link error is still possible and a compile error is not ruled out.** `P2` is the first task that builds this.
