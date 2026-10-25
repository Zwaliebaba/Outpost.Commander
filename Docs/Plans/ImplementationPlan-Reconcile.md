# Outpost Commander — Reconciliation Plan: Where the Code and the Documents Disagree

Status: **open** · From the documentation reconciliation of 2026-10-25, which checked every ADR and every design against the code and data at the head of `main`

The reconciliation brought the documents to the code wherever a commit, an ADR or a plan said why the two differed. These are the questions it left: places where the code and the documents disagree, or a document left a question to the owner, and only the owner can say which is meant. Meanwhile [the living design](../OutpostCommander.md) and the ADRs say what the code does, and each gate says where.

This plan puts those questions in order, as gates. It is a work queue, not an authority: where it disagrees with AGENTS.md, an ADR or the living design, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the gate's section below, then the ADR decisions and the living design's sections it names.**
2. **A gate is an owner decision.** Nothing a gate names changes until the owner has answered, and the answer is written into the gates table, with its date, first.
3. **An answer that keeps what the code does changes no code:** the documents already say it, and only what still calls the question open is edited. An answer that changes the code is a task in the plan that takes it up, with its ADR edited in place (AGENTS.md §6).
4. **Line numbers, and what the documents said, are at `main`'s 8366820,** where the reconciliation began. The ADRs and the living design have since been brought to the code, so read the old words with `git show 8366820:<path>`, and find a line of code by its words where the file has moved on.
5. **The plan closes once every gate is decided.** What it found goes to [the post-mortems](../Records/PostMortems.md), and the plan is deleted.

Gates are lettered R.

---

## Gates

Each is an owner decision, proposed in its section below.

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| R1 | **A joined world's tick rate.** Whether a world joined on a dedicated server takes its tick rate from the server, or checks it against the client's, rather than the client using its own build's tuning data. Proposed: keep what the code does while the game and `OutpostServer` package the same `Tuning.json`. | this plan | nothing built; a world's server with another tick rate | open |
| R2 | **Acknowledgement or prediction over a real network.** Whether play in a world joined from another machine needs command acknowledgement or client-side prediction. Proposed: neither for now; measure an order's response over that network in the owner's week, and decide from the figure. | this plan | nothing built; an acknowledgement in the protocol, or a prediction layer in the client | open |
| R3 | **A slow shooter's reveal.** Whether a shooter that keeps firing stays revealed between the shots of the slowest weapon, which fires every 6 s against a 3 s reveal. Proposed: keep the data; raising `shotRevealSeconds` is the data change if the owner wants every shooter that keeps firing revealed. | this plan | nothing built; the Rail Cannon's worth under fog | open |
| R4 | **Explored ground across a join.** Whether the ground a player has explored is part of what a world remembers for it, or the client's presentation that starts afresh on every join. Proposed: presentation, as built. | this plan | nothing built; what a returning player sees of explored ground, and where it may order a Mining Rig | open |
| R5 | **The cut-off share on the panel.** Whether a sector's panel reads the cut-off share from the data, or the share is fixed at half by decision. Proposed: keep the code in this run; with the next change to the territory panel, the snapshot carries the share and the panel writes it. | this plan | any change to `cutOffIncomePercent` | open |
| R6 | **A player's own color in seat 2.** Whether a player's own side is drawn in the Human set's blue whatever its seat, or the HUD colors sides by seat as the world does. Proposed: draw the sets by viewer, the viewer's own side always in the Human set's blue. | this plan | ADR-011 decision 3's wording; a world played from seat 2 | open |
| R7 | **Another player's structure, selected.** Whether a click selects another player's structure, seen or remembered, so that a remembered structure's panel opens, or that panel goes. Proposed: make the panel reachable, as gate V3 of [the second interface plan](ImplementationPlan-Interface2.md) decided. | this plan | ADR-081 decision 4's panel; ADR-013 decision 5's selection rule; Game §interface.hud.selection | open |
| R8 | **The balance shares after split orders.** Whether the shift in the balance check's shares from planning a large order over two ticks matters. Proposed: accept it. | this plan | — | open |
| R9 | **Criterion (d) at tier 1 after the arcs.** Whether the later balance check of record settles criterion (d) at tier 1, so that nothing waits on a retune since ships turn in arcs. Proposed: settled; nothing is retuned. | this plan | — | open |
| R10 | **The thin margins against a raid.** Whether the Defence Platform's and the Command Station's thin margins against a raid hold in play. Proposed: judge it in the owner's next playtest against the AI's raids; until then the numbers stand. | this plan | — | open |

