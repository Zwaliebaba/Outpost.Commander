# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

**Thirty-one answered, five open.** Eight came from an adversarial review that also reversed two earlier
answers and corrected three statements that were wrong. **All five open questions are under *Open* below**,
each with the milestone that settles it. **Every one carries a recommendation**, which none of Q26, Q33
and Q34 did before: three are measurements with a starting value to be moved from (Q26, Q34, Q37) and
three are decisions somebody has to take (Q33, Q35, Q36).

---

## Answered — 2026-09-20

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q1** | Is the playfield a volume, a plane, or a plane with altitude bands? | **A plane.** A tap is a ray and a ray has no depth, and R21 leaves no second input to supply one. | [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) |
| **Q2** | Is the home base a fixed station or a mobile mothership? | **A fixed station.** Removes base pathfinding, docking and a class of AI problem from the MVP; a mothership is a hull like any other later. | `GameDesign.md` §5 |
| **Q3** | How much of the *Warzone 2100* design system lands in the MVP? | **The model, not the interface.** Hull, drive and slots in the simulation and on the wire from the first line; three fixed designs and no designer screen. | [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) |
| **Q4** | What fleet scale should the MVP target? | **About fifty ships a player** — with the station and four modules that is **110 entities in the reduced MVP** (Q27, Q28) and 220 at four players, and 110 is what makes a snapshot fit one datagram, with nine entities of headroom. | `GameDesign.md` §10, [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) |
| **Q5** | How does a client find a host? | **It does not — the address is configuration.** A one-line file in `LocalState` with `127.0.0.1` compiled in as the default; no discovery, no address entry. The loopback exemption this implies is a development arrangement and never a shipping one. | [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md) |
| **Q6** | What is the target device? | **The Surface Pro.** 13-inch 3:2, 2880 × 1920 at 267 PPI, 200% scale, 1440 × 960 DIPs. The current model is ARM64, which makes that CI leg a real target. | `Interface.md` §1, [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) |
| **Q7** | Is the authored frame 16:9 or 3:2? | **3:2** — it follows from Q6 rather than being a separate choice, and there is no letterbox on the target panel at any scale. The 16:9 recommendation this register carried was wrong once the device was known. **The resolution behind it moved**: [`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md) makes the world a scale of the panel defaulting to 1:1, and leaves the interface's authored 1440 × 960 alone as a statement about fingertips. | [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md), [`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md) |

## Answered — 2026-09-20, second round

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q8** | How does a player select more than one ship? | **A hold on a ship takes every ship of the same design within a circle centered on it.** Screen-space, 192 authored pixels, drawn while the finger is down, so the camera's zoom is the group-size control. **No band select**, which leaves one-finger drag unconditionally panning. The owner's answer; it beat all four options on the ballot, including the draft's hold-then-drag. | [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md), `Interface.md` §3–§4 |
| **Q9** | What happens when a human disconnects mid-match? | **The slot persists, ships hold position, and they may reconnect.** Self-contained snapshots make reconnection free. An AI taking the slot is the better answer and waits for M4. | `GameDesign.md` §2 |
| **Q10** | Does the station have a weapon? | **Yes — two short-range `PointDefense` mounts.** It kills a loiterer, not a besieger and not a fleet: `PointDefense` reaches 400 and a `MassDriver` reaches 600, so a fighter can stand off and shell the station untouched. Deliberate — with the heavy design cut, a station whose defense outranged the fighter would be unkillable. | `GameDesign.md` §5, §6, §7 |
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
| **Q16** | Does the station's point defense cover the home asteroid field? | **No — the station only.** Miners at the field stay raidable, so early fighters have a job and there is a real choice between escorting and expanding. Retreating to the safe zone costs a cargo run, which is the exchange the point defense exists to create. Covering the field would make the opening safe and the early game pure build-up. | `GameDesign.md` §5 |
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
| **Q22** | Is asteroid ore replicated, and at what cost? | **Not at all before M3**, since inexhaustible asteroids have no simulation state and the client derives their positions from the seed. From M3, sparsely: only asteroids whose quantized ore bucket changed, at most one per active miner. The budget had excluded a thing R23 said must be replicated. | `GameDesign.md` §4, `TechnicalDesign.md` §4 |
| **Q23** | Is the client forced fullscreen? | **Yes, at launch.** Nothing previously forced it, which made [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md)'s exact 2× an accident of however the window happened to be sized. | `Interface.md` §1, `TechnicalDesign.md` §5 |
| **Q24** | What does the host validate on a command? | **Ownership, selection length, generation, target bounds and sequence wraparound** — labeled correctness rather than security, so it is not filed under the anti-cheat exclusion again. A 1,232-byte packet holds 610 identities against a peak of 110, reachable from an ordinary bug with no attacker anywhere. | `TechnicalDesign.md` §4, §8 |
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

