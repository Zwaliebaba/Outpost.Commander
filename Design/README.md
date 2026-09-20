# Design — what *Outpost Commander* is

[`AGENTS.md`](../AGENTS.md) says **how** code is written here; this directory says **what** is being built.
The two sit alongside each other: a design document never overrides an engineering rule, and an
engineering rule never decides a game mechanic. Where a design decision has to constrain the *shape* of
code, it is proposed as an `AGENTS.md` §5 rule citing the section here that is its source — the design is
the source and `AGENTS.md` is the rule, in that order.

**Status: DRAFT, 2026-09-20.** This is a first pass written against the brief and against `AGENTS.md`,
starting from an empty `Design/` and a build shell with no game in it. Seven questions have been put to
the owner and answered; eight are open on the register and **none of them blocks the first milestone**.
Nothing here is settled until the owner accepts it, and half the ADRs are still Proposed.

## The documents

| Document | What it settles |
|---|---|
| [`GameDesign.md`](GameDesign.md) | The game: the two lineages and which half the MVP is, the session and victory, the area and how it is generated, the economy, the station, ships as compositions, combat, the AI, where research goes, and the five milestones |
| [`TechnicalDesign.md`](TechnicalDesign.md) | How it is built inside `AGENTS.md`: what lives in which of the six libraries, the tick and the numbers, the world and its generator, replication and the transport, the client's frame, why there is no content pipeline, what each test suite owns, and what must be measured |
| [`Interface.md`](Interface.md) | What the commander sees and touches: the frame and the derived touch target, the gesture seam, the vocabulary, selection and orders, the camera, the five panels, and the six things it does not settle |
| [`OpenQuestions.md`](OpenQuestions.md) | The register: seven answered, eight open, each with its options, what each costs, a recommendation where there honestly is one, and the milestone that needs it |
| [`ADR/`](ADR/README.md) | Engineering decisions, one file per decision, `ADR-001` to `ADR-008` |

Read them in that order. `GameDesign.md` stands alone for a reader who knows real-time strategy games;
`TechnicalDesign.md` assumes `AGENTS.md` has been read, because it cites its rules by number rather than
restating them; `Interface.md` assumes both.

## What the design actually decided

Three things shape everything else, and each is an ADR because each is expensive to reverse:

**The playfield is a plane** ([`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md)). *Homeworld*'s third
axis does not survive contact with R21 — a tap is a ray and a ray has no depth, and there is no second
input to supply one. The camera keeps all three dimensions; the simulation has two. This is what makes
the MVP reachable, and the tactical z-axis is what it costs.

**Replication is full self-contained snapshots**
([`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md)). R19 already ruled out lockstep by refusing
the client a simulation; at 204 entities a snapshot is under two kilobytes, so there is no baseline, no
acknowledgement and no history — and therefore no divergence, because there is nothing accumulated to
diverge. What it costs is bandwidth and a 200-millisecond gap when a fragment is lost, both stated with
the arithmetic behind them.

**A ship is a composition** ([`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md)). The *Warzone 2100*
model goes in before its interface does, because retrofitting it would move damage, cost, build time,
mass, speed and the wire format in one change. The miner is the proof: it is not a ship type, it is a
scout hull with a mining tool where a weapon would go.

## What M0 has to find out

Nothing on the register blocks M0 any more, but the first milestone is still there to answer questions
rather than to build a game, and two of them are uncomfortable:

- **Whether a single-machine development loop is usable at all.** The host address is `127.0.0.1` by
  default ([`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md)), which needs a loopback exemption
  that Microsoft documents as a sideload-or-debugging arrangement, and that Visual Studio grants silently
  on every F5. If UDP replies turn out to need the inbound form, `CheckNetIsolation.exe` has to stay
  running the whole time — at which point two machines are the answer, and it is far better to know that
  in week one.
- **Whether the present step really lands on an exact 2×** on a Surface Pro
  ([`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md)). R13's whole arrangement is worth nothing if
  a conversion error puts the scale at 1.99, and that is a thing you confirm by looking at the screen.

## How a design changes

- **A change to what the game is** edits the relevant document in a pull request the owner approves. The
  document is the record; there is no separate changelog.
- **A decision taken while building** — a format, a protocol, a subsystem's shape, an exception to a rule
  — is an ADR, written in the same commit as the code (`AGENTS.md` §6), and the design document it settles
  is updated in that commit to cite it.
- **A question** goes on the register with its options and a recommendation, and its answer is written
  into the document it belongs to.

An ADR marked **Proposed** is a decision this design takes and the owner has not yet ruled on. **It is not
something to write code against.** Four of the eight are Proposed today.

## What is deliberately not designed yet

Audio, any campaign or narrative, art direction beyond `Interface.md` and the generated meshes of
`TechnicalDesign.md` §7, a content file format, mods, replays and saved matches. Each is worth doing
against something running, and `GameDesign.md` §10 says when that is.

**Research and the ship designer are the exception**, and they are deliberately half-designed: the
simulation is built so both are additive (`GameDesign.md` §9), but what research actually unlocks and in
what order is not settled, because settling it now would be guessing.
