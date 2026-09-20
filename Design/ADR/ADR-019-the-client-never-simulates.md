# ADR-019 — The client never simulates, and the game is multiplayer only

**Status:** Accepted; supersedes [`ADR-001`](ADR-001-solution-layout.md) on what `OutpostCommander` is built on, and changes what `GameDesign.md` §12 means by M1 and M2
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

[`ADR-018`](ADR-018-client-server-libraries.md) gives the tree a client side and a server side. It does not say whether the *client's process* may also run a host, and that is the decision with teeth: `OutpostCommander/LocalHost.cpp` exists precisely so that a single player gets a host on a worker thread over `LoopbackTransport`, and `m1-vertical-slice` — 41 of 43 tasks complete — proves "two commanders on a Small landscape build, design, research and fight to annihilation **over loopback**, the client a replica."

[`ADR-013`](ADR-013-uwp-application-model.md) then packages the client, and packaging brings a constraint that has no workaround inside this tree's premises. Microsoft's *Interprocess communication* documentation:

> To maintain security and network isolation, loopback connections for IPC are blocked by default for packaged applications. […] **Unpackaged applications and services don't have package identity, so they can't be declared in LoopbackAccessRules.** You can configure a packaged application to connect via loopback with unpackaged applications and services via `CheckNetIsolation.exe`, however **this is only possible for sideload or debugging scenarios** where you have local access to the machine, and you have administrator privileges.

So a packaged `OutpostCommander` cannot reach an unpackaged `OutpostHost` on the same machine in any shipping configuration. **LAN and internet play are entirely unaffected** — the isolation covers loopback and nothing else. Only same-machine play is reached, and it is reached whether the host is a second process or a thread in the first, because removing the in-process host is what raises the question at all.

Three answers were available, and the owner took the third:

- **Keep the host in-process**, behind the transport seam. Single player works, nothing is packaged twice, no capability is declared — and the interest set stays a convention in one address space, which is the thing `ADR-018` exists to fix.
- **Ship the host inside the same package** as a full-trust Win32 application, launched through `FullTrustProcessLauncher`, with `privateNetworkClientServer` and `uap4:LoopbackAccessRules` naming the package's own family. The boundary becomes genuine and same-machine play survives. It costs the `runFullTrust` **restricted** capability — Store submission with written justification, and a package containing a medium-integrity process, which is not a sandboxed application in any sense the owner meant by choosing one.
- **The client never simulates.**

## Decision

**`OutpostCommander` links no simulation, and there is no local host.** `LocalHost.cpp` is deleted rather than moved; a match is opened by an `OutpostHost` somewhere on the network and joined by the client. The client's process holds a replica, a renderer and an interface, and nothing that could answer a question about a commander it cannot see.

**The interest set becomes a boundary in the binary.** `ADR-012`'s security argument is now enforced by the linker: `GameLogic` is not in `OutpostCommander`'s reference set, `Sim.h` is not on its include path, and `Build/CheckProjectFiles.py` fails the build if either changes. That is the whole point of this ADR and of `ADR-018` beneath it.

**Two harnesses are allowed both sides, and neither ships.** `Tests/IntegrationTests` holds the three tests that prove the halves agree — convergence, the host endpoint, the interest set — because no other project may see both. `OutpostCapture` runs a whole match, client and host, in one process over `LoopbackTransport`, which is what keeps the CI capture gate alive. **Both are named in `BUILT_ON` by hand**, so that "everything" is a decision recorded in a table rather than a habit.

**The host runs on a separate machine, and that is the intended topology rather than a concession** (owner, 2026-09-20). The loopback isolation above is therefore not a constraint this design works around — it is a constraint the design never meets, because no shipping configuration puts a client and a host on one box. No `runFullTrust`, no `FullTrustProcessLauncher`, no packaged host, no invented protocol activation.

**Development is the one place loopback is used, and it is exempted rather than avoided.** A developer runs the client and `OutpostHost` on one machine: Visual Studio's *Allow Local Network Loopback* debugging property sets the exemption for a deployed debug package, and `CheckNetIsolation.exe LoopbackExempt -a -n=<packagefamilyname>` sets it by hand for a sideloaded one. **That path is a developer's and never a player's**, and nothing in the game is built as though a player had it. The trap to know about: Visual Studio adds the exemption on every F5 deploy, so a same-machine build works indefinitely on the machine that built it and on no other — `AGENTS.md` §3 says so where a developer will read it.

## Consequences

**A player with one machine cannot play this game**, and the owner's answer is that there is always a second machine: the host is a service that runs somewhere and the client joins it. That is a smaller claim than it sounds for a hobby project with one developer and it is a large one for anybody else, so it is written down. What it forecloses either way: single player against the AI, a skirmish while offline, a tutorial, and any first-run experience that does not begin with a host somewhere. `GameDesign.md` §12's milestone table means something different now — M1's "over loopback" is `OutpostCapture`'s in-process loopback and `IntegrationTests`', not a player's — and M2's save and resume become the *host's*, because the client has nothing to save.

**Two questions the topology opened, both since answered** (owner, 2026-09-20). The host runs on **Windows**, so `OutpostHost` stays MSVC and no port is owed — worth recording that R16 had already done the hard part had the answer gone the other way, since a simulation holding no floats hashes the same under any toolchain. And there is **no lobby**: [`ADR-020`](ADR-020-central-server-no-lobby-no-pause.md) makes the server central and authoritative, reading `MatchSettings` from its own configuration, and a joining client chooses nothing. That ADR names the one question still open — what a server does when a match ends.

**What it buys, and it is not only the security boundary.** A client that cannot simulate cannot *diverge* either: the whole class of defect where the client's copy of a rule drifts from the host's is unreachable, because there is no copy. The scripted AI, the economy, research, production and combat exist in exactly one binary. And the host becomes testable on its own terms — `OutpostHost` is now the only thing that runs a match, so a headless soak is a normal thing to run rather than a mode of the game.

**What would reopen it.** Two things and no others. A packaged host — the second option above — if the owner later decides `runFullTrust` is an acceptable price for same-machine play; that is a new ADR, a restricted capability, and a manifest with `LoopbackAccessRules` in it. Or a Windows release in which loopback isolation admits a packaged client and an unpackaged server, which would make the constraint disappear without changing anything else here.

**What this does not permit.** The client is not to gain "a little simulation" for prediction, smoothing or a preview. `GameClient/Interpolation` interpolates *records it was sent*; `GameClient/PlacementPreview` previews against rules that live in `GameShared` and are therefore the same rules the host validates with. If a client-side feature ever seems to need `Sim`, it needs a record it is not being sent, and the answer is in the protocol.

## Measurements

**The loopback constraint is documented, not measured**, and the documentation is quoted in full above rather than summarised. **It is no longer load-bearing**: the host runs elsewhere, so no shipping configuration depends on the answer either way. What `p1-uwp-shell/P1` still confirms is the *development* path — that the exemption works, and that a sideloaded package without one fails as documented, so that a same-machine build is never mistaken for a shipping one.

**Counted on this tree at `ffd797b`:** `OutpostCommander` links six libraries today and four after `P2`; `LocalHost.{h,cpp}` is 12.2 kB, and it is the only file that has to be deleted rather than moved. `GameLogic` is 51 files and `Tests/GameLogicTests` 44; none of either reaches the client's binary once `P2` lands, and the closing grep of `P11` is what says so.
