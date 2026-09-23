# Game Design — *Outpost Commander*

What the game is. [`AGENTS.md`](../AGENTS.md) says how the code is written; this document says what the
code is for. Where a decision here constrains the *shape* of code it is raised as an ADR under
[`ADR/`](ADR/README.md) and, if it belongs there, proposed as an `AGENTS.md` §5 rule citing the section
here that is its source — the design is the source and `AGENTS.md` is the rule, in that order.

**Status: DRAFT.** Nothing here is settled until the owner accepts it. The four questions that shaped it
were answered on 2026-09-20 and are recorded in [`OpenQuestions.md`](OpenQuestions.md); the questions it
raises and does not answer are there too.

**Every number in this document is a starting value, not balance.** The structure is the proposal; the
figures are there so the first build has something to run and so the arithmetic in
[`TechnicalDesign.md`](TechnicalDesign.md) has real quantities behind it. Expect all of them to move.

---

## 1. What it is, and what the MVP is not

Four commanders wake in the same volume of space. Each has a station, a handful of credits and an
asteroid field within reach. Ships are not chosen from a list — they are *designed*, from a hull, a
drive and whatever goes in the hull's slots, and the design is what the station builds. The field in
the middle is richer than the one you were given, which is the entire reason anyone fights.

Two lineages, and they contribute different halves:

- ***Homeworld*** gives the feel: a fleet, not an army; an empty volume with a sense of scale; ships
  that bank as they turn and read as silhouettes; collectors that make the economy a thing you have to
  defend rather than a number that goes up.
- ***Warzone 2100*** gives the spine: a unit is a **composition**, not a type. Hull plus drive plus what
  is in the slots. Research unlocks components, components make designs, designs are what you build.
  That loop is what gives a long match a direction.

**Be clear about what the MVP is: it is the Homeworld half.** The component model is in the simulation
from the first line (§6), but there is no designer screen and no research in the MVP, so what a player
actually sees is three fixed ships. The Warzone half arrives with the designer and the research tree
(§9), and the whole reason the component model is built early is that retrofitting it later would
rewrite damage, cost, build time, movement and the wire format in one change.

### The playfield is a plane

**Ships move on a single plane. The camera orbits and zooms above it.** Asteroids, wrecks and debris sit
visually above and below it, so the volume reads as a volume, but nothing the simulation owns has a
third coordinate. This is [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) and it is the most
consequential decision in the document.

It is taken because of touch. `AGENTS.md` R21 makes a gesture the only way in, and the vocabulary is
`Tapped`, `Holding`, and a manipulation's translate, scale and rotate. **A tap is a ray, and a ray has no
depth** — so a move order into a volume needs two independent inputs, which is exactly why *Homeworld*
invented the move disk and why it needed a mouse and a modifier key to drive it. On a tablet held in two
hands, that gesture competes with the camera for the same fingers. A plane makes a move order one tap,
unambiguously, and it makes pathfinding, collision and spatial indexing two-dimensional everywhere
downstream.

What it costs is the tactical z-axis — attacking from above, hiding below the plane, the vertical
envelopment that is the thing *Homeworld* is remembered for. That is a real loss and it is taken
deliberately. What survives is everything else: fleets, formations, strike craft against capitals,
collectors feeding a base, and the silhouette.

---

## 2. The session

**One area, four commanders, no lobby.** A match is four slots. Any slot is a human or an AI, and the
match is configured on the host before it starts — there is nothing to negotiate once it is running.

**The MVP ships two of those four slots** (§10). That is scope, not design: the generator's symmetry, the
AI count and the snapshot's per-player blocks are all sized by a runtime player count, so the third and
fourth slots are a configuration value rather than a change. What two slots buy is a match of about five
minutes instead of twenty, which is the only mechanism this project has for answering the balance
questions — twenty matches in an evening, rather than one match and a set of opinions.

