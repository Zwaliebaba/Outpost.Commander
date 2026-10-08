# Outpost Commander — Phase 5 Implementation Plan

Status: **open** · Started 2026-10-08, when the owner accepted [the Phase 5 design](OutpostCommander-Phase5.md) with gates H1 to H9 decided as proposed · Derived from the Phase 5 design

The Phase 5 design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed. [The Phase 4 plan](Archive/ImplementationPlan-Phase4.md) is closed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the Phase 5 design, then the ADRs the task names.** Phase 5 amends Phase 4, which amends the designs before it.
2. **Take the lowest-numbered task whose status is `todo` and whose dependencies are `done`.** One PR per milestone (owner, 2026-09-30), in milestone order.
3. **ADRs are edited in place** (AGENTS.md §6), and new ones take the next free number, ADR-077 onward.
4. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. The server, the AI and `GameLogicTests` build and run there against a stand-in for the Windows headers, the test framework and MsQuic, as Phases 2 to 4 did; the QUIC tests do not. The dedicated server's executable, the client and every week-long run are CI's or the owner's. Tasks marked *Owner run* stay `in review` until the owner has run them.
5. **The AI keeps playing at every milestone,** and `AiPlayerTests` keep passing.
6. **The gates are the design's, H1 to H10** (design §14).

Task numbers continue the Phase 4 plan's, whose last was 34.2. The horizon plan's outline put the dedicated server first; the owner chose to start with the world that survives a restart (2026-10-08), so it is milestone 35 and the server 36.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 35.1 | A world's state saved and loaded | — | H8 | done, [#92](https://github.com/Zwaliebaba/Outpost.Commander/pull/92) |
| 35.2 | The world's folder: saves, the command log and recovery | 35.1 | H7 | done, [#92](https://github.com/Zwaliebaba/Outpost.Commander/pull/92) |
| 36.1 | `OutpostServer` and a world's settings | 35.2 | H1 | built, in milestone 36's PR; awaiting the owner's run |
| 36.2 | Seats with tokens, taken again, and a kept certificate | 36.1 | H1 | built, in milestone 36's PR; awaiting CI |
| 36.3 | The client joins a world | 36.2 | H1 | built, in milestone 36's PR; awaiting CI and the owner's run |
| 37.1 | The AI starts from any state | — | — | todo |
| 37.2 | Clients on the server's thread | 35.2 | — | todo |
| 37.3 | The seat's controller and the keeper | 37.1, 37.2 | H2, H3 | todo |
| 38.1 | The server's events, and alerts read from them | — | H4 | todo |
| 38.2 | Scheduled orders | 38.1 | H4, H5 | todo |
| 38.3 | The client's orders window | 38.2 | H5 | todo |
| 39.1 | A world without an end | 37.3 | H6 | todo |
| 39.2 | The world log and `--world-run` | 35.2, 37.3 | — | todo |
| 39.3 | W1–W6 | all above | — | todo |

### Milestone order

35, 36, 37, 38, 39. Milestone 35 is the foundation the rest stands on and the part the container verifies end to end. Milestone 36 puts it on a server a client can reach. Milestone 37's AI start from any state (37.1) stands on its own and can move earlier if a milestone stalls. Milestone 38's events come before its orders, which fire on them. Milestone 39 ends the world's dependence on a match's ending, and measures.

---

## Milestone 35 — A world that survives a restart

### 35.1 — A world's state saved and loaded

- **Gate:** H8, decided.
- **Scope:**
  - The wire format's writer and reader become `GameProtocol`'s `ByteWriter` and `ByteReader`, and its field lists and enumerations' last values `WireFields.h`, so that a save writes `Simulation`'s state as a message is written (ADR-060 decision 4). The wire's bytes do not change.
  - `Simulation::SaveState` and `Simulation::LoadState`: every member of `Simulation` named by one structured binding, so a member added and not considered does not compile; the state written from field lists; what follows from it worked out again on load (design §5).
  - A save's header: its kind, `WORLD_STATE_VERSION`, the seed, the tick rate and the hash of the tuning data's and the map's files. A checksum after the state.
  - A description of the saved state's layout, and its hash, which a test pins for each state version (AGENTS.md R18).
- **ADR:** a new one, ADR-077: a world's state on disk. ADR-060 decision 4 edited in place for the shared field lists.
- **Acceptance:** `WorldStateTests`: a world saved and loaded compares equal and builds the same snapshots; a world loaded and run on compares equal with one that never stopped, over AI-against-AI play on the 10 km map; a save of another version, other data, or cut short or altered is refused; the layout's hash is the one recorded for the version. `WireFormatTests` unchanged.
- **Verify:** the container's run of `GameLogicTests`; CI.
- **As built:** recorded in [ADR-077](../Design/ADR/ADR-077-world-state-on-disk.md); AGENTS.md gains R18.
  - Worlds saved at minutes 6, 12 and 18 of two AIs on the 10 km map, loaded and given the commands the running world applied, equal it at minute 24, to the bit: the path graphs built again are the graphs the world had kept up to date. Every test passed on its first run, so each was checked against a broken load: with the PRNG's state not restored, or the graphs built over the map's obstacles alone, they fail.
  - The wire's bytes are unchanged: `WireFormatTests` and `InProcessServerTests` pass as they were.
  - The container has no `ExcludeHeaderFilterRegex` in its clang-tidy 18, so `RunClangTidy.py` did not run there; clang-tidy ran with the repository's checks over the changed files, against the stand-in, and its findings are fixed. CI's run is the first under MSVC.

