# Technical Design — *Outpost Commander*

How [`GameDesign.md`](GameDesign.md) is built inside the rules of [`AGENTS.md`](../AGENTS.md). That file is
read first and is cited by rule number here rather than restated.

**Status: DRAFT.** The figures marked *arithmetic* are exactly that — quantities derived from the design's
numbers, so the shape can be argued about before anything is written. `AGENTS.md` §6 requires a figure to
be measured before it is quoted as fact; §9 lists the ones that must be, and none of them can be until
there is code.

---

## 1. Where everything goes

The six libraries and their edges are `AGENTS.md` §2 and are not restated. This is the mapping of the
design onto them:

| Concern | Library | Why there |
|---|---|---|
| Fixed point, binary angles and the sine table, the PRNG, integer square root | `NeuronCore` | Engine, and both sides need it. |
| Packet header, fragmentation and reassembly | `NeuronCore` | Transport framing knows nothing about the game (R9), and the two sides must agree on it byte for byte. |
| The UDP endpoint over `DatagramSocket` | `NeuronClient` | C++/WinRT, Windows Store family. |
| Direct3D 12 device, swap chain, scene target, the scaled present | `NeuronClient` | R12, R13. |
| The DirectWrite glyph atlas and the text quad renderer | `NeuronClient` | Engine: a glyph cache knows nothing about the game (R9). |
| The `CoreWindow` seam: `GestureRecognizer` in, an input queue out | `NeuronClient` | R18, R21. The arithmetic under a gesture is a pure function with a suite over it. |
| The UDP endpoint over Winsock2 | `NeuronServer` | Desktop family. |
| Entities, the component catalog, derived stats, the damage table, the generator, every wire record | `GameCore` | Game vocabulary both sides must share. The client previews against these rules; the host validates with them (R19). |
| The simulation, the AI, the match | `GameLogic` | Host only. The client does not link it. |
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

**There are no floats anywhere in the simulation**, which R16 requires and which `/arch:AVX2` on x64 makes
load-bearing rather than tidy: the compiler may contract `a*b+c` into an FMA even under `/fp:precise`, and
may contract differently at different optimisation levels, so a float in the simulation is a Debug and
Release that disagree. ARM64 sets no such switch, so it is also a divergence between the two platforms CI
builds. Integers cannot be reached by either.

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

Drain incoming commands, then: orders, AI, movement, weapons, mining, build queues, deaths, victory. One
pass, fixed order, no system reading another's half-updated output. At the end of the tick the host
computes a **state hash** over every entity's identity, position, heading and hull, which is what the
determinism test asserts and what a desynchronisation report would carry.

---

## 3. The world, and generating it

The area is `GameCore` code taking a seed and producing a list of placed objects. It runs on the host to
populate the match, and **it runs on the client to draw the same asteroids** — the client derives the map
rather than being sent it, which keeps a large static payload off the wire and makes it impossible for the
two sides to disagree about where a rock is.

This is not the client simulating (R19). A generator is a rule, it lives in `GameCore`, and `GameCore` is
precisely the shared rules both sides evaluate. The simulation still owns how much ore is *left* in an
asteroid, and that is replicated like anything else.

One quadrant is generated and copied at 90°, 180° and 270° (`GameDesign.md` §3). The rotation is exact
because positions are integers and a 90° rotation on integers is a swap and a negation — a floating-point
rotation would make the four starts subtly unequal, which is the kind of unfairness nobody would find for
a year.

---

## 4. Replication

R19 settles the architecture before anything else does: **the client links no simulation, so lockstep is
impossible.** The host simulates and sends state; the client renders what it was sent and sends commands.
There is nothing to re-litigate here.

### Downstream: full snapshots, no delta

**Every snapshot is self-contained.** No baselines, no acknowledgements, no history ring. A lost packet
costs one frame of animation and cannot cause divergence, because there is no accumulated state on the
client to diverge. This is [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) and it is the single
largest simplification in the MVP.

