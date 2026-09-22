# Implementation Plan — *Outpost Commander*

[`AGENTS.md`](../../AGENTS.md) says **how** code is written here and [`Design/`](../README.md) says **what**
is being built. This directory says **in what order, who does it, and what counts as finished.**

**Status: DRAFT, 2026-09-20.** Written against `Design/` at commit `2533e29` — eleven ADRs Accepted,
twenty-six register questions answered and one open. **If `git log 2533e29..HEAD -- Design/` shows anything,
read it before trusting a step**: the plan cites the design rather than copying it, but a citation can
still go stale.

**This plan takes no decisions.** It is ordering, and one rule holds it together: **it cites the design and
never restates it.** No number lives here. No mechanic is settled here. Where the plan met something the
design does not cover it says so, names the ADR or register question that has to answer it, and **does not
answer it by writing it down** — a fourth copy of a figure is the thing this tree has already been bitten
by once, and `AGENTS.md` §6 says so about itself.

**It is written to be executed one step at a time by an agent**, which is why each step carries what to
read before writing, every file it touches including the project files, and an exit criterion something can
check. §*Executing this plan with an agent* below is the protocol, and it is not optional reading: the
constraint that an agent **cannot build or test this repository anywhere but a Windows machine with Visual
Studio 2026** shapes what a step can honestly claim.

---

## The milestones

The five are [`GameDesign.md`](../GameDesign.md) §10's and they are not renamed, rescoped or reordered here.

| | | What it proves | Steps | Gates |
|---|---|---|---|---|
| [`M0`](M0-the-wire.md) | **The wire** | The tick, the packet, two socket stacks talking, the D3D12 frame, the two-pass renderer, the gesture seam — and whether a single-machine loop is usable at all | 23 | 3 |
| [`M1`](M1-the-fleet.md) | **The fleet** | The component model under two designs, selection, orders, the interface, two clients on one host | 16 | 2 |
| [`M2`](M2-the-field.md) | **The field** | The generator both sides run, the economy as a loop rather than a number, and a base built out of modules | 15 | 3 |
| [`M3`](M3-the-fight.md) | **The fight** | Combat, death, victory — and a match you can restart twenty times in an evening | 12 | 2 |
| [`M4`](M4-the-opponent.md) | **The opponent** | An AI worth the name, four slots, and the one question the MVP could not answer | 8 | 2 |

## Where it stands

**M0 is written and its code is verified on the device.** Twenty-three steps, one of which grew an
`M0.21b` the plan had left out (F10) — the wire, the numbers, the simulation, the host loop, the scaled
present, the gesture seam, the camera, the world draw and the packaged client. A tap on a Surface Pro
moves a ship that the host, not the client, decided had moved.

**M1'S CODE IS IN; ITS TWO GATES ARE NOT.** M1.1 to M1.14 are built, and M1.15 and M1.16 are a hand's.
What that adds up to is a match you can look at: two stations placed 12,000 units apart on a seed both
sides derive, a station that builds and refunds, fifty ships that take a ring slot each rather than
stacking, a camera that sticks to the finger and opens on your own base, a tap that selects, expands and
orders, three authored CMO hulls drawn as three instanced calls — **and now an interface over it**:
Segoe UI rasterized into an atlas this tree owns, every plate and glyph drawn as one instanced call, and
the credits, selection, build and system panels with the quit's two taps. On the device the atlas builds
in 3 to 17 ms and uses 269 of its 512 rows; the quit was armed and confirmed by hand. **Whether the text
reads is M1.16's**, and nobody has judged it yet.

**The code M1's gates need is in, and nothing an agent can do is left.** After M1.14, a lost link puts up
the reconnecting overlay and rejoins as the same player. The package also declares multiple instances,
with each instance keeping its own session token, which gives M1.15 a one-machine route to try. **That
route has run once, on the Surface Pro at its lock screen.** Two instances took two seats from one host
through one loopback exemption. Nobody has played on it, because a locked screen suspends both clients,
and whether two can run visibly side by side is the half still open. A failed socket no longer ends
the client either: it is reopened, and a lost link always gets a fresh one. Both gates are still open
and both are a hand's.

