# ADR-088 — Selection and commands: what the pointer is over, the orders beside the selection, and the bars on the screen

Status: **accepted** · 2026-10-10

## Context

The second interface review of 2026-10-08 ([interface plan 2](../../GameDesign/ImplementationPlan-Interface2.md), findings 9 to 12) found the selection and its commands hard to read and hard to reach: nothing on the HUD answered the pointer, a warship's orders had no buttons, the one button there was stood apart from the selection it acted on, and the bars over a ship were drawn on the ground, where the camera turned and foreshortened them. The owner decided gates V6 and V7 as proposed on 2026-10-08. This ADR is milestone UI4's, one decision for each of its tasks.

## Decision

1. **The HUD answers the pointer** (task UI4.4). `Hud::Lay` takes the action under the pointer, as `Hud::Describe` does, which `GameClient` finds in the last frame's layout (`Layout::ActionAt`).
   - An enabled button that takes it has its edge lit in the windows' edge color, (0.25, 0.43, 0.76), short of a pick's edge. A pick keeps its own.
   - A status line that takes it lies on the cards' color. Lines whose click does the same light together, since a click on either does what a click on the other would: the Shipyards' line and the fleet's both open production.
   - Text the player cannot click is unchanged, and so is every word, place and click; `Lay` without a hovered action lays out as before.
2. **The commands stand beside the selection** (task UI4.2).
   - **The buttons' panel stands against the selection panel's right edge,** bottom-aligned, and the pair is centered at the bottom, so that the two read as one, at every width the selection panel takes. It moves right only where it would meet the minimap: with both panels at their widest, on a screen narrower than about 3:2, such as 4:3. A panel of buttons with no selection beside it stands alone at the middle. This rewrites [ADR-043](ADR-043-hud-in-the-windows-look.md) decision 1's anchor for the buttons, which was the bottom-right corner.
   - **A selection with a warship offers its orders first:** Attack-move, Hold sector, Patrol and Stop, each with its key's cap from `KeyBindings.h`, A, H, T and S. Hold sector is offered only on a map with sectors, as the Relay is (ADR-056). A selection of Constructors alone offers none; the Constructors' build buttons and the retreat follow the orders as before.
   - **A button does what its key does.** `PlayerControls` gains `ArmAttackMove`, `ArmStanding` and `Stop`, which the keys call too, and four action kinds name them. Attack-move, Hold sector and Patrol arm the next left-click, each in place of what was armed; Stop orders at once. They act on every selected ship, as the keys do.
   - **The armed one is lit,** drawn as a pick, until it is given or canceled: `Hud::Describe` takes the order the controls have armed. An order is given on the left press, and a release only acts on a press made in the world, so the press on a button never gives the order it arms.

## Consequences

- **What lights is the HUD's own.** The windows' cards, whose numbers the designer already previews under the pointer, and the menu's buttons are as they were.
- **The selection's panel and its buttons are one place to look,** where the buttons were across the screen from it. A tall column of buttons, a Constructor's six with its retreat, now stands beside a short selection panel rather than in the corner.
- **Tests:** `HudTests.OffersTheOrdersBesideTheSelection` checks the four orders with their caps, Hold sector only with sectors, none for Constructors alone, the armed one lit, and the two panels touching, bottom-aligned and centered, for three widths of the selection panel at 1920 and 1280 wide; `LaysOutButtonsForClicks` now finds the buttons at the bottom's middle. `PlayerControlsTests.GivesTheOrdersOfTheHudsButtons` checks each call arming or ordering as its key does, each replacing the last, and none without a selection. `PlayerControlsTests` and `GameClient` need DirectXMath, so CI runs them.
- **Tests (UI4.4):** `HudTests.LightsWhatThePointerIsOver` checks nothing lit without a pointer; the Research button's four edges lit, and only its, with the status line that opens research on the cards' color; over production, the enabled button and not a dim one naming the same action, and both lines that open it; and every text, its place and every click the same as without the pointer.

## What this forecloses

- A hover state the player must learn: what lights is what a click takes.
