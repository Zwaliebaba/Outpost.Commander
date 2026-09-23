# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

**Forty-two answered, six open.** Eight came from an adversarial review that also reversed two earlier
answers and corrected three statements that were wrong, one — Q38 — came from writing the code rather than
from reading the design, and **seven — Q39 to Q45 — came from integrating the mesh handoff**, which is the
first time a body of authored content met this design and asked it questions. Those seven were registered
with recommendations and answered the same day; the eighth round below is what they became.

**THE *OPEN* SECTION HOLDS TEN ENTRIES AND FOUR OF THEM ARE ANSWERED** — Q26, Q35, Q37 and Q46, all in
full — kept in place with their reasoning rather than flattened into a table row, because what each
was weighing is worth more than the row would be. Their headings say so. **The six that are genuinely
open are Q33, Q34, Q36, Q47, Q48 and Q49**, each with the milestone that settles it, and **every one carries a
recommendation**, which none of Q26, Q33 and Q34 did before.

**Q46 was asked and answered in one motion, on the owner's instruction**, which is worth marking
because it is not the pattern: it was registered with its recommendation, the owner ruled "proceed", and
M1.2 was built to it in the same change. The register keeps the question and the reasoning either way, so
that the figures have somewhere to be argued with later.

**Q37 is answerable now and is left open deliberately.** It asks how big each hull is; the handoff
delivers thirteen meshes whose extents match its recommendation almost exactly — `Scout` 60, `Frigate` 90,
`Station` 220, asteroids 62 to 167 against a recommended 60–180. **Closing it wants the catalog row
written at the same time** (R24), so that the figure and the file are two statements of one number with a
script between them rather than one number nobody stated. M1.9 carries that as a step.

**This line said "five open" while six were listed**, from the commit that registered Q37 and did not
count again. It is the same defect `Plan/README.md`'s step counts had and for the same reason — a count is
prose beside a list, and nothing reads the list. `Scripts/CheckDesign.py` recomputes the plan's counts and
does not yet recompute these.

---

## Answered — 2026-09-20

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q1** | Is the playfield a volume, a plane, or a plane with altitude bands? | **A plane.** A tap is a ray and a ray has no depth, and R21 leaves no second input to supply one. | [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) |
| **Q2** | Is the home base a fixed station or a mobile mothership? | **A fixed station.** Removes base pathfinding, docking and a class of AI problem from the MVP; a mothership is a hull like any other later. | `GameDesign.md` §5 |
| **Q3** | How much of the *Warzone 2100* design system lands in the MVP? | **The model, not the interface.** Hull, drive and slots in the simulation and on the wire from the first line; three fixed designs and no designer screen. | [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) |
| **Q4** | What fleet scale should the MVP target? | **About fifty ships a player** — with the station and four modules that is **110 entities in the reduced MVP** (Q27, Q28) and 220 at four players, and 110 is what makes a snapshot fit one datagram, with nine entities of headroom. | `GameDesign.md` §10, [`ADR-003`](ADR/ADR-003-the-record-and-the-command.md) |
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
| **Q24** | What does the host validate on a command? | **Ownership, selection length, generation, target bounds and sequence wraparound** — labeled correctness rather than security, so it is not filed under the anti-cheat exclusion again. A 1,232-byte packet holds 608 identities against a peak of 110, reachable from an ordinary bug with no attacker anywhere. | `TechnicalDesign.md` §4, §8 |
| **Q25** | How does a match end, and how do you start another? | **The host reseeds and restarts; the client shows a result overlay and reconnects.** Nothing said what happened at victory, and restart is the most-used operation in a solo testing loop — twenty matches an evening is impossible if playing again means relaunching a package. | `Interface.md` §7, `GameDesign.md` §10 |
| **Q27** | Does the MVP ship two players or four? | **Two, through M3.** The generator's symmetry, the AI count and the snapshot's per-player blocks are all sized by a runtime player count, so the other two slots are configuration rather than a change. Two buys a five-minute match, a single-datagram snapshot, and twenty matches an evening. | `GameDesign.md` §2, §10, [`ADR-003`](ADR/ADR-003-the-record-and-the-command.md) |

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

## Answered — 2026-09-22, seventh round: implementing the seam

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q38** | Does the rotation deadzone rebase at its crossing, the way the tap slop does? | **Yes, it rebases.** The heading follows the fingers from the point the eight degrees was passed, so engaging rotation moves nothing — exactly the rule `Interface.md` §3 already states for the pan. **The argument that settles it is what the deadzone is for**: a pan and a pinch rotate by accident, so the crossing is usually reached unintentionally, and passing the whole cumulative through would snap the world eight degrees in the middle of a pan — making the accident the deadzone exists to absorb worse rather than better. | `Interface.md` §5 |

