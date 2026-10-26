# ADR-004 — QUIC through MsQuic is the network transport

Status: **accepted** · 2026-10-25
Owns: quic-transport, msquic-package, connection-figures

## Context

[ADR-002](ADR-002-authoritative-server.md) makes the boundary between the server and its clients a `Transport`. A universe's players cross it from other machines ([ADR-078](ADR-078-dedicated-server.md) decision 9), so the protocol and library that carry commands and snapshots across it decide what the engine depends on.

The traffic is of two kinds, commands and snapshots. QUIC carries both over one UDP connection: reliable streams, and unreliable datagrams (RFC 9221). A lost packet on one stream does not stall the others, and TLS 1.3 is built in. Which of the two each kind of traffic uses is [ADR-060](ADR-060-quic-in-process.md)'s.

The Windows SDK has no QUIC API. No `msquic*.h` is under either installed SDK (10.0.26100.0 and 10.0.28000.0), and `System32` has no `msquic.dll` (checked on the development machine on 2026-09-30). So QUIC needs a package. R14 rules that out, and this ADR is the exception to it.

MsQuic is Microsoft's QUIC implementation. It is MIT-licensed, and the GDK documentation describes it as a client/server transport for real-time game traffic. Its Schannel build uses the TLS stack Windows already has, so it brings in nothing else.

## Decision