**Two closed gates stopped drawing over it.** M0.16's calibration cross and M0.17's probe rectangle were
still on the glass every frame; a closed gate's instrumentation is debris, and M1.9 is the first
milestone with something behind it worth seeing.

**M1.4 took the decision this plan said was not its to take** —
[`ADR-013`](../ADR/ADR-013-a-client-is-told-which-player-it-is.md) — and the code behind it: a client is
told which player it is, a reconnect gets its slot back, and a command from an endpoint the host never
seated is refused instead of believed.

**ADR-002's second owed measurement is closed.** M1.7's determinism test runs a two-minute scripted match
from one seed and hashes to `0x37f846ed90b74ca1` identically on Debug and Release, x64 and ARM64. Until it
ran, every claim in that record about bit-identical behavior was an argument.

**What is open is the geometry, and two measurements**:

| | |
|---|---|
| **M0.5** | One of its four runs is answered — loopback under the exemption Visual Studio grants, at zero loss and sub-millisecond jitter. **Three are open** and all three need two machines, admin rights or a real wireless link |
| **M0.23** | **Half closed, and the measured half has moved once.** Frame time was 1,118 microseconds over an empty frame at M0.23 and is **1,482 with M1.9's three hulls in it** — both in [`ADR-007`](../ADR/ADR-007-the-authored-frame-is-1440x960.md), and it is a standing figure rather than a settled one for exactly that reason. Tap-to-visible is measured **on loopback only** — 76 ms mean over nine taps, recorded in [`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md) as a stage rather than as the answer, because one machine is not the network §4's 152 ms predicts. **The two-machine run is owed** |
| **M1.4** | **Closed.** The ADR is Accepted and the join is built. **What it has not had is two machines** — every one of its properties is pinned by a socket-free suite, and a reconnect across a real relaunch is one of the things M0.23's outstanding run is now worth watching for |
| **M1.9** | **Built, and looked at once.** Three hulls read out of the package, converted, uploaded and drawn instanced, at **1,482 microseconds** a frame. **What the looking found was a bug**: the light rig was never converted out of the authored frame, so the key pointed nearly along the plane and every hull read as shapeless. Fixed. **The tactical-zoom silhouettes are still M2.13's**, and ADR-021's clean-install check is still owed — the deploy used here was a loose-file registration on the machine that built it, which is the one arrangement in which a missing payload cannot show |
| **M1.9b** | **Built, looked at twice, and the band withdrawn.** The blackbody table is pinned at all eight stops and between them, the seeded field gives the same sky twice, and the ceiling is asserted rather than intended — 38 tests across `NeuronClientTests` and `GameClientTests`. **The asserted half found the design's own arithmetic wrong**: 1:3:9:27:81:243 sums to 364 and divides no round number of stars evenly, so the division three documents called exact was neither exact nor what the code computed. **The looked-at half found the sky wrong twice**: first a band outshining invisible stars, then — dimmed — a band whose noise read as a painting behind the fleet while the faint stars delivered about 16 of 255 to a pixel. [`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md) now withdraws the band: the sky is 8,000 stars and nothing else, the tiers are 22, 66, 198, 593, 1,780 and 5,341, the faint end is 3.0 pixels at 0.24 under a flat-topped falloff, and the clear is black. **Built, measured and CONFIRMED on the device at the third look**: 8,000 stars at 7,992 distinct sizes, 3.00–9.96 pixels, lit area 0.140% of the frame. It reads as a sky, which closes the third of ADR-019's owed measurements ahead of M1.16 and leaves two. The withdrawal commit could not run `dxc`, so its `StarPS.hlsl` and the checked-in DXIL disagreed until the build here regenerated it |

