# Outpost Commander

A multiplayer-only real-time strategy game for Windows, in the lineage of *Warzone 2100*: C++23, an
authoritative simulation on a host, and a client that is only ever a replica of it. One developer, a
hobby project.

**The client is a packaged UWP application** whose view is a `CoreWindow`. **The host is an ordinary
Win32 console executable.** There is no single-player mode and no offline mode: a client with no host
on the network has nothing to show.

## The state of it

**This is a shell, not a game.** The solution builds an empty client and an empty host across four
configuration and platform pairs, six test suites run, and every edge of the build — each project
reference, include path and link — is exercised by a function that returns its own name. There is no
simulation, no renderer and no protocol in it yet.

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
| [`Design/`](Design/README.md) | What is being built — the game, the technical design, the touch interface, the open questions and the ADRs. **Draft; nothing in it is settled yet.** |
| [`.github/workflows/build.yml`](.github/workflows/build.yml) | What CI gates, and what it deliberately does not |

## Building it

Visual Studio 2026 with toolset `v145`, the Windows SDK, **and the Universal Windows Platform
development workload** — the client does not build without the last of those. Build through the
solution, never a project file directly: output paths and cross-project include directories are
anchored on `$(SolutionDir)`, and MSBuild defines it only for a solution build.

```powershell
msbuild OutpostCommander.slnx /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
```

x64 and ARM64, Debug and Release; CI builds all four and runs the suites on x64. The one NuGet
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