### 35.2 — The world's folder: saves, the command log and recovery

- **Gate:** H7, decided.
- **Scope:**
  - `ServerDesc::world`, a folder. A server made with one saves its state every 60 s of ticks, encoded on the server's thread and written by a thread of its own to a new file renamed into place, and keeps the newest three.
  - The command log written to the folder as each tick applies it, a file for each save, handed to the system before the next tick.
  - A server made with a folder that holds a save loads the newest that reads whole, and applies the log since it, before its first tick.
- **ADR:** ADR-077; ADR-009 and ADR-025 edited in place.
- **Acceptance:** `WorldFolderTests`: a world stepped, dropped and opened again compares equal with one that never stopped; a cut-off log's last record is ignored; a save cut short falls back to the one before; the newest three are kept; a folder for other data is refused.
- **Verify:** the container's run of `GameLogicTests`, including the save's time and size on a late 10 km world (W2); CI.
- **As built:** recorded in ADR-077; ADR-009 and ADR-025 edited in place, and ADR-060 for the field lists' new home.
  - A world opened again comes back at the tick after the last command it logged, and saves there at once, so its log begins anew. A save of a tick the folder holds already is not written again.
  - A cut log is passed over only at the end of the newest log; an older log cut short is refused, which matters only when the newest save is broken too.
  - W2 in the container: a save at minute 60 holds 274–293 entities in 75–78 KB and encodes in 0.36–0.38 ms (`MeasuresALateSave`, with `OUTPOST_WORLD_MEASURE`). On the development machine in Release, with the deputies in, it is the owner's run at milestone 39.
  - The folder's and the server's tests ran under ThreadSanitizer (g++ 13) with no report.
  - The shell does not make a world yet: `ServerDesc::world` is empty for every match it starts. `OutpostServer` is milestone 36.

## Milestone 36 — The dedicated server

### 36.1 — `OutpostServer` and a world's settings

- **Gate:** H1, decided.
- **Scope:** a console executable, `OutpostServer`, in the solution and ADR-002's table. `--new-world <folder>` writes a world's settings, `World.json`: the address and port it listens on, the host its players reach it at, the seed and a seat with a token for each of the map's starts. `OutpostServer <folder>` runs the world until Ctrl+C or a close of its console, and leaves it saved.
- **ADR:** a new one, ADR-078: the dedicated server and its seats. ADR-002 edited in place.
- **Acceptance:** `WorldSettingsTests`: settings and join files read back as written, and are refused when they are not.
- **Verify:** the container's run of `GameLogicTests`, and of `OutpostServer --new-world`; CI's build; **owner run**: a world made and run on the development machine, and stopped with Ctrl+C.

### 36.2 — Seats with tokens, taken again, and a kept certificate

- **Gate:** H1, decided.
- **Scope:**
  - A seat's token in the hello and the address, with `PROTOCOL_VERSION` 11.
  - A world's seats taken at any time, the newest connection holding a seat, and a replaced connection closed with `SeatTaken`.
  - A hello of another version refused as `WrongVersion` before it is decoded.
  - The listener's address and port from the world's settings.
  - A world's certificate kept in its folder.
  - Join files written by the server.
- **ADR:** ADR-078; ADR-060 edited in place.
- **Acceptance:** `QuicTransportTests`: another token, an unopened seat and another certificate refused, saying why; a world's seat taken after it starts and taken again; a world's certificate the same when it runs again; the listen address; a hello of another version.
- **Verify:** CI, since QUIC does not run in the container.

### 36.3 — The client joins a world

- **Gate:** H1, decided.
- **Scope:** `--join <file>`; the menu's "Join world" when the player's documents hold `Outpost Commander\Join.json`; a lost connection back to the menu with the reason in the warning's color.
- **ADR:** ADR-078.
- **Acceptance:** `HudTests.OffersAWorldAndSaysWhyTheLastGameEnded`.
- **Verify:** CI; **owner run**: the game on a second machine joins the owner's world with its join file, plays, loses its connection when the server stops, and joins again when it runs.

### As built

Recorded in [ADR-078](../Design/ADR/ADR-078-dedicated-server.md).

- **Run in the container.** `GameLogicTests` and `OutpostServer` built there against stand-ins for the Windows headers and MsQuic's posix header. `GameLogicTests` passes apart from `QuicTransportTests`. `--new-world` and its refusals were run. Running a world needs QUIC, and MsQuic's Linux build opens IPv6 sockets, which the container's kernel lacks. So every QUIC test, and a world run end to end, are CI's and the owner's.
- **The CNG side is untested here.** The kept certificate, and naming its key on a certificate read back from a file, were written without Windows. CI's `AWorldKeepsItsCertificate` is their first run.
- **The client's join and the menu's notice** are `GameApp` and shell code, which CI builds first and the owner sees first.

## Milestones 37 to 39

Each is scoped in detail when it becomes the next milestone, from the design section its tasks name.

- **37 — Deputies** (design §6).
- **38 — Orders that run while away** (design §7).
- **39 — A world without an end, the world log and the week** (design §8–§10).
