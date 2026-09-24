# Technical Design — *Outpost Commander*

How [`GameDesign.md`](GameDesign.md) is built inside the rules of [`AGENTS.md`](../AGENTS.md). That file is
read first and is cited by rule number here rather than restated.

**Status: DRAFT.** The figures marked *arithmetic* are exactly that — quantities derived from the design's
numbers, so the shape can be argued about before anything is written. `AGENTS.md` §6 requires a figure to
be measured before it is quoted as fact; §9 lists the ones that must be, and says which have been measured
since there was code to measure.

---

## 1. Where everything goes

The six libraries and their edges are `AGENTS.md` §2 and are not restated. This is the mapping of the
design onto them:

| Concern | Library | Why there |
|---|---|---|
| Fixed point, binary angles and the sine table, the PRNG, integer square root | `NeuronCore` | Engine, and both sides need it. |
| Packet header | `NeuronCore` | Transport framing knows nothing about the game (R9), and the two sides must agree on it byte for byte. |
| The UDP endpoint over `DatagramSocket` | `NeuronClient` | C++/WinRT, Windows Store family. |
| Direct3D 12 device, swap chain, scene target, the scaled present | `NeuronClient` | R12, R13. |
| The DirectWrite glyph atlas and the text quad renderer | `NeuronClient` | Engine: a glyph cache knows nothing about the game (R9). |
| The `CoreWindow` seam: `GestureRecognizer` in, an input queue out | `NeuronClient` | R18, R21. The arithmetic under a gesture is a pure function with a suite over it. |
| The UDP endpoint over Winsock2 | `NeuronServer` | Desktop family. |
| Entities, the component catalog, derived stats, the damage table, the generator, every wire record | `GameCore` | Game vocabulary both sides must share. The client previews against these rules; the host validates with them (R19). |
| The simulation, the AI, the match | `GameLogic` | Host only. The client does not link it. |
| Destination slot assignment around an order point | `GameLogic` | It reaches an outcome, so it is simulation and obeys R16's ordering rule. |
| Module placement validity — radius, clearance of the station and of other modules | `GameCore` | A rule both sides evaluate: the client previews it under the finger, the host validates it (R19). |
| Derived cargo capacity and extraction rate, summed over a hull's slots | `GameCore` | The same pure function as every other derived stat (R24). |
| The nearest thing that accepts ore | `GameLogic` | It reaches an outcome, so it is simulation: candidates ordered by entity identity (R16). |
| The uniform grid, the mining loop and the economy | `GameLogic` | Simulation (M2.5 to M2.7): 512-unit cells rebuilt each tick in index order, a standing mine order, and credits from what is unloaded, remainders carried per player. |
| Which effect a module has, and the player's multipliers | `GameCore`, `GameLogic` | The effect is a catalog field (`ModuleEffect`, R24); applying it is simulation (M2.12). The best module of a kind counts. |
| The derived field, the baked asteroid meshes, module placement and its radius | `GameClient` | The client's half of R23 and of the placement rule: it derives, draws and previews, and sends an order the host validates. |
| Command validation — ownership, bounds, generation, sequence | `GameLogic` | The host is the only thing that may decide an order is legal (R19). |
| Replica state, interpolation, the camera, selection, the HUD | `GameClient` | Client only. |
| `IFrameworkView` and application lifecycle | `OutpostCommander` | Windows Runtime glue and nothing else (R20). |
| `main`, host configuration, the tick loop's outer shell | `Server` | Same rule, other side. |

**Two naming traps this design walks into, named here so nobody walks into them.** `AGENTS.md` §2 forbids a
header spelled like an SDK or CRT header, because the other projects' directories sit ahead of the SDK on
the include path and MSVC matches them case-insensitively for an angled include too. So the fixed-point
header is `FixedPoint.h` and never `Math.h`; the two-dimensional vector is `Vec2.h` and never `Vector.h`,
which would be found by `<vector>`. And a ship's *size class* enumerator must not be spelled `Small` or
`Large` — `<windows.h>` defines `small`, and `IN`, `OUT` and `DELETE` are waiting for anyone who names an
order type carelessly.

---

## 2. The simulation

**Twenty ticks a second, fifty milliseconds a tick.** The tick is the clock and wall time reaches it at
exactly one seam (R16). Twenty is chosen over ten because a fighter at 140 units per second moves seven
units a tick, and over sixty because nothing in this design needs sixty and the replication budget is
already the binding constraint.

**There are no floats anywhere in the simulation**, which R16 requires. This document previously repeated
R16's old argument — that `/arch:AVX2` lets MSVC contract `a*b+c` into an FMA under `/fp:precise` — and
**that was wrong**: Microsoft's `/fp` documentation states contractions are not generated by default under
`/fp:precise` from Visual Studio 2022 onward, and this tree pins `v145`. R16 now carries the corrected
argument and it is not repeated here. What matters downstream is its shape: **CRT transcendentals are not
correctly-rounded and are not specified identical across architectures**, ARM64 always has FMA where x64
has it only under `/arch:AVX2`, and `/fp:precise` permits intermediate computation at machine precision
between its four rounding points. Integers are reached by none of them.

**CI builds `Debug|x64` only** (`AGENTS.md` §6), so nothing automated catches a divergence of any kind,
and ARM64 is the target platform.

### The numbers

| | |
|---|---|
| **Position** | `std::int32_t`, 8 fractional bits — one unit is 1/256 of a world unit, about four millimetres. The 16,384-unit square spans ±2,097,152, which leaves three orders of magnitude of headroom in an `int32`. |
| **Velocity** | The same format, per tick. |
| **Angle** | `std::uint16_t` binary angle: 65,536 is a full turn, and addition wraps for free, which is the whole reason for the format. |
| **Sine** | A 4,096-entry table of `std::int16_t` in Q1.15, indexed by `angle >> 4`. Eight kilobytes, no interpolation, exact on every platform. Angular resolution 0.088°, which is finer than anything a ship does. |
| **Multiply** | `(std::int64_t(a) * b) >> 8`. The intermediate is 64-bit and that is not optional. |
| **Distance** | Compared as squared distance in `std::int64_t`. Integer square root exists for the rare case that needs a magnitude, and the renderer — which is not the simulation — uses floats freely. |

**Randomness is one PRNG, pinned, seeded from the match**, and it is the only source. Never
`std::random_device`, never a hash of an address, never the tick count used as a seed somewhere else.
A 64-bit PCG or xoshiro written out in `NeuronCore` is a dozen lines and has a test suite over its first
thousand outputs; the standard library's engines are specified well enough but its *distributions* are
not portable, so nothing here uses one.

### Order is a correctness property

R16 forbids iteration over an unordered container whose order reaches the outcome, and in a simulation
with a spatial index that rule has teeth. **Entities live in a `std::vector` with a free list**, identified
by index and generation, and the tick iterates it in index order. The spatial index — a uniform grid,
512-unit cells, 32 × 32 over the square — is a *candidate* structure only: **every query that reaches an
outcome sorts its candidates by entity identity before using them.** A target chosen by "nearest" with two
candidates at equal distance must break the tie on identity, not on which cell was visited first.

