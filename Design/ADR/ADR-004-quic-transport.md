# ADR-004 — QUIC through MsQuic, for the network transport after the MVP

Status: **accepted** · 2026-09-30

## Context

ADR-002 shapes the code so that moving the server into its own process is a transport change. In the MVP, commands and snapshots cross a `LoopbackTransport`. When the dedicated server runs in a Windows Server container, a network `Transport` takes the loopback's place. Networked play is out of scope for the MVP (design §13). But the protocol and library that will carry it decide what the engine depends on, so the owner has chosen them now.

The traffic is of two kinds, commands and snapshots. QUIC carries both over one UDP connection: reliable streams, and unreliable datagrams (RFC 9221). A lost packet on one stream does not stall the others, and TLS 1.3 is built in. The owner has chosen QUIC. Which of the two each kind of traffic uses is ADR-060's: a snapshot holds its tick's events and is far larger than a datagram, so both go on one reliable stream.

The Windows SDK has no QUIC API. No `msquic*.h` is under either installed SDK (10.0.26100.0 and 10.0.28000.0), and `System32` has no `msquic.dll` (checked on the development machine on 2026-09-30). So QUIC needs a package. R14 as written rules that out, and this ADR is the exception to it.

MsQuic is Microsoft's QUIC implementation. It is MIT-licensed, and the GDK documentation describes it as a client/server transport for real-time game traffic. Its Schannel build uses the TLS stack Windows already has, so it brings in nothing else.

## Decision

1. **The network transport is QUIC, through MsQuic**: the package `Microsoft.Native.Quic.MsQuic.Schannel` 2.6.2, which is the Schannel build.
2. **The shell's matches use it, with the server still in the process.** The human and the AI reach the server inside the client over the loopback address. The headless runs and the tests that step the server keep ADR-002's `LoopbackTransport`. [ADR-060](ADR-060-quic-in-process.md) owns how.
3. **MsQuic lives in `NeuronCore`**, because both ends need it: the client connects and the dedicated server listens. The QUIC code in `NeuronCore` knows no game concept (R9). It moves messages. The `Transport` that carries commands and snapshots over those messages is game code, and so is their wire format (ADR-060).
4. **`msquic.h` is included only from `NeuronCore`'s `.cpp` files.** The package adds its include directory to `NeuronCore`'s include path and no other. A `NeuronCore` header that included `msquic.h` would therefore break every project that includes that header. The MsQuic headers include `<windows.h>`, so they come after `pch.h`, where `NeuronCore.h` sets the Windows macro family (AGENTS.md §4).
5. **It is restored through `NeuronCore/packages.config`** into `packages/`, which is not committed. CI restores it along with every other `packages.config` in the tree.

## Consequences

- **`NeuronCore` carries MsQuic to whatever links it.** The package's targets add `msquic.lib` to the linker of the project that imports them, but `NeuronCore` is a static library and has no link step. So `NeuronCore` merges `msquic.lib` into `NeuronCore.lib` (`Lib` `AdditionalDependencies`). It also lists `msquic.dll` and the package's `LICENSE` as content, which the build copies through the project reference into the executable's package. The executable names no package path, and the dedicated server will get the same through its own reference to `NeuronCore`. The package's targets also copy `msquic.dll` beside the libraries in `<Platform>\<Configuration>\`. That copy is harmless build output.
- **Every match loads `msquic.dll`.** The executable imports it, and so do `GameLogicTests`, because its tests play the server over QUIC, and `NeuronCoreTests`, which tests the channel itself (ADR-089). The x64 DLL is 548 KB.
- **The wiring was checked end to end on 2026-09-30,** in Debug and Release on both platforms. A temporary call to `MsQuicOpen2` in a `NeuronCore` `.cpp` compiled clean under the project's settings. The executable then imported `msquic.dll`, and run from its package layout, the call succeeded. The call was then removed.
- **Both platforms are covered.** The package ships `msquic.dll` and `msquic.lib` for x64 and ARM64, and the copies in those builds were checked with `dumpbin /headers`: `8664` and `AA64`. It also ships x86 binaries, which nothing here builds (ADR-003).
- **QUIC needs Windows 11 or Windows Server 2022.** QUIC requires TLS 1.3, and Schannel supports TLS 1.3 only from those versions (Microsoft Learn: *Protocols in TLS/SSL (Schannel SSP)*, and the platform dependencies of .NET's QUIC support). Every match uses QUIC, so the package's minimum is Windows 11 (`10.0.22000`, ADR-060). The dedicated server's container image is Windows Server 2022 or later.
- **The server needs a certificate.** TLS 1.3 authenticates the server, so the server needs a certificate and the client needs a way to trust it. The server makes a self-signed one, and the client pins its hash (ADR-060).
- **The licence travels with the DLL.** MsQuic is MIT-licensed (`LICENSE` in the package). The MSIX package carries that text as `Licenses\MsQuic.txt`, next to `msquic.dll`.
- **Q5 does not measure a network.** Q5 was measured in-process, before QUIC carried the match (design §3). A match over QUIC on the loopback address has not been measured against Q5's 150 ms. A real network adds latency that neither includes, and ADR-002 already says to measure that before a match crosses one.

## What this forecloses

- Another transport protocol or library, such as raw UDP with hand-written reliability, TCP, or another QUIC implementation, without changing this ADR.
- The OpenSSL build of MsQuic, which brings its own TLS library instead of using Windows'.
- Code outside `NeuronCore` that calls MsQuic, `msquic.h` in any header, and a package path in any other project.