**A solo match against three AI players is a first-class configuration**, and it is the only way the game
is testable before there are four people to test it with. It is not a single-player *mode*: the
architecture is unchanged, the host is still a separate process, and the client still links no
simulation (`AGENTS.md` R19). What it costs is stated plainly in [`TechnicalDesign.md`](TechnicalDesign.md)
§5 — a packaged client cannot reach a host on the same machine without a developer-mode loopback
exemption, so even solo play is a two-machine arrangement unless you grant one.

Four AI players and no human is not a game; it is a test fixture, and it is worth having for exactly
that.

**Victory is the last station standing.** A station destroyed eliminates its owner and removes their
remaining ships from the field. The alternative — leaving a beaten player's ships alive to be hunted —
turns the last ten minutes of every match into a search problem, and no amount of tuning fixes that.

**A player who disconnects keeps their slot.** Their ships hold position and keep whatever autonomous
behavior they have, the slot is held indefinitely, and they may reconnect -- recognized by a session token
the host issued when they first joined ([`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md)) — which is mechanically free
because every update is self-contained (`TechnicalDesign.md` §4), so a returning client is current again
within one sweep of the accumulator. The
cost is that an abandoned fleet sits on the board as free kills, and that is accepted rather than solved:
an AI taking the slot is the better answer and it waits for M4, when there is an AI that can start from
arbitrary mid-match state. Suspend and resume are the same path (`Interface.md` §7).

There is no pause and no save in the MVP. A host with no clients keeps simulating.

**Four is the game's number and stays so.** A host started in a stress configuration may seat more
([`ADR-023`](ADR/ADR-023-the-player-count-is-configurable.md)), and the wire carries any number of them
([`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md)). That configuration exists to load the
host, and it doesn't claim the fair starts or the balance this section is about.

---

## 3. The area

The area is generated from the match seed, deterministically, by code in `GameCore` that both the host
and the client run (`TechnicalDesign.md` §3). The client is not *told* the map; it derives it, which
takes a large static payload off the wire and means the two sides cannot disagree about where an
asteroid is.

**At four players the adjacent crossing is 8,485 units — 60.6 seconds — and falls out of §7's window
entirely.** Raids get materially shorter the moment the third and fourth slots ship (§2), so the radius
may have to become a function of the player count. That is a consequence to measure at M4, not a reason to
move the number now.

**It is rotationally symmetric about the center, to the player count.** At four players one quadrant is
generated and copied at 90°, 180° and 270°; **at the MVP's two players one half is generated and copied at
180°**, which on integer positions is a negation and therefore exact. This is the cheapest possible
fairness guarantee: every player's start is the same start, so no balance analysis is needed and no seed
can be unlucky. It is also visibly artificial, and that is the trade — a procedural generator that is
*fair* without being symmetric is a research project, and this is an MVP.

**M0 and M1 run one fixed seed with a hand-checked layout.** The generator is still `GameCore` code that
both sides run (R23); it simply has one input until M2, which keeps a whole class of "is the map wrong or
is the game wrong" question out of the first two milestones.

What a seed produces:

| | |
|---|---|
| **The square** | 16,384 world units on a side, centered on the origin. A world unit is nominally a meter. A fighter crosses it in about two minutes, a miner in under three. |
| **Four start anchors** | One per quadrant, **6,000 units** from the center, on the axes. A station spawns on each, facing the center. Two opposed stations are then 12,000 apart — **85.7 seconds at the Fighter's 140 u/s**, inside §7's 80-to-100-second crossing, which is the arithmetic the raid balance rests on. |
| **A home field** | A small asteroid cluster within about 1,500 units of each anchor. Enough to open on, not enough to win on. |
| **The asteroid count** | **Ten in each home field, and two contested clusters of six in each player's region**: 22 a region, 44 on a two-player map. It sets the sparse ore budget from M3 and how much a home field is worth holding. Provisional, and named in `GameCore/Generator.h` so it can move (`OpenQuestions.md` Q26). |
| **Contested fields** | Richer clusters toward the center, reachable by everyone. This is the map's only real proposition. |