---

## R1 — A joined world's tick rate

**The question.** Should a world joined on a dedicated server take its tick rate from the server, or check it against the client's, instead of the client using its own build's tuning data?

**What the code does.** The shell reads `rules.tickHz` once, from its own first skirmish server, and gives it to `GameClient`, the AI and the match log; a world it joins runs on the same rate. Nothing on the wire carries the server's rate: the welcome holds only the player, and the hello checks `PROTOCOL_VERSION` and the wire layout's hash, not the server's data. A server whose `Tuning.json` has another rate goes unnoticed, and its world is interpolated, timed and shown at the client's rate.

**What the documents said.** ADR-013 decision 2: "`Server::TicksPerSecond()` tells the client the tick rate. Over a network it would come once, at connection." No later document or commit says why that was not built.

**The proposal.** Keep what the code does while the game and `OutpostServer` package the same `Tuning.json`; ADR-013 decision 2 now says so. If a world's server may ever run another rate or other data, the welcome carries the rate, or the hello the data's hash. That is a new decision, and a wire change under `PROTOCOL_VERSION` 1.

**What it affects.** ADR-013 decision 2; ADR-060 decision 5; Game §session.join.

**Evidence.**

- `OutpostCommander/WinMain.cpp:301`: the rate, read from the first server.
- `OutpostCommander/WinMain.cpp:312-336` and `:349-358`: the rate given to the client, the AI and the match log, and a world joined with it.
- `GameProtocol/WireFormat.h:43-47`: the welcome, which holds only the player.
- `GameLogic/InProcessServer.cpp:250-256`: the hello's checks, the version and the layout's hash.
- `GameProtocol/Server.h:112`: `Server::TicksPerSecond`.
- `Docs/ADR/ADR-013-client-view-and-controls.md:15`: decision 2's words.

---

## R2 — Acknowledgement or prediction over a real network

**The question.** Does play over a real network, a world joined from another machine, need command acknowledgement or client-side prediction, decided from a measurement of the delay between an order and its response over that network?

**What the code does.** Neither exists. An order shows its effect only once the snapshot of the tick that applied it arrives, shown a tick behind and interpolated (`SnapshotInterpolator`); a refused command is not reported to the client. A world on `OutpostServer` is joined over QUIC from another machine (ADR-078 decisions 6 and 9), and no order's response has been measured over a network.

**What the documents said.** ADR-002's Consequences: "Before a real network is added, measure it and decide on command acknowledgement or local prediction then." ADR-002 decision 5: "Client-side prediction is not needed for the MVP." The MVP's target of 150 ms from an order to its visible response was met in-process only.

**The proposal.** Keep neither for now. Measure an order's response in a world joined from another machine during the owner's week, against the 150 ms target that [the budgets](../Budgets.md) carry, and decide from the figure. ADR-002 decision 5 says what the code does meanwhile.

**What it affects.** ADR-002 decision 5; ADR-002's Consequences; Game §orders.giving; [the budgets](../Budgets.md)' order-to-response row.

**Evidence.**

- `GameApp/SnapshotInterpolator.h:16-33`: a tick behind, interpolated, nothing predicted.
- `OutpostCommander/WinMain.cpp:349-358`: a world joined over `QuicTransport`.
- `GameLogic/InProcessServer.cpp:482`: a refused command, not reported.
- `Docs/ADR/ADR-002-authoritative-server.md:22` and `:30`: decision 5 and the Consequences.

---

## R3 — A slow shooter's reveal

