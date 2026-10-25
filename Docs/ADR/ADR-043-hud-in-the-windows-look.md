# ADR-043 — The HUD takes the windows' look, writes Ore one way, and keeps the production window's cards still

Status: **accepted** · 2026-10-10
Owns: hud-panel-look, ore-figure, hud-button-look, selection-panel-width, queue-window-layout, minimap-marks

## Context

The HUD and the floating windows ([ADR-031](ADR-031-floating-windows.md)) share the screen, and the player reads them as one interface. The windows have a dark navy body, corner brackets and the faces of the owner's mockup ([ADR-030](ADR-030-typography-and-sprites.md)), so a HUD panel in any other style reads as a second interface laid over the first (owner, 2026-10-03).

- **Ore is the figure the player reads everywhere**: on the stockpile, in a window's Ore box, and on every card's and button's cost. It reads at a glance only if it is written one way.
- **A queue above its cards moves them as it grows.** A player who builds the same design again and again clicks the same card, and a card that moves under the pointer defeats that (owner, 2026-10-03).
- **A design's name is the player's own**, and is cut short only where its card cannot hold it.
- **The selection panel holds anything from a structure's two lines to a group's count and its bars** ([ADR-088](ADR-088-selection-and-commands.md) decision 4), and it stands over the world, so a fixed width is too wide for the one or too narrow for the other.
- **The minimap is a small square for a large map**, so its marks have to say whose a thing is and what it is at a glance, and the ore, which is what the player looks for, has to stand out from the rocks that are only in the way.

## Decision

1. **Every panel of the HUD takes a window's look**, without a title bar, since it does not move. That is the Ore, the status panel ([ADR-066](ADR-066-production-status-on-the-hud.md)), the hint, the match's end, the selection, the buttons, the minimap's square and the main menu. Each has a window's body and a bracket at each corner, and stays anchored to its corner or edge ([ADR-015](ADR-015-ui-drawing.md) decision 4), but for the buttons, which stand against the selection's right edge, the pair centered at the bottom ([ADR-088](ADR-088-selection-and-commands.md) decision 2).
2. **Its text takes the windows' faces**: a panel's first line in the title face, its other lines in the name face, and figures in the figure face.
3. **Ore is written one way everywhere**: Ore's gem ([ADR-085](ADR-085-one-meaning-per-color.md) decision 3) and the figure grouped in thousands. This holds for the stockpile, a window's Ore box, a card's cost and a button's cost.
   - The stockpile has no word "Ore".
   - It is written from the panel's left: the gem stays put and the figure grows to its right. The income keeps to the panel's right edge.
4. **A button is a card.**
   - It has a card's face and edge, and its label in the name face.
   - Any cost shows as the gem and the figure at its right.
   - A button that cannot be pressed is dim on the field's color. One that cannot be pressed for a reason other than its cost says why in place of the cost, such as the Research Lab's "ONE PER PLAYER" once the player has one; the reason is on the button rather than on hover, so it shows without the pointer. A button the player only cannot afford is dim with its cost, as the windows' cards are.
5. **The selection panel is as wide as its longest line**, between 280 and 560 units, and stays centered.
6. **The production and research windows have their cards first and their queue under them.**
   - Their widths are [ADR-062](ADR-062-type-scale-and-contrast.md) decision 6's. A design's name is cut short only where it would run past its card, as its font measures it ([ADR-061](ADR-061-measured-text.md)).
   - The cards stay where they are as the queue grows, so a card can be clicked again and again.
   - The queue shows a row for each job and no empty ones. Its label counts the jobs against the limit, "QUEUE · 2 / 5".
   - The window grows at its foot.
7. **The minimap's marks say whose a thing is by color, and what it is by shape and size.** A player's color is its set's in `Models.json`, the color the world draws its ships and structures in ([ADR-011](ADR-011-meshes-and-shading.md) decision 3), whichever seat views them: player 1's blue and player 2's orange-red (owner, 2026-10-25). Its marks, its sectors' washes, outlines and hatching, a memory's cross and its figures on the territory panel take it; GameClient hands the HUD each player's color (`Hud::SideColorsOf`), and a player the data gives no set is drawn in the neutral marks' gray. The two sets' colors stand at 7.3:1 and 6.8:1 on the windows' navy, computed as WCAG's ratio of the linear colors, so they need no shade of the HUD's own. Violet is the pirates' ([ADR-073](ADR-073-pirates.md)) and gold is ore. A ship is a small square, a structure a larger one, a derelict a light gray one ([ADR-074](ADR-074-salvage.md)), and a rock a square at its size; each is drawn at no less than its kind's least size, and an ore asteroid's at least 8 units across, the largest of the least sizes, since ore is what the player looks for. An ore asteroid is in Ore's gold, darkened, and one run dry in rust; a field is darker than an ore asteroid, though its square is the larger. The neutral marks are drawn first, so that a rig's mark shows over its asteroid's. How the marks are outlined and colored is decision 8's. The camera's view is [ADR-015](ADR-015-ui-drawing.md) decision 4's, a memory's cross [ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 4's, and the territory ADR-081 decision 7's.
8. **How the minimap's marks are drawn, and their contrast.**
   - An ore asteroid's mark, dry or not, is a square outlined 2 units wide in its color, so that it differs from a player's filled mark in shape as well as in brightness.
   - A field is (0.13, 0.13, 0.14) in linear color, at 2.1:1 against the map, darker than an ore asteroid.
   - A dry asteroid is (0.32, 0.20, 0.147), at 3.1:1 against the map, darker than one with ore.
   - The minimap is 300 units square.

   The ratios are WCAG's, of the linear colors over the minimap's ground, computed, not measured on a screen.

## Consequences

- **The widths are measured** with the fonts the text is drawn in, and [ADR-061](ADR-061-measured-text.md) owns how.
- **The minimap's ratios are computed as `HudTests` computes the text's**: WCAG's ratio over the minimap's ground, its frame and map over black.
- **`HudTests` check it:**
  - `LaysTheHudOutInTheWindowsLook`
  - `WritesOreOneWay`
  - `KeepsTheOreGemStillAsTheFigureChanges`
  - `ShowsTheIncome` finds the income by its text rather than its place in the list.
  - `OffersTheStructuresToConstructors` checks a button's reason in place of its cost.
  - `FitsTheSelectionPanelToItsLines`
  - `KeepsTheCardsStillAsTheQueueGrows`
  - `LaysTheDesignerOutAsAWindow` checks that none of the HUD's sprites lie in the window.
  - `DrawsFieldsDarkerThanOreAsteroidsOnTheMinimap` compares brightness and checks the gold. A gold has less blue than the field's gray, so "darker" cannot be per channel.
  - `DrawsARigsMarkOverItsAsteroid`
  - `SetsTheMinimapsMarksApart`
  - `DrawsEachPlayerInItsSetsColor` draws players 1 and 2 in their sets' colors as seat 1 and as seat 2: their marks, washes, outlines, hatching and memories, and their figures on the territory panel at 4.5:1 or more.

## What this forecloses

- A HUD panel in a look other than the windows', without a new decision.
- A production or research window whose cards move as its queue changes.
