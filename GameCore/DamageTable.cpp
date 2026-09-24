// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "DamageTable.h"

#include "DerivedStats.h"

namespace Outpost
{

namespace
{
/// Percent, the unit section 7's table is written in.
constexpr std::uint64_t PERCENT = 100;

/// **THE ONE DIVISOR**: per second into per interval, and percent out of the product. Everything else is
/// multiplied first.
constexpr std::uint64_t PER_SECOND_TO_INTERVAL = static_cast<std::uint64_t>(TICKS_PER_SECOND) / DAMAGE_INTERVAL_TICKS;

/// A weapon's damage a second, per mount, or zero for anything that does not shoot.
[[nodiscard]] std::uint64_t BasePerSecond(ComponentId _weapon) noexcept
{
  return static_cast<std::uint64_t>(Component(_weapon).damagePerSecond);
}
} // namespace

std::uint32_t ModifierPercent(ComponentId _weapon, SizeClass _target) noexcept
{
  // The catalog's row: section 7's table lives with the weapon, not here (R24).
  return Component(_weapon).modifierPercent[static_cast<std::size_t>(_target)];
}

std::uint32_t ShipDamagePerInterval(ComponentId _weapon, SizeClass _target) noexcept
{
  const std::uint64_t numerator = BasePerSecond(_weapon) * ModifierPercent(_weapon, _target) * DAMAGE_UNITS_PER_POINT;
  return static_cast<std::uint32_t>(numerator / (PERCENT * PER_SECOND_TO_INTERVAL));
}

std::uint32_t StructureDamagePerInterval(ComponentId _weapon, std::uint32_t _hitValue) noexcept
{
  const std::uint64_t numerator = BasePerSecond(_weapon) * PERCENT * DAMAGE_UNITS_PER_POINT;
  return static_cast<std::uint32_t>(numerator / ((PERCENT + _hitValue) * PER_SECOND_TO_INTERVAL));
}

std::uint32_t DamagePerInterval(ComponentId _weapon, DesignId _target) noexcept
{
  const DerivedStats target = Derive(_target);
  if (target.hitValue > 0)
  {
    return StructureDamagePerInterval(_weapon, target.hitValue);
  }
  return ShipDamagePerInterval(_weapon, Hull(Design(_target).hull).sizeClass);
}

} // namespace Outpost
