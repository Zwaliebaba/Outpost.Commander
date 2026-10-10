# ADR-043 — The HUD takes the windows' look, writes Ore one way, and keeps the production window's cards still

Status: **accepted** · 2026-10-03

## Context

The HUD had kept its look of milestone 3, while the windows of tasks 9.3 and 9.4 took the owner's mockup. On 2026-10-03 the owner shared a screenshot of a match and asked for a review of the screen as a game's interface. The review found these faults:

- **There were two visual styles.**
  - The windows have a dark navy body, corner brackets and the mockup's faces.
  - The Ore panel, the research line, the selection panel, the buttons and the minimap are translucent boxes, with Segoe UI at one size.
- **Ore was written two ways.** The HUD wrote "Ore 12,345", and the windows wrote "◆ 12345", without a separator.
- **The production window cut design names short.** At 480 units its cards are 212 wide, and a name was cut at 20 characters, so "Small+Ion+Mass Driver" read "Small+Ion+Mass Dr...".
- **Its queue was always five rows**, empty or not, and stood above the cards. A full queue and an empty one took the same room. A queue that put its rows under the cards could not have kept them there without moving the cards as it grew.
- **The selection panel was 560 units wide** for a selected structure's two lines.

The owner asked on 2026-10-03 for these to be fixed in the same change as the rings (ADR-042).

## Decision

1. **Every panel of the HUD takes a window's look**, without a title bar, since it does not move. That is the Ore, the status panel ([ADR-066](ADR-066-production-status-on-the-hud.md)), the hint, the match's end, the selection, the buttons, the minimap's square and the main menu. Each has a window's body and a bracket at each corner, and stays anchored where it was (design §12, ADR-015).
2. **Its text takes the windows' faces**: a panel's first line in the title face, its other lines in the name face, and figures in the figure face.
3. **Ore is written one way everywhere**: Ore's gem ([ADR-085](ADR-085-one-meaning-per-color.md) decision 3) and the figure grouped in thousands. This holds for the stockpile, a window's Ore box, a card's cost and a button's cost.
   - The stockpile drops the word "Ore".
   - It is written from the panel's left: the gem stays put and the figure grows to its right ([ADR-046](ADR-046-second-look-at-the-screen.md)).
4. **A button is a card.**
   - It has a card's face and edge, and its label in the name face.
   - Any cost shows as the gem and the figure at its right.
   - A button that cannot be pressed is dim on the field's color. One that cannot be pressed for a reason other than its cost says why in place of the cost (ADR-046).
5. **The selection panel is as wide as its longest line**, between 280 and 560 units, and stays centered.
6. **The production and research windows have their cards first and their queue under them.**
   - Research is 560 units wide. Production is as wide as leaves both windows side by side, clear of the designer, at the reference width: 478 units ([ADR-062](ADR-062-type-scale-and-contrast.md)). A design's name is cut short only where it would run past its card, as its font measures it ([ADR-061](ADR-061-measured-text.md)).
   - The cards stay where they are as the queue grows, so a card can be clicked again and again.
   - The queue shows a row for each job and no empty ones. Its label still counts the jobs against the limit, "QUEUE · 2 / 5".
   - The window grows at its foot.
7. **The minimap draws an ore asteroid in Ore's gold, darkened**, so that blue is the player's, red the enemy's and gold is ore. A ship is a small square, a structure a larger one, a rock at its size, a run-dry asteroid in rust, and the camera's view the outline of the ground the screen shows, its four corners joined ([ADR-015](ADR-015-ui-drawing.md) decision 4). A structure the player only remembers is a cross over the fog, and the territory's borders and nodes are [ADR-081](ADR-081-territory-and-memory-over-the-fog.md)'s. A field's darkness, the least size of an ore asteroid's mark and the order the marks are drawn in are ADR-046's. An ore asteroid's mark is an outline, and the field's and the dry asteroid's colors and the minimap's size are [ADR-068](ADR-068-interface-polish.md)'s.

## Consequences

- **The widths are measured** with the fonts the text is drawn in, and [ADR-061](ADR-061-measured-text.md) owns how.
- **`HudTests` check it:**
  - `LaysTheHudOutInTheWindowsLook`
  - `WritesOreOneWay`
  - `KeepsTheCardsStillAsTheQueueGrows`
  - `FitsTheSelectionPanelToItsLines`
  - `DrawsFieldsDarkerThanOreAsteroidsOnTheMinimap`, which now compares brightness and checks the gold. A gold has less blue than the field's gray, so "darker" can no longer be per channel.
  - `ShowsTheIncome` finds the income by its text rather than its place in the list.
  - `LaysTheDesignerOutAsAWindow` no longer expects the HUD to draw no sprites. It checks that none of the HUD's lie in the window.
- **It is not run on Windows.** The HUD's tests ran in the Linux container in a standalone build, and every screen was drawn from `Hud::Lay`'s output with substitute fonts.

## What this forecloses

- The MVP's look for the HUD, without a new decision.
- A production or research window whose cards move as its queue changes.
