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

### The naming self-check, because nothing runs it

`.clang-tidy` is configured and nothing drives it (`AGENTS.md` §1), so **the last thing before handing a
step back is reading your own diff against §1's table.** The five that get missed:

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

Eight things. **None is a criticism of the design and none is settled here** — each is a question the
design has not been asked, a decision that has to be taken while building, or, in one case, a sentence
that contradicts another sentence. In the order they will be met:

**F1 — There is no build path for shaders, and there is no shader anywhere in the tree.** R13's scaled
present is a full-screen blit and therefore the first HLSL in the project, at M0.15. R14 allows `fxc` and
`dxc` because the Windows SDK installs them, but nothing says whether a `.cso` is package content or
whether `/Fh` bakes a byte array into a header. It is not a detail: `NeuronClient` is a **static library
with no package of its own**, so a `.cso` on disk must be carried into `OutpostCommander`'s package by a
project the library cannot see, while a header has no such problem. **ADR-012 is now drafted** — `fxc`
through `FxCompile` to a `/Fh` header — and is **Proposed rather than Accepted**, so M0.15 still waits on
the owner's ruling rather than on the record existing.

**F2 — The protocol has no join, so a client cannot learn which player it is.** `GameDesign.md` §2
configures the slots on the host before the match starts and `TechnicalDesign.md` §4 specifies the
snapshot, the command and the heartbeat — but nothing tells an arriving client which of the per-player
blocks is *its own*, and nothing says what the host does when a second client appears.
`TechnicalDesign.md` §5's "the protocol version in the header refuses a mismatched build" implies a
handshake defined nowhere. M0 dodges it, having one entity and no ownership. **M1 cannot: selection,
ownership validation and the credits readout all need it. Needs ADR-013 at M1.4.**

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
documentation defect rather than a decision** — the plan builds to 75, and that sentence wants correcting
in a pull request of its own.

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
| 5 | Frame time on a Surface Pro, one sample and four, **x64 and ARM64** | **M0.23**, then standing | A standing obligation rather than a measurement: ARM64 is the target platform and CI compiles none of it. |
| 6 | The interface pass against the world pass | **M1.16** gate | Needs the glyph atlas and a populated interface, so it cannot be earlier. |
| 7 | **That the present step really takes the filter the scale calls for, and which world scale ships** | **M0.16** gate | By looking at it, at both scales, plus frame time at each on x64 and ARM64. R13's arrangement is worth nothing at a scale of 1.99 — or 0.999 ([`ADR-016`](../ADR/ADR-016-the-world-resolution-is-a-scale.md)). |
| 8 | Which loopback exemption form a UDP client needs | **M0.5** gate | Remove the exemption and try again, exactly as `AGENTS.md` §3 instructs. |

[`ADR-002`](../ADR/ADR-002-tick-and-numbers.md) owes a ninth that is not on that list and matters more than
most of it: **the determinism test passing on all four configuration and platform pairs.** The test lands
at M1.7; running it on four pairs is standing work below, because nothing in CI will ever do it.

## The ADRs this plan expects

`AGENTS.md` §6 requires an ADR in the same commit as the code that takes the decision. Three are visible
from here. **Numbering continues from ADR-011**, and a decision that turns out not to be needed simply
never gets written — a gap in the sequence is cheaper than an ADR nobody meant.

| | Decision | Owed at |
|---|---|---|
| **ADR-012** | How a shader is built and how it reaches the binary — `fxc` to a `/Fh` header, or a `.cso` as package content (F1) | **Drafted 2026-09-21, awaiting a ruling** |
| **ADR-013** | The join record: how a client is told which player it is, and what a host does with an unexpected one (F2) | M1.4 |
| **ADR-015** | The base is built from modules, and a module is a separate destroyable entity (`GameDesign.md` §5) | **Taken 2026-09-21** |
| **ADR-014** | The firing interval in ticks and where integer damage rounds (F3), once the register has said what it should be | M3.1 |

## Standing work, in no milestone

Two things `AGENTS.md` says are worth doing and are not done, plus one this plan adds. **None belongs to a
milestone, all three protect every milestone, and the plan proposes rather than schedules them.**

**The project-file check, before M0's project files start moving.** `AGENTS.md` §6 names it: a script that
reads the twelve project files and asserts §3's table — Debug and Release differing in exactly those rows
and nothing else, no `ConformanceMode` or `LanguageStandard` drift, `EnableEnhancedInstructionSet` stated
per platform, one pinned SDK, one pinned package version. Seconds rather than the minutes a second build
pair costs, and it closes the Release and ARM64 gaps cheaply. **M0 adds files to all eight projects and
introduces the first shader item type, which is exactly when that table drifts** — so the cheapest moment
is before M0.13, not after M4. A script in `Build/`, which R14 explicitly does not bind. **This is the one
piece of standing work an agent can do end to end on any platform**, since it reads XML and runs nothing.

**The clang-tidy driver, before the tree gets big.** `AGENTS.md` §1: the configuration is in the tree and
nothing runs it, so naming is review's problem. Review can carry a tree of eight files; it cannot carry the
several hundred this plan adds. The natural moment is **between M0 and M1** — large enough for the script
to be worth writing, small enough for its first run to be fixable in an afternoon.

**The determinism test on all four pairs, at every milestone boundary.** ADR-002's second owed
measurement, and the one thing `AGENTS.md` §6's CI scope guarantees nobody will notice: the property R16
exists to protect is precisely the one the pipeline does not watch. Four `msbuild` invocations and four
`vstest` runs comparing one state hash.

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
