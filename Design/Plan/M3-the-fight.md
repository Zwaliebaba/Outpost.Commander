# M3 — The fight

[`GameDesign.md`](../GameDesign.md) §10: weapons, the damage table, the station's point defense, miner
flight, destruction, elimination and victory — **and a match that restarts on a new seed**, so twenty can
be played in an evening. Finite asteroids arrive here, and a stub AI so a match can be played by one
person.

**What it proves is not combat.** It is that **the loop can be played twenty times in an evening**, which
`GameDesign.md` §10 calls the only mechanism this project has for turning the balance questions in §7 into
answers rather than opinions. Everything in this milestone serves that; the match restart is not a
convenience feature, it is the point.

**Read [`README.md`](README.md) first.** Twelve steps, two gates, and **the first gate is a number the
design has not chosen** (F3) that silently moves every figure in §7.

**Entry state.** M2 complete: a generated field, miners, credits, two commanders. Nothing has ever taken
damage.

---

## The order of work

**Ruled 2026-09-24 (`OpenQuestions.md` Q72), from the mid-implementation review's M16.** The steps keep
their numbers, which are names cited across `Design/`, and are built in this order:

**M3.0 → M3.1 → M3.2 → M3.4 → M3.7 → M3.8 → M3.10**, which is a playable, restartable match against a stub
after seven steps. **Then the raid content**: M3.3, M3.3b, M3.5, M3.6, M3.8b and M3.9, while the first matches
are played and the economy's and the match length's questions (Q62, Q65) are answered by playing rather
than after the last step. **M3.3's wire half already exists** (`GameCore/Update.h`'s fire event and the
accumulator's `NoteFire`), so it is a client step. M3.11 comes last, gated by `README.md`'s class A.

What reordering does not change: each step's *Read first* and *Done when* still hold, and a step whose
question is not ruled when its turn comes waits for the ruling rather than assuming it.

### M3.0 — GATE: the firing cadence, and where the rounding lands · — · hand · **human**, then ADR-014

**Read first:** `README.md` F3; `GameDesign.md` §7 in full;
[`ADR-004`](../ADR/ADR-004-weapons-resolve-at-the-fire-tick.md); ADR-002.

**The design gives damage per second and the simulation ticks twenty times a second, and nothing says how
the one becomes the other.** `GameDesign.md` §7 gives `MassDriver` 25 damage **per second** per mount and
computes every figure in its raid table from that; ADR-004 applies damage **on the tick a weapon fires**,
"roughly once a second". Two things follow that nobody has chosen:

- **The firing interval in ticks.** Once a second is 20 ticks; every tick is 1.
- **Where the integer division lands.** `25 × 70 ÷ 100` is 17.5. Seventeen per shot at 1 Hz is not the
  17.5 a second §7's table was computed from. **Fire every tick instead and `25 ÷ 20` truncates to 1,
  which is 20 damage a second rather than 25 — a 20% error that arrives silently** and moves every row of
  the raid arithmetic.

§7's numbers are the most carefully argued part of the design — an earlier set of them **solved the game**,
and this register round is what fixed them. **This step can undo that by accident**, which is why it is a
gate and why the answer is the design's rather than the plan's.

**One shape worth putting on the register as an option:** accumulate damage in hundredths and settle the
integer at application, which makes the tick rate invisible to balance and makes §7's continuous figures
exactly what the simulation does. It costs one wider field per weapon and it keeps ADR-004's fire event
unchanged.

**Done when:** the register has the answer, `GameDesign.md` §7 says what a weapon does per tick rather than
only per second, and **ADR-014 records the decision** with whatever §7 figures moved.

**ANSWERED BY THE OWNER, 2026-09-24** (`OpenQuestions.md` Q66, to its recommendation): the shape above, in
ten-thousandths of a point rather than hundredths, which is what makes it exact. `GameDesign.md` §7 *What a
weapon does per tick* and [`ADR-014`](../ADR/ADR-014-damage-accumulates-every-tick.md) record it; no §7 figure
moved, since the rule reproduces all of them. M3.1 pins the rows as tests.

### M3.1 — The damage table · `GameCore` · `GameCoreTests` · agent

**Read first:** `GameDesign.md` §7; ADR-014; M1.1's size-class naming note.

**Adds:** **two mitigation models, because §7 now has two**, as one pure function beside M1.2's derived
stats. A ship takes `base × modifier[weaponClass][targetSizeClass] / 100`, where the modifier table is six
numbers and is the whole of the rock-paper-scissors between ships. **A station or a module takes
`base × 100 / (100 + hitValue)`** instead, where the hit value is derived from the hull and its components
like any other stat (R24).

