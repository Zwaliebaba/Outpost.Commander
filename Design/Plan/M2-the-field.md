# M2 — The field

[`GameDesign.md`](../GameDesign.md) §10: the procedural generator on a real seed, asteroid fields, miners,
the credit loop. **Asteroids are inexhaustible** until M3.

**What it proves** is R23 — that both sides derive the same world from one seed and no map is ever
transmitted — and that the economy is a **loop** rather than a number that goes up. A miner is a ship on
the map that can be shot, and until there is one, `GameDesign.md` §4's claim that the economy is something
a player can see failing is untested.

**Read [`README.md`](README.md) first.** Fifteen steps, three gates, and **the first is a gate on Q26**:
nothing can be generated until it has an answer. **Q36 is the milestone's other open question** and it does
not gate the start — it is owed by M2.7, where the income rate goes live.

**Entry state.** M1 complete: two commanders, one catalog, two designs, selection, orders, the interface.
One fixed seed with a hand-checked layout, and `GameCore/Layout` waiting to become a generator.

---

### M2.0 — GATE: answer Q26 · — · hand · **human**

**Read first:** `OpenQuestions.md` Q26; `GameDesign.md` §3 and §7.

**Q26 gates this milestone and nothing in it can start without the answer**: what is the asteroid count,
and what is the spawn anchor radius? Neither appears anywhere in the design, and both are inputs to
things that do — the anchor radius sets how long a strike force takes to cross the map, which is half of
§7's raid arithmetic, and the asteroid count sets the sparse ore budget from M3 and how much a home field
is worth holding.

**The register offers no recommendation and says why**: both follow from playing, and guessing them now
would add two more unmeasured numbers to a document that has plenty. **What it does require is that the
generator names them rather than leaving them implicit in code** — so the answer may be provisional, but
it may not be anonymous.

**Done when:** both figures are on the register with an answer and written into `GameDesign.md` §3, even if
the answer is "this, for now, and M3's twenty matches will move it".

**ANSWERED BY THE OWNER.** The anchor radius is 6,000 (2026-09-22). The asteroid count is ten per home
field plus two contested clusters of six per region (2026-09-23). Both are on the register and in §3.

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

**BUILT, 2026-09-23**, as `GameCore/Generator.h` `.cpp`. `GenerateRegion(seed, players)` places player
one's region: ten rocks between 600 and 1,500 units from the anchor, then two contested clusters of six
whose centers sit 1,500 to 3,500 from the middle. It draws from its own PCG32 stream, 3. **It generates the
region and nothing copies it yet**: at two players that is the half `x < 0`, and at four the quarter
between the diagonals. M2.2 does the copy. Every rock keeps half the 150-unit spacing from the region's
edge, and the suite already applies the rotations by hand to prove the copies keep their spacing too.

**Two proposed names were not used.** There is no `PlacedObject.h`: `Layout.h`'s `Placement` gained a
`PlacedKind` and a `FieldKind`, because an asteroid is not a design and two row types for one list
would disagree. And the host is unchanged. It still places stations through `GenerateLayout`, because
asteroids are not simulated or replicated before M3 and nothing on either side reads the field until
M2.3. `GeneratorTests` pins seed 20260922 row by row, runs the generator twice, and checks the counts,
the annulus, the band, the spacing and the region over 200 seeds at two and four players.

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

**BUILT, 2026-09-23**, as `GenerateField(seed, players)` in `GameCore/Generator.h` `.cpp`. It returns the
region and then each copy turned one step further round — 180° at two players, 90° at four — so **copy k is
the region of the player whose anchor is k steps round**, and a row's index modulo the region's size says
which rock it copies. It draws nothing from the PRNG, so M2.1's pinned table is still the field's first
rows. **The rotation is `Layout.h`'s `QuarterTurn`, moved out of `Layout.cpp`'s anonymous namespace** so
that the anchors and the rocks turn by one function rather than two that could disagree about which way is
a quarter turn. `FieldCopyCount` treats one player as two and three and every stress count as four, the way
`GenerateRegion` already did.