## Answered — 2026-09-21, sixth round: the mining cycle

| | Question | Answer |
|---|---|---|
| **Q32** | Is mining a standing order, and where does cargo capacity live? | **Yes, and in the mining tool.** A mine order is the only order in this design that does not complete — the miner shuttles until told otherwise, five states on the tick. Capacity and extraction rate are **derived and summed over the hull's slots** like mass and cost (R24), so a two-slot heavy miner is a table row rather than a mechanic. The unload target is **a query for the nearest owned thing that accepts ore**, written as a set although it holds one station today, because a mining factory at a contested field is only worth adding if the miner already asks. |

**The wire could not afford a cargo byte.** A fill level per entity is 110 bytes against 96 of headroom,
which would have split the MVP's single datagram. Cargo is **two bits in the flags byte** — four buckets,
which is what a bar needs — in the space the design identity vacated when Q28's review moved it to a byte
of its own. It is a small illustration of the thing ADR-015 warned about: the datagram now has nine
entities of headroom, and the next feature that wants a per-entity byte still cannot have one.

**One sentence was corrected while doing this.** `GameDesign.md` §4 said a home field "holds enough for a
long opening and not for a match" in a paragraph that reads as current — true from M3, false before it,
since asteroids are inexhaustible until then. **A consistency checker cannot catch that**: it is a claim
that is true in one milestone and false in another, not a figure that disagrees with itself.

---

## Open

### Q26 — What is the asteroid count, and the spawn anchor radius? — **needed by M2**

Neither appears anywhere in this design, and both are inputs to things that do. The anchor radius sets how
long a strike force takes to cross the map, which is half of the raid arithmetic in `GameDesign.md` §7. The
asteroid count sets the sparse ore budget from M3 (Q22) and how much a home field is worth holding.

**This row carried no recommendation, deliberately**, on the grounds that both follow from playing and
guessing them adds two more unmeasured numbers to a document that has plenty. That reasoning stands and the
numbers below do not displace it — but M2.0 cannot start on nothing, and **a starting value with its
derivation can be measured against, while a blank cannot.** Both are arithmetic on this design's own
figures, not observations, and both are expected to move.

**Recommendation: an anchor radius of 6,000 units.** Two opposed stations then sit **12,000 apart**, which
at the Fighter's 140 u/s is **86 seconds** — inside `GameDesign.md` §7's stated 80-to-100-second crossing,
which is the arithmetic the raid balance rests on. It leaves comfortable margin to the play area's edge for
the camera clamp.

**Watch what that radius does at four players, because it is not obvious.** Anchors at 90° on a 6,000
radius put *opposed* players 12,000 apart and **adjacent** ones 8,485 — **60 seconds, not 86.** Raids get
materially shorter the moment the third and fourth slots ship (`GameDesign.md` §2), so the radius may have
to be a function of the player count rather than a constant. That is a consequence to measure at M4, not a
reason to move the number now.

**Recommendation: ten asteroids per home field, 200 ore each.** The design puts income at about 15 credits
a second and one miner at 2.5, so a running economy is roughly six miners; ten rocks keeps them from
queuing on one. Two thousand ore to a field then covers a long opening without covering a match, which is
exactly what `GameDesign.md` §4 asks of a home field once M3 makes asteroids finite.

**What is required either way** is that the generator *names* both rather than leaving them implicit in
code.

### Q33 — Which side do the panels belong on? — **needed by M1**

Asked in the sixth round and not answered there. **The posture assumption underneath it was wrong**: a
Surface Pro 11 is 287 × 208 mm and 895 g, so it is used on a kickstand with index fingers rather than held
at its sides with the thumbs reaching the bottom corners (`Interface.md` §1). **Reach therefore stops being
the binding constraint and occlusion starts** — a reaching hand covers its target and a wedge of screen
around it, on the side of the dominant hand.

The bottom edge is still right and it is *which corner* that is open. The build panel belongs under the
reaching hand and the selection panel opposite it, because the selection panel is the readout the player
reads while their hand is on the glass (`Interface.md` §6); which hand that is, is handedness.

**Recommendation: ship right-handed as the default, make it a setting, and close this row.** The premise
that it waits on M1 is weaker than it looks, because **the cost of making it a preference has already been
paid**: `design_handoff_hud/geometry.json` carries the mirrored x for every affected rect, the flip is the
single reflection `x' = 1440 - x - w`, and `Scripts/CheckHudGeometry.py` asserts the tiers and the clear
space in *both* states. Once the mirror exists and is gated, "which side" is answered by whichever the
player picks, and the only decision left is the default — which is right-handed, because most hands are.

