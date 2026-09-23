---
name: datagram-budget
description: Audit and cost the UDP datagram budget for Outpost Commander — the update going down and the command packet coming up — so every datagram stays whole and the refresh rate stays honest. Use this skill whenever a change touches the wire format or could grow it — adding or widening a field on the entity record, the update header or a command, adding a repeated fact such as a removal or an event, changing the update rate or the per-client cap, adding a new kind of replicated entity, or anything that asks "will this still fit", "how often does an entity refresh", "how big is the packet", "are we near the MTU", "should we split the datagram" or "can we afford another byte". Use it proactively when reviewing a design or plan change that adds replicated state, even if nobody mentions packets, because a byte on the record is a byte on every record and the cost shows up as a slower refresh nobody measured. Running `Scripts/DatagramBudget.py` is a precondition of touching a datagram at all, not advice to consider: it computes the budget rather than estimating it, states the refresh interval a view will get, audits every field's width against what the client actually draws, and refuses a split that is fragmentation wearing a better name.
---

# The datagram budget

`Design/ADR/ADR-024` buys one thing above all: **every datagram is whole, and no datagram depends on
another.** A record is a self-contained fact about one entity at one tick; an update is a bag of them
that a per-client priority accumulator filled; a lost update delays some refreshes by a tick and breaks
nothing. There is no resend, no acknowledgment, no ordering and no reassembly, at a hundred players as at
two. **What a byte costs under this design is not "does it still fit" — it always fits — but "how many
records fit", and therefore how often each entity is refreshed.** That is the figure to state, and it is
the one people forget to, because nothing ever fragments to remind them.

## Run the script. Every time, before anything else

**Touching a datagram means running `Scripts/DatagramBudget.py` in that same piece of work.** Not when a
change looks big, not when the refresh feels fine — every time a field, a repeat count, a rate, a cap or a
record moves:

```bash
python3 Scripts/DatagramBudget.py                       # the update at the MVP's shape
python3 Scripts/DatagramBudget.py --players 100 --ships 50 --in-view 1000   # a view, at scale
python3 Scripts/DatagramBudget.py --add-bytes 1         # cost of a proposal, in records and refresh
python3 Scripts/DatagramBudget.py --datagrams 2         # the cap
python3 Scripts/DatagramBudget.py --all                 # + field audit + upstream
```

This is a precondition rather than a step because of how the failure presents now. Under the full snapshot
a byte too many fragmented the datagram, and at least that was a thing a test could see. **Under ADR-024
nothing fails.** A byte on the record means one record fewer per update, one tick longer per sweep, and an
entity at the edge of the tactical view that refreshes at 500 ms instead of 450. Nobody notices in the
build, nobody notices on the desk, and it accumulates until a stress run reports a refresh interval a
player would see. The arithmetic takes under a second and is the only cheap moment this defect has.

Two things follow, and both are absolute:

- **A figure the script did not produce does not go in a report, a document or a commit message.** The
  numbers here moved four times in two days under the old design; a stale one is indistinguishable from a
  true one.
- **If the script disagrees with `Design/TechnicalDesign.md` §4, one of them is stale.** Say which, fix
  it, and do not proceed on the other until they agree. `Scripts/CheckDesign.py` recomputes the figures
  from the script and fails when a document does not state them.

## What the script states, and what each number means

- **Records per datagram** — 99 at the pinned 1,232 with three removals and two fire events riding. The
  one number every byte on the record moves.
- **Per client per update** — 24.6 KB/s at 20 Hz, and it does not depend on the entity count. It depends
  on the cap: two updates a tick is 49.3.
- **The sweep** — ⌈live entities ÷ records per tick⌉ ticks. ADR-024's guarantee: no entity goes longer
  unrefreshed whatever its relevance, and the client forgets one after three sweeps of silence. Two ticks
  at the MVP, 56 at 5,500 entities with one update, 28 with two.
- **The refresh interval for a view** (`--in-view N`) — how often each of N entities on one screen is
  refreshed if the accumulator sends nothing else. This is the number a player feels. A hold between
  refreshes is invisible on a 3.5-pixel silhouette and visible on a ship filling the screen, so quote
  it with the zoom it is for.
- **Host egress** (`--clients N`) — per client times clients. 2.46 MB/s at a hundred with one update;
  the host's CPU for the accumulator is not in the script and is ADR-024's owed measurement.

## The one question that decides a split

"Split it into two datagrams" hides two very different things. Ask this, and nothing else:

> **Can the client draw a correct frame from one datagram without the other?**

**No → it is fragmentation**, whatever it is called, and ADR-024 removed it from the transport. Both
halves are needed, so losing either loses both, and that is the cost the whole design exists to avoid.
Do not take it to buy room, and do not bring the fragment fields back.

**Yes → it is another whole update, and it is what the cap already is.** Each update the accumulator
sends is complete and separately renderable; a second one a tick is more refresh, not a bigger frame.
Raising the cap is the lever for a view that holds more than fits, and it costs bandwidth linearly. **Do
not split by entity kind or region into datagrams that are each required**: a datagram of "the ships"
and a datagram of "the modules" that the client cannot draw one without is fragmentation with a better
name.

## The levers, cheapest first

Work down this list. Stop at the first one that buys enough, and **name what it costs** — every one of
these costs something. **The currency is records per datagram, and the price is quoted in refresh.**

**1. Raise the cap.** One constant, linear bandwidth, no format change. Right when a *view* needs more
than fits and the bandwidth is there; wrong when the problem is a field that should not have been added.
*Costs:* 24.6 KB/s per client per update, and a host that sends twice as many datagrams.