`GeneratorTests`' new `TheSymmetry` pins the 180° copy of seed 20260922 by number, and over 200 seeds at two
and four players asserts exactly one counterpart per rock at the rotated position, to the fixed-point bit
and through a rotation written out by hand rather than through `QuarterTurn`; no rock on the center; and
each copy's home field inside the annulus around its own player's anchor. M2.1's spacing test now runs
over `GenerateField` rather than its own hand copy. **The host is still unchanged**, as at M2.1: nothing
simulates or replicates an asteroid before M3, and the client's call is M2.3.

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

**BUILT, 2026-09-23, AND IT FOUND A GAP FIRST.** The field is derived from the seed **and the player
count**: M2.2 copies a half at two players and a quarter at four. The join reply carried only the seed, and
nothing else on the wire carries the count. **`OpenQuestions.md` Q50** records the gap, and the owner
ruled for a byte on `JoinReply`: 22 bytes to 23, `PROTOCOL_VERSION` 4 to 5,
[`ADR-013`](../ADR/ADR-013-a-client-is-told-which-player-it-is.md) amended, and R23 now names both inputs.
`Sessions::Admit` sends the configured count, not how many have joined, and zero on a refusal.
`JoinState` keeps it.

`GameClient/FieldView` holds the derived rows. `ClientFrame` owns one and rederives it after each join
reply is folded in: once on a seat, as a no-op on a repeat or a rejoin into the same match, and cleared
by a refusal. **So the Bot derives the field too**, which M2.6's miners will want. `App.cpp`'s two
`StartAnchor(2, …)` calls, the opening recenter and the hold recenter, now use the count from the join,
and the seat log names the field's size.

**"The host's and the client's entry points" had to be read, because the host does not generate the
field.** The host's side, since M2.1, does not generate asteroids at all. So the host's entry point is
the pair it sends and the client's is the pair it decodes. `SessionsTests` pins that every seat carries
the configured count and seed. `FieldViewTests` encodes the reply, drains it through `ClientFrame`, and
compares every row with `GenerateField` at counts 1, 2, 3, 4 and 8 and at the seed's extremes. **It cannot
catch x64 disagreeing with ARM64**, which is still the standing four-pair run. **For the grep**,
`HostTests` asserts that a match begins with one entity per station and nothing at any rock's position.
An update carries only world entities (ADR-024), so no rock reaches the wire. `grep -rn GenerateField
GameLogic` finds nothing.

### M2.4 — The asteroid variants · `GameClient` · hand · agent

**Read first:** [`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md), whose *Consequences* name this step's
whole problem; `TechnicalDesign.md` §6 and §7; `Design/design_handoff_meshes/`.

**Adds:** the rocks, as a **set of authored CMO variants** rather than one function driven by the match
PRNG. **This is what ADR-005 cost when it made a mesh a file**: a file is one rock, so variation comes
from the seed choosing among variants and applying its own yaw, pitch and scale jitter, and a variant set
repeats where a generator did not.

**Two figures move here and the second is owed at this step.** The field is **one instanced draw per
variant** where `TechnicalDesign.md` §6 used to state one for the whole field, and each variant is package
bytes. Settle the count against how obviously the set repeats at the tactical zoom, and write both
figures into ADR-005's *Measurements* and `TechnicalDesign.md` §6.

Asteroids, wrecks and debris **may be drawn above and below the plane** so the space reads as a volume;
[`ADR-001`](../ADR/ADR-001-the-playfield-is-a-plane.md) permits exactly that and is equally clear that none
of it is simulated and the host does not know it exists. **A visual offset must therefore live in the
client and never reach a `GameCore` record** — this is the step where R22's third coordinate would sneak
in if it were going to.

**Files:** `GameClient/AsteroidMesh.h` `.cpp`; the variant `.cmo` files and their package declaration;
`GameClient.vcxproj` + `.filters`.

**Done when:** the field draws as one instanced call **per variant**, rocks differ from one another at the
tactical zoom, the variant count and the package bytes are written into the two documents that state them,
and the visual vertical offset and the scale jitter exist only in client code — check your own diff for a
`z` that crossed into `GameCore`.

**BUILT, 2026-09-23, WITH ONE DEPARTURE FROM THE STEP AS WRITTEN: THE FIELD IS BAKED, NOT INSTANCED.** The
five variants were already delivered and packaged (`AsteroidA` to `E`). The step said "one instanced call
per variant". The ship pass's instance holds a position and a turn about Z. A rock turns on three axes,
scales and lifts, which the handoff's section 8 specifies, and a field never moves. So
`GameClient/AsteroidMesh` places each variant's rocks into one static mesh on the processor when the join
names the field. `App.cpp` draws each through the ship pass with one identity instance. That is five
draws, no second shader, and 552 KiB of upload heap at two players where instancing would have cost about
63 KiB. The trade and its figures are in [`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md)'s amendment.
**Whether that memory shows in frame time is owed on the device.**

