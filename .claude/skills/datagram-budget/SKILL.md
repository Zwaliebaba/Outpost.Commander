---
name: datagram-budget
description: Audit and optimise the UDP datagram budget for Outpost Commander — the snapshot going down and the command packet coming up — so each keeps fitting one packet. Use this skill whenever a change touches the wire format or could grow it — adding or widening a field on the entity record, the snapshot header or a command, raising an entity or module cap, adding a player, adding a new kind of replicated entity, changing the snapshot rate, or anything that asks "will this still fit", "how big is the packet", "are we near the MTU", "should we split the datagram" or "can we afford another byte". Use it proactively when reviewing a design or plan change that adds replicated state, even if nobody mentions packets, because the headroom is small and a change that overflows it is discovered late and expensively. Running `Scripts/DatagramBudget.py` is a precondition of touching a datagram at all, not advice to consider: it computes the budget rather than estimating it, audits every field's width against what the client actually draws, ranks the levers cheapest-first, and refuses a split that is fragmentation wearing a better name.
---

# The datagram budget

`Design/ADR/ADR-003` buys one thing above all: **a snapshot is self-contained and fits one datagram.**
No baseline, no acknowledgement, no history — so a lost packet costs one frame of animation and nothing
can diverge. Every byte spent is spent against that property, and it is nearly spent.

## Run the script. Every time, before anything else

**Touching a datagram means running `Scripts/DatagramBudget.py` in that same piece of work.** Not when a change
looks big, not when the headroom feels tight — every time a field, a cap, a rate or a record moves:

```bash
python3 Scripts/DatagramBudget.py            # the snapshot as it stands
python3 Scripts/DatagramBudget.py --all      # + field audit + upstream
python3 Scripts/DatagramBudget.py --add-bytes 1   # cost of a proposal
```

This is a precondition rather than a step because of how the failure actually presents. Nothing in the
build fails when a record grows past the MTU — the code compiles, the tests pass, and the game works on
the desk it was written on. It surfaces as fragment loss under battle load on someone else's network,
months later, and by then the field that cost the byte is load-bearing. The arithmetic takes under a
second and is the only cheap moment this defect has.

Two things follow, and both are absolute:

- **A figure the script did not produce does not go in a report, a document or a commit message.** The
  numbers here moved four times in two days; a stale one is indistinguishable from a true one.
- **If the script disagrees with `Design/TechnicalDesign.md` §4, one of them is stale.** Say which, fix
  it, and do not proceed on the other until they agree.

`AGENTS.md` §6 asks for figures that are measured rather than estimated. The script is the design's own
arithmetic, so a proposal arrives costed and two people get the same answer.

## Both directions are in scope

**Downstream, the snapshot**, is the one under pressure: 110 entities at 20 Hz, and the headroom is
double digits. Everything below about levers and splits is about this direction.

**Upstream, the command packet**, is not under pressure from its format — but it has a bound nobody
wrote down, and `--upstream` finds it. See *Upstream* below before concluding it is fine.

## The format is not a constraint yet, and that expires

**Nothing has shipped.** There is no deployed client, no replay saved in the old layout, no second
implementation to agree with. So the record can be *reshaped*, not merely widened or narrowed — a field
can be moved, split, made columnar, or deleted outright, and the cost of doing so is the cost of writing
the code once. Do not reason as though a field is fixed because it is in `TechnicalDesign.md` §4.

What that licenses is **lever 0** below: ask what shape the record should be before asking how wide its
fields are. What it does not license is three things, and they are the ones people get backwards:

- **It does not waive an ongoing cost.** Lever 2's bit writer costs the same whenever it is written, and
  costs again every time somebody reads a packet capture for the rest of the project. Freedom to change
  the format is not freedom from what the format costs to live with.
- **It does not make taking a lever early free — it makes deferring cheap.** With no migration to do,
  there is no "get it in before it is too late", so a lever you never need is a lever you never pay for.
  The obvious conclusion from "we can change it freely" is *change it now*; the correct one is the
  opposite, and lever 2 is where it bites.
- **It expires, and nobody decides when.** It ends at the first build handed to a second person, which is
  an event rather than a decision. The header's version byte already turns a mismatch into a refused
  connection rather than silent corruption (§5) — that is the cheap half. The expensive half is that a
  format change after that point is a coordinated release of two executables. **Spend this freedom before
  M1 ships, or accept that whatever is in the format then is what stays.**

## The one question that decides a split

People reach for "split it into two datagrams" when a packet gets full, and the phrase hides two very
different things. Ask this, and nothing else:

> **Can the client draw a correct frame from one datagram without the other?**

**No → it is fragmentation**, whatever it is called. Both halves are needed, so losing either loses the
frame, and all-or-nothing reassembly makes a snapshot roughly twice as likely to be lost as a packet is.
That is exactly the cost the reduced MVP was shaped to avoid. Do not take it to buy room.

