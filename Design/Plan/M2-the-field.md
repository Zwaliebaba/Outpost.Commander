# M2 — The field

[`GameDesign.md`](../GameDesign.md) §10: the procedural generator on a real seed, asteroid fields, miners,
the credit loop. **Asteroids are inexhaustible** until M3.

**What it proves** is R23 — that both sides derive the same world from one seed and no map is ever
transmitted — and that the economy is a **loop** rather than a number that goes up. A miner is a ship on
the map that can be shot, and until there is one, `GameDesign.md` §4's claim that the economy is something
a player can see failing is untested.

**Read [`README.md`](README.md) first.** Eleven steps, three gates, and **the first is the register's one
open question**: nothing can be generated until Q26 has an answer.

**Entry state.** M1 complete: two commanders, one catalog, two designs, selection, orders, the interface.
One fixed seed with a hand-checked layout, and `GameCore/Layout` waiting to become a generator.

---

### M2.0 — GATE: answer Q26 · — · hand · **human**

**Read first:** `OpenQuestions.md` Q26; `GameDesign.md` §3 and §7.

**Q26 is the register's only open question and it is needed by this milestone**: what is the asteroid
count, and what is the spawn anchor radius? Neither appears anywhere in the design, and both are inputs to
things that do — the anchor radius sets how long a strike force takes to cross the map, which is half of
§7's raid arithmetic, and the asteroid count sets the sparse ore budget from M3 and how much a home field
is worth holding.

**The register offers no recommendation and says why**: both follow from playing, and guessing them now
would add two more unmeasured numbers to a document that has plenty. **What it does require is that the
generator names them rather than leaving them implicit in code** — so the answer may be provisional, but
it may not be anonymous.

**Done when:** both figures are on the register with an answer and written into `GameDesign.md` §3, even if
the answer is "this, for now, and M3's twenty matches will move it".

### M2.1 — The generator · `GameCore` · `GameCoreTests` · agent

**Read first:** `TechnicalDesign.md` §3; R23; R16; `GameDesign.md` §3; ADR-002's PRNG and ordering
paragraphs.

**Adds:** a seed in, a list of placed objects out. It runs on the host to populate the match and **it runs
on the client to draw the same asteroids** — the client derives the map rather than being sent it, which
keeps a large static payload off the wire and makes it impossible for the two sides to disagree about
where a rock is.

**This is not the client simulating** (R19). A generator is a rule, and `GameCore` is where the rules both
sides evaluate live — the same arrangement that lets a client preview an order against the rules the host
validates with. What the simulation *owns* — how much ore is left — is replicated like any other state,
and at M2 there is none of it.

**It obeys R16 in full, and the reason is sharper here than anywhere else:** integers and fixed point, no
iteration over an unordered container whose order reaches the outcome, and the match's pinned PRNG from
M0.7 and nothing else. **A generator that reaches a different answer on the two sides is a defect of
exactly the same class as a desynchronisation**, and it will present as asteroids in the wrong place rather
than as anything that looks like a determinism bug.

The interface is a seed in and a list of placed objects out, so adding a kind of object later — nebulae,
wrecks, hazards, none of which are in the MVP — does not change its shape.