**M0.16 is CLOSED** — the filter was confirmed by eye on the device at both scales, the frame times are in
[`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md)'s Measurements, and **the scale that ships
is 1:1**, which is the decision that gate existed to take. The one thing it could not answer — whether
1:1 at one sample beats 0.5 at four — waits on a resolve step that does not exist, and ADR-016 carries it.

**Two of these steps were written before anything in this tree compiled** — M0.17 and M0.18 — and both
went in behind M0.16 rather than ahead of it, because the measurement that would have invalidated renderer
work came out right first. Both are now built and both have been looked at.

**Eight register rows came from building rather than from reviewing**, which is the kind this plan
expected fewest of. `OpenQuestions.md` Q38 came out of M0.18 — whether the rotation deadzone rebases at
its crossing the way the tap slop does; it does, and `Interface.md` §5 now says so. It was found the way
those are: two paragraphs described the same kind of threshold and only one of them said what happens at
the crossing. **Q39 to Q45 all came out of one afternoon integrating the mesh handoff**, and the register
says why that is the expected shape rather than a failure of review.

**The ordering inside a milestone is by risk, not by feature**, which is the principle `GameDesign.md` §10
orders the milestones by. Concretely: the transport goes in before the numbers it will carry and the
renderer goes in after both, because a wrong answer about the loopback exemption invalidates a fortnight of
renderer work and a wrong answer about the renderer invalidates nothing.

## How to read a step

Every step is one block with a header line — **`M<n>.<m>` — name · project · suite · who** — and four
fields under it:

| | |
|---|---|
| **Read first** | The design sections that decide this step. An agent reads these before writing, and a conflict between them and this plan means **the plan is wrong**. |
| **Adds** | What lands. Prose, because a step is an idea rather than a list. |
| **Files** | Every file, including the `.vcxproj`/`.vcxitems` and the `.filters` that must be edited with it. **Names are proposals** that satisfy `AGENTS.md` §1 and `TechnicalDesign.md` §1's two naming traps; rename freely, but keep the traps. |
| **Done when** | The exit criterion. Split where it needs splitting into what an agent can establish and what needs a Windows machine, a device or a pair of eyes. |

**Who** is `agent`, `human`, or `both`. A step marked **gate** is not code at all — it is a measurement, a
confirmation against hardware, or a question that must be answered before the next step is written.
**An agent never marks a gate done.** Every gate exists because the design says the answer cannot be
obtained by argument, and skipping one is the plan failing rather than running ahead.

---

## Executing this plan with an agent

### The constraint, first

This tree is MSBuild, MSVC `v145`, the Windows SDK and `vstest.console.exe` (`AGENTS.md` §3). **An agent
running anywhere else — a Linux container, a web session, a Mac — cannot build this repository, cannot run
a test, and cannot deploy the package.** What it can still do is write the code, edit the project files
correctly, and run the format check, which is `pip install clang-format` and platform-independent.

That is not a reason to refuse the work; it is a reason to be exact about the claim. `AGENTS.md` §3:
*"Report what you actually did. 'Builds clean, not run' and 'builds and runs' are different claims."*
**An agent that could not build says that in its report, in those words**, and does not imply a green
build by staying quiet about it. Every step's **Done when** is written so the unverifiable half is
visible rather than assumed.

### Before writing a line

1. **[`AGENTS.md`](../../AGENTS.md), the whole file.** It overrides habits from every other codebase, and
   §1's naming table is checked by nothing.
2. **[`Design/README.md`](../README.md)**, then the sections the step's *Read first* names.
3. **§*What planning found*** below, so that the known gaps are known before one is rediscovered as a bug.
4. **The milestone file, and the step.**

### One step, one commit

`AGENTS.md` §6 asks for one change per pull request, and a step is sized to be that. A step that turns out
to be three is three commits, which is ordinary; a commit that quietly does two steps is the thing that
cannot be bisected later. Branch, commit with an imperative subject describing the change, and do not
commit build output.

### What an agent must not do

Each of these is `AGENTS.md` speaking, cited so the reason is one click away rather than a rule to trust:

- **Do not lower the toolset, the language standard, `/permissive-` or `/W4`, and do not silence a warning
  with a pragma** (§3, §4, §7). A build error that tempts you to is a thing to **report**, not to route
  around.
- **Do not add a NuGet package or any third-party code** (R14). Propose it in the report with what it buys
  and what it costs.
- **Do not reformat a line the step did not touch** (§4). The tree is formatted and CI keeps it that way,
  so a drive-by reformat is pure churn that buries the real change.
- **Do not create a file without adding it to the owning `.vcxproj` or `.vcxitems` and its `.filters`**
  (§2, §7). A file that compiles locally and is missing from the project fails only in CI.
- **Do not add a `Source Files` or `Header Files` filter** (§2), and do not put a header in a
  subdirectory — `.clang-tidy`'s `HeaderFilterRegex` matches one level in, so a nested header is silently
  unchecked the day that gate is switched on.
- **Do not delete a suite's `SuiteSmoke`** unless the same commit puts a real test in that suite (§3).
- **Do not name a header like a CRT or SDK header, and do not spell an identifier like a Windows SDK
  macro** (§2). `FixedPoint.h` and never `Math.h`; `Vec2.h` and never `Vector.h`; and `small`, `near`,
  `far`, `IN`, `OUT`, `DELETE` and `INTERFACE` are already taken.
- **Do not invent a rule number, an ADR or a register answer** (R25, §6). R25 and up are reserved, and a
  question the design has not answered is asked rather than assumed.
- **Do not mark a gate done.**

### The naming self-check, because the driver reports rather than gates

`.clang-tidy` now has a driver — `Scripts/RunClangTidy.ps1` — but it reports and does not gate
(`AGENTS.md` §1), its output arrives buried in the SDK-header warnings it analyzes but does not report
on, and **since 2026-09-22 it does not run on your push at all** — it is a weekly job. So **the last
thing before handing a step back is still reading your own diff against §1's table**, and a green build
is not the second opinion it looks like. The five that get missed:

`_camelCase` on every parameter · `m_camelCase` on private class state and plain `camelCase` on a public
aggregate's fields · `UPPER_CASE` for a `constexpr` but `PascalCase` for an enumerator · no `I`, `C`, `E`,
`Base`, `Abstract` or `Impl` affix on any type · one spelling per family, and it is the SDK's — `color`,
`initialize`, `normalize`, `behavior`, `center`. **Prose and identifiers both spell `color`** — R11 covered only identifiers until 2026-09-21
(R11), and both appear in the same file all through this tree.

### What a step's report says

Six lines, and `AGENTS.md` §7's last checkbox asks for most of them anyway: **which step**; **what was
added, by file, including the project files**; **which suite and which test names now pin it**; **what was
actually verified and on what machine** — naming the configurations built, or saying plainly that none
was; **any rule bent, and why**; **any question this raised**, which goes to the register rather than into
the code as an assumption.

---

## The critical path

Most of this plan is wide rather than deep — the two transports, the numbers and the renderer have no
dependency on each other. What is actually serial:

```
M0.1 bytes ─► M0.2 header ─► M0.3 host socket ─┐
                                               ├─► M0.5 GATE: a datagram crosses, on the device
                             M0.4 client socket ┘          │
                                                           ▼
   M0.6 numbers ─► M0.8 the tick ─► M0.9 the snapshot ─► M0.10 commands ─► M0.11 the host runs
                                                           │
   M0.12 the fits ─► M0.13 device ─► M0.15 scene target ─► M0.16 GATE: the filter, and which world scale
                                                           │
                                   M0.18 the seam ─► M0.19 interpolation ─► M0.21 tap to order
                                                           │
                                                           ▼
                                               M0.23 GATE: tap-to-visible latency
                                                           │
   M1.2 derived stats ─► M1.3 the entity ─► M1.4 ADR-013: the join ─► the rest of M1
                                                           │
   M2.0 GATE: Q26 ─► M2.1 the generator ─► the rest of M2 ─► M3 ─► M4
```

**Three gates sit on it and two are in the first week.** M0.5 is the one that can end the single-machine
development loop ([`ADR-008`](../ADR/ADR-008-the-host-address-is-configuration.md)); M0.16 is the one that
can invalidate R13's whole arrangement ([`ADR-007`](../ADR/ADR-007-the-authored-frame-is-1440x960.md)) and
the one that settles the world's resolution, which
[`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md) defaults to 1:1 on a judgment rather than a
measurement.
Both are cheap, and both are why M0 is ordered the way it is.

