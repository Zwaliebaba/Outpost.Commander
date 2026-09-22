# M0 — The wire

[`GameDesign.md`](../GameDesign.md) §10 defines this milestone and is not restated here. Its shape in one
line: **the host simulates one moving entity and the packaged client draws it, and a tap moves it.** No
game at all — what it proves is the tick, the packet format, the two socket APIs talking to each other,
the D3D12 frame, the two-pass renderer, the gesture seam, and whether a single-machine development loop is
usable on real hardware.

**Read [`README.md`](README.md) first** if you have not: how a step is written, what an agent must not do,
and why a step marked **gate** is never an agent's to close.

**Entry state.** The tree as `AGENTS.md` describes it — six libraries, two executables, six suites, and
between them a handful of functions that return their own name so that every edge of the build is
exercised by something. Nothing below builds on anything that exists; it all builds on the shell.

**Twenty-three steps and three gates.** M0 is roughly half the engineering in the MVP and it is the
milestone labeled "no game at all" (`README.md` F8). That is the correct shape for a plan ordered by risk
— everything that can turn out to be impossible is in here — and it is said out loud because the name
reads like a week.

**Every suite loses its `SuiteSmoke` in this milestone**, each at the step that gives it a real test:
`NeuronCoreTests` at M0.1, `NeuronServerTests` at M0.3, `NeuronClientTests` at M0.4, `GameLogicTests` at
M0.8, `GameCoreTests` at M0.9, `GameClientTests` at M0.19. Delete it in that same commit and never before
(`AGENTS.md` §3).

**What M0 deliberately does not do:** no designs, no components, no derived stats, no generator, no
economy, no combat, no AI, no text. One entity, one player, one fixed position for it to start at. The
component model is M1's and it goes in before its interface, exactly as
[`ADR-006`](../ADR/ADR-006-a-ship-is-a-composition.md) requires — but it is not needed to prove a packet.

---

## The transport, and the gate under it

The first four steps exist to reach M0.5 as fast as possible, because M0.5 can end the single-machine
development loop and everything after it is cheaper to write once that is known.

### M0.1 — The byte reader and writer · `NeuronCore` · `NeuronCoreTests` · agent

**Read first:** `AGENTS.md` R9 and §1; `TechnicalDesign.md` §1.

**Adds:** `ByteWriter` and `ByteReader` over a caller-owned buffer — explicit little-endian, bounds
checked, no allocation and no exceptions. They live in the engine because framing knows nothing about the
game (R9), and everything on the wire in M0.9 and M0.10 is written through them.

**Files:** `NeuronCore/ByteWriter.h` `.cpp`, `NeuronCore/ByteReader.h` `.cpp`; `NeuronCore.vcxitems` and
its `.filters`; `Tests/NeuronCoreTests/ByteCodecTests.cpp` and that project's two files. Shared-items
sources are `PrecompiledHeader: NotUsing` and include their own master header first — `AGENTS.md` §2 states
it and the existing `NeuronCore.cpp` shows it.

**Done when:** every width round trips; a write past the end fails rather than corrupting; a read past the
end fails rather than returning rubbish; and **the byte order is asserted against a literal** — a codec
tested only against itself is a codec that agrees with itself about being wrong.

### M0.2 — The packet header · `NeuronCore` · `NeuronCoreTests` · agent