### The tick

Drain incoming commands, then: orders, AI, movement, weapons, mining, build queues, deaths, victory.
**Mining is a standing order and therefore the one system that re-issues work to itself** — a miner that
unloads is given its next destination inside the same pass, which keeps the cycle on the tick and out of
the command path. One pass, fixed order, no system reading another's half-updated output. At the end of
the tick the host computes a **state hash** over every entity's identity, position, heading, hull, hull
points, owner and mine order, and a **match hash** that adds each player's credits, income owed and item
building — widened on 2026-09-24 after the mid-implementation review (M6) found the hash blind to everything
M3's damage and the economy write (ADR-002). The match hash is what the determinism test asserts and what a
desynchronisation report would carry.

---

## 3. The world, and generating it

The area is `GameCore` code taking a seed and producing a list of placed objects. It runs on the host to
populate the match, and **it runs on the client to draw the same asteroids** — the client derives the map
rather than being sent it, which keeps a large static payload off the wire and makes it impossible for the
two sides to disagree about where a rock is.

This is not the client simulating (R19). A generator is a rule, it lives in `GameCore`, and `GameCore` is
precisely the shared rules both sides evaluate. The simulation still owns how much ore is *left* in an
asteroid, and since Q69 that stays on the host.

The generated region is copied by rotation to the player count (`GameDesign.md` §3): a quadrant at 90°,
180° and 270° for four players, a half at 180° for the MVP's two. **Both rotations are exact because
positions are integers** — a 90° rotation is a swap and a negation, a 180° rotation is a negation —
where a floating-point rotation would make the starts subtly unequal, which is the kind of unfairness
nobody would find for a year.

**M0 and M1 passed one fixed seed**, which kept "is the map wrong or is the game wrong" out of the two
milestones that could least afford the question. **Since M2 the host takes the match's seed and player
count**, and the join reply carries both, so the client derives the same field (ADR-013 as amended, R23).

---

## 4. Replication

R19 settles the architecture before anything else does: **the client links no simulation, so lockstep is
impossible.** The host simulates and sends state; the client renders what it was sent and sends commands.
There is nothing to re-litigate here.

### Downstream: prioritized records, one datagram at a time

**The unit of replication is the entity record, and a datagram is a bag of them at one tick.** A record is
a self-contained fact — this entity, at this tick, was here, facing this way, at this hull, of this design,
owned by this player — and nothing in it depends on any earlier record. So any subset of a datagram is
meaningful on its own, a lost datagram delays some entities' refresh by a tick and breaks nothing, and
there is no baseline, no acknowledgment, no ordering and no reassembly anywhere in the design. This is
[`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md), and it keeps the property
[`ADR-003`](ADR/ADR-003-the-record-and-the-command.md) chose the full snapshot for — nothing on the client
can diverge, because nothing on the client is computed — at any player count rather than at 110 entities.

An entity record is **twelve bytes**:

| Field | Bytes | Note |
|---|---|---|
| Entity identity | 3 | A 16-bit index and an 8-bit generation. Ten and six were enough for a 1,024-entity world; a hundred-player match holds thousands, and the generation must survive index reuse under loss. |
| Owner | 1 | A `PlayerId`. On the record rather than in two flag bits, because a record must say whose it is without the datagram being grouped or complete. |
| Position x, y | 4 | Two `std::int16_t`. The 16,384-unit square over 65,536 steps is **a quarter of a world unit** per step — far finer than a ship is wide. |
| Heading | 1 | 256 steps, 1.4°. A rendering quantity; the simulation's heading is 16-bit. |
| Hull remaining | 1 | Percent, rounded to nearest with **both ends exact** — and floored at 1 for anything still alive, because 0 is what a client draws as destroyed ([`ADR-003`](ADR/ADR-003-the-record-and-the-command.md)). |
| **Design identity** | 1 | **Its own byte.** Packed into the flags it had two bits — four designs, permanently — which contradicted R24 outright. |
| Flags | 1 | **State 3 bits, cargo 3 bits, two spare.** The team bits left with ADR-024; cargo took a spare bit at M2.7 (`OpenQuestions.md` Q53). |

**The datagram is an update, and its header is twenty-two bytes at any player count**: the transport's
four — version, type, and a sequence the receiver uses only to count loss (`NeuronCore/PacketHeader.h`;
the two fragment fields M0.2 reserved are gone, because nothing fragments) — then the tick every record
describes, the **live entity count**, **the recipient's own player block** (credits 4, last applied
command sequence 2, the currently building design 1 and its progress 1; since `OpenQuestions.md` Q80 the design
byte's high four bits carry how many items wait behind it, saturating at 15, and protocol 6 says so) and nothing about anyone else's,
and a count each of records, removals, fire events and — since `OpenQuestions.md` Q83 — the spent-rock mask. The
mask is one bit a rock in `GenerateField`'s order, eleven bytes at most, and rides **every** update once any rock
is spent, so each stays self-contained; protocol 8 says so. What a client draws about a rival is on the rival's
entities: a station's hull is on the station's record.

**The host fills one update per client per tick from a priority accumulator.** Each client has an integer
score per live entity; every tick it grows by the entity's relevance to that client, and sending the
entity resets it. The host takes the highest scores that fit — **98 records** at the pinned payload with
three removals, two fire events and a full spent-rock mask riding along — and sends them. Relevance is a sum of a base of one, a
term for being inside the client's view, a term for having moved or changed since last sent to this
client, and a term for being the client's own; the weights are constants beside the accumulator in
`GameLogic`, and tuning them is `OpenQuestions.md` Q49. **The sweep is the guarantee under the scores**:
any entity unsent for ⌈live entities ÷ (65 × the cap)⌉ ticks goes into the next update ahead of every
score, so relevance decides how *often* an entity refreshes and the sweep bounds how *long* it can go
without. The client computes the sweep from the header's entity count and **forgets an entity that has
gone three sweeps without a record**, which is the only way an entity leaves a client without a removal
and is what makes a rejoin correct after the removals it missed have stopped repeating. **Sixty-five is
the floor, not the typical fill**: removals are capped at 48 an update and fire events at 40, and the
records an update holds with both sections full is the constant both sides compute the sweep from.

**A client may receive up to two updates a tick, each of them whole.** When more records are due than one
holds, the host sends a second. Each is complete and separately renderable, which is the test that
separates rate separation from fragmentation (`.claude/skills/datagram-budget/`): losing one loses those
records' refresh, not the frame. At the MVP's 110 entities the second update is what keeps every entity
refreshed every tick; at a hundred players the cap is what bounds bandwidth and the accumulator ranks
what fits.

**Ordering is a per-entity tick comparison, and there is nothing else.** The client keeps, per entity,
the tick of the newest sample it holds, and drops a record whose tick is not newer. No sequence window,
no reorder buffer, no reassembler.

**Removals and fire events are repeated facts.** A removal — the three-byte identity, generation
included — rides every update to a client for **ten consecutive ticks** after the death, and the client
ignores one it has already applied; without a removal a death would be learned by *absence*, which the
accumulator makes ambiguous from the first tick. A fire event
([`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md): shooter 3, target 3, weapon 1) rides for
**three**. Losing a death needs ten consecutive losses, once in 10¹³ updates at 5% packet loss.