**The look is drawn from the match seed on PCG32 stream 4**, a client stream, with six draws per rock:
variant, yaw, pitch, roll, scale percent and lift. **The scale is clamped so no two rocks touch.** The
generator's 150-unit spacing is smaller than `AsteroidE` at 1.35, so each rock is held to half its nearest
neighbor's distance over its mesh's exact sphere. That clamps 3.5% of rocks over 200 seeds, none below
0.75. The lift and the jitter never leave `GameClient`. `git diff -- GameCore` holds no `z` and no look.
`AsteroidMeshTests` pins determinism, the handoff's ranges, every variant appearing on the match seed,
no two rocks touching over 200 seeds at two and four players, normals staying unit and triangles staying
outward, and the bake's index offsets and its 16-bit limit.

**Figures written:** five variants, five draws, 102 KiB on disk and 16 KiB deflated, in ADR-005's
*Measurements* and `TechnicalDesign.md` §6 and §7. **Not established here:** that rocks differ at the
tactical zoom, which is looked at on the device. Nothing here was built with MSVC or run.

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

**BUILT, 2026-09-23**, as `GameLogic/UniformGrid.h` `.cpp`. `Rebuild` takes the world whole, in index
order, as a two-pass counting sort into one flat array: each cell's entries sit in index order, and a
rebuild allocates nothing once the array has grown. It is rebuilt, not maintained, because at 110 entities
an incremental index would be a second copy of every position to keep in step. **An entity outside the
square is clamped into the edge cell rather than dropped**, and a query's box is clamped the same way, so
the grid finds it. `Query` returns every live entity within the radius, inclusive, **sorted by
`EntityId`**. `Nearest` takes a filter, which is how M2.6 will ask for "owned and accepts ore" without
the grid knowing about ore. It walks the sorted candidates and keeps a later one only when strictly
nearer, so an exact tie goes to the lower identity. **Nothing calls it yet**: the host rebuilds no grid
until M2.6 has a query to make, so this step leaves the tick and its hash untouched.

`UniformGridTests` checks every query against a brute-force scan: 600 entities scattered over and past
the square, nine centers including the edges and corners, and radii from 0 to wider than the map. It also
covers cell-edge ownership and clamping, both sides of a cell boundary, a point exactly on the circle,
identity order after slots are freed and reused, and a stale identity. For `Nearest`, a four-way exact
tie placed so that cell order would pick a different entity, and 200 brute-force probes. **Compiled and
passed under g++ with a stand-in for the test framework**, not under MSVC.

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

**BUILT, 2026-09-23, AFTER TWO REGISTER QUESTIONS.** The design gave unloading neither a duration nor a
reach, and a mine order had no way to name a rock, because a rock is not an entity. **Q51**: the owner ruled
50 ore a second, once the hulls touch plus 20 units of slack. **Q52**: the rock's index in the generated
field. Both were ruled before the code.

**Where it lives.** A `MineOrder` sits on each `World` slot beside its `MoveOrder`, so creating or
destroying a slot clears it, and `CommandIntake::Apply` keeps its signature. **The cargo is on the order and
outlives it**: `StopMining` ends the phase and keeps the load. `World` also holds the field now, which
`Host::BeginMatch` sets from `GenerateField`. That is R23's host half, and it is what lets the intake refuse
an index past the field. Cargo is counted in thousandths of ore, so a laser's 20 a second is 1,000 a tick
and unloading's 50 is 2,500. In whole ore the unload would be 2.5 a tick.