An entity record is nine bytes:

| Field | Bytes | Note |
|---|---|---|
| Entity identity | 2 | Index and generation packed. |
| Position x, y | 4 | Two `std::int16_t`. The 16,384-unit square over 65,536 steps is **a quarter of a world unit** per step — far finer than a ship is wide. |
| Heading | 1 | 256 steps, 1.4°. This is a rendering quantity; the simulation's heading is 16-bit. |
| Hull remaining | 1 | Percent. |
| Flags | 1 | Design identity index, team, and state bits. |

Peak occupancy is four players at fifty ships plus four stations, so **204 records, 1,836 bytes**, plus a
header carrying the protocol version, the snapshot sequence, the tick, the entity count, each player's
credits and each player's last applied command sequence — about 36 bytes. **1,872 bytes per
snapshot**, which is *arithmetic*, not a measurement.

The safe UDP payload is 1,200 bytes, so a snapshot is **two fragments**, and fragments reassemble
all-or-nothing under one sequence number. At ten snapshots a second that is **18.7 KB/s to each client and
75 KB/s out of the host**, or 600 kbit/s — comfortable on a LAN, which is the MVP's target, and modest
even off it.

**The honest cost of all-or-nothing reassembly:** with two fragments, a snapshot is lost at roughly twice
the packet loss rate, and a lost snapshot at 10 Hz is a 200-millisecond gap the interpolator has to cover.
On a LAN that is nothing. On a congested wireless link it is the first thing that will look wrong, and the
two levers are named in the ADR — raise the rate to 20 Hz, which halves the gap and doubles the bandwidth,
or add delta encoding, which is where the complexity the MVP declined is waiting.

**The snapshot carries a per-player entity set from the first line, and in the MVP that set is everything.**
There is no fog of war (`GameDesign.md` §10), but the host builds the list of what a given player may see
rather than serialising the world, so adding visibility later changes one function and not the wire format.
The client is written to never assume it can see everything.

### Upstream: commands, made reliable by the snapshot

A command is an order: a type, a target point or entity, and the identities of the selected ships. A
selection of fifty ships is a hundred bytes, so a command packet fits a single datagram with room to spare.

Commands carry a per-player sequence number and are **repeated in every outgoing packet until they are
acknowledged**, and the acknowledgement is the `lastCommandSeqApplied` field the snapshot already carries.
The host applies commands in sequence order and ignores anything at or below what it has applied. That is
reliable ordered delivery for the one channel that needs it, in about thirty lines, with no general
reliability layer and no second timer.

A client with nothing to say sends a heartbeat a few times a second so the host can time it out.

---

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

**M0 establishes both paths** (`GameDesign.md` §10), including which exemption form a UDP client actually
needs: if replies to a bound socket require the inbound form `-is`, then `CheckNetIsolation.exe` must stay
running the entire time the client is listening, and the single-machine loop stops being worth having.

Encryption, authentication and any defence against a hostile client are not in the MVP. The protocol
version in the header refuses a mismatched build, and that is the whole of it.

---

## 6. The client's frame

One drain of the `CoreWindow` dispatcher per frame (R18), then the packet queue, then the interpolation
clock, then render, then present.

**The client renders the past.** It holds the two most recent snapshots and draws at a time about 150
milliseconds behind the newest — one snapshot interval plus a jitter margin. Positions and headings are
interpolated between the two; headings interpolate the short way round, which the binary angle makes a
subtraction rather than a special case. If the next snapshot has not arrived, the client extrapolates for a
short bounded window and then holds position rather than sliding a ship somewhere it never was.

**Rendering is R13's arrangement**, and [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) settles
the resolution R13 deliberately leaves open: **the game is authored at 1440 × 960**. Every pass draws into
a scene target at that size, and the frame ends by fitting it into the back buffer with the aspect
preserved. The Surface Pro's panel is 2880 × 1920 and the swap chain is created at those physical pixels,
**so the fit is an exact 2× and takes the point-sampled path** — R13's crisp case is the only one the
target device takes. Exactly one place asks the window how big it is, and it is the conversion R18 requires
a suite over.