**Multiply before dividing, in both.** `base × (100 / (100 + hitValue))` in integers truncates to zero
before it multiplies, so every shot does nothing — and it is the kind of fault that passes a smoke test and
fails a match.

**Files:** `GameCore/DamageTable.h` `.cpp`; `GameCore.vcxitems` + `.filters`;
`Tests/GameCoreTests/DamageTableTests.cpp`.

**Done when:** every weapon against every size class is pinned; **the two models are pinned against each
other at the one point they agree** — a hit value of 300 and the Large column both quarter the damage, which
is the identity §7 chose the number to preserve, so a test that stops seeing it is a balance change
somebody made by accident; **§7's raid figures are reproduced by tests** — one fighter against one miner,
one fighter against one fighter, three fighters against six miners, and `GameDesign.md` §5's ten fighters
against a station — because those are the numbers the design's balance argument rests on, and **a rounding
change must break a test rather than a match**; and ADR-014's cadence is one named constant.

### M3.2 — Weapons in the tick · `GameLogic` · `GameLogicTests` · agent

**Read first:** ADR-004; `GameDesign.md` §7; `TechnicalDesign.md` §2's tick order; M2.5's ordering rule.

**Adds:** a ship with a weapon and no order engaging the nearest hostile in range on its own; a ship with
an attack order pursuing its target. **No formation system beyond M1.7's ring spacing.**

**Target selection is where M2.5's ordering rule earns its keep**: candidates come from the grid, are
sorted by identity, and a tie between two hostiles at equal distance breaks on identity. ADR-002 calls an
unordered tie-break a desynchronisation that appears once an hour and cannot be reproduced, and this is
the code it was talking about.

**Files:** `GameLogic/WeaponSystem.h` `.cpp`, `GameLogic/Targeting.h` `.cpp`; `GameLogic.vcxproj` +
`.filters`; `Tests/GameLogicTests/CombatTests.cpp`, `TargetingTests.cpp`.

**Done when:** range and arc are checked at the fire tick and damage lands the same tick; **a target at
exactly the range boundary resolves the same way every run**; two candidates at equal distance select the
lower identity, asserted; and the determinism test's scripted orders now include a fight.

### M3.3 — The fire event, and the tracer · `GameCore`, `GameClient` · both · agent

**Read first:** ADR-004; ADR-003's snapshot layout; `TechnicalDesign.md` §4.

**Adds:** what M0.9 already reserved room for. **The host emits a fire event — shooter 2, target 2, weapon
1 — behind a count byte, after the removal list.** It is not retransmitted: a lost snapshot costs a missing
tracer, which is not worth a reliability path. The client draws it as a tracer, a beam or a muzzle flash,
entirely client-side and on a client-side timer.

**ADR-004 asks for honesty here and it is worth quoting into the code review rather than the code: "a
client drawing a tracer that always hits is drawing a lie about a shot that has already resolved."** That
is fine for a tracer and it is not fine for anything a player is meant to react to — so nothing downstream
may treat the tracer's travel as a thing with duration.

**Files:** `GameCore/FireEvent.h` `.cpp`; `GameClient/Tracers.h` `.cpp`; both project files and
`.filters`; `Tests/GameCoreTests/SnapshotTests.cpp` extended; `Tests/GameClientTests/TracerTests.cpp`.

**Done when:** a snapshot carrying several fire events round trips; a dropped snapshot costs exactly one
missing tracer and nothing else — asserted by feeding the client a gap; and the tracer's lifetime is
client-side, with the host never told it exists.

### M3.3b — The alert, and hull bars in the world · `GameClient` · `GameClientTests` · agent

**Read first:** [`ADR-020`](../ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md); `Interface.md`
§1's pick order and §6, including *Where the geometry lives*;
[`design_handoff_hud/README.md`](../design_handoff_hud/README.md) §*Damage alert* and
§*World-anchored interface elements*; [`ADR-018`](../ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)'s
recenter.

**Adds:** the two things that make damage visible, both derived from data the client already holds.

**The alert.** M3.3 just made fire events arrive; an event naming one of *your* entities is an attack on
you, and the replica store has the position. **No wire bytes and no host change** — this step touches
`GameClient` and nothing else. It draws a directional indicator at the screen edge, fading over a few
seconds, **suppressed entirely when the event is already on screen**, clustered so a fleet caught in the
open is one indicator with a count rather than forty. **The handoff draws it and adds a cap ADR-020 did
not have**: at most three indicators at once, never closer than 112 pixels along an edge, two that would
collide merging and summing their counts, a fourth replacing the oldest — plus the clamp that keeps the
body clear of every panel band, and **no scrim and no plate behind it**, because the moment an alert looks
like chrome it becomes chrome and habituation is the failure ADR-020 names.

