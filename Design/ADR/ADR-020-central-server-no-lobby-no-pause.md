# ADR-020 — A central authoritative server: no lobby, and no pause

**Status:** Accepted; amends `Design/GameDesign.md` §12 (the lobby), `Design/Interface.md` §5, §6, §10 and §12 ruling 9 (the pause menu), and supersedes part of [`ADR-013`](ADR-013-uwp-application-model.md) on which key leaves the game
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

[`ADR-019`](ADR-019-the-client-never-simulates.md) took the simulation out of the client and put the host on another machine. That settles where the code runs; it leaves two things the old arrangement supplied and the new one does not, and neither survives contact with a **central, authoritative, long-running server** — which is what the owner decided on 2026-09-20 that the host is.

**The lobby.** `GameDesign.md` §12 read: "A host opens a match; players join by address… The lobby sets the landscape, the base, power and technology levels, the victory condition, alliances, which seats are AI with which personality and difficulty, and which mods are enabled." Every one of those is a field of `MatchSettings`, described in its own header as "the lobby's values, fixed before the first tick". With a local host the player who started the match chose them. With a central server there is nobody at the keyboard to choose.

**The pause.** `Interface.md` §10 gave F10 a menu with *Resume* and *Quit*, and was explicit about what it was: "It pauses the local host, which M1 may do because the host is a thread in the same process (`G1`); it is not a simulation state and no order carries it, so §11 notes it as the thing that must change before a match has a second human in it." `ADR-019` deleted that local host, so **that menu had already lost the thing it paused** before this ADR was written. §12 ruling 9 then deferred the *proper* version — "a pause that is part of the match rather than of the process" — to a later milestone.

**The proper version is entirely buildable, and this tree has already built one.** `m1-vertical-slice/G1b` notes it: "PAUSING FREEZES THE SIMULATION AND NOT THE HOST… `LocalHost` takes the shape a FINISHED `Sim` already had: the pacer is forgotten and no tick is run, while `Host::Advance` goes on publishing, acknowledging and counting heartbeats." That is precisely how an authoritative server pauses, and it is how *StarCraft II*, *Age of Empires* and *Company of Heroes* pause in multiplayer. **So this ADR is not recording an impossibility. It is recording a choice**, and it says so because a reason of the form "it cannot be done" is one that gets re-litigated by the first person who notices that it can.

## Decision

**There is no lobby. A server runs a match from its own configuration.**

`MatchSettings` is unchanged as a type — the simulation still takes it at construction, every snapshot and replay still carries it whole ([`ADR-003`](ADR-003-snapshot-and-replay-formats.md), [`ADR-009`](ADR-009-content-in-the-simulation.md)). What changes is where the values come from: **a configuration file beside `OutpostHost`, read at startup and validated by the same machinery the content tables go through** ([`ADR-006`](ADR-006-content-format.md)) — fail-fast, a line on every diagnostic, and a bad server configuration refused the way a bad table is refused. A server with no valid configuration does not start.

**A joining client chooses nothing.** It connects by address and the host hands it a free human seat; `Net::Host::FreeSeat` already does exactly this, so no protocol work is owed. The client's content hash must match the host's or it is refused, which is unchanged.

**There is no pause, of either kind.** The F10 menu loses *Resume* and becomes the **quit menu** — Back, Leave match, Quit — and the world goes on running behind it, because the world is not in this process. `Interface.md` §12 ruling 9 loses its third clause: the twenty-first order kind is still owed for rally points and design deletion, and a pause is not among the things it will carry.

**Escape never exits, and F10's menu is the only way out.** [`ADR-013`](ADR-013-uwp-application-model.md) said Escape's last press leaves; that contradicted `Interface.md` §7, where the last press swaps the pointer's mode, and `NeuronClient/PointerMode.h`'s `EscapePeel` enumeration, which `NeuronClientTests` pins. **`Interface.md` wins**: Escape peels an armed order, then a modal panel, then swaps the mode, and never exits. `ADR-013` is corrected.

## Consequences

**What the lobby's removal costs is choice, and it costs the player all of it.** No picking a map, a size, a victory condition, an ally or an opponent's difficulty. A player joins what is running. For a hobby project with one developer and a server the owner runs, that is a simplification with no loser; for anyone else it is the difference between a game and an installation, and it should not be reversed by accident later — a lobby is a protocol, a screen and a state machine, and re-adding one is a new ADR.

**What its removal buys is more than it costs.** The settings become a file that can be diffed, versioned and validated, instead of a screen whose state has to be replicated to every joiner before the first tick. The whole lobby phase of the protocol — propose, agree, ready, start — does not need designing, and `Net`'s message set stays what `ADR-012` fixed it at. And seat assignment, the one part that genuinely had to be host-side, is already written.

**What the pause's removal costs.** A player cannot stop for a phone call, and in a game where a heavy device crosses a Frontier landscape in six and a quarter hours (`GameDesign.md` §3), that is a real ergonomic loss. The answer is that a commander can leave and rejoin — `m3-multiplayer` already owes drop, grace and rejoin — and the AI holds the seat meanwhile. **That is a worse answer than a pause for one player and a better one for the other seven**, which is the whole argument.

**What it buys.** No twenty-first order kind for a pause and no wire change for one. No question about who may pause, how long for, how many times, or what happens when a paused server is the only server. And no denial-of-service lever: on a shared persistent server, a client that can stop the world can stop it for everybody, and the mitigation for that is a vote, a quota and a timeout — three mechanisms that exist only to make a feature safe that nobody needed.

**What this ADR does not answer, and names rather than leaves.** **What a server does when a match ends.** `CheckVictory` decides, the match-end overlay shows `VICTORY` or `DEFEAT`, and `Interface.md` §10 says "the simulation stops advancing once decided". A central server that stops advancing and then does nothing is a server that needs restarting by hand between games. Whether it restarts the same configuration, reads a new one, or idles until told is the next decision, and it is `m1-vertical-slice/N2`'s to raise.

## Measurements

**No figure here is measured and none needs to be**; this is a decision about the shape of the game rather than about the machine, and the alternatives were not rejected on performance.

Counted on this tree at `e612797`, to establish how much of this is deletion and how much is re-specification: **there is no lobby code and no pause code**. Every `lobby` hit in `.h` and `.cpp` is either a test helper named `Lobby()` that builds a `MatchSettings`, or a comment calling a field "a lobby setting"; every `paus` hit is either a production line paused at the device cap — **a game mechanic that stays** — or an input-trigger constant named `PAUSE` in `Tests/NeuronClientTests/InputSubscriptionTests.cpp` that stands for any key at all. The pause menu itself is drawn by `Hud.cpp` and driven by `App.cpp`, both of which `p1-uwp-shell/P3` moves and which lose the menu in the same commit that deletes `LocalHost`, because it is the same fact twice.

The design surface is the opposite proportion: **50 `lobby` references and 21 `paus` references across `Design/` and `tasks/`** when this ADR was written. The normative documents were changed with it — `GameDesign.md` §2 and §12, `Interface.md` §5, §6, §10 and §12, `TechnicalDesign.md` §3 and §11, and `MatchSettings.h` — and the **open** plans followed in the same pass: `m2-skirmish/T6` became the server's match configuration instead of a lobby screen, and `m3-multiplayer/T4` became joining a running server by address. **`m1-vertical-slice` and `tasks/Archive/m0-foundation` keep their prose**, because both are finished work and a plan is a record of what was built as much as an instruction for what to build; `Design/README.md` carries the map for reading them.
