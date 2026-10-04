# Outpost Commander — Self-Play Plan

Status: **open** · Started 2026-10-04, when the owner opened self-play as a side project and answered its first questions · Derived from [ADR-061](../Design/ADR/ADR-061-self-play-probe.md) and [the network's blueprint](../Design/SelfPlayNetwork.md)

Self-play is a side project beside the game's phases. AIs fight each other, and a search keeps what wins. It is a probe of the match's rules, and it changes nothing a player sees. [The Phase 2 plan](ImplementationPlan-Phase2.md) stays the game's open plan. This plan says **in what order** the side project is built, as a queue of tasks. It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## The owner's answers, 2026-10-04

- **What the network decides:** macro decisions and fleet tactics. Macro decisions are research, designs, and when to attack, hold, raid or claim. Fleet tactics are which target a group goes for and when it falls back. The scripted AI keeps placement, the Constructors and pathing.
- **What it is for:** a probe of the rules. The opponent the game ships stays the scripted one (design §10, ADR-041).
- **Where it trains:** an x64 laptop, on the Release|x64 build.
- **What comes first:** a search over `Opponent.json`'s numbers, with no network. It is the baseline a network has to beat.

## How an agent uses this plan

1. **Read AGENTS.md, ADR-061 and ADR-020, then the blueprint for milestone SP2.**
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone, in milestone order.
3. **Tasks are numbered SP\<milestone\>.\<task\>,** apart from the game's milestones, so that the phases keep their numbers. The blueprint's open questions are gates N1 to N5.
4. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. The server, the AI and the switch build and run there against a stand-in for the Windows headers, as the phases' did. A search on the real build is the owner's run.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| SP1.1 | `--ai-matches` plays any two settings | — | — | in review, [#64](https://github.com/Zwaliebaba/Outpost.Commander/pull/64) |
| SP1.2 | The search over the AI's numbers | SP1.1 | — | in review, [#64](https://github.com/Zwaliebaba/Outpost.Commander/pull/64) |
| SP1.3 | The first search, and what it found | SP1.2 | — | todo: owner run |
| SP2.1 | The network's shape, as an ADR | SP1.3 | N1–N5 decided | todo |
| SP2.2 | The network plays | SP2.1 | — | todo |
| SP2.3 | Trajectories, the warm start and the trainer | SP2.2 | — | todo |
| SP2.4 | The league, and what the network found | SP2.3 | — | todo: owner run |

### Milestone order

SP1, then SP2. The network is built only once the search over the numbers has a result, because that result is its baseline. SP1 answers what fixed numbers can do. SP2 answers what decisions that adapt to the match can do on top of them.

---

## Milestone SP1 — The search over the AI's numbers

### SP1.1 — `--ai-matches` plays any two settings

- **Goal:** a script plays any two settings of the AI against each other, on the seeds it names, into the log it names, with no message box.
- **Scope:** options for each AI's settings file, the first seed, the number of matches, the time limit and the log. `--quiet` reports a failure on standard error and by the exit code. `PlayAiMatches` takes each player's settings. With no options, the switch plays ADR-038's ten matches as before.
- **ADR:** ADR-061 decisions 1 and 2. ADR-038 decision 3, edited in place.
- **Verify:** CI builds it. In the container, against the stand-in: the same seeds log the same bytes as before, per-player settings take effect, and every malformed option fails with its own message. On Windows, one run of the switch with `--quiet` and a bad option shows the message on standard error; that is the owner's run in SP1.3.

### SP1.2 — The search over the AI's numbers

- **Goal:** ADR-061 decisions 3 to 8.
- **Scope:** `Tools/SelfPlay.py`: a separable CMA-ES over the 22 numbers of `Opponent.json` and its research order, against a champion and a hall of fame, from both seats on shared seeds; a champion confirmed on fresh seeds; state saved every generation, and `--resume`; the report against the packaged settings on seeds 1 to 40; `--self-test`.
- **Verify:** `--self-test`, and a short search against the stand-in in the container, stopped and resumed.

### SP1.3 — The first search, and what it found

- **Goal:** the headroom figure, and what the champion does that the packaged AI does not.
- **Scope:**
  - Build Release|x64.
  - Time one match: `--ai-matches --matches 1` prints its seconds.
  - Run `Tools\SelfPlay.py`, with `--generations` set to what the night allows.
  - Read `report.txt`.
  - Record here what the champion does, and sort each finding: a rule that wants changing, a weakness of the scripted AI, or neither.
  - The owner decides what follows: a rules change, an AI change, SP2, or nothing.
- **Verify:** **owner run.**

---

## Milestone SP2 — The network

[The blueprint](../Design/SelfPlayNetwork.md) describes the network in full: what it sees, what it decides, how it is trained, and what it costs. It is a proposal. SP2.1 turns it into decisions.

### SP2.1 — The network's shape, as an ADR

- **Goal:** the blueprint's gates N1 to N5 decided by the owner, and an ADR written from the blueprint as decided.
- **Scope:** the ADR only. The blueprint stays as the explanation, edited to what was decided.
- **Verify:** the owner's review.

### SP2.2 — The network plays

- **Goal:** a seat of `--ai-matches` plays with a network, deciding what the blueprint's heads decide, and the scripted body carries the decisions out.
- **Scope:**
  - In `Opponent`, the observation, the heads and their masks, the weights file and the forward pass.
  - The AI's own random draws, seeded from the match.
  - The switch's options that give a seat a network.
- **Acceptance:** `GameLogicTests`: the observation of a scripted snapshot; mirrored seats see mirrored observations; masks forbid what the server would refuse; a weights file reads back as written; a forward pass matches a reference; a network with random weights plays a whole match on the real server; the same seed and weights log the same match twice.
- **Verify:** CI; the container.

### SP2.3 — Trajectories, the warm start and the trainer

- **Goal:** the training loop runs, and its first network plays as well as the scripted AI it learned from.
- **Scope:**
  - The switch writes a trajectory for each seat that plays a network, and the scripted AI's own choices for a seat that does not.
  - A trainer under `Tools/` learns the scripted AI's choices first, then improves on them with PPO against the scripted AI.
- **Acceptance:** the trainer's self-test learns a toy problem; the warmed-up network wins about half its matches against the packaged settings.
- **Verify:** the container, on a short run; the owner's laptop for the rest.

### SP2.4 — The league, and what the network found

- **Goal:** the blueprint's league trained for the owner's budget, and its report: win rates against the packaged settings and SP1's champion, what the network does differently, and which decision carries its advantage.
- **Verify:** **owner run.**
