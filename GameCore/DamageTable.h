#pragma once

#include "Catalog.h"
#include "Design.h"
#include "SizeClass.h"
#include "TickRate.h"

#include <cstdint>

namespace Outpost
{

/// ADR-014: **damage is counted in ten-thousandths of a point.** It is the unit in which every row of
/// `GameDesign.md` section 7 is a whole number a tick -- 25 a second at 70% is 8,750 a tick, 0.875 of a
/// point -- so the table is what the simulation does and not an approximation of it.
inline constexpr std::uint32_t DAMAGE_UNITS_PER_POINT = 10000;

/// ADR-014's cadence, and the one named constant it asks for: **damage accumulates every tick.** A weapon
/// that should hit in volleys is a different row with its own rule, written when one exists.
inline constexpr std::uint32_t DAMAGE_INTERVAL_TICKS = 1;

/// **`GameDesign.md` section 7's six numbers**, in percent: what a weapon class does to a ship of each size
/// class, read from the weapon's catalog row. Zero for a component that is not a weapon. The whole of the
/// rock-paper-scissors between ships.
[[nodiscard]] std::uint32_t ModifierPercent(ComponentId _weapon, SizeClass _target) noexcept;

/// **ONE MOUNT AGAINST A SHIP, ONE DAMAGE INTERVAL**, in ten-thousandths of a point: `base x modifier / 100`,
/// per second, spread over the tick rate (section 7, ADR-014). Integer throughout, and exact for every
/// weapon and size class in the catalog.
[[nodiscard]] std::uint32_t ShipDamagePerInterval(ComponentId _weapon, SizeClass _target) noexcept;

/// **ONE MOUNT AGAINST A STATION OR A MODULE**, in ten-thousandths of a point: `base x 100 / (100 +
/// hitValue)`, per second, spread over the tick rate -- section 7's other mitigation model.
///
/// **IT MULTIPLIES BEFORE IT DIVIDES**, and there is one division. `base x (100 / (100 + hitValue))` in
/// integers truncates the fraction to zero first, so every shot would do nothing: the fault that passes a
/// smoke test and fails a match.
[[nodiscard]] std::uint32_t StructureDamagePerInterval(ComponentId _weapon, std::uint32_t _hitValue) noexcept;

/// **ONE MOUNT AGAINST A DESIGN**, choosing the model the way section 7 does: a design with a derived hit
/// value is a structure and takes the curve; anything else is a ship and takes the table, by its hull's
/// size class. Nothing here names a station (R24).
[[nodiscard]] std::uint32_t DamagePerInterval(ComponentId _weapon, DesignId _target) noexcept;

/// **ADR-014'S SETTLEMENT**: adds one interval's damage to a mount's remainder and returns the whole points
/// to take off the target's hull, keeping the fraction. The remainder is below one point on the way out.
[[nodiscard]] constexpr std::uint32_t Accumulate(std::uint32_t& _remainder, std::uint32_t _perInterval) noexcept
{
  const std::uint32_t total = _remainder + _perInterval;
  _remainder = total % DAMAGE_UNITS_PER_POINT;
  return total / DAMAGE_UNITS_PER_POINT;
}

} // namespace Outpost