**Yes → it is rate separation, and it is the good version.** Split by *how fast the state changes*, not by
byte count:

- **Fast**, every tick: position, heading — the things whose staleness is visible as a ship in the wrong
  place.
- **Slow**, a quarter of the rate: hull, cargo bucket, design identity, credits, build progress. Lose one
  and a bar is stale for 200 ms while the ships keep moving correctly.

That is independence in the sense that matters: each datagram is *separately renderable*. It also pays
twice, because the slow half stops costing full rate.

**What it costs**, and say so when proposing it: two encoders, two sequence numbers, a client that holds
two ages of state at once, and a class of bug where the halves disagree about which entities exist — so
the removal list belongs in the fast half, with the identities.

## The levers, cheapest first

Work down this list. Stop at the first one that buys enough, and **name what it costs** — every one of
these costs something.

**0. Reshape the record before shrinking its fields.** Available only while the section above holds, and
free until it expires. Three candidates the current layout never considered, because it was written as
though the format were already fixed:

- **Columnar rather than per-entity.** Six arrays instead of 110 records. That is the same bytes by
  itself — the point is that it makes each column's encoding independent: a 4-bit hull column packs two
  entities to a byte without disturbing anything around it, so lever 2's alignment cost is confined to
  the one column that earns it instead of being paid across the whole record. It also makes lever 5's
  split fall out by column rather than by surgery. *Costs:* the client assembles an entity from six
  parallel reads instead of one.
- **Delta the identity column.** Identity is an index and a generation packed (§4), and the host builds
  the per-player set itself — so it can emit it in index order, where a sorted column has small deltas
  and a raw one has none. At 2 B × 110 entities the identity column is the largest single block in the
  snapshot, which makes this plausibly the biggest saving available at zero precision cost. **It cannot
  be quoted until §4 states the index/generation bit split, which it does not.** Settle that first.
- **Ask whether a field is per-entity at all.** Design identity has a byte on every record because the
  record is per-entity — but a 50-ship fleet repeats a handful of designs. A design table in the header
  with a short index per entity is the same information in fewer bits. Lever 4 asks whether the client
  can derive a field; this asks whether it needs to be sent once instead of 110 times.

*Costs:* nothing today, which is exactly why it is first and why it has a deadline rather than a price.

**1. Reclaim the MTU margin — most of it is already claimed.** The pin is **1,232**, the IPv6
minimum-MTU payload exactly: 1,280 less 40 and 8 (ADR-003). The 32-byte reserve that QUIC's 1,200 keeps
back for an extension header or a tunnel has already been spent, so this lever has one step left rather
than two, and it is a long one — the IPv4 Ethernet path at **1,472**, worth **+240 bytes, 24 entity
records**, which is far the largest single block of unclaimed room in the budget. Claiming it is one
constant. Run the script; the ladder it prints is this lever.
*Costs:* a much larger claim than the last one, and a different kind. At 1,232 the game works over any
conformant IPv6 path and fails only under encapsulation. At 1,472 it works only where the path is IPv4
over Ethernet end to end — no VPN, no PPPoE (1,464), no IPv6 anywhere. Defensible while `GameDesign.md`
§2 says the target is a LAN; **a decision, not a default**, and at that size it wants an ADR rather than
a constant edit.

**2. Audit the field widths — what a field spends against what the client draws.** `--audit-fields`
prints this. The principle is worth holding on to, because it is what turns a vague "pack it tighter"
into an arithmetic answer:

> **A field's width on the wire is decoupled from its width in the simulation.** The simulation keeps
> whatever precision R16 needs — 16-bit headings, exact hull points — and none of that obliges the wire.
> A wire field is sized by **the number of distinct values the client can actually draw**, and nothing
> else. A `bool` costs a byte only if you let it have one; the flags byte is what it costs instead, and
> a hull percentage drawn as a 16-level bar is four bits pretending to be eight.

What the audit says today: **6 recoverable bits per entity**, from heading (8 → 7, still 2.8° and still
smooth) and hull (8 → 4, which is every level a bar resolves), plus the flags byte's spare bit. Over 110
entities that is **82 bytes**, taking the snapshot to 1,054 B and the headroom from 96 B to **178 B** —
nearly doubling it, and more than any other lever short of interest management.

**Read the reserved column before claiming slack.** Design identity is a full byte to hold a handful of
designs, and that is not waste: R24 widened it on purpose so research and a design interface do not
become a format change. A field widened by a decision already taken is not slack, and the script
excludes it rather than tempting you.