What playing at M1 can still tell us is whether the *occlusion* model behind §1 is right at all, which is a
different question from which corner. That one is worth keeping; this one is not.

### Q34 — Does the client target 60 Hz or the panel's 120? — **needed by M0**

Asked in the sixth round, and **decided by M0.23's frame times** rather than by argument. The panel is
120 Hz VRR, and touch responsiveness is bounded by frame time more than by anything else: 8.3 ms against
16.7 is perceptible on a direct-manipulation gesture like panning, which is the gesture this game is driven
by.

**It is not free.** It halves the frame budget, and it compounds with
[`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)'s 1:1 default — 120 Hz at 1:1 is four times the
pixel rate of 60 Hz at a 0.5 scale.

**Recommendation: 60 Hz for the MVP, and treat 120 as a measured upgrade rather than a goal.** The
intuition above does not survive the arithmetic. Tap-to-visible is **152 ms average** (`TechnicalDesign.md`
§4), of which frame time is 16.7 — about **11%**. Halving it saves 8.4 ms, or **5.5% of the number that
decides how the game feels**, against a 75 ms interpolation delay and a 20 Hz snapshot cadence that
dominate it. That is not perceptible, and it costs half the frame budget to buy.

**The honest counter is panning, and it should decide this rather than the order latency.** A pan is
client-side and not network-gated, so it is the one interaction where 8.3 ms against 16.7 is the whole
story rather than 11% of it. **So M0.23 should time the two separately** — order latency and camera
pan — because the first says 60 is plenty and only the second can argue for 120. The two are one constant
apart, so this stays cheap to reverse.

### Q35 — Does canceling *or replacing* a build refund, and how much? — **needed by M1**

**The plan asked for this row by name.** M1.6's exit criterion reads *"canceling refunds what the design
says it refunds — and if the design does not say, that is a register question rather than a guess in a
commit"*, and the design does not say: `GameDesign.md` §5 states that credits are deducted when the item
starts and says nothing about getting them back, and `Interface.md` §6 makes the current item tappable to
cancel without saying what a cancel costs.

- **Full refund.** A cancel is free, so a mis-hit on a 96 × 96 button costs only the build time already
  spent. It also makes the station a place to park credits at no risk, which matters only if something
  later charges for holding them — nothing in the MVP does.
- **No refund.** A cancel becomes a real decision, and it punishes the mis-tap that `Interface.md` §1's
  *under fire* tier exists to prevent, on a target sized for a player who is being shot at.
- **Proportional to the progress remaining.** The obvious middle, and the expensive one: the deduction was
  taken whole and the refund is a fraction of it, so it needs a rounding rule that is exact on both sides
  (R16), in the one part of the tick where that is least forgiving.

**The design handoff found the sharper case, and it is not the cancel.** `design_handoff_hud/` keeps
**all six build buttons live while something is building**, so a tap on any of them replaces the item in
progress — and since credits are spent at the start, the displaced item's credits are simply gone. That
is the common path, not the rare one: the cancel target has to be aimed at, while a replacement is one
tap on a 96-pixel button a player is already using. The handoff raises it and declines to answer it,
correctly — it is a `GameCore` rule, not an interface one.

**Recommendation: full refund, on both paths.** The MVP has one queue slot and no way to queue ahead
([`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md), Q21), so the only thing a cancel can express
is *I picked the wrong one* — which is the mis-tap case, not a strategic one. A proportional refund buys a
decision the MVP gives the player no other way to make interesting, and pays a rounding rule for it.

### Q36 — Does the income rate ship at all, and if so how and over what window? — **needed by M2**

**This question grew a first half.** `design_handoff_hud/` draws the credits panel — 272 × 88, a label, the
balance and a change flash — with **no room for a rate**, and states flatly that there is none because it
*has no data path*. **That reason is false**, and it is false for a traceable cause: the brief the handoff
was generated from asserted it, so the handoff inherited an error rather than finding one. The conclusion
may still be right — a rate the player does not need is a readout that costs panel space over the
battlefield — but it has to be decided rather than absorbed. **Whether comes before how.**

`Interface.md` §6 puts an income rate in the credits panel *"once there is one"*, and M2.7 is the milestone
in which there is one. **The snapshot has no income field** — the per-player block carries credits, the
last applied command sequence, and the current build item with its progress
(`TechnicalDesign.md` §4) — so the number is derived on the client, and M2.7 constrains the derivation
without choosing it: *"a client that derives income from the catalog is a client doing simulation"* (R19).

