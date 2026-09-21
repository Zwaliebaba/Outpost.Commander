# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

**Twenty-six answered, one open.** Eight came from an adversarial review that also reversed two earlier
answers and corrected three statements that were wrong. What remains is measurement, not decision.

---

## Answered — 2026-09-20

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q1** | Is the playfield a volume, a plane, or a plane with altitude bands? | **A plane.** A tap is a ray and a ray has no depth, and R21 leaves no second input to supply one. | [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) |
| **Q2** | Is the home base a fixed station or a mobile mothership? | **A fixed station.** Removes base pathfinding, docking and a class of AI problem from the MVP; a mothership is a hull like any other later. | `GameDesign.md` §5 |
| **Q3** | How much of the *Warzone 2100* design system lands in the MVP? | **The model, not the interface.** Hull, drive and slots in the simulation and on the wire from the first line; three fixed designs and no designer screen. | [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) |
| **Q4** | What fleet scale should the MVP target? | **About fifty ships a player** — with the station and four modules that is **110 entities in the reduced MVP** (Q27, Q28) and 220 at four players, and 110 is what makes a snapshot fit one datagram, with six entities of headroom. | `GameDesign.md` §10, [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) |
| **Q5** | How does a client find a host? | **It does not — the address is configuration.** A one-line file in `LocalState` with `127.0.0.1` compiled in as the default; no discovery, no address entry. The loopback exemption this implies is a development arrangement and never a shipping one. | [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md) |
| **Q6** | What is the target device? | **The Surface Pro.** 13-inch 3:2, 2880 × 1920 at 267 PPI, 200% scale, 1440 × 960 DIPs. The current model is ARM64, which makes that CI leg a real target. | `Interface.md` §1, [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) |
| **Q7** | Is the authored frame 16:9 or 3:2? | **3:2, at 1440 × 960** — it follows from Q6 rather than being a separate choice. An exact 2× point-sampled fit on the target panel, no letterbox. The 16:9 recommendation this register carried was wrong once the device was known. | [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) |

## Answered — 2026-09-20, second round

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q8** | How does a player select more than one ship? | **A hold on a ship takes every ship of the same design within a circle centred on it.** Screen-space, 192 authored pixels, drawn while the finger is down, so the camera's zoom is the group-size control. **No band select**, which leaves one-finger drag unconditionally panning. The owner's answer; it beat all four options on the ballot, including the draft's hold-then-drag. | [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md), `Interface.md` §3–§4 |
| **Q9** | What happens when a human disconnects mid-match? | **The slot persists, ships hold position, and they may reconnect.** Self-contained snapshots make reconnection free. An AI taking the slot is the better answer and waits for M4. | `GameDesign.md` §2 |
| **Q10** | Does the station have a weapon? | **Yes — two short-range `PointDefence` mounts.** It kills a loiterer, not a besieger and not a fleet: `PointDefence` reaches 400 and a `MassDriver` reaches 600, so a fighter can stand off and shell the station untouched. Deliberate — with the heavy design cut, a station whose defence outranged the fighter would be unkillable. | `GameDesign.md` §5, §6, §7 |
| **Q11** | How is text rendered? | **DirectWrite rasterised into a Direct3D 12 atlas we own.** No Direct2D and no `ID3D11On12Device`, which R12 bans by name — that closes the route every D3D12 text sample takes. Coverage is ClearType averaged to one channel, because subpixel output would fringe after the 2× scale. | [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md), `Interface.md` §6 |
| **Q12** | What happens on suspend and resume? | **Reconnect with an overlay**, then straight back in. The fleet was at risk the whole time, which is the honest consequence of a match that does not pause. | `Interface.md` §7 |
| **Q13** | Can two players say anything to each other? | **Nothing in the MVP.** Solo against AI is the only configuration the MVP can test. The gesture a ping would use is sitting unassigned (Q18). | `Interface.md` §7 |
| **Q14** | Is the counter to mass really speed? | **Deferred to M4, and the deferral is now honest.** The answer was that M3 would tell us — but M3 cannot: the heavy design cost 160 seconds of total income against a strike force that crosses the map in 80 to 100 seconds, so no match would ever have contained one. The `Cruiser` is cut from the MVP and returns at M4 with its weapon and its damage row. | `GameDesign.md` §6, §7, §10 |
| **Q15** | Should any design decision become an `AGENTS.md` rule? | **All three, as R22, R23 and R24**, each citing its source: the simulation is two-dimensional; the client derives the world from the seed; a ship is a composition and every stat is derived. `AGENTS.md` R25 and up are now the reserved range. | [`AGENTS.md`](../AGENTS.md) §5 |

