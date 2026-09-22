# Design — what *Outpost Commander* is

[`AGENTS.md`](../AGENTS.md) says **how** code is written here; this directory says **what** is being built.
The two sit alongside each other: a design document never overrides an engineering rule, and an
engineering rule never decides a game mechanic. Where a design decision has to constrain the *shape* of
code, it is proposed as an `AGENTS.md` §5 rule citing the section here that is its source — the design is
the source and `AGENTS.md` is the rule, in that order.

**Status: DRAFT, 2026-09-20**, with the fifth and sixth rounds applied on 2026-09-21. Written against the
brief and against `AGENTS.md`, starting from an empty `Design/` and a build shell with no game in it.
**Thirty-one questions answered, five open. All nineteen ADRs Accepted** — the nineteenth is
[`ADR-021`](ADR/ADR-021-content-ships-with-the-package.md), which rules that content files ship and is the
first record here to supersede part of an earlier one rather than amend it.

**A second designer was then asked to defeat it**, and the result is applied throughout. That review found
a solved game in the combat numbers, a factual error in `AGENTS.md` itself, and several features specified
in one document with no data path in another. It produced eight new questions, reversed two earlier
answers, and **cut roughly a third of the MVP**; `OpenQuestions.md`'s fourth round names what was wrong.

What is left is **measurement rather than decision**: `TechnicalDesign.md` §9 lists eight figures that
cannot be obtained until there is code. **The first is now discharged** — M0.9's encoder put the MVP
snapshot at 1,137 bytes and corrected the design's arithmetic by a byte in five documents. Three of the
remaining seven are owed at M0, and all three need hardware rather than code: tap-to-visible latency, loss
and jitter on a real wireless link, and which loopback exemption form a UDP client actually needs.

## The documents

| Document | What it settles |
|---|---|
| [`GameDesign.md`](GameDesign.md) | The game: the two lineages and which half the MVP is, the session and victory, the area and how it is generated, the economy, the station, ships as compositions, combat, the AI, where research goes, and the five milestones |
| [`TechnicalDesign.md`](TechnicalDesign.md) | How it is built inside `AGENTS.md`: what lives in which of the six libraries, the tick and the numbers, the world and its generator, replication and the transport, the client's frame, where the line around content actually is, what each test suite owns, and what must be measured |
| [`Interface.md`](Interface.md) | What the commander sees and touches: the frame and the derived touch target, the gesture seam, the vocabulary, selection and orders, the camera, the five panels, and the six things it does not settle |
| [`design_handoff_hud/`](design_handoff_hud/README.md) | **The HUD as drawn**: every rectangle in integer authored coordinates, the palette, the type scale and the motion table, with a `geometry.json` M1.14's test asserts against and four reference frames at 1440 × 960. It settles *where and what color*; `Interface.md` §6 *Where the geometry lives* settles which of the two wins where they overlap |
| [`OpenQuestions.md`](OpenQuestions.md) | The register: thirty-one answered across six rounds, five open — and, in the fourth round, what an adversarial review reversed and what it found simply wrong |
| [`ADR/`](ADR/README.md) | Engineering decisions, one file per decision, `ADR-001` to `ADR-021`, with `ADR-013` and `ADR-014` reserved |

Read them in that order. `GameDesign.md` stands alone for a reader who knows real-time strategy games;
`TechnicalDesign.md` assumes `AGENTS.md` has been read, because it cites its rules by number rather than
restating them; `Interface.md` assumes both. **`design_handoff_hud/` is read after `Interface.md` and
never instead of it** — it is a design pass over what §6 had already settled, so it restates figures it
does not own, and `CheckDesign.py` does not police those copies.

## What the design actually decided

Three things shape everything else, and each is an ADR because each is expensive to reverse:

**The playfield is a plane** ([`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md)). *Homeworld*'s third
axis does not survive contact with R21 — a tap is a ray and a ray has no depth, and there is no second
input to supply one. The camera still orbits and zooms over the plane; the simulation has two dimensions
and nothing else does. This is what makes the MVP reachable, and the tactical z-axis is what it costs.

**Replication is full self-contained snapshots**
([`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md)). R19 already ruled out lockstep by refusing
the client a simulation; at the MVP's 110 entities a snapshot is **1,137 bytes — one datagram** — so there
is no baseline, no acknowledgment, no history and no fragmentation. They go out at **20 Hz**, which is a
latency decision and not a bandwidth one: tap-to-visible is 152 ms average where 10 Hz made it 252 ms, and
on a touchscreen the tap is the only feedback a player gets.