**The question.** Should a shooter that keeps firing stay revealed between the shots of the slowest weapon, which fires every 6 s against a 3 s reveal?

**What the code does.** `shotRevealSeconds` is 3 in `Tuning.json`'s `sight` object, and each hit reveals its shooter to the side it hits for that long. The Rail Cannon fires every 6.0 s from 240 m, so a Rail Cannon ship firing from beyond its target's sight is seen for 3 s of every 6.

**What the documents said.** ADR-024 decision 1: "Three seconds is longer than the slowest fire interval, the Lance's 2.7 s, so a ship that keeps firing stays revealed." The owner's choice of 2026-10-02, in ADR-024's Context: "A shooter is revealed to the side it hits, for a while after each hit", so that a longer weapon does not shoot unseen.

**The proposal.** Keep the data. ADR-024 decision 1 now says what the reveal does: `shotRevealSeconds` after each hit, so a weapon slower than that is seen only for that time after each of its hits. If the owner wants every shooter that keeps firing to stay revealed, `shotRevealSeconds` rises above the slowest fire interval, which is a change to the data alone.

**What it affects.** ADR-024 decision 1; Game §combat.seen; Game §sight.reveal.

**Evidence.**

- `Docs/ADR/ADR-024-fog-of-war.md:18`: decision 1's words.
- `GameLogic/Simulation.cpp:1596-1598`: each hit reveals its shooter.
- `GameLogic/Simulation.cpp:423-434`: `RevealShooter`, for `shotRevealSeconds` from the hit.
- `GameLogic/Simulation.cpp:295-296`: a reveal fades.
- `OutpostCommander/Assets/Tuning.json:13`: `shotRevealSeconds` 3.
- `OutpostCommander/Assets/Tuning.json:30`: the Rail Cannon, every 6.0 s from 240 m.

---

## R4 — Explored ground across a join

**The question.** Is the ground a player has explored part of what a world remembers for it, or the client's presentation that starts afresh on every join?

**What the code does.** Which fog cells were seen before lives only in the client's `FogOfWar`; the server neither sends nor saves it. A game that joins a world, or joins again after a lost connection, draws everything out of sight as never seen, though the server's memories of structures, derelicts and reserves arrive whole in the first snapshot. ADR-046 decision 8's rule, a Mining Rig only onto an asteroid the player has seen, reads the same record in the client.

**What the documents said.** The owner's choice of 2026-10-02, in ADR-024's Context: "Ground never seen is dark, and ground seen before is dimmed". ADR-024's Consequences: "The fog the client draws is presentation." The Phase 5 design, as accepted on 2026-10-08: the first snapshot after a seat is taken again "shows everything the player sees and remembers". Nothing said what happens to explored ground across a join.

**The proposal.** Keep it as presentation, as built: ADR-024 decision 9 and Game §sight.memory say that a join starts it afresh. Keeping it in the world would be new saved state and a wire change (AGENTS.md R18), which is a new decision.

**What it affects.** ADR-024 decision 9; ADR-046 decision 8; Game §sight.memory; Game §session.join.

**Evidence.**

- `GameApp/GameClient.cpp:501` and `:603-620`: the client's fog, made afresh for each game.
- `GameApp/FogOfWar.cpp:84-90` and `:196`: what was seen, kept only in the client.
- `Docs/Plans/OutpostCommander-Phase5.md:68`: the Phase 5 design's words.

---

## R5 — The cut-off share on the panel

**The question.** Does a sector's panel read the cut-off share from the data, or is the share fixed at half by decision?

**What the code does.** A rig's or a Relay's panel writes the fixed text "\<sector\>: cut off, it earns half". The simulation pays the tuning data's `territory.cutOffIncomePercent`, 50 (`Simulation::TerritoryShare`), and `SectorView` does not carry it, so the client cannot read it. The two agree today; a change to the data would leave the text wrong.

**What the documents said.** ADR-056 decision 2 makes the share tuning data, and ADR-008's "What this forecloses" rules out a number the code keeps as a second copy. The owner set half as a starting value on 2026-10-03, when the territory rules were designed. No commit, ADR or plan explains the fixed text.