**Files:** `GameCore/Generator.h` `.cpp` (growing out of M1.5's `Layout`), `GameCore/PlacedObject.h`;
`GameCore.vcxitems` + `.filters`; `Tests/GameCoreTests/GeneratorTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's requirement is met — **the generator's output is pinned for a
seed** against a checked-in table; Q26's two figures are named constants and appear in the test by name;
and running the generator twice on one seed produces byte-identical output, asserted rather than assumed.

### M2.2 — The symmetry · `GameCore` · `GameCoreTests` · agent

**Read first:** `GameDesign.md` §3; `TechnicalDesign.md` §3.

**Adds:** the generated region copied by rotation to the player count. **At four players one quadrant is
generated and copied at 90°, 180° and 270°; at the MVP's two players one half is generated and copied at
180°.**

**Both rotations are exact because positions are integers** — a 90° rotation is a swap and a negation, a
180° rotation is a negation — where a floating-point rotation would make the starts subtly unequal, which
`TechnicalDesign.md` §3 calls the kind of unfairness nobody would find for a year. **This is the cheapest
possible fairness guarantee**: every player's start is the same start, so no balance analysis is needed and
no seed can be unlucky. It is also visibly artificial, and `GameDesign.md` §3 takes that trade explicitly.

**Files:** `GameCore/Generator.cpp`; `Tests/GameCoreTests/GeneratorTests.cpp` extended.

**Done when:** `TechnicalDesign.md` §8's requirement is met in full — **the symmetry is asserted at both
two and four players**, although the MVP runs only two. For each placed object there is exactly one
counterpart at the rotated position, with no rounding anywhere and no object landing on the center twice.

### M2.3 — The client derives the field · `GameClient` · `GameClientTests` · agent

**Read first:** R23; `TechnicalDesign.md` §3 and §4; `OpenQuestions.md` Q22.

**Adds:** the client calling the same `GameCore` generator with the match's seed and drawing what comes
out. **No map is transmitted and no asteroid is replicated at all before M3** (Q22): inexhaustible
asteroids have no simulation state, so there is nothing to send.

**Files:** `GameClient/FieldView.h` `.cpp`; `GameClient.vcxproj` + `.filters`;
`Tests/GameClientTests/FieldViewTests.cpp`.

**Done when:** a test runs the generator through both the host's and the client's entry points and asserts
the outputs are identical — **the one test that can catch an R23 violation before it presents as rocks in
the wrong place**; and a `grep` finds no asteroid position anywhere on the wire.

### M2.4 — The asteroid mesh · `GameClient` · hand · agent

**Read first:** [`ADR-005`](../ADR/ADR-005-meshes-are-generated-in-code.md); `TechnicalDesign.md` §7.

**Adds:** the same mesh function M1.9 built, **driven by the match PRNG so that no two rocks are
identical** — which ADR-005 names specifically, and which costs nothing because the PRNG is already there
and already pinned. One instanced draw for the whole field.

Asteroids, wrecks and debris **may be drawn above and below the plane** so the space reads as a volume;
[`ADR-001`](../ADR/ADR-001-the-playfield-is-a-plane.md) permits exactly that and is equally clear that none
of it is simulated and the host does not know it exists. **A visual offset must therefore live in the
client and never reach a `GameCore` record** — this is the step where R22's third coordinate would sneak
in if it were going to.

**Files:** `GameClient/AsteroidMesh.h` `.cpp`; `GameClient.vcxproj` + `.filters`.

**Done when:** the field draws as one instanced call, rocks differ from one another, and the visual
vertical offset exists only in client code — check your own diff for a `z` that crossed into `GameCore`.

### M2.5 — The uniform grid · `GameLogic` · `GameLogicTests` · agent

**Read first:** `TechnicalDesign.md` §2, the ordering section; ADR-002.

**Adds:** the spatial index — **a uniform grid, 512-unit cells, 32 × 32 over the square**. It is a
**candidate structure only**: every query whose result reaches an outcome **sorts its candidates by entity
identity before using them**, and every tie breaks on identity and never on which cell was visited first.

R16 forbids iteration over an unordered container whose order reaches the outcome, and this is where that
rule has teeth. ADR-002 is blunt about the cost: sorting costs real time in the hot path of target
selection, and it is not optional, because **an unordered tie-break is a desynchronisation that appears
once an hour and cannot be reproduced.**

**The client does not get one.** At 110 entities `GameClient`'s hit test is a linear scan (M1.10) and a
second index would be a second thing to keep correct for no measured gain.

**Files:** `GameLogic/UniformGrid.h` `.cpp`; `GameLogic.vcxproj` + `.filters`;
`Tests/GameLogicTests/UniformGridTests.cpp`.

**Done when:** a query returns every entity within a radius and nothing outside it, including across cell
boundaries and at the edges of the square; **the returned order is by identity and is asserted to be**; and
two entities at exactly equal distance resolve to the same one on every run.

### M2.6 — The mining loop · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §4 and §6; `TechnicalDesign.md` §2's tick order.

**Adds:** the whole economy, as a **five-state standing order** — going to ore, extracting, going to
unload, unloading, and back to ore. A mine order does not complete; it runs until the miner is told
something else, which is why an economy runs without a player shepherding it and is the only order in the
design that behaves this way (`GameDesign.md` §4).

**Two things this step must not hardcode**, both of which are one line now and a refactor later:

- **Cargo capacity and extraction rate are derived** from the components in the hull's slots and summed,
  exactly as mass and cost are (R24). Not a constant on "the miner", which is not a type.
- **The unload target is a query** — the nearest owned entity that accepts ore, candidates ordered by
  entity identity (R16). Today that set holds one station. Writing it as a constant is what would make a
  mining factory at a contested field a rewrite instead of a table row.

`GameDesign.md` §4's starting values are the design's and are not repeated here. **They are starting
values and not balance**, and M3's twenty matches are what move them.

The `MiningLaser` is a slot component with a range like any weapon (M1.1) and does no damage. **Nothing
here is a special case for a "miner"** — it is a design whose slot happens to hold a mining tool, which is
ADR-006's central claim being exercised rather than asserted.

**Files:** `GameLogic/MiningSystem.h` `.cpp`, `GameLogic/UnloadTarget.h` `.cpp`; `GameLogic.vcxproj` +
`.filters`; `GameCore/DesignStats.cpp` extended for derived capacity and rate;
`Tests/GameLogicTests/MiningTests.cpp`; `Tests/GameCoreTests/DesignStatsTests.cpp` extended.

**Done when:** a miner completes the full cycle in the tick count the design's figures imply **and then
starts the next one without a further order**; extraction stops exactly at a full hold rather than
overshooting by a tick's worth; a miner ordered elsewhere mid-cycle abandons cleanly **and keeps whatever
it was carrying**; a two-slot design with two mining lasers is pinned at twice the capacity and twice the
rate, which is the derivation being exercised rather than asserted; and the loop is pinned by the
determinism test's scripted orders, not only by its own.

### M2.7 — Credits, and the readout · `GameLogic`, `GameClient` · both · agent

**Read first:** `GameDesign.md` §4; ADR-003's per-player block; `Interface.md` §6.

**Adds:** credits accruing on unload, carried in the per-player block M0.9 already encodes, **and the
cargo bucket in the flags byte** — two bits, four buckets, which is what a fill bar needs and is all the
snapshot has room for (`TechnicalDesign.md` §4), and the top-left
readout M1.14 already draws going live — **including the income rate, which `Interface.md` §6 says appears
"once there is one"**, and this is the milestone in which there is one.

**Files:** `GameLogic/Economy.h` `.cpp`; `GameClient/Panels.cpp`;
`Tests/GameLogicTests/EconomyTests.cpp`.

**Done when:** credits reach the client only through the snapshot; the income rate is computed from
observed deliveries rather than from a rule the client evaluates — **a client that derives income from the
catalog is a client doing simulation** (R19); and building deducts at start as M1.6 established.

### M2.8 — The tap on an asteroid · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §4's table.

**Adds:** the third row of the tap table. **An asteroid with ore: mine it — miners in the selection take
it, the rest move to it.** A mixed selection therefore does two things from one tap, which is the design's
intent and is worth a test of its own because it is the only row that splits a selection.

**Files:** `GameClient/TapOrder.cpp`; `Tests/GameClientTests/TapOrderTests.cpp` extended.

**Done when:** a mixed selection issues a mine order for the miners and a move order for the rest, in one
gesture; and the host's validation accepts both, which means M0.10's validation is exercised by a case it
has not seen.

---

### M2.9 — The module frame and its catalog · `GameCore` · `GameCoreTests` · agent

**Read first:** [`ADR-015`](../ADR/ADR-015-the-base-is-built-from-modules.md);
`GameDesign.md` §5 *The base is built out of modules*, §6's catalog; R24.

**Adds:** the `ModuleFrame` hull and the four module components to the catalog, and nothing else. A module
is a composition, so the derived-stat function needs no change — **that it needs no change is the thing
this step proves**, and it is what ADR-015 claims about levels being component identities.

**Files:** `GameCore/Catalog.h` extended; `Tests/GameCoreTests/DesignStatsTests.cpp` extended.

**Done when:** derived stats are pinned for every module at every level, and the suite fails if a level's
cost or hull is changed without the test being updated.

---

### M2.10 — Placement validity · `GameCore` · `GameCoreTests` · agent

**Read first:** ADR-015's *Decision*; `Interface.md` §6 *Placing a module*; R16's ordering rule; R23 on why
a rule both sides evaluate lives in `GameCore`.

**Adds:** one pure function — is this point a legal module site for this station? Inside 400 units, clear
of the station, clear of every existing module, and the fifth module refused against a cap of four. Integer
throughout, candidates ordered by entity identity.

**Files:** `GameCore/ModuleSite.h`, `GameCore/ModuleSite.cpp`, both in `GameCore.vcxitems` and its
`.filters`; `Tests/GameCoreTests/ModuleSiteTests.cpp` new, in that project and its `.filters`.

**Done when:** the five refusal cases and the accept case are each a test, and the client and the host call
the same function — which a reader can check by grep, because there is only one.

---

### M2.11 — Building and placing a module · `GameLogic`, `GameClient` · both · agent

**Read first:** M2.10; `GameDesign.md` §5; `Interface.md` §6; ADR-003 on the entity record.

**Adds:** the module as an entity the host creates on completion at the placed point; the build panel's
second row; the armed-placement state and the drawn radius on the client; the placement point carried on
the build command and validated by the host with M2.10's function.

**Files:** `GameLogic/BuildQueue.cpp`, `GameLogic/CommandValidation.cpp`, `GameClient/BuildPanel.cpp`,
`GameClient/TapOrder.cpp`; `Tests/GameLogicTests/BuildQueueTests.cpp` and
`Tests/GameClientTests/TapOrderTests.cpp` extended.

**Done when:** a module is built, appears at the tapped point, and survives a reconnect — which is the
snapshot carrying it correctly. A placement the host refuses leaves the credits unspent.

---

### M2.11b — Two reasons a build button is dead · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §6's build panel row.

**Adds:** the distinction M2.11 makes necessary. Until modules existed there was one reason a build button
was dead — you could not afford it — and one gray for it. **A module now gates what can be built**, so
there are two, and a single gray leaves a player unable to tell "save up" from "build something else
first".

**An item you cannot pay for keeps its button lit and reddens its cost. An item you have no module for is
dimmed entirely.** Affordability changes second by second and unavailability does not, which is the other
reason they should not look alike.

**Files:** `GameClient/BuildPanel.cpp` extended; `Tests/GameClientTests/BuildPanelTests.cpp`.

**Done when:** the two states are distinguishable in a test by the values the panel emits rather than by
looking, and an item that is both unaffordable *and* unavailable reads as unavailable — the state you
cannot fix with credits wins.

### M2.12 — What the modules do · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §5's table; ADR-015 on integer percentages and where they round.

**Adds:** the shipyard's build-rate multiplier and the ore processor's cargo multiplier, both applied as
integer percentages. **Where each rounds is the question ADR-014 is reserved for** (`README.md` F3) — this
step must not invent an answer; it applies the rule ADR-014 sets, or waits.

**Files:** `GameLogic/BuildQueue.cpp`, `GameLogic/Mining.cpp`;
`Tests/GameLogicTests/ModuleEffectTests.cpp` new, in the project and its `.filters`.

**Done when:** the multipliers are pinned at both levels, and a test asserts the rounding direction rather
than accepting whatever the code does.

---

## The gates

### M2.13 — GATE: the silhouettes · — · hand · **human**

**Read first:** ADR-005's Consequences; `GameDesign.md` §1; `Interface.md` §5.

**ADR-005 names this risk against this milestone.** `GameDesign.md` §1 promises "ships that bank as they
turn and read as silhouettes", `Interface.md` §5 couples pitch to zoom so the tactical view is near
top-down, and **two hulls emitted by one shared parameterised function will tend to be the same shape at
two scales** — which is exactly unreadable at the zoom where identification matters most.

**Done when:** a field of miners and fighters is looked at from the tactical zoom on the device and the two
are distinguishable at a glance. **If they are not, ADR-005 has already named the answer — a shape-coded
overlay, not more triangles** — and that is an ADR rather than a quiet addition to the mesh function.

### M2.14 — GATE: the tick's cost · — · hand · **human**

**Read first:** `TechnicalDesign.md` §9.3; ADR-002's Measurements 1.

**Adds:** nothing. Measure an empty tick and a full one **at 110 entities**, against the 50-millisecond
budget. ADR-002 names the two candidates for consuming it — **ring slot assignment and target selection** —
and M2.5's sort is now in the second of them.

**Done when:** the figure is measured, written into ADR-002's Measurements and `TechnicalDesign.md` §9, and
compared against 50 ms with the headroom stated. **What reopens ADR-002 is a tick that does not fit, and
the answer then is a lower tick rate rather than floats** — the ADR says so, so that the wrong conclusion
is not available under pressure.

---

## Leaving M2

**The milestone is finished when** two commanders on a generated, symmetric field send miners to rocks, the
credits climb because ships did work, and both clients draw the same asteroids from the same seed with
nothing about the map on the wire.

**What M2 produces besides code:** Q26 answered and written into `GameDesign.md` §3; §9.3 struck through;
and either a confirmation that the hulls read or an ADR for the overlay that fixes them.

**The thing to watch** is M2.3. R23's claim is that the two sides cannot disagree, and the test that
asserts it is cheap — but it only asserts that one build agrees with itself. **The generator running on
x64 and on ARM64 and producing the same field is the real claim**, and it rides on the standing four-pair
run in [`README.md`](README.md) rather than on anything CI will ever do.