Nebulae, wrecks, hazards and anything that affects sensors are not in the MVP. The generator's interface
is a seed in and a list of placed objects out, so adding a kind later does not change its shape.

---

## 4. The economy

One resource, **credits**. There is no second currency and no upkeep in the MVP.

A **miner** flies to an asteroid, extracts until its cargo is full, flies to the nearest place that
accepts ore, unloads, and **goes back to the same asteroid and does it again**. That is the entire loop,
and it is a loop rather than a number because the miner is a ship on the map that can be shot. An idle
miner with a full hold and a dead station is the economy failing in a way a player can see and do
something about.

**A mine order is the only standing order in this design.** Every other order completes: a ship told to
move arrives and is finished. A miner told to mine **keeps mining until it is told to do something else**,
which is the whole of the cycle above and is why an economy runs without a player shepherding it. The
state machine is five states — going to ore, extracting, going to unload, unloading, and back — all of it
on the tick, all of it integer.

**Where it unloads is a query, not a constant.** The miner flies to the *nearest thing you own that
accepts ore*. Today that set has one member, your station. It is written as a set because a mining factory
placed out at a contested field is an obvious later feature, and the whole value of it — a shorter round
trip — depends on the miner already asking "what is nearest" rather than being told "the station".

**Cargo capacity is a derived stat like any other** (R24). It is carried by the mining tool, not by the
hull, and it is **summed over the hull's slots** exactly as mass and cost are — so a two-slot hull with two
mining lasers carries twice as much and extracts twice as fast, and a "heavy miner" later is a table row
rather than a mechanic. A ship with no mining tool has zero capacity and cannot be given a mine order.

**Asteroids are finite from M3, and infinite before it.** Finite asteroids are what make the contested
fields worth contesting; an infinite home field turns the map into scenery and the match into pure
military arithmetic, so the mechanic matters and it arrives with the milestone that needs it. Until then
an asteroid is an inexhaustible point, which removes husk state, miner retargeting and — because ore
remaining is the only per-asteroid state the simulation owns — **the entire question of replicating
asteroids at all** from M0 to M2 (`TechnicalDesign.md` §4).

From M3: an exhausted asteroid stays on the map as a husk and a miner with no order retargets the nearest
one with ore left. The cost is that a player who ignores their miners eventually finds them idle, which is
a management burden the MVP accepts rather than solves.

Starting values: a station begins with 1,000 credits. A `MiningLaser` carries 100 credits of capacity and
extracts at 20 per second, so the one-slot miner fills in five seconds; a round trip to the home field is
roughly thirty seconds, which puts one miner at about 2.5 credits per second. **From M3, when asteroids
become finite, a home field holds enough for a long opening and not for a match** — before M3 it holds
everything, because there is nothing to exhaust.

---

## 5. The station

One per player, **fixed**, spawned on the start anchor. It never moves, which removes base pathfinding,
docking geometry and a whole class of AI problem from the MVP. A mothership is a hull like any other and
can be added later without changing anything here, because position is mutable in the entity model from
the first line — the station simply never asks to move.

The station does four things: it builds, it receives ore, it shoots at whatever comes too close, and it
dies. **Building is a single queue.**
A design is selected, it is added to the queue, credits are deducted when the item starts, and the ship
appears at the station's spawn point when the item finishes. There is no rally point in the MVP; new
ships sit where they appear.

**Building goes at a rate of 20 credits of cost a second** (`OpenQuestions.md` Q47), so build time is a
design's cost over the rate and falls out of the composition the way mass and speed do: a Miner in 7.5
seconds, a Fighter in 15. The shipyard (§6) multiplies the rate. It sits just above the income a running
economy earns, so building is very slightly faster than earning and the multiplier has something to do.

**A station is a hull with slots, like everything else**, and it carries two `PointDefense` mounts. It
has no drive, which is the only thing that distinguishes it from a ship — §6's model allows a hull without
one, and giving the station hull a drive later is how a mothership arrives.

