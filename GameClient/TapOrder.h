#pragma once

#include "Camera.h"

#include "GameCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// What a tap means, and the ray that decides it.
///
/// **THE RAY IS M0.20's** (ADR-018, ADR-001). A tap is the anchor cast at a different moment, which
/// is why the camera comes first in the plan and why there is no second intersection routine here.
///
/// R19 IS THE WHOLE OF THIS FILE'S DISCIPLINE. Nothing here moves an entity, and nothing here
/// decides whether an order is legal -- the host validates ownership, selection length, generation
/// and bounds at intake (Q24), and this produces a request. The one thing the client draws of its
/// own accord is the marker next door in `OrderMarker.h`, which is presentation and not prediction.

/// `Interface.md` section 1's pick order, which exists because "what is under it" is not a total
/// order when things overlap. Nearest wins inside a tier; the tier wins across one.
///
/// **INTERFACE TARGETS COME BEFORE ALL OF THESE** and are not in this enum. A tap that lands on a
/// panel and a ship at once is every tap near the bottom of the screen (`Interface.md` section 1,
/// ADR-020), and the panel takes it -- but a panel is not a world entity and the test is not a
/// distance, so that tier is resolved before a world pick is attempted at all. There are no panels
/// until M1.12; this enum is what the world contributes.
///
/// **THE MEMBERS BELOW ARE THE TIERS, NOT THE VERBS.** What a tap on a hostile or an asteroid does
/// is `Interface.md` section 4's table, and M0 can express none of it: there are no teams, no ore
/// and no build panel yet. The tiers are stated here because the ORDER is the design's and is the
/// part that cannot be reconstructed later from a table of verbs.
enum class PickTier : std::uint8_t
{
  OwnShip,
  OwnStructure,
  Hostile,
  Asteroid,
  /// Not a candidate but an outcome: nothing was inside the radius.
  EmptySpace
};

/// `Interface.md` section 1's fourth gesture constant is **not here** -- it is
/// `Neuron::PICK_RADIUS_AUTHORED_PIXELS`, beside the three from the same table. That table is one
/// fact and splitting it across two libraries is how a figure starts to drift.

/// One thing a tap might have landed on.
///
/// R8: a public aggregate.
struct PickCandidate
{
  /// Packed as an `EntityRecord`'s identity is.
  WireIdentity identity = NO_WIRE_IDENTITY;
  PickTier tier = PickTier::OwnShip;
  /// From the tap, in authored pixels -- the units the radius is stated in.
  float screenDistanceAuthoredPixels = 0.0f;
};

/// The candidate a tap resolves to, by `Interface.md` section 1's rule.
///
/// FALSE MEANS EMPTY SPACE: nothing was within the radius. That is an outcome rather than a
/// failure, and it is the one M0 acts on.
[[nodiscard]] bool ResolvePick(std::span<const PickCandidate> _candidates, PickCandidate& _outHit) noexcept;

/// What the client decided a tap was asking for.
enum class TapAction : std::uint8_t
{
  /// A tap on empty space with nothing selected (`Interface.md` section 4, stated explicitly
  /// because "nothing happens" is a decision here rather than a gap).
  Nothing,
  /// Empty space with something selected. **The only verb M0 has.**
  MoveTo,
  /// Something was under the tap. Which verb that becomes is `Interface.md` section 4's table and
  /// arrives with the systems that can express it -- M0 resolves the tier and stops.
  Occupied
};

/// One tap, resolved.
///
/// R8: a public aggregate.
struct TapOutcome
{
  TapAction action = TapAction::Nothing;
  /// Meaningful when the action is MoveTo: where on the plane, in world units.
  float worldX = 0.0f;
  float worldY = 0.0f;
  /// Meaningful when the action is Occupied.
  PickCandidate hit;
};

/// A tap at an authored-pixel point, against the camera and whatever the caller found near it.
///
/// _hasSelection is the client's own state: with nothing selected, a tap on empty space does
/// nothing at all, which is `Interface.md` section 4 and not an omission.
///
/// The tap is refused -- Nothing -- when the ray misses the plane. The pitch floor is what keeps
/// the horizon off screen (`Interface.md` section 5), so that is a case the camera should make
/// unreachable rather than one the caller has to handle.
[[nodiscard]] TapOutcome ResolveTap(const CameraPose& _pose, float _aspectRatio, float _authoredX, float _authoredY, float _authoredWidth,
                                    float _authoredHeight, std::span<const PickCandidate> _candidates, bool _hasSelection) noexcept;

/// The move order a resolved tap becomes, ready for the outgoing packet.
///
/// **QUANTIZED THROUGH `GameCore`'s OWN QUANTIZER**, so the point the client asks for is a point
/// the wire can say and the rounding rule is the one `EntityRecord.h` owns rather than a second
/// one invented here.
///
/// The selection is copied rather than referenced: a command outlives the tap that made it, because
/// ADR-003 has it repeated in every outgoing packet until the host acknowledges the sequence.
[[nodiscard]] Command BuildMoveCommand(std::uint16_t _sequence, float _worldX, float _worldY,
                                       std::span<const WireIdentity> _selection) noexcept;

} // namespace Outpost