1. **The network transport is QUIC, through MsQuic** (owner, 2026-09-30): the package `Microsoft.Native.Quic.MsQuic.Schannel` 2.6.3, which is the Schannel build.
2. *(retired)*
3. **MsQuic lives in `NeuronCore`**, because both ends need it: the client connects and the dedicated server listens. The QUIC code in `NeuronCore` knows no game concept (R9). It moves messages. The `Transport` that carries commands and snapshots over those messages is game code, and so is their wire format ([ADR-060](ADR-060-quic-in-process.md) decision 3).
4. **`msquic.h` is included only from `NeuronCore`'s `.cpp` files.** The package adds its include directory to `NeuronCore`'s include path and no other. A `NeuronCore` header that included `msquic.h` would therefore break every project that includes that header. The MsQuic headers include `<windows.h>`, so they come after `pch.h`, where `NeuronCore.h` sets the Windows macro family (AGENTS.md §4).
5. **It is restored through `NeuronCore/packages.config`** into `packages/`, which is not committed. CI restores it along with every other `packages.config` in the tree.
6. **A channel reports how its connection does** (`QuicChannel::Statistics`, `QuicStatistics`): from `QUIC_PARAM_CONN_STATISTICS_V2`, MsQuic's smoothed and least round trip, the packets sent, those lost (suspected lost and not found again), and the congestion window; and the channel's backlog, the bytes of the messages it has handed MsQuic that MsQuic has not given back.
   - **MsQuic's send buffering is off on both ends** (`SendBufferingEnabled`). MsQuic then holds each message's buffer until the peer has acknowledged all of it, and only then reports the send complete, so the backlog is what has not reached the peer (MsQuic 2.6.3's `docs/Streams.md`, "Send Buffering"). With buffering on, as by default, MsQuic copies a message and completes it at once, and the backlog would count nothing. A channel holds each message's bytes until its send completes either way, so turning buffering off saves MsQuic's copy and holds nothing longer than the acknowledgement takes. MsQuic advises at least two sends pending, so that the connection does not go idle between one send's completion and the next; that is for a sender that waits for one to complete before the next, and a channel queues every message as it is given and waits for none.
   - The figures are read without the channel's lock, since MsQuic may wait for the connection's own thread, whose callbacks take it. So they are never read from a Receiver, and not while another thread closes the channel.
   - **The server reads each seat's connection once a second** (`SEAT_FIGURES_SECONDS`), after the tick, on the thread that steps it, and keeps the newest 3,600 samples (`SEAT_FIGURES_KEPT`) for `Server::TakeSeatFigures`, so that a server whose figures nobody takes, as the dedicated server's, holds no more than that. The shell logs them with `--measure`, and its own connections' figures beside them, once a second (`seat_net` and `client_net`), which `Tools/FrameTimes.py` summarizes; a universe's log carries them in its hour record ([ADR-082](ADR-082-universe-log-and-universe-run.md) decision 3).
   - MsQuic 2.6.3's network statistics event (`QUIC_NETWORK_STATISTICS`, `NetStatsEventEnabled`) is a preview API, and is not used.

## Consequences

- **`NeuronCore` carries MsQuic to whatever links it.** The package's targets add `msquic.lib` to the linker of the project that imports them, but `NeuronCore` is a static library and has no link step. So `NeuronCore` merges `msquic.lib` into `NeuronCore.lib` (`Lib` `AdditionalDependencies`). It also lists `msquic.dll` and the package's `LICENSE` as content, which the build copies through the project reference into the executable's package. The executable names no package path, and `OutpostServer` gets the same through its reference to `NeuronCore` ([ADR-078](ADR-078-dedicated-server.md) decision 1). A NuGet update moves `packages.config` and the targets' import to the new version but leaves that `Lib` line and the two content paths where they were, so a version bump repoints those three by hand. Adding the package to a project that links `NeuronCore` does not fix a missing `msquic.lib`: it puts a package path in another project, which this ADR forecloses. The package's targets also copy `msquic.dll` beside the libraries in `<Platform>\<Configuration>\`. That copy is harmless build output.
- **Every match loads `msquic.dll`.** The executable imports it, and so do `GameLogicTests`, because its tests play the server over QUIC, and `NeuronCoreTests`, which tests the channel itself ([ADR-089](ADR-089-test-dll-per-library.md)).
- **Both platforms are covered.** The package ships `msquic.dll` and `msquic.lib` for x64 and ARM64. It also ships x86 binaries, which nothing here builds ([ADR-003](ADR-003-x64-and-arm64.md)).
- **QUIC needs Windows 11 or Windows Server 2022.** QUIC requires TLS 1.3, and Schannel supports TLS 1.3 only from those versions (Microsoft Learn: *Protocols in TLS/SSL (Schannel SSP)*, and the platform dependencies of .NET's QUIC support). Every match uses QUIC, so the package's minimum is [ADR-060](ADR-060-quic-in-process.md) decision 8's, and the dedicated server's is [ADR-078](ADR-078-dedicated-server.md)'s.
- **The server needs a certificate.** TLS 1.3 authenticates the server, so the server needs a certificate and the client needs a way to trust it. The server makes a self-signed one, and the client pins its hash ([ADR-060](ADR-060-quic-in-process.md) decision 6); a universe keeps its certificate in its folder ([ADR-078](ADR-078-dedicated-server.md) decision 7).
- **The licence travels with the DLL.** MsQuic is MIT-licensed (`LICENSE` in the package). The MSIX package carries that text as `Licenses\MsQuic.txt`, next to `msquic.dll`.
- **The figures are what MsQuic says,** not what the game sees: a round trip is the transport's, without the server's tick or the client's frame. How late a state arrives and how far the client falls behind are measured over a relay that impairs the loopback (`ImpairedLink`, [ADR-060](ADR-060-quic-in-process.md)), and over a real network by the owner; their targets and their latest measurements are [`Docs/Budgets.md`](../Budgets.md)'s.
- **Tests:** `QuicChannelTests.ReportsItsConnection` checks a round trip once a message has crossed, the packets sent, and the backlog back to nothing once the peer has every message, on both ends; `QuicTransportTests.ReportsItsConnection` checks a seat's figures as the server takes them.

## What this forecloses

- Another transport protocol or library, such as raw UDP with hand-written reliability, TCP, or another QUIC implementation, without changing this ADR.
- The OpenSSL build of MsQuic, which brings its own TLS library instead of using Windows'.
- Code outside `NeuronCore` that calls MsQuic, `msquic.h` in any header, and a package path in any other project.
- MsQuic's send buffering, without a backlog that counts something else.