**The loop.** `GameLogic/MiningSystem` runs after movement, as TechnicalDesign §2 orders it, in index
order, and rebuilds M2.5's grid first. Phase changes happen **inside the same pass**: a miner extracts on the
tick it arrives, leaves on the tick it fills, and heads back on the tick it empties. So every leg lasts
exactly its figure. **The one exception is a fresh order**, which starts moving a tick after it is given,
because mining runs after movement. `GameLogic/UnloadTarget` is the "nearest owned thing that accepts ore"
query, with ties broken on identity through `UniformGrid::Nearest`. "Accepts ore" is `acceptsOre` on the hull
row, carried into `DerivedStats`, and only the `Station` sets it. The mining range is derived too: the longest
reach among the tools that extract, not a sum. **A move ends a mine order and keeps the cargo.** A `Mine`
command skips any ship whose derived capacity is zero and still accepts. With nowhere to unload, a miner
waits full where it is.

**Pinned.** `MiningTests` covers:
- a full cycle against a rock 2,000 units out: 328 ticks out, 100 extracting, 328 back, 40 unloading, 794 a
  cycle, then the next cycle with no further order;
- a hold filling to exactly full from part-way;
- abandonment keeping 50 ore, and a second order filling in the 50 ticks left;
- a full hold going straight to unload;
- the unload query ignoring a nearer station of another player, and breaking a tie on identity;
- a miner with nowhere to unload waiting until a station exists;
- the intake refusing a rock past the field or a foreign miner, and skipping a fighter.

The two-laser derivation was already pinned at M1.2 (`MiningSumsOverTheSlots`). `DerivedStatsTests` adds the
mining range and `acceptsOre`, and `CommandTests` the rock index round trip.

**The determinism script now mines.** Idle miners are sent to a home rock every fifty ticks, fighters alone
take the fleet moves, and at tick 1,500 everything is moved, miners included. It delivers 37 holds. **The
pin moved, deliberately, from `0x37f846ed90b74ca1` to `0xc8f7f00e056d4d46`.** The new value was computed off
Windows, under g++ and clang at two optimization levels, which agreed. The old value still reproduces with
this code and the old script. **The four-pair MSVC run is owed**, and M1.14c's own check of the old hash has
to be run at M1.14c's commit.

**Not here, and whose it is.** Credits from deliveries: `MiningSystem::Deliveries()` produces them and M2.7
turns them into credits, so the script's flat grant of 15 a second stays until then. The client's mine
order is M2.8. A mining laser drawn to a rock lifted off the plane (M2.4) is a presentation question for
when the laser is drawn. **Compiled and run under g++ with a stand-in for the test framework, not under
MSVC.**

### M2.7 — Credits, and the readout · `GameLogic`, `GameClient` · both · agent

**Read first:** `GameDesign.md` §4; `OpenQuestions.md` Q36; ADR-003's per-player block; `Interface.md` §6.

**Adds:** credits accruing on unload, carried in the per-player block M0.9 already encodes, **and the
cargo bucket in the flags byte** — two bits, four buckets, which is what a fill bar needs and is all the
snapshot has room for (`TechnicalDesign.md` §4), and the top-left
readout M1.14 already draws going live — and **the income rate, which `Interface.md` §6 says appears
"once there is one"**, if it ships at all. **It may not.** `design_handoff_hud/` draws the credits panel
with no room for a rate and states that it has no data path; that reason is false, but the conclusion is
still open, and **Q36 settles whether before this step can say how**.

**Files:** `GameLogic/Economy.h` `.cpp`; `GameClient/Panels.cpp`;
`Tests/GameLogicTests/EconomyTests.cpp`.