**Cargo rides in the flags byte**, three bits since M2.7: zero to four lit chips, quarters rounded up, which
is what the selection panel's four chips draw ([`ADR-003`](ADR/ADR-003-the-record-and-the-command.md),
`OpenQuestions.md` Q53). Two bits could say four states and four chips need five.

**Updates go out at 20 Hz** and the client renders **75 milliseconds** behind — one update interval
plus a jitter margin. This was 10 Hz and 150 ms, and the change is about **latency**, which ADR-003
originally never computed:

| tap → visible | send wait | host tick | update wait | interpolation | **total** |
|---|---|---|---|---|---|
| 10 Hz / 150 ms — average | 25 | 25 | 50 | 150 | **252 ms** |
| **20 Hz / 75 ms — average** | 25 | 25 | 25 | 75 | **152 ms** |
| **20 Hz / 75 ms — worst** | 50 | 50 | 50 | 75 | **227 ms** |

On a touchscreen there is no cursor and no hover: **the tap is the only feedback the player gets**, so a
quarter-second of nothing is a dead interface.

### What it costs, and what a client sees

| | live entities | records a tick | per client | host egress | every entity refreshed within |
|---|---|---|---|---|---|
| **MVP — 2 players × (50 ships + 1 station + 4 modules)**, two updates a tick | 110 | 198 | 49.3 KB/s | 0.1 MB/s | **every tick** |
| Post-M2 — 4 players, same per player, two updates | 220 | 198 | 49.3 KB/s | 0.2 MB/s | 2 ticks |
| 100 players, same per player, two updates | 5,500 | 198 | 49.3 KB/s | **4.93 MB/s** (39 Mbit/s) | 43 ticks, 2.15 s |
| 100 players, one update | 5,500 | 99 | 24.6 KB/s | 2.46 MB/s (19.7 Mbit/s) | 85 ticks, 4.25 s |

**The per-client cost is 24.6 KB/s per update per tick whatever the entity count**; what the entity count
moves is the refresh interval, and the accumulator spends the refreshes on what the client is looking at.
A screen holding 200 entities refreshes each every two ticks at the cap; a tactical view of 1,000 ships
as 3.5-pixel silhouettes refreshes each every six, 300 ms, and holds between, which at that size is not
visible. **5,500 in one view is the cap's true ceiling** — each every 1.4 seconds, the nearest and the
moving far more often — and a match that wants a whole hundred-player war on one screen wants a higher
cap and the bandwidth that goes with it.

**Before changing any of this, run the budget.** `.claude/skills/datagram-budget/` computes the figures
above from the record layout rather than restating them — `--in-view` gives the refresh interval, and
`--datagrams` the cap — and applies the one test that separates a legitimate split from fragmentation
with a better name: *can the client draw a correct frame from one datagram without the other?* These
figures have disagreed with the ADRs' copies before; when one moves, move both.

**Modules are ordinary entity records** ([`ADR-015`](ADR/ADR-015-the-base-is-built-from-modules.md)):
they never move, so the accumulator refreshes them rarely, but their hull does, so they cannot be treated
as static the way asteroids are. Their cap of four was a replication decision under the full snapshot and
is a design one now.

**Resume is a sweep, not a snapshot.** On a rejoin the host resets that client's scores so everything is
sent within one sweep — one tick at the MVP, 43 at 5,500 entities — and the client clears its store so
nothing stale survives. The reconnecting overlay (`Interface.md` §7) stays up until the first update
after the rejoin lands.

**Asteroids are not replicated at all before M3**, because inexhaustible asteroids have no simulation
state (`GameDesign.md` §4) and the client derives their positions from the seed (R23). **From M3 they are
still not replicated** (`OpenQuestions.md` Q69, ruled 2026-09-24): ore remaining is host-side state, hashed
and never sent, and a spent rock is drawn as it always was. No wire byte moved.

**The accumulator is per-client host state and not simulation.** A score and a last-sent tick per entity
per client — a few megabytes at 100 × 5,500 — never hashed, never replayed, never reaching the tick's
outcome. `GameLogic`'s tick does not know that replication is per client, and M1.7's determinism hash must
be the same after ADR-024 as before it.

### Upstream: commands, made reliable by the update

A command is an order: a type, a target point or entity, and the identities of the selected ships. Commands
carry a per-player sequence number and are **repeated in every outgoing packet until acknowledged** by the
`lastCommandSeqApplied` field the update's own player block carries. The host applies in sequence order and ignores
anything at or below what it has applied. Reliable ordered delivery for the one channel that needs it, in
about thirty lines, with no general reliability layer.

**Both clients do it since 2026-09-24.** Until then only the Bot resent; the packaged client sent each
command once, so a lost datagram was a lost order and its marker stayed on screen (the mid-implementation
review, M5). `ClientFrame` now holds every command until its sequence is acknowledged and packs the
outstanding ones, oldest first, into every packet it sends, the quarter-second view report included. **A
command still unacknowledged after two seconds is given up, with its marker**, since the host acknowledges
every command it has decided on — **a refusal past the sequence check is acknowledged too** (Q24 as
amended), and a dead ship in a selection is skipped rather than refusing the order.

**A mine order names its rock by index** (M2.6, `OpenQuestions.md` Q52): `Mine` is command type 5, carrying
the rock's position in `GenerateField`'s order in `targetX`. Asteroids are not entities before M3 (Q22), and
both sides derive the same rows from the join's seed and count (R23). The host refuses an index past the
field. Ships in the selection that cannot mine are skipped, not refused. **The command's layout does not
change**: eight bytes and three per identity, like every other.

**A module is placed and upgraded by two more types** (M2.11, `OpenQuestions.md` Q55). `PlaceModule` (6) carries
its point in the target at full wire precision, **plus a ninth fixed byte for the design**, which no other type
writes. `UpgradeModule` (7) is eight bytes: the module's identity in the target, as an attack's is, and the level
in `targetY`'s spare high byte. Neither carries a selection, so neither moves the worst case:
`Scripts/DatagramBudget.py --upstream` measures a placement at 9 bytes and an upgrade at 8. The host judges the
site with `CheckModuleSite` before anything is spent.

**The widths are measured, and M0.10's encoder is where they became facts; ADR-024 moved two of them.**
The packet header is **twelve bytes** -- the transport's four (`NeuronCore/PacketHeader.h`), the player
identity, the command count, and **the client's view center (4) and view radius (2)**, which is what the
host's accumulator scores relevance against; a lost one leaves the host scoring against the last view it
saw. **A seated client sends an empty command packet four times a second** as a view report, since commands
only go out when the player taps (`GameClient/ClientFrame.h`). One command is **eight bytes and three per selected identity**. `CommandTests` pins both. `Scripts/DatagramBudget.py` modelled the header as four until that encoder was written, because it
carried only the `version` and `type` of the header as it stood before M0.2 and never gained the sequence
and fragment fields; the figures below are the corrected ones, and the retransmit window is unchanged by
the correction.