---

## Answered — 2026-09-20, third round

Each of these confirmed the draft rather than changing it, which is worth recording: a question put and
answered the way it was already written is a different thing from a question never asked.

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q16** | Does the station's point defence cover the home asteroid field? | **No — the station only.** Miners at the field stay raidable, so early fighters have a job and there is a real choice between escorting and expanding. Retreating to the safe zone costs a cargo run, which is the exchange the point defence exists to create. Covering the field would make the opening safe and the early game pure build-up. | `GameDesign.md` §5 |
| **Q17** | What does `Holding` over empty space mean? | **Nothing, deliberately.** R21 hands out three verbs and this design has already had to refuse a feature for want of one — order queueing has no gesture and is out of the MVP because of it. An idle affordance is cheap; a gesture spent on something marginal is not there when something real needs it. | `Interface.md` §7 |
| **Q18** | Is pixel-doubled text acceptable? | **Superseded by [`ADR-011`](ADR/ADR-011-the-interface-draws-after-the-scale.md): the interface now draws after the scale, so text is not doubled at all.** The original answer, for the record: yes, and M1 confirms rather than decides. Large type, a dozen strings, and a doubled pixel at 267 PPI is a 133-PPI effective pixel — ordinary desktop density, not visible pixel art. Drawing the interface after the scale was declined: it would buy pixel-perfect text for an explicit exception to R13 and a second place that knows the window size. | [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md), `Interface.md` §6 |

---

## Answered — 2026-09-20, fourth round: an adversarial review

A second designer was asked to defeat this design where it could be defeated. **Eight new questions came
out of it, two earlier answers were reversed** (Q14 and Q18 above) **and three statements were corrected
because they were simply wrong.**

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q19** | How do fifty ships occupy one point? | **A ring slot per ship, assigned at order time, ordered by entity identity.** No separation force and no flocking — those are floating-point-shaped problems in an integer simulation, and a formation system later is this same assignment with a different slot layout. The cost is *induced by* [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md): in a volume ships miss each other in the third dimension, on a plane they stack. It was named once and owned by nobody. | `GameDesign.md` §7, `TechnicalDesign.md` §1, §8 |
| **Q20** | What does the client draw between a tap and confirmation? | **A destination marker and a line, drawn the instant the gesture resolves and cleared when the host acknowledges that sequence.** Pure presentation: R19 forbids the client simulating, not the client drawing what it asked for. Without it there are 152 ms of nothing on a device that has no cursor. | `Interface.md` §4, `TechnicalDesign.md` §6 |
| **Q21** | Does the build queue have a wire record? | **There is no queue — the snapshot carries the item currently building and its progress, two bytes per player.** `Interface.md` had specified a cancellable queue that no wire record could feed; the MVP cuts the queue rather than inventing a format for it. | `Interface.md` §6, `TechnicalDesign.md` §4 |
| **Q22** | Is asteroid ore replicated, and at what cost? | **Not at all before M3**, since inexhaustible asteroids have no simulation state and the client derives their positions from the seed. From M3, sparsely: only asteroids whose quantised ore bucket changed, at most one per active miner. The budget had excluded a thing R23 said must be replicated. | `GameDesign.md` §4, `TechnicalDesign.md` §4 |
| **Q23** | Is the client forced fullscreen? | **Yes, at launch.** Nothing previously forced it, which made [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md)'s exact 2× an accident of however the window happened to be sized. | `Interface.md` §1, `TechnicalDesign.md` §5 |
| **Q24** | What does the host validate on a command? | **Ownership, selection length, generation, target bounds and sequence wraparound** — labelled correctness rather than security, so it is not filed under the anti-cheat exclusion again. A 1,200-byte packet holds 592 identities against a peak of 102, reachable from an ordinary bug with no attacker anywhere. | `TechnicalDesign.md` §4, §8 |
| **Q25** | How does a match end, and how do you start another? | **The host reseeds and restarts; the client shows a result overlay and reconnects.** Nothing said what happened at victory, and restart is the most-used operation in a solo testing loop — twenty matches an evening is impossible if playing again means relaunching a package. | `Interface.md` §7, `GameDesign.md` §10 |
| **Q27** | Does the MVP ship two players or four? | **Two, through M3.** The generator's symmetry, the AI count and the snapshot's per-player blocks are all sized by a runtime player count, so the other two slots are configuration rather than a change. Two buys a five-minute match, a single-datagram snapshot, and twenty matches an evening. | `GameDesign.md` §2, §10, [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) |

