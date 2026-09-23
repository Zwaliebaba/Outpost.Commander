#pragma once

#include "HitTest.h"
#include "Selection.h"

#include "GameCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// [`ADR-017`](../Design/ADR/ADR-017-group-selection-is-a-double-tap.md): a second tap on one of your
/// ships takes **every ship of the same design within a circle centered on it**.
///
/// **THE STRUCTURE THAT MATTERS IS THAT NOTHING DEFERS.** M1.10's single tap already selected one ship
/// and already fired; this only ever UPGRADES that result when a second tap within the window resolves
/// to the **same entity**. Build it as a decision between two outcomes and every tap in the game gets
/// slower; build it as an upgrade and none does.
///
/// **MATCHING ON IDENTITY RATHER THAN ON SCREEN DISTANCE** is what makes it work while the fleet is
/// moving, which is when it is used.
///
/// **"SAME DESIGN" IS ONLY A COHERENT IDEA BECAUSE ADR-006 MADE A DESIGN A FIRST-CLASS IDENTITY**, and
/// that is why this sits after M1.2 rather than beside M1.10.

/// `Interface.md` sections 3 and 4: **192 authored pixels in radius** -- four times the touch floor,
/// about a quarter of the frame's width.
///
/// **SCREEN-SPACE, WHICH MAKES THE CAMERA THE GROUP-SIZE CONTROL.** Zooming in takes a squad and
/// zooming out takes the fleet, through a gesture the player is already driving constantly. A
/// world-space radius would shrink on screen as the camera pulled back and stop meaning anything.
inline constexpr float GROUP_RADIUS_AUTHORED_PIXELS = 192.0f;

/// The window a second tap has to land in. `TappedEventArgs::TapCount` under
/// `GestureSettings::DoubleTap` is what actually decides this (ADR-017), so the constant is here for
/// the suite and for anything that has to state the figure rather than as a timer this code runs.
inline constexpr std::uint32_t DOUBLE_TAP_WINDOW_MILLISECONDS = 300;

/// What an expansion did.
///
/// R8: a public aggregate.
struct ExpansionOutcome
{
  /// False when the second tap did not resolve to the same entity, which leaves two single taps --
  /// ADR-017's third case and the one that is easy to get wrong.
  bool expanded = false;

  /// How many ships the selection holds afterwards.
  std::size_t selected = 0;
};

/// Every ship the circle takes: **own ships only, same design only, fixed radius, centered on the
/// SHIP** rather than on the finger, because the finger is covering the ship.
///
/// The anchor itself is always included -- it is inside its own circle at distance zero, and saying so
/// is cheaper than a reader wondering.
[[nodiscard]] std::vector<WireIdentity> ShipsInGroupCircle(const CameraPose& _pose, const HitTestRequest& _request,
                                                           std::span<const EntityRecord> _entities, WireIdentity _anchorIdentity);

/// The second tap. **It is an upgrade and it can only ever add**, so a selection that was already
/// expanded and is tapped again simply stays expanded.
///
/// _anchorIdentity is what the FIRST tap selected; _tappedIdentity is what this tap resolved to.
/// Returns `expanded = false` and changes nothing when they differ.
[[nodiscard]] ExpansionOutcome ExpandSelection(Selection& _selection, const CameraPose& _pose, const HitTestRequest& _request,
                                               std::span<const EntityRecord> _entities, WireIdentity _anchorIdentity,
                                               WireIdentity _tappedIdentity);

} // namespace Outpost