Two derivations fit the data the client has, and they fail differently:

- **Difference the credits and subtract the spend the client can already see.** Credits are deducted when
  an item starts (`GameDesign.md` §5) and the client watches that item appear in the same per-player block,
  so the deduction is observable rather than inferred. It evaluates no economy rule, which is the cleanest
  reading of R19. It needs the item's cost, which is the same catalog read the build panel already makes —
  **and it is wrong by exactly whatever a cancel refunds, which is Q35.**
- **Count deliveries and convert.** This is what M2.7's *observed deliveries* names, and a delivery is
  visible in the flags byte's cargo buckets going from full to empty. Turning it into credits needs the ore
  processor's multiplier out of the catalog — an economy rule evaluated on the client, which is the thing
  M2.7's own sentence warns against.

**The window is a number nobody has written down.** A one-slot miner fills in five seconds and round-trips
in roughly thirty (`GameDesign.md` §4), so with the two or three miners of an opening a delivery is a rare
discrete event: an instantaneous rate reads as zero most of the time and as a spike for one frame.

**Recommendation: no numeric rate in the MVP — which is the handoff's conclusion, reached by a route that
holds.** This row first recommended shipping it and differencing the credits. That reversed on noticing what
the handoff's credits panel already carries: **a 120 × 3 change flash, cyan on a gain and amber on a spend,
decaying exponentially.** It delivers the signal a player actually acts on — money is arriving, money just
left — with no number, no window constant, no dependency on Q35 and no derivation to argue about. A rate is
**instrumentation for the designer more than a control for the player**, who can count their miners on the
map; and every pixel of that panel is battlefield they cannot see.

**Revisit it after twenty matches** (`GameDesign.md` §10), which is the mechanism this MVP exists to
provide. If the flash turns out not to answer "is my economy growing", the analysis above is the answer to
*how*: **difference the credits over a window stated in ticks rather than seconds** (R16 — the tick is the
clock), settling Q35 first because that derivation is wrong by whatever a cancel or a replacement gives
back. It evaluates no rule the host also evaluates, so it cannot disagree with the host.

**What must not happen is shipping the omission for the handoff's stated reason**, which is false. "No data
path" would make this a constraint; it is a choice, and it should be re-openable on evidence.

### Q37 — How big is each hull, in world units? — **needed by M1.9**

**No hull has a size anywhere in this design.** `GameDesign.md` §6's table gives a *size class* — Small,
Medium, Large — and that is an axis of the damage table (§7) rather than a dimension: it says what a
`MassDriver` does to a small hull and nothing about how long a `Scout` is. **Three things need the number
and none of them can be written without it:**

- **[`ADR-005`](ADR/ADR-005-meshes-are-generated-in-code.md)'s mesh function** (M1.9), which emits a hull
  from parameters and has no absolute scale to hang them on.
- **Q19's ring slot assignment**, which spaces fifty ships around a point without knowing how wide any of
  them is.
- **M2.10's placement validity**, which asks whether a module site is "clear of the station, clear of every
  existing module" — a question about footprints, asked of a design that states none.

**The figures that *are* stated give a derivation**, and it is arithmetic on them rather than an
observation:

- Fully zoomed out the camera sits at **22,500 units** with a 40° field of view, showing about **24,600
  units of width** across the 1,440-pixel authored frame (`Interface.md` §5) — **17.1 units per authored
  pixel**.
- `Interface.md` §1 justifies the 24-pixel pick radius by keeping **"a 4-pixel ship at tactical zoom"**
  hittable, which puts a small hull at about **68 units**.
- At the close end the camera is about **1,500 units** out, showing roughly 1,638 units of width — so that
  same hull is about **60 authored pixels** across, which is the size it is actually looked at.
- For scale against what is fixed: a move-order marker draws a **120-unit** square on the plane, modules
  are placed within **400 units** of the station, and point defense reaches 400 against a mass driver's 600.

**Recommendation, to be moved by looking at it on the device at M2.13:**

| | Longest dimension |
|---|---|
| `Scout` | **60** |
| `Frigate` | **90** |
| `ModuleFrame` | **90** |
| `Station` | **220** |
| Asteroid | **60–180**, varied by the match PRNG |

**One of these is a packing constraint rather than a free choice.** Four modules must sit inside 400 units
of the station, clear of it and of each other (`GameDesign.md` §5), so the station's footprint and the
module's are solved together or M2.10 refuses placements it should allow. A 220-unit station and 90-unit
modules leave room; a 300-unit station does not.

**What is required either way** is that the catalog *names* a size per hull, derived like every other stat
(R24), rather than leaving it implicit in whatever the mesh function happens to emit.

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
