#pragma once

#include "Camera.h"
#include "TapOrder.h"

#include "GameCore.h"
#include "NeuronClient.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **WHAT IS UNDER A TAP**, built from the replica store. M0.21 settled the tier ORDER and the rule
/// for resolving between candidates; this is what produces the candidates.
///
/// **A LINEAR SCAN, AND DO NOT BUILD A SPATIAL INDEX ON THE CLIENT** (M1.10). At the design's 110
/// entities a scan is nothing; the simulation's grid is `GameLogic`'s and arrives at M2.5, and a
/// second one here would be a second thing to keep correct for no measured gain.
///
/// **NOTHING HERE MOVES ANYTHING** (R19). It projects, measures and sorts.

/// One tap's worth of context: who is asking, and what the camera was doing.
///
/// R8: a public aggregate.
struct HitTestRequest
{
  /// The tap, in authored pixels -- the units `Interface.md` section 1's radius is stated in.
  float authoredX = 0.0f;
  float authoredY = 0.0f;

  /// The frame the tap was reported against, which is the authored one.
  float authoredWidth = 1440.0f;
  float authoredHeight = 960.0f;

  /// The scene target's, because that is what the world was drawn at.
  float aspectRatio = 1.5f;

  /// Whose ships are "own". `NO_PLAYER` before the join is answered (ADR-013), in which case
  /// nothing is own and everything owned is hostile.
  PlayerId player = NO_PLAYER;
};

/// **WHERE A ROCK IS DRAWN, FOR THE TAP TO AIM AT** (M2.8): its place on the plane from the generated field,
/// and the height the client drew it at (M2.4). A tap aims at what is on screen, and a rock 240 units above
/// the plane is on screen somewhere other than its plane position. R8: a public aggregate.
struct RockPickPoint
{
  float worldX = 0.0f;
  float worldY = 0.0f;
  float liftUnits = 0.0f;
  /// Its index in the field, which is what the mine order carries (Q52).
  std::uint16_t rock = 0;
};

/// Everything within the pick radius, in no particular order -- `ResolvePick` is what orders them.
///
/// **THE RADIUS IS `Neuron::PICK_RADIUS_AUTHORED_PIXELS`** and is not restated here: `Interface.md`
/// section 1's gesture table is one fact, and splitting it across two libraries is how a figure starts
/// to drift.
///
/// An entity behind the camera contributes nothing: `PlaneToScreen` refuses it, and a candidate that
/// projected to the far side of the eye would be selectable through the floor.
///
/// **ROCKS ARE CANDIDATES TOO SINCE M2.8**, at the asteroid tier -- after own ships, own structures and
/// hostiles, as `Interface.md` section 1's pick order has it -- measured from their drawn centers. An
/// empty _rocks is a client with no field yet, and nothing is lost by it.
[[nodiscard]] std::vector<PickCandidate> CandidatesUnderTap(const CameraPose& _pose, const HitTestRequest& _request,
                                                            std::span<const EntityRecord> _entities,
                                                            std::span<const RockPickPoint> _rocks = {});

/// Which tier a record falls in, for a given player.
///
/// **THE TIER IS OWNERSHIP AND DESIGN, WHICH IS ALL A SNAPSHOT CARRIES.** `EntityRecord`'s flags hold
/// the team in two bits and the design has its own byte (ADR-003), so this needs nothing the client
/// does not already have -- which is the property that let M0.21 state the order before there were
/// teams to order.
[[nodiscard]] PickTier TierOf(const EntityRecord& _record, PlayerId _player) noexcept;

/// The team a record's flags name.
[[nodiscard]] PlayerId OwnerOf(const EntityRecord& _record) noexcept;

} // namespace Outpost
