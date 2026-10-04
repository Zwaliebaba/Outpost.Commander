# ADR-025 — The in-process server runs its ticks on a thread of its own

Status: **accepted** · 2026-10-02

## Context

The server's ticks used to run on the frame loop's thread: `wWinMain` called `Advance` once a frame, and the loopback's queues were plain vectors. A slow tick delayed the frame, and a slow frame delayed the ticks.

Design §3 measured that cost on the development machine on 2026-10-01. The ticks that order both 100-ship fleets took 5.7–20.7 ms, and a few ticks in combat took 9–10 ms. Each one held up the frame it fell in, and a frame has 16.7 ms. The owner asked on 2026-10-02 for the server to move to a thread of its own.

## Decision

1. **`Server::Start` replaces `Server::Advance` in the interface.** Started, `InProcessServer` runs its ticks on a `std::jthread` of its own. The shell connects both players, over QUIC since [ADR-060](ADR-060-quic-in-process.md), then starts the server, and never steps it. A headless run steps a server it never starts ([ADR-038](ADR-038-phase-one-match-log.md)). The thread stops and is joined when the server is destroyed, which is when the shell leaves a match.
2. **The thread sleeps until the next tick is due.** `Neuron::TickHost::UntilNextTick` says how much more wall time makes a tick due. The thread waits that long on a waitable timer, high-resolution where Windows has one, and only a stop request ends the wait early ([ADR-055](ADR-055-high-resolution-tick-timer.md)). It then hands the time that has really passed to `TickHost`. The rate stays exact over a match, and a stall still drops the backlog beyond 5 ticks (ADR-009).
3. **`InProcessServer::Advance` stays, for tests.** It steps a server that has not been started, on the test's own thread, as every test before this did. Once the server has started, `Advance`, `Connect` and a second `Start` throw.
4. **The loopback transport locks.** Each `LoopbackChannel` has a mutex that guards its command and snapshot queues. The client's `Send` and `Receive` take it, and so does the server's tick, briefly, to take the commands and to add each snapshot. A player on QUIC has its commands added under the same lock by MsQuic's thread, and its snapshots go to MsQuic instead (ADR-060). A snapshot is built before the lock is taken. The tick durations have a lock of their own. Nothing holds two locks at once.
5. **A failure on the server's thread reaches the frame loop.** An exception the thread meets is kept, and the thread stops. `TakeTickTimings`, which the shell calls every frame, throws it again on the frame loop's thread. The shell reports it there as it reports any other error.
6. **What the server touches is the server's.** Match setup (`World`, `MapData`, `StartStressLoad`) happens before `Start`. After it, the simulation is touched only by the server's thread, so `Simulation` has no locks and stays as it was (ADR-009).

## Consequences

- **A slow tick no longer holds up a frame.** The slow ticks of design §3 now delay the snapshots for the ticks behind them. They do not delay a frame. Whether that removes the hitch the player saw at a big order has to be seen on the development machine.
- **Q4's frame CPU work no longer includes the ticks.** The figures of 2026-10-01 in design §3 include them, so a later run is not comparable without saying so. The tick durations are timed as before, and logged with `--measure` from the frame loop.
- **The match runs while the frame loop does not.** Holding the window's title bar, or any other stall of the frame loop, no longer pauses the simulation. Snapshots queue for the client meanwhile, and the AI, which still runs on the frame loop as a client (ADR-020), answers them late.
- **Replays are unchanged.** Which tick an order lands on already depended on wall time. The server logs the tick it applied each command at, and a replay from the seed and that log runs on any thread (ADR-009).
- **Checked in the Linux container.** `InProcessServerTests.RunsItsTicksOnItsOwnThread` starts a real server thread. It sends a hundred orders from the test's thread while the server ticks, then checks that a snapshot arrives for every tick in order, that the orders were applied, and that nothing arrives once the server is gone. The whole of `GameLogicTests`' server tests ran under ThreadSanitizer (g++ 13, `-fsanitize=thread`) with no report. With the lock removed from `LoopbackTransport::Send`, the same test reported 27 races, so the test can catch a missing lock. CI runs it under MSVC without a sanitizer.

## What this forecloses

- Locks in `Simulation`. Only the server's thread touches a started simulation.
- The AI on the server's thread. It is a client (ADR-002) and stays on the frame loop, as the human's client does, until clients and server are separate processes.
- Stepping a started server by hand, and connecting a player to one.