**Done when:** credits reach the client only through the snapshot; the income rate is computed the way
**`OpenQuestions.md` Q36** settles — the snapshot carries no income field, two derivations fit the data the
client has, and the constraint on both is that **a client that derives income from the catalog is a client
doing simulation** (R19); and building deducts at start as M1.6 established.

**BUILT, 2026-09-23, AFTER TWO RULINGS.** **Q36**: no income rate, the change flash only. **Q53**: the
handoff's four cargo chips needed five states and the wire's two bits gave four, so cargo took one of the
flags byte's three spare bits. The record is still twelve bytes. The field audit in `DatagramBudget.py`
goes from nine recoverable bits to eight, and the skill quoting it moved with it.

**The host.** `GameLogic/Economy` turns each tick's `MiningSystem::Deliveries()` into credits through
`BuildSystem::Grant`, the balance's one owner. One ore is one credit, from GameDesign §4's "100 credits of
capacity". **A per-player remainder of thousandths is kept**, so unloading's 2.5 credits a tick lands a
hold as exactly 100. `RecordOf` takes the world and slot rather than the entity, because the cargo lives
on the slot's `MineOrder`, and it writes `CargoChips` into the flags. Quarters are rounded up, so any ore
lights a chip. **Credits reach the client only through the per-player block**, as before.

**The client.** `GameClient/CreditFlash` is a clock in and an alpha out: cyan on a gain with τ 120 over 400
ms, amber on a spend with τ 180 over 600 ms. The first reading only records, and a link that is not up
resets it, so a rejoin does not flash. `Panels` draws it at `CREDITS_FLASH`. The selection panel draws the
four chips, `ORE` lit and `TRACK` under a keyline empty, **only for a design whose derived capacity is not
zero**. A group lights its members' mean, rounded to nearest. `CheckHudGeometry.py` now claims the cargo
rows, which were on its not-yet-drawn list.

**The determinism script's income is mined**, replacing the flat 15 a second. It still builds a fleet of
eleven and spends for it. The pin moved again, from `0xc8f7f00e056d4d46` to `0x18e094912655348f`,
computed under g++ and clang at -O0 and -O2, which agreed. **The four-pair run is owed.**

**Pinned:**
- `EconomyTests`: exact holds, separate remainders, and a full cycle through the loop paying its owner.
- `HostTests`: cargo chips on the record.
- `UpdateTests`: the chip mapping and the three bits.
- `HudLayoutTests`: the aggregation, the chip row present or absent, and the flash's rect, color and alpha.
- `CreditFlashTests`: the curve.

**Compiled and run under g++ with a stand-in for the test framework, not under MSVC; the flash has not been
looked at on the device.**

### M2.8 — The tap on an asteroid · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §4's table.

**Adds:** the third row of the tap table. **An asteroid with ore: mine it — miners in the selection take
it, the rest move to it.** A mixed selection therefore does two things from one tap, which is the design's
intent and is worth a test of its own because it is the only row that splits a selection.

**Files:** `GameClient/TapOrder.cpp`; `Tests/GameClientTests/TapOrderTests.cpp` extended.

**Done when:** a mixed selection issues a mine order for the miners and a move order for the rest, in one
gesture; and the host's validation accepts both, which means M0.10's validation is exercised by a case it
has not seen.

**BUILT, 2026-09-23.** **Rocks are hit-test candidates at their drawn centers.** `RockPickPoint` carries a
rock's plane position, the lift M2.4 drew it at, and its field index. `WorldToScreen` generalizes
`PlaneToScreen` to project a point off the plane. `App.cpp` computes the points when it bakes the field.
They sit at the asteroid tier, below own ships, own structures and hostiles, so a ship over a rock takes
the tap. **An unowned record is no longer a candidate**: `TierOf` put one in the asteroid tier, from before
rocks existed, where it would now have been ordered to mine rock zero. `TheTierOrderBeatsDistance` uses a
real rock for its asteroid instead.

**The split.** `Selection::Tap` returns `Mine` with the rock's index and its plane position.
`SplitForMine` separates the selection by derived capacity, the same test the host applies. An identity the
update no longer carries goes with the movers, for the host to judge. `App.cpp` sends a `Mine` command for
the miners and a `MoveTo` to the rock for the rest, **as two commands in one packet**, with one order marker
each.

