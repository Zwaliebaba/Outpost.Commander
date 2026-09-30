# ADR-008 — Tuning data is JSON, in files the game and the model both read

Status: **accepted** · 2026-09-30

## Context

Design §12 holds the game's starting numbers, and it said they "will be loaded from a data file". Until now the design document was the data: `Tools/BattleModel.py` read §12's hull, drive and weapon tables, and §8's research table, by their Markdown column headings. The game needs the same numbers, and it must not be able to disagree with the model that tuned them (implementation plan, task 3.1).

G6 asked for the format, and whether §12 keeps a copy. It also covers the map (2.3) and the provisional footprint radii and turn rates (2.4), so milestone 2 reads one format from its first task. R14 rules out a third-party parser in the game. A Python script under `Tools/` may use anything, but Python's standard library is enough for every format considered.

The options were CSV with one file per table, JSON with a hand-written parser, the §12 Markdown tables read by both the game and the model, and a C++ header generated from the data at build time. The owner chose JSON on 2026-09-30, and chose that §12 keeps no copy of the numbers.

## Decision

1. **Tuning data is JSON**, strict RFC 8259 in UTF-8: no comments, no trailing commas, no NaN or infinity, and no member name twice in one object. The game reads it with `Neuron::ParseJson` in `NeuronCore/Json.h`, written for this repository. `Tools/BattleModel.py` reads it with Python's `json` module.
2. **The data lives in `Data/` at the repository root, one file per concern.** `Data/Tuning.json` holds design §12's numbers and §8's research topics: the match rules, hulls, drives, weapons, structure weapons, structures and research. The map is its own file, `Data/Map.json` (2.3). The provisional footprint radii and turn rates join the hull and drive entries of `Tuning.json` (2.4).
3. **The file is the only copy.** Design §12 and §8 point to it and keep their prose: what each number is for, why it moved, and where it stands against the Q2 check. `Tools/BattleModel.py` reads the file (`--tuning`, which defaults to `Data/Tuning.json`), so the model and the game read the same bytes.
4. **The shape.** The top level is an object of named sections. Each list is an array of objects, and an entry that others refer to has an `id`: a whole number of 1 or more, unique in its list. References use that `id`, as `requires`, `structureWeapon` and a research effect's `weapon` or `unlockHull` do. A structure's `kind` is spelled as its `Outpost::StructureKind` enumerator. Member names are identifiers and follow AGENTS.md §1: camelCase, a unit in the name where there is one (R6), and US spelling (R11). A `name` is display text, so it keeps the design's spelling, as in `"Defence gun"`.
5. **`Outpost::LoadTuning` in `GameLogic` is the game's reader, and it is strict.** The server owns the rules (ADR-002), so the loader is in the server's library. It rejects a missing member, a member it does not know, a wrong type, a fraction where the design counts whole numbers, a value out of range, a repeated `id`, a reference to nothing, a structure kind missing or listed twice, and a research topic that requires itself. The message names the place, such as `hulls[1].armor`.
6. **Numbers are held as the file states them.** Hit points, armor, damage, Ore and percentages are integers; rates, times, distances and factors are doubles. Converting them into simulation units, such as seconds into ticks, belongs to the simulation and to its determinism rules (ADR-009). The parser converts a number with `std::from_chars`, which rounds correctly, so every build reads the same double from the same text.
7. **The data files ship as content of `GameLogic`,** linked as `Assets\Data\Tuning.json` and `Assets\Data\Map.json` in the same way `NeuronCore` and `NeuronClient` ship their licences, so they reach the package through the executable's project reference. `CreateInProcessServer` reads both from there (ADR-009). `GameLogicTests` reads the repository's copies.

## Consequences

- **A bad edit fails CI.** `TuningTests` loads `Data/Tuning.json` and compares every field the loader produces with the member of the same name in the file, so a typo, a trailing comma or a dangling reference turns the build red. The model is less strict: it ignores members it does not use. The test is therefore the check that the file is valid for the game.
- **The file has no comments.** The reasons behind the numbers live in design §12, and the reasons behind the shape live here. A number that needs a note gets one in §12.
- **The tables are no longer in the design.** Reading a number means opening the file. The "Tuned on" table in §12 records the tuning pass of 2026-09-30, including the values it chose, and where it and the file disagree, the file is what the game plays.
- **Diffs are noisier than CSV.** To keep them readable, each hull, drive, weapon and structure is one line of the file, and each research topic two, so a changed number is a one-line diff.
- **The parser is this repository's to maintain.** `JsonTests` covers the grammar, the escapes and surrogate pairs, the depth limit, and the error positions. It reads the bytes of a string as they are and does not check that they are valid UTF-8.
- **The client cannot include the loader.** `GameApp` cannot include `GameLogic` (ADR-002). The ship designer (5.2) shows a design's stats on the client, so it will need component numbers. Either the tuning types move to `GameProtocol`, or the server sends the numbers to the client. That choice is made in 5.2 and recorded there.

## What this forecloses

- Comments, trailing commas and the other JSON5 or JSONC extensions in the data files.
- A third-party JSON library in the game (R14), and a code generator in the build.
- Numbers that the design or the code keeps as a second copy. A number that the game or the model needs is in a data file.
