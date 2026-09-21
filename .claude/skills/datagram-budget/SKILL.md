---
name: datagram-budget
description: Audit and optimise the UDP snapshot datagram budget for Outpost Commander so a snapshot keeps fitting one packet. Use this skill whenever a change touches the wire format or could grow it — adding or widening a field on the entity record or the snapshot header, raising an entity or module cap, adding a player, adding a new kind of replicated entity, changing the snapshot rate, or anything that asks "will this still fit", "how big is the packet", "are we near the MTU", "should we split the datagram" or "can we afford another byte". Use it proactively when reviewing a design or plan change that adds replicated state, even if nobody mentions packets, because the headroom is small and a change that overflows it is discovered late and expensively. It computes the budget rather than estimating it, ranks the levers cheapest-first, and refuses a split that is fragmentation wearing a better name.
---

# The datagram budget

`Design/ADR/ADR-003` buys one thing above all: **a snapshot is self-contained and fits one datagram.**
No baseline, no acknowledgement, no history — so a lost packet costs one frame of animation and nothing
can diverge. Every byte spent is spent against that property, and it is nearly spent.

Run the numbers first, always:

```bash
python3 .claude/skills/datagram-budget/scripts/budget.py                 # as it stands
python3 .claude/skills/datagram-budget/scripts/budget.py --add-bytes 1   # cost of a proposal
```

`AGENTS.md` §6 asks for figures that are measured rather than estimated. The script is the arithmetic the
design already states, so a proposal arrives costed and two people get the same answer. **If the script
disagrees with `Design/TechnicalDesign.md` §4, one of them is stale — say which, and fix it.**

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

**1. Reclaim the MTU margin.** The design pins a 1,200-byte payload, which is the conservative
*safe-anywhere* figure: it sits below IPv6's 1,232-byte minimum-MTU payload and survives tunnels. A LAN
IPv4 Ethernet path gives **1,472**. Run the script — that margin is usually the largest single block of
unclaimed room, and claiming it is one constant.
*Costs:* the game stops working over a small-MTU path — a VPN, PPPoE, an IPv6 route with a low MTU.
Defensible while `GameDesign.md` §2 says the target is a LAN; **a decision, not a default**, and it wants
recording rather than assuming.

**2. Pack into bits that already exist.** The flags byte carries team, state and cargo with one bit spare
(`TechnicalDesign.md` §4). Free when it fits, and nearly exhausted — check before promising it.

**3. Quantise harder.** Position is already a quarter of a world unit over the play area and heading is
already 8 bits. Going further shows: ask what the *client draws* from a field, and give it exactly the
precision that draws.

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

## What not to do

- **Do not fragment to gain room.** See the independence test. If the answer is "it fragments but only
  sometimes", that is worse — an intermittent doubling of the loss rate under exactly the load that causes
  it.
- **Do not put a float on the wire.** R16 keeps the simulation integer; a float in a record is a
  cross-platform divergence with a network hop in front of it.
- **Do not solve a budget problem by making the client compute simulation state.** R19. Deriving from a
  shared `GameCore` rule is lever 4 and is fine; deriving by simulating is not.
- **Do not quote a figure the script did not produce.** The reason this skill exists is that the numbers
  moved four times in two days and a stale one is indistinguishable from a true one.

## What to hand back

A proposal is not a direction, it is a change with a price. Report in this shape:

```
Budget now:      <entities> entities, <record> B record, <total> B — <free> B free (<n> entities)
The change:      <what it adds, per entity and per header>
Budget after:    <total> B — <free> B free, still one datagram / FRAGMENTS at <cap>
Lever chosen:    <which of the seven, and why the cheaper ones were not enough>
What it costs:   <the thing that gets worse — every lever has one>
Documents to update: TechnicalDesign.md §4, ADR-003's table, and any figure the consistency check moves
```

Then update the design in the same change, because a budget that lives only in a report is a budget
nobody can check. `Design/TechnicalDesign.md` §4 and ADR-003's table both state these figures, and they
have disagreed before — when one moves, move both.
