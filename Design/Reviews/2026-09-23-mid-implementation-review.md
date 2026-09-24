# Mid-implementation design review, 2026-09-23, at M2.12

**What this is.** A hard-nosed review of *Outpost Commander* at commit `67a4ad8`, roughly halfway through
the five milestones: M0 and M1 built, M2 built through M2.12 with its two closing gates owed, M3 and M4 not
started. It was run as a multi-agent review: a Lead built a ground-truth map from the documents and the
code (not from the status prose), six specialists then reviewed in isolation against that map, and the Lead
cross-examined every Blocker and Major before writing this. Every claim below is marked fact or inference;
a fact cites a file and line, a document section, or a run of the shipped simulation.

**How it was measured.** `GameCore` and `GameLogic` compile unmodified under g++ 13 on Linux against a
one-line stub for `windows.h`, so the reviewers drove the real generator, the real mining loop, the real
build system, the real session table and the real accumulator, and the client's real replica store. The
sources and outputs are in [`2026-09-23-mid-implementation-review/`](2026-09-23-mid-implementation-review/);
nothing was run under MSVC or on the device, and every host-cost figure is an order of magnitude from a
cloud VM, not a Surface Pro measurement.

**What was not supplied.** The review brief's context block was left empty: no pillars, no time budget per
week, no specific worries. The pillars used below are read off `GameDesign.md` §1 and §10 and are stated
in §1; where a finding depends on the missing budget it says so.