**The proposal.** Keep the code as it is in this run, and quote the text as built. With the next change to the territory panel, the snapshot carries the share and the panel writes it, so that the data stays the only copy.

**What it affects.** ADR-056 decisions 2 and 11; ADR-008 decision 3; Game §territory.cutoff; Game §interface.hud.selection.

**Evidence.**

- `GameApp/Hud.cpp:2607-2608`: the fixed text.
- `OutpostCommander/Assets/Tuning.json:14`: `cutOffIncomePercent` 50.
- `GameProtocol/Snapshot.h:287-308`: `SectorView`, without the share.
- `GameLogic/Simulation.cpp:2978`: the share paid from the data.

---

## R6 — A player's own color in seat 2

**The question.** In a world, is a player's own side drawn in the Human set's blue whatever its seat, or does the HUD color sides by seat as the world does?

**What the code does.** `Models.json`'s players give seat 1 the Human set, in blue, and seat 2 the Tarkan set, in orange-red, and `GameClient` draws every model, beam, bar's back and territory outline by its owner's seat (`ModelCatalog::SetForPlayer`). The HUD colors by viewer: `OWN_COLOR` blue for the player's own marks, nodes and outlines, and `ENEMY_COLOR` red for the other's. A human in seat 2 of a world sees its own ships orange-red in the world and blue on the minimap, and the enemy's the other way round.

**What the documents said.** ADR-011 decision 3 (owner, 2026-10-01): blue for the player's Human set and orange-red for the AI's Tarkan set, which assumes the player is seat 1. ADR-085: one meaning per color. ADR-078 decision 2 makes a seat for each start, and a join file names any player. No ADR or plan decides seat 2's colors.

**The proposal.** Draw the sets by viewer: the viewer's own side always takes the Human set in blue, and the other player the Tarkan set in orange-red, so that the world and the HUD agree and ADR-085's one meaning per color holds; ADR-011 decision 3 then says so. Meanwhile ADR-011 decision 3 and Game §look.models.sets state what the code does.

**What it affects.** ADR-011 decision 3; ADR-085 decision 1; Game §look.models.sets; Game §interface.style.colors.

**Evidence.**

- `OutpostCommander/Assets/Models.json:53-56`: the players' sets, by seat.
- `GameApp/ModelCatalog.cpp:304-316`: `SetForPlayer`.
- `GameApp/GameClient.cpp:726`, `:1226`, `:1643` and `:1757`: a bar's back, a sector's outline, a model and a beam, by seat.
- `GameApp/Hud.cpp:26-27`: `OWN_COLOR` and `ENEMY_COLOR`, by viewer.
- `Docs/ADR/ADR-011-meshes-and-shading.md:15`: decision 3's words.

---

## R7 — Another player's structure, selected

**The question.** Does a click select another player's structure, seen or remembered, so that the remembered structure's panel opens, or does that panel go?

**What the code does.** `Hud::Describe` lays out a remembered structure's selection panel, "Last seen 4:12 ago" and then what was seen "when seen", and `HudTests.DescribesAMemoryWithItsAge` checks it. But `PlayerControls` picks only the player's own ships and structures, and every other way to select, a ship's bar or the Shipyards' line, selects the player's own. A remembered structure is always another player's, so no player meets the panel; the hint under the pointer is the memory's only description.

**What the documents said.** Gate V3 of [the second interface plan](ImplementationPlan-Interface2.md) (owner, 2026-10-08) and ADR-081 decision 4: a memory's age shows on hover and on its selection panel. ADR-013 decision 5: a click selects the player's own structure. ADR-066 decision 4: an enemy structure's queue stays unshown. No commit, ADR or plan says the panel was meant to be unreachable.

**The proposal.** Make the panel reachable as V3 decided: a left-click on another player's structure, seen or remembered, selects it alone for its panel, which offers no buttons and takes no orders; ADR-013 decision 5 then says so. The other way is to drop the panel from ADR-081 decision 4, and the enemy clause from ADR-066 decision 4. Meanwhile ADR-081 decision 4 says that the panel is laid out and no click opens it, and Game §interface.hud.hint states the hint only.

