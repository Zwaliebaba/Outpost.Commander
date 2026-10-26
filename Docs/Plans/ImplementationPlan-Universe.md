# Outpost Commander — The Universe, and How the Game Reaches It

Status: **open** · gates J1–J7 decided by the owner on 2026-10-25 · From the owner's two questions of 2026-10-25: the dedicated server talks about a world, but the game is set in space; and the game should try its server before it offers a local skirmish.

The dedicated server runs a world, a match that does not end, for a circle of friends (Game §universe). The word reads as a planet, and the game is set in space. This plan renames it to a universe, in the code, the server's switches and files, the menu and the documents. It also makes the game join its universe at launch when the player has a join file, so that the universe comes first and a local skirmish is what the player falls back to. A player who can join at launch also needs a way back out before the player's empire falls, which the banner alone gave.

This plan is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the ADRs the task names.**
2. **Take the lowest-numbered task whose status is `todo` and whose dependencies are `done`.** One PR per milestone (owner, 2026-09-30), in the milestone order below.
3. **ADRs are edited in place** (AGENTS.md §6), in the same commit as the change that implements them.
4. **A task that touches rendering, input or presentation is run, not only built**, and the owner's run accepts it.

Task numbers carry the prefix UN, and gates are lettered J.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| UN1.1 | A world becomes a universe, and the game's state the simulation | — | J1, J2, J3, J6, J7 | merged, [#126](https://github.com/Zwaliebaba/Outpost.Commander/pull/126); awaiting a universe made, run and joined |
| UN2.1 | Esc leaves a universe | UN1.1 | J5 | merged, [#127](https://github.com/Zwaliebaba/Outpost.Commander/pull/127); awaiting a run against a dedicated server and the owner's run |
| UN2.2 | The game joins its universe at launch | UN2.1 | J4 | merged, [#127](https://github.com/Zwaliebaba/Outpost.Commander/pull/127); awaiting the three runs with a join file and the owner's run |

### Milestone order

UN1, UN2.

- **UN1 changes no behavior.** It renames, and a universe plays exactly as a world did. Its one effect outside the tree is on disk: a universe's settings are `Universe.json` and its log `Universe.log`, its saves are `Universe-<tick>.save` and begin with another kind, and the server's switches are `--new-universe` and `--universe-run`. Nothing reads the old names, since nothing is grandfathered (AGENTS.md), and the game is not live, so a save of the old kind is refused, as a save of an old layout is (AGENTS.md R18). Join files name no world, and keep working. The owner's week (the Phase 5 plan's task 39.4) starts after UN1, or its folder is made again with `--new-universe`.
- **UN2.2 depends on UN2.1.** A game that joins at launch, with no way out of the universe, leaves the skirmish out of reach whenever the server answers.

---

## Gates

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| J1 | **The name.** The dedicated server runs a world (Game §universe). The game is set in space, and a world reads as a planet. **Decided on 2026-10-25:** it is a universe. | the owner, 2026-10-25 | UN1.1 | decided 2026-10-25 |
| J2 | **What the rename covers.** "World" means three things in the tree: the mode the dedicated server runs; the state the server simulates, in any match ("what one player may see of the world after a tick"); and the 3D space models are placed and drawn in (`WorldMatrix`, world space, the marks drawn in the world). **Decided on 2026-10-25:** the mode becomes the universe everywhere: types, functions, files, the server's switches and files, the menu, the ADRs' names and `Owns:` subjects, and the documents. The simulated state becomes the simulation. The 3D space keeps "world". The records keep their words, apart from links and citations that move. | this plan | UN1.1 | decided 2026-10-25 |
| J3 | **The design's keys.** A key is never renamed (AGENTS.md §8), and the design's universe section is keyed §world and eight keys under it. **Decided on 2026-10-25:** the nine keys are retired and §universe keys replace them, with every citation moved. | this plan | UN1.1 | decided 2026-10-25 |
| J4 | **Joining at launch.** The game knows a server only from a join file. When one is there, the game could join at once, on every return to the menu, or only when a skirmish is asked for. A join to a server that is not there takes up to 10 s, the QUIC handshake's timeout, and only Quit can be pressed meanwhile (ADR-078 decision 9). **Decided on 2026-10-25:** once, at launch, when the player's documents hold the join file or `--join` names one; a join that fails goes back to the menu with its notice, and no later return to the menu tries again. Only Quit can be pressed while it connects, and the automatic join gives the handshake 3 s. | the owner, 2026-10-25 | UN2.2 | decided 2026-10-25 |
| J5 | **The way out of a universe.** The way back to the menu from a universe is the banner's, which shows only once the player's empire has fallen (ADR-084 decision 7). **Decided on 2026-10-25:** Esc, last in its chain: with no window open and nothing armed, it opens a panel, Leave universe or Stay. A skirmish keeps its way out as it is. | this plan | UN2.1 | decided 2026-10-25 |
| J6 | **The name of the accessor.** `InProcessServer::World` and the tests' `World` return the server's `Simulation`. A member named `Simulation` would hide its own return type in the class, which GCC rejects. **Decided on 2026-10-25:** `SimulationState`, as `MatchArena::TuningData` names its `Tuning`. | this plan | UN1.1 | decided 2026-10-25 |
| J7 | **The client tests' fixtures.** `PlacementTests`, `PlayerControlsTests` and `OrderFormTests` call what they set up `World`, a list of entity views or a snapshot, not a `Simulation`. **Decided on 2026-10-25:** each is named for what it is. | this plan | UN1.1 | decided 2026-10-25 |

---

## Milestone UN1 — A world becomes a universe

### UN1.1 — A world becomes a universe, and the game's state the simulation

- **Gates:** J1, J2, J3, J6, J7.
- **Scope:**
  - Every type, function, member, constant, test and file of the mode: `UniverseSettings`, `UniverseFolder`, `UniverseLog`, `UniverseRun`, `UniverseState`, `UniverseSeat`, `UniverseIdentity`, `UNIVERSE_STATE_VERSION`, `ServerDesc::universe`, `Simulation::UseUniverseRules`, `ActionKind::JoinUniverse`, `CreateUniverseServer`, `Tools/UniverseLog.py`, and the tests that test them.
  - The server's switches and files: `--new-universe`, `--universe-run`, `Universe.json`, `Universe.log`, `Universe-<tick>.save`, a save's kind `OCUNIV`, and `OUTPOST_UNIVERSE_MEASURE`.
  - The menu's Join universe, and "Could not join the universe:".
  - The accessor `SimulationState`; the client tests' `AsteroidAndStation`, `FiveShips`, `Entities` and `ThreeSectors`; and a local named for the simulated state named `simulation`.
  - The design's §universe keys, ADR-077, ADR-082 and ADR-084 under their new names, the `Owns:` subjects, the Glossary, the pillars, the horizon, the budgets, AGENTS.md and the open plans. The Glossary's Simulation says what "world" still means.
- **Not in scope:** `CreateUniverseServer` also makes the battle matchups' server, as `CreateWorldServer` did, and its name says less than it does.
- **Acceptance:** no identifier, string or document outside the records names the mode a world; the save's layout hash is unchanged, since `ByteLayout` describes shapes and not names.
- **Verify:** the checkers; CI's build and tests; a universe made with `--new-universe`, run and joined.

---

## Milestone UN2 — The universe first, and a way back out

### UN2.1 — Esc leaves a universe

- **Gate:** J5.
- **Scope:**
  - In a universe, Esc with no window open and nothing armed opens the leave window, a floating window at first in the middle of the screen: "Leave for the menu?", that the deputy keeps the empire going, and "Leave universe" and "Stay". Esc again, its ×, or Stay closes it. The universe runs on behind it.
  - Leave universe closes the player's connection and shows the menu with no notice. The server sees the connection gone at once, and the deputy takes the seat 60 s of ticks later (ADR-079 decision 4).
  - The next skirmish's server is made as the menu comes back, as Back to menu does now.
  - A skirmish: Esc is unchanged.
- **ADR:** [ADR-031](../ADR/ADR-031-floating-windows.md) decisions 2, 5 and 6, Esc's chain and where the window opens; [ADR-078](../ADR/ADR-078-dedicated-server.md) decision 11, the leave window, beside the join and the lost connection that ADR-078 already holds; Game §interface.controls, Game §interface.windows.leave and Game §session.join.
- **Acceptance:** `HudTests.OffersToLeaveAUniverse`: the window in the middle, its words whole, Leave universe and then Stay; `PlayerControlsTests.SaysWhetherAnythingWaitsForAClick`: an attack-move, a standing order and a placement each keep Esc from the window until Esc cancels them; and the longest content's checks of fit, overlap and contrast with the window open. `GameClient`'s Esc needs D3D12 and Windows, so it has no test.
- **Verify:** CI; run against a dedicated server; owner run.

### UN2.2 — The game joins its universe at launch

- **Gate:** J4.
- **Scope:**
  - At launch, with no `--measure`, `--load`, `--stress`, `--matchup` or `--ai-matches`, the game joins the universe of `Outpost Commander\Join.json` in the player's documents when it is there, as the menu's Join universe does; `--join <file>` still names another.
  - It joins once. The menu says "Connecting to" and the server, with only Quit live, as a join from the menu does (ADR-078 decision 9).
  - The automatic join gives the handshake 3 s (`LAUNCH_JOIN_CONNECT_TIMEOUT`, as `QuicTransport`'s connect timeout), and the join from the menu or `--join` keeps `QuicChannel::CONNECT_TIMEOUT`, 10 s.
  - A join that fails shows the menu with "Could not join the universe:" and why. Nothing later tries again; the menu's Join universe does.
  - The skirmish's server is still made at launch, as it is now, for the menu to fall back to (ADR-049).
- **ADR:** [ADR-078](../ADR/ADR-078-dedicated-server.md) decision 9; Game §session.launch and Game §session.join.
- **Acceptance:** `QuicTransportTests.GivesUpAJoinInTheTimeItIsGiven`: a join given 1 s gives up on a server that has gone well inside the 10 s a join from the menu waits. Which switches skip the join at launch is the shell's, which has no test.
- **Verify:** CI; run with a join file against a server that answers, one that is stopped, and an address where no machine answers; owner run.