*Costs:* **the record stops being byte-aligned.** A bit writer on both sides instead of a memcpy of a
packed struct, and fields that no longer line up with anything a debugger or a packet capture shows you.
Take it when the alternative is a second datagram. **Do not take it before then, and note that no
backwards compatibility makes that stronger rather than weaker:** there is no migration to get ahead of,
so deferring costs nothing and a lever never needed is never paid for. It buys the most of any lever
short of interest management, and it makes every subsequent wire change more expensive to reason about.

**3. Quantise harder.** Distinct from lever 2: that one spends fewer bits on the same value range, this
one shrinks the range. Position is already a quarter of a world unit over the play area. Going further
means asking what the *client draws* from a field and giving it exactly the precision that draws.

**4. Let the client derive it instead.** R23 already does this for the whole map — the client runs the
generator rather than being sent asteroids. Anything a shared rule in `GameCore` can compute from a seed
or from state already sent does not need sending. This is the only lever that costs zero bytes *and* zero
precision.
*Watch:* it must be a rule both sides evaluate, never the client simulating (R19).

**5. Rate-separate**, per the independence test above.

**6. Interest management.** The host already serialises a per-player entity set rather than the world; in
the MVP that set is everything. Making it smaller is the fog-of-war work, and it is the lever that scales
best and costs most.

**7. Delta encoding — declined, and know why before re-proposing it.** ADR-003 examined and rejected it,
not on the costs first given (a history ring is 240 KB and the acknowledgement channel already exists in
`lastCommandSeqApplied`) but on its merits: **delta saves most when nothing is moving and least when
everything is.** It optimises the idle case and degenerates to a full snapshot plus a bitmask during the
battle that is the only time the budget is under pressure. Re-propose it only by defeating that.

## Upstream: the command packet, and the bound nobody wrote down

Run `--upstream`. The answer it gives is not the reassuring one it first looks like.

**The format is not the constraint.** A command is a type, a target, a selection of identities and a
sequence; one command at the peak 110-identity selection is 228 B, and a packet has room for 610
identities — the 5.5× amplification `TechnicalDesign.md` §5 already makes the host validate away.

**The retransmit window was the constraint, and finding it is what `--upstream` is for.** Commands are
*repeated in every outgoing packet until acknowledged*, so the packet holds however many are outstanding
— and at a full selection **five fit and six fragment**. That count is behaviour, not format: a stalled
host or a run of lost snapshots pushes it up, under exactly the load where a fragmented command packet is
worst. §5 now answers it structurally — **the packet is filled oldest-first and stops when the next
command will not fit** — so the packet cannot exceed the payload, no order is dropped, and the sequence
gains no gap the host would discard.

Two things to take from that shape. First, **no lever from the list above was the answer**: an unbounded
count of anything is not fixed by making each one smaller, and a packing lever spent there would have
bought a larger number before the same cliff. Second, **upstream's own bound is one command**, not the
packet — a change is upstream's problem only when a single command grows past the payload. Today that
needs **a designer submitting a design definition rather than an identity** (M4+), which turns a 2-byte
field into a variable-length record. Run `--selection` raised, or `--add-bytes` against the command, the
day it is proposed.

## What not to do

- **Do not fragment to gain room.** See the independence test. If the answer is "it fragments but only
  sometimes", that is worse — an intermittent doubling of the loss rate under exactly the load that causes
  it.
- **Do not put a float on the wire.** R16 keeps the simulation integer; a float in a record is a
  cross-platform divergence with a network hop in front of it.
- **Do not solve a budget problem by making the client compute simulation state.** R19. Deriving from a
  shared `GameCore` rule is lever 4 and is fine; deriving by simulating is not.
- **Do not narrow a field the simulation needs wide.** Lever 2 narrows what is *sent*. The simulation's
  own precision is R16's business and is not the budget's to spend.

## What to hand back

A proposal is not a direction, it is a change with a price. Report in this shape, and **every number in
it comes from the script** — a report without the script's figures is not a finished report:

```
Script run:      DatagramBudget.py <flags used>            ← if this line is empty, the work is not done
Budget now:      <entities> entities, <record> B record, <total> B — <free> B free (<n> entities)
The change:      <what it adds, per entity, per header, per command>
Budget after:    <total> B — <free> B free, still one datagram / FRAGMENTS at <cap>
Upstream:        <unchanged, or the new per-command cost and the in-flight count that fragments>
Lever chosen:    <which one, and why the cheaper ones above it were not enough>
What it costs:   <the thing that gets worse — every lever has one>
Documents to update: TechnicalDesign.md §4 (and §5 if upstream moved), ADR-003's table, and any
                 figure the consistency check moves
```

Then update the design in the same change, because a budget that lives only in a report is a budget
nobody can check. `Design/TechnicalDesign.md` §4 and ADR-003's table both state these figures, and they
have disagreed before — when one moves, move both.