**Its hull is 8,000, down from 12,000, because the siege unit was cut** (§10). The battleship was what a
station was priced against; without it, ten fighters standing off at 500 units take about a minute to
bring one down, which is a siege a player can mount and lose.

**The point defense outranges nothing, and that is deliberate — say it out loud or it reads as a bug.**
`PointDefense` reaches 400 units; a `MassDriver` reaches 600. **A fighter can therefore stand off at 500
and shell the station untouched.** What the point defense protects is the *unloading area* — a raider that
chases a fleeing miner home crosses 400 and dies in under six seconds — not the station itself. If it
outranged the fighter instead, a station with no siege unit left in the MVP would simply be unkillable.

**So it kills a loiterer, not a besieger and not a fleet.** It is weighted against small and medium hulls
(§7), so anything that comes close dies quickly. That is the whole intent: the area around the station
is a **safe zone**, which gives miners
somewhere to retreat to and turns an early raid into a tactical exchange rather than a free kill. An
undefended station makes a raid a threat, which reads well on paper; in practice it means finding out your
economy is dead rather than seeing it happen.

**The safe zone stops well short of the home field, and that is the point.** At 400 units against a field
about 1,500 out, **miners at the rocks are raidable** — so an early fighter has a job, and a defender has
a real choice between escorting and expanding. Retreating to the station is not free: it costs the cargo
run. Extending the range to cover the field would make the opening simply safe and the early game pure
build-up, with nothing happening until someone reaches the middle.

### The base is built out of modules

**A station on its own is a small base, and the rest of it is built.** A **module** is a separate entity
placed near the station, owned, upgradeable, and **destroyable on its own** — which is the whole point of
making it an entity rather than an upgrade inside the station
([`ADR-015`](ADR/ADR-015-the-base-is-built-from-modules.md)). A raid that kills your ore processor and
leaves has done real damage without touching your station, and that move is what *Warzone 2100* base
building is for.

**A module is a composition like everything else** (R24): a `ModuleFrame` hull with one slot, no drive,
carrying one module component. **Each upgrade level is its own component**, so upgrading replaces
`ShipyardL1` with `ShipyardL2` and the derived-stat function is untouched.

**The MVP has two working modules**, at two levels each:

| Module | What it does | L1 | L2 |
|---|---|---|---|
| **Shipyard** | Raises the station's build rate. From M4 its levels also gate heavier hulls and the designer. | 400 cr, ×1.5 | 700 cr, ×2.0 |
| **Ore processor** | Raises what a delivered cargo is worth. | 350 cr, +25% | 600 cr, +50% |

**The research station is designed and not built until M4**, when there is research for it to do (§9).
A module that costs credits and does nothing is the mistake §6 already made once with the heavy design.

**Placement is a tap and costs no new gesture.** With your station selected and a module chosen, a tap on
empty space within **400 units** places it — an interaction that was dead, because a tap on empty space is
a move order and the station cannot move (`Interface.md` §4).

**400 is the point-defense range, so the safe zone means exactly "your base".** The consequence is
positional and intended: a `MassDriver` reaches 600, so **a fighter standing off at 500 can shell the
modules on the near side while staying outside point-defense cover**. Which side of your station you build
on is a decision.

**Four modules to a station.** That cap is as much a replication budget as a design one
(`TechnicalDesign.md` §4), and raising it is a protocol decision rather than a game one.

Beyond modules there are no other structures in the MVP. No turrets, no outposts — the station's own
defense is a component in its slots, not a building you place.

---

## 6. Ships are designs

**This is the Warzone 2100 spine, and it is built now even though its interface is not.**

A ship is a **hull**, a **drive**, and a component in each of the hull's **slots**. Nothing in the
simulation knows what a "fighter" is; it knows a design, and a design is a composition. Every stat a
ship has is *derived* from its components by one pure integer function in `GameCore`, which both the host
and the client call and which has a test suite over it — it is the single place a rule about what a ship
can do is allowed to live (`AGENTS.md` R19).

