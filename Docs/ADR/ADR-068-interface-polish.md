# ADR-068 — The windows' headers, what a production card says, and the Controls window

Status: **accepted** · 2026-10-05
Owns: window-headers, saved-design-chips, production-card, controls-window, key-bindings

## Context

A window's header has to read at a glance. A queue reads as a count against its room, and the ships built are a separate count. Arrows belong beside what they step. A window that pages has to say which page it shows, and a window's body has to hide the bright shapes of the world behind it. A production card is where a player picks what to build, so it has to say how long a design takes and what it is good against. And a key with no button, such as A, S, Alt, the control groups or the camera's keys, has to be taught somewhere in the game (owner, 2026-10-03; 2026-10-05).

## Decision

1. **The windows' headers.**
   - The designer writes its queue over its slots as the other windows write a queue, "QUEUE · 2 / 5", and its ships built apart, "BUILT · 4", beside the slots.
   - Arrows flank what they step: "< SHIPYARD 05 > · DESIGNER" in the designer's title bar, and "< SHIPYARD 05 >" on the production window's title line. The production window's step through the player's finished producers, the Command Station first and then the Shipyards by their numbers (`ProductionTarget`), and the designer's through its Shipyards by number, round the end.
   - An arrow's button is 24 units square, and so are the research window's and the saved designs' arrows.
   - The saved-design chips are 216 units wide, a little narrower than a part card ([ADR-062](ADR-062-type-scale-and-contrast.md) decision 4), so that the chips' arrows stand clear of the third chip.
   - Research says where its page is, "TOPICS · 11-20 OF" and the count of topics, with an ASCII hyphen ([ADR-030](ADR-030-typography-and-sprites.md)).
   - A window's body is opaque.
2. *(retired)*
3. **What a production card says.** A Shipyard's card has a third line under the code and cost:
   - The design's build time at the player's Shipyard speed, as the designer gives it.
   - A short bar for each hull, marked with the hull's initial, S, M and L. Each bar is three segments, lit as the designer rates the design's damage against that hull: three for Good, two for Fair and one for Poor. The bars read without their colors.
   - The Constructor's card has its build time; the snapshot carries the Command Station's time for a Constructor (`Snapshot::constructorBuildSeconds`).
   - A card is 74 units tall. A card whose hull is above the Shipyard's level says so in its code line.
4. **The Controls window.**
   - F1 opens and closes it in a match. It is a fourth `WindowKind`, in the windows' look, and opens in the middle of the screen. It lists every key and mouse action the game reads in a match, under three headings: selecting and orders, the camera, and the windows.
   - Every key the input code reads in a match is in `GameApp/KeyBindings.h`: the orders' and the control groups' in `PlayerControls`, the camera's in `Camera`, and the windows', Esc, Space and Alt in `GameClient`. The window's lines are made from those constants by `KeyBindings()`, so the window cannot name a key the code does not read.
   - The main menu has no line for the Controls window (owner, 2026-10-05).
5. *(retired)*

## Consequences

- **The production window is as tall as its cards.** With six designs, three lines of cards take 238 units.
- **The name field's typing keys are not listed.** Enter, Backspace and Esc edit a design's name while the field has the keyboard ([ADR-017](ADR-017-research-and-the-designer.md)). That is text entry, not a control of the match. The canvas's interaction takes Esc first while the field has the keyboard ([ADR-100](ADR-100-the-canvas.md) decision 3).
- **The tests.**
  - `HudTests.WritesTheWindowsHeaders`
  - `SaysWhatAProductionCardBuilds`
  - `ListsEveryKeyInTheControlsWindow`
  - `WindowManagerTests.OpensTheControlsAsAFourthKind`
  - `WireFormatTests` carries `constructorBuildSeconds`.
  - The layout tests of [ADR-061](ADR-061-measured-text.md) and the contrast test of ADR-062 lay out the Controls window and the cards' third line with the longest content.

## What this forecloses

- A key the Controls window names that is defined anywhere but `KeyBindings.h`.
