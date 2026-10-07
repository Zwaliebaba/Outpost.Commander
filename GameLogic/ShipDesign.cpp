#include "pch.h"
#include "ShipDesign.h"

#include <algorithm>
#include <cmath>

namespace
{
template <typename Entry, typename IdType> const Entry& Find(const std::vector<Entry>& _list, IdType _id, std::string_view _what)
{
  const auto found = std::ranges::find(_list, _id, &Entry::id);
  if (found == _list.end())
    throw Neuron::Exception(std::format("No {} {} in the tuning data", _what, _id.value));
  return *found;
}

// Whether a research topic unlocks this component.
template <typename IdType> bool IsUnlockedByResearch(const Outpost::Tuning& _tuning, IdType _id)
{
  return std::ranges::any_of(_tuning.research,
                             [_id](const Outpost::ResearchTopicTuning& _topic)
                             {
                               const IdType* unlocks = std::get_if<IdType>(&_topic.effect);
                               return unlocks != nullptr && *unlocks == _id;
                             });
}
} // namespace

Outpost::ShipMovement Outpost::MovementFor(const Tuning& _tuning, HullId _hull, DriveId _drive)
{
  const auto hull = std::ranges::find(_tuning.hulls, _hull, &HullTuning::id);
  const auto drive = std::ranges::find(_tuning.drives, _drive, &DriveTuning::id);
  if (hull == _tuning.hulls.end() || drive == _tuning.drives.end())
    throw Neuron::Exception(std::format("MovementFor: no hull {} or no drive {} in the tuning data", _hull.value, _drive.value));
  return DesignStatsOf(ViewOf(*hull, {}, true), ViewOf(*drive, true), {}).movement;
}

Outpost::HullView Outpost::ViewOf(const HullTuning& _hull, const Upgrades& _upgrades, bool _available)
{
  return {.id = _hull.id,
          .nameUtf8 = _hull.name,
          .hitPointsHundredths = static_cast<std::int32_t>(std::llround(_hull.hitPoints * HUNDREDTHS * _upgrades.hullHitPointsFactor)),
          .armorHundredths = _hull.armor * HUNDREDTHS,
          .speedMetersPerSecond = _hull.speedMetersPerSecond * _upgrades.shipSpeedFactor,
          .turnRateDegreesPerSecond = _hull.turnRateDegreesPerSecond,
          .footprintRadiusMeters = _hull.footprintRadiusMeters,
          .cost = _hull.cost,
          .buildSeconds = _hull.buildSeconds,
          .available = _available,
          .commandPoints = _hull.commandPoints};
}

Outpost::DriveView Outpost::ViewOf(const DriveTuning& _drive, bool _available)
{
  return {.id = _drive.id,
          .nameUtf8 = _drive.name,
          .speedFactor = _drive.speedFactor,
          .hitPointsFactor = _drive.hitPointsFactor,
          .turnRateFactor = _drive.turnRateFactor,
          .cost = _drive.cost,
          .available = _available};
}

Outpost::WeaponView Outpost::ViewOf(const WeaponTuning& _weapon, const Upgrades& _upgrades, bool _available)
{
  return {.id = _weapon.id,
          .nameUtf8 = _weapon.name,
          .damageHundredths = _weapon.damage * HUNDREDTHS,
          .fireIntervalSeconds = _weapon.fireIntervalSeconds / _upgrades.FireRateFactor(_weapon.id),
          .rangeMeters = _weapon.rangeMeters,
          .splashRadiusMeters = _weapon.splashRadiusMeters,
          .cost = _weapon.cost,
          .available = _available};
}

Outpost::ModuleView Outpost::ViewOf(const ModuleTuning& _module)
{
  return {.id = _module.id,
          .nameUtf8 = _module.name,
          .sightMeters = _module.sightMeters,
          .speedFactor = _module.speedFactor,
          .cost = _module.cost,
          .available = true};
}

Outpost::DesignStats Outpost::DesignStatsFor(const Tuning& _tuning, const DesignComponents& _components, const Upgrades& _upgrades)
{
  if (!_components.module.IsValid())
    return DesignStatsFor(_tuning, _components.hull, _components.drive, _components.weapon, _upgrades);
  const ModuleView module = ViewOf(Find(_tuning.modules, _components.module, "module"));
  return DesignStatsOf(ViewOf(Find(_tuning.hulls, _components.hull, "hull"), _upgrades, true),
                       ViewOf(Find(_tuning.drives, _components.drive, "drive"), true),
                       ViewOf(Find(_tuning.weapons, _components.weapon, "weapon"), _upgrades, true), &module);
}

Outpost::DesignStats Outpost::DesignStatsFor(const Tuning& _tuning, HullId _hull, DriveId _drive, WeaponId _weapon,
                                             const Upgrades& _upgrades)
{
  return DesignStatsOf(ViewOf(Find(_tuning.hulls, _hull, "hull"), _upgrades, true), ViewOf(Find(_tuning.drives, _drive, "drive"), true),
                       ViewOf(Find(_tuning.weapons, _weapon, "weapon"), _upgrades, true));
}

std::vector<Outpost::DesignComponents> Outpost::StartingDesigns(const Tuning& _tuning)
{
  std::vector<DesignComponents> designs;
  for (const HullTuning& hull : _tuning.hulls)
  {
    for (const DriveTuning& drive : _tuning.drives)
    {
      for (const WeaponTuning& weapon : _tuning.weapons)
      {
        if (!IsUnlockedByResearch(_tuning, hull.id) && !IsUnlockedByResearch(_tuning, drive.id) &&
            !IsUnlockedByResearch(_tuning, weapon.id))
          designs.push_back({hull.id, drive.id, weapon.id});
      }
    }
  }
  return designs;
}

std::string Outpost::StartingDesignName(const Tuning& _tuning, const DesignComponents& _components)
{
  const auto named = std::ranges::find_if(_tuning.startingDesigns,
                                          [&_components](const StartingDesignTuning& _design)
                                          {
                                            return !_components.module.IsValid() && _design.hull == _components.hull &&
                                                   _design.drive == _components.drive && _design.weapon == _components.weapon;
                                          });
  return named != _tuning.startingDesigns.end() ? named->name : DesignName(_tuning, _components);
}

std::string Outpost::DesignName(const Tuning& _tuning, const DesignComponents& _components)
{
  std::string name =
    std::format("{}+{}+{}", Find(_tuning.hulls, _components.hull, "hull").name, Find(_tuning.drives, _components.drive, "drive").name,
                Find(_tuning.weapons, _components.weapon, "weapon").name);
  if (_components.module.IsValid())
    name += std::format("+{}", Find(_tuning.modules, _components.module, "module").name);
  return name;
}