### The catalog

Five hulls, two drives, three weapon components and four module components. **The station is one of
the hulls and so is a module frame**, which is what makes §5
possible without a second kind of thing in the simulation, and `Cruiser` stays in the catalog although
nothing in the MVP builds it — the derivation function and its tests cover every hull, and reinstating a
heavy design later is a table row (§10).

| Hull | Slots | Hull HP | Mass | Size class | Hit value |
|---|---|---|---|---|---|
| `Scout` | 1 | **450** | low | Small | — |
| `Frigate` | 2 | 600 | medium | Medium | — |
| `Cruiser` | 4 | 3,000 | high | Large | — |
| `Station` | 2 | **8,000** | — | Large | **300** |
| `ModuleFrame` | 1 | 1,500 | — | Large | **300** |

**Only the two base structures carry a hit value**, and a dash means the hull is damaged through §7's
size-class table instead. The two mitigation models are the cost §7 names.

| Drive | Character |
|---|---|
| `IonDrive` | Balanced thrust, cheap. |
| `BurnDrive` | More thrust for more mass and more cost. |

| Slot component | What it does | Range |
|---|---|---|
| `MiningLaser` | Extracts 20 ore a second and carries 100 of it. Does no damage. Capacity and rate both sum over a hull's slots. | 200 |
| `MassDriver` | 25 damage per second per mount. | 600 |
| `PointDefense` | 60 damage per second per mount. Station slots only. | 400 |

| Module component | What it does | Cost |
|---|---|---|
| `ShipyardL1` / `L2` | Station build rate ×1.5 / ×2.0. From M4 its levels also gate heavier hulls and the designer. | 400 / 700 |
| `OreProcessorL1` / `L2` | A delivered cargo is worth +25% / +50%. | 350 / 600 |
| `ResearchStationL1` | Designed at M4, when there is research for it to do (§9). | — |

**A drive is optional.** A hull with none does not move, which is what a station and a module frame both
are. **Speed is thrust
divided by mass**, and mass is the hull plus everything in it — so a `Frigate` carrying two mass drivers
is slower than an empty one, and that falls out of the arithmetic rather than being written down anywhere.
Turn rate derives the same way, and a ship turns while it flies. Because speed and turn rate both fall
with mass, every ship turns on the same radius, and none ever orbits its target (`OpenQuestions.md` Q59).

### The two designs the MVP ships

The player's two buildable ships are not special cases in the code. They are rows in a table:

| Design | Hull | Drive | Slots | Cost | Speed |
|---|---|---|---|---|---|
| **Miner** | `Scout` | `IonDrive` | 1× `MiningLaser` | 150 | 100 u/s |
| **Fighter** | `Frigate` | `BurnDrive` | 2× `MassDriver` | 300 | 140 u/s |

**The battleship is cut from the MVP and the reason is arithmetic, not taste.** It cost 2,400 credits
against an income of about 15 a second — **160 seconds of total income, spending nothing on defense** —
while a strike force crosses the map in roughly 80 to 100 seconds. It was a unit that no match would
ever contain, which made it untested content: one more mesh, one more component, one more damage row and
one more branch in the AI, none of which anything would exercise. Worse, the question the design most
wanted M3 to answer — whether speed counters mass (§7) — **could not have been answered at M3**, because
the mass never appears. Cutting it makes that deferral honest instead of accidental, and ADR-006 makes
reinstating it a table row.

**Note what the miner is:** it is not a ship type. It is a `Scout` with a mining tool where a weapon would
go — and the station is a `Station` hull with two point-defense mounts and no drive. That three things
that look like three kinds of object fall straight out of one component model, with no special case
anywhere, is the evidence that the model is worth building before its interface exists.

---

## 7. Combat

A ship with a weapon and no order engages the nearest hostile in range on its own. A ship with an attack
order pursues its target.

