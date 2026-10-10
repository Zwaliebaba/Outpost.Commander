# ADR-068 — The windows' headers, the minimap's marks, what a production card says, and the Controls window

Status: **accepted** · 2026-10-05

## Context

The review of 2026-10-03 found smaller problems that the interface plan (`GameDesign/ImplementationPlan-Interface.md`) gathers in milestone 16:

- "QUEUE · 0 BUILT" in the designer read as one statement.
- The production window's arrows sat by its small label and changed the big title below it.
- Research gave no page position.
- A window's body was 97% opaque, so bright shapes showed through its near-black.
- On the minimap, enemy marks and ore were both warm and differed only by brightness and size. Fields stood at 1.3:1 against the map, and dry asteroids at 2.5:1.
- A production card showed a name, a code and a cost, but not the build time or what the design is good against.
- D, P and R open the windows, and nothing taught A, S, Alt, the control groups or the camera's keys.

The owner decided gate K5 on 2026-10-03, yes to each look as proposed, and gate K4 the same day. On 2026-10-05 the owner answered what the gates had left for the run:

- The main menu stays as it is, with no line for the Controls window.
- The brightest stars stay ADR-028's crosses, so task 16.6 changes nothing.

As task 9.6's looks shared [ADR-046](ADR-046-second-look-at-the-screen.md), tasks 16.1–16.4 share this ADR.

## Decision

1. **The windows' headers (task 16.1).**
   - The designer writes its queue over its slots as the other windows write a queue, "QUEUE · 2 / 5". It writes its ships built apart, "BUILT · 4", beside the slots.
   - Arrows flank what they step: "< SHIPYARD 05 > · DESIGNER" in the designer's title bar, and "< SHIPYARD 05 >" on the production window's title line, where they were beside its small "PRODUCTION" label.
   - An arrow's button is 24 units square, where it was 18. The research window's and the saved designs' arrows grow with it.
   - The saved-design chips narrow from 220 units to 216, so the chips' arrows stand clear of the third chip.
   - Research says where its page is: "TOPICS · 11-20 OF 21", with an ASCII hyphen ([ADR-030](ADR-030-typography-and-sprites.md)).
   - A window's body is opaque.
2. **The minimap's marks and size (task 16.2).**
   - An ore asteroid's mark, dry or not, is a square outlined 2 units wide in its color, where it was filled. It differs from an enemy's filled mark in shape as well as in brightness.
   - A field is (0.13, 0.13, 0.14) in linear color, where it was (0.06, 0.06, 0.065): 2.1:1 against the map, from 1.3:1. It is still darker than an ore asteroid.
   - A dry asteroid is (0.32, 0.20, 0.147), where it was (0.24, 0.15, 0.11): 3.1:1 against the map, from 2.5:1. It is still darker than one with ore.
   - The minimap is 300 units square, where it was 260: about 18 m a unit on the 5 km map.
3. **What a production card says (task 16.3).** A Shipyard's card adds a third line under the code and cost:
   - The design's build time at the player's Shipyard speed, as the designer gives it.
   - A short bar for each hull, marked with the hull's initial, S, M and L. Each bar is three segments, lit as the designer rates the design's damage against that hull: three for Good, two for Fair and one for Poor. The bars read without their colors.
   - The Constructor's card adds its build time. The snapshot now carries the Command Station's time for a Constructor (`Snapshot::constructorBuildSeconds`).
   - A card is 74 units tall, where it was 48. A card whose hull is above the Shipyard's level still says so in its code line.
4. **The Controls window (task 16.4).**
   - F1 opens and closes it. It is a fourth `WindowKind`, in the windows' look, and opens in the middle of the screen. It lists every key and mouse action the game reads in a match, under three headings: selecting and orders, the camera, and the windows.
   - Every key the input code reads in a match is in `GameApp/KeyBindings.h`: the orders' and the control groups' in `PlayerControls`, the camera's in `Camera`, and the windows', Esc, Space and Alt in `GameClient`. The window's lines are made from those constants by `KeyBindings()`, so the window cannot name a key the code does not read.
   - The main menu is unchanged (owner, 2026-10-05). It stays MVP §9's Start skirmish and Quit, now one button per difficulty ([ADR-065](ADR-065-ai-difficulty.md)).
5. **The sky keeps its crosses (task 16.6, owner 2026-10-05).** [ADR-028](ADR-028-vector-grid-and-crosses.md) decision 3 stands as it is.

## Consequences

- **The production window is taller.** With six designs, three lines of cards take 238 units, where they took 160.
- **The name field's typing keys are not listed.** Enter, Backspace and Esc edit a design's name while the field has the keyboard ([ADR-017](ADR-017-research-and-the-designer.md)). That is text entry, not a control of the match. The designer's own Esc is still `VK_ESCAPE` in `Designer.cpp`.
- **The contrast figures are computed, not measured on a screen.** They use WCAG's ratio over the minimap's ground, its frame and map over black, as `HudTests` computes the text's.
- **The tests.**
  - `HudTests.WritesTheWindowsHeaders`
  - `SetsTheMinimapsMarksApart`
  - `SaysWhatAProductionCardBuilds`
  - `ListsEveryKeyInTheControlsWindow`
  - `WindowManagerTests.OpensTheControlsAsAFourthKind`
  - `WireFormatTests` carries the new field.
  - 14.1's layout and contrast tests lay out the Controls window and the cards' third line with the longest content.

## What this forecloses

- A key the Controls window names that is defined anywhere but `KeyBindings.h`.
