#pragma once

#include "Design.h"
#include "EntityRecord.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Outpost
{

/// **WHERE A MODULE MAY GO** (M2.10, ADR-015): one pure function, in `GameCore` because both sides evaluate
/// it -- the client previews it under the finger and the host validates the placement with it (R19,
/// `TechnicalDesign.md` section 2's table). **There is exactly one**, which a grep for `CheckModuleSite` can
/// confirm.
///
/// **INTEGER THROUGHOUT** (R16): positions in `Neuron::Fixed`, distances compared squared in 64 bits, sizes
/// from Q37's catalog row. **The one inexactness is the inputs'**, not the rule's: the host passes exact
/// positions and the client the quarter-unit ones the wire carries, so a preview can disagree with the host by
/// that much at an edge -- and the host is the one that decides.

/// **400 WORLD UNITS, BECAUSE THAT IS THE POINT-DEFENSE RANGE** (ADR-015): the safe zone means exactly "your
/// base". `ModuleSiteTests` asserts the two are equal, so moving one without the other fails.
inline constexpr std::int32_t MODULE_BUILD_RADIUS_UNITS = 400;

/// **FOUR TO A STATION** (ADR-015): as much a replication budget as a design one, and raising it is a protocol
/// decision rather than a game one.
inline constexpr std::size_t MAXIMUM_MODULES_PER_STATION = 4;

/// Why a site was refused, or `None`.
enum class ModuleSiteFault : std::uint8_t
{
  None,
  /// The design is not a module -- its hull is not a `ModuleFrame`.
  NotAModule,
  /// The station already carries four.
  AtCapacity,
  /// The site's center is more than `MODULE_BUILD_RADIUS_UNITS` from the station's.
  OutsideRadius,
  /// The module would overlap the station.
  OnStation,
  /// The module would overlap a module already there.
  OnModule
};

/// One module already placed at the station, as either side knows it: the host from its world, the client from
/// the records it was sent. R8: a public aggregate.
struct PlacedModule
{
  WireIdentity identity = NO_WIRE_IDENTITY;
  Neuron::Vec2 position{};
  DesignId design = DesignId::ModuleShipyardL1;
};

/// The verdict, and **which module blocked it when one did** -- the lowest identity among those it overlaps,
/// so two sides that list the same modules in different orders name the same one (R16). R8: a public aggregate.
struct ModuleSiteVerdict
{
  ModuleSiteFault fault = ModuleSiteFault::None;
  WireIdentity blockedBy = NO_WIRE_IDENTITY;

  [[nodiscard]] constexpr bool Legal() const noexcept
  {
    return fault == ModuleSiteFault::None;
  }
};

/// **CAN _moduleDesign GO AT _site, FOR THE STATION AT _stationPosition WITH _existing ALREADY THERE?**
///
/// The checks run in a fixed order, so the fault is the same on both sides whatever the order of _existing:
/// not a module, then the cap, then the radius, then the station, then the modules. **"Clear of" is the
/// catalog's sizes** (Q37 made `sizeUnits` the bound that spaces things): two footprints are clear when their
/// centers are at least half of each size apart -- 155 units from a 220-unit station for a 90-unit frame, and
/// 90 between two frames. Touching is clear; overlapping by a step is not.
///
/// The radius is inclusive: a site exactly 400 out is inside it.
[[nodiscard]] ModuleSiteVerdict CheckModuleSite(const Neuron::Vec2& _stationPosition, DesignId _stationDesign,
                                                std::span<const PlacedModule> _existing, const Neuron::Vec2& _site,
                                                DesignId _moduleDesign) noexcept;

} // namespace Outpost