**The three corrections, which were not questions:** R16's FMA argument — contractions are *not* generated
by default under `/fp:precise` from Visual Studio 2022 onward and the tree pins `v145`, so the rule was
defended by a citation that does not survive being looked up;
[`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md)'s "a ray has no depth", which a camera-relative focus
plane defeats, so the plane now stands on implementation cost and the camera's gesture budget instead; and
the wire format's two-bit design identity, which capped designs at four and contradicted R24 outright.

---

## Answered — 2026-09-21, fifth round: the base is built from modules

The owner added a feature: a base built out of modules, each doing something and each upgradeable, with
more added later. Four questions settled its shape; all four are in
[`ADR-015`](ADR/ADR-015-the-base-is-built-from-modules.md) and `GameDesign.md` §5.

| | Question | Answer |
|---|---|---|
| **Q28** | Are modules separate map entities, or part of the station? | **Separate, destroyable entities**, placed within 400 units of the station. Taken for one reason: it is the only version where an attacker can cripple an economy without killing the base, which is what *Warzone 2100*'s buildings are for. Costs the single-datagram property most of its headroom — six entities left where there were fourteen — which is why the cap is four. |
| **Q29** | Can a bare station build ships? | **Yes; the shipyard raises the ceiling.** In the MVP that ceiling is **build rate**, not new hulls, because the MVP has two designs and the station already builds both — a module gating *what* could be built would gate nothing. From M4 the same levels gate heavier hulls and the designer. |
| **Q30** | What does the research station do with no research? | **It is designed now and built at M4**, with research. The MVP ships two working modules. A module that costs credits and does nothing is the mistake the heavy design already made. |
| **Q31** | How do upgrade levels work? | **Each level is its own component identity** — `ShipyardL1` and `L2` are two catalog rows and upgrading replaces one with the next. Free under R24: no new axis in the derived-stat function, and research gating a level is the gate it already needs over any component. |

**One observation recorded rather than acted on.** The radar module the owner offered as a future example
would show a minimap, which `Interface.md` §5 cut on the grounds that maximum zoom-out already *is* a
top-down view of the whole map. A radar module is only a real reward when there is something you cannot
see — so **radar and fog of war are the same decision**, and the module system's first genuinely valuable
module arrives with fog, post-MVP.

---

## Open

### Q26 — What is the asteroid count, and the spawn anchor radius? — **needed by M2**

Neither appears anywhere in this design, and both are inputs to things that do. The anchor radius sets how
long a strike force takes to cross the map, which is half of the raid arithmetic in `GameDesign.md` §7. The
asteroid count sets the sparse ore budget from M3 (Q22) and how much a home field is worth holding.

**No recommendation.** Both follow from playing, and guessing them now would add two more unmeasured
numbers to a document that has plenty. What is required is that the generator *names* them rather than
leaving them implicit in code.

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