The claim that nothing can diverge is **narrower than it was first stated**: true of positions, never true
of the client, which accumulates a selection set, order markers and wrecks by inference. An explicit
removal list is what makes that derived state correct — and what makes fog of war additive for the client
and not only for the protocol.

**A ship is a composition** ([`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md)). The *Warzone 2100*
model goes in before its interface does, because retrofitting it would move damage, cost, build time,
mass, speed and the wire format in one change. The miner is the proof: it is not a ship type, it is a
scout hull with a mining tool where a weapon would go — and so is the station, which is a hull with two
point-defense mounts and no drive.

It also paid for something unrelated. **Selection works by design**
([`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md)): a hold takes every ship of the same design
within a circle on screen, which is only a coherent idea because a design is a first-class identity. That
removes band select, which leaves one-finger drag meaning panning and nothing else — the cleanest the
gesture budget has been.

## What is checked by a hand rather than an argument

The register has one entry open, but several answers are **confirmations owed against hardware** rather
than choices already validated: whether 192 pixels is the right selection circle and whether the interface
pass outweighs the world pass, both at M1; and whether the repaired raid arithmetic and the station's safe
zone actually play, at M3.

And M0 exists to answer questions rather than to build a game. Three of them are uncomfortable:

- **Tap-to-visible latency.** 152 ms average is arithmetic on four design constants, none of them
  observed. It is the number that decides how the game feels, and every other decision is cheap beside it.
- **Whether a single-machine development loop is usable at all.** The host address is `127.0.0.1` by
  default ([`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md)), needing a loopback exemption
  Microsoft documents as a sideload-or-debugging arrangement and that Visual Studio grants silently on
  every F5. If UDP replies need the inbound form, `CheckNetIsolation.exe` has to stay running throughout.
- **Whether the present step really lands on the scale it computed** on a Surface Pro, and **which world
  scale ships** ([`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)). R13's whole arrangement is
  worth nothing if a conversion error puts the scale at 1.99 rather than 2, or 0.999 rather than 1, and
  that is confirmed by looking at the screen.

## How a design changes

- **A change to what the game is** edits the relevant document in a pull request the owner approves. The
  document is the record; there is no separate changelog.
- **A design decision that constrains the shape of code** is proposed as an `AGENTS.md` §5 rule citing the
  section here that is its source. Three have been: **R22** the simulation is two-dimensional, **R23** the
  client derives the world from the seed, **R24** a ship is a composition and every stat is derived. R25
  and up are the reserved range now.
- **A decision taken while building** — a format, a protocol, a subsystem's shape, an exception to a rule
  — is an ADR, written in the same commit as the code (`AGENTS.md` §6), and the design document it settles
  is updated in that commit to cite it.
- **A question** goes on the register with its options and a recommendation, and its answer is written
  into the document it belongs to.

An ADR marked **Proposed** is a decision this design takes and the owner has not yet ruled on. **It is not
something to write code against.** **None are Proposed today** — ADR-002 to ADR-005 were ruled on
2026-09-20 after the review, all four with changes, and **ADR-005 was ruled a second time on 2026-09-22**,
reversed rather than amended: a mesh is a CMO file where it had said a mesh was a function.

## What is deliberately not designed yet

Audio, any campaign or narrative, art direction beyond `Interface.md` and the meshes of
`TechnicalDesign.md` §7, mods, replays and saved matches. Each is worth doing against something running,
and `GameDesign.md` §10 says when that is.

**A content file format is no longer on that list.** [`ADR-005`](ADR/ADR-005-a-mesh-is-a-cmo-file.md)
settled it: a mesh is a CMO file, with the reader written here because CMO's only reader in the wild is
the DirectXTK12 that R14 closes.

**Research and the ship designer are the exception**, and they are deliberately half-designed: the
simulation is built so both are additive (`GameDesign.md` §9), but what research actually unlocks and in
what order is not settled, because settling it now would be guessing.