**The packet is filled oldest-first and stops when the next command will not fit.** "Repeated until
acknowledged" bounds the packet by how many commands are outstanding, and nothing about the format bounds
*that*: at the peak 110-identity selection a command is 338 bytes, so **three fit in the pinned 1,232-byte
payload and a fourth would fragment**. What reaches four is not a fast player — touch cannot issue three orders in
150 ms — it is a **stalled acknowledgment**: a host hitch or a run of lost snapshots, which is exactly the
load under which a fragmented command packet is worst. Filling oldest-first makes the bound structural
rather than a constant to tune: the packet cannot exceed the payload, no order is ever dropped, and the
sequence never gains a gap — which matters because the host applies in sequence order and ignores anything
at or below what it has applied, so a missing middle command would be discarded rather than waited for. The
cost is that the newest order waits a packet, 50 ms, when the window is that deep; it was already waiting
on the stall that made it deep.

**The host validates every command, and this is correctness rather than security.** §5 declines
authentication and any defense against a hostile client; that exclusion silently covered ownership and
bounds checks too, which are a different category. A 1,232-byte command packet holds **404 identities
against a peak of 110** — a 3.7× amplification into a single-threaded loop, reachable from an ordinary bug
or a reordered packet with no attacker anywhere. So the host: rejects entities the sender does not own,
bounds the selection at the sender's own entity count, rejects stale generations, clamps target points to
the play area, and handles `uint16` sequence wraparound explicitly rather than letting the "at or below"
comparison invert.

**A fire event rides the update** — shooter 3, target 3, weapon 1, behind a count byte, after the
removals ([`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md)) — for three consecutive ticks;
losing a tracer takes three losses in a row.

**A client learns which player it is from a join, and nothing else on the wire tells it.** The client
sends a `Join` carrying the session token it was issued last time, or zero; the host answers with the slot
it assigned, a session token to keep, and **the match seed and player count**, which R23 makes the two things a client cannot
derive for itself. The host assigns the slot -- there is no lobby to choose in (`GameDesign.md` §2) -- and a
join with no slot free is refused with a reason rather than dropped. This is [`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md), and until it
existed a client was told which player it was by a compiled-in constant.

**The host resolves the sender from its session, not from the player byte the command packet carries.**
The byte stays on the wire and the eight-byte command header above is unchanged; what changed is that a
command from an endpoint which never joined is refused and counted.

A client with nothing to say sends a heartbeat a few times a second so the host can time it out. **A
timeout forgets an ENDPOINT and never a slot** -- §2's disconnected player keeps their slot indefinitely,
and the two sentences describe different things ([`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md)).

## 5. The transport, and the two socket APIs

**The host is Winsock2.** One non-blocking UDP socket, drained at the top of each tick and written at the
end of it. One thread. At four clients and 20 Hz there is no reason for a second.

**The client is `Windows::Networking::Sockets::DatagramSocket`** through C++/WinRT, which is the Windows
SDK's projection and therefore inside R14's closed list. `Microsoft.Windows.CppWinRT` is already the tree's
one package.

**The threading seam is real and it is named here so it is not discovered later.** `DatagramSocket`
delivers `MessageReceived` **on a thread pool thread**, not on the frame's thread. The handler does exactly
one thing: copy the datagram's bytes into a mutex-guarded queue and return. The frame loop drains that
queue. The critical section is a `memcpy` and a `push_back`; nothing parses a packet on the pool thread and
nothing touches renderer or replica state there.

### Where the host is, and the trap under it

**The host address is a configuration value with a compiled-in default of `127.0.0.1`** — a one-line text
file in the package's `LocalState` folder, which is the application's own storage and needs no capability.
There is no discovery, no broadcast probe and no address entry, because R21 leaves no way to type one.
This is [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md).

The package declares **`privateNetworkClientServer`**, which is the capability for inbound and outbound
traffic on home and work networks and is what Microsoft names for LAN games. **On Windows it does not grant
internet access**, so going live additionally needs `internetClientServer`.

**`127.0.0.1` works only under a loopback exemption, and that exemption is not a shipping configuration.**
`AGENTS.md` §3 states the rule; this is why it bites here in particular. `Server` is an ordinary Win32
console executable and therefore **unpackaged**, which closes off the manifest's `LoopbackAccessRules` —
that route works only between two *packaged* applications. What is left is
`CheckNetIsolation.exe LoopbackExempt`, which Microsoft documents as **"only possible for sideload or
debugging scenarios where you have local access to the machine, and you have administrator privileges."**

**Visual Studio grants it on every F5 deploy, which is the danger**: localhost will work for the whole of
development and will not exist for anyone else. And since the development machine and the target device
are not the same machine, **the Surface Pro needs a LAN address from the first day it is used** — which is
the whole reason the address is a file rather than a constant.

**The client enters fullscreen at launch** (`OpenQuestions.md` Q23). Nothing previously forced it, which
quietly made [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md)'s exact 2× an accident of however
the window happened to be sized — a `CoreWindow` application can run windowed, and then the fit is an
arbitrary bilinear scale and the whole arrangement buys nothing. A touch-only game in a resizable window
is not a coherent object in any case.

**M0 establishes both loopback paths** (`GameDesign.md` §10), including which exemption form a UDP
client actually needs: if replies to a bound socket require the inbound form `-is`, then
`CheckNetIsolation.exe` must stay running the entire time the client is listening, and the
single-machine loop stops being worth having.

Encryption, authentication and any defense against a hostile client are not in the MVP. The protocol
version in the header refuses a mismatched build, and that is the whole of it -- **including the join**,
so a client on another version is dropped rather than told, and a mismatched build is indistinguishable
from an unreachable host ([`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md)). The session token the join issues is a **name, not a
credential**, for the same reason.

---

## 6. The client's frame

One drain of the `CoreWindow` dispatcher per frame (R18), then the packet queue, then the interpolation
clock, then render, then present.

**The client renders the past, per entity.** It draws at a time **75 milliseconds** behind the newest
update — one update interval at 20 Hz plus a jitter margin. Since
[`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md) a datagram is a bag of records rather than
the world, so the store holds **three samples per entity**, each stamped with its tick, and a record whose
tick is not newer than the sample it holds is dropped, which is the whole of the reordering logic.
Positions and headings are interpolated between the pair when it straddles the render time; headings
interpolate the short way round, which the binary angle makes a subtraction rather than a special case.
**An entity whose newest sample is older than the render time holds its last position** and never
extrapolates: at a hundred players the accumulator refreshes a distant, stationary ship rarely on
purpose, and a hold is what "rarely" looks like.

**This said "the two most recent snapshots" and that was arithmetically impossible.** Two snapshots span
one interval, 50 ms, ending at the newest; a render time 75 ms behind the newest is 25 ms *older than the
older of the two*, so the pair never contained the frame being drawn. It was wrong at 10 Hz too, where
150 ms behind the newest sits outside a 100 ms pair by the same margin, so this is not residue from the
rate change the way the 150 itself was — it never held.
[`ADR-003`](ADR/ADR-003-the-record-and-the-command.md) has always read the other way: a lost snapshot
being "a 50-millisecond gap inside a 75-millisecond buffer, covered without extrapolating" describes a
buffer with more than one interval of history in it. **That paragraph described a snapshot ring that no longer exists**: under ADR-024 the
store is three samples per entity -- the depth `GameClient/ReplicaStore.h` computes from the delay and the
interval, as it once did for snapshots -- and the pair is whichever two of that entity's samples straddle the
render time. The hold above is what a missing sample does.

**The frame is three passes into two surfaces, and the last one is a recorded departure from R13.**

**The world is two of them and they share the scene target.** The **ship pass** draws each authored hull
once, instanced, with the position, heading and owner's colour per instance -- so one call covers every
ship of a shape regardless of owner (M1.9). The **world pass** draws a generated arrow for anything with
no authored mesh: the `Cruiser` today, and every hull on an install where the package did not carry the
files, which is [`ADR-021`](ADR/ADR-021-content-ships-with-the-package.md)'s named failure made visible
rather than silent.

**The world** draws into a scene target and is fitted into the back buffer with the aspect preserved. Its
resolution is **a scale of the panel, defaulting to 1:1 — 2880 × 1920**
([`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)); the swap chain is created at those physical
pixels, so at the default the fit is 1:1 and unfiltered, and at the other settled point, a 0.5 scale, it is
an exact 2× on the point-sampled path.

