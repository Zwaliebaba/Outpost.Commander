# ADR-025 — The in-process server runs its ticks on a thread of its own

Status: **accepted** · 2026-10-08
Owns: server-thread, loopback-transport, server-failure-reporting, server-thread-ownership

## Context

A tick must not hold up a frame, and a frame must not hold up the ticks. A tick's cost varies with what happens in it: the ticks that order large fleets, and those of a large fight, cost the most ([Budgets](../Budgets.md) holds what a tick costs), and on the frame loop's thread each of them would hold up the frame it fell in. The owner asked for the server to run on a thread of its own (owner, 2026-10-02).

## Decision

1. **`Server::Start` is in the interface.** Started, `InProcessServer` runs its ticks on a `std::jthread` of its own. The shell connects both players, over QUIC ([ADR-060](ADR-060-quic-in-process.md) decision 1), then starts the server, and never steps it. A headless run steps a server it never starts ([ADR-038](ADR-038-phase-one-match-log.md)). The thread stops and is joined when the server is destroyed, which is when the shell leaves a match.
2. **The thread sleeps until the next tick is due.** `Neuron::TickHost::UntilNextTick` says how much more wall time makes a tick due. The thread waits that long on a waitable timer, high-resolution where Windows has one, and only a stop request ends the wait early ([ADR-055](ADR-055-high-resolution-tick-timer.md)). It then hands the time that has really passed to `TickHost`. The rate stays exact over a match, and a stall drops the backlog beyond 5 ticks (ADR-009).
3. **`InProcessServer::Advance` stays, for tests.** It steps a server that has not been started, on the test's own thread. Once the server has started, `Advance`, `Connect` and a second `Start` throw.
4. **The loopback transport locks.** Each `LoopbackChannel` has a mutex that guards its command and snapshot queues. The client's `Send` and `Receive` take it, and so does the server's tick, briefly, to take the commands and to add each snapshot. A player on QUIC has its commands added under the same lock by MsQuic's thread, and its snapshots go to MsQuic instead (ADR-060). A snapshot is built before the lock is taken. The tick durations have a lock of their own. Nothing holds two locks at once.
5. **A failure on the server's thread reaches the frame loop.** An exception the thread meets is kept, and the thread stops. `TakeTickTimings`, which the shell calls every frame, throws it again on the frame loop's thread. The shell reports it there as it reports any other error.
6. **What the server touches is the server's.** Match setup (`SimulationState`, `MapData`, `StartStressLoad`, `UseUniverse`) happens before `Start`. After it, the simulation is touched only by the server's thread, so `Simulation` has no locks (ADR-009). A universe's save is encoded on the server's thread, between two ticks, and only its bytes cross to the universe folder's own thread, which writes them ([ADR-077](ADR-077-universe-state-on-disk.md)). The players the server hosts, a universe's AI empires and deputies, are touched only by the server's thread too, after each tick ([ADR-079](ADR-079-seat-controller-and-deputy.md)).

## Consequences

- **A slow tick does not hold up a frame.** It delays the snapshots for the ticks behind it, and not a frame.
- **The frame's CPU work does not include the ticks.** The tick durations are timed on the server's thread, and logged with `--measure` from the frame loop.
- **The match runs while the frame loop does not.** Holding the window's title bar, or any other stall of the frame loop, does not pause the simulation. Snapshots queue for the client meanwhile, and the AI, which runs on the frame loop as a client (ADR-020), answers them late.
- **A replay does not depend on the thread.** Which tick an order lands on depends on wall time. The server logs the tick it applied each command at, and a replay from the seed and that log runs on any thread (ADR-009).
- **Checked in the Linux container.** `InProcessServerTests.RunsItsTicksOnItsOwnThread` starts a real server thread. It sends a hundred orders from the test's thread while the server ticks, then checks that a snapshot arrives for every tick in order, that the orders were applied, and that nothing arrives once the server is gone. The whole of `GameLogicTests`' server tests ran under ThreadSanitizer (g++ 13, `-fsanitize=thread`) with no report. CI runs it under MSVC without a sanitizer.

## What this forecloses

- Locks in `Simulation`. Only the server's thread touches a started simulation.
- A match's AI on the server's thread. It is a client (ADR-002) and stays on the shell's frame loop, as the human's client does. A universe's AI empires and its players' deputies, whose server runs apart from any client, are played on the server's thread ([ADR-079](ADR-079-seat-controller-and-deputy.md)).
- Stepping a started server by hand, and connecting a player to one.
