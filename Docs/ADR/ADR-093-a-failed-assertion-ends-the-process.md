# ADR-093 — A failed assertion names where it failed and ends the process at once, in every configuration

Status: **accepted** · 2026-10-11
Owns: assertions, fail-fast, exception-vs-assertion

## Context

`NeuronCore/Debug.h` holds the engine's assertions: `ASSERT` and `ASSERT_TEXT`, checked in every configuration, and `DEBUG_ASSERT`, `DEBUG_ASSERT_TEXT` and `DEBUG_WARNING`, checked only in Debug. Every assertion calls `Fatal` when it fails. A failed assertion says the code is wrong, and it has to stop the program there, with its message, in every configuration:

- `Fatal` is declared `noreturn`. If it returns, a failed assertion is undefined behavior, which the optimizer may turn into anything, rather than a check.
- A process with no debugger attached that breaks into one ends the way a crash does, so stopping cannot depend on a debugger being there.
- The message has to reach someone: an exception whose constructor drops its text, such as MSVC's `std::exception(const char*)`, loses it.

An assertion does not throw `Neuron::Exception`. That type means that a file, a message or a peer is bad, and the code that catches it reports that and carries on:

- `InProcessServer` refuses a peer whose handshake throws one, as a malformed message.
- `WinMain.cpp` reports one from receiving snapshots as a lost connection.
- The loaders of tuning, maps, matchups, settings and join tickets report one as a bad file, and `UniverseFolder` skips a save that throws one as a save it refuses.

A failed assertion is none of these. Any exception type has two further problems. The tree marks much of its code `noexcept`, and an exception that reaches one ends the process through `std::terminate`, without its message. And MsQuic's callbacks in `QuicChannel.cpp` catch everything and return `QUIC_STATUS_INTERNAL_ERROR`.

## Decision

1. **`ASSERT` and `ASSERT_TEXT` are checked in every configuration**; `DEBUG_ASSERT`, `DEBUG_ASSERT_TEXT` and `DEBUG_WARNING` in Debug only, and in Release their arguments are not evaluated. An assertion that holds evaluates its expression once and formats nothing.
2. **A failed assertion names where it failed and what**: `ASSERT` writes its file and line, then the expression as written; `ASSERT_TEXT` writes its file and line, then the text its format string makes. The text is UTF-8, as `Neuron::Exception`'s `std::string` is; there is no wide form. A format string is checked when it compiles.
3. **Then the process ends, and nothing can catch it.**
   - `Neuron::Fatal` formats the text and calls `Neuron::FailFast`, in `Debug.cpp`. Should the formatting itself throw, out of memory, it fails with a fixed text instead, so `Fatal` keeps its `noexcept`.
   - `FailFast` writes it to the debugger's output and, when the process has one, to standard error: the dedicated server's console. Then it calls `__fastfail(FAST_FAIL_FATAL_APP_EXIT)`.
   - With a debugger attached, the debugger stops there. Without one, Windows Error Reporting takes the process as it stands, at the failing frame.
   - No destructor runs, and no `catch` sees it. This holds in Debug and Release alike, on x64 and ARM64.
4. **`Neuron::Exception` stays what is wrong with what the program was given**, which it reports and survives. A broken invariant is never thrown.

## Consequences

- **A player sees no message.** The game closes, as it would on a crash, and the message is in the debugger's output if anything was listening. The failures the game reports in a dialog are the ones that reach `ReportFailure`: exceptions.
- **An assertion that fails on the in-process server's thread ends the game**, the client with it. The server's failures that are exceptions reach the client's report.
- **No test can watch an assertion fail**, since the test host would end with it. `NeuronCoreTests.DebugTests` holds what a passing assertion does: its expression checked once, its text never formatted, and a `DEBUG_ASSERT` checked only in Debug. It also makes MSVC compile the macros and `Fatal` under `/W4 /WX`, which nothing else in the tree calls.
- **An assertion's file and line are in the binary**, as the build named the file: a full path under MSBuild.

## What this forecloses

- An assertion that throws, or that any handler can catch, without a new decision.
- An `ASSERT` compiled out of Release: that is what `DEBUG_ASSERT` is for.
- A broken invariant reported as `Neuron::Exception`.