**2. Weight the accumulator, not the wire.** If the complaint is "this entity refreshes too slowly", the
first question is whether relevance is scoring it right, which is `OpenQuestions.md` Q49 and costs an edit
to a constant. A byte on the record is the wrong fix for a weight.

**3. Reclaim the MTU margin.** The pin is **1,232**, the IPv6 minimum-MTU payload exactly (ADR-003). The
next step is the IPv4 Ethernet path at **1,472**: +240 B, **twenty records**, or 119 against 99. *Costs:*
a path that works only where IPv4 over Ethernet runs end to end — no VPN, no PPPoE, no IPv6. Defensible on
a LAN; a decision rather than a default, and at that size it wants an ADR.

**4. Audit the field widths.** `--audit-fields` prints what each field spends against what the client
draws. Today: heading 8 → 7, hull 8 → 4, owner 8 → 7, and three spare flag bits: **9 recoverable bits per
record**, a 12-byte record to 87 bits, **109 records per datagram against 99**. *Costs:* the record stops
being byte-aligned — a bit writer on both sides and a record no debugger or capture shows — and every
subsequent wire change is harder to reason about. Take it when a measured refresh is too slow and the cap
is too expensive, not before. **Read the reserved column before claiming slack**: design identity is a
byte on purpose (R24) and the script excludes it.

**5. Quantize harder.** Distinct from lever 4: that spends fewer bits on the same range, this shrinks the
range. Position is already a quarter of a unit. Going further means asking what the client *draws* from a
field and giving it exactly that.

**6. Let the client derive it instead.** R23 does this for the whole map. Anything a shared rule in
`GameCore` computes from the seed or from state already sent does not need sending. Zero bytes and zero
precision. *Watch:* it must be a rule both sides evaluate, never the client simulating (R19).

**7. Interest management proper.** The accumulator already sends each client a different set every tick;
fog of war makes the set smaller by scoring what the client may not see at zero. It is a change to the
relevance function and not to the wire, and it is the lever that scales best.

**8. Delta encoding — declined, and know why before re-proposing it.** ADR-003 examined and rejected it
on its merits: **delta saves most when nothing is moving and least when everything is.** Under ADR-024 it
would also cost the property that a record is meaningful alone. Re-propose it only by defeating both.

## Repeated facts are not free

A removal rides ten consecutive updates and a fire event three, so a death costs 30 bytes over ten ticks
and a shot 21 over three. At the defaults that is 23 bytes of every update, two records' worth. **A new
kind of repeated fact — an event, a notification, anything sent for reliability by repetition — is a
change to this budget**, and its repeat count is a number the script takes. Under a barrage the fire
events alone could fill an update; ADR-004 bounds that by the weapon count, and the day it does not is
the day fire events want a cap of their own.

## Upstream: the command packet

Run `--upstream`. **The format is not the constraint.** A command is a type, a target, a selection of
three-byte identities and a sequence; the header carries the client's view center and radius since
ADR-024, twelve bytes. One command at the peak 110-identity selection is 338 B, and a packet holds 404
identities against a peak of 110 — an amplification the host validates away (`TechnicalDesign.md` §4).

**The retransmit window was the constraint, and §4 bounds it structurally.** Commands are repeated until
the update's own block acknowledges them, so a stalled host pushes the count up under exactly the load
where a fragmented command packet is worst. The packet is filled oldest-first and stops when the next
command will not fit — three at a full selection — so it cannot exceed the payload and the sequence gains
no gap. Upstream's own bound is one command: a change is upstream's problem only when a single command
grows past the payload, which needs a designer submitting a design definition rather than an identity
(M4+). Run `--selection` raised the day it is proposed.

## What not to do

- **Do not fragment to gain room**, and do not reintroduce the fragment fields for "later".
- **Do not put a float on the wire.** R16 keeps the simulation integer; a float in a record is a
  cross-platform divergence with a network hop in front of it.
- **Do not solve a budget problem by making the client compute simulation state.** R19. Deriving from a
  shared `GameCore` rule is lever 6 and is fine; deriving by simulating is not.
- **Do not narrow a field the simulation needs wide.** Lever 4 narrows what is *sent*. The simulation's
  own precision is R16's business and is not the budget's to spend.
- **Do not make a record depend on another record.** A design table in the header, a delta against the
  previous update, an owner implied by position in the datagram — each makes a record meaningless alone,
  which is the property everything else here rests on.

## What to hand back

A proposal is not a direction, it is a change with a price. Report in this shape, and **every number in
it comes from the script** — a report without the script's figures is not a finished report:

```
Script run:      DatagramBudget.py <flags used>            ← if this line is empty, the work is not done
Budget now:      <record> B record, <header> B header, <records> per datagram, sweep <n> ticks at <entities>
The change:      <what it adds, per record, per header, per repeated fact, per command>
Budget after:    <records> per datagram, sweep <n> ticks; a view of <N> refreshes every <k> ticks
Upstream:        <unchanged, or the new per-command cost and how many fit>
Lever chosen:    <which one, and why the cheaper ones above it were not enough>
What it costs:   <the thing that gets worse — every lever has one>
Documents to update: TechnicalDesign.md §4 (and its upstream half if that moved), ADR-024's figures,
                 and any figure the consistency check moves
```

Then update the design in the same change, because a budget that lives only in a report is a budget
nobody can check. `Design/TechnicalDesign.md` §4 and ADR-024 both state these figures, and the old
design's copies disagreed before; when one moves, move both.
