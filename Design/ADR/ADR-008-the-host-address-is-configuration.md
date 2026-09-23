# ADR-008 — The host address is configuration; there is no discovery

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

The game is server-authoritative (`AGENTS.md` R19) with no lobby and no matchmaking, so a client needs
exactly one piece of information to play: where the host is. **There is no keyboard** (R21), so nobody can
type an address, which leaves the address as configuration, as something discovered on the network, or as
something the game draws a keypad for.

## Decision

**The host address is a configuration value with a compiled-in default of `127.0.0.1`.** There is no
discovery, no broadcast, no multicast probe and no address-entry interface.

**It is read from a one-line text file in the package's `LocalState` folder**, falling back to the
compiled-in default when the file is absent or unreadable. `ApplicationData::Current().LocalFolder()` is
the application's own storage and needs no capability. This is what makes the value *configuration* rather
than a constant: pointing a deployed package at a different host is dropping a file, not a rebuild and a
repackage — which matters because **the development machine and the target device are not the same
machine** and the Surface Pro will need a LAN address from the first day it is used.

**The package declares `privateNetworkClientServer`.** It is the capability for inbound and outbound
traffic on home and work networks, it is what Microsoft names for LAN games, and **on Windows it does not
grant internet access.** Going live therefore also requires `internetClientServer`, which supersedes
`internetClient` and need not be declared alongside it.

## Consequences

**`127.0.0.1` works only under a loopback exemption, and that exemption is not a shipping configuration.**
This is the trap in this ADR and it is written out rather than left to be met:

- `Server` is an ordinary Win32 console executable and therefore **unpackaged**. The manifest's
  `LoopbackAccessRules`, which lets two *packaged* applications talk over loopback, is not available to
  this pair at all.
- What is left is `CheckNetIsolation.exe LoopbackExempt -a -n=<PackageFamilyName>`, which Microsoft
  documents as **"only possible for sideload or debugging scenarios where you have local access to the
  machine, and you have administrator privileges."**
- **Visual Studio grants it automatically on every F5 deploy**, which is the danger: localhost will work
  for the whole of development and will not exist for anybody else. `AGENTS.md` §3 already says to remove
  the exemption and try again before concluding that same-machine play works, and this is the ADR that
  says why it matters.
- If UDP replies to a bound socket turn out to need the inbound form, `-is`, then
  **`CheckNetIsolation.exe` must stay running the whole time** the packaged application is listening. That
  would make the single-machine loop bad enough that two machines become the answer immediately. Which
  form is needed is not known and is M0's to establish.

**So the real configuration is two-valued**: `127.0.0.1` for the fast F5 loop on the development machine,
and a LAN address the moment anything is tested on the Surface Pro. Both are the same code path and the
same file, which is the point.

**What this forecloses:** playing on any network the address was not configured for. There is no way for a
player to join a host they were not handed, and **no route to Microsoft Store distribution at all**, since
a Store build has no loopback exemption and no way to be given an address. Neither is an MVP goal.

**What would reopen it:** wanting anyone other than the developer to play, which needs either the drawn
numeric keypad this decision declined — perhaps eighty lines — or real matchmaking.

## Measurements

**THE FIRST ONE IS CLOSED. 2026-09-23, at M0.5's third run.**

**`-a` alone is enough, and the single-machine loop is usable.** Measured on the Surface Pro 11 with a
`Release|ARM64` `Server` on `127.0.0.1` and the package registered from the same build, seed 20260922,
the client launched from the shell rather than by F5, and three runs of the same setup:

| Exemption | Client | Host |
|---|---|---|
| As Visual Studio left it | `JOIN accepted` on the first attempt, an update every tick, `lost=0` | `clients=1` |
| **Removed** (`-d`) | 19 join attempts in 20 seconds, **no datagram received** | `clients=0`, `updates=0` — **no join ever arrived** |
| **`-a` only**, no `CheckNetIsolation.exe` left running | `JOIN accepted` on the first attempt, 95 datagrams, `lost=0` | 599 ticks, 0 abandoned, 547 updates, 1 join |

**The exemption blocks the client's own outbound datagrams**, not only replies, since the host never saw
a join. And a reply to the client's bound socket needs nothing more than `-a`, so the inbound form `-is`,
and the listener process it would have to keep running, is not needed. **The trap above stands
unchanged**: without the exemption, same-machine play does not work at all, and nothing but a developer
with administrator rights can grant it.

None is owed now:

1. ~~**Which loopback exemption form a UDP client actually needs**, `-a` alone or `-a` and `-is`, which
   decides whether the single-machine development loop is usable.~~ — **DISCHARGED**, above.
2. ~~**That the client reaches a host on another machine over the LAN** with `privateNetworkClientServer`
   and nothing else declared.~~ — **WITHDRAWN by the owner, 2026-09-23: this game is tested on one
   machine, the Surface Pro, and no second machine will be used.** The capability set is therefore still
   read from Microsoft's documentation and has never been run over a LAN. **What would reopen it** is the
   first time a host and a client sit on two machines, and then it is the first thing to try.
