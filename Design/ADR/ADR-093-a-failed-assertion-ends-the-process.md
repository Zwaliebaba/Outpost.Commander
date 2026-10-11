# ADR-093 — A failed assertion names where it failed and ends the process at once, in every configuration

Status: **accepted** · 2026-10-11

## Context

`NeuronCore/Debug.h` came with the engine's other headers: `ASSERT` and `ASSERT_TEXT`, meant to be checked in every configuration, and `DEBUG_ASSERT`, `DEBUG_ASSERT_TEXT` and `DEBUG_WARNING`, checked only in Debug. On 2026-10-11 a fix to the Release build put the bodies of `DebugTrace` and of `Fatal`, which every assertion calls when it fails, under `#ifdef _DEBUG`. `Fatal` is declared `noreturn`, so in Release it returned from a function the compiler is told never returns: a failed `ASSERT` was undefined behavior, which the optimizer may turn into anything, rather than a check. In Debug, `Fatal` broke into a debugger that may not be there, which ends a process with no debugger the way a crash does, then threw `std::exception("Fatal Error")`, a constructor only MSVC has, which drops the message. Nothing in the tree called any of them yet.

The obvious repair, throwing `Neuron::Exception` with the message, would be wrong here. That type means that a file, a message or a peer is bad, and the code that catches it reports that and carries on:

- `InProcessServer` refuses a peer whose handshake throws one, as a malformed message.
- `WinMain.cpp` reports one from receiving snapshots as a lost connection.
- The loaders of tuning, maps, matchups, settings and join tickets report one as a bad file, and `WorldFolder` skips a save that throws one as a save it refuses.

A failed assertion is none of these: it says the code is wrong. Any exception type has two further problems. The tree marks much of its code `noexcept` (741 lines of the `.h` and `.cpp` files outside the test projects and `d3dx12.h` carry the word, counted with `git grep`), and an exception that reaches one ends the process through `std::terminate`, without its message. And MsQuic's callbacks in `QuicChannel.cpp` catch everything and return `QUIC_STATUS_INTERNAL_ERROR`.

## Decision

1. **`ASSERT` and `ASSERT_TEXT` are checked in every configuration**; `DEBUG_ASSERT`, `DEBUG_ASSERT_TEXT` and `DEBUG_WARNING` in Debug only, and in Release their arguments are not evaluated. An assertion that holds evaluates its expression once and formats nothing.
2. **A failed assertion names where it failed and what**: `ASSERT` writes its file and line, then the expression as written; `ASSERT_TEXT` writes its file and line, then the text its format string makes. The text is UTF-8, as `Neuron::Exception`'s is ([ADR-008](ADR-008-tuning-data.md)); there is no wide form. A format string is checked when it compiles.
3. **Then the process ends, and nothing can catch it.**
   - `Neuron::Fatal` formats the text and calls `Neuron::Fail`, in `Debug.cpp`. Should the formatting itself throw, out of memory, it fails with a fixed text instead, so `Fatal` keeps its `noexcept`.
   - `Fail` writes it to the debugger's output and, when the process has one, to standard error: the dedicated server's console. Then it calls `__fastfail(FAST_FAIL_FATAL_APP_EXIT)`.
   - With a debugger attached, the debugger stops there. Without one, Windows Error Reporting takes the process as it stands, at the failing frame.
   - No destructor runs, and no `catch` sees it. This holds in Debug and Release alike, on x64 and ARM64.
4. **`Neuron::Exception` stays what is wrong with what the program was given**, which it reports and survives. A broken invariant is never thrown.

## Consequences

- **A player sees no message.** The game closes, as it would on a crash, and the message is in the debugger's output if anything was listening. The failures the game reports in a dialog are the ones that reach `ReportFailure`: exceptions, as before.
- **An assertion that fails on the in-process server's thread ends the game**, the client with it. The server's failures that are exceptions still reach the client's report.
- **No test can watch an assertion fail**, since the test host would end with it. `NeuronCoreTests.DebugTests` holds what a passing assertion does: its expression checked once, its text never formatted, and a `DEBUG_ASSERT` checked only in Debug. It also makes MSVC compile the macros and `Fatal` under `/W4 /WX`, which nothing else in the tree calls yet. The failing path was run in the Linux container only, with gcc 13 and clang 18 against stand-ins for the four Win32 calls: it wrote `Main.cpp(19): name != nullptr && "a name"` and exited with the fast-fail's code, in both configurations.
- **An assertion's file and line are in the binary**, as the build named the file: a full path under MSBuild.

## What this forecloses

- An assertion that throws, or that any handler can catch, without a new decision.
- An `ASSERT` compiled out of Release: that is what `DEBUG_ASSERT` is for.
- A broken invariant reported as `Neuron::Exception`.
