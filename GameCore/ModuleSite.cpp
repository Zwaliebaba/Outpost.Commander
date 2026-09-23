// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "ModuleSite.h"

#include <array>

namespace Outpost
{

namespace
{
[[nodiscard]] std::int64_t DistanceSquared(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  const std::int64_t dx = static_cast<std::int64_t>(_a.x) - _b.x;
  const std::int64_t dy = static_cast<std::int64_t>(_a.y) - _b.y;
  return (dx * dx) + (dy * dy);
}

/// Half of each hull's size, summed, in `Fixed`: how far apart two centers must be for the footprints to clear.
[[nodiscard]] std::int64_t ClearanceFixed(DesignId _first, DesignId _second) noexcept
{
  const std::int64_t first = Hull(Design(_first).hull).sizeUnits;
  const std::int64_t second = Hull(Design(_second).hull).sizeUnits;
  return ((first + second) * Neuron::FIXED_ONE) / 2;
}

/// Q54's pairs, lower level first.
struct ModuleUpgrade
{
  DesignId from;
  DesignId to;
};

constexpr std::array<ModuleUpgrade, 2> MODULE_UPGRADES{
  ModuleUpgrade{.from = DesignId::ModuleShipyardL1, .to = DesignId::ModuleShipyardL2},
  ModuleUpgrade{.from = DesignId::ModuleOreProcessorL1, .to = DesignId::ModuleOreProcessorL2}};
} // namespace

bool IsModule(DesignId _design) noexcept
{
  return (static_cast<std::size_t>(_design) < Designs().size()) && (Design(_design).hull == HullId::ModuleFrame);
}

bool UpgradesTo(DesignId _from, DesignId _to) noexcept
{
  for (const ModuleUpgrade& upgrade : MODULE_UPGRADES)
  {
    if ((upgrade.from == _from) && (upgrade.to == _to))
    {
      return true;
    }
  }
  return false;
}

bool IsPlacedLevel(DesignId _design) noexcept
{
  if (!IsModule(_design))
  {
    return false;
  }
  for (const ModuleUpgrade& upgrade : MODULE_UPGRADES)
  {
    if (upgrade.to == _design)
    {
      return false;
    }
  }
  return true;
}

std::uint32_t UpgradeCostCredits(DesignId _from, DesignId _to) noexcept
{
  if (!UpgradesTo(_from, _to))
  {
    return 0;
  }
  const std::uint32_t before = Derive(_from).cost;
  const std::uint32_t after = Derive(_to).cost;
  return (after > before) ? (after - before) : 0;
}

ModuleSiteVerdict CheckModuleSite(const Neuron::Vec2& _stationPosition, DesignId _stationDesign, std::span<const PlacedModule> _existing,
                                  const Neuron::Vec2& _site, DesignId _moduleDesign) noexcept
{
  if (!IsModule(_moduleDesign))
  {
    return ModuleSiteVerdict{.fault = ModuleSiteFault::NotAModule};
  }
  if (_existing.size() >= MAXIMUM_MODULES_PER_STATION)
  {
    return ModuleSiteVerdict{.fault = ModuleSiteFault::AtCapacity};
  }

  const std::int64_t radius = static_cast<std::int64_t>(MODULE_BUILD_RADIUS_UNITS) * Neuron::FIXED_ONE;
  if (DistanceSquared(_site, _stationPosition) > (radius * radius))
  {
    return ModuleSiteVerdict{.fault = ModuleSiteFault::OutsideRadius};
  }

  const std::int64_t stationClearance = ClearanceFixed(_stationDesign, _moduleDesign);
  if (DistanceSquared(_site, _stationPosition) < (stationClearance * stationClearance))
  {
    return ModuleSiteVerdict{.fault = ModuleSiteFault::OnStation};
  }

  // THE LOWEST IDENTITY AMONG THE OVERLAPS, found in one pass rather than by sorting: the answer is the same
  // whatever order the caller listed the modules in, which is the whole of R16's ordering rule here.
  WireIdentity blockedBy = NO_WIRE_IDENTITY;
  bool blocked = false;
  for (const PlacedModule& module : _existing)
  {
    const std::int64_t clearance = ClearanceFixed(module.design, _moduleDesign);
    if ((DistanceSquared(_site, module.position) < (clearance * clearance)) && (!blocked || (module.identity < blockedBy)))
    {
      blocked = true;
      blockedBy = module.identity;
    }
  }
  if (blocked)
  {
    return ModuleSiteVerdict{.fault = ModuleSiteFault::OnModule, .blockedBy = blockedBy};
  }

  return ModuleSiteVerdict{};
}

} // namespace Outpost