**What became of it, 2026-09-24.** The findings that were defects against rules already written down are
fixed: B5, B4 (ADR-013 amended), M4 (Q24 amended by the owner), M5, M6 (ADR-002, the pin moved and its
four-pair run owed at M3's entry), m1, m2, m3, m4, m7 and m9; m15's glyph premise had already been struck
by a measurement. Every other finding, including the twelve decisions of §6, is an open question on the
register with this review's default as its recommendation: Q62 to Q77 in
[`OpenQuestions.md`](../OpenQuestions.md), and Q48 moved to M3.10. M11 and M16 were partly overtaken
before that, by M1.17's turning and avoidance (Q59 to Q61). **Nothing below has been edited**; it is the
review as it was delivered.

---

## 1. Verdict

**Finish with changes, and the changes are to the numbers and rules, not to the architecture.** The
engineering is in better shape than the design: the six reviewers independently found the linker-kept
client boundary, the determinism practices, the composition model, the replication design at MVP scale
and the register discipline sound and worth protecting. What does not hold is the design's own
arithmetic: the shipped generator, mining loop and build slot produce an economy 2.5× richer than
`GameDesign.md` §4 assumes and bound by the 20-credits-a-second build slot rather than by income, the
station's safe zone is geometrically empty against a 600-range weapon, miner flight as specified never
applies to a miner that is mining, and nothing ends a contested match in five minutes. Two small defects
in the code would corrupt any evening of matches before those questions are reached: every hosted match
gives player 1 a free Fighter at the map centre, and a host restarted on the same seed live-locks two
returning clients on seat 1. **The single biggest risk is that M3.11, the project's only instrument, is
run against the design as written**: its four answers would be artefacts of a build slot that binds at
four miners, a safe zone with nothing inside it, a flight rule that cannot fire and matches that cannot
end, and the project would spend its scarcest resource, the developer's hours on the device, tuning the
wrong game while about thirty earlier hardware confirmations are still open.

**The pillars, as read off the documents** (the brief did not state them; challenge them if they are wrong):

| | Pillar | Source |
|---|---|---|
| P1 | *Homeworld*'s feel: a fleet not an army, scale, ships that bank as they turn and read as silhouettes, an economy you have to defend | `GameDesign.md` §1 |
| P2 | *Warzone 2100*'s spine: a unit is a composition, every stat derived, research and a designer later | `GameDesign.md` §1, §6, §9; R24 |
| P3 | Touch only, one plane, one tap is one order | ADR-001, R21 |
| P4 | A deterministic, replayable, authoritative simulation the client never runs | R16, R19 |
| P5 | Twenty five-minute two-player matches in an evening turn §7's balance questions into answers | `GameDesign.md` §2, §10; M3.11 |

---

## 2. Design ↔ implementation map, condensed to deviations and gaps

Every row was checked against the code at `67a4ad8`. The full map, with the tuning table and the
measurements, is [`2026-09-23-mid-implementation-review/phase0-map.md`](2026-09-23-mid-implementation-review/phase0-map.md).

| # | System | Classification | Evidence |
|---|---|---|---|
| M1 | A free Fighter for player 1 at the map centre on every hosted match | Implemented, not designed | `Server/Server.cpp:146-149`; recorded unnoticed as "both drew three entities" in `Plan/M1-the-fleet.md:809` |
| M2 | Turning, turn rate, facing | Referenced (§1's pillar, §6 "turn rate derives the same way", ADR-004's "arc"), never specified, not implemented, not planned | no write to `Entity::heading` after `World::Create`; no turn field in `GameCore/Catalog.h`; no M3 or M4 step |
| M3 | Miner income and round trip | Implemented differently from the design: 6.4 credits a second and a 16-second cycle at the nearest rock against §4's 2.5 and 30 | harness, appendix B; `GameCore/Generator.h` inner radius 600, added by Q26 on 2026-09-23 |
| M4 | The build rate as the pacing device (Q47: "just above income") | Inverted: the 20-a-second slot binds from about fifty seconds in; income past four miners is unspendable | harness: 30,165 credits banked and unspendable at 300 s |
| M5 | "Contested fields are richer" | Referenced, never specified: fewer rocks (6 against 10), no ore figure, 0.7 to 1.3 credits a second at their distance | `Generator.h:54`; `GameDesign.md` §3 |
| M6 | Per-rock throughput ("ten rocks keeps six miners from queuing") | Assumed, not implemented: any number of miners extract from one rock at once | `MiningSystem.cpp`; `Generator.h:38` |
| M7 | The safe zone "protects the unloading area" | Designed, geometrically false: a fighter at 401 reaches every point of the 160-unit unloading area | `Catalog.cpp` (400 and 600), `UnloadTarget.h` (160); finding B2 |
| M8 | Miner flight "with no order" | Designed so that it excludes every miner under the standing mine order | `GameDesign.md` §4, §7; `Plan/M3-the-fight.md:214-217`; finding M1 |
| M9 | The client forgets an entity after three sweeps | Implemented as designed, but at the MVP that is 150 ms against a 1,000 ms link-loss window | `ReplicaStore.cpp:86-94`; measured, appendix B |
| M10 | The accumulator costs "nothing" at 100 clients (ADR-024) | Wrong by two orders of magnitude: 51 ms a tick measured | `Accumulator.cpp` (records built twice, full sort); appendix B |
| M11 | Commands "repeated in every outgoing packet until acknowledged" (TechnicalDesign §4) | True of the Bot only; the packaged client sends each command once | `OutpostCommander/App.cpp` four one-shot sends; `GameClient/BotPolicy.cpp` has the resend |
| M12 | Attack order | Designed, not implemented; accepted and acknowledged as a no-op, target never validated | `CommandIntake.cpp` |
| M13 | Endpoint timeout | Designed, deferred by ADR-013, not implemented | `Host.cpp` |
| M14 | Combat, damage, cadence, point defense, flight, death, elimination, victory, restart, finite ore, the stub AI | Designed, not implemented (M3, M4); the firing cadence is unspecified (F3, ADR-014 reserved) | `Plan/M3-the-fight.md` |
| M15 | Finite ore's replication | Designed twice, incompatibly: TechnicalDesign §4 says an ordinary record, Plan M3.9 says a sparse list in a `Snapshot.cpp` that no longer exists | `TechnicalDesign.md` §4; `Plan/M3-the-fight.md:280-304` |
| M16 | Collision and pathfinding | Never specified; movement passes through everything, ships stack on one point | `GameLogic/Tick.cpp`, `BuildSystem::SpawnPoint` |
| M17 | Module effect stacking | Implemented, recorded, not designed: the best of a kind counts | `ModuleEffects.cpp`; ADR-015 "As built" |
| M18 | Session tokens | Implemented as designed and the design is wrong: tokens are a function of the seed, so a host restart on the same seed re-issues last match's tokens | `Sessions.cpp`; ADR-013 "Tokens repeat across matches" |
| M19 | The state hash | Covers identity, position, heading and hull *type*; blind to hull points, cargo, phase, credits and build items | `StateHash.cpp` |
| M20 | Three-player matches | Allowed by the host without `--stress`, admittedly unfair, not designed | `Host.h`, `Layout.h` |
| M21 | The determinism pin for the current script | Computed under g++ and clang only; the four MSVC pairs are owed | ADR-002 Measurements; `README.md` |
| M22 | Everything M2 put on the screen | Built, never drawn or touched on the device | `Plan/M2-the-field.md` step annotations |

---

## 3. Findings

Severity means the same thing in every finding. **Blocker**: the MVP cannot reach its stated goal, twenty
trustable two-player five-minute matches at M3.11, without this. **Major**: real rework, or a gate's
answer invalidated, unless it is fixed before M3. **Minor**: cheap, or cosmetic. The reviewer codes are
R1 rules, R2 balance, R3 exploits, R4 core loop, R5 feasibility, R6 scope, and L the Lead; a finding
that several reviewers reached independently names all of them. Where reviewers disagreed the tension is
stated, not averaged.

### Blockers

```
[B1] [R2 R3 R6 L] [Blocker] [Balance]
Claim: The economy is bound by the 20-credits-a-second build slot, not by income, so miners past four are dead weight, the ore processor is a dominated purchase, and a raid on miners cannot touch what a player can spend.
Evidence: GameDesign §4 ("about 2.5 credits per second", "roughly thirty seconds"), §5 and Q47 ("sits just above the income a running economy earns"); GameCore/Generator.h:42-47 (home rocks 600 to 1,500 from the anchor, the 600 added by Q26 on 2026-09-23); BuildSystem.h (20 credits of cost a second, one slot); harness (appendix B): one Miner earns 6.40 credits a second on the nearest rock of seed 20260922 (797 units, 16-second cycle), 4.0 to 4.7 on typical rocks, 3.33 on the farthest; four spread miners earn 21.7 a second.
Scenario / proof: Fact, measured on the shipped code. With miners built back to back and sent to the nearest rock, income passes the slot before sixty seconds and 30,165 credits sit unspendable at 300 s. Across fifteen scripted openings, every line with ShipyardL1 then L2 fields 28 to 30 fighters at 300 s and every line without fields 17 to 18; adding OreProcessorL1 to the best line yields 28 against 30. A raid that kills three of eight miners leaves 23 credits a second, still above the slot: the defender loses nothing.
Why it matters: P1 needs an economy worth defending and M3.11's first question asks whether the opening has more than one viable line. As shipped the opening is one forced sequence, the module row is a script to execute, and the twenty matches would answer §7's economic questions about an economy that stops mattering at minute one. §4's "2.5 a second" and "thirty seconds" describe the field's outer edge only, and three code comments cite those figures as the reason for a constant.
Recommendation: Move the home field rather than the slot: HOME_FIELD_INNER_RADIUS_UNITS 600 → 1,200 and HOME_FIELD_OUTER_RADIUS_UNITS 1,500 → 2,000 (Generator.h). Measured: one Miner then earns 2.67 to 3.70 a second (mean 3.10), six miners 18.6, under the slot as Q47 intends, and five of the scripted openings finish within 25% of each other (14 to 19 fighters) with different profiles. Rewrite §4, §5, Q26, Q47 and the three code comments with the measured figures in the same change, and put the per-rock income table on Q26 as the derivation the register asked for. Alternative: keep the field and raise BUILD_RATE_CREDITS_PER_SECOND 20 → 30; six spread miners (27.6) then sit just under the slot, but the flight distances stay 640 to 1,340 units and §7's 1,500 stays wrong.
Confidence: High. Three harnesses reached the same figures from the unmodified loop; the remap was measured the same way. A human spreads miners less evenly than the harness, which changes exhaustion timing under M3.9, not the slot arithmetic.
```

```
[B2] [R2 R3 R4] [Blocker] [Balance]
Claim: The station's safe zone is geometrically empty: from 401 units a fighter is outside point defense (400) and inside MassDriver range (600) of every point of the 160-unit unloading area, the extraction point on the nearest rock, the spawn point and every near-side module, so "what the point defense protects is the unloading area" and "which side of your station you build on is a decision" are both false as written.
Evidence: GameCore/Catalog.cpp (PointDefense rangeUnits 400, MassDriver 600); GameLogic/UnloadTarget.h and .cpp (unload reach (60 + 220) / 2 + 20 = 160 for a Scout at a Station); MiningSystem.cpp (range checked centre to centre); GameDesign §5 ("a raider that chases a fleeing miner home crosses 400 and dies in under six seconds"; "which side of your station you build on is a decision"); Plan/M3-the-fight.md:203 (M3.5 done-when: "a fighter at 500 units takes no return fire").
Scenario / proof: Fact, on the shipped field. 401 + 160 = 561 < 600, so a fighter on the 401 ring fires on any unloading miner while the station never fires. On seed 20260922 the nearest home rock is 797 from the station; the miner extracts 602 from the station and unloads 157 from it; a raider 500 out on that radial is 104 from the extraction point, 343 from the unload point, 405 from the spawn point and 100 from a module placed at 400 on that side, and the miner is inside 600 for 100% of its 16-second cycle. Flight to the station centre passes 22 units from the raider and ends 500 from it, still in range. Three fighters kill six miners in §7's own 25.7 s without ever entering 400. The far side of the station is nine seconds' flight for the raider, so the side a module is built on is a nine-second decision.
Why it matters: M3.11's second question, "is the safe zone too safe?", is unanswerable as posed: there is nothing inside it. §7's flight row ("about 3½ of six lost") describes a flight that lands in the raider's reach, and M3.5's and M3.6's exit criteria would pin a mechanic that does not exist. The design's two intentions, a point defense that protects unloading and a fighter that stands off at 500 unpunished, contradict each other by 200 units and one has to give.
Recommendation: PointDefense rangeUnits 400 → 480 (Catalog.cpp; ModuleSiteTests' "ring equals PD range" becomes "ring 400 ≤ PD 480"), and make the flight and unload destination the point 160 beyond the station on the ray away from the nearest hostile, computed on the host in integers with ties on identity. A fighter at 481 then reaches the near side (321) but not the far side (641 > 600); the station stays shellable from 481 to 600 (a 119-unit band, one six-slot ring at 90 spacing); far-side modules at 155 clearance are safe from that ring and near-side ones are not, which makes §5's side sentence true; "crosses 400 and dies" becomes "crosses 440". This needs B1's radii: at the shipped 600 inner radius a miner extracting at 400 from the station would sit inside 480 and the opening would become simply safe, which Q16 rejected. Alternative: PointDefense range 800 with its Medium modifier 90 → 30, so a fighter lasts 16.7 s inside it: an unescorted siege then needs nine fighters and loses six, a module raid needs four and loses two or three, a field at 1,200 or more stays raidable, and §5's near-side standoff disappears. Either way, delete the side sentence from §5 and ADR-015 unless the 480 rule is taken.
Confidence: High on the geometry (three catalog constants and one derivation). Medium on 480, which assumes M3.2 checks range centre to centre as the mining loop does; edge to edge shifts every figure by about 155 and keeps the 200-unit gap.
```

```
[B3] [R2 R4 R6 R3] [Blocker] [Balance]
Claim: Nothing ends a contested match in five minutes: the unopposed floor is 5.0 to 5.7 minutes, under symmetric play the defender's transit edge exceeds any economic edge the numbers can produce, and under Q26's 200 ore per rock the home field is dry at 106 to 141 s and both stations idle for the second half.
Evidence: Catalog.cpp (Station 8,000 HP, hit value 300, so a quarter lands; MassDriver 25 dps, two mounts on a Fighter); Layout.h (12,000 units between opposed stations, 85.7 s at 140 units a second); BuildSystem.h (20 a second; a Fighter is 15 s of slot); GameDesign §2 ("a match of about five minutes"), §7 (fighter against fighter 20 s); Plan/M3-the-fight.md:352 (M3.11 question 3 already asks "or does it run to fifteen?"); harness (appendix B).
Scenario / proof: Fact for the floor: one fighter lands 12.5 dps on a station, 640 s; the earliest ten-fighter siege against a defender who does nothing ends at 300 to 330 s across three openings and a three-fighter rush at 344 s. Inference for the typical match: 86 s of crossing at 20 credits a second is 1,720 credits, 5.7 fighters built while a raid is in transit (11.5 at ShipyardL2), which is larger than any economic edge the shipped numbers hand out, so symmetric play settles into mutual fighter production and a match ends only when somebody stops responding. Fact for finite ore: with the plan's nearest-with-ore retargeting layered over the unmodified mining loop, 200 ore per home rock is dry at 106 s with eight miners, 112 with six, 141 with four; the slot then idles 85 to 188 s of the match, income falls to 3 to 10 a second, every ShipyardL2 line finishes below every no-shipyard line, and each side ends with seven to ten fighters and no economic reason to leave home.
Why it matters: P5's whole mechanism is twenty matches in an evening and its balance answers; twenty matches of ten to fifteen minutes is four to five hours, and a match decided by who stopped responding says nothing about §7. M3.8 restarts only on a victory.
Recommendation: Three things together. (1) A match clock: the host ends the match at tick 7,200 (six minutes) and names the higher station hull the winner, ties broken by credits plus the catalog cost of live ships and modules, all host-side facts and one comparison in M3.7; every M3.11 match then yields a result. (2) One consistent ore set: 200 ore per home rock on B1's remapped field (dry at 150 to 167 s with six miners, half the match, which is "a long opening and not a match"), 600 ore per contested rock, extraction, capacity and unload unchanged, plus the forward unload point of M10 so the second half has an economy and a symmetry breaker (measured: 12 to 19 fighters at 300 s against 6 to 9, last-minute income 10 to 45 a second against 4 to 12). (3) An acceptance test in the harness the day M3.2 lands: a scripted three-fighter rush against an idle defender ends by 5:30 and a scripted mirror match ends by 8:00 or on the clock. Alternative: no clock, station hull 8,000 → 5,000 or hit value 300 → 100 (six fighters kill it in 53 s) and ANCHOR_RADIUS_UNITS 6,000 → 4,500, accepting that §7's 80-to-100-second crossing and every row built on 85.7 s is restated.
Confidence: High on the floor and the exhaustion times (arithmetic and runs on shipped constants); Medium on the typical length, which the scripted mirror will settle in minutes.
```

```
[B4] [R1 R3] [Blocker] [Logic]
Claim: Session tokens are a pure function of the match seed and the order of joining, so a host restarted with the same seed with two returning clients that launch in the other order live-locks them on seat 1, each evicting the other every second, with seat 2 never taken.
Evidence: GameLogic/Sessions.cpp (Begin reseeds Pcg32{seed, 1}; a presented token that matches a held seat is Rejoined and the seat's endpoint moves, from any endpoint, before anything else; a fresh seat draws the next token from the same stream); Host.cpp (BeginMatch calls Sessions::Begin with the match seed); Server/Server.cpp (DEFAULT_MATCH_SEED unless --seed); OutpostCommander/App.cpp (the client presents the token it stored and stores a changed one); GameClient/ClientFrame.h (a seated client rejoins after 1,000 ms of silence, on a fresh socket); ADR-013 Consequences ("tokens repeat across matches on one host, the outcome anybody would want"); SessionsTests pins that two runs of one seed issue the same tokens.
Scenario / proof: Fact, reproduced twice on the unmodified Sessions.cpp (r1/tokens.cpp and r3/sim.cpp). Evening 1 at seed 20260922: A is player 1 with token b11fa2dcd9134829, B is player 2 with ba4ef9dcf6f26c24. Evening 2, same seed, B launches first: its old token matches nothing, it is seated as player 1 and issued the stream's first token, which is A's old token. A then joins with that token, is Rejoined as player 1 and takes the seat's endpoint; B hears silence, rejoins with the token it now holds, is Rejoined as player 1; they alternate, seat 2 stays free, both screens show the reconnecting overlay every 1.3 s. With the same join order, or a different seed, both are seated correctly.
Why it matters: An evening starts with a host launch, the design encourages a fixed hand-checked seed, and each tablet keeps last match's token in LocalState; about half of host restarts on a repeated seed cannot start a match, the symptom looks like a network fault, and the workaround (delete session.txt on both devices, or change --seed) is written nowhere. ADR-022's "restart the host to run again" re-triggers it for any bot process that kept its JoinState.
Recommendation: Salt the token stream with a per-host-run nonce taken at the seam: Sessions::Begin(count, seed, nonce) with the stream seeded from seed ^ nonce, the nonce read from the wall clock in Server.cpp's RunHost (the shell is where R16 allows wall time) and a constant in every suite; delete the "two runs of one match issue the same tokens" test, add "a token from a previous Begin never matches a seat of this one", rewrite ADR-013's paragraph. Within one host run tokens then still map the same client to the same side across matches, which is what M8's restart wants. Alternative: mix the first-claiming endpoint into the token, which also works but breaks the moment a rejoin arrives on a new port.
Confidence: High. Only a guarantee that no seed is ever reused across host runs would lower it.
```

```
[B5] [R3 L] [Blocker] [Logic]
Claim: Every hosted match gives player 1 a free Fighter at the map centre, so no match played on the real host has the symmetric start the design promises.
Evidence: Server/Server.cpp:146-149 (RunHost creates DesignId::Fighter owned by player 1 at the origin after BeginMatch and orders it to (4096, 2048); the comment calls it "the whole of the simulation at M0"); AGENTS.md R20 (no game logic in Server); HostTests asserts stations-only on Host, not on Server, which is why it hid; Plan/M1-the-fleet.md:809 records the M1.15 run as "both drew three entities" without noticing.
Scenario / proof: Fact for the entity, arithmetic for the timing: the origin is 4,507 units from player 2's nearest home rock, 32 s at 140 units a second; player 2's first miner delivers at about 14.5 s and its first possible fighter needs 300 credits and 15 s of the slot; under M3's rules the free fighter kills a working miner every 12.9 s (miners under a mine order do not flee, M1) from about 32 s, before any opponent fighter can exist.
Why it matters: A 300-credit and first-raid head start for whichever client joined first makes every one of M3.11's matches a measurement of this leftover rather than of §7. The fix is trivial, which is exactly why it must be scheduled rather than assumed.
Recommendation: Delete the four lines as the first commit of M3 and add a HostTests-style assertion that a match begun through Server's path has exactly `players` live entities after twenty ticks. Alternative: keep the M0 mover behind a --demo switch that PlayerCountAllowed-style validation refuses outside a stress run.
Confidence: High.
```

```
[B6] [R6 R4] [Blocker] [Scope]
Claim: The human's hardware confirmations open faster than they close, and without a triage rule M3.11 is reached behind about thirty owed confirmations that consume the same evenings the twenty matches need.
Evidence: git log (78 non-merge commits in 34 hours; M1's fifteen code steps in 9.6 hours on 2026-09-22 and M2's fourteen in 15.6 hours on 2026-09-23, about one step an hour; five owner commits recording hardware runs in the same window); the owed list at HEAD (map §6 and the plan): M0.5 (2 runs), M0.23 (1), M1.14b (5), M1.14c (4), M1.15 (1), M1.16 (7), ADR-021 (1), M2.13 (2), M2.14 (1), the four-pair run for the current pin (1), six M2 screens never looked at, and four decisions (M3.0, Q34, Q48, Q49); Plan/README.md ("An agent never marks a gate done"; "A gate does not move without a reason written down"); no weekly time budget is stated anywhere.
Scenario / proof: Fact for the counts: between 2026-09-22 and 2026-09-23 seven gate items closed or half-closed while the steps of the same window opened about twenty. Inference for the rate, which assumes the demonstrated ratio holds and an evening is about three hours: at about three confirmations a calendar day, thirty-one items is ten days of the owner's evenings spent not playing, and M3 adds at least five more pin moves ("appears in the determinism test" at M3.2, M3.6, M3.7, M3.9, M3.10), each of which the M2 annotations treat as owing a four-pair MSVC run.
Why it matters: The owner's device time is the only scarce resource this project has; the plan treats every confirmation as blocking and the agent opens two per step, so the instrument that the reduced MVP exists to run keeps receding.
Recommendation: Write a three-class gate rule into Plan/README.md. Class A, blocks M3.11: M3.0 (the cadence); Q48 before M3.10; B1's re-derivation; one four-pair run at M3 entry and one at M3 exit; M2.14 as a five-minute number (the Linux harness already says the simulation is 13 µs at 110 entities). Class B, folded into the first three M3.11 matches and recorded in one commit: M1.16's items 1, 2, 4, 5 and 7; M2.13 both halves; the six M2 screens; M1.14c's loopback tap-to-visible re-run; ADR-020's habituation question. Class C, deferred past M3.11 with the reason written: M0.5's LAN and wireless runs, M0.23's two-machine run, M1.14b's churner, flooder and ceiling, M1.14c's eight-seat run, M1.15's "two people playing", ADR-021's clean install, §9.6, the sky's frame time, Q34 (rule 60 Hz, its own recommendation), Q49. Reduce the four-pair run to once per milestone boundary, which Plan/README already says, and make the M2 annotations cite it. Alternative: keep every gate as written but move all class B and C items out of the milestone files into one "Confirmations owed" table in Plan/README.md, so the owner sees one list and a milestone file reads as steps again.
Confidence: High on the counts; Medium on the rate, because 34 hours is a short window and the owner's commit times batch the agent's work.
```

### Majors

```
[M1] [R1 R3 R4 R6] [Major] [Logic]
Claim: Miner flight as specified never applies to a miner that is doing any work, because every productive miner carries the standing mine order and flight is conditioned on "no order"; the flight row of §7's raid table describes a rule that cannot fire, and the shipped state machine cannot express flight for an ordered miner without a new phase.
Evidence: GameDesign §4 ("a miner told to mine keeps mining until it is told to do something else", the design's only standing order); §7 ("a miner with no order that is fired upon flees to its station"; the table's "miners lost fleeing 1,500 units to the station: about 3½" of six); Plan/M3-the-fight.md:214-217 ("with no order is the condition and it matters: a miner explicitly ordered to mine keeps mining"); GameLogic/World.h and MiningSystem.cpp (a working miner's phase is never None from ToOre through Unloading; only a MoveTo clears it; HeadFor re-issues the rock as the move destination every ToOre tick; Extracting and Unloading never re-check range). Harness: a flee written as a move order on a ToOre miner is overwritten in the same pass; a miner in Extracting moved 496 units away keeps extracting.
Scenario / proof: Fact. Three fighters reach a home rock where six miners shuttle: none flees, each dies in 12.9 s, all six in 25.7 s, the "was" column §7 says the redesign fixed. The only miners that ever flee are freshly built ones sitting idle 140 units in front of the station, inside point defense already.
Why it matters: §7 calls flight the thing without which "the defender must be watching the right part of a 16,384-unit map at the right moment to have any counterplay at all"; M3.11's first question would test a raid that is a slaughter by rule rather than by numbers, and M3.6's exit criterion ("§7's figure reproduced") could only pass on a fixture whose miners were never ordered to mine.
Recommendation: Make flight a phase of the mine order: a miner in ToOre or Extracting that a fire event names as target enters Fleeing, keeps its rock and cargo, heads for B2's far-side point, and returns to ToOre on the same rock after 60 ticks without being fired on; a miner under an explicit MoveTo does not flee, which keeps the player override M3.6 wants; Extracting and Unloading re-check Within each tick and fall back. Re-derive §7's flight row under that rule from B1's field. Alternative: define "no order" in §4 and §7 as "no move or attack order", which is the same rule stated once.
Confidence: High. Three documents agree on the condition; the resume interval is the part that belongs on the register.
```

```
[M2] [R1 R5] [Major] [Balance]
Claim: The firing-cadence gate (M3.0, ADR-014) is narrower than the plan believes: any per-tick integer damage rule makes stations and modules immune to every ship weapon, the wire caps fire events so the cadence must be at least ⌈3M/40⌉ ticks for M mounts, and death ordering and first-shot phase are unpinned.
Evidence: Catalog.cpp (MassDriver 25 dps, PointDefense 60); GameDesign §7 (modifiers 70/60/25 and 120/90/30; structures base × 100 / (100 + 300), a quarter); Plan/M3-the-fight.md:31-35 (computes only "25 ÷ 20 truncates to 1"); GameCore/Update.h (MAX_FIRES_PER_UPDATE 40, FIRE_REPEAT_TICKS 3); Accumulator.cpp (only the first update of a tick carries repeated facts; only sent events decrement); harness C (appendix B).
Scenario / proof: Fact, arithmetic: MassDriver against a station or module is 25 × 100 / 400 = 6.25 dps = 0.3125 a tick, which is 0 under truncation and under rounding, so no ship weapon ever damages a base and §5's whole standoff argument is void; against Small it is 0.875 a tick, 0 or 1 (20 dps, +14%); PointDefense against Medium is 2.7 a tick, 2 or 3 (80 or 120 dps against the designed 108). A 1 Hz integer-per-shot rule lands within +4% to +9% of every §7 row; only the hundredths accumulator the plan lists as "an option" reproduces §7 exactly. Fact, measured: with three repeats and a cap of 40, 100 mounts at a one-tick cadence back up 52,000 sends in 200 ticks and the tracers lag 171 ticks; at an eight-tick cadence they keep up. Also unpinned: deaths applied inside the weapons pass in slot order give the lower slot's shot first, a systematic player-1 edge in symmetric fights; at 1 Hz, whether the first shot fires on acquisition decides whether a ship leaving range takes zero or one extra shot; overkill with two shooters on one target.
Why it matters: M3.11's answers are only trustworthy if the simulation does what §7's table says; a per-tick integer rule makes the station unkillable and a per-shot rule quietly moves every row; and the cadence chosen for balance also decides whether ADR-020's alerts, derived from fire events, are timely.
Recommendation: Settle ADR-014 as: damage accumulated per weapon in hundredths of a point per tick (dps × modifier × 100 / 20 a tick, whole points applied, the remainder kept), deaths applied at the deaths step after every weapon has fired, overkill allowed; a fire *event* emitted at most once per shooter per ten ticks whatever the damage cadence (104 mounts is then 31 sends a tick, under 40), the repeat dropped to two while a client's pending queue exceeds 40; §7's rows pinned as tests with one tick's damage of tolerance. Alternative: 1 Hz shots with integer damage per (weapon, size class), first shot immediate on acquisition, deaths deferred, and §7's rows rewritten to the +4% to +9% figures.
Confidence: High on the arithmetic and the measurement; Medium on which mitigation the design prefers.
```

```
[M3] [R4 R5] [Major] [Logic]
Claim: The attack order has no standoff formation: hex rings put three of ten fighters inside point defense at a 500-unit standoff, and pursuit as the plan describes it stacks every attacker on the target's point.
Evidence: GameLogic/RingAssignment.cpp (ring k at radius k × spacing, 6k slots, spacing the widest hull, 90 for a Fighter); Catalog.cpp (400 and 600); Plan/M3-the-fight.md:77-79 (M3.2: "a ship with an attack order pursues its target", "no formation system beyond M1.7's ring spacing"); M3.1 must reproduce "§5's ten fighters against a station"; harness (the shipped RingSlotOffset): ten fighters ordered to the best point on the approach axis put seven in the 200-unit band between 400 and 600 and three inside 400; twenty put twelve in the band, four inside and four out of range; fifty put twenty-one, thirteen and sixteen.
Scenario / proof: Fact for the geometry, inference for M3.2 as unwritten: ordered to 500 the three inner fighters die in 17 s to 108 dps and the seven survivors take 91 s on the station instead of §5's 64; ordered to 600 three are out of range and the rest sit on the boundary where quarter-unit rounding decides whether they fire; with a pursue-to-contact attack order all die to point defense, with a pursue-to-range order all halt at about 600 on their own approach line and stack into one silhouette.
Why it matters: This is one of the two things the loop cannot exist without: a way to bring a fleet to a target that produces §5's siege rather than a coin flip on ring geometry. M3.11's second question would be answered by the ring layout, not by the safe zone.
Recommendation: Specify the attack order at M3.2 as pursue-to-standoff with an arc layout: standoff radius = the target's longest weapon range + 100 (500 against a station), slots spaced by the widest hull along the standoff circle (34 at radius 500), assigned nearest-first with identity ties exactly as OrderFleetTo does, overflow to a second arc at +90, re-solved whenever the target has moved a spacing; this is the "different slot layout" Q19 promised. Alternative: keep hex rings and halve the spacing for attack formations (45 units; nineteen fighters fit inside the band), since overlap is harmless without collision.
Confidence: High on the arithmetic; the standoff rule that would change it does not exist today.
```

```
[M4] [R1] [Major] [Logic]
Claim: Once ships die, an order whose selection names a ship that died since the client's last record is refused in its entirety and never acknowledged, so a retreat tapped during a raid silently does nothing for the survivors and its marker stays on screen.
Evidence: GameLogic/CommandIntake.cpp (every selection identity is resolved before any is acted on; StaleGeneration is returned before m_lastApplied is written; the "acknowledgment still advances on a refusal" rule applies to station orders only); Q24; CommandValidationTests pins "a half-valid selection applies none of it"; GameClient/Selection.cpp prunes only against records already received. Harness: Move {a, b, dead c} gives StaleGeneration, LastAppliedSequence 0, neither a nor b ordered.
Scenario / proof: Fact for the mechanism, inference for the frequency: three fighters kill a miner every 4.3 s; the client learns of a death 50 to 150 ms after the host; a retreat tap on the group inside that window carries the dead identity, the whole order is refused with no acknowledgment, and nothing resends (M5). Roughly 2% to 3% of orders issued during a raid, which are exactly the orders the balance questions depend on.
Why it matters: The defender's counterplay fails exactly when it is exercised; Q24's rule was written when nothing died.
Recommendation: In Apply, drop identities that resolve to NO_ENTITY (a dead ship cannot be ordered, so nothing is half-applied), refuse only NotOwned and SelectionTooLong, accept an all-dead selection as a no-op, and advance m_lastApplied on every refusal past the sequence check; re-pin the test as "a foreign identity applies none; a dead one is skipped". Alternative: keep all-or-nothing but acknowledge refusals and add a one-byte refusal code to the player block so the client re-issues with the pruned selection.
Confidence: High on the mechanism; Medium on the frequency.
```

```
[M5] [R1 L] [Major] [Logic]
Claim: The packaged client sends each command exactly once; the "repeated in every outgoing packet until acknowledged" reliability of TechnicalDesign §4 exists on the wire and in the Bot but not in the game, so a lost datagram is a lost order.
Evidence: OutpostCommander/App.cpp (four sites build a CommandPacket with that tap's commands and send it once; the 250 ms view report carries commands = {}); no outstanding list or FillOldestFirst call anywhere in App.cpp or GameClient/ClientFrame (verified by the Lead); only Bot/Bot.cpp and GameClient/BotPolicy.cpp implement the resend; GameClient/OrderMarker.cpp clears a marker only on acknowledgment; wireless loss is unmeasured (M0.5 owed).
Scenario / proof: Fact. At 1% loss and about a hundred taps a match one order a match vanishes silently, twenty an evening, and in a quiet phase its marker persists until some later order is acknowledged.
Why it matters: Balance readings are contaminated by orders that never arrived, and a stated design property is false on the device it is for.
Recommendation: Move BotPolicy's pattern into ClientFrame: keep m_outstanding, retire by SequenceIsNewer against the own block's lastCommandSequenceApplied in DrainPackets, and FillOldestFirst it into every outgoing packet including the 250 ms view report; drop a command older than 40 ticks and clear its marker so a permanently refused one cannot ride forever. Alternative: keep send-once but time markers out at two seconds so the interface stops asserting an order that was lost.
Confidence: High.
```

```
[M6] [R1 R5] [Major] [Logic]
Claim: The state hash folds the hull type and not the hull points, and nothing of cargo, mine phase, credits or build items, so M3's damage arithmetic can diverge between the four builds without the determinism test or the four-pair run noticing until a death or a completion happens to shift.
Evidence: GameLogic/StateHash.cpp (folds id, position, heading and static_cast<uint8_t>(entity.hull)); GameCore/Entity.h marks HullId as hashed and hullRemaining as not; TechnicalDesign §2 says "hull"; DeterminismTests pins the hash and asserts credits and ore only with inequalities. Harness: setting hull 450 → 1, cargo 0 → 99,000, phase None → Unloading and owner 1 → 2 each leave the hash unchanged.
Scenario / proof: Fact. Two builds disagree on where 25 × 70 ÷ 100 rounds (F3's own worry): hullRemaining differs by one per shot, positions match, the hash matches, the four-pair run passes, and the divergence surfaces as one machine killing a miner a tick earlier only if the scripted match happens to run a fight to a kill at that point.
Why it matters: R16's only executed check does not watch the quantity M3 adds, and the 20% error class F3 describes is exactly a hull-points divergence.
Recommendation: Fold per live slot hullRemaining, owner, the mine order's phase, rock, cargo and unload target, and after the loop each player's credits, pending milli-credit hundredths and build item (about fifteen lines; StateHash takes the BuildSystem and Economy or a MatchHash combines them); move the pin deliberately once before M3.1; amend ADR-002 and TechnicalDesign §2's four-field list. Alternative: leave ADR-002's four fields and add a second pinned value over the rest in the test itself, which is not what a desync report would carry.
Confidence: High.
```

```
[M7] [R1] [Major] [Logic]
Claim: "Last station standing" has no answer when the last two stations die on the same tick, and the mirrored AI-against-AI fixture makes that the expected outcome, so the unattended match never ends.
Evidence: GameDesign §2 ("victory is the last station standing"; "four AI players and no human is a test fixture"); Plan/M3-the-fight.md:237-239 (simultaneous destruction must "resolve the same way every run" but names no rule); M3.10 ("a match against it reaches a victory unattended"); Layout.h and Generator.cpp make every region an exact copy.
Scenario / proof: Inference on the fixture, fact on the missing rule: two identical deterministic stubs on mirrored halves launch identical sieges on the same tick; with deaths deferred to the deaths step both stations reach zero together, zero stations are standing, the condition is never true and the fixture hangs; with deaths applied in slot order instead, player 1 wins every mirror match by construction.
Why it matters: M3.10's exit criterion and M3.8's restart loop stall on the fixture; a human match hitting it runs on with two eliminated players.
Recommendation: §2: "if every remaining station is destroyed on one tick the match is a draw; a draw ends and restarts the match like a victory and the result overlay names it"; a VictoryTests case that kills both on one tick and asserts the match ends. Alternative: rank by hull remaining at the start of the tick, ties a draw.
Confidence: High that the rule is missing; Medium on the fixture's exact simultaneity, which depends on M2's death-order choice.
```

```
[M8] [R5 R1 R6] [Major] [Feasibility]
Claim: The restart path as built drops every seat, so a match ends in a one-second blackout, the clients re-seat in arrival order (sides can swap and a human can take an AI's base), no result is shown, the camera stays on the old base, and ADR-013's proposed detector ("the tick going backwards") can never fire because BeginMatch never resets the tick.
Evidence: GameLogic/Host.cpp (BeginMatch resets the world, intake, build, economy, sessions and accumulator and does not touch m_tick); Sessions.cpp (Begin clears every seat; a token no seat holds is given the lowest free slot); GameClient/ClientFrame.h (rejoin after 1,000 ms of silence); ReplicaStore.cpp (refuses any record or own block whose tick is not newer, so a reset tick would freeze the client); App.cpp recenters once on the first seat; Panels.h ("winner: nothing sets it before M3"); ADR-013 Consequences names the gap. Reviewer 6 reads the same path as "already falling out of what is built"; Reviewers 5 and 1 list what it costs. Both are right: it works, with these defects.
Scenario / proof: Fact for the mechanics: at victory BeginMatch clears the seats, both clients go silent for a second, each rejoins with a token the host no longer holds, whoever lands first is player 1 (two clients retrying every 250 ms is a race), the field is re-derived correctly, the selection keeps stale identities until the next tap and the player reads "reconnecting" instead of a result. Inference: players will mind the side swap.
Why it matters: M3.11 is twenty restarts in an evening; each costs a second of overlay, a possible seat swap and no winner shown, and solo against the stub is the only configuration M3.11 can run.
Recommendation: Keep sessions across BeginMatch (B4's per-run salt already keeps "same token, same side" within a run) and reserve AI seats (Sessions::Reserve from Server --ai N, excluded from LowestFreeSlot); add two bytes to the update header, a match generation (u8, incremented per BeginMatch) and a winner (PlayerId, NO_PLAYER while running), so the header goes 21 → 23, records per update stay 100 and the 65-record floor is unchanged, protocol 6; a client that sees a new generation shows the result overlay, re-sends Join with its token (Rejoined → same slot, new seed → the field re-derives), clears selection, markers and arming and recenters; the tick stays monotonic. Alternative: a MatchEnded packet (winner, seed, count) repeated ten ticks like a removal, which costs no header bytes and adds a third record to version.
Confidence: High on what the code does; Medium on which of the two wire shapes is cheaper.
```

```
[M9] [R3 R6 R5 R4] [Major] [Coherence]
Claim: M3.10's stub AI, written the obvious host-side way, reads order state no client is ever sent and reacts at tick cadence with exact positions, nothing enforces §8's no-cheating rule, Q48 is scheduled after the first AI is written, and the tree would then hold three decision-makers where one would do.
Evidence: GameDesign §8 ("not given information a human in its position would not have; a cheating AI is untestable"); Plan/M3-the-fight.md:306-324 (the stub in GameLogic on the tick, "without reading anything a client is not sent", stated not built); OpenQuestions Q48 (open; "retrofitting it means rewriting the AI's inputs"; recommends the AI over wire-shaped records); World.h (MoveOrder and MineOrder are host-only by design); GameClient/BotPolicy.cpp (the only decision code in the tree already takes a replica store and decides every twenty ticks); Accumulator.cpp's RecordOf already builds the client-shaped view on the host.
Scenario / proof: Inference on the unwritten stub, fact on the asymmetry: a stub that reads World sees a raid the tick it is ordered and pulls its miners thirty seconds before a human could see anything move, holds fighters at exactly 599 from a target with no quantization error and never misses a tap; a stub that "eventually attacks" with an order that pursues into 400 feeds every wave to point defense in 5.6 s. Either way the human's raid outcomes are artefacts of the stub. M3.10 written as StubAi.cpp and M4.5 as Ai.cpp is two AIs; Q48's recommendation dead or a rewrite of the inputs.
Why it matters: M3.11 is the project's only mechanism for turning §7 into answers, and an opponent with order-level foresight or none at all makes those answers unusable.
Recommendation: Rule Q48 before M3.10 and take its second option: the stub decides over a span of EntityRecords built by RecordOf for that player plus its PlayerBlock, once per twenty ticks, emitting Commands applied through CommandIntake::Apply, in the file M4.5 extends, with a test that the header takes no World; give it one defend reflex (on a fire event naming its entity, send idle fighters at the shooter), so the human meets a defender; M4.6's takeover of an abandoned slot then comes free by construction, and BotPolicy is replaced by it rather than grown. Alternative: keep a host-side stub but forbid it the order state by type, through a read-only view that exposes Entity and never OrderInSlot or MineInSlot, and pin its decision interval at BotPolicy's twenty ticks.
Confidence: Medium on the stub's behaviour (unwritten); High on the information asymmetry and the schedule.
```

```
[M10] [R5 R6 R2 R4 L] [Major] [Coherence]
Claim: M3.9 is specified two incompatible ways and both its numbers are missing, and the contested field is an economic null at this map's scale whatever its ore, so "richer" cannot be expressed by an ore quantity at all.
Evidence: TechnicalDesign §4 ("from M3, ore remaining rides an ordinary record whose owner is NO_PLAYER") against Plan/M3-the-fight.md:280-304 ("since the last snapshot", "four bytes each", files GameCore/AsteroidRecord.h and GameCore/Snapshot.cpp, which became Update.cpp at M1.14c; "TechnicalDesign §4's sparse-ore paragraph", which does not exist); World.h keeps the field beside the store; Command.h's Mine names a rock by index (Q52); HostTests asserts no asteroid is an entity; Generator.h:54 ("richer is M3's to make true"); harness: a miner unloading at home earns 0.67 to 1.33 a second from a contested rock 4,509 to 5,858 units out at two players and 1.1 to 1.5 from 3,416 to 4,686 at four, and 600 against 1,200 ore per contested rock give identical outcomes because the rate binds, not the quantity; a cycle is 2 × (d − 360) / 100 + 7 seconds per 100 ore, so any rock beyond 2,000 units yields under 3 a second whatever it holds; GameDesign §4 already names a forward unload point ("a mining factory placed out at a contested field is an obvious later feature") and UnloadTarget already asks "nearest owned acceptor".
Scenario / proof: Fact. The M3.9 agent reads the plan and writes a fourth capped section on the update, which moves the 65-record floor and the sweep on both sides, or reads TechnicalDesign and adds a design row that R24 says an asteroid is not; either way the other document is wrong and the budget script is stale. With the home field dry (B3) and no unload point, six miners sent 4,500 units out return one fighter per seventy seconds between them; both players can count. A hull that accepts ore at a contested cluster's centroid gives 6.7 to 9 credits a second per miner; placed at 90 s in a 200/600 match it lifts last-minute income to 10 to 45 a second against 4 to 12 and fighters at 300 s to 12 to 19 against 6 to 9.
Why it matters: §3 calls contested fields "the map's only real proposition" and §1 "the entire reason anyone fights"; M3.11 would find that nobody fights for them and be right. Reviewers 5, 6 and 2 disagree on the fix and the tension is real: Reviewer 6 wants the smallest M3.9 (host-side only, no wire change), Reviewer 5 wants rocks as ordinary entities so the ore bucket reaches the panel through the hull byte with no format change, Reviewer 2 wants a fifth placed module because without a forward unload point the second half has no economy. This is the one place this review recommends an addition against Scope's cut list, and it is a decision for the designer (§6, D3).
Recommendation: For the MVP, M3.9 is host-side finite ore, husk and retarget, in GameLogic only, pinned in the determinism script, with no wire change and no ore number in the panel; the plan step is rewritten to say so and the sparse list is never built. Numbers: B3's set (200 per home rock on the remapped field, 600 per contested rock). Then decide the forward unload point: a fifth placed design, ModuleDepot, a DepotFrame hull with acceptsOre and 1,500 HP carrying an OreDepot component at 300 credits, placed by tap at least 2,000 from every station and within 800 of a rock, capped at two per player outside the station's four; it dies in 120 s to one fighter and 40 s to three, and a dead depot sends its miners 4,500 units home, which is §4's "economy failing in a way a player can see"; no line in MiningSystem changes. Alternative: no depot, move CONTESTED_FIELD_NEAREST/FARTHEST from 1,500/3,500 to 3,000/3,500 from the origin so a contested miner earns about 2 a second, and say in §3 that the clusters are contested by nobody but their owner; if the panel must show ore later, take Reviewer 5's rocks-as-entities path (hullPercentRemaining as the ore bucket, +44 entities, the sweep at 2 ticks, MAXIMUM_INSTANCES raised), never the sparse list.
Confidence: High on the disagreement and on the null (arithmetic on speed, capacity and distance, and three harnesses agree); Medium on the depot's placement numbers, which raids that are not yet written would test.
```

```
[M11] [R4 R6 L] [Major] [Coherence]
Claim: Ships never turn, so there is no facing, no arc for M3.2 to check, no silhouette for the P1 pillar, no test for M4.7, and orbit shows a fleet of identical arrows from another angle.
Evidence: No code writes Entity::heading after World::Create; every built ship takes its station's heading (BuildSystem.cpp), which faces the map centre (Layout.h); the client draws the wire heading as sent (Interpolation.cpp, App.cpp) and derives nothing from motion; GameDesign §6 ("turn rate derives the same way") names a stat that is in no catalog row; §1's pillar; ADR-004 and Plan/M3-the-fight.md:89 ("range and arc are checked at the fire tick"); Catalog.cpp ("nothing authored a Cruiser mesh"); no M3 or M4 step adds turning; ADR-018 names orbit as the candidate to cut.
Scenario / proof: Fact. Player 1's fifty ships face +x from spawn to death; a raid on a home field off the x axis is inside a forward arc for one side and outside it for the other by spawn geometry alone, so any arc rule makes roughly half of engagements one-sided. M4.7 ("does speed counter mass") would compare a Cruiser and a Frigate that differ only in speed and hull, and confirm an identity.
Why it matters: A balance answer taken from fights that are asymmetric by construction is not an answer; the pillar is currently fifty identical arrows pointing at the map centre; and the milestone that exists to falsify ADR-006 cannot be falsified by playing.
Recommendation: For the MVP, strike "arc" from M3.2's done-when and from ADR-004 (360° weapons, one range check) and derive facing on the client from the motion vector between the two samples the interpolation already holds, keeping the last facing at rest: presentation only, R19-clean, no wire and no hash change. If the Cruiser returns at M4.4, add host-side turning then: a heading written at OrderMoveTo from the destination delta via a binary search over the pinned sine table, a derived turn rate (thrust over mass, like speed), the pin moved once, and a Cruiser mesh added to M4.4's files. Alternative: strike "bank as they turn" from §1 for the MVP, move M4.4 and M4.7 to post-MVP with formations, and cut orbit under ADR-018's own kill switch (it removes four constants and eight degrees of every gesture).
Confidence: High on the facts; Medium on whether the owner wants turning in the MVP.
```

```
[M12] [R5 L] [Major] [Feasibility]
Claim: The one property nothing automated checks, the host and the client deriving the same field on x64 and ARM64, presents as a miner mining a rock the player cannot see, and eight bytes on the join reply would turn it into a refusal at join.
Evidence: Generator.h ("presents as rocks in the wrong place rather than as anything that looks like a desynchronization"); R23 has both sides run GenerateField; MiningSystem.cpp mines field[mine.rock] while the client aims at its own rock k; ADR-002 (the current pin computed under g++ and clang only); .github/workflows/build.yml (Debug|x64 only, no ARM64 of anything); GeneratorTests pin the region only on the pair that runs; the generator is integer-only on its own stream, so a divergence would be an overflow, undefined behaviour or an unsequenced draw of the kind Sessions.cpp already caught once.
Scenario / proof: Fact for the gap, inference for the trigger: a later edit introduces such a defect, g++ CI is green, the x64 Surface run is green, the ARM64 client places rocks elsewhere, a Mine order names index 3 and the host mines a rock drawn somewhere else, and nothing logs, tests or refuses for the whole match.
Why it matters: ARM64 is the device, and the failure is silent.
Recommendation: JoinReply.fieldHash, a 64-bit FNV-1a over every Placement of GenerateField and GenerateLayout from one GameCore function both sides call, 23 → 31 bytes, protocol 6; the client compares after deriving and shows a build-mismatch refusal instead of playing (about forty lines and two tests). Plus a CI job that compiles GameLogicTests and GameCoreTests for Debug|ARM64 on the existing x64 runner (compile only, no UWP client, minutes rather than a doubled pipeline), so an ARM64-only break in the simulation stops reaching main. Alternative: run those two suites on a Windows ARM64 hosted runner if the account has one, which would also execute the pin on ARM64 every push.
Confidence: High on the gap; Medium on runner availability.
```

```
[M13] [R5 R6 R4 L] [Major] [Feasibility]
Claim: ADR-024's "on the order of ten million integer operations a second, which is nothing" is two orders of magnitude off at 100 clients: the accumulator alone costs the whole 50 ms tick, while at two and four players it costs nothing.
Evidence: ADR-024 Consequences; GameLogic/Accumulator.cpp (every entity's record is built twice per client per tick, once in the removal scan and once in scoring, and all candidates are fully sorted); harness B (appendix B): the real Fill costs 51.5 to 53.4 ms a tick at 100 clients × 5,500 entities on this VM (removal scan 9.9 ms, scoring 17.9, sort 25.4), 16 µs at two players and 77 µs at four; records built once for all clients plus a top-K select brings it to 11.1 ms with the sent order byte for byte identical (0 of 32,000 records out of order at four players).
Scenario / proof: Fact for the figures, inference for the device: MSVC on a Snapdragon differs by a constant, not an order. Reviewer 5 wants the forty-line fix now; Reviewers 6 and 4 want the 100-player target and its five owed hardware runs descoped because none of it is on M3.11's path. Both hold.
Why it matters: The 100-player target ADR-024 was written for cannot hold 20 Hz single-threaded as built, and M1.14c's owed "accumulator cost per client" will report it; but it is a stress risk, not an MVP risk.
Recommendation: Scope the stress target to eight seats for the MVP (ADR-024's measurement 4 "at 2, 4 and 8"; M1.14b's owed runs reduced to one seated-and-acknowledged run of players only, which from a laptop over Wi-Fi is also M0.5's wireless measurement), record the 51 ms figure in ADR-024 as the reason, and move churner, flooder, ceiling and the fix to a post-MVP scale list. Take the fix whenever the accumulator is next touched: a BeginTick that builds the records once, identities compared from it in the removal scan, a lean candidate of {slot, due, age, score}, and nth_element then sort of the top UPDATES_PER_TICK × MAX_RECORDS_PER_UPDATE, which keeps the same strict total order and every AccumulatorTest. Alternative: fill clients on worker threads, which would be the first thread in GameLogic and rests the sent-set property on no shared state; the first fix is better.
Confidence: High on the figures; the device changes the constant.
```

```
[M14] [R4] [Major] [Coherence]
Claim: A five-minute match is about four real decisions and forty-plus chore commands, because the one-slot queue needs a tap per item with the station on screen and the panel open, every spawned miner needs a hand-issued mine order, and the fixed spawn point stacks idle ships into one silhouette that hides how many the enemy has.
Evidence: Harness: the best opening issues 42 host commands in 300 s (34 builds, 4 mine orders, 2 placements, 2 upgrades), one every seven seconds, and beats the alternatives, so the module row is a sequence to execute; BuildSystem.h (no queue, Q21); App.cpp (the panel opens on a station tap and closes on any ship selection or clear; a hold recenters on the station only); BuildSystem.cpp (a new ship sits at one fixed spawn point with no order); Interface §1 and §6 (the 96-pixel "under fire" tier exists because build buttons are hit while something is exploding; a tap on any live button replaces the item in progress); design_handoff_hud (the HUD may read no enemy counts); a Frigate is 5.3 pixels at the tactical zoom.
Scenario / proof: Fact for the counts, inference for the gestures: a player who has just sent the fleet must, every seven to fifteen seconds, hold to recenter, tap the station, tap FIGHTER and pan back, three to four gestures per item, about a hundred a match, during which a mis-tap on MINER replaces the fighter in progress; the real decisions are miners-before-fighters (solved), module order (solved), where to park fighters and when to attack, one per 75 s. Both players keep fighters at home and each sees the other's base as one dot in front of the station; neither can tell three from thirty until the enemy moves, so the attack decision is a guess and the twenty matches cannot separate "the raid arithmetic is wrong" from "the attacker guessed wrong".
Why it matters: Twenty matches of this measure a player's tolerance for a build chore rather than §7, a tier sized for taps under fire is sized for a tap the game should not need, and the no-fog premise that justified cutting attack-move and the minimap is defeated silently by a stack.
Recommendation: Three small builds, none touching the wire. (1) Client-side repeat: a double tap on a build button (Tapped with a count, no new verb) arms repeat, and the client re-sends Build when its own block shows the slot empty and credits cover the cost, exactly as BotPolicy::Decide already does. (2) Auto-mine at spawn: BuildSystem::Advance issues OrderMine to the nearest rock with ore (M3.9's retarget function, identity tie-break) for any design with oreCapacity > 0; the rally §5 declined, for miners only; fighters spawn idle and auto-engage, which is the right default. (3) Spawn on ring slots: SpawnPoint adds RingSlotOffset(builtCount mod 7, 90) around a point 245 units in front of the station, all inside point defense and clear of the hull, so a stack reads as a disc for both sides and own-ship picks stop being ties. Alternative: a host-side four-deep queue (eight bytes in the player block, costed with DatagramBudget.py) plus a rally tap, and a client-side count badge at the projected centroid of three or more same-owner records within 24 pixels.
Confidence: High on the counts; Medium on the gesture overhead, since nothing M2 drew has been touched on the device.
```

```
[M15] [R4 R6] [Major] [Coherence] (an explicit challenge to pillar P5's mechanism)
Claim: Twenty human-against-stub matches cannot answer §7's balance questions, because the stub as planned never defends, escorts or raids modules, so every raid is a slaughter and every siege meets a defender who always responds; the arithmetic questions M3.11 asks are answerable by the deterministic harness in minutes, and the human matches are needed for the questions arithmetic cannot answer.
Evidence: Plan/M3-the-fight.md:322-323 (the stub "builds miners, mines, builds fighters and eventually attacks"); GameDesign §8 (M4's AI adds no defense either); M3.11's questions 1 to 3 are raid exchange, safe-zone size and match length; Plan/M1-the-fleet.md:821 (the owner chose two snapped windows on one device for two clients, one person on both sides); GameDesign §10 ("the only mechanism this project has for turning the balance questions in §7 into answers rather than opinions"); the shipped simulation runs a five-minute match in milliseconds on Linux.
Scenario / proof: Fact for the stub's specification, inference for the outcomes: a human raids the stub's miners, nothing flees (M1) and nothing responds, so question 1 reads "the raid table is too strong"; the stub attacks the human, who always responds, so question 2 reads "the safe zone is too safe"; both answers are properties of the stub.
Why it matters: The design's stated reason for the reduced MVP is the balance answers; against a non-defending stub they are opinions with a sample size, and the harness that produced every number in this review already exists.
Recommendation: Split M3.11 into (a) a harness gate, twenty scripted matches on the Linux or CI build (rush against idle, mirror, three fighters against six fleeing miners, a module raid at the standoff) whose figures are written into §7 and re-run whenever a constant moves, ported into GameLogicTests as a test that logs its curves through Logger::WriteMessage as M0.9 did for the snapshot size; and (b) five human matches for the questions arithmetic cannot answer: question 0's alert habituation, tap precision, orbit, whether the panels read; and give the stub M9's defend reflex so the human meets a defender. Alternative: two humans on two devices for the twenty, accepting that the one-person configuration answers nothing about balance.
Confidence: High that the stub as planned cannot answer §7; Medium on the split, which depends on how the owner weighs feel data against balance data.
```

```
[M16] [R6] [Major] [Scope]
Claim: M3's step order puts the two steps that make a match exist, restart and the stub AI, at positions nine and twelve, and the step with the most open design, finite ore, before them, so the first match happens after twelve steps instead of seven; M3.3's wire half already exists.
Evidence: Plan/M3-the-fight.md (M3.8 is step 9, M3.10 step 12; "Leaving M3: one person can play a match against a stub AI, straight into the next one"); GameCore/Update.h and Accumulator.h (FireEvent, MAX_FIRES_PER_UPDATE, FIRE_REPEAT_TICKS, NoteFire, seventeen test references), so M3.3 is a client-only step; ADR-013 (BeginMatch "empties the world, drops every seat and places the layout, which is what a restart wants").
Scenario / proof: Fact. Ordered M3.0 → M3.1 → M3.2 → M3.4 → M3.7 → M3.8 → M3.10, a playable, restartable match exists after seven steps, about a day of agent time at M2's rate, and the raid content (M3.3, M3.3b, M3.5, M3.6, M3.8b, M3.9) lands while the owner plays the first matches and answers B3's length question and B1's economy question on the first evening rather than the last.
Why it matters: The two findings that can invalidate §7 are answered a week earlier and before M3.1's tests are pinned to them.
Recommendation: Reorder M3 as above, and M4 as M4.5 → M4.6 → M4.1 (verify at four) → M4.8 as one human and three AI → M4.4 and M4.7 only if M11's turning is in → M4.3 with Bot players as the extra clients. Alternative: keep the order but pull M3.8 and M3.10 to positions three and four.
Confidence: High.
```

```
[M17] [R6] [Major] [Scope] (a tension with AGENTS.md §6 and the design-consistency skill, stated rather than resolved)
Claim: The documentation discipline costs about a fifth of each step's lines and an owner hardware run per pin move, and five documents carry the same status paragraph; the one-second scripts are worth keeping and the essays and the per-pin-move run are not.
Evidence: git numstat (M2.6: Design 122 / Tests 475 / Code 545 lines; M2.11: 135 / 693 / 878; M2.7: 106 / 357 / 392); ten of 78 non-merge commits are documentation syncs; the status is stated in README.md, AGENTS.md, Design/README.md, Plan/README.md and each milestone file; Design/ is 6,145 lines of which Plan/ is 3,371; CheckDesign.py's manifest has sixteen rows, eight of them retired sky and mesh figures; Plan/README.md says the four-pair run is owed "at every milestone boundary" while M2.6 and M2.7 say "owed again" per pin move, and M3 moves the pin at five steps or more.
Scenario / proof: Fact for the counts. Each M3 step would write a thirty-to-sixty-line annotation across three to five documents, and each pin move would ask the owner for four builds and four test runs on the Surface, five times in M3.
Why it matters: The owner's runs are the bottleneck (B6); the essays are read by nobody before the next step, and drift is what the one-second script catches anyway. This review would not exist without the discipline that produced the documents it reads, so the recommendation cuts the cost, not the practice.
Recommendation: One status paragraph (Plan/README.md's "Where it stands") with the other four documents linking to it; a step's annotation is the six-line report Plan/README.md already defines; the four-pair run at milestone boundaries only, with M2's "owed again" lines made to cite that rule; keep CheckDesign.py and stop adding manifest rows for withdrawn features. Alternative: keep the essays in a per-milestone "as built" file so the plan files read as plans.
Confidence: High on the counts; Medium on "a fifth", which is lines rather than time.
```

```
[M18] [R2] [Major] [Coherence] (M4-scoped; rated Major because M4.7 is a human gate whose question the current row pre-answers)
Claim: The Cruiser at 2,400 credits loses to an equal cost of fighters before speed is considered, moves at 62 units a second and not §7's 50, and is 28% to 69% of a five-minute match's whole economy, so M4.7's "does speed counter mass" cannot be answered by the row as it stands.
Evidence: Catalog.cpp (mass 60, cost 2,080), DerivedStats.cpp (5,600 / (60 + 10 + 20) = 62; Q46's table says 62); GameDesign §7 ("a Cruiser at 50"; MassDriver against Large 25, against Medium 60); §6 ("2,400 credits, 160 seconds of total income"); Plan/M4-the-opponent.md (M4.4, M4.7); harness totals of 3,500 to 4,600 credits a side under Q26's ore and 5,300 to 8,700 under B3's set.
Scenario / proof: Fact, arithmetic: four MassDrivers give 100 dps × 0.6 = 60 dps against a fighter, a kill every ten seconds; eight fighters (2,400 credits) deal 8 × 12.5 = 100 dps against Large, so the Cruiser dies in 30 s having killed three (900 credits), with nobody kiting; it crosses 12,000 units in 194 s and costs 120 s of slot, so in a five-minute match it cannot reach the enemy even if ordered at 0:00.
Why it matters: M4.7 costs real matches and would return "mass loses", truly, about the damage table rather than about speed.
Recommendation: M4.4's weapon row, a HeavyDriver at 50 dps per mount with modifiers Small/Medium/Large 40/100/60: four mounts give 200 dps against fighters (eight die in 24 s having dealt 2,400 < 3,000; twelve, 3,600 credits, kill it in 20 s losing seven), which makes numbers and kiting at 140 against 62 the counters M4.7 wants to test; keep 2,400 for the four-player format §2 describes and correct §7's 50 to 62. Alternative: keep the MassDriver row and price it at 1,200 (hull 880): it then beats four fighters and loses to eight, and one appears per five-minute match.
Confidence: High on the arithmetic; Medium on the weapon numbers (any row with 160 dps or more against Medium at 2,400 passes the same test).
```

### Minors

The format is the same; the fields are shortened.

```
[m1] [R2] [Minor] [Logic]  (cheap and worth fixing now)
Claim: A tap on an unaffordable build button cancels the item in progress, so the "save up" state Interface §6 draws as a lit button with a reddened cost destroys up to twenty seconds of slot on a mis-tap.
Evidence: BuildSystem.cpp Commit (refunds the current item first, then "item = BuildItem{}; return Unaffordable" when the balance does not cover the new cost); Interface §6 ("an item you cannot yet pay for keeps its button lit").
Scenario: A Miner at 140 of 150 ticks with 100 credits banked; FIGHTER is tapped; refund 150 → 250 < 300; the miner is cancelled and nothing builds. The bank sits at 100 to 200 credits for the whole first minute in every harness table.
Why it matters: The one place the interface says "save up", the host answers "cancel".
Recommendation: In Commit test credits + refund ≥ cost before touching the item and return Unaffordable leaving the current item running (three lines; Q35's refund-first order survives for the affordable case). Alternative: dim, not redden, a button whose cost exceeds credits plus the current item's refund, on the client.
Confidence: High on the code; the frequency is inference.
```

```
[m2] [R5 R1 L] [Minor] [Logic]
Claim: The client forgets an entity after three sweeps, which is 150 ms at the MVP, against a link-loss window of 1,000 ms, and M3.4's criterion ("never evicted from absence") contradicts it.
Evidence: ReplicaStore.cpp, Update.h (SweepTicks, FORGET_AFTER_SWEEPS), ClientFrame.h (LINK_SILENCE_MILLISECONDS); measured twice: any silence of three to nineteen ticks forgets exactly the ten entities that rode the tick's second datagram and re-adds them from nothing in the same drain (a one-frame pop instead of a hold); if that datagram is also lost they vanish for a tick.
Recommendation: forgetTicks = max(FORGET_AFTER_SWEEPS × sweep, LINK_SILENCE_MILLISECONDS / TICK) in ReplicaStore::Accept, client only, one test; wrecks and selection eviction from removals only; reword M3.4 to "never treated as dead by absence inside the forget horizon". Alternative: forget only after Clear() during a rejoin and drop the rule, since a removal is lost ten times in a row once in 10¹³ updates.
Confidence: High.
```

```
[m3] [R1 R5] [Minor] [Logic]
Claim: Pending removals are served first-in-first-out under the cap of 48, so a 55-entity elimination leaves the last seven ships as ghosts for 500 ms before their first removal is sent.
Evidence: Update.h (MAX_REMOVALS_PER_UPDATE 48, ten repeats), Accumulator.cpp (the first 48 in list order; only those sent decrement); measured: 48 delivered per update from tick 6 for ten ticks, the remaining seven first sent at tick 16, the last completing at tick 25.
Recommendation: After sending, rotate the sent prefix to the back so pending removals round-robin (every removal's first send within ⌈pending/48⌉ ticks); one line, one test. Alternative: raise the cap to 64 (the record floor drops 65 → 61) and re-run the budget script.
Confidence: High.
```

```
[m4] [R3] [Minor] [Logic]
Claim: Ring slots are clamped to the play area one destination at a time, so a fleet ordered near an edge collapses onto the edge line and stacks, the outcome Q19 introduced rings to prevent.
Evidence: RingAssignment.cpp (ClampToPlayArea per slot); RingAssignmentTests asserts "inside" only; measured: fifty fighters ordered to the corner get 32 distinct destinations, to (8192, 0) 46, elsewhere 50.
Recommendation: Clamp the target inward by the outermost ring's radius before assigning slots so the ring stays whole against the wall. Alternative: assert distinctness in the corner test and document the stack.
Confidence: High.
```

```
[m5] [R3 L] [Minor] [Logic]
Claim: The intake acknowledges an Attack without resolving or ownership-checking its target, so M3.2 inherits a command whose target may be own, dead or nothing, and an Attack on your own miner becomes a free follow verb if pursuit is implemented literally.
Evidence: CommandIntake.cpp (the Attack branch reads nothing; TargetEntity is never called) against the resolve-and-owner-check every selection identity gets; Interface §4 issues Attack only on a hostile, so today only the Bot or a modified client reaches it.
Recommendation: In M3.2 resolve TargetEntity with ResolveWireIdentity, refuse and still acknowledge an unresolved or own target, add both cases to CommandValidationTests. Alternative: make own-target Attack a designed "follow" verb in Interface §4.
Confidence: High on the gap; Medium on the exploit.
```

```
[m6] [R1] [Minor] [Coherence]
Claim: M3.8b's exit criterion "killing a shipyard slows the queue already running" cannot be met, because the build system bakes the multiplier into ticksRequired when the item starts and never re-reads it.
Evidence: CommandIntake.cpp ("an item keeps the rate it began at"); BuildSystem.cpp (ticksRequired fixed at Start, one tick per tick); Plan/M3-the-fight.md:274-275.
Recommendation: Make progress rate-based (progressHundredths += rate × multiplier per tick, complete at cost × 100; Q56's round-up survives as "completes on the tick progress reaches cost"). Alternative: rewrite M3.8b and §5 to "the rate is fixed when the item starts".
Confidence: High.
```

```
[m7] [R1] [Minor] [Logic]
Claim: Nothing in the build system, economy or intake knows about elimination or a module's death, so a dead player's item keeps ticking and an upgrade of a destroyed module is shown building until it completes.
Evidence: BuildSystem.cpp (a stranded item is refunded only on completion; the upgrade target is checked only on completion); measured: a module destroyed at tick 11 is still shown upgrading at 3% and refunded at tick 300.
Recommendation: Check the upgrade target every tick in Advance and refund the tick it dies; add BuildSystem::Eliminate(player) for M3.7 to call. Alternative: leave it and let M3.7's removal of the player's ships make the refund moot.
Confidence: High.
```

```
[m8] [R4] [Minor] [Coherence]
Claim: The hit-value curve is dead content at the MVP by the design's own construction (300 reproduces the Large column exactly), so M3.1 would build and test a second mitigation model that changes no number.
Evidence: GameDesign §7 ("the formula changed and the balance did not"; the cost of two models named at §7:422-428); Catalog.cpp (Station and ModuleFrame already Heavy; no armour component); Plan/M3-the-fight.md M3.1 asks for both models pinned against each other.
Recommendation: M3.1 implements the size-class table only, keeps hitValue in the catalog as data, and adds the curve with the first component that changes it. Alternative: keep both and accept the clarity cost §7 names.
Confidence: High.
```

```
[m9] [R4] [Minor] [Logic]
Claim: The hold recenters on the station unconditionally rather than "on the selection, or on your station when nothing is selected", so with something selected, the normal state, the only way back to your own fleet is panning.
Evidence: App.cpp ("nothing is selectable yet, so it goes to this player's station") against ADR-018 decision 7 and Interface §3 and §5.
Recommendation: Implement the selection half: focus = the centroid of the selected records' newest samples, zoom unchanged. Alternative: tapping a selection-panel group recenters on that group.
Confidence: High.
```

```
[m10] [R4] [Minor] [Coherence]
Claim: With two module kinds and best-of-kind effects, only two of the four module slots can ever do anything; the other two are 350-to-700-credit decoys, and the "past the cap" unavailable state is reachable only by building them.
Evidence: ModuleSite.h (cap 4), ModuleEffects.h (best of kind), GameDesign §5 (two kinds), Interface §6 (dimmed past the cap).
Recommendation: State it in §5 as a decoy ("a second module of a kind adds hull and nothing else") and keep the cap. Alternative: cap at one per kind and delete the unreachable state.
Confidence: High.
```

```
[m11] [R2 L] [Minor] [Balance]
Claim: With no per-rock throughput, stacking miners on the best rock beats spreading by 21% to 46% of income, and Q26's "ten rocks keeps six miners from queuing on one" names a rule that must be written or deleted.
Evidence: MiningSystem.cpp (extraction is per miner, no per-rock state); Generator.h:37-38; measured: ten miners on the two nearest rocks earn 66.5 a second against 45.6 spread over ten.
Recommendation: One extractor per rock per tick, a per-rock flag in MiningSystem::Advance in slot order; a second miner in range holds in Extracting with no cargo change; a rock then yields at most 20 ore a second and ten rocks serve about thirty miners, which makes Q26's sentence true. Alternative: delete the sentence in Q26 and Generator.h and accept stacking, which finite ore turns into a timing difference only.
Confidence: High.
```

```
[m12] [R2] [Minor] [Scope]
Claim: The "50 ships a player" fleet cap is a sizing assumption, not a rule: nothing refuses a 51st ship, and the shipped economy reaches forty miners by 300 s under infinite ore.
Evidence: World.h (65,536 slots), BuildSystem.cpp (no count check), Update.h (the sweep is computed from the live count), ADR-004 Measurements.
Recommendation: BuildSystem::Start refuses at OwnedCount ≥ 50 ships (modules excluded) with a FleetFull rejection dimmed in the panel like the module cap. Alternative: if M3.9 lands first, record in §10 that 50 is a budget and not a rule.
Confidence: High.
```

```
[m13] [R5 L] [Minor] [Coherence]
Claim: Four decisions are taken in code that the documents treat as open or unstated: three players are allowed without --stress on an admittedly unfair layout; the record's three state bits are "not settled" yet audited as five drawn states; research at M4 needs a per-player unlock byte no document budgets; the grid is rebuilt inside MiningSystem::Advance while TechnicalDesign §2 runs weapons before mining; and the endpoint timeout ADR-013 deferred is still absent.
Evidence: Host.h (PlayerCountAllowed 1 to 4), Layout.h; EntityRecord.h, Accumulator.cpp (always writes 0), DatagramBudget.py; Design.h (buildable is a constexpr flag), Panels.cpp (caches BuildableDesigns in a function-static); MiningSystem.cpp:48; Host.cpp.
Recommendation: Refuse 3 without --stress; define the state bits in ADR-024 (idle, moving, mining, fleeing, engaged lets M3 draw flight with no wire change) or drop the audit row; add "research needs an unlock byte per player" to TechnicalDesign §4 so it is budgeted with M8's header bytes; hoist the UniformGrid into Host, rebuilt once after movement and passed to both systems. Alternative for the player count: document three as a permitted unfair practice count in §2.
Confidence: High.
```

```
[m14] [R5] [Minor] [Feasibility]
Claim: Every tuning number is constexpr in GameCore by decision, so each figure M3.11 moves costs a solution rebuild and a redeploy of both binaries, including rows only the host reads.
Evidence: Catalog.h, ADR-021, TechnicalDesign §7; the damage table and the cadence are read by the host alone (the client draws hull percent from the record).
Recommendation: Keep the tables constexpr where the client previews them, but let Server --tuning override the host-only rows (damage modifiers, hit values, cadence, point-defense range) at BeginMatch, print the override's hash beside the seed, and give the determinism test the same override so a replay is seed plus tuning (about eighty lines). Alternative: accept the rebuild and say so in M3.11's text.
Confidence: Medium.
```

```
[m15] [R5 R1] [Minor] [Coherence]
Claim: Three premises in the documents are stale: TechnicalDesign §9.6 and Interface §7 still plan to measure "an unbatched quad per glyph" although the interface is one instanced draw since M1.13; World.h says Create takes "the lowest-numbered free slot" while the store is last-in-first-out, which is the determinism blind spot whose documentation must not be wrong; ADR-013's reseed detector ("the tick going backwards") cannot fire (M8).
Evidence: TextRenderer.cpp (one DrawInstanced for every plate and glyph); World.h:79 against World.cpp and TickTests' "the free list is taken from the end it was pushed on"; Host.cpp.
Recommendation: Strike the glyph premise and keep the world-against-interface GPU measurement; change World.h's sentence to "the most recently freed slot, else a fresh one"; fold the detector into M8's decision.
Confidence: High.
```

```
[m16] [R6] [Minor] [Scope]
Claim: M3.11's question 0 (does the alert fire too often, and do you act on it) will be answered as a feeling unless the client counts alerts, and one WIP commit reached main through a pull request against the plan's bisectability rule.
Evidence: ADR-020 Measurements ("none yet", "played rather than computed"); README.md (probe-log.txt is where every device figure comes from); git (2d17148 "WIP: ADR-024 replication, budget script, design edits", 21 files, merged through PR #14; M1.14c's four parts in one commit, stated in the plan).
Recommendation: M3.3b writes per match to probe-log.txt: alerts raised, suppressed on screen, merged, tapped, and time to tap, so question 0 becomes "tapped ÷ raised ≥ X" with X on the register; one sentence in AGENTS.md §6: squash WIP commits before merge. Alternative: a counter in the system panel during M3.11 only.
Confidence: High.
```

---

## 4. Recommendations, sequenced

Impact is what it does for M3.11's answers or the project; effort is agent time (S: under a day, M: one to
three days, L: more) plus the owner's decision where one is needed. Nothing here is a redesign: every
item is a number, a rule, a plan change or a small code change against the game as designed.

