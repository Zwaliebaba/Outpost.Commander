# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

**Fifteen answered, three open**, and nothing blocks M0.

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

## Open

**Everything the design raised has been answered.** What is left are three questions the answers
themselves created, and none of them blocks a milestone before M1.

### Q16 — What are the station's point-defence numbers? — **needed by M3**

`GameDesign.md` gives 60 damage per second per mount at 400 units, two mounts, and a damage row of
120/90/30 against small, medium and large. Those are starting values in exactly the sense §1 of that
document means. **The thing to watch is whether the safe zone is too safe** — a station that shrugs off
raiding entirely removes the pressure that makes the early game a game. Same class as Q14 and answered
the same way, by playing M3.

### Q17 — What does `Holding` over empty space mean? — no milestone

It is the one gesture left over after [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md), and it
is unassigned rather than filled. **A map ping is the obvious candidate** and was declined at Q13 because
the MVP has nobody to ping. Leaving it empty is deliberate: a gesture spent on something marginal is a
gesture unavailable when something real needs it, and R21 hands out only three.

### Q18 — Does pixel-doubled text look acceptable on the device? — **needed by M1**

The scene target is 1440 × 960 and the fit is an exact 2×, so a glyph rasterised at 24 authored pixels
reaches the glass as 2 × 2 blocks. R13 anticipates this and names dense small type as where it hurts;
this interface is large type with a dozen strings, and a doubled pixel at 267 PPI is a 133-PPI effective
pixel, which is ordinary desktop density. **The argument says it is fine and only a screen can confirm
it.** If it disappoints, the lever is the authored resolution rather than the text path — authoring at
2880 × 1920 makes the scale 1:1 at the cost of a 5.5-megapixel scene target and multisampling four times
as expensive, which would supersede [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) rather
than [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md).

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