**What it costs is about eight degrees of every deliberate orbit**, against the fifty a kickstand grip has
before a re-grip ([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)) — roughly a sixth of a
gesture. That is the same price §3 pays for the pan and it is paid for the same reason, but it is worth
writing down, because **it is one more thing on the bill if orbit is cut at M1.16**: ADR-018 already names
orbit the candidate and counts three constants that exist only to protect it.

**A fourth constant was declined.** Ramping rotation in over the first few degrees past the threshold
would cost neither the lurch nor the arc — and it would put the camera in a band where the ground turns at
some fraction of the fingers, which is the "slides at a different rate from the finger" complaint ADR-018
opens by rejecting rate-based cameras over.

**This is the first row the register took from writing code rather than from reviewing design**, and it is
worth noting how it was found: two paragraphs of `Interface.md` described the same kind of threshold and
only one of them said what happens at the crossing. Nothing compares two paragraphs for that.

---

## Answered — 2026-09-22, eighth round: the mesh handoff

**Seven, and they are the first questions a body of authored content asked this design.** Every one came
from integrating `Design/design_handoff_meshes/` rather than from reading the documents: three are numbers
the design never stated and the content needed, three are decisions about what the client draws, and one
is a specification that was approved without a home.

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q39** | What are the near and far clip planes? | **Near 50, far 50,000.** The near plane is what sets depth precision, not the far one: at 24 bits, near 1 resolves ~30 units at the 22,500-unit tactical camera and the station's 12-unit plate step z-fights away; near 50 resolves ~0.6 units and is still far closer than the camera ever gets to a hull. Reversed-Z is the lever if it is ever not enough. | `Interface.md` §5 |
| **Q40** | Combat camera at 1,400 units or 1,500? | **1,400.** A 60-unit hull is 57 authored pixels at 1,400 and 53 at 1,500 — both inside the 53–79 the handoff's combat plate was accepted against, so it is not visible. This document and [`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md) both already said 1,400 and `GameClient/Camera.h` was built to it; **the handoff's README is corrected rather than the design**, which is one document instead of three. | `Interface.md` §5 |
| **Q41** | Where do weapons attach to a hull? | **The hull's origin, for the MVP.** Wrong by up to half a hull length for one frame of a tracer during an explosion. Not inferred from the vertices — the arrays are face-split with no groups to find a recess by, and a re-modeled hull would move every muzzle silently. Safe to defer because [`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md)'s event names entities and not positions. Authored transforms are requested when a tracer is drawn. | `TechnicalDesign.md` §7 |
| **Q42** | What does an asteroid's ore state look like? | **Nothing — the number in the selection panel and nowhere else.** The question a player is asking is answered by that number, and four buckets over five variants is twenty combinations to author for one line of text. The cheap version if it is ever wanted is the vertex-color hull tone, which costs no material and no second draw. | `Interface.md` §7 |
| **Q43** | May a mesh be authored off-origin? | **Yes, and what is asserted is the extent rather than the centroid.** Three of the thirteen are deliberately asymmetric and the station's centroid sits at Y ≈ +6, so a `min == -max` check — the one that looks obviously missing — fails all three. `Scripts/CheckMeshes.py` compares min and max per axis against the manifest instead. | `TechnicalDesign.md` §7 |
| **Q44** | Repack CMO's 52-byte vertex to 28? | **No — and M1.9 found the premise had moved.** The decision refused to pay for a load-time transform and a second layout; the **handedness conversion forces a load-time rebuild anyway** (the handoff is Y-up and the camera is Z-up), so once that array is being built the narrow vertex is free. The client uploads **32 bytes**. **The real finding stands and all three figures are now measured**: **430 KiB** on disk, **52 KiB** deflated in the appx, **266 KiB** in VRAM for all thirteen and **85 KiB** for the three M1.9 ships. | `TechnicalDesign.md` §7 |
| **Q45** | Does the shape-coded overlay ship? | **Not in the MVP, and the dependency is recorded rather than the decision alone.** The hulls were tuned for the combat view instead of being bent into silhouettes legible at 3.5 px *because* the overlay was expected to carry tactical-zoom identification. Cutting it silently reopens that trade, and the answer would be re-authoring thirteen meshes. | `Interface.md` §7 |

---

## Open

### Q26 — What is the asteroid count, and the spawn anchor radius? — **answered, provisionally, 2026-09-23**

