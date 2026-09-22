# Outpost Commander

A multiplayer-only real-time strategy game for Windows, in the lineage of *Warzone 2100*: C++23, an
authoritative simulation on a host, and a client that is only ever a replica of it. One developer, a
hobby project.

**The client is a packaged UWP application** whose view is a `CoreWindow`. **The host is an ordinary
Win32 console executable.** There is no single-player mode and no offline mode: a client with no host
on the network has nothing to show.

## The state of it

**There is a match, and you can look at it.** M0 is complete and M1 is built as far as its interface:
the host places two stations 12,000 units apart on a seed both sides derive, builds ships and spends
credits for them, and the packaged client joins, draws three authored hulls, and lets a tap select a ship,
a double tap take every ship of its design in a circle, and a drag move the camera with the ground stuck
to the finger.

| | |
|---|---|
| **The numbers** | Fixed point at eight fractional bits, the vector over it, the binary angle, a 4,096-entry sine table and a PCG32 seeded from the match — all integer, because a simulation that cannot reproduce from its seed cannot be replayed |
| **The wire** | The packet header, the snapshot, the command packet and the join, encoded and decoded in full. A 110-entity snapshot measures **1,137 bytes** against the 1,232 pinned, so it is one datagram |
| **The simulation** | A component catalog, designs derived from it, a build system, ring slot assignment so fifty ships ordered to one point do not stack — and a **determinism test**: a two-minute scripted match hashes to `0x37f846ed90b74ca1` identically on x64 and ARM64, Debug and Release |
| **The session** | A client is told which player it is, keeps a session token, and gets its slot back on reconnect; a command from an endpoint the host never seated is refused |
| **The frame** | A flip-model swap chain at the panel's **physical** pixels, a scene target fitted into it, and three CMO hulls drawn as three instanced calls with per-instance team colour — **1,482 microseconds** a frame on a Surface Pro |

**595 tests** across six suites, run on all four configuration and platform pairs.

**What is not here yet**: the sky, the glyph atlas, the text renderer and the four panels — so there are
no readouts, no build buttons and nothing on screen but the world. Mining, modules and combat are M2 and
M3. **Two things are measured only on one machine**: tap-to-visible latency, and that the package carries
its content on an install that did not build it.

## The shape of it

Six libraries on two axes — which layer the code is, and which side it runs on:

| | shared | client only | server only |
|---|---|---|---|
| **engine**, `Neuron` | `NeuronCore` | `NeuronClient` | `NeuronServer` |
| **game**, `Outpost` | `GameCore` | `GameClient` | `GameLogic` |

Two executables over them: `OutpostCommander`, the packaged client, built from `GameClient` and
`NeuronClient`; and `Server`, the authoritative host, built from `GameLogic` and `NeuronServer`.
**`OutpostCommander` links no `GameLogic`**, so what a client may know is a boundary the linker keeps
rather than a convention inside one address space.

`NeuronCore` and `GameCore` are shared-items projects (`.vcxitems`) rather than libraries, because
the client half of the tree is compiled for the Windows Store application type and the server half is
an ordinary desktop build — one static library cannot be both, and shared items compile the same
sources correctly for each side.

One suite per library, under [`Tests/`](Tests/), each an ordinary desktop test DLL.

## Where to start

| | |
|---|---|
| [`AGENTS.md`](AGENTS.md) | How code is written here — naming, layout, build settings, the standing rules. **Read this before generating a line.** §2 is the project layout and why it is shaped this way. |
| [`Design/`](Design/README.md) | What is being built — the game, the technical design, the touch interface, the open questions and the ADRs. **Draft, but the decisions are ruled: twenty ADRs Accepted.** |
| [`.github/workflows/build.yml`](.github/workflows/build.yml) | What CI gates, and what it deliberately does not |

## Building it

Visual Studio 2026 with toolset `v145`, the Windows SDK, **and the Universal Windows Platform
development workload** — the client does not build without the last of those. Build through the
solution, never a project file directly: output paths and cross-project include directories are
anchored on `$(SolutionDir)`, and MSBuild defines it only for a solution build.

```powershell
msbuild OutpostCommander.slnx /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
```

x64 and ARM64, Debug and Release. **CI builds `Debug|x64` only** and runs the suites there, so the
other three pairs are checked by whoever builds them — see [`AGENTS.md` §3](AGENTS.md#3-build-and-verify).
The one NuGet
package — `Microsoft.Windows.CppWinRT`, referenced by the five projects on the C++/WinRT side —
restores from each of their `packages.config`, all pinned to the same version:

```powershell
Get-ChildItem -Recurse -Filter packages.config |
  ForEach-Object { nuget restore $_.FullName -PackagesDirectory packages }
```

Running the tests, and checking formatting before a push, are both in
[`AGENTS.md` §3](AGENTS.md#3-build-and-verify).

The client is a packaged application: it is deployed and launched rather than run from a shell, it
needs developer mode, and it needs a host somewhere on the network to have a match to join — **a
packaged client cannot reach a host on the same machine** without a loopback exemption.

**Where it looks for that host is a one-line file**, `host.txt` in the package's `LocalState`, with
`127.0.0.1` compiled in as the default ([`ADR-008`](Design/ADR/ADR-008-the-host-address-is-configuration.md)).
There is no discovery and no address entry, because there is no keyboard. Beside it the client keeps
`session.txt`, the token that gets its slot back on reconnect
([`ADR-013`](Design/ADR/ADR-013-a-client-is-told-which-player-it-is.md)), and `probe-log.txt`, which is
where every figure this project has measured on a device came from.

**It needs Windows 11 22H2 or newer to run**, which is `10.0.22621.0` in the manifest. That is not
caution: the shaders are compiled at Shader Model 6.7
([`ADR-012`](Design/ADR/ADR-012-a-shader-is-compiled-into-a-header.md)), which shipped in-box with that
release, so an older machine would install a client whose pipeline states cannot be created.
