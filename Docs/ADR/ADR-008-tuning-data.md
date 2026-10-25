# ADR-008 — Tuning data is JSON, in files the game and the model both read

Status: **accepted** · 2026-10-08
Owns: data-file-format, json-parser, tuning-data-loader, data-file-deployment

## Context

The game's numbers are read by two programs: the game, and `Tools/BattleModel.py`, the battle model they are tuned with. The game must not be able to disagree with the model that tuned them, so both read the same data. R14 rules out a third-party parser in the game. A Python script under `Tools/` may use anything, but Python's standard library is enough for every format considered.

The options are CSV with one file per table, JSON with a hand-written parser, Markdown tables in the design read by both the game and the model, and a C++ header generated from the data at build time.

## Decision

1. **Tuning data is JSON** (owner, 2026-09-30), strict RFC 8259 in UTF-8: no comments, no trailing commas, no NaN or infinity, and no member name twice in one object. The game reads it with `Neuron::ParseJson` in `NeuronCore/Json.h`, written for this repository. `Tools/BattleModel.py` reads it with Python's `json` module.
2. **The data lives in the executable's `OutpostCommander/Assets/` folder, one file per concern,** beside the converted meshes under `Assets/Models/`. `Tuning.json` holds the match's rules and every component, structure and research topic, with the footprint radii and turn rates in the hull and drive entries ([ADR-010](ADR-010-movement-and-pathing.md)). `Map.json` holds the map ([ADR-036](ADR-036-map-sectors.md), [ADR-072](ADR-072-seeded-placement.md)); `Matchups.json` the battle matchups ([ADR-083](ADR-083-battle-matchups.md)); `Opponent.json`, `OpponentEasy.json` and `OpponentHard.json` the AI's settings ([ADR-020](ADR-020-ai-and-match-flow.md), [ADR-065](ADR-065-ai-difficulty.md)); `Camera.json` the camera's ([ADR-012](ADR-012-rts-camera.md)); and `Models.json` the meshes' sets and sizes ([ADR-011](ADR-011-meshes-and-shading.md)). The ADR named with a file owns what it holds.
3. **The file is the only copy** (owner, 2026-09-30). A number the game or the model needs is in a data file and nowhere else: the living design keeps the reasons and links the generated Catalogue (`Docs/Reference/Catalogue.md`) for the numbers. `Tools/BattleModel.py` reads the file (`--tuning`, which defaults to `OutpostCommander/Assets/Tuning.json`), so the model and the game read the same bytes.
4. **The shape.** The top level is an object of named sections. Each list is an array of objects, and an entry that others refer to has an `id`: a whole number of 1 or more, unique in its list. References use that `id`, as `requires`, `structureWeapon` and a research effect's `weapon`, `unlockHull`, `unlockDrive` or `unlockWeapon` do. A structure's `kind` is spelled as its `Outpost::StructureKind` enumerator. Member names are identifiers and follow AGENTS.md §1: camelCase, a unit in the name where there is one (R6), and US spelling (R11). A `name` is display text, so it keeps the design's spelling, as in `"Defence gun"`.
5. **`Outpost::LoadTuning` in `GameLogic` is the game's reader, and it is strict.** The server owns the rules ([ADR-002](ADR-002-authoritative-server.md)), so the loader is in the server's library. It rejects a missing member, a member it does not know, a wrong type, a fraction where the game counts whole numbers, a value out of range, a repeated `id`, a reference to nothing, a structure kind missing or listed twice, and a research topic that requires itself. The message names the place, such as `hulls[1].armor`.
6. **Numbers are held as the file states them.** Hit points, armor, damage, Ore and percentages are integers; rates, times, distances and factors are doubles. Converting them into simulation units, such as seconds into ticks, belongs to the simulation and to its determinism rules ([ADR-009](ADR-009-deterministic-core.md)). The parser converts a number with `std::from_chars`, which rounds correctly, so every build reads the same double from the same text.
7. **The data files ship as deployment content of the executable,** under `Assets\` in the package, in every configuration and on both platforms, so the game's data and its meshes sit in one folder. `CreateInProcessServer` reads `Tuning.json`, `Map.json` and `Matchups.json` from the package's `Assets` folder. The dedicated server reads its own copies, which its build puts in an `Assets` folder beside it ([ADR-078](ADR-078-dedicated-server.md) decision 1). `GameLogicTests` and `Tools/BattleModel.py` read the repository's copies in `OutpostCommander/Assets/`.

## Consequences

- **A bad edit fails CI.** `TuningTests` loads `OutpostCommander/Assets/Tuning.json` and compares every field the loader produces with the member of the same name in the file, so a typo, a trailing comma or a dangling reference turns the build red. The model is less strict: it ignores members it does not use. The test is therefore the check that the file is valid for the game.
- **The file has no comments.** The reasons behind the numbers live in the [living design](../OutpostCommander.md), and the reasons behind the shape live here.
- **Diffs are noisier than CSV.** To keep them readable, each hull, drive, weapon and structure is one line of the file, and each research topic two, so a changed number is a one-line diff.
- **The parser is this repository's to maintain.** `JsonTests` covers the grammar, the escapes and surrogate pairs, the depth limit, and the error positions. It reads the bytes of a string as they are and does not check that they are valid UTF-8.
- **The client cannot include the loader.** `GameApp` cannot include `GameLogic` ([ADR-002](ADR-002-authoritative-server.md)). The ship designer shows a design's stats on the client, so it needs component numbers. The server sends them in each snapshot, with the player's research applied, and the tuning types stay here ([ADR-017](ADR-017-research-and-the-designer.md)).

## What this forecloses

- Comments, trailing commas and the other JSON5 or JSONC extensions in the data files.
- A third-party JSON library in the game (R14), and a code generator in the build.
- Numbers that the design or the code keeps as a second copy. A number that the game or the model needs is in a data file.
