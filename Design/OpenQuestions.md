# Open Questions

The register. A question is added here with its options and a recommendation, put to the owner, and the
answer is written into the document it belongs to — this file keeps the row saying what was answered and
where. A task that needs an answer this design does not give **asks and gets it written in before the code
is**, rather than assuming.

**Needed by** is the milestone (`GameDesign.md` §10) that cannot be finished without the answer. A question
with no milestone can wait indefinitely.

---

## Answered — 2026-09-20

| | Question | Answer | Recorded in |
|---|---|---|---|
| **Q1** | Is the playfield a volume, a plane, or a plane with altitude bands? | **A plane.** A tap is a ray and a ray has no depth, and R21 leaves no second input to supply one. | [`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md) |
| **Q2** | Is the home base a fixed station or a mobile mothership? | **A fixed station.** Removes base pathfinding, docking and a class of AI problem from the MVP; a mothership is a hull like any other later. | `GameDesign.md` §5 |
| **Q3** | How much of the *Warzone 2100* design system lands in the MVP? | **The model, not the interface.** Hull, drive and slots in the simulation and on the wire from the first line; three fixed designs and no designer screen. | [`ADR-006`](ADR/ADR-006-a-ship-is-a-composition.md) |
| **Q4** | What fleet scale should the MVP target? | **About fifty ships a player**, 204 entities at peak, which is what makes full snapshots affordable. | `GameDesign.md` §10, [`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md) |

---

## Open

### Q5 — How does a client find a host? — **needed by M0**

**This is a hole in the brief and it blocks the first milestone.** There is no keyboard (R21), so nobody
can type an address, and there is no lobby. A client that cannot reach a host is not a client.

| Option | |
|---|---|
| **LAN discovery (recommended)** | The client multicasts a probe; hosts answer with a name and a player count; the client lists them as tappable rows. `DatagramSocket` supports it and `privateNetworkClientServer` in the manifest permits it. Fully touch-native, no text entry, and it is the LAN case the MVP targets anyway. |
| **An on-screen numeric keypad** | The game draws twelve large targets and the player taps an address. Works anywhere, including across the internet, and is tedious every single time. |
| **A configuration file in the package** | Cheapest to write and means a rebuild to change the address. Adequate for M0 and untenable after it. |

**Recommendation: LAN discovery, with the keypad as the fallback the moment discovery proves unreliable
on a real network.** The two are not exclusive and the keypad is perhaps eighty lines.

### Q6 — What is the target device? — **needed by M0**

The 72-pixel touch target in `Interface.md` §1 is derived from an assumed 12.3-inch 3:2 display at 188
pixels per inch. **A different device makes every number in that derivation different**, and a 10-inch
tablet makes them all worse.

There is no recommendation. This is a fact about the hardware the game is for, and it should be pinned
before the first panel is laid out — the derivation is reproduced in `Interface.md` §1 precisely so it can
be redone against the real number.

### Q7 — Is the authored resolution 16:9 or 3:2? — **needed by M1**

1920 × 1080 is 1:1 on the display a developer works on and letterboxes about fifteen per cent away on a
3:2 tablet. A 3:2 authored frame is the reverse. R13's scaling makes either correct; the question is which
case is common. **Recommended: 16:9**, on the grounds that it is exact on the common display and that the
answer is entangled with Q6.

### Q8 — Does hold-then-drag band select survive a real hand? — **needed by M1**

`Interface.md` §3 puts panning on a one-finger drag and band select behind a hold, because a drag cannot
mean two things. It costs roughly 300 ms before a band appears and it is not self-evident. The
alternatives are panning on two fingers, which taxes the most frequent action, or a selection-mode toggle,
which adds a mode. **No recommendation — this is answered by using it, not by arguing about it**, and it is
the first interface thing to re-examine once anything is playable.

### Q9 — What happens when a human disconnects mid-match? — **needed by M1**

Their ships freeze; an AI takes the slot; they are eliminated; they can reconnect. **Recommended: the slot
persists, ships hold position and keep whatever autonomous behaviour they have, and the player may
reconnect** — full snapshots ([`ADR-003`](ADR/ADR-003-replication-is-full-snapshots.md)) make reconnection
mechanically trivial, because there is no state to catch up on. What is not designed is what the *other*
players see while it happens.

### Q10 — Does the station have a weapon? — **needed by M3**

`GameDesign.md` §5 gives it none, so it is a thing you defend rather than a thing that defends itself, and
a raid is a real threat. The risk is that an undefended station makes a single early fighter decisive.
**No recommendation — this is a balance question and it is answered by playing M3**, not before.

### Q11 — How is text rendered? — **needed by M1**

There is no text renderer in this tree and no font in R14's dependency list. Credits, costs and counts all
need glyphs. **Recommended: a bitmap font baked into a header and drawn as instanced quads** — no
dependency, no file, and it is exact at 1:1 which is what R13's scaling is built around. What it forecloses
is any language that needs more than a few hundred glyphs.

### Q12 — What does suspend and resume look like? — **needed by M2**

A packaged application is suspended when it loses the foreground and the match continues without it. The
reconnect is mechanically trivial; what the player *sees* — and whether a suspended player's fleet should
be at risk for the whole of it — is not designed.

### Q13 — Can two players say anything to each other? — no milestone

There is no chat, no ping and no map drawing. With no keyboard the first needs a phrasebook and the second
needs a gesture, and nobody has proposed either. Worth nothing in a solo-against-AI MVP and worth a great
deal the first time two people play.

### Q14 — Is the fighter-against-battleship counter real? — **needed by M3**

`GameDesign.md` §7 claims the counter to a battleship is not a weapon but a speed difference — fighters
pick the fight, kill miners and leave. That is a claim about how a match plays and **it is the first thing
M3 should be used to check.** If it is false, the damage table is the cheapest lever and the price list is
the next.

### Q15 — Should any of this become an `AGENTS.md` rule? — no milestone

`AGENTS.md` R22 and up are reserved, and it says not to invent one. Two decisions here plainly constrain
the *shape* of code rather than the design — **the simulation is two-dimensional**
([`ADR-001`](ADR/ADR-001-the-playfield-is-a-plane.md)) and **the client derives the map from the seed rather
than being sent it** (`TechnicalDesign.md` §3) — and are the kind of thing a reviewer needs a rule
number to settle an argument with. Proposed, not taken: adding a rule to `AGENTS.md` is the owner's call.

---

## Adding one

Number it in sequence, give it a milestone or say it has none, state the options with what each costs, and
make a recommendation or say plainly why there is not one. **A question with no recommendation is fine**
when the honest answer is that it needs to be played rather than argued; a question with a recommendation
nobody wrote down is not.