**A 1.38-megapixel scene target is small, and that is what makes multisampling affordable.** The sample
count is one constant; the MVP ships one sample, and 4× — 5.5 megasamples, which this hardware will not
notice — is the expected first change, with the resolve step going in beside it. Space is thin bright
silhouettes against black, which is exactly the content that wants it. The back buffer cannot be
multisampled at all, since DXGI's flip model requires `SampleDesc.Count` of 1, and that is most of why the
scene target exists.

Drawing 204 ships is **one instanced draw per hull**, with a per-instance buffer of a transform and a team
colour. Three hulls, one station mesh, one asteroid mesh: five draws for the whole field. Two frames in
flight with a fence per frame. None of this is near any limit, and the renderer should not be optimised
until something measured says to.

**Text is DirectWrite rasterised into an atlas we own**
([`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md)),
built at startup and drawn as instanced quads — one more draw. **No Direct2D and no `ID3D11On12Device`**,
both of which R12 bans by name, which closes the route every D3D12 text sample takes. Coverage is
rasterised as ClearType and the three subpixel values averaged into one channel, because subpixel output
would arrive as colour fringing after the 2× scale. The font family is pinned and a missing family fails
at startup rather than substituting, since a substituted font has different advance widths and R13 requires
every layout number to be unconditional.

**Suspend and resume cost nothing structurally.** A packaged application is suspended when it loses the
foreground and the match runs on; on resume the client reconnects and the first self-contained snapshot
restores everything, with a reconnecting overlay in between (`Interface.md` §7). There is no
resynchronisation path to write, which is [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) paying
for itself a second time. The same is true of a player who disconnects outright (`GameDesign.md` §2).

---

## 7. Content, and why there is no content pipeline

**R14 closes the dependency list**, and a mesh format is where a project reaches for a library without
noticing. There is no glTF loader here, no FBX, no DirectXTK12, and there will not be one in the MVP.

**Meshes are generated in code.** A hull is a function that emits a few dozen triangles — a fuselage, an
engine block, a pair of wings — parameterised so the three hulls share the code that makes them. Normals
are baked per face onto split vertices, which is flat shading, which is what a low-polygon faceted look
wants anyway; there is no smoothing group to decide and no tangent basis to get wrong. Team colour is a
vertex attribute selecting between a hull palette and the owner's colour.

This costs nothing today and buys an MVP with **no file format, no loader, no asset build step and no
third-party anything**. It is [`ADR-005`](ADR/ADR-005-meshes-are-generated-in-code.md), and what reopens it
is the first time a ship needs to look like something a function cannot describe — at which point a small
hand-rolled binary format read by `GameClient` is the answer, still with no dependency.

The component catalog, the designs and the damage table are `constexpr` tables in `GameCore` for the MVP,
not files. A content file format is worth designing when there is something to put in it and when mods are
a goal; both are post-MVP.

---

## 8. What each suite owns

`AGENTS.md` §3 is blunt that vstest reports an empty suite as a pass, so each of the six has a stated job
and the placeholder goes the day the first real test lands.

| Suite | Owns |
|---|---|
| `NeuronCoreTests` | Fixed-point multiply and divide at the edges of `int32`, the sine table against a reference, integer square root, the PRNG's first thousand outputs pinned, fragmentation and reassembly including a lost fragment and a duplicate. |
| `NeuronClientTests` | The device-independent-pixel to physical-pixel conversion (R18), the present-scaling fit at 1:1, at integer multiples and at neither, and the gesture arithmetic — **the sign of a pinch and of a rotation**, which R21 points out a package can hide and a test cannot. Plus atlas packing, and that a glyph's advance width survives the round trip. |
| `NeuronServerTests` | The Winsock2 endpoint against a loopback peer: send, receive, a short read, a datagram larger than the buffer. |
| `GameCoreTests` | Derived design stats for every catalog combination, the damage table, the generator's output pinned for a seed **and its four-fold symmetry asserted**, and every wire record encoded and decoded round trip. |
| `GameClientTests` | Interpolation between two snapshots including the wrap-around case, the camera's transform, hit-testing a tap against the plane at several camera angles, and **the hold-selection circle** — which ships a 192-pixel screen-space radius takes at several zoom levels, including the boundary case of a ship exactly on the edge. |
| `GameLogicTests` | The simulation: movement toward a point, the mining loop, combat resolution, elimination and victory — and **the determinism test**, which runs a fixed tick count from a seed against a scripted order list and asserts the state hash. That last one is what protects R16, and it is the most valuable test in the tree. |

---

## 9. What must be measured, and is not yet

`AGENTS.md` §6 requires a figure to be measured before it is quoted, and everything numeric above that is
not a definition is arithmetic on the design's own starting values. These are owed:

1. **The snapshot's real size** at 204 entities, from the encoder rather than from this table.
2. **The tick's cost** at 204 entities on the host, and how far from 50 milliseconds it is.
3. **Packet loss and jitter on a real wireless link between two machines**, which decides whether 10 Hz
   and two-fragment snapshots survive contact — owed at M0, because it is the cheapest possible moment to
   find out the answer is no.
4. **The frame time on an actual Surface Pro** at 1440 × 960, at one sample and at four, on **both x64 and
   ARM64** — the Surface Pro 11 is a Snapdragon X part, so the ARM64 leg is a real target here rather than
   a CI formality.
5. **That the present step really takes the point-sampled path on the device**, confirmed by looking at it.
   R13's whole arrangement is worthless if a conversion error lands the scale at 1.99.
6. **Which loopback exemption form a UDP client needs**, `-a` alone or `-a` and `-is`, established at M0 by
   removing the exemption and trying again exactly as `AGENTS.md` §3 instructs.

---

## 10. The decisions this design takes

Recorded under [`ADR/`](ADR/README.md) because each is expensive to reverse and each constrains how code is
shaped:

| ADR | |
|---|---|
| [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) | The simulation is two-dimensional; the camera is not. |
| [`ADR-002`](ADR/ADR-002-tick-and-numbers.md) | The 20 Hz tick, the 1/256 position unit, the binary angle and the sine table, the pinned PRNG, and ordering as a correctness property. |
| [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) | Full self-contained snapshots at 10 Hz with no delta and no acknowledgement; commands made reliable by a sequence the snapshot already carries. |
| [`ADR-004`](ADR/ADR-004-weapons-resolve-at-the-fire-tick.md) | No projectile entities; damage lands on the firing tick and the client draws an event. |
| [`ADR-005`](ADR/ADR-005-meshes-are-generated-in-code.md) | No content pipeline and no mesh format in the MVP. |
| [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) | A ship is a hull, a drive and its slots from the first line, with every stat derived by one tested pure function. |
| [`ADR-007`](ADR/ADR-007-the-authored-frame-is-1440x960.md) | The authored frame is 1440 × 960 — an exact 2× point-sampled fit on the Surface Pro. |
| [`ADR-008`](ADR/ADR-008-the-host-address-is-configuration.md) | The host address is configuration with a compiled-in default; no discovery, and the loopback exemption is a development arrangement. |
| [`ADR-009`](ADR/ADR-009-text-is-directwrite-into-an-atlas.md) | Text is DirectWrite rasterised into a D3D12 atlas — no Direct2D, no D3D11On12, no dependency. |
| [`ADR-010`](ADR/ADR-010-selection-is-proximity-and-design.md) | A tap selects one ship; a hold selects the same design within a screen-space circle. No band select, and one-finger drag is unconditionally panning. |

The decisions that are *not* taken yet, and which the work will meet, are on the register in
[`OpenQuestions.md`](OpenQuestions.md).