**The interface** then draws straight into the back buffer at physical resolution
([`ADR-011`](ADR/ADR-011-the-interface-draws-after-the-scale.md)). It is laid out in authored coordinates —
1440 × 960, a statement about fingertips — unconditionally, and each position is carried through **the
interface's own fit transform**, which maps that authored space into the back buffer. Glyphs are rasterised
at the physical size it produces rather than doubled from 24 authored pixels.

**Two transforms, one place that asks the window how big it is.** The world fit and the interface fit are
separate values from the same computation. They were one value until ADR-016, and reusing the world's fit
made the interface's scale a function of the world's resolution — at 1:1 it becomes identity, which renders
the entire interface at half size in one corner. No pass branches on the window size, exactly one place
asks, and R13's intent holds while its letter does not.

**What this buys is that the world's resolution binds nothing but the world.** Changing it is a decision
about fill rate, not a reauthoring of every panel — which was the compounding cost that made ADR-007 the
most expensive decision in this design, and is now the cost it no longer has.

**The scale is what makes multisampling affordable, and it is a trade rather than a free lunch.** At 1:1
the scene target is 5.53 megapixels; at 0.5 it is 1.38. The sample count is one constant, the MVP ships
one sample, and **1,440 × 960 at four samples and 2,880 × 1,920 at one are the same 5,529,600 samples** —
so the choice is between resolution and edge quality at a fixed footprint. Space is thin bright silhouettes
against black, which wants both. **Neither the back buffer nor the interface pass can be multisampled**,
since DXGI's flip model requires `SampleDesc.Count` of 1; for rectangles and text quads that costs nothing,
and for the world it is most of why the scene target exists — at 1:1 with one sample the present step is a
pure copy that buys only the ability to turn multisampling on as a constant.

**The sky is one more draw, and it is stars and nothing else**
([`ADR-019`](ADR/ADR-019-the-sky-is-generated-from-the-seed.md)): 8,000 instanced quads in a single draw,
generated from the match seed with no vertex buffer, over a black clear. A baked galaxy band behind them
was built and withdrawn — it read as a painting — and the Milky Way is carried by star density rising
toward the galactic plane. **It draws last, with depth test on**, so it shades no pixel the fleet already
covers and pays no multisample resolve on a surface with no edges. Evaluating the sky analytically per
pixel instead would cost an estimated 2–5 ms a frame at 5.53 megapixels, which is the figure that would
have taken [`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)'s 1:1 default away.

**Nothing about the sky reaches the host, and nothing about it changes.** It is seeded from the match seed
the client already holds for R23's generator, so it costs no wire bytes and cannot desynchronise anything;
it takes no time input, so it is generated once and never updated. Being floats throughout, it
is also what `Scripts/CheckDeterminism.py` would catch the day somebody moved it into `GameCore`.

Drawing 204 ships is **one instanced draw per hull**, with a per-instance buffer of a transform and a team
color. Seven meshes are drawn this way: `Scout`, `Frigate`, the station and, since M2.10b, **one per module
level**, so a shipyard reads apart from an ore processor. `ModuleFrame.cmo` ships but no design draws it.
Then there is **one draw per asteroid variant, five for the field**, rather than one, which is what
[`ADR-005`](ADR/ADR-005-a-mesh-is-a-cmo-file.md) cost when it made a rock a file instead of a function.
**The field is baked, not instanced** (M2.4): a rock turns on three axes, scales and leaves the plane,
which the ship pass's instance cannot say, and a field never moves. So each variant's rocks are placed on
the processor into one static mesh when the join names the field. The ship pass draws each with one
identity instance, at 552 KiB of upload heap at two players and 1,104 KiB at four, where instancing would
have cost a second shader. The `Cruiser` is a sixth hull the
catalog carries and the MVP never draws (`GameDesign.md` §10), and under ADR-005 it costs a file rather
than a parameter. Two frames in
flight with a fence per frame. None of this is near any limit, and the renderer should not be optimized
until something measured says to.

**Text is DirectWrite rasterised into an atlas we own**
([`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md)),
built at startup and drawn as instanced quads in the interface pass. **The atlas is sized against the
window**, so a resize invalidates it, as does device removal; both are rebuild paths that must not stall a
frame visibly. **No Direct2D and no `ID3D11On12Device`**,
both of which R12 bans by name, which closes the route every D3D12 text sample takes. Coverage is
rasterised as ClearType and the three subpixel values averaged into one channel, because subpixel output
would arrive as color fringing after the 2× scale. The font family is pinned and a missing family fails
at startup rather than substituting, since a substituted font has different advance widths and R13 requires
every layout number to be unconditional.

**The client draws its own order markers, and that is presentation rather than prediction.** A tap is not
visible for 152 milliseconds at best (§4), and on a touchscreen there is no cursor to acknowledge it. So
the client draws the order it just *sent* — a destination marker and a line from the selection — the
instant the gesture resolves, and clears it when `lastCommandSeqApplied` passes that command's sequence.
**Nothing is predicted and no rule is bent**: R19 forbids the client simulating, not the client drawing
what it asked for. The ships themselves do not move until the host says they did.