---

## What planning found

Ten things. **None is a criticism of the design and none is settled here** — each is a question the
design has not been asked, a decision that has to be taken while building, or, in three cases, a sentence
that contradicts another sentence. In the order they will be met:

**F1 — There is no build path for shaders, and there is no shader anywhere in the tree.** R13's scaled
present is a full-screen blit and therefore the first HLSL in the project, at M0.15. R14 allows `fxc` and
`dxc` because the Windows SDK installs them, but nothing says whether a `.cso` is package content or
whether `/Fh` bakes a byte array into a header. It is not a detail: `NeuronClient` is a **static library
with no package of its own**, so a `.cso` on disk must be carried into `OutpostCommander`'s package by a
project the library cannot see, while a header has no such problem. **Answered: [`ADR-012`](../ADR/ADR-012-a-shader-is-compiled-into-a-header.md), Accepted 2026-09-21.**
The header won; the compiler did not. The ruling took **Shader Model 6.7**, and therefore `dxc` rather
than the `fxc` this paragraph assumed — `FxCompile` dispatches to it natively, so the Visual Studio setup
still drives the build with no custom step.

**F2 — The protocol has no join, so a client cannot learn which player it is.** `GameDesign.md` §2
configures the slots on the host before the match starts and `TechnicalDesign.md` §4 specifies the
snapshot, the command and the heartbeat — but nothing tells an arriving client which of the per-player
blocks is *its own*, and nothing says what the host does when a second client appears.
`TechnicalDesign.md` §5's "the protocol version in the header refuses a mismatched build" implies a
handshake defined nowhere. M0 dodges it, having one entity and no ownership. **M1 cannot: selection,
ownership validation and the credits readout all need it. Answered:
[`ADR-013`](../ADR/ADR-013-a-client-is-told-which-player-it-is.md), Accepted 2026-09-22.** The host
assigns the slot and a client does not choose; a returning client is recognized by a session token the
host issued; and the record carries **the match seed**, which R23 had already made necessary and which
this finding did not ask for.

