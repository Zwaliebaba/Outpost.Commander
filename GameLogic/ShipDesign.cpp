#include "pch.h"
#include "ShipDesign.h"

#include <algorithm>
#include <cmath>
#include <numbers>

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
  constexpr double RADIANS_PER_DEGREE = std::numbers::pi / 180.0;
  return {.speedMetersPerSecond = static_cast<float>(hull->speedMetersPerSecond * drive->speedFactor),
          .turnRateRadiansPerSecond = static_cast<float>(hull->turnRateDegreesPerSecond * drive->turnRateFactor * RADIANS_PER_DEGREE),
          .radiusMeters = static_cast<float>(hull->footprintRadiusMeters)};
}

Outpost::DesignStats Outpost::DesignStatsFor(const Tuning& _tuning, HullId _hull, DriveId _drive, WeaponId _weapon)
{
  const HullTuning& hull = Find(_tuning.hulls, _hull, "hull");
  const DriveTuning& drive = Find(_tuning.drives, _drive, "drive");
  const WeaponTuning& weapon = Find(_tuning.weapons, _weapon, "weapon");
  return {.movement = MovementFor(_tuning, _hull, _drive),
          .hitPointsHundredths = static_cast<std::int32_t>(std::llround(hull.hitPoints * drive.hitPointsFactor * HUNDREDTHS)),
          .armorHundredths = hull.armor * HUNDREDTHS,
          .cost = hull.cost + drive.cost + weapon.cost,
          .buildSeconds = hull.buildSeconds,
          .damageHundredths = weapon.damage * HUNDREDTHS,
          .fireIntervalSeconds = weapon.fireIntervalSeconds,
          .rangeMeters = static_cast<float>(weapon.rangeMeters)};
}

double Outpost::DamagePerSecond(const DesignStats& _stats, std::int32_t _armorHundredths) noexcept
{
  if (_stats.fireIntervalSeconds <= 0.0)
    return 0.0;
  return static_cast<double>(HitHundredths(_stats.damageHundredths, _armorHundredths)) / HUNDREDTHS / _stats.fireIntervalSeconds;
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

std::string Outpost::DesignName(const Tuning& _tuning, const DesignComponents& _components)
{
  return std::format("{}+{}+{}", Find(_tuning.hulls, _components.hull, "hull").name, Find(_tuning.drives, _components.drive, "drive").name,
                     Find(_tuning.weapons, _components.weapon, "weapon").name);
}
