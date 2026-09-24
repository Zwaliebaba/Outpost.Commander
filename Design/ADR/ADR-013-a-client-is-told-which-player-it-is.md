# ADR-013 — A client is told which player it is, by a join the host answers

**Status:** Accepted — **amended 2026-09-23 at M2.3**: the reply also carries the match's player count,
because the field is derived from the seed *and* the count and a client told only the seed would draw the
wrong one. `OpenQuestions.md` Q50, ruled by the owner.
**Date:** 2026-09-22
**Owner:** Stefan Zwaal

## Context

`Plan/README.md` F2. [`GameDesign.md`](../GameDesign.md) §2 configures the four slots on the host before
the match starts and says there is nothing to negotiate once it is running.
[`TechnicalDesign.md`](../TechnicalDesign.md) §4 specifies the snapshot, the command and the heartbeat.
**Between them, nothing tells an arriving client which of the per-player blocks is its own**, and nothing
says what a host does with a client it was not expecting. §5's "the protocol version in the header refuses
a mismatched build" implies a handshake that is defined nowhere.

M0 dodged this by having one entity and no ownership. The client was told which player it was by a
compiled-in constant, and `GameClient/ClientFrame.h` says so in as many words: *"there is no join yet"*.
**M1 cannot dodge it.** Selection needs to know which entities are mine, validation needs to know who is
sending, and the credits readout needs to know which block to read.

**The match seed was missing from the question and R23 already required it.** The client runs the asteroid
generator itself — that is the whole of R23, and no map is ever transmitted — so a client cannot draw the
field without the seed, and a join is the only place the seed can arrive.
[`ADR-019`](ADR-019-the-sky-is-generated-from-the-seed.md) is a second consumer of the same number and
costs nothing extra because of it.

## Decision

**Two packet types, two records, and a session table on the host.**

### The client sends a `Join`; the host answers a `JoinReply`

`PacketType` gains `Join = 4` and `JoinReply = 5`. The protocol version goes to **2**, because a build
that does not know these two types is a build this one cannot play against, and that is the whole of what
the version field is for.

| `Join` | bytes | |
|---|---|---|
| session token | 8 | what the host issued last time, or **zero for a client that has none** |

| `JoinReply` | bytes | |
|---|---|---|
| result | 1 | `Accepted` 1, `Rejoined` 2, `MatchFull` 3. **Zero is not a result**, for the reason `PacketType` gives |
| player | 1 | the slot, numbered from one; `NO_PLAYER` on a refusal |
| player count | 1 | **since M2.3**: the match's configured count, which the field is derived from alongside the seed; zero on a refusal |
| session token | 8 | the client stores it and presents it next time |
| match seed | 8 | R23's, and with the count one of the two things on the wire that a client cannot get from a snapshot |

Nineteen bytes of payload since M2.3, eighteen before it, behind the header. Neither record is
retransmitted and neither is acknowledged: **the client repeats the `Join` until a reply arrives**, every 250 milliseconds, and the
host answers every one of them. A `Join` from a client that already holds a session is answered with the
**same** reply rather than claiming a second slot, which is what makes a lost reply cost one retry instead
of a slot.

### Amended at M2.3: the reply carries the player count

**R23's inputs are two numbers, and this record sent one.** M2.1's generator places one player's region
and M2.2 copies it — a half at two players, a quarter at four — so one seed is two different maps, and
`GenerateField` takes the count for that reason. Nothing on the wire carried it: the reply had the seed,
and since [`ADR-024`](ADR-024-replication-is-prioritized-records.md) an update carries only its recipient's
per-player block. The client's camera had been calling `StartAnchor(2, player)` with the count
compiled in, which put a four-player match's second player on the wrong side of the map.

**A byte on this reply, ruled by the owner on 2026-09-23** (`OpenQuestions.md` Q50), over two alternatives:

- **A field that does not depend on the count** — always a quarter copied four ways. Nothing on the wire
  moves, but `GameDesign.md` §3's "a half at two players" goes, M2.1's pinned table is repinned, and a
  two-player map carries two home fields nobody owns.
