# ADR-088 — Selection and commands: what the pointer is over, the orders beside the selection, and the bars on the screen

Status: **accepted** · 2026-10-10

## Context

The second interface review of 2026-10-08 ([interface plan 2](../../GameDesign/ImplementationPlan-Interface2.md), findings 9 to 12) found the selection and its commands hard to read and hard to reach: nothing on the HUD answered the pointer, a warship's orders had no buttons, the one button there was stood apart from the selection it acted on, and the bars over a ship were drawn on the ground, where the camera turned and foreshortened them. The owner decided gates V6 and V7 as proposed on 2026-10-08. This ADR is milestone UI4's, one decision for each of its tasks.

## Decision

1. **The HUD answers the pointer** (task UI4.4). `Hud::Lay` takes the action under the pointer, as `Hud::Describe` does, which `GameClient` finds in the last frame's layout (`Layout::ActionAt`).
   - An enabled button that takes it has its edge lit in the windows' edge color, (0.25, 0.43, 0.76), short of a pick's edge. A pick keeps its own.
   - A status line that takes it lies on the cards' color. Lines whose click does the same light together, since a click on either does what a click on the other would: the Shipyards' line and the fleet's both open production.
   - Text the player cannot click is unchanged, and so is every word, place and click; `Lay` without a hovered action lays out as before.

## Consequences

- **What lights is the HUD's own.** The windows' cards, whose numbers the designer already previews under the pointer, and the menu's buttons are as they were.
- **Tests:** `HudTests.LightsWhatThePointerIsOver` checks nothing lit without a pointer; the Research button's four edges lit, and only its, with the status line that opens research on the cards' color; over production, the enabled button and not a dim one naming the same action, and both lines that open it; and every text, its place and every click the same as without the pointer.

## What this forecloses

- A hover state the player must learn: what lights is what a click takes.
