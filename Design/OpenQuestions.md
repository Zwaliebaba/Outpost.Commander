# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

**Seventy-nine answered, six open** — Q34 and Q49, and four of Q62 to Q77 from the mid-implementation review (Q62 to Q70, Q72, Q75 and Q76 are ruled, and Q77's first item). Q78 was asked and answered at M3.2, and Q79 to Q85 on 2026-09-24. Eight came from an adversarial review that also reversed two earlier
answers and corrected three statements that were wrong, one — Q38 — came from writing the code rather than
from reading the design, and **seven — Q39 to Q45 — came from integrating the mesh handoff**, which is the
first time a body of authored content met this design and asked it questions. Those seven were registered
with recommendations and answered the same day; the eighth round below is what they became.

**SIXTEEN MORE, Q62 TO Q77**, were registered on 2026-09-24 from the mid-implementation review with its
recommended defaults, in their own section below the answered ones. **Q62 to Q70, Q72, Q75 and Q76 are ruled, and Q77's first item; Q71, Q73, Q74 and the rest of Q77 are open.**

**THE *OPEN* SECTION HOLDS TWENTY-TWO ENTRIES AND TWENTY OF THEM ARE ANSWERED** — Q26, Q33, Q35, Q36, Q37,
Q46, Q47, Q48 and Q50 to Q61, all in full — kept in place with their reasoning rather than flattened into a table row, because what each
was weighing is worth more than the row would be. Their headings say so. **The two that are genuinely
open are Q34 and Q49**, each with the milestone that settles it, and **every one carries a
recommendation**, which none of Q26, Q33 and Q34 did before.

**Q46 was asked and answered in one motion, on the owner's instruction**, which is worth marking
because it is not the pattern: it was registered with its recommendation, the owner ruled "proceed", and
M1.2 was built to it in the same change. The register keeps the question and the reasoning either way, so
that the figures have somewhere to be argued with later. **Q50 is the second**, found while writing M2.3
and ruled before its code was, **and Q51 and Q52 the third and fourth**, from M2.6 the same way, **and Q53
the fifth**, from M2.7, **and Q54 to Q57 four more**, from M2.11 and M2.12, ruled together before either step's
code.

**Q37 was left open deliberately until the owner answered it on 2026-09-22**, and this paragraph is the
reason as it stood. It asks how big each hull is; the handoff
delivers thirteen meshes whose extents match its recommendation almost exactly — `Scout` 60, `Frigate` 90,
`Station` 220, asteroids 62 to 167 against a recommended 60–180. **Closing it wants the catalog row
written at the same time** (R24), so that the figure and the file are two statements of one number with a
script between them rather than one number nobody stated. M1.9 carried that as a step, and M2.10b moved the
module frame to 90 (Q37's note).

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
| **Q10** | Does the station have a weapon? | **Yes — two short-range `PointDefense` mounts.** It kills a loiterer, not a besieger and not a fleet: `PointDefense` reaches 480 (400 until Q63) and a `MassDriver` reaches 600, so a fighter can stand off and shell the station untouched. Deliberate — with the heavy design cut, a station whose defense outranged the fighter would be unkillable. | `GameDesign.md` §5, §6, §7 |
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
| **Q19** | How do fifty ships occupy one point? | **A ring slot per ship, assigned at order time, ordered by entity identity.** No separation force and no flocking — those are floating-point-shaped problems in an integer simulation, and a formation system later is this same assignment with a different slot layout. **The ring stands; "no separation" was reversed on 2026-09-23 by Q61**, which has every ship steer around every other. The cost is *induced by* [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md): in a volume ships miss each other in the third dimension, on a plane they stack. It was named once and owned by nobody. | `GameDesign.md` §7, `TechnicalDesign.md` §1, §8 |
| **Q20** | What does the client draw between a tap and confirmation? | **A destination marker and a line, drawn the instant the gesture resolves and cleared when the host acknowledges that sequence.** Pure presentation: R19 forbids the client simulating, not the client drawing what it asked for. Without it there are 152 ms of nothing on a device that has no cursor. | `Interface.md` §4, `TechnicalDesign.md` §6 |
| **Q21** | Does the build queue have a wire record? | **Reversed by Q80 on 2026-09-24: there is a queue, and its count rides the building-design byte's spare bits.** As first answered: **There is no queue — the snapshot carries the item currently building and its progress, two bytes per player.** `Interface.md` had specified a cancellable queue that no wire record could feed; the MVP cuts the queue rather than inventing a format for it. | `Interface.md` §6, `TechnicalDesign.md` §4 |
| **Q22** | Is asteroid ore replicated, and at what cost? | **Not at all before M3**, since inexhaustible asteroids have no simulation state and the client derives their positions from the seed. From M3, sparsely: only asteroids whose quantized ore bucket changed, at most one per active miner. The budget had excluded a thing R23 said must be replicated. | `GameDesign.md` §4, `TechnicalDesign.md` §4 |
| **Q23** | Is the client forced fullscreen? | **Yes, at launch.** Nothing previously forced it, which made [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md)'s exact 2× an accident of however the window happened to be sized. | `Interface.md` §1, `TechnicalDesign.md` §5 |
| **Q24** | What does the host validate on a command? | **Ownership, selection length, generation, target bounds and sequence wraparound** — labeled correctness rather than security, so it is not filed under the anti-cheat exclusion again. A 1,232-byte packet holds 608 identities against a peak of 110, reachable from an ordinary bug with no attacker anywhere. **Amended 2026-09-24 by the owner, after the mid-implementation review (M4):** a dead or reused identity is *skipped* rather than refusing the order, since a retreat tapped during a raid names ships that died a moment earlier; a foreign identity still refuses the whole order; the length bound counts live identities; and **every refusal past the sequence check is acknowledged**, so the client stops resending it. | `TechnicalDesign.md` §4, §8 |
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
which is what a bar needs, **and three bits since M2.7** (Q53), because four chips need five states — in the space the design identity vacated when Q28's review moved it to a byte
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
| **Q40** | Combat camera at 1,400 units or 1,500? | **1,400, and since 2026-09-24, 700**: the owner halved the opening distance and the near end followed it, recorded in `Interface.md` §5. **1,400.** A 60-unit hull is 57 authored pixels at 1,400 and 53 at 1,500 — both inside the 53–79 the handoff's combat plate was accepted against, so it is not visible. This document and [`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md) both already said 1,400 and `GameClient/Camera.h` was built to it; **the handoff's README is corrected rather than the design**, which is one document instead of three. | `Interface.md` §5 |
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
600-unit inner edge of a home field, which keeps rocks off the 400-unit module ring (**1,200 to 2,000 since
Q62**, 2026-09-24, with the measured income table there), the 1,500-to-3,500
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

### Q33 — Which side do the panels belong on? — **ANSWERED**

**A SETTING, RIGHT-HANDED BY DEFAULT. The owner's answer, 2026-09-23**, on the recommendation below: the
build panel sits under the right hand and the selection panel opposite it, and left-handed is the mirror
`Scripts/CheckHudGeometry.py` already gates. **What is not built is the control that flips it**: there is no
settings surface in the interface yet, so until one is designed the value is `LEFT_HANDED` in
`OutpostCommander/App.cpp`, false. **What stays open is not this row's**: whether the occlusion model behind
`Interface.md` §1 is right at all is M1.16's to confirm by playing.


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

**The replacing half is gone since 2026-09-24 (Q80)**: a build order while an item builds joins a queue instead
of replacing it. The cancel's full refund stands, and applies to the newest queued item first.

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

### Q36 — Does the income rate ship at all, and if so how and over what window? — **ANSWERED**

**NO RATE IN THE MVP; THE CHANGE FLASH ONLY. The owner's answer, 2026-09-23**, on the recommendation below,
before M2.7's code. `GameClient/CreditFlash` draws the handoff's flash under the balance, cyan on a gain and
amber on a spend, and nothing derives income. **Revisit after M3's twenty matches**, by the route the last
paragraphs give. The reason is the one below, not the handoff's "no data path", which is false.

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
carries a `sizeUnits` row — `Scout` 60, `Frigate` 90, `Station` 220, `ModuleFrame` 84, **90 since M2.10b** — taken from the
thirteen delivered meshes and **rounded up, because the figure is a bound**: it is what spaces things so
they do not overlap and what sets how far in front of a station a new ship appears. **The `Cruiser`'s 150
is the one row no file backs**, because nothing authored a mesh for a design the MVP cut; M4 authors to
the number rather than the other way round. M1.9 adds the script that compares the two statements.

**`ModuleFrame` moved from 84 to 90 at M2.10b, by this ruling's own rule.** Each module level draws with its
own mesh, and both shipyards are 90 units long where the bare frame is 83.52 — so a hull is bounded by every
mesh that draws it, and `Scripts/CheckMeshes.py` and `HullMeshTests` now take the longest of them. At 84, two
shipyards placed exactly clear would have overlapped by six units. M2.10's clearances follow: 155 from the
station and 90 between modules.

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
  are placed within **400 units** of the station, and point defense reaches 480 (Q63) against a mass driver's 600.

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

### Q47 — How fast does a station build? — **ANSWERED**

**A RATE, AT 20 CREDITS OF COST A SECOND. The owner's answer, 2026-09-23**, on the recommendation below, which
M1.6 was already built to (`BuildSystem::BUILD_RATE_CREDITS_PER_SECOND`). The figure is now in
`GameDesign.md` §5, so the paragraph below that says it is not is the history of why this row was out of
order, not its state. **What would reopen it** is M3's or M4's playtesting finding the first minute wrong,
which is a constant.


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
starts with **1,000 credits** (§4; 500 and a starting fleet since Q84) and a running economy is about **six miners** at 150 each; at twenty a
second that bank takes **45 seconds** to spend against a match of five minutes, so the first minute is
spending what you started with and after that income paces you — which is the shape §4 describes. At ten
it would be 90 seconds with credits piling up unspent, a third of the match spent waiting.

**It sits just above the income rate on purpose.** Income is about 15 credits a second (**measured at 19.8 for
six miners since Q62 moved the home field**, 2026-09-24; it had been far above the slot before that), so building is
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

### Q48 — Is the AI of §8 written so that a bot client can run it too? — **ANSWERED**

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

**Needed by M3.10, not M4.5** (the mid-implementation review, M9, 2026-09-23). M3.10's stub is the first AI
this tree will hold, and written the obvious host-side way it reads order state no client is ever sent and
reacts at tick cadence — so a human's raid outcomes against it would be artifacts of what it can see. Ruled
after the stub is written, the recommendation above is a rewrite of the stub's inputs. The review's default
is this recommendation, taken at M3.10: the stub decides over `RecordOf`'s records for its player plus its
block, once per twenty ticks, through `CommandIntake::Apply`, in the file M4.5 extends, with one defend
reflex and AI seats reserved (Q70).

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION.** The AI sees what a player is sent and nothing else: the
wire records of every live entity and its own player block, at wire precision. It decides once a second and orders
through the same command path a client uses. M3.10's stub is written that way, and M4.5 extends it. **Built
(M3.10)**: `GameLogic/StubAi`'s `AiView` holds the records, the block and the seed-derived field, and nothing else.
AI seats are the last seats, reserved so no client is given one, and `Server --ai N` sets how many.

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
at the cap. Cap 2. **Revisit at M4.8**, the first time a full four-slot field is played, by the owner against three AI (Q79), and again the
first time a stress run of ADR-022's harness reports a refresh interval a player would notice. Both
numbers are constants beside the accumulator, so the answer costs an edit and a test.

### Q50 — How does a client learn the player count its field is derived from? — **ANSWERED**

**A BYTE ON THE JOIN REPLY. The owner's answer, 2026-09-23**, on the recommendation below, asked and ruled
before M2.3's code was written. Recorded in
[`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md)'s amendment and in R23.

**The gap.** R23 reads as though the seed is the generator's one input, and ADR-013 sends the seed and
nothing else about the match. **M2.1 and M2.2 made the count a second input**: `GameDesign.md` §3 copies a
half at two players and a quarter at four, so one seed is two maps. Nothing else on the wire carries the
count, because since ADR-024 an update carries only its recipient's block. It went unnoticed until M2.3
because nothing on the client called the generator until then. The one client that needed the count
before that, the camera's opening recenter, had `2` compiled in.

- **A byte on `JoinReply`**: 22 bytes to 23, protocol version 4 to 5, and ADR-013 amended. It rides the
  one record that already exists to carry what a snapshot cannot, and a rejoin gets it again for free.
- **A count-free field**: always a quarter copied four ways. Nothing on the wire moves, but §3's half at
  two players goes, M2.1's table is repinned, and two home fields on a two-player map belong to nobody.
- **Inferred from the replica store**, from the station owners it has heard of. Nothing on the wire
  moves, but ADR-024's records arrive in priority order over several ticks, so the field could be drawn
  wrong and then redrawn, and at M3 a destroyed station changes the answer.

**Recommendation: the byte.** It is the only option that is right on the first frame and stays right.

### Q51 — How long does a miner unload, and how close must it be? — **ANSWERED**

**50 ORE A SECOND, ONCE TOUCHING. The owner's answer, 2026-09-23**, on the recommendation below, before
M2.6's code. Both figures are named and provisional, like Q26's: `MiningSystem.h`'s `UNLOAD_ORE_PER_SECOND`
and `UnloadTarget.h`'s `UNLOAD_SLACK_UNITS`. They are also in `GameDesign.md` §4.

**The gap.** `GameDesign.md` §4 names five states and "unloading" is one of them, but no figure gives it a
duration or a reach. §7 says point defense protects "the unloading area", so how long a miner sits there is
part of the raid arithmetic rather than a detail.

- **50 ore a second, touching**: two seconds for a one-laser hold, once the hulls touch. Touching is a
  center distance of at most the two sizes' halves (Q37) plus 20 units of slack, 160 for a `Scout` at the
  station. It is a real dwell inside point defense's reach, and small next to a round trip.
- **Instant**: one tick on arrival. Simplest, but the unloading state is one tick long and §7's protected
  unloading area has nothing in it to protect.
- **At the extraction rate**, 20 a second: symmetric, but it doubles the time at the station and cuts §4's
  per-miner income by about a sixth.

**Recommendation: 50 a second, touching.** **What it produces**, arithmetic on the design's figures at 20 Hz:
1,000 thousandths of ore a tick extracted and 2,500 unloaded, so a hold takes 100 ticks to fill and 40 to
empty. `MiningTests` pins a full cycle against a rock 2,000 units out at 794 ticks, plus a turn about at each end since Q59.

### Q52 — How does a mine order name its asteroid? — **ANSWERED**

**BY ITS INDEX IN THE GENERATED FIELD. The owner's answer, 2026-09-23**, on the recommendation below,
before M2.6's code.

**The gap.** A mine order targets a rock, and a rock is not a world entity before M3 (Q22), so there is no
entity identity to put in the command.

- **The field index**: the rock's position in `GenerateField`'s order, carried in `targetX` with `targetY`
  zero. Both sides derive the same rows from the same seed and count (R23, Q50), so the index names the
  same rock on both. The host refuses one past the field's size, and it is the natural key for M3's
  per-asteroid ore.
- **The tapped point**, with the host picking the nearest rock inside a tolerance. It needs no new identity
  scheme, but a quantized point can land nearer the wrong rock in a dense cluster, and it adds a lookup and
  a tolerance to validation.

**Recommendation: the index.** It is exact, it is one bounds check, and it is what M3 needs anyway.

### Q53 — How does a two-bit cargo bucket fill four chips? — **ANSWERED**

**THREE BITS, ZERO TO FOUR CHIPS. The owner's answer, 2026-09-23**, on the recommendation below, before M2.7's
code.

**The gap.** `design_handoff_hud` draws **four** cargo chips per selection group. The wire carried a **two-bit**
bucket, which has four states, so "empty" and "all four lit" could not both be drawn. The handoff also never
said how a group of several miners aggregates.

- **Three bits, 0 to 4 chips**: cargo takes one of the flags byte's three spare bits, for five states. Quarters
  are rounded up, so any ore lights a chip and a full hold lights all four. The record stays twelve bytes and
  every refresh figure in `Scripts/DatagramBudget.py` is unchanged. The field audit's recoverable bits go from
  nine to eight.
- **Two bits, drawn as 0, 2, 3 and 4 chips**: no format change, but one chip is never lit alone and the
  discrete buckets read unevenly.
- **Two bits, three chips**: exact, but it contradicts the handoff's four-chip geometry and the gate that pins
  it.

**Recommendation: three bits.** A group lights **its members' mean, rounded to nearest**, which is how the
hull bar already aggregates. `GameCore/EntityRecord.h`'s `CargoChips` is the mapping, and `UpdateTests` pins it.

### Q54 — How does a level-2 module come about? — **ANSWERED**

**AN UPGRADE IN PLACE, AT THE DIFFERENCE. The owner's answer, 2026-09-23**, on the recommendation below, before
M2.11's code.

**The gap.** ADR-015 says upgrading "replaces `ShipyardL1` with `ShipyardL2`", and M2.11b's dimmed state needs
something that gates an item. Neither says how a player gets an L2.

- **An upgrade in place, paying the difference**: an L2 button upgrades one of your L1 modules of that kind.
  You arm it and tap the L1. It pays 300 for a shipyard and 250 for an ore processor, so an L2 costs §5's
  figure in total. It builds through the station's queue, and L2 is dimmed without an L1.
- **An upgrade in place, at full price**: the same interaction, with 700 or 600 on top of the L1 already paid.
  A shipyard L2 then costs 1,100 in all.
- **An L2 placed fresh**: its own module, anywhere in the radius, at full price, with no prerequisite. It is
  the simplest, but "upgrade" stops meaning anything, M2.11b has nothing to dim, and two shipyards of different
  levels can coexist.

**Recommendation: in place, at the difference.** `GameCore/ModuleSite.h` holds the pairs (`UpgradesTo`) and the
price (`UpgradeCostCredits`). The host refuses a placement naming an L2. The upgrade is the same entity with a
new design.

### Q55 — How does a placement carry both a design and a point? — **ANSWERED**

**A NEW COMMAND TYPE WITH ONE MORE BYTE. The owner's answer, 2026-09-23**, on the recommendation below.

**The gap.** A command's target is four bytes. A `Build` puts its design in `targetX`'s low byte, and a point
needs all four.

- **A new type, one more byte**: `PlaceModule` (6) carries the point at full wire precision in `targetX` and
  `targetY`, plus a ninth fixed byte for the design that only this type carries. An in-place upgrade fits the
  existing layout as `UpgradeModule` (7): the module's identity in the target, and the level in `targetY`'s
  spare high byte. It rides protocol 5, which has not left the branch.
- **Pack the design and an offset**: no codec change. The point is a 1-unit offset from the station in 11
  bits, with 5 bits of design per axis field. It caps designs at 32, which fights R24's growth at M4, and the
  packing is clever rather than obvious.

**Recommendation: the new type.** The upstream is never the constraint: `Scripts/DatagramBudget.py --upstream`
puts a placement at 9 bytes and an upgrade at 8, with no selection. The worst case is unchanged.

### Q56 — Where does the shipyard's build-rate multiplier round? — **ANSWERED**

**TICKS ROUND UP. The owner's answer, 2026-09-23**, on the recommendation below, before M2.12's code.

**The gap.** M2.12 says it "must not invent" where a multiplier rounds, and points at ADR-014. Ships divide
exactly under both levels. A module does not: 400 credits at ×1.5 is 266.67 ticks.

- **Round up**: a shipyard never makes anything faster than its stated rate, so that is 267 ticks.
- **Round down**: today's truncation, 266 ticks, a third of a tick faster than the rate.
- **Round to nearest**: 267 here, but a .5 case picks a direction arbitrarily.

**Recommendation: round up.** The ore processor needs no rule, because cargo is carried in thousandths of ore
per player: 2,500 at 125% is 3,125, exactly. **This is recorded here, and ADR-014 stays reserved for M3's
damage**, which is the rounding question it was held for. `BuildSystem::TicksForCost` is the one division.

### Q57 — What does a tap on one of your own modules do? — **ANSWERED**

**NOTHING, UNLESS AN UPGRADE IS ARMED. The owner's answer, 2026-09-23**, on the recommendation below.

**The gap.** `Interface.md` §4's table has no row for a module, and §1's pick order puts modules in the
station's tier.

- **Nothing, unless upgrading**: picked at the structure tier, so it wins over rocks and hostiles underneath,
  and then it has no verb, except as the target of an armed L2 upgrade. The selection is unchanged.
- **Opens the build panel**: treated like the station. It is consistent with "own structure", but it makes
  modules a second way to open the same panel.

**Recommendation: nothing, unless upgrading.** `Selection::Tap` gives a module no verb, and `ResolvePlacementTap`
turns it into an upgrade when one is armed.

### Q58 — Is a second machine part of how this game is tested? — **ANSWERED**

**NO. The owner's answer, 2026-09-23: everything is tested on one machine, the Surface Pro, and a second
machine will not be used.** This is the answer `Plan/README.md` F5 and M1.15 asked the register for.

**Why it was a question at all.** A packaged client cannot reach a host on the same machine without a
loopback exemption (ADR-008), and a second instance on one machine needed a manifest declaration and a
per-instance session token (M1.15). The other answer was to buy a second device, which was the owner's to
decide. M0 and M1 had four runs that asked for one:

| | What it asked | What happens to it |
|---|---|---|
| M0.5 run 1 | Host and client on two machines over a LAN | **Withdrawn.** ADR-008's second measurement says so, and what would reopen it |
| M0.5 run 4 | Loss and jitter on a real wireless link | **Withdrawn.** ADR-003 and `TechnicalDesign.md` §9.4 say so |
| M0.23 | Tap-to-visible with the host on another machine | **The loopback figure stands as the answer.** ADR-003 says what it leaves out |
| M1.15 | Two clients on one host | **Closed on one machine, 2026-09-23**: two seats on one host through the package's multiple instances, played one foreground window at a time, because only the window in front runs |

**What it costs, stated once.** No figure in this tree has a real network term in it. Tap-to-visible is
loopback. Loss is loopback's, which is zero. Every capacity figure is the host sharing one CPU with
whatever loads it. **What would reopen it** is a second machine arriving, and then ADR-008's LAN run is
the first thing to try.

### Q59 — How does a ship turn? — **ANSWERED**

**IN AN ARC, AT A TURN RATE DERIVED LIKE SPEED. The owner's answer, 2026-09-23**, asked after the M1.16
hand session found ships flying sideways: `GameDesign.md` §6 said "turn rate derives the same way" and
nothing else, so the tick moved a ship straight at its target and never changed its heading.

- **Turn in place, then fly.** Exact arrivals and a visible turn, but it doesn't look like flying.
- **Turn while flying** — the owner's choice. A ship always flies along its heading and steers toward
  the target at its turn rate. It looks most natural, and it needs a rule that stops a ship circling a
  point inside its turning circle.
- **Face the direction of travel instantly.** The cheapest, and it drops the sentence the design already has.

**Built to these, which are the recommendation under the answer.** The turn rate is **234 × thrust ÷ mass
binary-angle units a second** (65,536 is a full turn): a Fighter turns half a circle in a second and a
Miner in 1.4. Because speed and turn rate are both thrust over mass, **every ship turns on the same
radius, about 45 world units**, which is below every hull's size and well inside a ring slot's spacing. A
ship steering toward a point **throttles by the cosine of how far off its heading the point is**, and
stops moving forward beyond a quarter turn. So a target behind it is a turn nearly in place, and no
target can be orbited. All of it is integer, through ADR-002's sine table and a pinned integer bearing.
**What would reopen it** is M3's combat wanting a weapon's arc to depend on heading, which is when
the 234 stops being a matter of looks.

### Q60 — Do ships avoid stations? — **ANSWERED**

**YES: A SHIP ROUTES AROUND ANY STRUCTURE ITS STRAIGHT LINE CROSSES. The owner's answer, 2026-09-23**,
asked for the same reason as Q59: the M1.16 session watched a Miner fly through its own station.
`GameDesign.md` §7 ruled out separation *between ships* and said nothing about anything solid.

- **Route around** — the owner's choice. When a ship's line to its destination crosses a structure, it
  steers for the tangent past it instead.
- **Keep passing through**, recorded as deliberate.
- **Decide at M2**, when asteroids are a second kind of obstacle.

**Q61 widened this the same day to other ships**, with the rules it needed; what follows is the structure
half, and it is unchanged apart from Q61's 400-unit look-ahead.

**Built to this, which is the recommendation under the answer.** A **structure** is anything with no drive:
a station or a module, anyone's. Its keep-out circle is **half its size plus half the ship's**, both
from Q37's `sizeUnits`, so no figure is new. Each tick a moving ship takes the nearest structure its
straight line crosses, ties broken on identity, and steers for the tangent on the side it is already on.
**A structure is ignored if the ship is already inside its circle, or its destination is**, so a ship
built in front of its station can leave it, and an order onto a station arrives. No waypoint is stored:
the route is recomputed every tick from the world, so there is nothing new to keep in step. **Asteroids
are not in it.** M2 draws them as the generated field, not as world entities, so nothing steers around
one. Whether they should is a question for when fleets fight among them.

### Q61 — Do ships avoid each other? — **ANSWERED**

**YES, EVERY SHIP AVOIDS EVERY OTHER SHIP. The owner's answer, 2026-09-23, and it reverses half of Q19.**
Q19 ruled out separation between ships, and M1.17 built Q60 to that. On the device the owner then
watched two ships fly through each other and chose full avoidance over two cheaper options:

- **Avoid parked ships only.** Moving ships would still overlap briefly mid-flight.
- **Full avoidance** — the owner's choice. Every ship steers around every other ship, moving or not.
- **Keep passing through**, and record it as deliberate.

**What stays from Q19 is the ring.** Ships ordered to one point still get distinct slots, ordered by
identity. What goes is "no separation": a ship now steers around another ship the way Q60 steers it
around a structure, by the tangent past it, recomputed every tick and stored nowhere.

**Built to these rules, which are the recommendation under the answer.** Each one exists because the
plain version jams or weaves:

1. **Every other ship is an obstacle**, anyone's, parked or moving. Its keep-out is half its size plus
   half the mover's, from Q37's `sizeUnits`, the same as Q60.
2. **Except a ship flying the same way.** A moving ship whose heading is within an eighth of a turn of
   the mover's is in the same stream, and is not avoided. Without this, a fleet flying in formation
   swerves around itself, because the ring puts neighbors exactly one hull apart.
3. **Ships given the same order ignore each other.** One fleet order is one group, and the group
   outlives the order. Without this, a ship bound for an inner ring slot could never get past the ships
   already parked around it. **It replaced a first attempt the same day**, a final approach within three
   ship sizes of the destination. That rule let a Fighter fly straight through a Miner parked 98 units
   past its destination, which is how the device found it.
4. **Only what is within 400 world units is considered**, structures included, found through M2.5's
   `UniformGrid`. Four hundred is well past the 45-unit turning radius (Q59) and the widest steering
   circle, so a ship still starts its swerve in time. The grid is what keeps a 128-player stress run from
   checking every pair. **The tick first had a grid of its own**, built on a branch that did not yet
   have M2.5's. The two were folded into one after the merge, and the pinned hash did not move. The
   query reaches 64 units past the look-ahead, because the grid measures from where a ship is now, and
   the start-of-tick snapshot then decides on exactly 400.
5. **Head on, both pass on the right**, which is Q60's fixed rule for an obstacle dead on the line.

**Positions are read as they were at the start of the tick**, so which ship moves first in index order
never changes what the other one sees. The nearest obstacle along the line wins, and the lower entity
index breaks a tie.

**What it costs, measured 2026-09-23** with ADR-022's harness on the Surface Pro, `Release|ARM64`, as the
host's whole CPU over 60 seconds. **At 128 seats it was 17.8 ms a tick with the first rule and 15.6
with the order groups, against 11.2 before Q61**, and no tick was abandoned. At two seats it is 0.36 ms,
lost in the noise. **Read these as a check and not a benchmark.** One run with the groups measured
5.3 ms, and it delivered about a third fewer updates than the others, for a reason nobody has found. So
it is not quoted as the figure. The grid is why avoidance costs a fraction of the tick and not most of it.

**What would reopen it** is a formation or a battle that jams anyway. That is M3's to find, when fleets
first meet on purpose.

## Registered — 2026-09-24, from the mid-implementation review

**Sixteen questions, all open, all from
[`Reviews/2026-09-23-mid-implementation-review.md`](Reviews/2026-09-23-mid-implementation-review.md).** The
review reached its figures by driving the unmodified `GameCore` and `GameLogic` under g++ on Linux, not by
estimating them; its §6 lists twelve decisions it says no agent may take, and the rest of its findings name
a rule the design has not stated. Each is registered here with **the review's recommended default as the
recommendation**, the finding it came from, and what it is needed by. **Q62, Q66 and Q72 were ruled the same day, to their recommendations; the rest are open.** The review's
defects against rules that were already written down were fixed in the same change, not registered: the
free Fighter at the map center (B5), the token stream (B4, ADR-013 amended), the dead-identity refusal (M4,
Q24 amended), send-once in the packaged client (M5), the unaffordable tap (m1), the hash's blind fields (M6,
ADR-002), the forget horizon (m2), the removal backlog (m3), the ring against the wall (m4), the dead
upgrade (m7) and the hold's selection half (m9).

**Two of the review's findings were overtaken before they were registered.** It was written at `67a4ad8`,
before M1.17 merged: ships now turn as they fly (Q59) and route around structures and each other (Q60,
Q61), so M11's "fifty identical arrows" and M16's "ships stack on one point" no longer describe the code.
Q68 keeps what is left of M11.

### Q62 — What binds the economy: income, or the build slot? — **ANSWERED**

**The finding (B1, m11).** On the shipped field one Miner earns 6.40 credits a second at the nearest home
rock and 3.33 at the farthest, against the 2.5 that `GameDesign.md` §4 assumes. Income passes the 20-a-second
build slot before sixty seconds, and past four miners it cannot be spent: 30,165 credits sat unspent at
300 s in the harness. Every scripted opening that took ShipyardL1 then L2 fielded 28 to 30 fighters at
300 s, and every one without it 17 to 18, so the opening is one forced sequence. Q47 meant the slot to sit
"just above the income a running economy earns", and it sits far below it. Separately, nothing limits how
many miners take ore from one rock, so stacking on the nearest two beats spreading by 21% to 46%, and Q26's
"ten rocks keeps six miners from queuing" names a rule nobody wrote.

- **Move the home field out**: `HOME_FIELD_INNER_RADIUS_UNITS` 600 → 1,200, outer 1,500 → 2,000. Measured:
  one Miner then earns 2.67 to 3.70 a second, six earn 18.6 (under the slot, as Q47 intends), and five
  openings finish within 25% of each other with different profiles.
- **Raise the slot to 30.** Six spread miners (27.6 a second) then sit just under it, but flights stay 640 to
  1,340 units, and §7's 1,500 stays wrong.
- **Per-rock throughput**, either way: one extractor per rock per tick, the rest holding. Or delete Q26's
  sentence and accept stacking.

**Recommendation: move the field, keep the slot at 20, and write one extractor per rock.** Restate §4, §5,
Q26, Q47 and the three code comments that cite 2.5 and thirty seconds, from the measured table, in the same
change. Q63 depends on the radii.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION, AND BUILT.** `HOME_FIELD_INNER_RADIUS_UNITS` is 1,200
and the outer 2,000 (`GameCore/Generator.h`); `MiningSystem` lets one miner extract from a rock a tick, in slot
order, and a second in range holds with its cargo unchanged. **Re-measured on the shipped loop after the
change, not taken from the review**, which predates M1.17's turning and routing (seed 20260922, two players,
g++ on Linux, steady state over the second half of ten minutes):

| | Credits a second |
|---|---|
| One miner, per home rock, nearest to farthest (1,254 to 1,948 from the anchor) | 3.67, 3.67, 3.47, 3.00, 3.00, 3.00, 2.67, 2.67, 2.33, 2.33 — **mean 2.98** |
| Six miners, one on each of the six nearest rocks | **19.82**, under the 20-a-second slot |
| Ten miners, one per rock | 30.02 |
| Ten miners, five on each of the two nearest rocks | 35.68 with one extractor per rock; 36.67 without |
| One miner at a contested rock, unloading at home | 0.94 to 1.00 |

**Two things differ from what the review predicted, and both are stated rather than smoothed.** Six miners
earn 19.8, not 18.6: under the slot, but by 1%, so a seventh passes it. And **stacking still beats spreading
by 19%**, because on this field the two nearest rocks are simply the shortest flights; one extractor per rock
removed only 3 points of it, since five miners at a rock rarely arrive together. So the rule makes Q26's
"ten rocks serve thirty miners" true and does not make spreading optimal. **What would reopen it** is M3.11
finding a stacked opening dominant, which is then a question of rock placement rather than of throughput.
§4 carries the figures; Q26, Q47, `Generator.h` and `BuildSystem.h` point at this table. The determinism pin
moved to `0x4c8b850e5dec326e` (ADR-002) and the generator's pinned region was regenerated.

### Q63 — What does the station's point defense protect? — **ANSWERED**

**The finding (B2).** Point defense reaches 400 units and a MassDriver 600, and a miner unloads within 160
of the station. From 401, a fighter is outside point defense and inside MassDriver range of every point of
the unloading area, the spawn point and every near-side module. So "what the point defense protects is the
unloading area" and "which side of your station you build on is a decision" are both false as built, and
M3.11's "is the safe zone too safe?" has nothing inside it to measure.

- **Point defense 480, with flight and unload to the far side**: the unload point is 160 beyond the station
  on the ray away from the nearest hostile, computed on the host in integers, ties broken on identity. The
  near side is then shellable from 481 to 600 and the far side is not, which makes §5's side sentence true.
  It needs Q62's radii.
- **Point defense 800 at a 30% Medium modifier.** An unescorted siege needs nine fighters, and the near-side
  standoff disappears.
- **Delete the side claim** from §5 and ADR-015, and test the zone as a radius only.

**Recommendation: the first**, with "crosses 400 and dies" restated as "crosses 440".

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION, AND BUILT (M3.5).** Point defense reaches 480. A miner
unloads at a point its unload reach (160 for a `Scout`) beyond the station's center, on the ray away from the
nearest hostile, found through the grid with ties to the lower identity. The ray comes from the pinned bearing and
sine table, so it is integers throughout (R16).

**One detail the ruling left open was decided in building: "nearest hostile" means within a threat radius**, the
home field's outer 2,000 plus a mass driver's 600. An enemy station is always on the map, so an unbounded search
would have moved every unload to the side away from the enemy base, raid or no raid, and changed Q62's measured
economy. **With no hostile inside 2,600 the unload point is the near side, as before.** An unloading miner
re-checks every tick and goes round when a raider arrives on its side. Measured by `CombatTests`: a Fighter at 440
dies in 112 ticks, 5.6 seconds. A Miner at exactly 480 is hit and one at 481 is not. A fleet ordered to attack a
station now stands off at 580, which is the target's reach plus 100.

### Q64 — Does a miner under a mine order flee? — **ANSWERED**

**The finding (M1).** §4, §7 and M3.6 all put flight on "a miner with no order", and every productive miner
carries the standing mine order. So flight never fires on a miner that is working, and §7's "about 3½ of
six lost fleeing" describes a rule that cannot run. In the harness three fighters kill six shuttling miners
in 25.7 s and none flees.

- **Flight as a phase of the mine order.** A miner in ToOre or Extracting that a fire event names enters
  Fleeing: it keeps its rock and cargo, heads for Q63's far-side point, and returns to ToOre after 60 ticks
  without being fired on. An explicit MoveTo does not flee, which keeps the player's override. Extracting
  and Unloading re-check range every tick.
- **Redefine "no order"** as "no move or attack order", which is the same rule stated once.

**Recommendation: the first**, with §7's flight row re-derived on Q62's field. The 60-tick resume is the
figure to argue with.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION, AND BUILT (M3.6).** A miner going to its rock or
extracting that is fired on enters a fifth phase, fleeing: it keeps its rock and cargo, heads for Q63's unload point, and goes back to
its rock after 60 ticks without taking damage. "Takes damage" is the weapon system's list of what it hit this
tick, not the fire events, which are thinned to one in ten ticks and would miss most hits. A miner on its way
home or unloading is already heading for the same point, so it does not flee. **A move order is the player's
override and never flees**, since it ends the mine order. **A flight is a retreat and not a trip home**: out of
the raider's 600 and three calm seconds later the miner turns back for its rock. Measured by `MinerFlightTests`:
three Fighters holding 600 past six miners at a rock 1,500 out kill none in a minute with flight, and two
without it.

### Q65 — How does a contested match end in about five minutes, and what is a tie? — **ANSWERED**

**The finding (B3, M7).** One fighter lands 12.5 dps on an 8,000-point station, which is 640 s. The earliest
ten-fighter siege against an idle defender ends at 300 to 330 s. A defender builds 5.7 fighters while a raid
crosses 86 s of map. Under Q26's 200 ore a rock, the home field runs dry at 106 to 141 s. And "last station
standing" has no answer when the last two stations die on one tick, which is what two mirrored stub AIs
produce.

- **A match clock**: the host ends the match at tick 7,200 (six minutes). The higher station hull wins,
  ties broken by credits plus the catalog cost of live ships and modules. **Plus a draw**: if every
  remaining station dies on one tick, the match ends as a draw and restarts like a victory.
- **A softer station and a shorter map**: 5,000 hull or hit value 100, and anchors at 4,500, with §7's
  crossing figures restated.

**Recommendation: the clock and the draw**, and an acceptance test in the harness the day M3.2 lands: a
three-fighter rush ends by 5:30, and a mirror ends by 8:00 or on the clock.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION, AND BUILT (M3.7).** The last station standing wins. A player whose station
dies is eliminated, and their ships and modules are removed on the same tick. **At tick 7,200, six minutes, the
match ends on the clock**: the surviving station with the most hull wins. A tie on hull goes to credits plus the
catalog cost of the player's live ships and modules, and a tie on that is a draw. **If every remaining station
dies on one tick, the match is a draw.** A draw restarts like a victory. The harness acceptance test is M3.11's.
**A match of one seat has nobody to outlast and ends only on the clock**, which is what a solo test match against
nothing does; with the stub AI (M3.10) it has an opponent.

### Q66 — Where does damage round, and how often is a fire event sent? — **ANSWERED**

**The finding (M2)**, which narrows M3.0's gate. Any per-tick integer rule makes stations and modules immune
to every ship weapon: a MassDriver against Heavy is 0.3125 a tick, which truncates and rounds to zero. The
wire carries 40 fire events an update, repeated three times, so M mounts need a cadence of at least ⌈3M/40⌉
ticks. At one tick, 100 mounts back up 52,000 sends in 200 ticks. Death order and the first shot's phase are
unpinned: deaths applied in slot order give player 1 the first shot in every symmetric fight.

- **A hundredths accumulator per weapon per tick**: whole points applied, the remainder kept. Deaths are
  applied after every weapon has fired, and overkill is allowed. A fire event goes out at most once per
  shooter per ten ticks, whatever the damage cadence, with the repeat dropped to two while a client's queue
  is past 40. §7's rows are pinned as tests with one tick's tolerance.
- **1 Hz shots with integer damage per weapon and size class**: the first shot on acquisition, deaths
  deferred, and §7's rows restated at +4% to +9%.

**Recommendation: the first.** It is the only option that reproduces §7 exactly, and ADR-014 records it.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION**, and recorded in
[`ADR-014`](ADR/ADR-014-damage-accumulates-every-tick.md) and `GameDesign.md` §7 *What a weapon does per tick*,
which answers M3.0. **One correction in the writing down:** "hundredths of a point" leaves a fraction a tick
(25 dps at 70% is 87.5 hundredths), and the formula the recommendation gives, dps × modifier × 100 ÷ 20, is in
ten-thousandths, where every §7 figure is a whole number. ADR-014 states that unit. Nothing is built yet:
M3.1 and M3.2 build to it.

### Q67 — How does an attack order bring a fleet to its target? — **ANSWERED**

**THE STANDOFF ARC. The owner's answer, 2026-09-24**, to the recommendation below, taken at M3.2. **Built to one
refinement the recommendation did not state**: the radius is the target's longest range plus 100 *only while
that is inside the attackers' own range*. A Fighter attacking a Fighter would otherwise stand at 700 with a
600-unit weapon and never fire. So the radius is the smaller of that and the fleet's shortest weapon range less
50, and never less than the two hulls' keep-out plus 20. Against a station it is 500, which is §5's standoff.
The arc spans a half circle facing the fleet, centred on the bearing from the target to the fleet's middle.

**The finding (M3, m5).** Ring slots at a 90-unit spacing put three of ten fighters inside point defense at a
500-unit standoff. Pursuing to contact feeds every attacker to point defense, and pursuing to range stacks
the fleet on its own approach line. Separately, the intake acknowledges an Attack without resolving its
target, so an Attack on your own ship would become a free follow verb if pursuit were written literally.

- **Pursue to a standoff, on an arc.** The standoff radius is the target's longest weapon range plus 100.
  Slots sit a hull's width apart along the circle, assigned nearest first with identity ties as
  `OrderFleetTo` does. The overflow goes to a second arc 90 further out, and the slots are re-solved when the
  target has moved a spacing. This is the "different slot layout" Q19 promised.
- **Keep the hex rings at half spacing** for attacks: 45 units, which fits nineteen fighters in the band.

**Recommendation: the arc**, and M3.2 resolves the target with `ResolveWireIdentity`. It refuses and
acknowledges an unresolved target or an own one, like every other refusal past the sequence check.

### Q68 — Do weapons have arcs? — **ANSWERED**

**YES, NOW: ±45° FOR A SHIP'S WEAPON, 360° FOR THE STATION'S POINT DEFENSE. The owner's answer, 2026-09-24**,
against the recommendation below, taken at M3.2 with two further rulings the same day. **The arc is a catalog
row** (`ComponentEntry::arcHalfAngle`, a binary angle), so a weapon's arc is data like its range. **What turns a
ship to bear**: a ship with no order, or one standing at its attack slot, turns in place toward its target at its
own turn rate (Q59). A ship under a move order keeps its course and fires only at what falls inside its arc.
Point defense needs no turning, and a station has no drive to turn with. **What it costs**, which the
recommendation named: an engagement now depends on approach geometry, and a Fighter needs about a quarter of a
second to swing onto a target 45° off. §7's rows hold for ships already bearing, which is how
`DamageTableTests` measures them.

**The finding (M11), half overtaken.** The review found no turning at all and recommended 360° weapons, with
facing derived on the client. Q59 has since given ships a host-side heading that turns at a derived rate, so
facing exists and is hashed. What is left is whether ADR-004's "range and arc are checked at the fire tick"
means an arc. With no formation logic that faces a target, an arc makes an engagement's outcome depend on
approach geometry that no order controls.

- **360° weapons for the MVP.** One range check; the heading stays presentation for combat. Reopen with
  the Cruiser at M4.4, which is when mass and turn rate are meant to matter.
- **Arcs now**, with an attack order that turns a ship to bear, which Q67's arc would have to own.

**Recommendation: 360° for the MVP**, and strike "arc" from M3.2's done-when and from ADR-004 until M4.4.

### Q69 — Finite ore: which specification, what numbers, and is there a forward unload point? — **ANSWERED**

**The finding (M10).** M3.9 is specified two incompatible ways: TechnicalDesign §4 has an ordinary record
owned by nobody, and the plan has a sparse list in a `Snapshot.cpp` that no longer exists. Neither states
its numbers. And a contested rock is an economic null at this map's scale whatever it holds: a miner
unloading at home earns 0.67 to 1.33 a second from one, because the flight binds, not the quantity.

- **Host-side finite ore, a husk and a retarget, in `GameLogic` only**: no wire change, no ore number in
  the panel, pinned in the determinism script. 200 ore per home rock on Q62's field and 600 per contested
  rock. **Plus a fifth placed design as a forward unload point**: a depot with 1,500 hull at 300 credits,
  placed at least 2,000 from every station and within 800 of a rock, at most two a player. Measured: 12 to
  19 fighters at 300 s against 6 to 9, and last-minute income of 10 to 45 a second against 4 to 12. No line
  in `MiningSystem` changes.
- **No depot**: move the contested clusters to 3,000–3,500 from the origin, and say in §3 that nobody but
  their owner contests them.
- **Rocks as entities**, if the panel must show ore: the ore bucket rides the hull byte, at +44 entities.
  Never the sparse list.

**Recommendation: host-side finite ore and the depot.** The depot is the one addition the review makes
against the scope cut list, which is why it is the owner's decision and not an agent's.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION.** Finite ore is host-side, with 200 a home rock and 600 a
contested one. An empty rock stays a husk, and a miner retargets the nearest rock with ore left. There is a fifth
placed design, the depot: a forward unload point with 1,500 hull and a 300-credit cost, at least 2,000 from every
station, within 800 of a rock, and at most two a player. **Built (M3.9)**, with three details decided in
building. **The depot is its own hull row** (`DepotFrame`), so it is not a module and does not count against the
station's four. **Its size is 79**, the longest side of the level-one ore processor's mesh, which it borrows
until the mesh handoff has a depot (Q37 makes size the drawn mesh's). **Its button is the ship row's free third
place**, which the HUD handoff leaves empty. The button is not in `geometry.json`, and the geometry check does
not claim it.

### Q70 — What does a restart keep, and how does a client learn one happened? — **ANSWERED**

**The finding (M8).** `BeginMatch` drops every seat, so a match ends in a one-second blackout, and clients
re-seat in arrival order: sides can swap, and a human can take an AI's base. No result is shown. ADR-013's
proposed detector, the tick going backwards, cannot fire, because `BeginMatch` never resets the tick.

- **Keep seats across `BeginMatch`**, reserve AI seats, and add two bytes to the update header: a match
  generation and a winner. That takes the header from 21 to 23 bytes, protocol 6, with 100 records an update
  unchanged. A client that sees a new generation shows the result, re-joins with its token, clears its
  selection and markers, and recenters. The tick stays monotonic.
- **A `MatchEnded` packet repeated for ten ticks**, which costs no header bytes and adds a third record type.

**Recommendation: the header bytes, with seats kept.** B4's salt (ADR-013 as amended) already keeps "same
token, same side" within a host run. `Scripts/DatagramBudget.py` is run before either lands.

**RULED 2026-09-24 BY THE OWNER: THE `MatchEnded` PACKET, AGAINST THE RECOMMENDATION, WITH SEATS KEPT. BUILT (M3.8).** The
update header stayed at 21 bytes (22 since Q83, for another reason). When a match ends, the host sends each seated client a `MatchEnded` record
carrying the winner (or a draw) for ten consecutive ticks, keeps every seat, and begins the next match on a new
seed. A client that hears it shows the result, clears its derived state, and joins again with its token. That
join's reply carries the new seed, so R23's "the seed arrives on the join reply and nowhere else" still holds.
**Two details were decided in building.** The protocol went to 7, since a version-6 client would count the new
record as a fault and never rejoin. And the command intake is kept across a restart, like the seats, because its
sequences belong to the seat. The next seed is SplitMix64's finalizer over the last, so a run of restarts is the
same run of maps on every host.

### Q71 — What answers M3.11's balance questions: twenty matches against a stub, or the harness? — **needed by M3.11**

**The finding (M15).** The stub as planned never defends, escorts or raids modules. A human raiding it meets
no response, and a human it attacks always responds. So M3.11's raid, safe-zone and match-length questions
would be answered by the stub's absences. Those questions are arithmetic, which the deterministic harness
answers in milliseconds.

- **Split M3.11.** First a harness gate: twenty scripted matches (a rush against an idle defender, a mirror,
  a raid against fleeing miners, a module raid at the standoff) whose figures are written into §7 and re-run
  whenever a constant moves, ported into `GameLogicTests`. Then **five human matches** for what arithmetic
  cannot answer: alert habituation, tap precision, orbit and panel legibility. The stub gets one defend
  reflex.
- **Twenty matches by the owner against the stub** (Q79 rules out a second person), accepting that the stub's absences answer nothing about
  balance.

**Recommendation: the split.** It challenges `GameDesign.md` §10's "the only mechanism", so it is the
owner's call.

### Q72 — Which confirmations block M3.11, and in what order is M3 built? — **ANSWERED**

**The finding (B6, M16, M17, m16).** About thirty hardware confirmations are owed. The steps open them about
three times as fast as the evenings close them: in the review's window, about twenty opened against seven
closed. Every one is treated as blocking. M3 puts the two steps that make a match exist, restart and the
stub, ninth and twelfth. Five documents carry the same status paragraph. Each pin move asks for a four-pair
run, and M3 moves the pin at least five times. One WIP commit reached `main` through a pull request.

- **Three gate classes in `Plan/README.md`.** Class A blocks M3.11: M3.0, Q66, Q62, one four-pair run at M3's
  entry and one at its exit, and M2.14 as a five-minute number. Class B is folded into the first three M3.11
  matches. Class C is deferred past M3.11 with the reason written. **The four-pair run happens at milestone
  boundaries only**, as `Plan/README.md` already says. There is **one status paragraph**, in
  `Plan/README.md`, and a step's annotation is the six-line report. **M3 is reordered** to
  M3.0 → M3.1 → M3.2 → M3.4 → M3.7 → M3.8 → M3.10, then the raid content. **A WIP commit is squashed before
  merge.**
- **One consolidated "confirmations owed" table** in `Plan/README.md`, with every gate otherwise as written.

**Recommendation: the classes, the order and the single status paragraph.** The four-pair run this
change's pin move owes (ADR-002) is class A under this rule, and is made once at M3's entry.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION.** Written where each part lives: the classes and the
milestone-boundary rule for the four-pair run in [`Plan/README.md`](Plan/README.md) *The gate classes*, with
the lists re-taken against the plan as it stood that day rather than the review's, since M1.16's session had
closed several; the order in [`Plan/M3-the-fight.md`](Plan/M3-the-fight.md) *The order of work*, the steps
keeping their numbers; the single status paragraph as `Plan/README.md` *Where it stands*, with `README.md`,
`AGENTS.md` and `Design/README.md` pointing at it; and the squash rule in `AGENTS.md` §6.

### Q73 — What is the stress target for the MVP? — **needed by M1.14b's owed runs**

**The finding (M13).** ADR-024 says the accumulator costs "nothing" at 100 clients. It costs 51.5 to 53.4 ms
a tick on the review's VM, the whole tick. Every record is built twice per client and every candidate is
fully sorted. At two players it costs 16 µs and at four, 77 µs.

- **Eight seats for the MVP.** M1.14b's owed runs shrink to one seated-and-acknowledged run, which from a
  laptop over Wi-Fi is also M0.5's wireless measurement. The churner, flooder, ceiling and fix go on a
  post-MVP list, and the fix is taken whenever the accumulator is next touched: records built once, a lean
  candidate, and `nth_element` then a sort of the top slice. Measured at 11.1 ms, with byte-identical output.
- **Keep 100, and take the fix now.**

**Recommendation: eight seats**, with the 51 ms figure recorded in ADR-024 as the reason.

### Q74 — Does the game do the build chores for the player? — **needed by M3.11's human matches**

**The finding (M14).** The best opening is 42 commands in 300 s, 34 of them builds: three to four gestures
every seven seconds, with the station on screen and the panel open. A mis-tap replaces the item in
progress. Every spawned miner needs a mine order by hand. New ships stack on one spawn point, so neither
player can tell three fighters from thirty.

- **Three small builds, none touching the wire.** A double tap on a build button arms repeat, re-sent by
  the client when its block shows the slot empty and the credits cover the cost. A design that can mine is
  ordered at spawn to the nearest rock with ore. Ships spawn on ring slots in front of the station, inside
  point defense.
- **A host-side queue four deep, plus a rally tap and a count badge**, costed with `DatagramBudget.py`.

**Recommendation: the three small builds.**

**The spawn half was ruled on 2026-09-24, and not as recommended.** The owner watched two ships built back to
back appear on one point, and asked for the spawn point to move out past the module circle. A new ship now
appears at 400 plus half a module, half itself and 20: 495 for a Miner and 510 for a Fighter. If something is
already there, it takes the first free slot of a ring around that point. That is ring slots, as recommended,
but **outside** point defense rather than inside it, so a new ship is not covered by the station's 480 (Q63). Q80's
queue answers the mis-tap. The repeat build and the order at spawn stay open.

### Q75 — What weapon does the Cruiser carry, and what does it cost? — **ANSWERED**

**The finding (M18).** At 2,400 credits with four MassDrivers, the Cruiser loses to an equal cost of fighters
before speed is considered. It moves at 62 units a second, not §7's 50. It cannot reach the enemy inside a
five-minute match even if ordered at 0:00. So M4.7's "does speed counter mass" would answer a question about
the damage table.

- **A HeavyDriver row**: 50 dps a mount, with modifiers 40/100/60 against Small/Medium/Large. Numbers then
  count, and kiting at 140 against 62 is the counter M4.7 tests. §7's 50 is corrected to 62.
- **Keep the row and price it at 1,200.** It then beats four fighters and loses to eight.

**Recommendation: the HeavyDriver row**, with the cost kept at 2,400 for the four-player format.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION.** The Cruiser carries HeavyDrivers, 50 damage a second a
mount, at 40, 100 and 60 percent against Small, Medium and Large. It costs 2,400, and §7's 50 units a second is
corrected to the 62 its mass gives. M4.4 builds it, and Q85 puts the weapon behind research.

### Q76 — Does the join check that both sides derived the same field, and does CI compile ARM64? — **ANSWERED**

**The finding (M12).** Nothing automated checks the one property that fails silently: the host and an
ARM64 client deriving the same field. A defect there presents as a miner mining a rock the player cannot
see. CI builds `Debug|x64` only.

- **A 64-bit field hash on the join reply** (FNV-1a over `GenerateField` and `GenerateLayout`, from one
  `GameCore` function both sides call). That takes the reply from 23 to 31 bytes, protocol 6, and a mismatch
  is refused at join. **Plus a CI job that compiles** `GameLogicTests` and `GameCoreTests` for `Debug|ARM64`
  on the x64 runner: compile only, minutes rather than a doubled pipeline.
- **Run those two suites on a Windows ARM64 runner**, if the account has one, which would execute the pin
  on ARM64 every push.

**Recommendation: both halves of the first.** The CI half is a change to what `AGENTS.md` §6 says CI gates,
which is the owner's.

**RULED 2026-09-24 BY THE OWNER, TO THE RECOMMENDATION: BOTH.** The join reply carries a 64-bit hash of the field
and the layout, and a client whose own derivation hashes differently refuses the seat. CI also compiles
`GameCoreTests` and `GameLogicTests` for `Debug|ARM64`, compile only. **Built** before M3.9: the reply is 31 bytes,
riding protocol 7, which had not left this branch. A reply with a zero hash is not checked, which only a refusal and a
suite's hand-built reply carry. The shipped map's hash is pinned in `GeneratorTests`, the same on all four pairs.
`AGENTS.md` §6 says what the new CI job covers and what it still leaves open. **It has not run on GitHub's runner
yet**: its first run is the next push.

### Q79 — Does any gate need more than one person? — **ANSWERED**

**NO. Every gate and every approval can be passed by the owner alone. The owner's answer, 2026-09-24**, in the
owner's words: *"I have no friends."* It is Q58's rule, one machine, extended from the hardware to the people.
Where a gate asked for a second human, it now asks for **the owner and an AI or a harness bot in the other
seats**:

| Where | It asked for | It asks for now |
|---|---|---|
| `Plan/M2-the-field.md` *Leaving M2* | Two commanders, and both clients drawing the field | The owner and a second seat, a harness bot or a second instance played one window at a time as M1.15 was |
| `Plan/M4-the-opponent.md` M4.8 | Four slots, humans and AI mixed, over a real network | The owner and three AI, on one machine (Q58) |
| `Plan/M4-the-opponent.md` *Leaving M4* | Four commanders, any mix of people and AI | The owner and three AI |
| Q49's revisit | Four people looking at a full field | The owner, in M4.8's four-slot match |
| Q71's second option | Twenty human matches on two devices | Twenty matches by the owner against the stub |
| `GameDesign.md` §2 | Solo play as the only test "before there are four people" | Solo play against AI as how the game is tested |

**What it costs, stated once.** No gate measures two people's play against each other: not how a raid feels to
the side receiving it, and not whether a human rival does what the AI does not. Balance questions are answered
by the owner against AI, and by ADR-022's harness where arithmetic can answer them (Q71). **What would reopen it**
is a second person arriving, and then M4.8 is the first gate worth running again.

### Q80 — Is there a build queue? — **ANSWERED**

**YES: AN ORDER WHILE SOMETHING BUILDS JOINS A QUEUE, PAID FOR WHEN IT IS QUEUED. The owner's answer, 2026-09-24**,
in the owner's words: *"I rather add the additional ship to the queue to be built. As long as there is money, I
can add ships to the build queue."* It reverses two earlier answers:

- **Q35's replacement.** A build order while an item builds used to replace it, at a full refund. It now waits
  behind it. Q35's full refund on cancel stands.
- **Q21's "there is no queue".** Q21 cut the queue because no wire record could feed it. The count now rides the
  spare high bits of the per-player block's building-design byte, so the update's size does not move. The
  contents are not on the wire, and nothing draws them.

**Built to these rules**, which the owner's words decided or which follow from them:

1. **Paid when queued**, so money is the only limit: an order the balance cannot cover is refused and nothing
   else moves, as before. There is no cap on the queue's length beyond the credits.
2. **First in, first out.** The next item starts on the tick the one before it finishes, at the build rate the
   player had when it was queued, which is M2.12's rule that an item keeps the rate it began with.
3. **Cancel takes the newest first**, at a full refund of what it cost. With nothing queued it cancels the item
   in progress, as Q35 said.
4. **Modules queue too**, one queue for both rows as before. A queued placement counts against the site rules
   and the cap of four, as if it were built. An upgrade already queued or building for a module refuses a second.
5. **The panel shows how many wait**, as "+N" beside the item in progress. The count saturates at 15 on the wire.

### Q81 — What does a shot, and a miner at work, look like? — **ANSWERED**

**A BEAM FOR EACH, DRAWN BY THE CLIENT. The owner's report, 2026-09-24**: *"I do not see when a ship is shooting
or mining, that animation does not exist."* M3.2 sent fire events and nothing drew them, and a miner at its rock
looked like a miner parked beside one. This is M3.3's tracer and a mining beam, built together:

1. **A shot is a tracer**: a soft-edged line from the shooter's drawn position to the target's, lying on the
   plane, drawn a quarter of a second and fading. It starts after the interpolation delay, so it joins the hulls
   where they are drawn rather than where they will be. Colored by weapon: orange for a mass driver, pale blue
   for point defense. The three repeats of one event are folded into one tracer by shooter, target and weapon.
2. **Mining is a green beam** from an extracting miner to the nearest rock in its reach, and **unloading is an
   amber beam** into the owner's nearest ore acceptor. The client does not know which rock or station the host
   chose (R19). It takes the nearest one, which is the one the host chose in every case the rules allow.
3. **Two of the record's eight state values are taken** for it: 1 is extracting, 2 is unloading, and 0 is
   neither. **Extracting means taking ore this tick**, so a miner waiting its turn at a worked rock (Q62)
   draws no beam. Nothing else is assigned, and an unassigned value draws as nothing. The protocol stays at 6,
   since the bits were already on the wire as zero.
4. **A beam's width is the larger of a pixel count and a world-unit floor**, so it stays visible at the default
   camera distance and does not become a hair beside a hull seen close.

**Not tuned.** Colors, widths and the lifetime are first guesses, and the owner's eye is the gate.

### Q82 — Which rock does a miner mine? — **ANSWERED**

**ONE MINER TO A ROCK, AND IT KEEPS IT. The owner's report and ruling, 2026-09-24**, playing the first match against
the stub AI: *"every mining ship seems to pick the same random asteroid to mine... The idea would be that every ship
will pick one asteroid and return to that asteroid. Other miners can be assigned to another asteroid which they will
use until depleted."* As built, a mine order gave every selected miner the tapped rock, and a spent rock's miners all
retargeted to the same nearest one, where Q62's one extractor per rock made them queue.

1. **A group mine order spreads.** The miner nearest the tapped rock takes it; each of the others takes the nearest
   rock nobody has, measured from the tapped rock, so a group ordered to a field covers it.
2. **A miner keeps its rock** through every cycle until the rock is spent.
3. **A spent rock's miner takes the nearest rock nobody has**, from where it is. **Only when every rock with ore is
   taken do two share**, at the one the fewest have, then the nearest, ties to the lower index (R16).

"Has" counts every miner with a mine order on that rock, of any owner, because Q62 lets one extract at a time
whoever owns it. Built in `MiningSystem`'s `RockClaims` and `BestRock`, used by the intake and by retargeting.

### Q83 — What happens to a spent rock? — **ANSWERED**

**IT IS REMOVED FROM THE MAP. The owner's ruling, 2026-09-24**, asked when the owner saw a spent rock still drawn:
*"either restock it or remove it"*, and removal was chosen over restocking, which would have made ore finite in name
only. Q69 had kept ore on the host, so the client could not know a rock was spent.

**The wire**: every update carries a spent-rock mask, one bit a rock in `GenerateField`'s order, behind a new count
byte. It is at most eleven bytes, the largest field's 88 rocks, and empty until a rock is spent. **On every update
rather than repeated for a while**, so each stays self-contained (ADR-024): a client that joins late, or lost a
hundred updates, knows every spent rock from the next one. The header went from 21 to 22 bytes, the guaranteed
floor from 65 records to 64 and the typical fill from 99 to 98, and the sweep is still one tick at the MVP's 110.
`Scripts/DatagramBudget.py` computes all of it. The protocol went to 8, because 7 had been pushed.

**The client** keeps the newest mask and bakes the field again without the spent rocks, looks computed over the
whole field first so no survivor changes shape. A spent rock can no longer be tapped, and no mining beam points at it.

### Q85 — What is research in the MVP? — **ANSWERED**

**ONE UNLOCK, THE CRUISER'S WEAPON. The owner's ruling, 2026-09-24**, asked at the start of M4 because M4.4b needs
it and `GameDesign.md` §9 leaves research unsettled on purpose. It keeps §9's one structural rule: research
unlocks components, components make designs, and designs are what a station builds.

- **A research station** is a third module, `ResearchStationL1`, at 500 credits, placed like the other two.
- **Research** is one project, the `HeavyDriver`, at 600 credits and 60 seconds. It is started by an order and runs
  while the player owns a research station. **Losing the station stops research in progress**; a finished unlock is
  kept.
- **The Cruiser also needs a level-two shipyard**, the first time a shipyard's level gates a hull, as §5 said it
  would.
- **The wire** gains one unlock byte a player, in the player's own block.

The figures are a starting point, for M4.7's matches to move. **The alternative was no research in M4**: the Cruiser
behind a level-two shipyard alone, and M4.4b moved past the MVP with the designer.

**As built at M4.4b**, three readings the ruling left open:
- **"Stops" is taken as holds.** Research without a research station keeps its progress and goes on when another is
  built; nothing is refunded. Cancelling it and losing the 600 was the other reading, and harsher than the ruling
  needed.
- **The wire took two bytes, not one**: the unlock byte, and the research in progress as a percent plus one, so the
  button can show progress. The update header went from 22 to 24 bytes and a full update from 98 records to 97; the
  guaranteed floor stayed at 64. Protocol 9.
- **The research station has no mesh of its own.** It draws the bare module frame, which no other design draws,
  until the mesh handoff has one. Its place is the new top row of the build panel, which grew 112 pixels upward.

### Q84 — How does a match open? — **ANSWERED**

**WITH TWO MINERS AND A FIGHTER, 500 CREDITS, AND NO SHIP UNTIL THERE IS A SHIPYARD. The owner's ruling,
2026-09-24**, asked when the owner saw ships being built with no shipyard: *"start with 2 mining ships and 1 fighter
in space, close to the station and that you can only build a new ship when you have an yard"*. Until then a player
opened with 1,000 credits and nothing else, and the station built ships at its own rate from the first tick.

- **The fleet** comes from `GenerateLayout`, after every station so each station keeps its index: Miner, Fighter,
  Miner, 650 units toward the center and 110 apart across that line. Both sides run it (R23), and the host creates
  them as ordinary entities. 650 and not nearer, because a built ship appears 495 to 510 out on the same line and
  the first one out of the yard must not land on them.
- **The shipyard** is checked at the intake, where every order, a client's, the AI's and the scripted match's,
  comes in. A refused ship order is acknowledged like any other refusal. An item already building or queued when
  the last yard dies goes on at the station's own rate (M3.8b): it was paid for when there was a yard. The client
  dims the ship row with no yard, as it dims an unavailable module, so there is no button the host always refuses.
- **500 credits** buys the 400-credit yard with 100 over, and the two miners pay for what comes next.
- **The ore processor is unchanged.** The owner asked whether it was needed and chose to keep it as it is.
- **The stub AI** places a shipyard on the first legal of eight sites 250 units out before it orders a ship.

The map hash, the scripted match's and the stub AI match's all moved, because the layout, the opening bank and
the script's first order moved.

### Q78 — Does a ship under a move order fire? — **ANSWERED**

**YES, AT WILL, WITHOUT CHANGING COURSE. The owner's answer, 2026-09-24**, asked at M3.2. `GameDesign.md` §7 says
a ship with a weapon and *no order* engages the nearest hostile in range, and said nothing of a ship that has
one. A ship flying under a move order, or on its way to an attack slot, shoots the nearest hostile inside both
its range and its arc (Q68) and keeps flying. It never turns or stops for it. **The alternative was holding
fire**, so that a player moving through enemies would have to stop to fight.

### Q77 — Six small rules the code took or the design left open — **each needed by the step named**

The review's minors that are decisions rather than defects. Each has the review's default.

1. **Does a shipyard lost mid-build slow the item already building?** (m6, M3.8b.) The build system fixes
   the rate at the start, so M3.8b's exit criterion cannot be met. *Default: make progress rate-based, in
   hundredths a tick. The alternative is rewriting M3.8b and §5 to "fixed at the start".* **RULED 2026-09-24 BY THE OWNER,
   TO THE DEFAULT**: progress is rate-based, so a shipyard lost mid-build slows the item on the tick it dies. M3.8b
   builds it.
2. **Two module kinds, four slots, best-of-kind** (m10, §5). Two of the four slots can never do anything.
   *Default: say so in §5 ("a second module of a kind adds hull and nothing else") and keep the cap.*
3. **Is 50 ships a player a rule?** (m12, M3.9.) Nothing refuses a 51st. *Default: `BuildSystem::Start`
   refuses at 50 ships, modules excluded, dimmed in the panel like the module cap.*
4. **Four quiet decisions** (m13). The host seats three players without `--stress`; the record's three state
   bits are "not settled" yet audited; research needs a per-player unlock byte no document budgets; the grid
   is rebuilt inside mining while the tick has its own (Q61's "folding the two into one is owed").
   *Default: refuse three without `--stress`; define the bits as idle, moving, mining, fleeing and engaged
   in ADR-024; budget the unlock byte in TechnicalDesign §4 with Q70's header bytes; build one grid in
   `Host` after movement.* **Q81 gave the state bits a narrower meaning on 2026-09-24**: what a miner is
   visibly doing, in two of the eight values. The rest are free for this default if it is still wanted.
5. **Host-only tuning overrides** (m14). Every M3.11 figure costs a rebuild and a redeploy. *Default: a
   `Server --tuning` override of the host-only rows, hashed and printed beside the seed, and taken by the
   determinism test too.*
6. **The hit-value curve at M3.1** (m8). At the MVP it reproduces the size-class table exactly. *Default:
   M3.1 builds the size-class table only, and the curve arrives with the first armor component.*

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
