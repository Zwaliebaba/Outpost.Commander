# Leak check: Horizon and self-play material in A1–A7

Agent **R**. HEAD `a053fe294d91dc885bbb061c2f34d76ca6897738`.

## Verdict: **PASS**

Nothing from the Horizon (`OutpostCommander-Horizon.md`, `ImplementationPlan-Horizon.md`) or from self-play (`ImplementationPlan-SelfPlay.md`, `Design/SelfPlayNetwork.md`, ADR-063) is described as part of the game as built. The same holds for anything a plan lists as not built (Look plan LV2.2, LV2.3, LV3.1, LV4.1–4.3, and its "Not in this plan" list).

One note for the writer, at hit 4: two developer switches whose comments cite ADR-063.

## Method

**First pass.** `grep -n -i -E` over `A1/A1.md` … `A7/A7.md` for:
- `horizon`, `self-play`, `selfplay`, `self play`, `galaxy`, `season`, `--ai-matches`, `ADR-063`;
- `phase 6`, `phase 7`, `phase 8`;
- `LV2.2`, `LV2.3`, `LV3.1`, `LV4`, `beacon`, `fleet plate`, `plates over`, `paint slot`, `two-tone`;
- `SelfPlayNetwork`, `probe`.

**Second pass.** For `relay jump`, `forward shipyard`, `pathing by sector`, `upgrade steps`, `third seat`, `more seats`, `Tools/SelfPlay`, `AiMatchesOptions`, `network`, `neural` and `tone map`.

**Checking.** Every hit was read in context. Where the source said something specific, it was checked against the source: `OutpostCommander/WinMain.cpp:44-55`, `Design/ADR/ADR-038-phase-one-match-log.md:48`, `GameDesign/OutpostCommander-Phase5.md:55, 137, 145`, `Tools/BattleModel.py:1-10`.

## Hits

| # | File:line | Text (abridged) | Verdict |
|---|---|---|---|
| 1 | `A1/A1.md:90` | "Ore sink / bound in a world: Not decided: '… decided in Phase 6'" (`Phase5.md:137`) | **Legit mention.** It quotes the in-scope Phase 5 design's deferral, and the cell is marked "Not decided". Nothing of Phase 6 is described. |
| 2 | `A1/A1.md:582` | Open 4, the same deferral | **Legit mention**, as hit 1 (gap G-003). |
| 3 | `A2/A2.md:630` | "`GameLogic/Matchup.h:5` cites 'horizon §9', which is excluded; the in-scope source is ADR-083" | **Legit mention.** It flags a code comment and points to the in-scope ADR. Battle matchups are in scope: Phase 5 adopts Horizon §9's measurement by name (`Phase5.md:33, 145`; index.md). |
| 4 | `A3/A3.md:593` | "Developer switches, not part of play: `--measure`, `--load`, `--stress`, `--ai-matches`, `--quiet` and `--matchup` …" | **Legit mention, with a note.** The switches exist at HEAD (`WinMain.cpp:30-55`), and A3 lists them only as not part of play. `--ai-matches` is Phase 1's tool (`ADR-038:48`); its options and `--quiet` are commented as ADR-063's, the self-play probe (`WinMain.cpp:44-48`). **Writer's note:** keep both as bare developer-switch names. Do not describe what `--quiet` or `--ai-matches`' options are for, since that is self-play. |
| 5 | `A3/A3.md:718` | "H10, 'A galaxy or friends next', is decided from W6 and the week's log (`Phase5.md:55, 189`)" | **Legit mention.** An open gate of the in-scope Phase 5 design. Nothing is described as built. |
| 6 | `A4/A4.md:439` | "It is empty when a corner sees past the horizon." | **False positive**: the camera's geometric horizon. |
| 7 | `A5/A5.md:15` | "Excluded, as the brief says: Horizon, self-play, ADR-063, and Look plan LV2.2 and later." | **Legit**: a statement of the exclusion. |
| 8 | `A6/A6.md:34` | "Not built, so not drawn: paint slots, two-tone lines, beacons, fleet plates (LV2.2, LV2.3, LV3.1, LV4.1–4.3)." | **Legit**: stated as absent. |
| 9 | `A7/A7.md:16` | "Excluded, not read for content: Horizon, self-play, ADR-063." | **Legit**: a statement of the exclusion. |
| 10 | `A7/A7.md:76`, `A7/A7.md:645` | "40° below horizontal"; "half the horizontal" | **False positives.** |
| 11 | `A7/A7.md:124` | "Twinkle / parallax / galaxy: none (`ADR-022 §What this forecloses`)" | **Legit**: a starfield galaxy band that ADR-022 forecloses, not the Horizon's galaxy of systems. |
| 12 | `A7/A7.md:673` | "Tone map: none" (ADR-091 Consequences) | **Legit**: stated as absent. |
| 13 | `A7/A7.md:684-694` | "Not built at HEAD": LV2.2, LV2.3, LV3.1, LV4.1–LV4.3 and the Look plan's "Not in this plan", each `todo` or not built; beacons and plates "confirmed absent in code" | **Legit**: every row is stated as not built. R checked `GameDesign/ImplementationPlan-Look.md:42-53`. |
| 14 | `A3/A3.md:617` | "Network: … router and firewall" (`ADR-078` Consequences) | **False positive**: the dedicated server's network, not the self-play network. |

**No hits** for `self-play`, `SelfPlay`, `season`, `phase 7`, `phase 8`, `relay jump`, `forward shipyard`, `pathing by sector`, `upgrade steps`, `third seat`, `more seats`, `Tools/SelfPlay`, `AiMatchesOptions` or `neural`. The only `ADR-063` hits are the exclusion statements (hits 7 and 9).

## Related item outside A1–A7

`Tools/BattleModel.py`, which A2 cites as a secondary source for the damage formula, is the MVP's Q2 balance model (`Tools/BattleModel.py:1-10`, "for the balance check of the MVP design"). It is not Horizon or self-play material.