**It is a hit-test rectangle, not a gesture.** Tapping it recenters the camera there, reusing M1.8's
recenter. R21's gesture budget is untouched and the banked `Holding` stays banked — which is why
`Interface.md` §1's pick order now puts **any interface target ahead of every world tier**, a precedence
it never stated.

**Hull bars in the world, on damaged ships only.** A ship at full hull draws nothing, so the map stays
quiet until something is wrong; the cost is one quad per damaged ship, at most 110. Position by projecting
the world point and then through the **interface's** fit transform (ADR-011, ADR-016) — the world's fit is
the other one and using it here is the ADR-016 defect in miniature. **The bar is 18 × 6 and a fixed size
at every depth**, and only the *lost* portion is bright — so a healthy fleet is a row of dark ticks and a
dying one a row of red, and loudness tracks severity without a rule.

**Without this step, half of `GameDesign.md` §7 cannot be played.** It says a defender "must be watching
the right part of a 16,384-unit map at the right moment to have any counterplay" — with no minimap, no
audio and no hull bar outside the selection panel, that is not a hard ask, it is a guess.

**Files:** `GameClient/DamageAlert.h` `.cpp`, `GameClient/HullBar.h` `.cpp`; `GameClient.vcxproj` +
`.filters`; `Tests/GameClientTests/DamageAlertTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's requirement is met — a fire event naming one of yours raises an
alert and one naming somebody else's does not; an event already on screen raises none; several hits in one
place cluster to one indicator with the right count; **and the bearing is right at several camera
headings, including an event behind the camera**, which is the case that gets the sign wrong. Plus: an
undamaged ship draws no bar; the alert's target is at least 64 × 64; **a fourth simultaneous cluster
replaces the oldest rather than drawing a fourth indicator**; and the geometry matches `geometry.json`
through M1.14's gate, which now covers the alert's rects.

### M3.4 — Death, the removal list, and wrecks · `GameLogic`, `GameClient` · both · agent

**Read first:** ADR-003's removal-list paragraph; ADR-004; `TechnicalDesign.md` §4.

**Adds:** the removals carrying something for the first time. **This is the step ADR-003 added the list
for**: without it a death is learned by *absence* — and since
[`ADR-024`](../ADR/ADR-024-replication-is-prioritized-records.md) absence is ambiguous from the first tick,
because the accumulator sends what is due and not everything. **A removal rides ten consecutive updates
and the client ignores one it has applied**; M1.14c built that path with nothing to carry, and this is
where it carries a death.

A wreck is **presentation and not simulation**: spawned by the client from the death, decaying on a
client-side timer, and the host never knows it exists.

**Files:** `GameLogic/DeathSystem.h` `.cpp`; `GameClient/Wrecks.h` `.cpp`, `GameClient/ReplicaStore.cpp`;
`Tests/GameLogicTests/DeathTests.cpp`; `Tests/GameClientTests/RemovalTests.cpp`.

**Done when:** a death produces a removal entry and the client evicts the entity **from the removal list,
and never treats it as dead by absence inside the forget horizon** — assert this by feeding the client
updates in which an entity is missing *without* a removal entry for up to
`ReplicaStore::FORGET_FLOOR_TICKS` and requiring that it is **not** treated as dead; a dead ship leaves the
selection, and a wreck is spawned, **from the removal and not from the store forgetting**; and a wreck
decays without the host being told. *(Reworded 2026-09-24 after the mid-implementation review, m2: past
the horizon the store does forget, deliberately — it is how a death whose ten removals were all lost is
cleaned up — so "never from absence" would have pinned a test against the store as built. A forgotten
entity spawns no wreck.)*

### M3.5 — The station's point defense · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §5 in full; `OpenQuestions.md` Q10 and Q16.

**Adds:** the station shooting, which needs no new code beyond what M3.2 built — it is a hull with two
`PointDefense` mounts (M1.1) and the weapon system does not know it is a station.

**The point defense outranges nothing, and `GameDesign.md` §5 says to say so out loud or it reads as a
bug.** It reaches 400 units; a `MassDriver` reaches 600. **A fighter can stand off at 500 and shell the
station untouched.** What the point defense protects is the *unloading area* — a raider that chases a
fleeing miner home crosses 400 and dies in under six seconds — not the station itself. **So it kills a
loiterer, not a besieger and not a fleet**, and Q16 confirms it deliberately does not cover the home field,
so miners at the rocks stay raidable.

**Do not "fix" this while implementing it.** It is the most likely thing in the milestone to be mistaken
for a defect, and `GameDesign.md` §5 explains at length why a station whose defense outranged the fighter
would simply be unkillable now that the siege unit is cut.

**Files:** `GameLogic/WeaponSystem.cpp`; `Tests/GameLogicTests/CombatTests.cpp` extended.

**Done when:** a fighter at 500 units takes no return fire and a raider at 300 dies inside the design's
figure; and the safe zone's boundary is a test rather than an observation.

### M3.6 — Miner flight · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §7's flight paragraph.

**Adds:** thirty lines, and `GameDesign.md` §7 is explicit about their value: **without them the defender
must be watching the right part of a 16,384-unit map at the right moment to have any counterplay at all.**
A miner with no order that is fired upon flees to its station.

**With no order** is the condition and it matters — a miner explicitly ordered to mine keeps mining, which
is the player overriding the behavior, and that distinction is what makes the behavior a help rather
than an annoyance.

**Files:** `GameLogic/MinerBehavior.h` `.cpp` — **`Behavior`, R11's spelling, in an identifier**;
`GameLogic.vcxproj` + `.filters`; `Tests/GameLogicTests/MinerFlightTests.cpp`.

**Done when:** an unordered miner under fire heads for its station and an ordered one does not; the flee
path is deterministic; and §7's figure for miners lost fleeing 1,500 units is reproduced within the
tolerance the design's own table implies.

### M3.7 — Elimination and victory · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §2.

**Adds:** **victory is the last station standing.** A station destroyed eliminates its owner **and removes
their remaining ships from the field** — `GameDesign.md` §2 takes that deliberately, because leaving a
beaten player's ships alive to be hunted turns the last ten minutes of every match into a search problem
and no amount of tuning fixes that.

**Files:** `GameLogic/Victory.h` `.cpp`; `GameLogic.vcxproj` + `.filters`;
`Tests/GameLogicTests/VictoryTests.cpp`.

**Done when:** losing a station eliminates the player and removes their ships in one tick, through the
removal list; the last player standing wins; and simultaneous destruction on one tick resolves the same
way every run rather than by iteration order.

### M3.8 — The match restarts · `GameLogic`, `GameClient` · both · agent

**Read first:** `Interface.md` §7's *When a match ends*; `OpenQuestions.md` Q25; ADR-003.

**Adds:** **the host reseeds and starts another; the client shows a result overlay and reconnects into
it.** Q25 exists because nothing previously said what happened at victory, and because **for a solo
testing loop this is the single most-used operation in the project** — twenty matches in an evening is not
possible if playing again means relaunching a packaged application.

**It costs almost nothing structurally**, which is
[`ADR-003`](../ADR/ADR-003-the-record-and-the-command.md) paying for itself a third time: snapshots
are self-contained, so there is no resynchronisation path to write. The client's derived state —
selection, markers, wrecks — is what must be cleared, and it is the only thing that must be.

**Files:** `GameLogic/Match.h` `.cpp`; `GameClient/MatchState.h` `.cpp`, `GameClient/Panels.cpp`;
`Tests/GameLogicTests/MatchTests.cpp`; `Tests/GameClientTests/MatchStateTests.cpp`.

**Done when:** the host reseeds and runs on without being restarted; the client shows the result, clears
**all** derived state, and is playing the next match without a relaunch; and a reconnecting client lands in
the current match rather than the finished one.

### M3.8b — Modules under fire · `GameLogic` · `GameLogicTests` · agent

**Read first:** [`ADR-015`](../ADR/ADR-015-the-base-is-built-from-modules.md)'s *Consequences*;
`GameDesign.md` §7 on modules as targets; M2.9 to M2.12.

**Adds:** nothing new to combat — a module is an entity with a hull and a size class, so the damage table
already covers it. What this step adds is the **two consequences** of that: a destroyed module stops doing
its job the tick it dies, and **elimination removes a player's modules with their ships**.

**Files:** `GameLogic/Damage.cpp`, `GameLogic/Victory.cpp`;
`Tests/GameLogicTests/ModuleEffectTests.cpp` and `VictoryTests.cpp` extended.

**Done when:** killing an ore processor drops its owner's income the same tick, killing a shipyard slows
the queue already running, and eliminating a player removes their modules in the same snapshot as their
ships.

---

### M3.9 — Finite asteroids · `GameCore`, `GameLogic` · both · agent

**Read first:** `GameDesign.md` §4; `OpenQuestions.md` Q22; `TechnicalDesign.md` §4's sparse-ore paragraph.

**Adds:** what M2 deliberately left out. **An exhausted asteroid stays on the map as a husk, and a miner
with no order retargets the nearest one with ore left.** Finite asteroids are what make the contested
fields worth contesting; an infinite home field turns the map into scenery and the match into pure military
arithmetic.

**And the first asteroid replication in the project** (Q22): ore remaining is sent **sparsely** — only
asteroids whose **quantized ore bucket** changed since the last snapshot, which is at most one per active
miner, four bytes each. The quantization is what keeps it sparse; sending exact ore would send every
asteroid every snapshot.

**The cost is stated rather than solved**: a player who ignores their miners eventually finds them idle,
and `GameDesign.md` §4 accepts that as a management burden the MVP does not fix.

**Files:** `GameCore/AsteroidRecord.h` `.cpp`, `GameCore/Snapshot.cpp`; `GameLogic/MiningSystem.cpp`;
both project files; `Tests/GameCoreTests/SnapshotTests.cpp` extended;
`Tests/GameLogicTests/MiningTests.cpp` extended.

**Done when:** the sparse list carries only changed buckets, asserted by a test that mines one asteroid and
requires exactly one entry; **the snapshot with a full sparse list is re-measured against M0.9's
single-datagram figure**, because this is the first thing since M0 to grow the snapshot; retargeting picks
the nearest asteroid with ore and breaks ties on identity; and a husk is still drawn.

### M3.10 — The stub AI · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §8 and §10; R16.

**Adds:** enough opponent that **one person can play a match**, and no more. `GameDesign.md` §10 asks for
"a stub AI that builds and attacks"; §8's proper state machine is M4's.

**It issues the same orders a human does, through the same command path.** It is not given information a
human in its position would not have and it is not given a discount — **a cheating AI is untestable,
because you cannot tell a bug from the cheat.** It runs on the host inside `GameLogic`, on the tick, under
R16 in full: no wall clock, no unordered iteration that reaches an outcome, and the match's pinned PRNG for
anything random.

**Files:** `GameLogic/StubAi.h` `.cpp` — `Ai` rather than `AI`, since R4 capitalises acronyms as words;
`GameLogic.vcxproj` + `.filters`; `Tests/GameLogicTests/StubAiTests.cpp`.

**Done when:** the AI builds miners, mines, builds fighters and eventually attacks, without reading
anything a client is not sent; a match against it reaches a victory unattended; and **it appears in the
determinism test**, because an AI that reads the clock is the easiest possible way to lose R16.

---

### M3.11 — GATE: twenty matches in an evening · — · hand · **human**

**Read first:** `GameDesign.md` §10 and §7; `OpenQuestions.md` Q16; `Design/README.md`'s *What is checked
by a hand*.

**This is what the whole reduced MVP is for.** `GameDesign.md` §10: two players rather than four buys a
five-minute match, and **twenty matches in an evening is the only mechanism this project has for turning
the balance questions in §7 into answers rather than opinions.** One long four-player match cannot supply
it.

Play them, and answer:

0. **Does the alert fire too often to be worth reading, and do you act on it?**
   ([`ADR-020`](../ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md)). It is numbered zero because
   every answer below depends on it: **a defender who cannot tell they are under attack is not playing the
   game §7 describes**, and a raid that lands unanswered because nobody noticed is not evidence about the
   raid arithmetic. The failure is habituation — an indicator that cries wolf becomes wallpaper, and then
   it is worse than none. If the answer is that players ignore it, the indicator is the wrong shape and a
   panel entry is the alternative.
1. **Is the raid arithmetic right?** §7's table predicts an exchange rather than a slaughter. The previous
   set of numbers **solved the game** and it took an adversarial review to notice, so the bar is whether
   the opening has more than one viable line.
2. **Is the station's safe zone too safe?** — the confirmation `Design/README.md` owes at M3, and Q16's
   answer being confirmed or reversed.
3. **Does the five-minute match actually happen**, or does it run to fifteen?

**Done when:** twenty matches have been played, §7's figures are updated where the play disagreed with the
arithmetic, and Q16 is confirmed or reopened. **A figure moved here is moved in `GameDesign.md`** — the
design is the record and the plan cites it.

---

## Leaving M3

**The milestone is finished when** one person can play a match against a stub AI from a generated field to
a victory overlay and straight into the next one, twenty times, and the design's numbers have been moved by
what happened rather than by argument.

**What M3 produces besides code:** ADR-014; §7's numbers confirmed or corrected; Q16's confirmation; and a
re-measured snapshot size now that asteroids are on the wire.

**The thing to watch** is M3.0. Every other step in this milestone is arithmetic the design already did;
that one is arithmetic the design **did not know it had left open**, and it is capable of moving every
figure in §7 by a fifth without anybody noticing until the matches feel wrong.
