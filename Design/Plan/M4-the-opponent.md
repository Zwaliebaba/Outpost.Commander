# M4 — The opponent, and the other two slots

[`GameDesign.md`](../GameDesign.md) §10: the AI of §8; three and four players; the `Cruiser` reinstated as
a design with its weapon and its damage row — **the milestone that can finally answer whether speed
counters mass.**

**What it proves is that the design's claims about scale were true.** Three of them have been standing
since the first draft and none has ever been executed: that the third and fourth slots are **a runtime
value rather than a change**, that reinstating a heavy design is **a table row**, and that a four-player
snapshot fragments into two datagrams that reassemble correctly. If any is false, it is false here.

**Read [`README.md`](README.md) first.** Eight steps, two gates. This milestone is deliberately at lower
resolution than M0 to M3 — `GameDesign.md` §9 is explicit that what comes after the MVP is half-designed on
purpose, and a plan written against a guess is worse than no plan.

**Entry state.** M3 complete: a playable match, a stub AI, twenty evenings' worth of balance data, and
numbers in `GameDesign.md` §7 that have met contact.

---

### M4.1 — The player count as a runtime value · `GameCore`, `GameLogic` · both · agent

**Read first:** `GameDesign.md` §2 and §10; `OpenQuestions.md` Q27; ADR-003's header.

**Adds:** three and four slots. **Q27's claim is that this is configuration rather than a change** — the
generator's symmetry (M2.2), the AI count and the snapshot's per-player blocks are all already sized by a
runtime player count, so this step should be small. **If it is not small, that is the finding**, and it is
worth reporting as one rather than absorbing.

**M1.14c has already done most of this step** ([`ADR-023`](../ADR/ADR-023-the-player-count-is-configurable.md)).
The count is a host argument and the arrays are sized at `Begin`. What is left is running a real match at
three and four. **The 46-byte header below is history**: since
[`ADR-024`](../ADR/ADR-024-replication-is-prioritized-records.md) the header is twenty-one bytes at any
player count, and the criterion becomes that the four-player update measures the same as the two-player one.

**Files:** `GameLogic/Match.cpp`, `GameCore/Snapshot.cpp`; tests in both suites extended.

**Done when:** a match runs at two, three and four players with no format change and no branch on the
count; ADR-003's 46-byte four-player header is reproduced by the encoder; and the generator's 90° symmetry
(already pinned by M2.2) is what places the extra starts.

### M4.2 — Fragmentation and reassembly · `NeuronCore` · `NeuronCoreTests` · agent

**Read first:** ADR-003's fragment paragraph; `TechnicalDesign.md` §4 and §8; M0.2.

**Adds:** the path M0.2 reserved header fields for and deliberately did not write. **Fragments reassemble
all-or-nothing** under one sequence number with an index and a count, and an incomplete set is discarded.

**The reassembler holds partial sets for the two most recent sequences, not one**, and ADR-003 says exactly
why: with a single slot, any cross-snapshot reorder discards a snapshot whose fragments had all arrived.
That is the bug this step exists to not have.

**SUPERSEDED BY ADR-024 ON 2026-09-23 AND NOT TO BE BUILT.** There is no fragmentation in the transport
any more: the fragment fields left the packet header at M1.14c, and a datagram that would not fit is a
datagram the accumulator does not send. The step is kept so the numbering and this milestone's count stand;
its text below is what it was for.

**Files:** `NeuronCore/Reassembler.h` `.cpp`; `NeuronCore.vcxitems` + `.filters`;
`Tests/NeuronCoreTests/ReassemblerTests.cpp`.

**Done when:** `TechnicalDesign.md` §8's requirement is met — **fragmentation and reassembly including a
lost fragment and a duplicate** — plus the case the two-slot rule exists for: fragments of sequence *n+1*
arriving between fragments of *n*, with both delivered.

### M4.3 — The four-player snapshot · `GameCore` · `GameCoreTests` · agent

**Read first:** ADR-003's cost table; `TechnicalDesign.md` §4; M0.9's measurement.

**Adds:** nothing structural. Under [`ADR-024`](../ADR/ADR-024-replication-is-prioritized-records.md) a
four-player match sends the same 1,232-byte updates a two-player one does; what changes is the refresh
interval, which the budget puts at every entity within two ticks at the cap of two updates. This step
**measures that on the wire** with four real clients, and it is where Q49's weights get their first look
with a full field on screen.

**Done when:** the refresh interval per entity is measured over a four-player match at the design's fleet
and matches the budget's prediction; loss is measured and is packet loss, not a multiple of it; and Q49
is either confirmed at its recommendation or moved, with the figure written beside the constant.

### M4.4 — The `Cruiser` reinstated · `GameCore` · `GameCoreTests` · agent

**Read first:** [`ADR-006`](../ADR/ADR-006-a-ship-is-a-composition.md); `GameDesign.md` §6 and §10.

**Adds:** **a table row, and ADR-006 exists to make that claim.** The hull has been in the catalog since
M1.1 and its derived stats have been pinned since M1.2 precisely so that this step is a design row, its
weapon component, and one row in the damage table.

**If this turns out not to be a table row, ADR-006's central claim is wrong** and that is worth saying
loudly — it is the one place in the whole plan where a milestone is structured to falsify a decision rather
than to implement one.

**Files:** `GameCore/Catalog.cpp`, `GameCore/Design.cpp`, `GameCore/DamageTable.cpp`; the three test files
extended.

**Done when:** the heavy design builds, moves at the speed its mass implies without anybody writing that
speed down, and takes its damage row; and **the diff is a table row plus tests** — if it is more, say what
it was.

