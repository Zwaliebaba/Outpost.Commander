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
behaviour they have, the slot is held indefinitely, and they may reconnect — which is mechanically free
because snapshots are self-contained (`TechnicalDesign.md` §4), so there is nothing to catch up on. The
cost is that an abandoned fleet sits on the board as free kills, and that is accepted rather than solved:
an AI taking the slot is the better answer and it waits for M4, when there is an AI that can start from
arbitrary mid-match state. Suspend and resume are the same path (`Interface.md` §7).

There is no pause and no save in the MVP. A host with no clients keeps simulating.

---

## 3. The area

The area is generated from the match seed, deterministically, by code in `GameCore` that both the host
and the client run (`TechnicalDesign.md` §3). The client is not *told* the map; it derives it, which
takes a large static payload off the wire and means the two sides cannot disagree about where an
asteroid is.

**It is four-fold rotationally symmetric about the centre.** One quadrant is generated and copied three
times at 90°, 180° and 270°. This is the cheapest possible fairness guarantee: every player's start is
the same start, so no balance analysis is needed and no seed can be unlucky. It is also visibly
artificial, and that is the trade — a procedural generator that is *fair* without being symmetric is a
research project, and this is an MVP.

What a seed produces:

| | |
|---|---|
| **The square** | 16,384 world units on a side, centred on the origin. A world unit is nominally a metre. A battleship crosses it in about five and a half minutes, a fighter in two. |
| **Four start anchors** | One per quadrant, at a fixed radius from the centre. A station spawns on each. |
| **A home field** | A small asteroid cluster within about 1,500 units of each anchor. Enough to open on, not enough to win on. |
| **Contested fields** | Richer clusters toward the centre, reachable by everyone. This is the map's only real proposition. |

Nebulae, wrecks, hazards and anything that affects sensors are not in the MVP. The generator's interface
is a seed in and a list of placed objects out, so adding a kind later does not change its shape.

---

## 4. The economy

One resource, **credits**. There is no second currency and no upkeep in the MVP.

A **miner** flies to an asteroid, extracts until its cargo is full, flies back to the station and unloads.
That is the entire loop, and it is a loop rather than a number because the miner is a ship on the map
that can be shot. An idle miner with a full hold and a dead station is the economy failing in a way a
player can see and do something about.

**Asteroids are finite.** An exhausted asteroid stays on the map as a husk and a miner with no order
retargets the nearest one with ore left. Finite asteroids are what make the contested fields worth
contesting; an infinite home field turns the map into scenery and the match into pure military
arithmetic. The cost is that a player who ignores their miners eventually finds them idle, which is a
management burden the MVP accepts rather than solves.

Starting values: a station begins with 1,000 credits. A miner carries 100 credits of ore, extracts at 20
per second and so fills in five seconds; a round trip to the home field is roughly thirty seconds, which
puts one miner at about 2.5 credits per second. A home field holds enough for a long opening and not for
a match.

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

**A station is a hull with slots, like everything else**, and it carries two `PointDefence` mounts. It
has no drive, which is the only thing that distinguishes it from a ship — §6's model allows a hull without
one, and giving the station hull a drive later is how a mothership arrives.

**The point defence kills a loiterer, not a fleet.** It is short-ranged and weighted against small and
medium hulls (§7), so a lone fighter that parks near your station dies and a battleship group barely
notices it. That is the whole intent: the area around the station is a **safe zone**, which gives miners
somewhere to retreat to and turns an early raid into a tactical exchange rather than a free kill. An
undefended station makes a raid a threat, which reads well on paper; in practice it means finding out your
economy is dead rather than seeing it happen.

**The safe zone stops well short of the home field, and that is the point.** At 400 units against a field
about 1,500 out, **miners at the rocks are raidable** — so an early fighter has a job, and a defender has
a real choice between escorting and expanding. Retreating to the station is not free: it costs the cargo
run. Extending the range to cover the field would make the opening simply safe and the early game pure
build-up, with nothing happening until someone reaches the middle.

There are no other structures in the MVP. No turrets, no outposts, no research facility — the station's
defence is a component in a slot, not a building you place.

---

## 6. Ships are designs

**This is the Warzone 2100 spine, and it is built now even though its interface is not.**

A ship is a **hull**, a **drive**, and a component in each of the hull's **slots**. Nothing in the
simulation knows what a "fighter" is; it knows a design, and a design is a composition. Every stat a
ship has is *derived* from its components by one pure integer function in `GameCore`, which both the host
and the client call and which has a test suite over it — it is the single place a rule about what a ship
can do is allowed to live (`AGENTS.md` R19).

### The catalog

Four hulls, two drives, four slot components. That is ten components, and it is deliberately just enough
to prove the model rather than to make interesting choices. **The station is one of the hulls**, which is
what makes §5 possible without a second kind of thing in the simulation.