**Pinned:**
- `HitTestTests`' new `TheTapOnAnAsteroid`: the mixed tap end to end at a rock lifted 240 units, an unknown
  identity going with the movers, a ship over a rock winning, no verb with nothing selected, no unowned
  record as a candidate, and pick points following the field and its looks.
- `MiningTests`' `ASplitOrderFromOneTapIsAcceptedWhole`: the host accepts both commands from one packet,
  the miner takes the standing order, the fighter heads for the rock, and the acknowledgment reaches the
  second command.

**Compiled and run under g++ with a stand-in for the test framework, not under MSVC; nobody has tapped a
rock on the device.**

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

### M2.10b — The module mesh, and telling four modules apart · `GameClient` · hand · agent

**Read first:** [`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md), whose status line corrects the
shape list this step exists to complete; [`ADR-015`](../ADR/ADR-015-the-base-is-built-from-modules.md)'s
*Decision*; `OpenQuestions.md` Q37; M1.9's reader and its identity-to-mesh map;
`Design/design_handoff_meshes/`.

**This step was missing and the milestone could not have finished without it.** ADR-005 counted the MVP's
shapes the day before ADR-015 made a module a separate drawn entity, so M1.9 builds the hulls and the
station, M2.4 builds the asteroid, and **nothing built the thing M2.11 places on the map.**

**Adds:** `ModuleFrame` at the size Q37 settles, and **the scheme that tells the four variants apart** —
no new mesh code either way, because M1.9's reader already reads whatever this step ships.

**That second half is a design problem and not a detail.** ADR-015's whole argument is that *"a raid that
kills your ore processor and leaves has done real damage without touching your station"*. That move needs
**an attacker who can pick the right target and a defender who can see what they lost**, from the
near-top-down tactical camera (`Interface.md` §5) — the client knows each module's design identity from
the snapshot's own byte ([`ADR-003`](../ADR/ADR-003-the-record-and-the-command.md)), so what is missing
is what it draws with it, not what it knows.

**[`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md) is what makes this tractable and it is the largest
single thing that decision buys.** The old rule left one mesh, the team-color attribute and no icons as
the whole budget; a mesh being a file means **four authored module meshes are available**, at a draw call
and a file each. Take them if the distinction needs them and say so; one mesh reading four ways is still
cheaper if it works.

**Files:** `GameClient/HullMesh.cpp` extended; the module `.cmo` files and their package declaration;
`Tests/GameClientTests/` only if the identity-to-mesh map becomes worth pinning.

**Done when:** a module draws as an instanced call off the same function as the hulls, four sit inside the
400-unit placement radius without touching (M2.10), and **a shipyard is distinguishable from an ore
processor at the tactical zoom** — which M2.13 judges rather than this step.

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

**ADR-005 names this risk against this milestone, and replacing that record removed its cause without
removing the constraint.** `GameDesign.md` §1 promises "ships that bank as they turn and read as
silhouettes" and `Interface.md` §5 couples pitch to zoom so the tactical view is near top-down. The old
mechanism — two hulls emitted by one shared parameterised function tending to be the same shape at two
scales — is gone now that each hull is modeled on its own. **What has not moved is 17.1 units to the
authored pixel**, which puts a `Scout` at 3.5 pixels and a `Frigate` at 5.3, seen from almost directly
above. A modeler can draw two different ships and still hand over two identical four-pixel smudges.

**Done when:** a field of miners and fighters is looked at from the tactical zoom on the device and the two
are distinguishable at a glance; **and a base of four modules is looked at the same way and a shipyard is
distinguishable from an ore processor** (M2.10b), because ADR-015's raid depends on picking the right
target and this gate is the only thing that checks it. **If either fails, the answer is a shape-coded
overlay drawn by the interface, not more triangles** — which ADR-005 named while it still ruled meshes
were functions and which survives it, because the overlay was never about how the geometry was made. That
is an ADR rather than a quiet addition to the world pass.

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