**F3 — The damage table is continuous and the tick is discrete, and the conversion is unspecified.**
`GameDesign.md` §7 gives `MassDriver` 25 damage **per second**;
[`ADR-004`](../ADR/ADR-004-weapons-resolve-at-the-fire-tick.md) applies damage **on the tick a weapon
fires**, "roughly once a second". Two things follow that nobody has chosen: **the firing interval in
ticks**, and **where the integer division lands**. `25 × 70 ÷ 100` is 17.5, and 17 per shot at 1 Hz is not
the 17.5 a second §7's table was computed from; fire every tick instead and `25 ÷ 20` truncates to 1,
which is 20 damage a second rather than 25 — **a 20% error that arrives silently.** §7's raid arithmetic is
the most carefully argued part of the design and this is the step that can quietly invalidate it. **A
register question, needed by M3**: it moves balance, so it is the design's to answer and not the plan's.

**F4 — `TechnicalDesign.md` §6 says the client draws 150 milliseconds behind; §4 and
[`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md) say 75.** Both call it "one snapshot interval
plus a jitter margin", which at 20 Hz is 75. The 150 is the 10 Hz figure surviving the rate change. **A
documentation defect rather than a decision** — the plan builds to 75. **CORRECTED**: §6 says 75, and the
only 150s left in it are the 10 Hz row of the latency table, where the figure is history rather than a
claim. **This entry went on saying §6 was wrong after it had been fixed**, which is the same defect one
level up and is why F9 above records its own correction rather than leaving the next reader to check.

**F5 — M1 wants two clients on one host and a packaged application is single-instanced.** A second
instance of `OutpostCommander` on one machine is a manifest declaration (`SupportsMultipleInstances`, in
the `desktop4`/`iot2` namespace — confirm the current form before relying on it), not a code change, and it
interacts with [`ADR-008`](../ADR/ADR-008-the-host-address-is-configuration.md)'s loopback exemption, which
is already strained. The alternative is a second machine, which is not an engineering decision at all. **A
register question, needed by M1**, because one of its answers is "buy a tablet".

**F6 — Nothing in this tree can construct a `CoreWindow`, so the seam must be split for testability.** The
two client suites are desktop test DLLs (`AGENTS.md` §3) and a test host has no `CoreWindow`. R21 and
`Interface.md` §2 require the gesture arithmetic to have a suite over it, and it can only have one if the
tested half takes **plain values** — a contact count, a translation, a scale, a rotation — and never a
`PointerPoint`. That shape is forced at M0.18 and it is far easier to write than to retrofit.

**F7 — The wire format is specified as totals, not as fields.**
[`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md) gives a ten-byte record and a thirty-byte
header and names what is in them, but not every width — appropriate for a design and insufficient for an
encoder. **M0.9 is where the widths become facts**, and the test that measures the encoded size is what
turns ADR-003's snapshot size from arithmetic into a measurement.