**Read first:** [`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md) Decision; `TechnicalDesign.md`
§4.

**Adds:** protocol version, packet type, sequence, and the fragment index and count. **The fragment fields
are present from the first packet although the MVP never fragments**: ADR-003 puts the two-fragment path at
the fourth player, and a field added later is a format change where a field always there is a fast path.
Reassembly itself is M4.3 and is not written here.

**Files:** `NeuronCore/PacketHeader.h` `.cpp`; `NeuronCore.vcxitems` + `.filters`;
`Tests/NeuronCoreTests/PacketHeaderTests.cpp`.

**Done when:** the header round trips; a mismatched protocol version is rejected by the reader and the
rejection is distinguishable from a malformed packet; a single-fragment packet is recognized as complete
without consulting a reassembler that does not exist.

### M0.3 — The Winsock2 transport · `NeuronServer` · `NeuronServerTests` · agent

**Read first:** `TechnicalDesign.md` §5; `AGENTS.md` §2 — `NeuronServer` is a desktop build and that is
why this side is Winsock at all.

**Adds:** one non-blocking UDP socket — bind, send, receive — drained at the top of a tick and written at
the end, on one thread, with `WSAStartup` and `WSACleanup` owned by RAII rather than by a pair of calls
someone has to remember.

**No shared `Transport` interface with M0.4's client side.** The two never link into one binary, and R2 is
explicit that a base class for one derived class is ceremony. Two concrete types with no relationship is
the right shape here and it is worth not inventing the abstraction.

**Files:** `NeuronServer/WinsockTransport.h` `.cpp`; `NeuronServer.vcxproj` + `.filters`;
`Tests/NeuronServerTests/WinsockTransportTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's four are pinned against a loopback peer the test stands up itself
— send, receive, a short read, and a datagram larger than the buffer.

### M0.4 — The DatagramSocket transport and the packet queue · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** `TechnicalDesign.md` §5, the threading-seam paragraph; R14.

**Adds:** `DatagramSocket` through C++/WinRT, and the mutex-guarded queue the frame loop drains. **The
`MessageReceived` handler does exactly one thing: copy the datagram's bytes in and return.** It arrives on
a thread pool thread, and nothing parses a packet there, nothing allocates there, and nothing touches
renderer or replica state there. The critical section is a copy and a push.

**Files:** `NeuronClient/DatagramTransport.h` `.cpp`, `NeuronClient/PacketQueue.h` `.cpp`;
`NeuronClient.vcxproj` + `.filters`; `Tests/NeuronClientTests/PacketQueueTests.cpp`.

**Done when:** the queue is pinned — concurrent pushes from several threads drain in order with nothing
lost and nothing torn; a full queue drops the oldest rather than blocking the pool thread, and the drop is
counted rather than silent. **The socket itself is not unit-tested and cannot honestly be**; M0.5 is what
proves it, and saying so here is better than a test that pretends.

### M0.5 — GATE: a datagram crosses · — · hand · **human**

**Read first:** [`ADR-008`](../ADR/ADR-008-the-host-address-is-configuration.md) in full; `AGENTS.md` §3's
loopback paragraph; `TechnicalDesign.md` §9.4 and §9.8.

**Adds:** no product code. A temporary host mode that sends a numbered packet at a fixed rate and a client
that logs what arrives — the shell's existing `OutputDebugStringA` reporting, one step on.

**Done when** each of these has an answer, written into ADR-008 and `TechnicalDesign.md` §9 in one pull
request:

1. Host and packaged client **on two machines over the LAN**: packets arrive.
2. Same machine over `127.0.0.1` under the exemption Visual Studio grants on every F5: packets arrive.
3. **The exemption removed and tried again** — §9.8. Whether `-a` suffices or the inbound form `-is` is
   needed for replies to a bound socket, and if it is `-is`, that `CheckNetIsolation.exe` must stay
   running the whole time the client is listening.
4. **Loss and jitter measured on the real wireless link** over a run of some minutes — §9.4.

**If the single-machine loop turns out not to be usable, that is this gate succeeding.** `GameDesign.md`
§10 wants that answer in week one, and it is far better to know it before there is a renderer to deploy.

---

## The numbers, and the host

### M0.6 — Fixed point, the vector, the angle and the sine table · `NeuronCore` · `NeuronCoreTests` · agent

**Read first:** [`ADR-002`](../ADR/ADR-002-tick-and-numbers.md) Decision, in full; `TechnicalDesign.md` §1
and §2.

**Adds:** the representation ADR-002 settles and nothing beyond it — position and velocity as
`std::int32_t` with eight fractional bits, multiplication through a 64-bit intermediate that is not
optional, the two-dimensional vector over that, the `std::uint16_t` binary angle, the 4,096-entry Q1.15
sine table, and an integer square root.

**ADR-002 says `std::int32_t`, so this is an alias with named conversions and not a strong type.** A strong
type would catch a class of unit error the compiler otherwise cannot, and it would amend ADR-002 — which
makes it that ADR's decision to take, in its own pull request, not this step's to take quietly.

**The table saturates at the cardinals.** Q1.15 spans [−1, +0.999969], so `sin(90°)` is 32,767 and not
32,768. ADR-002 states it precisely so that the first implementer meets it as a documented property rather
than as an off-by-one that compiles.

**Files:** `NeuronCore/FixedPoint.h` `.cpp` — **never `Math.h`**; `NeuronCore/Vec2.h` `.cpp` — **never
`Vector.h`**, which an angled `<vector>` would find; `NeuronCore/SineTable.h` `.cpp`; `NeuronCore.vcxitems`
+ `.filters`; `Tests/NeuronCoreTests/FixedPointTests.cpp`, `SineTableTests.cpp`.

**Done when:** multiply and divide are pinned **at the edges of `int32`** and not only in the comfortable
middle; the sine table matches a reference within its stated 0.088° resolution and the four cardinals are
asserted at the saturated value; the integer square root is exact on perfect squares and correctly floored
between them; and the difference of two headings across the wrap is a subtraction with no special case,
which is the whole reason for the binary angle.

### M0.7 — The PRNG · `NeuronCore` · `NeuronCoreTests` · agent

**Read first:** ADR-002's PRNG paragraph; R16.

**Adds:** one generator, written out, seeded from the match, and the only source of randomness in the tree.
Name the type for what it is — `Pcg32`, `Xoshiro256ss` — and the file follows R7. **No
`std::random_device`, no hash of an address, and no standard-library distribution**: the engines are
specified well enough and the distributions are not portable, so the bounded draw is written here and
pinned here.

**Files:** `NeuronCore/<TheGenerator>.h` `.cpp`; `NeuronCore.vcxitems` + `.filters`;
`Tests/NeuronCoreTests/<TheGenerator>Tests.cpp`.

**Done when:** the first thousand outputs from a fixed seed are pinned against a table checked into the
test; the bounded draw is uniform and free of modulo bias, asserted rather than assumed; and two instances
from one seed produce the same stream.

### M0.8 — The entity store and the tick · `GameCore`, `GameLogic` · `GameLogicTests` · agent

**Read first:** ADR-002's ordering paragraph; `TechnicalDesign.md` §2 and §1.

**Adds:** entities in a `std::vector` with a free list, identified by **index and generation**; the tick
iterating in index order; the fixed system order `TechnicalDesign.md` §2 names, carrying only the systems
that exist; and the **state hash** over identity, position, heading and hull that the determinism test will
one day assert. At M0 the simulation is one entity moving toward a point.

`GameCore` owns the entity *record* and `GameLogic` owns the *simulation* over it (`TechnicalDesign.md`
§1). If that split turns out to be awkward in practice, **say so in the report** rather than moving the
boundary quietly — it is the boundary R19 rests on.

**Files:** `GameCore/Entity.h` `.cpp`; `GameLogic/World.h` `.cpp`, `Tick.h` `.cpp`, `StateHash.h` `.cpp`;
both project files and both `.filters`; `Tests/GameLogicTests/TickTests.cpp`.

**Done when:** a freed and reused index carries a new generation and a stale identity does not resolve;
movement toward a point arrives and stops rather than oscillating across it, which is the classic
fixed-point failure here; the state hash is stable across two runs of one input and changes when any
hashed field changes.

### M0.9 — The wire records and the snapshot encoder · `GameCore` · `GameCoreTests` · agent

**Read first:** ADR-003 Decision, in full; `TechnicalDesign.md` §4.

**Adds:** the snapshot as ADR-003 specifies it — the header, the per-player blocks sized by the count in
the header, the ten-byte entity records with the design identity in its own byte, the removal list, and the
fire-event list behind its count — **encoded and decoded in full at M0**, although M0 has one entity,
nothing to remove and nothing firing. Proving the format is what this milestone is for, and the empty lists
cost a byte each.

**This step is where the field widths become facts** (`README.md` F7). ADR-003 gives totals and names the
contents; only the encoder settles how wide each field is, and the sizes it produces are what ADR-003's
table gets corrected against.

**Files:** `GameCore/Snapshot.h` `.cpp`, `GameCore/EntityRecord.h` `.cpp`; `GameCore.vcxitems` +
`.filters`; `Tests/GameCoreTests/SnapshotTests.cpp`.

**Done when:** every record round trips, including a removal list, a fire event and both player counts —
**and `TechnicalDesign.md` §9.1 is discharged.** A test encodes 102 synthetic entities with two player
blocks and three removals, asserts the result is a single datagram inside the 1,232-byte figure ADR-003
uses, and writes the byte count through `Logger::WriteMessage` so it appears in every CI log rather than in
one person's notes. The same test records the 204-entity size. **The measured figures replace the
arithmetic in ADR-003's table in the same pull request** — that is what `AGENTS.md` §6 means by a figure
being measured before it is quoted.

### M0.10 — Commands, and the host's validation of them · `GameCore`, `GameLogic` · both suites · agent

**Read first:** ADR-003's command and validation paragraphs; `TechnicalDesign.md` §4 upstream;
`OpenQuestions.md` Q24.

**Adds:** the command record — type, target point or entity, the selected identities, a per-player sequence
— its codec in `GameCore`, and the host's intake in `GameLogic`: apply in sequence order, ignore anything
at or below what has been applied, and validate before applying.

**Validation is correctness, not security** (Q24), and the five checks are ADR-003's: ownership; the
selection bounded at the sender's own entity count; generation; the target point clamped to the play area;
and `uint16` sequence wraparound handled explicitly rather than left to an "at or below" comparison that
inverts. At M0 there is one entity and the ownership check has nothing to reject — **write it anyway.**
Thirty lines now against a retrofit into a loop that has since grown.

**Files:** `GameCore/Command.h` `.cpp`; `GameLogic/CommandIntake.h` `.cpp`; both project files and
`.filters`; `Tests/GameCoreTests/CommandTests.cpp`; `Tests/GameLogicTests/CommandValidationTests.cpp`.

**Done when:** the record round trips; a foreign entity, an over-long selection, a stale generation, an
out-of-map target and a wrapped sequence are each rejected by a test named for that case; and a resent
command that was already applied is ignored with no side effect.

### M0.11 — The host runs · `Server`, `GameLogic` · hand · agent

**Read first:** R20; `TechnicalDesign.md` §1 and §5; ADR-008.

**Adds:** `main`, the host address and port as configuration, and the outer loop — drain the socket, tick,
encode, send, wait for the next 50 ms boundary. **This is the one seam where wall time meets the tick**
(R16) and it is the only place the two are allowed to meet.

**The executable holds glue and nothing else** (R20): the loop's body lives in `GameLogic` where a suite
can reach it and `Server.cpp` holds the shell around it. Check your own diff for this specifically — it is
the rule an executable breaks quietly, and a thing an executable holds is a thing no suite can reach.

**Files:** `Server/Server.cpp`; `GameLogic/Host.h` `.cpp`; `GameLogic.vcxproj` + `.filters`.

**Done when:** the host runs for some minutes at 20 Hz without accumulating drift, and a client on another
machine sees the snapshot sequence advance by exactly one per snapshot.

---

## The frame

### M0.12 — Window metrics and the two fit transforms · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** [`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md) **before**
[`ADR-007`](../ADR/ADR-007-the-authored-frame-is-1440x960.md) and
[`ADR-011`](../ADR/ADR-011-the-interface-draws-after-the-scale.md), which it amends; R13 and R18;
`Interface.md` §1.

**Adds:** **the one place in the client that asks how big the window is.** Two pure functions:
device-independent pixels to physical pixels, which R18 requires a suite over by name; and the fit —
authored size and physical size in, a scale, an offset and a filter choice out, covering R13's three cases
of 1:1 unfiltered, point sampling at an exact integer multiple, and bilinear otherwise, aspect preserved
and letterboxed.

**It returns a value rather than applying one, and it is called twice** (ADR-016). The **world fit** takes
the scene target's size; the **interface fit** takes 1440 × 960, the authored layout space. Those are two
different numbers doing two different jobs, and they coincide only at a 0.5 world scale.

**An earlier version of this step said "two consumers, one computation" and meant one value.** That is the
defect ADR-016 corrects: at the 1:1 world default the shared transform is identity, which renders every
panel, glyph and touch target at half size in one corner. One place asks the window anything; **two values
come out of it.**

**Files:** `NeuronClient/WindowMetrics.h` `.cpp`, `NeuronClient/FitTransform.h` `.cpp`;
`NeuronClient.vcxproj` + `.filters`; `Tests/NeuronClientTests/FitTransformTests.cpp`,
`WindowMetricsTests.cpp`.

**Done when:** the world fit is pinned at both settled scales — 2880 × 1920 into 2880 × 1920 is 1:1 and
unfiltered, the default; 1440 × 960 into 2880 × 1920 is exactly 2 and point-sampled, the 0.5 scale. **The
interface fit from 1440 × 960 into 2880 × 1920 is exactly 2 whichever the world is doing** — that assertion
is the regression the split exists to prevent and it is the most valuable test in this step. Also: the
world at 1:1 into 1920 × 1080 is 0.5625, bilinear and pillarboxed — the development case, deliberately not
the optimized one; a 16:9 window letterboxes and a 3:2 window does not; and the DIP conversion is pinned at
100%, 150%, 200% and a fractional scale.

### M0.13 — The device, the swap chain and frames in flight · `NeuronClient` · hand · agent

**Read first:** R12; `TechnicalDesign.md` §6; ADR-007 — **physical pixels, not DIPs.**

**Adds:** the D3D12 device, the command queue and allocators, a flip-model swap chain created at the
panel's physical pixels, and two frames in flight with a fence each. **COM lifetimes are RAII from the
first line** (R12): `winrt::com_ptr` is the smart pointer, WRL's `ComPtr` is not used, and a raw
`AddRef`/`Release` pair in new code is a defect rather than a style. Device removal is a path to be
written, not a crash to be met.

**Files:** `NeuronClient/GraphicsDevice.h` `.cpp`, `NeuronClient/SwapChain.h` `.cpp`;
`NeuronClient.vcxproj` + `.filters`.

**Done when:** the packaged client clears the back buffer and presents, on the device, at the panel's
physical resolution. **Nothing automated reaches this step** and the report says so.

### M0.14 — ADR-012: how a shader is built · `Design/ADR`, `NeuronClient` · hand · **human**, then agent

**Read first:** R14; [`ADR-005`](../ADR/ADR-005-a-mesh-is-a-cmo-file.md), which draws the same
dependency line one subsystem over; `README.md` F1.

**Adds:** the decision, taken before the shader rather than after it. `fxc` and `dxc` ship with the Windows
SDK and are inside R14's closed list; what nothing settles is whether the compiled object is **package
content** or a **header**. `NeuronClient` is a static library with no package of its own, so a `.cso` on
disk must be carried into `OutpostCommander`'s package by a project the library cannot see, while `/Fh`
produces a byte array the library includes and nothing else needs to know about. **That is the
recommendation; the ADR is where it is taken.**

Either way, `.hlsl` files become project items with a build step — `.vcxproj` and `.filters` entries, and
an `.editorconfig` section that is already written and waiting.

**Files:** `Design/ADR/ADR-012-<slug>.md`; the table row in `Design/ADR/README.md`; whatever project-file
change the decision implies.

**Done when:** the ADR is written, Accepted, and cited by M0.15.

### M0.15 — The scene target and the scaled present · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** R13; ADR-016, then ADR-007 and ADR-011; ADR-012.

**At 1:1 with one sample this step's present is a pure copy and buys nothing visible.** That is expected
and the indirection stays: it is what makes four samples a constant change rather than a rewrite, because
a flip-model back buffer cannot be multisampled (ADR-016).

**Adds:** the off-screen color target and its depth buffer at **the size the world scale gives** — two
constants now, the scale and the sample count (ADR-016). The default scale is 1:1, so on the target device
the target is 2880 × 1920; the MVP ships one sample and `TechnicalDesign.md` §6 expects four to be the
first change. Then the present step that fits the target into the back buffer through **M0.12's world
transform**, not the interface's. The
first HLSL in the tree: a full-screen triangle and the sampler the transform's filter choice selects.

**Files:** `NeuronClient/SceneTarget.h` `.cpp`, `NeuronClient/PresentStep.h` `.cpp`;
`NeuronClient.vcxproj` + `.filters`. **The shader is two files rather than the one this line first
proposed** — [`ADR-012`](../ADR/ADR-012-a-shader-is-compiled-into-a-header.md) fixed the layout as
`Shaders\PresentVS.hlsl` and `Shaders\PresentPS.hlsl`, one file per stage, and put both of them and their
project items in at M0.14; this step writes what they contain.

**Done when:** the scene target reaches the back buffer at the right scale with the right filter,
letterboxed where the aspect does not match, over several window sizes on a desktop machine. The
arithmetic is pinned by M0.12's tests rather than by this step, which is exactly why they went in first.

### M0.16 — GATE: the filter is right, and which world scale ships · — · hand · **human**

**Read first:** [`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md)'s Measurements 1 and 2;
`TechnicalDesign.md` §9.5 and §9.7.

**Adds:** nothing, and it now answers two questions rather than one. Deploy to the Surface Pro, fullscreen,
put a hard one-pixel edge in the scene target, and **look at it** — at the 1:1 default and again at a 0.5
scale. Then take the frame time at both, at one sample, **on x64 and on ARM64.**

**Done when:** the filter is confirmed by eye at both — unfiltered and pixel-exact at 1:1, point-sampled
and cleanly doubled at 0.5 — **and the four frame times are written into ADR-016's Measurements along with
which scale ships.** ADR-016 defaults to 1:1 on a judgment and names this gate as the thing that settles
it; if 1:1 does not hold the budget on ARM64 the scale goes to 0.5 and **the constant is the only thing
that changes.**

**R13's whole arrangement is worth nothing if a conversion error lands the scale at 1.99 rather than 2, or
at 0.999 rather than 1** — a soft edge at either means the conversion is wrong, and no renderer work after
this point is safe until it is right. The four-sample figures are not available here; they wait for the
resolve step and stay a standing obligation (`Plan/README.md`, measurement 5).

### M0.17 — The interface pass · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** ADR-011 in full; `Interface.md` §1.

**Adds:** the second pass — after the present blit, before `Present`, straight into the back buffer at
physical resolution, laid out in **authored coordinates carried through M0.12's transform**. At M0 it draws
one rectangle, which is enough to prove the render target bind, the pipeline state and the ordering
constraint ADR-011 names.

**No text yet.** The glyph atlas is M1.12: M0 has nothing to say, and
[`ADR-009`](../ADR/ADR-009-text-is-directwrite-into-an-atlas.md)'s atlas is sized against the window, which
is a rebuild path worth introducing beside the interface that needs it. **That deferral is the plan's and
not the design's** — `GameDesign.md` §10 requires the interface *pass* at M0 and says nothing about its
contents.

**Files:** `NeuronClient/InterfacePass.h` `.cpp`, `NeuronClient/Shaders/InterfaceVS.hlsl` and
`InterfacePS.hlsl`; `NeuronClient.vcxproj` + `.filters`;
`Tests/NeuronClientTests/InterfaceTransformTests.cpp`. **The shader is two files rather than the one
this line first proposed**, for the reason M0.15's line already records:
[`ADR-012`](../ADR/ADR-012-a-shader-is-compiled-into-a-header.md) fixed the layout as one file per
stage under `Shaders\`, compiled into a checked-in header under `CompiledShader\`.

**Done when:** an authored rectangle lands at the physically correct place at several window sizes — the
transform asserted by a test and the pixels confirmed by looking once — and **nothing in the pass branches
on the window size**, which is the intent R13 exists to protect and the thing ADR-011 promised to keep.

### M0.18 — The gesture seam · `NeuronClient` · `NeuronClientTests` · agent

**Read first:** R21; `Interface.md` §2 and §3; `README.md` F6.

**Adds:** the one path in. `PointerPressed`, `PointerMoved` and `PointerReleased` forwarded to a
`GestureRecognizer` with **`GestureSettings::DoubleTap` enabled** (ADR-017 — it arrives on `Tapped` with a
count, not as a fourth verb); **a `PointerPoint` whose `PointerDeviceType` is not `Touch` dropped at
exactly one site**; keyboard events not subscribed at all. The seam records the contact count at
`ManipulationStarted` and the manipulation keeps that meaning until it ends, so a thumb landing mid-drag
does not change what the drag is doing.

**Two things go in at this site and nowhere else** (`Interface.md` §1, §2):

- **Palm rejection.** A contact whose `ContactRect` exceeds **78 authored pixels — 14.9 mm** in either
  dimension is not a fingertip and never becomes an input record. The contact-count latch above only
  protects a gesture already running; a palm landing *first* starts one of its own, and on a 287 mm screen
  played on a desk that happens routinely.
- **The inertia gesture settings stay off** ([`ADR-018`](../ADR/ADR-018-the-camera-is-anchored-to-the-plane.md)).
  The action immediately after positioning this camera is a precise tap, and momentum fights it. Leaving
  them off is a decision rather than an omission, so do not enable them to "see what it feels like" without
  changing the ADR.
- **The tap slop, pinned at 16 authored pixels rather than inherited.** It is what separates a tap from a
  pan, and the asymmetry `Interface.md` §3 relies on — an accidental pan is free, an accidental move order
  is not — only holds if the number is right. **Begin the pan at the point the threshold was crossed**, so
  engaging it does not jump.

**Split it, or it cannot be tested** (F6). The half that touches WinRT turns each event into a plain input
record — a contact count, a translation, a scale, a rotation, a point — and the half that does arithmetic
takes only that record. Nothing in this tree can construct a `CoreWindow`, so the arithmetic has a suite
over it **only if it never sees a `PointerPoint`**, and R21 requires it to have one.

**Files:** `NeuronClient/GestureSeam.h` `.cpp`, `NeuronClient/InputEvent.h`,
`NeuronClient/GestureArithmetic.h` `.cpp`; `NeuronClient.vcxproj` + `.filters`;
`Tests/NeuronClientTests/GestureArithmeticTests.cpp`.

**Done when:** **the sign of a pinch and the sign of a rotation are pinned by tests** — R21 names these as
the things a package can hide and a test cannot; the rotation deadzone is pinned either side of its
threshold **and its latch is pinned**, so a manipulation that crosses back under eight degrees keeps
rotating rather than stuttering; the **2% scale deadzone** holds, so a pure orbit does not creep the zoom
and therefore the pitch; the 16-pixel tap slop is pinned either side, with the pan starting at the
crossing point; a contact wider than 78 authored pixels produces no input record; a manipulation that
began with two contacts still reports two when a third lands; and the non-touch drop is asserted at the
single site that performs it.

---

## The client

### M0.19 — The replica store and interpolation · `GameClient` · `GameClientTests` · agent

**Read first:** `TechnicalDesign.md` §6 and §4; ADR-003. **And `README.md` F4 and F9, both of which are
now corrected in §6 rather than outstanding against it.** The delay is 75 milliseconds — §4 and ADR-003
always said so, §6 said 150 until it was fixed, and all three call it "one snapshot interval plus a jitter
margin", which at 20 Hz is 75. The depth that delay implies is three snapshots and is computed rather than
quoted (F9).

**Adds:** the snapshots the delay reaches back over — **three** at 20 Hz, computed from the delay and the
interval rather than fixed, and not the two this line first said (`README.md` F9) — the interpolation
clock, and drawing 75 milliseconds behind the newest. Positions and headings interpolate and headings go the short way round, which the binary angle
makes a subtraction rather than a special case. If the next snapshot has not arrived, the client
extrapolates for a short bounded window and then **holds position rather than sliding a ship somewhere it
never was.**

**Files:** `GameClient/ReplicaStore.h` `.cpp`, `GameClient/Interpolation.h` `.cpp`; `GameClient.vcxproj` +
`.filters`; `Tests/GameClientTests/InterpolationTests.cpp`.

**Done when:** interpolation between two snapshots is pinned including the heading wrap-around; a missing
snapshot extrapolates and then holds, with the bound asserted rather than implied; an out-of-order arrival
does not move the clock backwards; and the 75 is one named constant rather than a literal in three places.

### M0.20 — The camera, minimally · `GameClient` · `GameClientTests` · agent

**Read first:** [`ADR-018`](../ADR/ADR-018-the-camera-is-anchored-to-the-plane.md) **before**
`Interface.md` §5 and [`ADR-001`](../ADR/ADR-001-the-playfield-is-a-plane.md) — ADR-001 settles the degrees
of freedom and §5 settles which gesture drives which, and ADR-018 is the mapping between them that neither
states.

**Adds:** a camera that looks at a focus point on the plane and never rolls, with pan, orbit and zoom,
pitch coupled to zoom, and the focus clamped to the play area plus a margin. **Floats are correct here** —
the renderer is not the simulation (R16). The part that must be exact is the inverse: a tap becomes a ray
and the ray meets the plane, which is where ADR-001 lands in code, is M0.21's input, **and is also the
anchor solve this camera is driven by.**

**Build the anchor solve, not a delta-accumulator.** At `ManipulationStarted` the ray through the contact
centroid meets the plane and that world point is kept for the life of the gesture. Each update: scale to a
new distance and hence a new pitch, rotation to a new heading, **then one solve** placing the focus so the
anchor lands under the current centroid at the *new* pose. **Do not also apply the recognizer's
translation** — it is already in the solve, and applying both is the defect that makes the camera
accelerate. One finger is the same solve with no scale and no rotation, which is why §3's "two fingers pan
identically to one" needs no separate code.

**ADR-001 states the budget exactly and it is smaller than "a 3D camera":** four degrees of freedom — a
focus point on the plane, a heading, and a distance — with **pitch derived from the distance rather than
separately controlled**. A fifth is not a feature to add later; it is this decision being reversed.

**Files:** `GameClient/Camera.h` `.cpp`; `GameClient.vcxproj` + `.filters`;
`Tests/GameClientTests/CameraTests.cpp`.

**Done when:** the transform is pinned at several pitches; a screen point maps to a plane point and back to
the same screen point within a stated tolerance; **the anchor solve's property holds — project the anchor
and it lands on the centroid** — for one contact and for two, with scale and rotation applied, and **with
no drift over a long synthetic gesture**, which is the failure this model actually has; the focus clamp
holds at the corners **and lets the anchor slip rather than fighting it**; the coupling of
pitch to zoom is monotonic at both ends of the range; and **the pitch floor holds** — `Interface.md` §5
pins a 40° vertical field of view and a 30° minimum pitch, which puts the top edge of the frame 10° below
horizontal and the horizon off screen. **Assert the stretch ratio**, 5.67 camera heights at the top edge
against 1.73 at the center: that ratio is what bounds tap error near the top of the frame and what bounds
ADR-010's wedge, and it grows without bound if the floor slips.

### M0.21 — The tap, the order and the local marker · `GameClient` · `GameClientTests` · agent

**Read first:** `Interface.md` §4; `OpenQuestions.md` Q20; `TechnicalDesign.md` §6's order-marker
paragraph; R19.

**Adds:** the verb. A tap on empty space with something selected becomes a move command and is sent at
once; **and the client draws a destination marker and a line from the selection the instant the gesture
resolves**, clearing it when a snapshot's `lastCommandSeqApplied` passes that command's sequence.

**The ray-plane intersection this step needs is the one M0.20 already built for the anchor solve**
(ADR-018), which is why the camera comes first: a tap is the same cast at a different moment.

**"What is under it" needs a radius and an order, and `Interface.md` §1 now states both**: a **24-pixel
pick radius**, nearest candidate inside it, with the tier order own ship → own station or module →
hostile → asteroid → empty space. A point hit test against a four-pixel silhouette is a coin flip, and the
failure is the expensive one — you miss the ship, hit empty space, and the selected fleet flies there.

**Nothing is predicted.** The entity does not move until the host says it did. R19 forbids the client
simulating, not the client drawing what it asked for, and holding that line precisely is the whole of this
step — it is also the step where a well-meaning optimization ("just move it locally, the host will agree")
breaks the architecture.

**Files:** `GameClient/OrderMarker.h` `.cpp`, `GameClient/TapOrder.h` `.cpp`; `GameClient.vcxproj` +
`.filters`; `Tests/GameClientTests/OrderMarkerTests.cpp`.

**Done when:** a marker appears on the resolving gesture, survives an unacknowledged round trip, and clears
on the acknowledgment that covers it — including the case where one later sequence clears two markers at
once; and **nothing in `GameClient` moves an entity.**

### M0.22 — The package · `OutpostCommander`, `GameClient` · hand · agent

**Read first:** ADR-008; `OpenQuestions.md` Q23; R18 and R20; `Interface.md` §1.

**Adds:** fullscreen at launch; `privateNetworkClientServer` in the manifest — the capability Microsoft
names for LAN games, which on Windows does **not** grant internet access; and the host address read from a
one-line file in `LocalState` with the compiled-in default behind it. `IFrameworkView` wires the
`CoreWindow` to the client's frame loop and holds nothing else (R20): one drain of the dispatcher per
frame, then the packet queue, then the clock, then render, then present.

**Files:** `OutpostCommander/App.cpp`, `OutpostCommander/Package.appxmanifest`;
`GameClient/ClientFrame.h` `.cpp`; `NeuronClient/HostAddress.h` `.cpp`; the project files and `.filters`
for both libraries.

**Done when:** the package deploys, launches fullscreen, reads the address from `LocalState` when the file
is present and falls back when it is not — **and the fallback is pinned by a test** even though the file
read itself is not reachable from a desktop test host.

### M0.23 — GATE: tap-to-visible latency, and the frame time · — · hand · **human**

**Read first:** `TechnicalDesign.md` §9.2 and §9.5; ADR-003's Measurements 2; ADR-007's Measurements 1.

**Adds:** nothing. On an actual Surface Pro with the host on another machine: timestamp the `Tapped` event
and the first frame in which the drawn position differs, over many taps, against §4's predicted **152 ms
average and 227 ms worst**. Then frame time **at both world scales** at one sample, **on x64 and on ARM64** — the
target device is a Snapdragon part, so ARM64 is the platform the game is for and the platform nothing
automated compiles.

**Done when:** both figures are measured, written into ADR-003's and ADR-007's Measurements sections with
how they were measured, and **§9's predictions are confirmed or corrected in the same pull request.**
`GameDesign.md` §10 calls tap-to-visible the number that decides how the game feels, and every other
decision in the design is cheap beside it.

---

## Leaving M0

**The milestone is finished when** the packaged client, fullscreen on a Surface Pro, draws one shape that a
host on another machine is simulating; a tap puts a marker down immediately and the shape arrives where it
was sent; all six suites carry real tests and none carries `SuiteSmoke`; and the three gates have written
their answers into the documents that predicted them.

### Where it actually stands, 2026-09-22

**Twenty-two of the twenty-three steps are built, and the milestone is not finished.** Two things are in
the way and they are different kinds of thing.

**The suites criterion is met.** All six carry real tests, `SuiteSmoke` is gone from the last one that had
it, and the six library-name placeholders it existed to assert went with it — every one of them said
*"delete it when the first real declaration lands"* and every library now has several. **All four
configuration pairs build clean under `/warnaserror`**, which is worth stating because CI compiles exactly
one of them: `Debug|x64`, `Release|x64`, `Debug|ARM64` and `Release|ARM64`, on 2026-09-22.

**What is in the way, first: a step that does not exist** (`README.md` F10). Nothing in M0 draws into the
scene target, so there is no shape for the gate to time and no drawn position for a tap to move. A step
between M0.21 and M0.22 is owed — one shape on the plane through M0.20's camera at M0.19's interpolated
position — and until it exists the two sentences above cannot both be true.

**And second: M0.23 is a human gate on hardware.** It needs a Surface Pro, a host on a second machine, and
an ARM64 leg that nothing automated compiles. It is not work that can be brought forward; it is the
milestone's whole purpose, and it is the owner's.

**M0.22 carries one piece of its own**, stated where it was left rather than implied: `IFrameworkView`
still runs M0.5's probe rather than the frame loop. `GameClient/ClientFrame` is written and has a suite —
drain, decode, fold, clock, clear acknowledged markers — and wiring it in is a wiring job the day there is
something for the render step to call. That day is the step F10 names.

**What M0 produces besides code:** ADR-012; measured figures replacing arithmetic in ADR-003 and ADR-007;
ADR-008 amended with which loopback form is actually needed; and four of the eight owed measurements in
`TechnicalDesign.md` §9 struck through — §9.1, §9.2, §9.4, §9.7, with §9.5 begun and standing.

**What can go wrong here, in the order it would hurt:** the loopback exemption needing `-is`, which ends
the one-machine loop and makes a second device a prerequisite rather than a convenience (M0.5); the fit
landing at 1.99, which invalidates R13's arrangement on the target device (M0.16); and tap-to-visible
measuring far above 152 ms, which is not fatal but is the number every interface decision in
`Interface.md` was taken against.

**Before M1**, the two pieces of standing work in [`README.md`](README.md) are worth doing: the project-file
check, whose value peaks exactly now that M0 has touched all eight project files and added a shader item
type, and the clang-tidy driver, while a first run is still fixable in an afternoon.
