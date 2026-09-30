# ADR-004 — QUIC through MsQuic, for the network transport after the MVP

Status: **accepted** · 2026-09-30

## Context

ADR-002 shapes the code so that moving the server into its own process is a transport change. In the MVP, commands and snapshots cross a `LoopbackTransport`. When the dedicated server runs in a Windows Server container, a network `Transport` takes the loopback's place. Networked play is out of scope for the MVP (design §13). But the protocol and library that will carry it decide what the engine depends on, so the owner has chosen them now.

The traffic is of two kinds. Commands must arrive. For snapshots only the newest one matters, and a lost snapshot should not hold up the next. QUIC carries both over one UDP connection: reliable streams, and unreliable datagrams (RFC 9221). A lost packet on one stream does not stall the others, and TLS 1.3 is built in. The owner has chosen QUIC.

The Windows SDK has no QUIC API. No `msquic*.h` is under either installed SDK (10.0.26100.0 and 10.0.28000.0), and `System32` has no `msquic.dll` (checked on the development machine on 2026-09-30). So QUIC needs a package. R14 as written rules that out, and this ADR is the exception to it.

MsQuic is Microsoft's QUIC implementation. It is MIT-licensed, and the GDK documentation describes it as a client/server transport for real-time game traffic. Its Schannel build uses the TLS stack Windows already has, so it brings in nothing else.

## Decision

1. **The network transport is QUIC, through MsQuic**: the package `Microsoft.Native.Quic.MsQuic.Schannel` 2.6.1, which is the Schannel build.
2. **The MVP does not use it.** In the MVP, client and server talk through ADR-002's `LoopbackTransport`. No QUIC code is written until the server moves out of process. The package is restored now so that the dependency, and the layer it lives in, are settled before any code needs them.
3. **MsQuic lives in `NeuronCore`**, because both ends need it: the client connects and the dedicated server listens. The QUIC code in `NeuronCore` knows no game concept (R9). It moves messages. The network `Transport` that carries commands and snapshots over those messages is game code. How commands and snapshots are serialised is its own ADR, written along with it.
4. **`msquic.h` is included only from `NeuronCore`'s `.cpp` files.** The package adds its include directory to `NeuronCore`'s include path and no other. A `NeuronCore` header that included `msquic.h` would therefore break every project that includes that header. The MsQuic headers include `<windows.h>`, so they come after `pch.h`, where `NeuronCore.h` sets the Windows macro family (AGENTS.md §4).
5. **It is restored through `NeuronCore/packages.config`** into `packages/`, which is not committed. CI restores it along with every other `packages.config` in the tree.

## Consequences

- **`NeuronCore` carries MsQuic to whatever links it.** The package's targets add `msquic.lib` to the linker of the project that imports them, but `NeuronCore` is a static library and has no link step. So `NeuronCore` merges `msquic.lib` into `NeuronCore.lib` (`Lib` `AdditionalDependencies`). It also lists `msquic.dll` and the package's `LICENSE` as content, which the build copies through the project reference into the executable's package. The executable names no package path, and the dedicated server will get the same through its own reference to `NeuronCore`. The package's targets also copy `msquic.dll` beside the libraries in `<Platform>\<Configuration>\`. That copy is harmless build output.
- **The MVP carries `msquic.dll` but never loads it.** A linker imports a DLL only for a function that is called, and nothing in the MVP calls MsQuic. So the package holds 548 KB (the x64 DLL) that it does not use until the network transport lands.
- **The wiring was checked end to end on 2026-09-30,** in Debug and Release on both platforms. A temporary call to `MsQuicOpen2` in a `NeuronCore` `.cpp` compiled clean under the project's settings. The executable then imported `msquic.dll`, and run from its package layout, the call succeeded. The call was then removed.
- **Both platforms are covered.** The package ships `msquic.dll` and `msquic.lib` for x64 and ARM64, and the copies in those builds were checked with `dumpbin /headers`: `8664` and `AA64`. It also ships x86 binaries, which nothing here builds (ADR-003).
- **Networked play needs Windows 11 or Windows Server 2022.** QUIC requires TLS 1.3, and Schannel supports TLS 1.3 only from those versions (Microsoft Learn: *Protocols in TLS/SSL (Schannel SSP)*, and the platform dependencies of .NET's QUIC support). The package's minimum version stays Windows 10 1809 (ADR-001) while nothing uses QUIC. When the network transport lands, either that minimum rises to Windows 11 (`10.0.22000`), or older Windows keeps offline play and is refused network play. That choice is made then. The dedicated server's container image is Windows Server 2022 or later.
- **The server needs a certificate.** TLS 1.3 authenticates the server, so the dedicated server needs a certificate and the client needs a way to trust it. In development that is a self-signed certificate. How the client trusts it is decided with the transport.
- **The licence travels with the DLL.** MsQuic is MIT-licensed (`LICENSE` in the package). The MSIX package carries that text as `Licenses\MsQuic.txt`, next to `msquic.dll`.
- **Q5 does not measure a network.** Q5 is measured in-process (design §3). A network adds latency that Q5's 150 ms does not include, and ADR-002 already says to measure that before a network transport lands.

## What this forecloses

- Another transport protocol or library, such as raw UDP with hand-written reliability, TCP, or another QUIC implementation, without changing this ADR.
- The OpenSSL build of MsQuic, which brings its own TLS library instead of using Windows'.
- A QUIC transport in the MVP. ADR-002 decision 6 and design §13 would both change first.
- Code outside `NeuronCore` that calls MsQuic, `msquic.h` in any header, and a package path in any other project.
