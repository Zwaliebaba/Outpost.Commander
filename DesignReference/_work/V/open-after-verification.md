# After verification

Two independent agents checked the published canvas against the repository.

**V** checked 60 numbers, 26 screen elements, coverage of states and models, scope, colors and sources. Three checks passed, four were partial and one failed, with 17 failing items (`DesignReference/_work/V/V.md`).

**V2** re-checked the 17 after they were fixed (`DesignReference/_work/V/V2.md`). 16 now pass, and all 14 new figure spot checks are consistent.

## Left open

Two things stay open. The brief's rule is that a second failure on the same check goes here, not into a third attempt.

- **The Hulls table overflows its artboard** (`ships-parts`, V item 10, failed twice). The table runs about 150 px past the 1,440 px board, so its last three columns, Unlocked by, Mesh lengthMeters and Source, spill past the edge. The first fix let every table cell break mid-word. That split words and numbers across the whole canvas, so it was reverted. A fix limited to the header cells of that one table is still to do.
- **The minimap's sample marks are incomplete** (found by V2). The frames' minimaps leave out the player's own Shipyards and Research Lab, even a selected one, and most of its warships. The geometry and colors of the marks that are drawn are right. This is a gap in the sample content of `DesignReference/_work/A4/derive_layouts.py`, not in the game.

## Fixed after V2

- **Sector outlines.** Held and guarded outlines were all drawn dashed. Only a sector that is cut off is dashed (`Hud.cpp:3569-3580`).
- **Escaped pipes.** A table's escaped pipe (`\|`) leaked into the code text on six boards.