**F9 — `TechnicalDesign.md` §6 held two snapshots and drew 75 milliseconds behind, and those two clauses
could not both be true. CORRECTED 2026-09-22**, in §6 and in M0.19's own line. Two snapshots span one interval, 50 ms at 20 Hz, ending at the newest; a
render time 75 ms behind the newest is 25 ms **older than the older of the two**, so the pair does not
contain the frame being drawn. It is F4's defect one clause further on — the delay moved from 150 to 75
and the depth it implies was never recomputed — and it was never right at 10 Hz either, where 150 behind
the newest sits outside a 100 ms pair by the same margin.
[`ADR-003`](../ADR/ADR-003-replication-is-full-snapshots.md) reads the other way and is the one to trust:
"a lost snapshot is a 50-millisecond gap inside a 75-millisecond buffer, covered without extrapolating"
describes a buffer holding more than one interval of history. **A documentation defect rather than a
decision**, like F4, and M0.19 builds to the arithmetic rather than to the sentence: the retained depth is
computed from the delay and the interval, which is three snapshots at the current pair and becomes four by
itself if either figure moves. The sentence is corrected to the arithmetic rather than to a
number, so §6 now names no depth of its own to drift: `GameClient/ReplicaStore.h` computes it and §6 says
so.

**F10 — M0's headline needs a step M0 does not have: nothing ever draws into the scene target.** The
milestone opens with *"the host simulates one moving entity and the packaged client draws it, and a tap
moves it"* and closes with *"the packaged client, fullscreen on a Surface Pro, draws one shape that a host
on another machine is simulating"*. **No step between those two sentences puts a pixel of the world on the
screen.** M0.13 makes the device and the swap chain, M0.15 makes the scene target and blits it to the back
buffer, M0.17 draws one rectangle *in the interface pass at physical resolution* — and M0.19 to M0.21 build
the replica store, the camera and the tap without a consumer for any of them. The scene target is created,
cleared, and presented empty.

**It is not a gap in the design**, which is why this is a finding and not a question: `TechnicalDesign.md`
§6 describes the world pass in detail and M1.9 authors the meshes it draws. What is missing is a **plan
step** between M0.21 and M0.22 — call it the world draw — that puts one shape on the plane through M0.20's
camera, at the interpolated position M0.19 produces. It needs a vertex and index buffer, a pipeline state,
a constant buffer for the transform, and a shader pair, all of which M0.15 and M0.17 have already
established the shape of.