| Hull | Slots | Hull HP | Mass | Size class |
|---|---|---|---|---|
| `Scout` | 1 | 200 | low | Small |
| `Frigate` | 2 | 600 | medium | Medium |
| `Cruiser` | 4 | 3,000 | high | Large |
| `Station` | 2 | 12,000 | — | Large |

| Drive | Character |
|---|---|
| `IonDrive` | Balanced thrust, cheap. |
| `BurnDrive` | More thrust for more mass and more cost. |

| Slot component | What it does | Range |
|---|---|---|
| `MiningLaser` | Extracts ore. Does no damage. | 200 |
| `MassDriver` | 25 damage per second per mount. | 600 |
| `PlasmaCannon` | 120 damage every two seconds per mount. | 1,400 |
| `PointDefence` | 60 damage per second per mount. Station slots only. | 400 |

**A drive is optional.** A hull with none does not move, which is what a station is. **Speed is
thrust divided by mass**, and mass is the hull plus everything in it — so a Cruiser with four
plasma cannons is slower than an empty one, and that falls out of the arithmetic rather than being
written down anywhere. Turn rate derives the same way.

### The three designs the MVP ships

The player's three buildable ships are not special cases in the code. They are rows in a table:

| Design | Hull | Drive | Slots | Cost | Speed |
|---|---|---|---|---|---|
| **Miner** | `Scout` | `IonDrive` | 1× `MiningLaser` | 150 | 100 u/s |
| **Fighter** | `Frigate` | `BurnDrive` | 2× `MassDriver` | 300 | 140 u/s |
| **Battleship** | `Cruiser` | `IonDrive` | 4× `PlasmaCannon` | 2,400 | 50 u/s |

**Note what the miner is:** it is not a ship type. It is a `Scout` with a mining tool where a weapon
would go. That the user's three ships fall straight out of the component model without a single special
case is the evidence that the model is worth building before its interface exists.

---

## 7. Combat

A ship with a weapon and no order engages the nearest hostile in range on its own. A ship with an attack
order pursues its target. There is no formation system in the MVP beyond ships holding a loose spacing
so they do not occupy the same point.

**Damage is `base × modifier[weaponClass][targetSizeClass] / 100`**, integer throughout. The modifier
table is the whole of the rock-paper-scissors, and it is six numbers:

| | Small | Medium | Large |
|---|---|---|---|
| `MassDriver` | 100 | 60 | 25 |
| `PlasmaCannon` | 20 | 60 | 100 |
| `PointDefence` | 120 | 90 | 30 |

Mass drivers hurt small fast things and scratch capitals; plasma is the reverse, and point defence is the
mass driver taken further — it shreds anything small that loiters and is irrelevant to a capital. The
counter to a battleship is not a better weapon, it is that a battleship moves at 50 units per second and a
fighter moves at 140 — fighters pick the fight, kill miners and leave.

**Whether that reads as a counter in a real match is the first thing M3 is for**, and no mechanism is being
added in advance to guarantee it. If it turns out false, the damage table is the cheapest lever and the
price list is the next; tracking-and-evasion rolls and a minimum range on capital weapons are the two
structural answers, and both were declined for now because this is a question you play rather than argue.

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

---

## 9. Research, and what the MVP must not foreclose

**There is no research in the MVP**, and there is no designer screen. Both are the next thing after it,
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
| **M0** | **The wire** | The host opens a UDP socket and simulates one moving entity. The client connects over `DatagramSocket`, receives snapshots, interpolates, and draws one shape in Direct3D 12. A tap sends a move order and the shape goes there. No game at all — this proves the tick, the packet format, the two socket APIs talking to each other, the D3D12 frame, the gesture seam, and — on an actual Surface Pro — whether the loopback exemption makes a single-machine loop usable at all. **Everything after this is content.** |
| **M1** | **The fleet** | Four stations, the three designs, the build queue, move orders, selection. Multiple clients on one host. |
| **M2** | **The field** | The procedural generator, asteroid fields, miners, the credit loop. |
| **M3** | **The fight** | Weapons, the damage table, destruction, elimination and victory. A stub AI that builds and attacks, so a match can be played by one person. |
| **M4** | **The opponent** | The AI of §8, match configuration, one to three AI slots. |

After the MVP, in the order they are most likely worth doing: the designer screen, research, fog of war
and the interest set, delta replication, formations, and a mobile mothership hull.

**What is deliberately not designed yet:** audio, any campaign or narrative, art direction beyond
[`Interface.md`](Interface.md) and the generated meshes of `TechnicalDesign.md` §7, mods or content
files, replays and saved matches. Each is worth doing against something running, and §10 says when that
is.