**Suspend and resume cost one sweep.** A packaged application is suspended when it loses the
foreground and the match runs on; on resume the client reconnects, clears its store, and the host resets
its scores so everything is sent within one sweep — one tick at the MVP — with a reconnecting overlay in
between (`Interface.md` §7). There is no resynchronization path to write beyond that, which is
[`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md) keeping what ADR-003 bought. The same is true of a player who disconnects outright (`GameDesign.md` §2).

---

## 7. Content, and where the line around it actually is

**Content files ship** ([`ADR-021`](ADR/ADR-021-content-ships-with-the-package.md)). Meshes, textures,
fonts and audio may be authored as files and carried in the package; `OutpostCommander` declares them as
package content and `Server` reads from its own directory.

**The line is against dependencies, not against files.** R14 closes the dependency list, and a mesh format
is where a project reaches past it without noticing. There is no glTF loader here, no FBX, no DirectXTK12
and no DirectXTex, and there will not be: a format used here has a reader written here, or is one the
Windows SDK already reads — WIC for images, Media Foundation for audio, both inside R14 already.

**Nothing blocks the frame thread on an asynchronous operation**, because the frame thread is an ASTA
and blocking it on one is a deadlock rather than a delay
([`ADR-012`](ADR/ADR-012-a-shader-is-compiled-into-a-header.md) records it as observed).

**THIS SAID `Package.Current.InstalledLocation` WAS ASYNCHRONOUS AND IT IS NOT** -- `InstalledLocation()`
and its `Path()` are **properties**, safe anywhere; `GetFileAsync` is the asynchronous one. The
distinction is the whole of how content gets read at all, and it is the same one
`Neuron::ReadHostAddress` learned the expensive way at M0.22: the tidy-looking `GetFileAsync(...).get()`
killed the client between opening its log and writing the first line into it. `NeuronClient/PackageFile.h`
takes the path from the property and reads the file with an `ifstream`.

**It is still blocking file I/O, and it runs once before the frame loop.** Four hundred kilobytes at
launch is not a frame's worth of work to hide; the moment something has to load DURING a match it needs a
worker and a handoff.

**A mesh is a CMO file** ([`ADR-005`](ADR/ADR-005-a-mesh-is-a-cmo-file.md)). The MVP's thirteen files —
`Scout`, `Frigate`, the station, the bare `ModuleFrame`, one per module level and five asteroid variants —
are modeled and shipped as package content rather than emitted by code, and `Design/design_handoff_meshes/` specifies them. **That record
used to rule the opposite**, and it was replaced rather than superseded because nothing has shipped.

**The reader is written here, which is the whole of how CMO stays inside R14.** It exists:
`NeuronClient/CmoReader.h` (M1.9), and `Tests/NeuronClientTests/CmoReaderTests.cpp` feeds it a file
carrying skinning, bones and animation clips to prove it walks past them. CMO's only reader in the wild is
DirectXTK12's, which the paragraph above closes by name. The MVP uses the position, the normal and
the vertex color of CMO's fixed vertex and writes tangent and texture coordinate as zeros — 24 dead bytes
of 52 — and the reader **skips the materials' eight texture slots, the skinning buffer, the bones and the
animation clips rather than rejecting a file that has them.**

**Normals are baked per face onto split vertices**, which is flat shading and is what a low-polygon
faceted look wants: no smoothing group to decide and no tangent basis to get wrong. **Team color is the
vertex color channel** selecting between a hull palette and the owner's color, so one instanced draw
covers every ship of a shape regardless of owner — the one property of the old decision that constrains
the authoring, and the reason a shape may not spend a second material on its team.

**A mesh may be authored off its own centre, and what is asserted is the extent rather than the
centroid** (Q43). The mesh origin is the simulated position; nothing requires the *volume* to be balanced
around it, and three of the thirteen deliberately are not — `ModuleOreProcessorL2` is asymmetric in X, the
shipyard truss heavily so in Z, and the station's hub puts its centroid at **Y ≈ +6**. A sanity check that
expects `min == -max` looks like the obviously missing one and fails all three;
[`Scripts/CheckMeshes.py`](../Scripts/CheckMeshes.py) compares `min` and `max` per axis against the
manifest instead, which catches a mesh that moved without caring where its middle is.

**A weapon fires from the hull's origin in the MVP** (Q41). The mining laser and the two `MassDriver`
mounts have positions implied by hull geometry and **authored nowhere**, and they are not inferred from the
vertices — the arrays are face-split with no groups to find a recess by, and a re-modeled hull would move
every muzzle silently. The origin is wrong by up to half a hull length, about 45 units on a `Frigate`, for
one frame of a thin bright line while something is exploding. **What makes it safe to defer is that
[`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md)'s event names entities and not positions**, so
nothing on the wire or in the simulation depends on the answer; authored attachment transforms are a
change to what the handoff carries, requested when a tracer is actually drawn.

**Three sizes, and they are not one number** (Q44). CMO's vertex is fixed at 52 bytes and this content uses
28 of them — position, normal and vertex color — with the tangent and texture coordinate written as zeros.
**All three are now measured** (M1.9, 2026-09-22):

| | | |
|---|---|---|
| **On disk** | **430 KiB** over thirteen files | the figure to quote for nothing |
| **In the appx** | **52 KiB** | 188 KiB of literal zeros deflates to almost nothing, and this is what package size means |
| **In VRAM** | **266 KiB** for all thirteen, **85 KiB** for the three M1.9 ships | the only one of the three that bears on frame time |

**THE VRAM FIGURE IS NOT WHAT THIS SECTION PREDICTED, AND THE REASON IS THAT Q44's PREMISE MOVED.** That
row declined repacking the vertex — it "costs a load-time transform and a second vertex layout" — and
said VRAM would be the full 423 KiB with the dead 24 bytes fetched on every draw. **The handedness
conversion forces a load-time rebuild anyway**: the handoff authors Y-up and the camera is Z-up, so
`GameClient/HullMesh.cpp` builds a new vertex array on load whatever it does with the width. Once that
array is being built, dropping the tangent and the texture coordinate is free, and the client uploads a
**32-byte vertex** — position, normal and the two shade channels. The dead bytes never reach the GPU.

**So the decision was not overturned; the thing it was weighing disappeared.** What Q44 refused to pay for
is a cost the conversion had already paid.

**The instance buffers are 18 KiB** — 220 instances of 28 bytes across three frames in flight — which is
the whole of what the per-frame path allocates.

**What is given up is that a geometry change was a change the compiler checked.** The replacement is a
script asserting each mesh's bounds against the size the catalog states, which is a gate rather than a
compile error — the arrangement `Scripts/CheckHudGeometry.py` already has for the interface pass. And the
asteroid loses its generator: a file is one rock, so variation is a set of authored variants with the
match seed choosing among them and jittering yaw, pitch, roll, scale and height off the plane in the
client. **Five variants, 102 KiB on disk and 16 KiB deflated** (M2.4, measured in ADR-005). The scale is
clamped so no two rocks touch.

**Presentation may be data; rules stay code.** The component catalog, the designs and the damage table are
`constexpr` tables in `GameCore` and do not become files. A table the host and the client can disagree
about is a desync with a file format in front of it, which is what R16 and R23 exist to prevent — and it
is the one part of this section that is a constraint rather than a description. Moving that line is a
decision of its own and ADR-021 explicitly does not take it.

---

## 8. What each suite owns

`AGENTS.md` §3 is blunt that vstest reports an empty suite as a pass, so each of the six has a stated job
and the placeholder goes the day the first real test lands.