**Ships ordered to a point are given distinct destinations, not the same one.** On a plane, fifty ships
sent to one coordinate stack; in a volume they would have missed each other in the third dimension, so
this is a cost the plane induces ([`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md)) and it was
previously named once and owned by nobody. The order assigns each selected ship a **slot on a ring around
the destination**, ordered by entity identity so the assignment is deterministic, with the ring sized to
the selection. A formation system later is this same assignment with a different slot layout.

**A ship does not fly through anything solid.** A station, a module or another ship whose keep-out
circle lies across a ship's line gets steered around, on the side the ship is already on, and the route
is recomputed every tick rather than stored (`OpenQuestions.md` Q60, Q61). Three exceptions keep a
fleet from jamming. Ships sent by one order ignore each other. A ship flying the same way is part of the
same stream. And only what is within 400 units counts.

**A module is a target like anything else.** Its hit value quarters what reaches it, so one fighter needs
about two minutes to kill a module and three need forty seconds. That is deliberate — a module is a raid
objective a player has to commit to, not something a passing fighter removes. When a station dies its
owner is eliminated and **their modules go with their ships** (§2).

**Damage to a ship is `base × modifier[weaponClass][targetSizeClass] / 100`**, integer throughout. The
modifier table is the whole of the rock-paper-scissors between ships, and it is six numbers:

| | Small | Medium | Large |
|---|---|---|---|
| `MassDriver` | **70** | 60 | 25 |
| `PointDefense` | 120 | 90 | 30 |

Mass drivers hurt small things and scratch heavy hulls; point defense is the mass driver taken further —
it shreds anything small that loiters and is irrelevant to anything large.

### The base is not damaged directly; it mitigates

**A station and a module do not take damage through the table above.** What reaches them goes through a
**hit value**, a stat of the structure:

```
damage = base × 100 / (100 + hitValue)
```

**Integer throughout, and it multiplies before it divides.** Writing it as `base × (100 / (100 + hitValue))`
in integers truncates the fraction to zero before it multiplies anything, so every shot does no damage at
all — the same class of silent arithmetic fault the rounding question below already warns about. A hit
value of 0 is no mitigation; 100 halves what lands; 300 quarters it.

| Structure | Hull HP | Hit value | What lands |
|---|---|---|---|
| `Station` | 8,000 | **300** | a quarter |
| `ModuleFrame` | 1,500 | **300** | a quarter |

**Hit value is derived, not baked on a type** (`AGENTS.md` R24). The hull carries it and a component may
add to it, summed by the same pure integer function in `GameCore` that derives mass and speed — which is
what leaves room for an armor module later without moving anything here.

**Nothing in the raid arithmetic moves, and that is the point of the number chosen.** 300 is the hit value
that reproduces the Large column exactly: `100 / (100 + 300)` is `0.25`, which is what a `MassDriver`
already did against a Large hull. A fighter still needs about two minutes to kill a module and three still
need forty seconds; a station still absorbs roughly ten and a half minutes of one fighter. **The formula
changed and the balance did not**, which is what makes it safe to take before M3 has played a match.

**What the curve buys over the table cell.** The base's toughness is now a number on the *structure*
rather than a cell shared with every Large thing in the game, so a module can be made softer than a
station without touching a weapon's row. And the returns diminish — each point of hit value is worth less
than the last, and `100 / (100 + hitValue)` approaches zero without ever reaching it — so **no stack of
defensive upgrades can make a base immune**, which a flat percentage cannot promise.

**What it costs, and this is a real cost rather than a caveat.** There are now two mitigation models in
one game and a reader has to know which one they are in: ships use the table, base structures use the
curve. It also collapses the per-weapon distinction against the base, where `PointDefense` did 30% against
Large and a `MassDriver` did 25%; both now do whatever the structure's hit value says. That column was
close to dead content — point defense is station-slot-only at range 400 and two bases are never that close
— but it is **gone rather than deferred**, and a weapon meant to be better against structures now needs a
penetration term rather than a table row.

### The raid arithmetic, which was degenerate and is not any more

**The first version of these numbers solved the game, and the fault was structural rather than a bad
value.** A miner was simultaneously the cheapest unit, the lowest-hull unit, and the unit that took *full*
damage from the cheapest weapon — three independently reasonable choices whose product was not checked:

| | was | now |
|---|---|---|
| One fighter kills one miner | **4.0 s** | 12.9 s |
| One fighter kills one fighter | 20.0 s | 20.0 s |
| Defense slower than offense by | **5.0×** | 1.6× |
| Three fighters kill six miners in | **8.0 s** | 25.7 s |
| Miners lost fleeing 1,500 units to the station | **all six** | about 3½ |

At 5× a raider destroyed 750 credits of miners in the time a defender destroyed 300 credits of fighter,
before counting the lost income — so the dominant opening was an all-in strike, the dominant reply was the
same, and the economy was decoration. **`Scout` hull is now 450 and `MassDriver` does 70% against Small**,
which makes a raid an exchange rather than a slaughter.

**A miner with no order that is fired upon flees to its station.** Thirty lines, and without it the
defender must be watching the right part of a 16,384-unit map at the right moment to have any counterplay
at all.

**The counter to mass is still speed, and now nothing in the MVP tests it.** A `Frigate` moves at 140
units per second and a `Cruiser` at 50 — strike craft pick the fight and leave — and
[`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md) makes disengaging actually work, because a
ship out of range takes no further damage from a shot already fired. But the MVP builds no `Cruiser`, so
**this is deferred rather than answered**, honestly and on the register.

**A weapon resolves its damage on the tick it fires.** There are no projectile entities in the
simulation and none on the wire; the host emits a *fire event* which the client draws as a tracer or a
beam. At the MVP's scale this removes roughly as many entities as there are ships, and with them a
doubling of the replication budget. This is [`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md),
and it says what reopens it: the first weapon whose travel time is supposed to *matter* — a torpedo you
are meant to be able to shoot down — is a real entity, and cannot be anything else.

A destroyed ship leaves a wreck, which is presentation and not simulation: it is spawned by the client
from the death event, it decays on a client-side timer, and the host never knows it exists.

---

## 8. The AI

An AI occupies a slot and issues the same orders a human does, through the same command path. It is not
given information a human in its position would not have, and it is not given a discount — a cheating AI
is untestable, because you cannot tell a bug from the cheat.

The MVP's AI is a small state machine, and the ambition is deliberately low: keep a target number of
miners alive and mining, build military with the surplus, and send it at the nearest enemy station when
it has enough. That is the whole of it. It exists so the game is playable and testable by one person,
not so it is a worthy opponent.

An AI runs on the host, inside `GameLogic`, on the simulation's tick and under the same determinism rules
as everything else (`AGENTS.md` R16) — no wall clock, no unordered iteration that reaches an outcome, and
the match's pinned PRNG for anything random.

Difficulty levels, personalities and anything resembling strategic planning are post-MVP.

**A bot client is not this AI.** [`ADR-022`](ADR/ADR-022-a-bot-is-a-headless-client.md) (Proposed) adds
headless clients that join like people, run many to a process, and load the host from outside it. They are
a stress harness, and nothing in this section waits on them or is replaced by them. **Whether this AI is
written so that a bot can run the same code is [`OpenQuestions.md`](OpenQuestions.md) Q48**, and it has to
be settled before M4.5 is written.

---

## 9. Research, and what the MVP must not foreclose

**There is no research in the MVP**, and there is no designer screen. When both arrive at M4 they arrive
**in a module** — the research station of §5, which is designed now and built then, so that research has a
place on the map that an enemy can take away rather than being a menu.

The rest of this section is unchanged: Both are the next thing after it,
and the design is arranged so that neither needs the simulation changed to arrive:

- A **design** is already a composition of components, already stored by identity, already the thing the
  build queue holds and already the thing the wire format names. The designer is a screen that writes a
  new row into a table the simulation already reads.
- A **component** already carries everything the derived-stat function needs. Research adds an
  availability gate — a set of component identities a player may use — and nothing else. The gate is not
  in the MVP; the fact that a component is referred to by identity rather than by being hardcoded is.

What research *should* be is not settled here, because it is a long way off and settling it now would be
guessing. The one thing it must respect is the MVP's structure: research unlocks components, components
make designs, designs are what a station builds.

---

## 10. Scope

Five milestones to the MVP. The ordering is by **risk**, not by feature: the first milestone contains
almost no game and all of the things that can turn out to be impossible.

| | | |
|---|---|---|
| **M0** | **The wire** | The host opens a UDP socket and simulates one moving entity. The client connects over `DatagramSocket`, receives snapshots at 20 Hz, interpolates, and draws one shape in Direct3D 12 — **fullscreen**, world at 1440 × 960 scaled 2×, interface pass at physical resolution. A tap sends a move order, a local marker appears at once, and the shape goes there. No game at all — this proves the tick, the packet format, the two socket APIs talking to each other, the D3D12 frame, the two-pass renderer, the gesture seam, and — on an actual Surface Pro — whether the loopback exemption makes a single-machine loop usable. **It also measures tap-to-visible latency, which is the number that decides how the game feels.** Everything after this is content. |
| **M1** | **The fleet** | Two stations, the two designs, the current build item, move orders with ring assignment, selection by tap and by double tap. **A generated sky — a star field and a galaxy band from the match seed** ([`ADR-019`](ADR/ADR-019-the-sky-is-generated-from-the-seed.md)). Two clients on one host. Host-side command validation. |
| **M2** | **The field** | The procedural generator on a real seed, asteroid fields, miners, the credit loop. Asteroids are inexhaustible. **Modules**: the `ModuleFrame` entity, placement by tap inside the build radius, the shipyard's build rate and the ore processor's cargo multiplier, both at two levels ([`ADR-015`](ADR/ADR-015-the-base-is-built-from-modules.md)). |
| **M3** | **The fight** | Weapons, the damage table, the station's point defense, miner flight, **modules as targets and the near-side standoff they create**, **the off-screen damage alert and hull bars in the world** ([`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md)), destruction, elimination and victory — **and a match that restarts on a new seed**, so twenty can be played in an evening. Finite asteroids arrive here. A stub AI that builds and attacks, so a match can be played by one person. |
| **M4** | **The opponent, and the other two slots** | The AI of §8; three and four players; the `Cruiser` reinstated as a design with its weapon and its damage row — the milestone that can finally answer whether speed counters mass. **Research, and the research station module with it**; the shipyard's levels start gating hulls and the designer rather than only build rate. |

**What M0 to M3 deliberately omit, and what each omission buys:** two players rather than four (a
five-minute match, and a snapshot that fits one datagram); one fixed seed until M2 (no "is the map wrong
or is the game wrong"); inexhaustible asteroids until M3 (no husk state, no retargeting, and no asteroid
replication at all); the current build item rather than a queue (a two-byte field instead of a queue
format); no `Cruiser` design; no minimap; **two working modules rather than three**, the research station
waiting for the research that gives it a job.

**The reduced thing proves something the full one would not** — that the loop can be played twenty times
in an evening. That is the only mechanism this project has for turning the balance questions in §7 and on
the register into answers rather than opinions, and one long four-player match cannot supply it.

After the MVP, in the order they are most likely worth doing: the designer screen, research, fog of war
(the interest set is already per client since [`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md),
so fog is a change to what the accumulator scores rather than to the wire), formations, and a mobile
mothership hull.

**What is deliberately not designed yet:** audio, any campaign or narrative, art direction beyond
[`Interface.md`](Interface.md) and the meshes of `TechnicalDesign.md` §7, mods, replays and saved
matches. Each is worth doing against something running, and §10 says when that
is.