- **A count inferred from the replica store**, from the distinct station owners it has heard of. Nothing on
  the wire moves, but ADR-024 sends records in priority order over several ticks, so the count can be low
  for the first second and the field drawn wrong and then redrawn — and at M3 a destroyed station changes
  the answer.

**The count is the host's configured one, not how many have joined**, because the field is fixed when the
match begins: the first client of a four-player match derives the four-player field. One byte holds it
because `MAX_PLAYERS` is 254. The protocol version goes to **5**, because a version-4 client would read the
count's byte as the first of its token.

### The host assigns the slot and a client does not choose

§2 configures the match on the host and says there is nothing to negotiate; R21 leaves no way to type a
slot number in any case. A `Join` carries no preference and the host hands back **the lowest free slot**.

**This collapses "a second client claims a taken slot" into the only case that can actually happen** —
there is no free slot at all — which is answered with `MatchFull`. That is a reply and not a silence,
because a client that is refused has something to show a player and a client that is dropped has nothing
to distinguish it from a host that is not running.

### A session token is a name, not a credential

**A client does not choose its own identity; the host issues one.** The token is 64 bits, drawn from a
`Neuron::Pcg32` on **stream 1**, and it is the first claim anyone has made on that space —
`NeuronCore/Pcg32.h` left the derivation to M2's generator, which now owns every stream but this one.