**THE ANCHOR RADIUS IS ANSWERED: 6,000 units, on the owner's instruction, 2026-09-22.** M1.5 could not
place a station without one. It is written into `GameDesign.md` §3 and into `GameCore/Layout.h`, and
`GameCoreTests` pins both crossings against §7's window rather than restating them.

**THE ASTEROID COUNT IS ANSWERED: ten per home field and two contested clusters of six per player's
region, on the owner's instruction, 2026-09-23.** That is 22 rocks a region and 44 on a two-player map
once M2.2 copies it. The home field is the recommendation below. The contested half had no
recommendation and was chosen over two alternatives, 10 home with contested fields deferred and one
cluster of eight, because it builds both kinds of field the design names and lets M2.2's symmetry test
cover both. Every figure the generator needs is a named constant in `GameCore/Generator.h` and
`GameCoreTests` checks each one by name. That includes the four the register never asked about: the
600-unit inner edge of a home field, which keeps rocks off the 400-unit module ring, the 1,500-to-3,500
band where a contested cluster may sit, its 600-unit spread, and 150 units between rocks. **All of it
is provisional**, and M3's twenty matches are where it is expected to move. The reasoning below is why
these were the starting values.

Neither appeared anywhere in this design, and both are inputs to things that do. The anchor radius sets how
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

### Q35 — Does canceling *or replacing* a build refund, and how much? — **ANSWERED**

**FULL REFUND, ON BOTH PATHS. The owner's answer, 2026-09-22**, and M1.6 is built to it: a cancel and a
replacement both give back exactly what was taken, from the amount recorded on the item rather than
recomputed from the catalog. `GameLogicTests` pins both paths, including the one that matters — replacing
a design with itself, which only works because the refund happens before the new cost is checked.

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
([`ADR-003`](ADR/ADR-003-the-record-and-the-command.md), Q21), so the only thing a cancel can express
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

### Q37 — How big is each hull, in world units? — **ANSWERED**

**THE DELIVERED EXTENTS, WRITTEN INTO THE CATALOG. The owner's answer, 2026-09-22.** `HullEntry` now
carries a `sizeUnits` row — `Scout` 60, `Frigate` 90, `Station` 220, `ModuleFrame` 84 — taken from the
thirteen delivered meshes and **rounded up, because the figure is a bound**: it is what spaces things so
they do not overlap and what sets how far in front of a station a new ship appears. **The `Cruiser`'s 150
is the one row no file backs**, because nothing authored a mesh for a design the MVP cut; M4 authors to
the number rather than the other way round. M1.9 adds the script that compares the two statements.

It landed at M1.6 rather than M1.9 because the build system needed a spawn offset and deriving one from
two hull sizes beat inventing a distance.



**No hull has a size anywhere in this design.** `GameDesign.md` §6's table gives a *size class* — Small,
Medium, Large — and that is an axis of the damage table (§7) rather than a dimension: it says what a
`MassDriver` does to a small hull and nothing about how long a `Scout` is. **Three things need the number
and none of them can be written without it:**

- **[`ADR-005`](ADR/ADR-005-a-mesh-is-a-cmo-file.md)'s authored meshes** (M1.9). A mesh is a CMO file
  and **is not scaled at draw time**, so a hull's authored extent *is* its size — the number has to be
  agreed before anything is modeled, and it is then a figure a script can check the file against rather
  than a parameter somebody passes.
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
(R24), rather than leaving it implicit in whatever the mesh happens to contain. Under
[`ADR-005`](ADR/ADR-005-a-mesh-is-a-cmo-file.md) that is stronger than it was: a mesh is not scaled at
draw time, so the catalog's number and the file's extent are two statements of one figure and **a script
compares them** rather than a reader trusting both.

---

### Q47 — How fast does a station build? — **needed by M1.6, and M1.6 is built to the recommendation**

**THE DESIGN STATES NO BUILD TIME ANYWHERE.** `GameCore/DerivedStats.h` said so in as many words from
M1.2: [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) names build time alongside cost and mass as a
derived stat, `GameDesign.md` §5 has a shipyard multiplying a build *rate*, and no figure for that rate
exists. M1.6 is the step that first needs one.

**THIS ROW IS OUT OF ORDER AND THAT IS STATED RATHER THAN HIDDEN.** The register's rule is that a task
needing an answer this design does not give **asks and gets it written in before the code is**. M1.6 was
built to the recommendation below instead, because it is one constant
(`BuildSystem::BUILD_RATE_CREDITS_PER_SECOND`) and stopping seven implementation steps on a balance number
would have cost more than changing it will. **The figure is therefore NOT in `GameDesign.md`** — it lives
in the code and on this row until the owner rules, which is what keeps the design document honest.

