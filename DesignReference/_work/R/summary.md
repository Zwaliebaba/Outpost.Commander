# R — reconciliation summary

Written by the lead from R's report, because the harness refused R's own write of this file. The full entries are in `DesignReference/_work/conflicts.md`, `gaps.md` and `glossary.md`.

## Counts

| What | Count |
|---|---|
| Conflicts | 68 |
| — code-vs-intent (all OWNER?=yes) | 5 |
| — stale-doc | 32 |
| — superseded | 28 |
| — agent-disagreement | 3 |
| Gaps | 79 |
| — not specified or open | 58 |
| — on screen, with no rule another agent explains | 2 |
| — a rule with nothing on any screen | 19 |
| Glossary terms | 201 |
| Horizon and self-play leak check | **PASS** (14 hits read, none a leak) |

## OWNER?=yes

Each changes what a screen shows and may be a code defect rather than a stale document. In every case the reference shows what the code draws.

- **C-001: HUD side colors.**
  - The code draws the minimap marks, sector wash and node figures in fixed own and enemy colors, `OWN_COLOR` `#A0D3FF` and `ENEMY_COLOR` `#FFA689` (`Hud.cpp:26-27, 3477, 3508`).
  - The world uses `Models.json`'s side colors by player number: Human `#76ADF9` and Tarkan `#F38961` (`Models.json:53-56`).
  - So a player in seat 2 sees its own forces red-orange in the world but blue on the minimap.
  - Question: is the HUD meant to be relative (own and enemy), or should it follow the side colors?
- **C-002: Human Command Station size.**
  - The mesh is drawn about 181 m deep on its 90 m footprint at every level (A6).
  - ADR-064 says "level 1 is square", which predates the owner's mesh replacement (commit `09638bc`).
  - Question: is the overhang intended, or is the mesh or its footprint wrong?
- **C-003: Designer Build row.**
  - The row runs about 13 units into the footer: `ExtentOf` reserves 6 stat rows, but a Sensor Array design always draws 7 (`Hud.cpp:1087, 772-777`).
  - This is derived from the code; nobody has seen it on screen.
  - Question: a layout defect to fix?
- **C-004: Lit footprint ring color.**
  - The code draws a lit ring in the side's ship-line color (`GameClient.cpp:1246, 1376`).
  - ADR-042 says the structure's own line color.
  - Question: is the code intended (so ADR-042 is stale), or a defect?
- **C-005: Large hull card numbers.**
  - "1,200 HP · ARM 14 · 25m/s" may be cut short at 1080p in Cascadia Mono: about 200 px against 196 units (A4 open item 3).
  - This is unproven and needs a look on a Windows screen.

## Lead's notes, as R settled them

- **Structure levels: a gap (G-014), not a conflict.** The data gives 5 levels to the Command Station, 3 to the Shipyard and 4 to the Research Lab, against L1–L5 meshes for all three. Phase 3 gate K3 stops the Shipyard at L3 and the Lab at L4 on purpose.
- **Command Station gun hardpoints: stale-doc (C-006).** The meshes have 3, 3, 3, 6, 6; the docs say 4 rising to 12. The owner's own art came after the docs.
- **State versions.** Protocol 15 and world-state 6 are superseded in the Phase 5 plan (C-037) and stale in ADR-064, -071, -081 and -084 (C-008). The code has 1 and 1, as AGENTS.md R18 requires.
- **Look plan LV2.1.** Its "awaiting CI" is stale-doc (C-036): PR #108 merged it.
- **Leak check note.** `--quiet` and `--ai-matches` are named only as developer switches; the code's comments tie them to ADR-063, the self-play probe.

## The owner's answer (2026-10-25)

Asked in the session, as the brief's phase 2 requires, the owner answered:

- **C-001, C-002 and C-004:** left open. Each stays an open owner decision, and the reference draws what the code does.
- **C-003 and C-005:** drawn and flagged. The designer frames show the Build row's overlap and the cut-short Large hull numbers as the code derives them, marked "derived, not seen".
