# ADR-055 — Ticks wait on a high-resolution timer

Status: **accepted** · 2026-10-03

## Context

[ADR-025](ADR-025-server-thread.md) put the in-process server's ticks on a thread of its own. Between ticks the thread used to wait on a `std::condition_variable_any` until `Neuron::TickHost::UntilNextTick` said the next tick was due, and only a request to stop woke it early.

That wait had a cost. On Windows a timed wait ends on the system timer's tick, 15.6 ms apart by default, unless something in the process has asked for a finer one, and nothing in the tree does. So a tick could start up to about 16 ms late. The rate does not drift, because each wait is taken from the time that really passed, but a late tick delays its snapshot, and the client renders a tick behind the server (ADR-002 decision 5): at 20 Hz a tick is 50 ms, so a third of the client's margin could go to waiting for the system timer. A high-resolution waitable timer had been left until a measurement asked for it. This decision takes it now, as part of the server's performance work, without a measurement that asked for it: the cost is a few lines on the server's thread, and nothing else changes.

## Decision

1. **The server's thread waits on a waitable timer and on an event.** `InProcessServer::Run` creates a timer with `CreateWaitableTimerExW` and `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, and a manual-reset event. Before each wait it sets the timer to the time left until the next tick is due, from `TickHost::UntilNextTick` and the time the last ticks were taken at, rounded up to the timer's 100 ns, as a relative due time. It then waits with `WaitForMultipleObjects` on both. A due time that has passed already is not waited for, as the timed wait before did not wait for one.
2. **A request to stop signals the event.** A `std::stop_callback` on the `std::jthread`'s stop token sets it, so the server's destruction still ends the wait at once, and the thread then stops as before. A stop requested before the wait begins signals the event at once.
3. **A plain waitable timer serves where there is no high-resolution one.** Windows before 10 version 1803 refuses the flag; the thread then creates a timer without it, which wakes on the system timer's tick, as the condition variable did. Failing both, or failing to create the event, set the timer or wait, is a failure of the server's thread, kept and thrown again from `TakeTickTimings` (ADR-025 decision 5).
4. **The handles are `winrt::handle`s,** as the renderer's are (AGENTS.md R12), owned by the thread's function and closed when it returns. The stop callback is declared after the event, so that it is gone before the event is.
5. **When ticks are taken is unchanged.** After each wait the thread hands the time that has really passed to `TickHost::Advance`, which decides how many ticks are due, with the same catch-up cap (ADR-009 decision 4). A timer that ends its wait a little before the tick is due takes no tick, and the thread waits again for what is left.

## Consequences

- **A tick starts within the high-resolution timer's precision of when it is due,** rather than up to the system timer's 15.6 ms later. Nothing in the process changes the system timer's resolution, so nothing else on the machine runs differently.
- **Not measured yet.** How late ticks start, before and after, is for the owner's `--measure` run on the development machine, which logs every tick's timing from the frame loop (ADR-025). No figure is claimed here. The Linux container has no Win32, so this change was not built or run there: CI's build and `InProcessServerTests.RunsItsTicksOnItsOwnThread`, which starts a real server thread and destroys it, are its first test on Windows.
- **The tick rate, the catch-up cap and replays are unchanged.** Which tick an order lands on depended on wall time before and still does; the command log records it (ADR-009).
- **A started server holds two more kernel handles** for as long as its thread runs.

## What this forecloses

- Raising the system timer's resolution with `timeBeginPeriod` for the server's sake: it changes the whole machine's timer, and the high-resolution waitable timer makes it unnecessary.
- A busy wait or a spin before the due time, without a new decision.