### Fix now, before more code lands on top

| # | Change | Finding | Impact | Effort |
|---|---|---|---|---|
| 1 | Delete the free Fighter (`Server.cpp:146-149`); assert stations-only through the host's real path | B5 | H | S |
| 2 | Salt the session-token stream with a per-host-run nonce; retire the "same tokens" test; rewrite ADR-013's paragraph | B4 | H | S |
| 3 | Re-derive the economy from the code: home field 1,200 to 2,000; the measured income table onto Q26; §4, §5, Q47 and the three code comments restated; decide the per-rock throughput rule | B1, m11 | H | S code, M docs |
| 4 | Decide the safe-zone geometry (default: point defense 480 and far-side flight and unload) and delete or make true the "which side you build on" sentence | B2 | H | S |
| 5 | Flight as a phase of the mine order, with range re-checked in Extracting and Unloading | M1 | H | M |
| 6 | Rule ADR-014 as a hundredths accumulator, deaths after every weapon, fire events thinned to one per shooter per ten ticks; pin §7's rows as tests | M2 | H | S decision, M code |
| 7 | Skip dead identities at intake and acknowledge every refusal past the sequence check | M4 | H | S |
| 8 | Resend unacknowledged commands from the packaged client (port BotPolicy's pattern into ClientFrame) | M5 | H | M |
| 9 | An unaffordable tap leaves the current item running | m1 | M | S |
| 10 | The three-class gate rule in Plan/README; the four-pair run once per milestone boundary; M3 reordered so a restartable match exists after seven steps | B6, M16, M17 | H | S |
| 11 | Fold hull points, cargo, phase, credits and build items into the state hash; move the pin once; amend ADR-002 | M6 | H | S |

### Fix before M3 is feature-complete

| # | Change | Finding | Impact | Effort |
|---|---|---|---|---|
| 12 | The match clock at six minutes with the hull-then-value tiebreak, and the draw rule | B3, M7 | H | S |
| 13 | M3.9 as host-side finite ore, husk and retarget with B3's ore set and no wire change; decide the forward unload point (default: yes, as a fifth placed design) | M10 | H | M |
| 14 | Rule Q48 now; the stub decides over wire-shaped records through the command path, in the file M4.5 extends, with one defend reflex; AI seats reserved | M9 | H | M |
| 15 | Keep seats across BeginMatch; match generation and winner in the update header; the client re-joins on a generation change and shows the result | M8 | H | M |
| 16 | The attack order as pursue-to-standoff with an arc layout | M3 | H | M |
| 17 | Client-derived facing from motion; strike "arc" from ADR-004 and M3.2 for the MVP | M11 | M | S |
| 18 | The field hash in the join reply; Debug|ARM64 compile of the two simulation suites in CI | M12 | M | S |
| 19 | Build repeat by double tap, auto-mine at spawn, spawn on ring slots | M14 | M | M |
| 20 | Split M3.11 into a harness gate and five human matches for the feel questions; port the economy and rush scripts into GameLogicTests | M15 | H | M |
| 21 | The small fixes bundle: forget horizon, removal round-robin, ring clamp inward, attack-target validation, elimination and dead-upgrade handling in the build system, the selection half of the hold, alert counters, the stale sentences | m2 to m7, m9, m15, m16 | M | S each |

### Defer, with the reason written

| # | Change | Finding | Impact | Effort |
|---|---|---|---|---|
| 22 | Scope the stress target to eight seats; take the accumulator fix when it is next touched | M13 | L for the MVP | M |
| 23 | The Cruiser's weapon row and the 50 → 62 correction, at M4.4 | M18 | M at M4 | S |
| 24 | Orbit's kill switch, decided by the first human matches | M11, ADR-018 | L | S |
| 25 | Host-only tuning overrides for the evening's loop | m14 | L | M |
| 26 | The size-class table alone at M3.1, the curve with the first armour component | m8 | L | S |
| 27 | The decoy sentence or a one-per-kind cap; the fleet cap as a rule; three players refused | m10, m12, m13 | L | S |

---

## 5. Validation plan

Five experiments, each cheaper than an evening, each with a pass criterion. The first three run on the
Linux build of the unmodified simulation that produced this review's figures; porting them into
`GameLogicTests` makes them run on every push.

| # | Experiment | Resolves | Pass | Fail |
|---|---|---|---|---|
| V1 | **The economy script**: 300 s of scripted openings through the real BuildSystem and MiningSystem at candidate constants, logging income and fighters at 300 s | B1, m11 | six miners earn 15 to 20 credits a second (under the slot) and at least four openings finish within 25% of the best | one opening dominates by more than 25%, or income at six miners exceeds the slot |
| V2 | **The scripted rush and mirror**, the day M3.2 lands: three fighters against an idle defender; both sides on the best opening attacking at ten fighters | B3, M3, M7 | the rush ends by 5:30; the mirror ends by 8:00 or on the clock with a winner; no hang on simultaneous death | no result by 10:00, or a draw the code cannot end |
| V3 | **The raid geometry**: three fighters at the chosen standoff against six miners on the remapped field, with the chosen flight rule | B2, M1 | two to four miners survive (§7's "about 3½") and the point defense fires at least once | zero or six survive, or the point defense never fires |
| V4 | **The field hash across platforms**: the join reply's field hash from the ARM64 client against the x64 host over 200 seeds, and one four-pair determinism run at M3 entry | M12, M6 | every seed matches; the pin holds on all four pairs | any mismatch, which is the desynchronization class R23 warns of |
| V5 | **The offline silhouette raster**: a top-down orthographic raster of the handoff's OBJ meshes at 17.1 units per authored pixel, sorted blind | M2.13 (pre-empted) | Scout against Frigate and shipyard against ore processor separate in 20 crops | they do not, and the overlay ADR is written before M3 rather than after a device session |

A sixth, once M3.2 exists: count fire events per minute in the scripted fight and size the alert's
cluster thresholds so alerts stay under about two a minute (M3.11's question 0, m16).

---

## 6. Decisions the designer must make

Each with the reviewers' recommended default. None of these can be taken by an agent.

| # | Decision | Default | Alternatives on the table |
|---|---|---|---|
| D1 | The safe zone's geometry | Point defense 480, flight and unload to the far side, with B1's radii | Point defense 800 at a 30% Medium modifier (no near-side standoff); or delete the side claim and test the zone as a radius only |
| D2 | The economy's binding constraint | Move the home field to 1,200 to 2,000 and keep the slot at 20 | Keep the field and raise the slot to 30 |
| D3 | Finite ore, and the forward unload point | Host-side finite ore with 200 per home rock and 600 per contested rock; a fifth placed design as the forward unload point, the one addition this review makes against Scope's cut list | Move the clusters instead and say they are contested by nobody; rocks as entities if the panel must show ore |
| D4 | How a match ends | A six-minute clock with the hull-then-value tiebreak, plus the draw rule | A softer station (5,000 HP or hit value 100) and a shorter map (anchors at 4,500), with §7 restated |
| D5 | The firing cadence (ADR-014) | A hundredths accumulator per mount per tick, deaths after every weapon has fired, events thinned to one per shooter per ten ticks | 1 Hz integer shots with §7's rows restated at +4% to +9% |
| D6 | Turning and facing | Client-derived facing now and 360° weapons; host turning with a derived turn rate if the Cruiser returns at M4.4 | Strike "bank as they turn" for the MVP and move M4.4 and M4.7 to post-MVP |
| D7 | P5's mechanism | Split M3.11: the harness answers the arithmetic questions, five human matches answer the feel questions | Twenty human matches on two devices |
| D8 | The restart's wire | Two header bytes (match generation, winner), seats kept across BeginMatch | A MatchEnded packet repeated ten ticks |
| D9 | The AI's shape (Q48) | The stub decides over wire-shaped records now, in the file M4.5 extends | Host-side with a read-only view type and the twenty-tick interval pinned |
| D10 | The stress target | Eight seats for the MVP; the accumulator fix when convenient | Keep 100 with the fix taken now |
| D11 | The documentation discipline | One status paragraph, six-line step reports, the four-pair run per milestone boundary | As it is |
| D12 | The gate classes | B6's A, B and C | One consolidated "confirmations owed" table |

---

## 7. Protect list

What is working, found sound by more than one reviewer, and must not be broken by the fixes above.

- **The linker-kept boundary**: the client links no `GameLogic`; the placement preview, the split-for-mine and the field all come from `GameCore`, and no simulation rule is duplicated on the client. Every recommendation above that touches the client (repeat, facing, resend) is presentation or transport, not simulation.
- **The determinism practices**: identity-ordered ties everywhere a query reaches an outcome, the last-in-first-out free list with the generation advanced at death, one PRNG stream per consumer, the field-by-field little-endian hash, the scripted match that grows with every system, and the four one-second gates in CI. Widen the hash (M6); do not change how it walks.
- **The composition model**: modules fell out of the catalog at M2.9 with the derivation untouched, the miner and the station needed no special case, and the Cruiser stays a row. The depot of M10 is a row too.
- **The mining loop's shape**: a standing order that re-issues work inside the pass, phase changes on the tick they are earned, cargo that outlives the order, and the unload target as a query, which is what makes a forward unload point a table row.
- **The build system's rules**: cost at start, full refund on cancel and replace with the refund before the check, ticks rounded up, at least one tick, exact remainders in the economy, the best-of-kind module effect (stacked L2 shipyards would give 80 credits a second).
- **Command validation**: the wrap-safe sequence, ownership, the selection bound at the owned count, resolve-before-act, oldest-first fill, the host resolving the sender from its session, the client adopting the applied sequence after a relaunch.
- **Replication at MVP scale**: the twelve-byte record, one whole datagram at a time, the sweep guarantee, the deterministic sent set, removals found by looking, the per-entity tick ordering, and the second update per tick that refreshes everything every tick at 110 entities. M13's fix preserves every one of these byte for byte.
- **The join**: token, then endpoint, then the lowest free slot; a refusal is a reply; the lost-reply window is closed. B4 changes only where the token stream is seeded.
- **The generator**: integer-only on its own stream, exact quarter-turn copies, bounded attempts, the field index as the rock's name.
- **The touch model**: one plane, one tap is one order, the tap-slop asymmetry, the pick order with the interface first, the double tap that acts on the first tap, no band select, no subset selection, the order marker as presentation and never prediction.
- **The register discipline** ("asked and ruled before the code") and the one-step-one-commit protocol with the agent never marking a gate done; and the Bot's linkability into an unpackaged process, which is the cheapest second seat and the cheapest wireless probe this project has.
- **What already exists for M3**: the fire event, the repeat counts and `NoteFire` (M3.3's wire half), the removal path, `BeginMatch` as a restart, and the sky.

---

## Appendix A. Method, and what was cross-examined

**Phase 0.** The Lead read every document under `Design/`, `AGENTS.md`, the three skills, the six libraries' simulation and client code, the tests and the git history, ran the four static gates (all clean at HEAD), compiled `GameCore` and `GameLogic` unmodified under g++ 13 on Linux, and wrote the map and the tuning table before any critique. Eleven deviations from the designer's own status were found there and are D1 to D11 in the map.

**Phase 1.** Six specialists reviewed in parallel and in isolation from the map and the source: rules and systems integrity, economy and balance, an adversarial player, core loop and coherence, technical feasibility, and scope and delivery. Each compiled its own copy of the harness; their unedited reports are in the review folder, and the synthesis above supersedes them where they disagree.

**Phase 2.** The Lead checked every Blocker and Major against the code before accepting it: the send-once client (a grep of the four send sites and the marker rule), the dead-identity refusal (the intake's order of checks), the token collision (the admit logic, and two independent reproductions), the restart path (the fields BeginMatch resets), the hash's fields, the standoff geometry on the real field, the fire-event bound, the match-length floor, the ring arithmetic and the forget rule (the Lead's own experiment). Findings that several reviewers reached from different directions were merged under one root cause (the flight condition, four reviewers; the hash, two; the token collision, two; M3.9's two specifications, two; the removal cap, two; the economy, three). Severity was recalibrated to one scale.

**Where the reviewers disagreed, stated rather than averaged:** the accumulator (fix now against defer, M13); M3.9 (host-only against rocks-as-entities against a forward unload point, M10); the restart ("falls out of what is built" against "cannot be built on this wire", M8); more options (the depot) against fewer (the cut list), decided as D3; the P5 mechanism (M15) against `GameDesign.md` §10's "only mechanism"; the documentation discipline (M17) against `AGENTS.md` §6.

**What this review could not do.** Nothing was built with MSVC or run on the device; every host-cost figure is an order of magnitude from a cloud VM; combat figures assume the hundredths cadence unless stated; the reviewers did not play the game, because it cannot yet be played.

## Appendix B. The figures

All measured in this session on the unmodified simulation unless marked arithmetic. Seed 20260922, two players unless stated. Sources and raw outputs are in the review folder.

**Income per miner, steady state, one Miner, no processor**

| Rock | Distance from the anchor | Credits a second | First delivery |
|---|---|---|---|
| nearest home (index 0) | 797 | 6.40 | tick 289 (14.5 s) |
| typical home | 1,029 to 1,223 | 4.00 to 4.67 | 372 to 502 |
| farthest home (index 8) | 1,496 | 3.33 | 560 |
| contested (index 10 to 21) | 4,509 to 5,858 | 0.67 to 1.33 | 1,769 to 2,310 |
| nearest home with OreProcessorL1 / L2 | 797 | 8.00 / 9.60 | |
| remapped field, 1,200 to 2,000 | | 2.67 to 3.70, mean 3.10 | |

**Openings at 300 s, shipped field, one build slot at 20 a second** (fighters at 300 s, credits banked): three miners, no shipyard, 18 and 150; six miners with ShipyardL1 then L2, 30 and 600; six miners with the shipyard and OreProcessorL1, 28 and 2,646; twelve miners, no shipyard, 14 and 13,600; miners built back to back and all sent to the nearest rock, 40 miners, 233 credits a second of income and 30,165 banked. Four spread miners earn 21.7 a second. On the remapped field five openings finish within 25% (14 to 19 fighters).

**Finite ore** (nearest-with-ore retargeting over the unmodified loop): 200 ore per home rock is dry at 106 s with eight miners, 112 with six, 141 with four; 300 at 159 s, 600 at 277 s with six; the slot then idles 85 to 188 s of the match. On the remapped field 200 per rock is dry at 150 to 167 s with six miners. A forward unload point at a contested cluster gives 6.7 to 9 credits a second per miner; placed at 90 s in a 200/600 match it gives 12 to 19 fighters at 300 s against 6 to 9.

**Match length, arithmetic on shipped constants**: one fighter lands 12.5 dps on a station (640 s); the earliest ten-fighter siege against an idle defender ends at 300 to 330 s; a three-fighter rush at 344 s; the defender builds 5.7 fighters during the 86 s crossing (11.5 at ShipyardL2).

**The standoff, on the real field**: station at (−6,000, 0), nearest rock at 797; the miner extracts 602 from the station and unloads 157 from it; a raider 500 out on that radial is 104 from the extraction point, 343 from the unload point, 405 from the spawn point and 100 from a module at 400 on that side; 401 + 160 = 561 < 600.

**Host cost on this VM, single thread, g++ -O2**

| Players | Entities | Simulation per tick | Accumulator per tick | Per client |
|---|---|---|---|---|
| 2 | 110 | 13 µs | 16 to 17 µs | 8 µs |
| 4 | 220 | 25 µs | 75 to 79 µs | 19 µs |
| 100 | 5,500 | 0.66 ms | 51.5 to 53.4 ms | 0.52 ms |
| 100, with records built once and a top-K select | 5,500 | | 11.1 ms | 0.11 ms |

A synthetic M3 targeting pass (a grid query per armed ship per tick, sorted by identity): 11 µs spread and 104 µs in a brawl at 110 entities, 437 µs in a brawl at 220, 6.7 ms at 5,500.

**The wire under M3**: with 40 fire events per update, three repeats and only the first update of a tick carrying them, M mounts need a cadence of at least ⌈3M/40⌉ ticks: 100 mounts at a one-tick cadence back up 52,000 sends in 200 ticks; at eight ticks they keep up. Fifty-five removals against the cap of 48: the last seven are first sent ten ticks after the death and complete nineteen ticks after it.

**The client's forget rule**: any silence of three to nineteen ticks forgets the ten entities that rode the tick's second datagram and re-adds them in the same drain; one or two ticks forget nothing; twenty rejoins.

**Ring assignment at the edge**: fifty fighters ordered to the corner get 32 distinct destinations, to the mid-edge 46.

**The token collision**: at seed 20260922 evening one issues b11fa2dcd9134829 to player 1 and ba4ef9dcf6f26c24 to player 2; after a restart on the same seed with the second client joining first, that client is seated as player 1 with the first client's old token, and the two then evict each other from seat 1 every second.

**Velocity**: 78 non-merge commits in 34 hours; M1's fifteen code steps in 9.6 hours and M2's fourteen in 15.6; seven gate items closed or half-closed against about twenty opened in the same window; 31 confirmations and four decisions open at HEAD.

## Appendix C. The full tuning table

The complete table of every number the simulation and the wire carry, as read from the code, is §3 of [`2026-09-23-mid-implementation-review/phase0-map.md`](2026-09-23-mid-implementation-review/phase0-map.md); the raid arithmetic reproduced from it is under "Combat" there. Nothing in this review moves a number in the code or the documents; every recommended value above is a proposal for the designer to rule on, and the register is where each should be asked.
