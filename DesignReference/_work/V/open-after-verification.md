# After verification

Two independent agents checked the published canvas against the repository.

**V** checked 60 numbers, 26 screen elements, coverage of states and models, scope, colors and sources. Three checks passed, four were partial and one failed, with 17 failing items (`DesignReference/_work/V/V.md`).

**V2** re-checked the 17 after they were fixed (`DesignReference/_work/V/V2.md`). 16 now pass, and all 14 new figure spot checks are consistent.

## Fixed at the owner's request

The brief's rule sends a second failure to Open items, not to a third attempt. On 2026-10-25 the owner asked for both of these to be fixed anyway.

- **The Hulls table overflowed its artboard** (`ships-parts`, V item 10, failed twice). A table's header now breaks only where a camelCase word joins (`speedMeters·PerSecond`), and a cell breaks only a word that cannot fit. The table fits its 1,440 px board, and no word splits.
- **The minimap's sample marks were incomplete** (found by V2). Every in-match minimap now draws what the frame's panels imply: the player's Command Station, Shipyards, Research Lab, rigs and Relays, its warships and Constructors, and the enemy ships its alerts name. `derive_layouts.py` now fails a run whose minimap disagrees with its status panel or its alerts. In the victory frame, A4's wreck is shown salvaged, because no structure stands on a derelict until then (`Simulation.cpp:2126`; ADR-074).

## Fixed after V2

- **Sector outlines.** Held and guarded outlines were all drawn dashed. Only a sector that is cut off is dashed (`Hud.cpp:3569-3580`).
- **Escaped pipes.** A table's escaped pipe (`\|`) leaked into the code text on six boards.
