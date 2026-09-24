#pragma once

#include "Design.h"
#include "Layout.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Outpost
{

/// **A DEPOT STANDS AT LEAST THIS FAR FROM EVERY STATION** (`OpenQuestions.md` Q69): it is a forward point, and one
/// beside a station would be a second station's worth of unloading for 300 credits.
inline constexpr std::int32_t DEPOT_MIN_STATION_DISTANCE_UNITS = 2000;

/// **AND WITHIN THIS OF A ROCK** (Q69), center to center: what it is forward of.
inline constexpr std::int32_t DEPOT_MAX_ROCK_DISTANCE_UNITS = 800;

/// **TWO A PLAYER** (Q69), counting those built and those paid for and still building.
inline constexpr std::size_t MAXIMUM_DEPOTS_PER_PLAYER = 2;

enum class DepotSiteFault : std::uint8_t
{
  None,
  /// The design is not a depot.
  NotADepot,
  /// The player already has two, built or building.
  AtCapacity,
  /// Closer than `DEPOT_MIN_STATION_DISTANCE_UNITS` to some station, anybody's.
  NearStation,
  /// No rock within `DEPOT_MAX_ROCK_DISTANCE_UNITS`.
  FarFromRock,
  /// It would overlap another of the player's depots.
  OnDepot
};

/// **CAN A DEPOT GO AT _site?** One pure integer function, in `GameCore` because both sides evaluate it -- the client
/// previews it under the finger and the host validates the placement with it (R19), as `CheckModuleSite` is for a
/// module. The checks run in a fixed order -- not a depot, the cap, the stations, the rocks, the depots -- so the
/// fault is the same on both sides whatever the order of the inputs.
///
/// _stations is every station's position; _field the rocks, of which only the positions are read; _ownDepots the
/// player's depots, built and building. Two depots are clear when their centers are a whole depot's size apart.
[[nodiscard]] DepotSiteFault CheckDepotSite(std::span<const Neuron::Vec2> _stations, std::span<const Placement> _field,
                                            std::span<const Neuron::Vec2> _ownDepots, const Neuron::Vec2& _site, DesignId _design) noexcept;

/// True when the design's hull is a `DepotFrame`. False for an identity the table does not have.
[[nodiscard]] bool IsDepot(DesignId _design) noexcept;

} // namespace Outpost