| Suite | Owns |
|---|---|
| `NeuronCoreTests` | Fixed-point multiply and divide at the edges of `int32`, the sine table against a reference, integer square root, the PRNG's first thousand outputs pinned, and the packet header's round trip including a refused version. |
| `NeuronClientTests` | The blackbody temperature-to-chromaticity table — eight stops, interpolated, **pinned exactly**, because it is the number that decides whether the sky reads as a sky or as confetti ([`ADR-019`](ADR/ADR-019-the-sky-is-generated-from-the-seed.md)); that the magnitude tiers come out in the 1 : 3 : 9 : 27 : 81 : 243 ratio for a given seed, and that the same seed gives the same sky twice. The device-independent-pixel to physical-pixel conversion (R18), the present-scaling fit at 1:1, at integer multiples and at neither, and the gesture arithmetic — **the sign of a pinch and of a rotation**, which R21 points out a package can hide and a test cannot. Plus atlas packing, that a glyph's advance width survives the round trip, and **the interface's own authored-to-physical transform**, which is a second value from the same computation rather than the present step's ([`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)) — pinned at both world scales, because it must not move when the world's does. Plus the gesture constants `Interface.md` §1 derives: the 16-pixel tap slop either side of its threshold, the rotation deadzone's **latch**, the 2% scale deadzone, and that a contact wider than 78 authored pixels never becomes an input record. |
| `NeuronServerTests` | The Winsock2 endpoint against a loopback peer: send, receive, a short read, a datagram larger than the buffer. |
| `GameCoreTests` | Derived design stats for every catalog combination **including `Cruiser`, which no MVP design uses**, and every module level; **module placement validity** — inside the radius, outside it, overlapping the station, overlapping another module, and the fifth module against a cap of four; the damage table; the generator's output pinned for a seed **with its symmetry asserted at both two and four players**; and every wire record encoded and decoded round trip, including removals, a fire event and **a full update, whose measured size is ADR-024's figure**. |
| `GameClientTests` | Interpolation between an entity's two samples including the wrap-around case, **that a record older than the held sample is dropped and a newer one replaces the older of the pair, and that an entity three sweeps silent is forgotten** ([`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md)), the camera's transform, hit-testing a tap against the plane at several camera angles; **the anchor solve** ([`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)) — the property is *project the anchor and it lands on the centroid*, across the pitch range, for one contact and for two, with the scale and rotation applied, and with **no drift over a long synthetic gesture**, which is the failure this model actually has; that the clamp stops the focus and lets the anchor slip rather than fighting it; and that `pitch(distance)` **saturates** at the floor instead of ending the zoom range; **the double-tap selection circle** — which ships a 192-pixel screen-space radius takes at several zoom levels, including a ship exactly on the edge and **the raking-camera case where the circle's world footprint is a wedge** ([`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md)), now bounded by `Interface.md` §5's pitch floor; **that the first tap selects one ship and only a second tap resolving to the same entity expands it** ([`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md)), including two taps on *different* ships staying two single taps; the 24-pixel pick radius and its tier order against overlapping candidates; the order marker's lifetime against an acknowledgment; and **the alert** ([`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md)) — that a fire event naming one of your entities raises one and a fire event naming somebody else's does not, that an event already on screen raises none, that several hits in one place cluster to one indicator with the right count, and that its bearing is right at several camera headings including behind the camera. |
| `GameLogicTests` | The simulation: movement toward a point, the mining loop, combat resolution, elimination and victory; **the shipyard's build-rate multiplier and the ore processor's cargo multiplier, both as integer percentages, and that elimination removes a player's modules with their ships**; **ring slot assignment** — that the same selection ordered to the same point yields the same slots in the same order; **command validation** — a foreign entity, an over-long selection, a stale generation, a wrapped sequence, an out-of-map target; **the accumulator** ([`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md)) — that an update never exceeds the payload, that no entity goes a sweep unsent whatever the scores, that a removal rides ten consecutive updates, that the sent set is the same twice from the same state and view, and that a client's scores reset on rejoin; and **the determinism test**, which runs a fixed tick count from a seed against a scripted order list and asserts the state hash. That last one is what protects R16, and it is the most valuable test in the tree. |

---

## 9. What must be measured, and is not yet

`AGENTS.md` §6 requires a figure to be measured before it is quoted, and everything numeric above that is
not a definition is arithmetic on the design's own starting values. These are owed:

1. ~~**The snapshot's real size** at 110 entities and at 220~~ — **DISCHARGED at M0.9.** The encoder
   produces **1,137 bytes** at 110 and **2,253** at 220; the MVP's really is one datagram, with **95 bytes**
   of headroom, nine entity records. `Tests/GameCoreTests/SnapshotTests.cpp` pins both and writes them
   through `Logger::WriteMessage` so they land in every CI log. The figure was 1,136 here and in three
   ADRs: §4's arithmetic omitted the fire-event count byte this same section specifies two paragraphs
   below. **Historical since [`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md)**, which owes
   the update's measured size in their place, and the four other figures its Measurements list.
2. ~~**Tap-to-visible latency on real hardware**~~ — timestamp the `Tapped` event and the first frame in
   which the ship's drawn heading changes. §4 predicts 152 ms average. **CLOSED AT M0.23 ON LOOPBACK**,
   by the owner's ruling of 2026-09-23 that this game is tested on one machine: the figure and the three
   ways it differs from §4's question are in
   [`ADR-003`](ADR/ADR-003-the-record-and-the-command.md)'s Measurements. The network term is not in it
   and will not be.
3. ~~**The tick's cost** at 110 entities on the host, and how far from 50 milliseconds it is.~~ —
   **DISCHARGED at M2.14, 2026-09-24: 27 µs mean and 1.1 ms worst at 110 entities, 49 µs and 0.81 ms at
   220**, `Release|ARM64` on the Surface Pro, with every fleet ordered inside the timed tick. The table,
   the method and what it leaves out are in [`ADR-002`](ADR/ADR-002-tick-and-numbers.md)'s Measurements.
4. ~~**Packet loss and jitter on a real wireless link between two machines**~~ — **WITHDRAWN by the
   owner, 2026-09-23**: this game is tested on one machine and no second machine will be used. Loopback
   is the only link measured, at zero loss.
5. **The frame time on an actual Surface Pro** at **2880 × 1920 and at 1440 × 960**, at one sample and at
   four, on **both x64 and ARM64** — the four figures that settle which scale ships
   ([`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)). The Surface Pro 11 is a Snapdragon X
   part, so **ARM64 is the target platform — and CI builds no ARM64 at all** (`AGENTS.md` §6). The platform this game is actually for is the one nothing automated
   ever compiles, which makes this a standing obligation rather than a one-off measurement.
   **HALF-DISCHARGED at M0.16:** both scales, both platforms, **at one sample**, are measured on the
   device and the figures are in ADR-016's Measurements — stated there and not repeated here, because
   four copies of a frame time is how the other figures in this section went wrong. The four-sample half
   waits on the resolve step. **And the figures are of a frame with nothing in it**, so they bound the
   present path rather than the budget; ADR-016 says which is which.