### M4.4b — The research station module · `GameCore`, `GameLogic` · both · agent

**Read first:** `GameDesign.md` §9 and §5's module table;
[`ADR-015`](../ADR/ADR-015-the-base-is-built-from-modules.md) on why this module waited.

**Adds:** `ResearchStationL1` as a third module, and with it the thing that made it wait — research has a
place on the map an enemy can take away, rather than being a menu. The shipyard's levels start gating
**hulls and the designer** here, which is the effect they were always described as having and which the
MVP could not exercise with two designs.

**Files:** `GameCore/Catalog.h`, `GameLogic/Research.cpp`;
`Tests/GameCoreTests/DesignStatsTests.cpp` and `Tests/GameLogicTests/ModuleEffectTests.cpp` extended.

**Done when:** a component is unavailable until its research completes, that gate is one predicate over a
component identity as R24 says it should be, and destroying the research station stops research in
progress.

---

### M4.5 — The AI of §8 · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §8; R16; M3.10's stub.

**Adds:** the small state machine §8 describes and no more: **keep a target number of miners alive and
mining, build military with the surplus, and send it at the nearest enemy station when it has enough.**
§8 sets the ambition deliberately low — it exists so the game is playable and testable by one person, not
so it is a worthy opponent. **Difficulty levels, personalities and anything resembling strategic planning
are post-MVP**, and adding one here is scope this milestone does not have.

Same constraints as the stub: the same command path, no privileged information, no discount, R16 in full.

**Files:** `GameLogic/Ai.h` `.cpp` and its states; `GameLogic.vcxproj` + `.filters`;
`Tests/GameLogicTests/AiTests.cpp`.

**Done when:** the AI holds a miner count against losses, expands to contested fields, and attacks with
something rather than with everything; it appears in the determinism test; and a four-AI match — which
`GameDesign.md` §2 calls a test fixture rather than a game, and worth having for exactly that — runs
unattended to a victory.

### M4.6 — An AI takes an abandoned slot · `GameLogic` · `GameLogicTests` · agent

**Read first:** `GameDesign.md` §2; `OpenQuestions.md` Q9; `Interface.md` §7.

**Adds:** what Q9 deferred to this milestone. A disconnected player's ships currently sit on the board as
free kills — `GameDesign.md` §2 accepts that rather than solving it and says **an AI taking the slot is the
better answer and it waits for M4, when there is an AI that can start from arbitrary mid-match state.**

That last clause is the whole difficulty and is worth reading twice: M4.5's AI must be able to begin from a
position it did not build. **If it cannot, this step is where that is discovered**, and the honest outcome
is a register question rather than a special case.

**Done when:** a client disconnecting mid-match hands its slot to the AI without a tick's pause; the player
reconnecting takes it back; and the AI's first decision from an inherited position is tested from a
scripted mid-match state rather than from a fresh one.

---

## The gates

### M4.7 — GATE: does speed counter mass · — · hand · **human**

**Read first:** `OpenQuestions.md` Q14; `GameDesign.md` §7 and §6; ADR-004.

**The question the MVP was built to ask and could not.** `GameDesign.md` §7 claims the counter to a heavier
ship is speed — a `Frigate` moves at 140 units per second and a `Cruiser` at 50, so strike craft pick the
fight and leave — and
[`ADR-004`](../ADR/ADR-004-weapons-resolve-at-the-fire-tick.md) is what makes disengaging actually work,
because a ship out of range takes no further damage from a shot already fired.

**Q14's deferral was made honest rather than convenient**: the original heavy design cost 160 seconds of
total income against a strike force that crosses the map in 80 to 100 seconds, so no match would ever have
contained one, and M3 could not have answered this whatever anyone hoped. With M3's economy figures now
measured rather than assumed, the heavy design's cost can be set against real income for the first time.

**Done when:** matches have been played in which a heavy design is actually built, and Q14 is answered on
the register: whether speed counters mass, or whether the two decisions that were taken independently —
instant resolution and the speed counter — need one of them reopened.

### M4.8 — GATE: the MVP, played by four · — · hand · **human**

**Read first:** `GameDesign.md` §2 and §10; `TechnicalDesign.md` §9.

**Adds:** nothing. Four slots, humans and AI mixed, on real hardware over a real network — the
configuration every figure in `TechnicalDesign.md` §4's second row was computed against and which **the
MVP itself had no way to generate.** **Since 2026-09-24 that is the owner and three AI, on one machine**
(`OpenQuestions.md` Q58, Q79): no gate waits for a second person, and none waits for a second device.

**Done when:** a four-player match has been played to a victory; host egress, snapshot loss and frame time
are measured at that load against §4's arithmetic; and `GameDesign.md` §2's five-minute-match claim is
re-checked, since it was sized for two.

---

## Leaving M4

**The milestone is finished when** four commanders — the owner and three AI (Q79) — play a match on a
symmetric generated field with heavy ships in it, and the design's three standing claims about scale have
each been executed rather than asserted.

**What M4 produces besides code:** Q14 answered; Q9 closed; ADR-003's four-player figures measured; and
`TechnicalDesign.md` §9's list finally empty apart from the standing ARM64 obligation, which never empties.

**After this the plan stops**, and `GameDesign.md` §10 says what comes next and in what order: the designer
screen, research, fog of war and the interest set, delta replication, formations, and a mobile mothership
hull. **Each is worth doing against something running**, and by then there is something running — which is
the first moment any of them can be planned against something other than a guess.