- **A rate in credits of cost a second**, so build time falls out of cost the way mass and speed already
  fall out of components (R24). A per-design time would be a second table to keep in step with the first.
- **A flat time per design**, which is simpler and severs the relationship the shipyard multiplier acts on.
- **A rate that varies by hull size class**, which is a third mechanic for the MVP to tune.

**Recommendation: a rate, at 20 credits of cost a second.** The derivation is the opening. A station
starts with **1,000 credits** (§4) and a running economy is about **six miners** at 150 each; at twenty a
second that bank takes **45 seconds** to spend against a match of five minutes, so the first minute is
spending what you started with and after that income paces you — which is the shape §4 describes. At ten
it would be 90 seconds with credits piling up unspent, a third of the match spent waiting.

**It sits just above the income rate on purpose.** Income is about 15 credits a second, so building is
very slightly faster than earning — which is what leaves §5's shipyard multiplier something to do. Set it
far above and credits always bind and the multiplier is dead; far below and the station is the bottleneck
and mining stops mattering.

**What it produces:** a Miner in 7.5 seconds and a Fighter in 15, both exact divisions at the 20 Hz tick
(150 and 300 ticks), so neither figure depends on a rounding rule. With `ShipyardL1` a Fighter is 10
seconds and with `ShipyardL2` it is 7.5.

### Q46 — What are the catalog's mass, thrust and per-item costs? — **ANSWERED**

**`GameDesign.md` §6 states them as relations and states the outcomes they have to produce.** A hull's
mass is "low", "medium" or "high"; a drive is "balanced thrust, cheap" against "more thrust for more mass
and more cost". No figure for either. What *is* fixed is what the arithmetic must yield:

- **Miner** — `Scout` + `IonDrive` + 1× `MiningLaser` — **150 credits at 100 u/s**
- **Fighter** — `Frigate` + `BurnDrive` + 2× `MassDriver` — **300 credits at 140 u/s**
- **The cut battleship** — `Cruiser` + a drive + 4 weapons — **2,400 credits**, which is the figure §6
  used to cut it and is therefore as binding as the other two

**M1.2 cannot be written without numbers** — speed is thrust over mass and cost is a sum, and neither is
computable from "low" and "medium". **M1.1 shipped the catalog without them** rather than inventing them,
which is why this is here.

**Recommendation.** One set that satisfies all three anchors exactly, in integers, with no rounding
anywhere:

| Hull | Mass | Cost |
|---|---|---|
| `Scout` | **10** | **60** |
| `Frigate` | **20** | **100** |
| `Cruiser` | **60** | **2,080** |
| `Station`, `ModuleFrame` | — | — |

| Drive | Mass | Thrust | Cost |
|---|---|---|---|
| `IonDrive` | **5** | **2,000** | **40** |
| `BurnDrive` | **10** | **5,600** | **80** |

| Slot component | Mass | Cost |
|---|---|---|
| `MiningLaser` | **5** | **50** |
| `MassDriver` | **5** | **60** |
| `PointDefense` | **5** | — |
| The four modules | — | §6's own, already in the catalog |

**What that reproduces, and it is not over-determined — three anchors, and all three land exactly:**

| | Mass | Speed | Cost |
|---|---|---|---|
| Miner | 10 + 5 + 5 = **20** | 2,000 / 20 = **100 u/s** ✓ | 60 + 40 + 50 = **150** ✓ |
| Fighter | 20 + 10 + 2×5 = **40** | 5,600 / 40 = **140 u/s** ✓ | 100 + 80 + 2×60 = **300** ✓ |
| Cut battleship | 60 + 10 + 4×5 = 90 | 5,600 / 90 = 62 u/s | 2,080 + 80 + 4×60 = **2,400** ✓ |

**Every division is exact for the two shipped designs**, which matters under R16: 2,000 over 20 and 5,600
over 40 are whole numbers, so the figure does not depend on a rounding rule.

**And the relations §6 states all hold rather than being asserted.** The `BurnDrive` has more thrust, more
mass and more cost than the `IonDrive`. **An empty `Frigate` does 186 u/s against a loaded one's 140** —
ADR-006's "a cruiser with four plasma cannons is slower than an empty one because of arithmetic, not
because anyone wrote it down", which is now a property a test can assert rather than a claim. And the
`Cruiser` is the slowest thing that moves, which is what makes §7's "does speed counter mass" a question
M4 can actually ask.