**The stream is seeded once per host run, from a salt the shell reads off the wall clock, and no match
reseeds it** (amended 2026-09-24 after the mid-implementation review's B4). It was seeded from the match
seed at every `BeginMatch`, which made the tokens a pure function of the seed and the join order. A host
restarted on the same seed then handed out last evening's tokens again, in the new join order: two
returning clients that launched the other way round were seated as player 1 with each other's old token,
evicted each other every second and never took seat 2 — a symptom that looks like a network fault, with a
workaround written nowhere. `Server` passes the salt through `Host::SaltTokens` (R16 allows wall time at the
shell and nowhere below it); a suite passes a constant, so a reconnect is still pinned. Because `Begin`
continues the stream rather than restarting it, no token of an earlier match on the same host names a seat
of a later one either.

**A client that knew the salt could compute the tokens, and that is accepted rather than overlooked.**
§5 declines authentication and any defense against a hostile client outright; what a token has to separate
is *a client returning* from *a client arriving*, which is an accident rather than an adversary. What it
does buy over a counter is that a fresh client cannot take a held slot by asserting a small number.

**Zero is never issued**, so zero means "I have none" with no second field to say so.

### A version mismatch is dropped, exactly as every other packet is

A `Join` whose header carries another version is dropped where every other packet is dropped, and §5 stays
true as written. **The cost is stated rather than discovered: a mismatched build is indistinguishable
from an unreachable host**, and both show the same overlay. The alternative was a refusal that must itself
decode on a client of a different version, which means freezing the first bytes of one reply across every
future version — a real commitment, taken here for a two-machine MVP whose address is hand-typed into a
text file ([`ADR-008`](ADR-008-the-host-address-is-configuration.md)), where a wrong address is by far the
likelier failure.

### A timeout drops the endpoint; it never drops the slot

**`TechnicalDesign.md` §4 and `GameDesign.md` §2 read as though they disagree and do not.** §4 has a
client heartbeat "so the host can time it out"; §2 holds a disconnected player's slot **indefinitely** and
lets them reconnect. Both are true of different things: **a timeout forgets an endpoint, so the host stops
sending snapshots into the dark. The session, the slot and the fleet all stay.** A client that comes back
presents its token and is given the same slot with a new endpoint.

The timeout itself is not implemented by this decision, because nothing yet suffers from its absence —
four endpoints that never expire cost four `sendto` calls a tick. The rule is written down so that
whoever adds it cannot add the other one by mistake.

### The command packet's player byte is what the client says; the session is what the host believes

`CommandPacket` keeps its player byte and `TechnicalDesign.md` §4's eight-byte command header is
unchanged. **The host no longer takes it at face value**: it resolves the player from the sender's
session, and a packet from an endpoint with no session is refused and counted. Where the byte and the
session disagree the host believes the session and counts that separately — which turns a stale packet
from a previous match from a silence into a number somebody can look at.

**A command from an endpoint that never joined is refused.** That is the teeth of this whole record: until
now the host created a client on the first command packet and believed whatever player it claimed.

### The seed is the host's, and it is configuration

The host holds the match seed, `Server.cpp` takes `--seed`, and the default is the one fixed value M0 and
M1 run (`GameDesign.md` §3). Every accepted join gets the seed, so a reconnecting client gets **the same
seed** and redraws the same field.

**`Host::BeginMatch` STARTS A MATCH RATHER THAN SETTING A NUMBER, AND M1.5 MADE THAT TRUE.** This
record introduced it for the seed alone and said the world was M3's; the moment `GameCore/Layout.h`
placed stations, a `BeginMatch` that did not clear would place a second set on a second call. It now
empties the world, drops every seat and places the layout — which is what a restart wants in any case.

## Consequences

**A client cannot tell that the host has reseeded**, and `Interface.md` §7 has the host reseeding and
starting another match at victory. A client that stays connected across that boundary draws the previous
match's asteroids. Nothing detects it today; the cheapest fix when M3 has a victory to trigger it is the
snapshot's own tick going backwards, which is already on the wire and costs nothing. **It is named here
rather than solved here** because there is no way to reach it before M3. **That detector cannot fire as
built** (the mid-implementation review, M8 and m15): `BeginMatch` never resets the host's tick, and the
replica store refuses anything not newer. The restart's wire is the review's D8 (`OpenQuestions.md` Q70).

**A lost `JoinReply` after the host has issued a token is the one hazard the token model carries.** The
retry closes it while the client's socket is alive, because a repeated `Join` from the same endpoint is
answered with the same session. It does not close the case where the client is torn down between the
host issuing a token and the client storing it: that client returns with no token, is given a *second*
slot, and the first is held indefinitely by §2 — at two slots, a match that is full with one player in it.
**The host refuses with `MatchFull` rather than dropping**, so this surfaces as a message instead of a
hang, and the operator restarts a host that was going to be restarted anyway.

**Nothing reaches a client before it joins.** The host sends snapshots to sessions, so an unjoined
endpoint sees nothing at all — which is correct, and is a change from M0, where any endpoint that sent a
command started receiving the world.

**Tokens do not repeat, across matches or across host runs** (amended 2026-09-24). This paragraph used to
say the opposite — "tokens repeat across matches on one host … the outcome anybody would want" — and the
mid-implementation review (B4) reproduced the live-lock that followed from it twice on the unmodified
session table. **A returning client keeps its side across a restart only if the seats survive the
restart**, which is a question about `BeginMatch` rather than about tokens; it is on the register as the
review's D8 (`OpenQuestions.md` Q70).

**This decision does not settle what a client draws while it is joining.** `Interface.md` §7's
reconnecting overlay is the specification and M1's interface steps build it; until then the client retries
in silence.

## Measurements

**No measurement. Both figures below are arithmetic on the record layouts above**, which is the whole of
what this decision costs on the wire:

- A `Join` is **12 bytes** — four of `Neuron::PacketHeader` and eight of token. It was 14 until ADR-024 took the
  header's two fragment fields out.
- A `JoinReply` is **23 bytes** — four of header, then 1 + 1 + 1 + 8 + 8. It was 24 until ADR-024 for the same
  reason, then 22 until M2.3 added the player count.

Neither is in ADR-003's datagram budget and neither can affect it: both are sent outside the snapshot
path, once per join rather than twenty times a second, and `Scripts/DatagramBudget.py` models the snapshot
and the command packet because those are the two that recur.
