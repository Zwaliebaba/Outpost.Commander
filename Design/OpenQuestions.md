# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

**Eighteen answered, none open.** What remains is measurement rather than decision — see the end of
this file.

---

## Answered — 2026-09-20

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q1** | Is the playfield a volume, a plane, or a plane with altitude bands? | **A plane.** A tap is a ray and a ray has no depth, and R21 leaves no second input to supply one. | [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) |
| **Q2** | Is the home base a fixed station or a mobile mothership? | **A fixed station.** Removes base pathfinding, docking and a class of AI problem from the MVP; a mothership is a hull like any other later. | `GameDesign.md` §5 |
| **Q3** | How much of the *Warzone 2100* design system lands in the MVP? | **The model, not the interface.** Hull, drive and slots in the simulation and on the wire from the first line; three fixed designs and no designer screen. | [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) |
| **Q4** | What fleet scale should the MVP target? | **About fifty ships a player**, 204 entities at peak, which is what makes full snapshots affordable. | `GameDesign.md` §10, [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) |
| **Q5** | How does a client find a host? | **It does not — the address is configuration.** A one-line file in `LocalState` with `127.0.0.1` compiled in as the default; no discovery, no address entry. The loopback exemption this implies is a development arrangement and never a shipping one. | [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md) |
| **Q6** | What is the target device? | **The Surface Pro.** 13-inch 3:2, 2880 × 1920 at 267 PPI, 200% scale, 1440 × 960 DIPs. The current model is ARM64, which makes that CI leg a real target. | `Interface.md` §1, [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) |
| **Q7** | Is the authored frame 16:9 or 3:2? | **3:2, at 1440 × 960** — it follows from Q6 rather than being a separate choice. An exact 2× point-sampled fit on the target panel, no letterbox. The 16:9 recommendation this register carried was wrong once the device was known. | [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) |

## Answered — 2026-09-20, second round

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q8** | How does a player select more than one ship? | **A hold on a ship takes every ship of the same design within a circle centred on it.** Screen-space, 192 authored pixels, drawn while the finger is down, so the camera's zoom is the group-size control. **No band select**, which leaves one-finger drag unconditionally panning. The owner's answer; it beat all four options on the ballot, including the draft's hold-then-drag. | [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md), `Interface.md` §3–§4 |
| **Q9** | What happens when a human disconnects mid-match? | **The slot persists, ships hold position, and they may reconnect.** Self-contained snapshots make reconnection free. An AI taking the slot is the better answer and waits for M4. | `GameDesign.md` §2 |
| **Q10** | Does the station have a weapon? | **Yes — two short-range `PointDefence` mounts**, weighted against small and medium hulls. It kills a loiterer, not a fleet: the area around the station becomes a safe zone miners can retreat to. Reverses the draft, which had it undefended. | `GameDesign.md` §5, §6, §7 |
| **Q11** | How is text rendered? | **DirectWrite rasterised into a Direct3D 12 atlas we own.** No Direct2D and no `ID3D11On12Device`, which R12 bans by name — that closes the route every D3D12 text sample takes. Coverage is ClearType averaged to one channel, because subpixel output would fringe after the 2× scale. | [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md), `Interface.md` §6 |
| **Q12** | What happens on suspend and resume? | **Reconnect with an overlay**, then straight back in. The fleet was at risk the whole time, which is the honest consequence of a match that does not pause. | `Interface.md` §7 |
| **Q13** | Can two players say anything to each other? | **Nothing in the MVP.** Solo against AI is the only configuration the MVP can test. The gesture a ping would use is sitting unassigned (Q18). | `Interface.md` §7 |
| **Q14** | Is the fighter-against-battleship counter real? | **Unknown, and deliberately left so — M3 answers it.** No mechanism is added in advance. If it is false, the damage table is the cheapest lever and the price list the next; tracking-and-evasion rolls and a minimum range on capital weapons are the structural answers, both declined for now. | `GameDesign.md` §7 |
| **Q15** | Should any design decision become an `AGENTS.md` rule? | **All three, as R22, R23 and R24**, each citing its source: the simulation is two-dimensional; the client derives the world from the seed; a ship is a composition and every stat is derived. `AGENTS.md` R25 and up are now the reserved range. | [`AGENTS.md`](../AGENTS.md) §5 |

---

## Answered — 2026-09-20, third round

Each of these confirmed the draft rather than changing it, which is worth recording: a question put and
answered the way it was already written is a different thing from a question never asked.

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q16** | Does the station's point defence cover the home asteroid field? | **No — the station only.** Miners at the field stay raidable, so early fighters have a job and there is a real choice between escorting and expanding. Retreating to the safe zone costs a cargo run, which is the exchange the point defence exists to create. Covering the field would make the opening safe and the early game pure build-up. | `GameDesign.md` §5 |
| **Q17** | What does `Holding` over empty space mean? | **Nothing, deliberately.** R21 hands out three verbs and this design has already had to refuse a feature for want of one — order queueing has no gesture and is out of the MVP because of it. An idle affordance is cheap; a gesture spent on something marginal is not there when something real needs it. | `Interface.md` §7 |
| **Q18** | Is pixel-doubled text acceptable? | **Yes, and M1 confirms rather than decides.** Large type, a dozen strings, and a doubled pixel at 267 PPI is a 133-PPI effective pixel — ordinary desktop density, not visible pixel art. Drawing the interface after the scale was declined: it would buy pixel-perfect text for an explicit exception to R13 and a second place that knows the window size. | [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md), `Interface.md` §6 |

---

## Open

**Nothing. Every question this design raised has been put to the owner and answered.**

That is not the same as the design being right, and the difference is worth stating plainly. **What is
left is measurement, not decision.** [`TechnicalDesign.md`](TechnicalDesign.md) §9 lists six figures that
are owed and that cannot be obtained until there is code, three of them at M0. And three of the eighteen
answers above are **confirmations to be checked against hardware rather than choices already validated**:
whether 192 pixels is the right selection circle (Q8, M1), whether the text reads acceptably (Q18, M1),
and whether the safe zone is too safe (Q16, M3).

A design with no open questions and no measurements behind it is a design that has not been wrong yet.
That is a different thing from a design that is right.

**Four of the ten ADRs are still marked Proposed** — [`ADR-002`](ADR/ADR-002-tick-and-numbers.md),
[`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md),
[`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md) and
[`ADR-005`](ADR/ADR-005-meshes-are-generated-in-code.md) — and the ADR index says plainly that a Proposed
decision is not something to write code against. **That is now the only thing between this design and
being settled.**

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