6. ~~**The interface pass against the world pass**, which the review predicts will be the larger of the two:
   five instanced draws of simple geometry against an unbatched quad per glyph.~~ — **MEASURED 2026-09-23,
   and the prediction was wrong: the interface is about a seventeenth of the world.** The mean GPU time
   over 3,600 frames was **world 852 µs** (max 1,922), **present 636 µs** (max 2,160) and **interface
   50 µs** (max 700), out of a whole frame of 1,539 µs. Setup: the Surface Pro 11, `Release|ARM64`, a
   fullscreen 2880 × 1920 swap chain at the 1:1 world scale, the sky and three hulls in the world, and
   the four panels at rest with nothing selected. The host ran on the same machine. The spans come from two timestamps
   `GraphicsDevice` writes inside the frame (`MarkWorldDrawn`, `MarkPresentScaled`), split by
   `Neuron::SplitGpuFrame`, which `NeuronClientTests` pins. **The prediction assumed an unbatched quad
   per glyph**, and M1.13 made text one instanced call, which is most of why it is wrong. **The finding
   is the present**: a 1:1 blit that needs no filtering costs three-quarters as much as the whole world.
   It includes the back buffer's clear and its barriers, and it is where a frame-time saving would
   come from first.
7. ~~**That the present step really takes the path the scale calls for on the device**~~ — **DISCHARGED
   at M0.16.** Confirmed by looking at it on the device at both scales: unfiltered and pixel-exact at the
   1:1 default, point-sampled at an exact 2× at 0.5, no soft edge at either. R13's whole arrangement was
   worthless if a conversion error landed the scale at 1.99 rather than 2, or at 0.999 rather than 1, and
   that is the error a one-pixel edge exists to show. **The same gate settled which scale ships — 1:1**
   ([`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md)). ~~**And that the interface lands
   identically at both**~~ — **DISCHARGED at M0.15**, by the two fit tests ADR-016's Measurements name; it
   was a test rather than a look, and it is the regression ADR-016's two transforms exist to prevent.
8. ~~**Which loopback exemption form a UDP client needs**, `-a` alone or `-a` and `-is`, established at M0 by
   removing the exemption and trying again exactly as `AGENTS.md` §3 instructs.~~ — **DISCHARGED at
   M0.5, 2026-09-23: `-a` alone.** Without it the client cannot even send; with it, replies arrive with no
   listener running. The runs are in [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md)'s
   Measurements.

## 10. The decisions this design takes

Recorded under [`ADR/`](ADR/README.md) because each is expensive to reverse and each constrains how code is
shaped:

| ADR | |
|---|---|
| [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) | The simulation is two-dimensional; the camera is not. |
| [`ADR-002`](ADR/ADR-002-tick-and-numbers.md) | The 20 Hz tick, the 1/256 position unit, the binary angle and the sine table, the pinned PRNG, and ordering as a correctness property. |
| [`ADR-003`](ADR/ADR-003-the-record-and-the-command.md) | The entity record's fields, the 1,232-byte payload, 20 Hz and the 75 ms delay, and commands made reliable by a sequence the downstream state carries and validated by the host. Its full snapshot is replaced by ADR-024. |
| [`ADR-024`](ADR/ADR-024-replication-is-prioritized-records.md) | Replication is prioritized absolute-state records: one whole datagram at a time from a per-client accumulator with a sweep guarantee, a twelve-byte record, a header that does not scale with players, repeated removals, no fragmentation and no ordering. |
| [`ADR-022`](ADR/ADR-022-a-bot-is-a-headless-client.md) | A bot is a headless client, and one unpackaged process runs many of them as players, churners and flooders to load the host. |
| [`ADR-023`](ADR/ADR-023-the-player-count-is-configurable.md) | The host's player count is a run-time argument, past four only in a stress configuration. |
| [`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md) | No projectile entities; damage lands on the firing tick and the client draws an event. |
| [`ADR-012`](ADR/ADR-012-a-shader-is-compiled-into-a-header.md) | A shader is compiled by `dxc` at Shader Model 6.7 into a checked-in header, which raises the minimum Windows to 10.0.22621.0. |
| [`ADR-013`](ADR/ADR-013-a-client-is-told-which-player-it-is.md) | A client learns its player, a session token, the seed and, since M2.3, the player count from a join the host answers. |
| [`ADR-015`](ADR/ADR-015-the-base-is-built-from-modules.md) | The base is built from modules, each a separate destroyable entity placed by tap inside the point-defense radius. An L2 is an in-place upgrade of an L1, at the difference in cost. |
| [`ADR-005`](ADR/ADR-005-a-mesh-is-a-cmo-file.md) | A mesh is a CMO file, authored as content, with the reader written here because CMO's only reader in the wild is the DirectXTK12 R14 closes. Replaced the opposite decision, that meshes are functions. |
| [`ADR-021`](ADR/ADR-021-content-ships-with-the-package.md) | Content files ship with the package. The line is against dependencies and against simulation data becoming files, not against files as such. |
| [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) | A ship is a hull, a drive and its slots from the first line, with every stat derived by one tested pure function. |
| [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) | The authored frame was pinned at 1440 × 960 for an exact 2× fit; amended by ADR-016, which makes it a scale. |
| [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md) | The host address is configuration with a compiled-in default; no discovery, and the loopback exemption is a development arrangement. |
| [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md) | Text is DirectWrite rasterised into a D3D12 atlas — no Direct2D, no D3D11On12, no dependency. |
| [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md) | A tap selects one ship; a hold selects the same design within a screen-space circle. No band select, and one-finger drag is unconditionally panning. |
| [`ADR-011`](ADR/ADR-011-the-interface-draws-after-the-scale.md) | The interface draws after the scale at physical resolution, in authored coordinates through the transform R13 already computes. Breaks R13's letter, keeps its intent. |
| [`ADR-016`](ADR/ADR-016-the-world-resolution-is-a-scale.md) | The world's resolution is a scale of the panel defaulting to 1:1, and the interface gets a fit transform of its own rather than borrowing the world's. |
| [`ADR-017`](ADR/ADR-017-group-selection-is-a-double-tap.md) | Group selection is a double tap rather than a hold; the first tap acts at once and the second upgrades it, and `Holding` is freed. |
| [`ADR-018`](ADR/ADR-018-the-camera-is-anchored-to-the-plane.md) | The camera is ray-anchored to the plane; one solve drives pan, zoom and orbit, there is no inertia, and a hold on empty space recenters. |
| [`ADR-019`](ADR/ADR-019-the-sky-is-generated-from-the-seed.md) | The sky is instanced stars generated from the match seed, and nothing else: the galaxy band was withdrawn on 2026-09-22 after it was looked at on the device. |
| [`ADR-020`](ADR/ADR-020-damage-offscreen-is-announced-at-the-edge.md) | Off-screen damage shows as a clustered directional indicator at the screen edge, derived from data already sent and tappable to recenter. |

The decisions that are *not* taken yet, and which the work will meet, are on the register in
[`OpenQuestions.md`](OpenQuestions.md).