**A hull with no drive has no speed rather than a speed of zero from a division.** `Station` and
`ModuleFrame` show a dash for mass in §6 because mass is unobservable without a drive — nothing divides by
it — so the recommendation leaves them at zero and the derivation returns zero before it divides.

**What this recommendation does NOT cover, deliberately: build time.** ADR-006 says "cost and build time
are sums" and **no figure for it exists anywhere in the design**, nor any outcome it has to reproduce —
there is no stated base build rate for the shipyard's ×1.5 to multiply. Inventing one here would be
choosing a balance number nobody can check yet. **M1.6 is the step that first observes it** and is where
it should be asked; M1.2's own exit criteria name cost and speed and not build time.

### Q48 — Is the AI of §8 written so that a bot client can run it too? — **needed by M4.5, and decided before it is written**

**The question.** `GameDesign.md` §8 puts the AI on the host, inside `GameLogic`, on the tick. Written the
obvious way, it reads the host's `World`: exact positions, every entity's full state, everything the host
knows. [`ADR-022`](ADR/ADR-022-a-bot-is-a-headless-client.md)'s bot can never run code like that, because
a client links no simulation (R19). So there would be two decision-makers that can't share a line: the
stress harness's rule-based policy and the real AI. The owner asked whether the bot can simply reuse the
game's bot logic. Today there is none to reuse, and whether there ever is depends on how M4.5 is written.

**The options, and what each costs:**

- **Host-only, as §8 reads now.** The AI takes the `World`. It is the simplest to write, has full
  precision, and M4.5 needs nothing new. The bot keeps its own policy for good. §8's rule that the AI "is
  not given information a human in its position would not have" is kept by discipline, and nothing checks it.
- **Written against what a player sees, and living in `GameCore`.** The AI's input is a view built from
  snapshot-shaped records: the entities a player is sent, at wire precision, plus that player's block. On
  the host the view is built from the host's own encoded snapshot for that player. On a bot it is the
  decoded snapshot. **One decision function then runs in both places**, and §8's no-cheating rule is kept
  by construction, because the function has nothing else to read. It costs three things. The AI works at
  wire precision (positions to a quarter unit, heading to 256 steps, hull as a percentage) even on the host.
  It must obey R16 in full, because on the host it is simulation. And the view has to be cheap enough to
  build each decision interval. M4.6's takeover of an abandoned slot comes nearly free, because the
  function never assumed it had been running from the start.
- **Shared policy primitives only.** Target choice, build choice and threat scoring are shared, and each
  side builds its own loop. Less coupling than the second option and less reuse, and the no-cheating
  property is back to discipline.

**Recommendation: the second.** §8 already demands that the AI see only what a human would, and this is
the only option where the code enforces it. It also makes M4.6's hardest clause, "an AI that can start
from arbitrary mid-match state", a property of the input and not a thing to test for. Wire precision is
enough for a player to play on, so it is enough for an AI that §8 deliberately keeps modest. **Settle it
before M4.5 is written**, because retrofitting it means rewriting the AI's inputs. If it is taken,
`BotPolicy` (ADR-022) is replaced by that function, not grown into it.

### Q49 — What are the accumulator's weights, and is a cap of two updates a tick enough? — **needed by M4.8, built to the recommendation at M1.14c**

**The question.** [`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md) scores each entity's
relevance to each client as an integer sum — a base, a term for being in view, a term for having moved or
changed, a term for being the client's own — and sends up to two whole updates a tick. The weights decide
what a player sees refreshed first when more is on screen than fits, and nothing in the design derives
them: they are the kind of number that is right when a match looks right and not otherwise. **Two things
have to be played, not argued.** Whether a stationary enemy fleet at the edge of a tactical view may
refresh every half second without anyone noticing, and whether a four-player match at the design's fleet
ever wants the second update — the budget says the MVP wants it every tick and 220 entities need it to
refresh everything within two.

**Recommendation, and M1.14c builds to it:** base 1, in view 4, moved or changed 2, own 2, so an entity
in view that moved scores 7 a tick against a stationary one out of view at 1 — a seven-to-one ratio,
which is roughly the sweep-to-refresh ratio at which 1,000 entities in view still refresh within six ticks
at the cap. Cap 2. **Revisit at M4.8**, the first time four people look at a full field, and again the
first time a stress run of ADR-022's harness reports a refresh interval a player would notice. Both
numbers are constants beside the accumulator, so the answer costs an edit and a test.

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
