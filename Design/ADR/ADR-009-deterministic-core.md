# ADR-009 — The simulation reproduces on the same build

Status: **accepted** · 2026-09-30

## Context

ADR-002 decision 8 asks that a match reproduce from a seed and a command log on the same build, so that a bug can be replayed, and it allows floats in the simulation because no cross-machine determinism is required. AGENTS.md R16 says that a simulation which replays is a deterministic core, whose rules are recorded in an ADR: no float where an integer will do, no unordered iteration that reaches the outcome, ticks as the only clock, and a pinned PRNG with a recorded seed. Implementation plan task 2.2 builds that core and the in-process server that runs it, and it asked what the replay test promises: the same build on the same platform, or also across x64 and ARM64 (ADR-003). The development machine is ARM64 and CI is x64.

The owner answered on 2026-09-30: the same build on the same platform.

## Decision

1. **A replay reproduces on the same binary.** The same executable, or the same test DLL, given the same seed and the same commands at the same ticks, reaches the same state, bit for bit. No promise is made across x64 and ARM64, nor between Debug and Release, which are different binaries. Float results may differ: x64 and ARM64 can disagree wherever float code reaches the standard library's math functions or DirectXMath, which are implemented separately for each (R16).
2. **So floats are allowed for continuous quantities**, as ADR-002 decision 8 already says: positions, headings and speeds. This is how R16's "no float where an integer will do" applies here. Under this promise a float gives the same result on every run of one binary, so an integer does no better for these. A quantity that is a count is an integer, including ticks, identifiers, Ore, and hit points, armor and damage as the tuning data holds them (ADR-008). In the simulation they count in hundredths, still as integers ([ADR-014](ADR-014-designs-and-combat.md) decision 3).
3. **The tick is the only clock.** `Neuron::TickHost` in `NeuronServer` is the one place wall time becomes ticks. `InProcessServer` hands it the wall time that passed, from the server's own thread or from `Advance` in a test, and it runs the ticks that are due. It keeps the remainder exactly, as nanoseconds times ticks per second, so the rate does not drift. The rate is `rules.tickHz` from the tuning data, 20 Hz.
4. **Catch-up is capped at five ticks per `Advance`,** 250 ms at 20 Hz. After a longer stall, such as a debugger break or a dragged window, the backlog is dropped and the simulation resumes at its own pace. It does not race to catch up, and the command log still says which tick each command was applied at, so a replay is unaffected.
5. **Randomness is `Neuron::Random`:** xoshiro256\*\* 1.0 seeded through SplitMix64, written in `NeuronServer`, rather than `<random>`. `<random>`'s engines are specified by the standard, but its distributions are not, so the same seed could draw differently under another standard library. The generator is owned by the simulation and is part of its state. `RandomTests` pins its output against an independent implementation and the published test vector. A cold weapon's first shot draws from it, and so does the Q2 check's random targeting ([ADR-014](ADR-014-designs-and-combat.md)). The executable seeds each match from the system clock and writes the seed to the debugger output, in Debug and Release.
6. **Order is defined wherever it reaches the outcome.** Entities are held in identifier order, which is creation order. At the start of each tick the server applies the commands that arrived since the last one, in the order players connected and then in the order each player sent them. It stamps each command with its connection's player, never the client's, and logs it with its tick, so the log replays in the same order. No unordered container is iterated in the simulation.
7. **The server ticks on a thread of its own** ([ADR-025](ADR-025-server-thread.md)). While the window is minimized, the frame loop wakes every 16 ms instead of sleeping until the next message, so the clients on it keep taking their snapshots.
8. **`Simulation` is the core, and `InProcessServer` is its host.** `Simulation` knows nothing of wall time, transports or clients. It takes a seed and a list of commands per tick, and it compares equal to another `Simulation` in the same state. `InProcessServer` owns it, the tick host, the connections and the command log. `CreateInProcessServer` loads the tuning data and the map from the package (ADR-008).

## Consequences

- **A bug report needs the seed, the command log and the build.** The log lives in the server's memory. Writing it to a file comes when a bug first needs it; until then, the seed in the debugger output and a test that replays the scenario do the job.
- **A replay made on the owner's ARM64 machine cannot be checked on CI's x64.** A replay test has to run the match and its replay in the same test run, as `ReplaysFromItsCommandLog` does.
- **A rejected command is invisible to the client.** `Simulation::Tick` returns a result per command, but the protocol has no message for it. A task that needs the player to see a rejection adds one.
- **A simulation without the tuning data,** as the movement and combat tests run, rejects the base's, research's and the designer's orders as not yet supported (ADR-016).

## What this forecloses

- Lockstep networking and replays shared between machines, without a new ADR that replaces floats in the core.
- `std::random_device`, `rand`, `<random>`'s distributions, wall-clock time and address-dependent values anywhere in `Simulation`.