**What it affects.** ADR-081 decision 4; ADR-013 decision 5; ADR-066 decision 4; Game §interface.hud.selection; Game §interface.hud.hint.

**Evidence.**

- `GameApp/PlayerControls.cpp:263-275`: a click picks only the player's own ships and structures.
- `GameApp/Hud.cpp:2574-2588`: the remembered structure's panel.
- `GameAppTests/HudTests.cpp:961-993`: `DescribesAMemoryWithItsAge`.
- [The second interface plan](ImplementationPlan-Interface2.md): gate V3, and task UI1.2's scope and As built.
- `Docs/ADR/ADR-081-territory-and-memory-over-the-fog.md:31`: decision 4's words.

---

## R8 — The balance shares after split orders

**The question.** Does the shift in the balance check's shares from planning a large order over two ticks matter? At 12,000 Ore under spread fire the Medium hull becomes worth building, in M+I+La at 21%, and the 4,500 Ore stage under spread fire becomes an even three-way.

**What the code does.** A large order is planned over two ticks (ADR-032). The balance check passes all four of its criteria before the change and after it, and every recorded counter holds (`BalanceCheckTests.TheRecordedCountersHold`).

**What the documents said.** ADR-032's Consequences gave the shift and said: "Whether the shift matters is the owner's to say." No answer is recorded. ADR-032 now says what the code does, and the shift's figures are in [the balance log](../Records/BalanceLog.md).

**The proposal.** Accept the shift: the check passes all four criteria and every recorded counter holds, and the shares are a property of the battle model, not a rule.

**What it affects.** ADR-032; the balance log's entry for the split orders.

**Evidence.**

- `Docs/ADR/ADR-032-order-ticks.md:27`: the shift, and the open clause.

---

## R9 — Criterion (d) at tier 1 after the arcs

**The question.** Is the balance check's criterion (d) at tier 1 settled by the later check of record that passed, so that nothing waits on a retune since ships began to turn in arcs?

**What the code does.** Ships turn in arcs (ADR-039), and the balance check of record passes (`BalanceCheckTests`; [the balance log](../Records/BalanceLog.md) records each check).

**What the documents said.** ADR-039's Consequences: criterion (d) was unsure at tier 1, the brawler winning 53% of 480 battles against the plated swarm at 2,000 Ore under spread fire, and "Nothing is retuned until the owner decides." The later check of record passed. No owner answer is recorded. ADR-039 now says what the code does, and the check's figures are in the balance log.

**The proposal.** Settled by the passing check: nothing is retuned.

**What it affects.** ADR-039; the balance log's entries for the checks with arcs.

**Evidence.**

- `Docs/ADR/ADR-039-ships-turn-in-arcs.md:31-33`: criterion (d) unsure, and the open clause.

---

## R10 — The thin margins against a raid

**The question.** Do the Defence Platform's and the Command Station's thin margins against a raid hold in play? The platform keeps 28% of its hit points against five raiders, and the station's minute is 2.4 s from failing.

**What the code does.** The numbers stand as tuned on 2026-10-01, and no later retune moved what the margins depend on. Nothing was measured again for this plan.

**What the documents said.** The MVP design's starting numbers left the margins as they were until the owner's playtests showed whether raids play out as intended (owner, 2026-10-01). No playtest's verdict is recorded.

**The proposal.** Judge it in the owner's next playtest against the AI's raids, recorded in [the playtest log](../Records/PlaytestLog.md); until then the numbers stand.

**What it affects.** Game §structures.platform; Game §structures.station; Game §combat.structures.

**Evidence.**

- The MVP design's starting numbers, in `OutpostCommander-MVP.md` at 8366820, lines 392-395: the platform keeps 420 of 1,500 hit points against five Small+Ion+Mass Driver ships, the station destroys seven in 57.6 s, and the margins wait on the owner's playtests.
