# ADR-016 — Package identity: the two directories, and how the game is started

**Status:** Accepted
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

A packaged application does not live where an executable lives and is not started the way an executable is started, and `Neuron::Paths` and `Outpost::ParseCommandLine` were both written on the assumption that it does and is.

`Paths` answers four questions — the executable's directory, `GameData`, `Mods`, and the user's directory — and it answers them from `GetModuleFileNameW` and the `LOCALAPPDATA` environment variable. Under the app container neither is the right answer: the install directory is read-only and reached through `Package::Current().InstalledLocation()`, the writable place is `ApplicationData::Current().LocalFolder()`, and `%LOCALAPPDATA%` resolves to a per-package redirection whose path nobody should be constructing by hand. `ADR-014` puts `Paths` in `Core`, which is a desktop static library shared with three console executables that have no package identity at all, so the answer cannot simply be "call the WinRT API".

`ParseCommandLine` takes what `CommandLineToArgvW` split and recognises `--warp`, `--novsync` and `--capture <landscape> <ticks> <directory>`. **A packaged app has no command line in that sense.** Arguments reach it as a string on `LaunchActivatedEventArgs`, set by whoever activated it, which on a normal tile launch is empty.

And `.github/workflows/build.yml` drives the capture with `Start-Process -FilePath 'x64\Debug\OutpostCommander.exe' -ArgumentList '--warp', '--capture', 'Slice', '9000', 'Captures' -Wait`, reads `Captures\capture.log`, greps four assertions out of it and fails the job on the exit code. That gate is the subject of `ADR-014` and its answer is already decided; this ADR is where the three pieces meet.

## Decision

**`Core` keeps naming no path-producing API at all.** `Neuron::Paths` stops calling `GetModuleFileNameW` and `GetEnvironmentVariableW` and becomes what it already half was: a pair of directories set once at startup and read everywhere after. The test override it already carries (`Paths::OverrideForTests`) is renamed `Paths::SetRoots(_contentRoot, _userRoot)` and stops being about tests; `Paths::ClearRoots` stays for the suites that need to put it back.

**Each executable supplies its own two roots, because each knows how to.**

| Executable | Content root | User root |
|---|---|---|
| `OutpostCommander` (packaged) | `Package::Current().InstalledLocation().Path()` | `ApplicationData::Current().LocalFolder().Path()` |
| `OutpostCapture`, `OutpostHost` (console) | the executable's directory, from `GetModuleFileNameW` in that executable's `Main.cpp` | `%LOCALAPPDATA%\OutpostCommander`, likewise |
| the six test suites | the fixture roots they already set | as today |

**This is the whole of how `Core` stops being platform-coupled**, and it is worth saying why it is the answer rather than a `#ifdef`: the two Win32 calls were never about *what* the game needed, they were one way of finding out, and the right owner of that question is the process that knows what kind of process it is. Nothing in `Core` gains a WinRT include, nothing gains a family macro, and the `platform-header` rule of `ADR-001` extends to `Core` for these APIs. `Build/CheckProjectFiles.py` gains the grep and keeps it returning nothing.

**Launch options come from `LaunchActivatedEventArgs::Arguments()`, split by the same parser, and only `--warp` and `--novsync` remain.** `--capture` is gone from the packaged executable entirely: it belongs to `OutpostCapture` (`ADR-014`), and a flag that cannot be reached is worse than no flag. In Visual Studio the arguments come from the project's **Command Line Arguments** debugger property, which the packaged debug launch passes through; there is no supported way to hand them to a tile launch and none is invented.

**`GameData` ships in the package.** The tables, models, textures, terrain palettes and landscape definitions are `Content` items in `OutpostCommander.vcxproj` with `DeploymentContent` set, so they land beside the manifest and are read from `InstalledLocation`. They are read-only there, which they already were in practice.

**`Mods` moves to the user's directory.** `ADR-001` put it beside the executable, which is now inside the package and unwritable by anything including the game. `Paths::ModsDirectory()` becomes `UserDirectory() / "Mods"`. This is a change to what M3's mod loading will see and it is made now, before anything reads it, rather than discovered then.

**The capture gate does not change shape.** CI keeps `Start-Process ... -Wait -PassThru`, keeps the four log assertions verbatim, keeps the exit codes, and changes one word: the executable is `x64\Debug\OutpostCapture.exe`. The `--validate` gate on `OutpostHost` is untouched.

**A second gate is added: the package builds.** `msbuild` over the solution produces the `.msix`, and the job asserts the file exists and that the manifest is the one in the tree. It is not run — running a packaged app on a hosted runner needs deployment and developer mode, and `AGENTS.md` §3 already says a green build says nothing about whether the game draws. It catches a manifest regression and an import the container forbids, which is what a build gate can honestly catch.

## Consequences

**The game can no longer be started from a shell with arguments, and the one workflow that needed to be has its own executable.** Anything else that grows a need for a flag gets a console executable or a file in the user's directory, never a protocol activation invented for the purpose.

**Uninstalling the package deletes the user's directory**, because `LocalFolder` is inside the package's app data. Saves, replays and logs go with it. That is the app model's behaviour and not a defect, and it is named here because it will surprise someone: a save the player wants to keep has to be exported, and M2, which is where save and resume land, is where that becomes a design question rather than a note.

**`%LOCALAPPDATA%\OutpostCommander` and the package's `LocalState` are different directories**, so a capture run and a packaged run do not share a log. That is correct — they are different programs — but a log path quoted in a bug report now has to say which.

**What would reopen it.** The `broadFileSystemAccess` restricted capability, if the game ever needs to read a file the player picked from an arbitrary path; that is a capability declaration, a store justification and a privacy prompt, and it is not taken for mods.

## Measurements

**Not measured; read from documentation and from this tree.** `Package::Current().InstalledLocation()` and `ApplicationData::Current().LocalFolder()` are the documented package-relative locations. That `LaunchActivatedEventArgs::Arguments()` carries the debugger's command-line arguments through a packaged debug launch is **the claim in this ADR most likely to be wrong in detail**, and `p1-uwp-shell/P6` proves it before `--warp` is relied on; if it does not hold, `--warp` moves to a file in `LocalState` read at startup, which is a smaller change than it sounds because only two flags remain.

The one figure from this tree: `Neuron::Paths` is **113 lines** and makes **three** Win32 calls (`GetModuleFileNameW` once, `GetEnvironmentVariableW` twice), counted at `ec702e2`. After this ADR it makes none.
