# Outpost Commander

A real-time strategy game for Windows: C++23, Direct3D 12, an authoritative simulation and a client
that is only ever a replica of it. One developer, a hobby project.

It is a hard fork of [*Frontier Commander*](https://github.com/Zwaliebaba/Frontier-Commander), taken
at its M1 state on 2026-09-20, and the fork exists for the application model. *Frontier Commander* is
a Win32 desktop executable over an `HWND`. **This is a packaged UWP application whose view is a
`CoreWindow`**, and the client holds no simulation at all.

## Where to start

| | |
|---|---|
| [`AGENTS.md`](AGENTS.md) | How code is written here — naming, layout, build settings, the standing rules. **Read this before generating a line.** |
| [`Design/README.md`](Design/README.md) | What the game is, and the index of every design document |
| [`Design/UwpMigration.md`](Design/UwpMigration.md) | The migration this fork exists for: the surface measured, the tree before and after, the risks ranked |
| [`Design/ADR/README.md`](Design/ADR/README.md) | Nineteen engineering decisions, one file each |
| [`tasks/`](tasks/) | The work, as directed acyclic graphs. `python3 Tools/CheckTaskDag.py --next tasks/<plan>.yaml` says what can start |

## The shape of it

Six libraries on two axes — which layer the code is, and which side it runs on:

| | shared | client only | server only |
|---|---|---|---|
| **engine**, `Neuron` | `NeuronCore` | `NeuronClient` | `NeuronServer` |
| **game**, `Outpost` | `GameShared` | `GameClient` | `GameLogic` |

Three executables over them: `OutpostCommander`, the packaged game; `OutpostHost`, the headless
host; and `OutpostCapture`, the headless capture CI drives, which arrives with `p1-uwp-shell/P4`. **`OutpostCommander` links no
`GameLogic`**, so the interest set that decides what a client may know is a boundary the linker
keeps rather than a convention inside one address space
([`ADR-019`](Design/ADR/ADR-019-the-client-never-simulates.md)).

## Building it

Visual Studio 2026, toolset `v145`, the Windows SDK, and the UWP C++ workload for the game itself.
Build through the solution, never a project file directly — output paths and cross-project include
directories are anchored on `$(SolutionDir)`, and MSBuild defines it only for a solution build.

```powershell
msbuild OutpostCommander.slnx /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo

python Build\CheckFormat.py           # clang-format, whole tree; --fix rewrites the offenders
python Build\CheckProjectFiles.py     # build shape, project registration, the layering table
python Build\RunClangTidy.py          # needs a Developer PowerShell
python Tools\CheckTaskDag.py          # every plan validates
```

The game is a packaged application: it is deployed and launched, not run from a shell, and it needs
a host somewhere on the network to have a match to join.