**M0.23 cannot run without it.** That gate times *"the first frame in which the drawn position differs"*,
and there is no drawn position. The measurement the whole milestone exists to obtain is the thing the step
list forgot to make possible.

**F8 — M0 is about half the engineering in the MVP, and it is the milestone labeled "no game at all".**
Twenty-three steps against M2's fifteen, touching all eight projects, containing every subsystem that can
turn out to be impossible. That is the correct shape for a risk-ordered plan, and it is said here because
"M0: the wire" reads like a week and is not one.

---

## The measurements, and where each is taken

[`TechnicalDesign.md`](../TechnicalDesign.md) §9 owes eight figures and assigns three to M0. This plan
assigns all eight to a step, which is the whole of its contribution to them:

| §9 | The figure | Taken at | How |
|---|---|---|---|
| 1 | The snapshot's real size at 110 entities and at 220 | **M0.9** | A `GameCoreTests` test encodes synthetic entities and writes the byte count. It needs no game. |
| 2 | **Tap-to-visible latency on real hardware** | **M0.23** gate | Timestamp the `Tapped` event and the first frame whose drawn position differs, against §4's predicted 152 ms. |
| 3 | The tick's cost at 110 entities | **M2.10** gate | The first milestone with enough entities and enough per-tick work for the number to mean anything. |
| 4 | Packet loss and jitter on a real wireless link | **M0.5** gate | A fixed-rate dummy stream with sequence numbers, before there is anything to put in it. |
| 5 | Frame time on a Surface Pro, one sample and four, **x64 and ARM64** | **M0.23**, then standing | A standing obligation rather than a measurement: ARM64 is the target platform and CI compiles none of it. **Taken twice so far** — 777 microseconds over an empty frame at M0.16 and **1,482 with M1.9's hulls** — and the four-sample half still waits on a resolve step that does not exist. |
| 6 | The interface pass against the world pass | **M1.16** gate | Needs the glyph atlas and a populated interface, so it cannot be earlier. |
| 7 | ~~**That the present step really takes the filter the scale calls for, and which world scale ships**~~ **TAKEN** | **M0.16** gate, closed | Looked at on the device at both scales, plus frame time at each on x64 and ARM64. The filter is right at both and **1:1 ships** ([`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md)). |
| 8 | Which loopback exemption form a UDP client needs | **M0.5** gate | Remove the exemption and try again, exactly as `AGENTS.md` §3 instructs. |

**Q44 owed a ninth and M1.9 discharged it**: what the meshes cost **in the appx**, which is the only one
of `TechnicalDesign.md` §7's three sizes that means anything for package size. **52 KiB deflated against
430 on disk** — 0.8% of the 6.3 MB [`ADR-019`](../ADR/ADR-019-the-sky-is-generated-from-the-seed.md)
saved by not shipping a painted cubemap, which is what says that geometry is not where package size
lives.

[`ADR-002`](../ADR/ADR-002-tick-and-numbers.md) owes a tenth that is not on that list and matters more
than most of it: **the determinism test passing on all four configuration and platform pairs.**
**DISCHARGED at M1.7** — a two-minute scripted match from one seed hashes to `0x37f846ed90b74ca1`
identically on Debug and Release, x64 and ARM64. Running it again after anything touches the simulation is
standing work below, because nothing in CI will ever do it.

## The ADRs this plan expects

`AGENTS.md` §6 requires an ADR in the same commit as the code that takes the decision. Three are visible
from here. **Numbering continues from ADR-011**, and a decision that turns out not to be needed simply
never gets written — a gap in the sequence is cheaper than an ADR nobody meant.

| | Decision | Owed at |
|---|---|---|
| **ADR-012** | How a shader is built and how it reaches the binary — `dxc` at Shader Model 6.7 to a checked-in header, not a `.cso` (F1) | **Taken 2026-09-21** |
| **ADR-013** | The join record: how a client is told which player it is, and what a host does with an unexpected one (F2) | M1.4 |
| **ADR-015** | The base is built from modules, and a module is a separate destroyable entity (`GameDesign.md` §5) | **Taken 2026-09-21** |
| **ADR-014** | The firing interval in ticks and where integer damage rounds (F3), once the register has said what it should be | M3.1 |

## Standing work, in no milestone

Two things `AGENTS.md` says are worth doing, plus one this plan adds. **None belongs to a milestone, all
three protect every milestone, and the plan proposes rather than schedules them.** Two are now done, and
are kept here with what they actually became rather than deleted — the third is what is left.

**The project-file check, before M0's project files start moving. DONE** — `Scripts/CheckProjectFiles.py`,
a gate in the `static checks` job. `AGENTS.md` §6 names it: a script that
reads the twelve project files and asserts §3's table — Debug and Release differing in exactly those rows
and nothing else, no `ConformanceMode` or `LanguageStandard` drift, `EnableEnhancedInstructionSet` stated
per platform, one pinned SDK, one pinned package version. Seconds rather than the minutes a second build
pair costs, and it closes the Release and ARM64 gaps cheaply. **M0 adds files to all eight projects and
introduces the first shader item type, which is exactly when that table drifts** — so the cheapest moment
is before M0.13, not after M4. A script in `Build/`, which R14 explicitly does not bind. **This is the one
piece of standing work an agent can do end to end on any platform**, since it reads XML and runs nothing.

**The clang-tidy driver, before the tree gets big. WRITTEN, NOT FINISHED** — `Scripts/RunClangTidy.ps1`
drives `.clang-tidy` and CI reports its output **weekly, in its own job, rather than on a push** — so naming
is still review's problem (`AGENTS.md` §1).
Review can carry a tree of eight files; it cannot carry the several hundred this plan adds, and a report
nobody reads carries none of them. **What is left is the noise**: `HeaderFilterRegex` limits what is
reported and not what is analyzed, so each run walks the Windows SDK headers, which is both why it is
slow and why its findings are hard to see. Quiet it and it can run with `-Gate`, which is the point.

**The determinism test on all four pairs, at every milestone boundary. THE TEST EXISTS AND HAS RUN
ONCE** — M1.7, `0x37f846ed90b74ca1`, identical on Debug and Release, x64 and ARM64 — **and running it
again is the standing part.** ADR-002's second owed measurement is discharged; the obligation it leaves
behind is not. This is the one thing `AGENTS.md` §6's CI scope guarantees nobody will notice: the
property R16 exists to protect is precisely the one the pipeline does not watch. Four `msbuild`
invocations and four `vstest` runs comparing one state hash.

**A change that moves the hash is either deliberate or it is a desynchronisation**, and the test says
which by failing rather than by being run.

## What is deliberately not planned

**Everything after M4.** `GameDesign.md` §10 names the order — the designer screen, research, fog of war
and the interest set, delta replication, formations, a mobile mothership — and says why each waits: it is
worth doing against something running. A plan for them now would be a plan against a guess, and
`GameDesign.md` §9 is explicit that what research unlocks is not settled.

**Audio, campaign, art direction, content files, mods, replays and saved matches**, for the same reason and
by the same list.

**Estimates.** There are none here and there will not be: one developer, a hobby project, and
`AGENTS.md` §6 requires a figure to be measured before it is quoted. A step count is a size, not a date.

## How this plan changes

- **A milestone's content changes in `GameDesign.md` §10**, and the plan follows. The plan never redefines
  a milestone, and a disagreement between the two is the plan being wrong.
- **A step changes freely**, in the pull request that implements it. Splitting one, reordering two that do
  not block each other, or finding that a step is three — all ordinary, and none needs ceremony.
- **A gate does not move without a reason written down.** Each exists because the design says the answer
  cannot be obtained by argument; moving one later is a decision to find out later.
- **A gap found here goes to the register or to an ADR**, never into this directory as an answer.
  §*What planning found* is a list of things that need deciding elsewhere, and it shrinks by things being
  decided elsewhere.
