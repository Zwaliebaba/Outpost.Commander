#include "pch.h"
#include "Research.h"

#include <algorithm>

namespace
{
const Outpost::ResearchTopicTuning& FindTopic(const Outpost::Tuning& _tuning, Outpost::ResearchTopicId _id)
{
  const auto found = std::ranges::find(_tuning.research, _id, &Outpost::ResearchTopicTuning::id);
  if (found == _tuning.research.end())
    throw Neuron::Exception(std::format("No research topic {} in the tuning data", _id.value));
  return *found;
}

template <typename IdType> bool Unlocks(const Outpost::ResearchTopicTuning& _topic, IdType _id)
{
  const IdType* unlocks = std::get_if<IdType>(&_topic.effect);
  return unlocks != nullptr && *unlocks == _id;
}

template <typename IdType>
bool IsAvailableComponent(const Outpost::Tuning& _tuning, std::span<const Outpost::ResearchTopicId> _researched, IdType _id)
{
  const bool locked =
    std::ranges::any_of(_tuning.research, [_id](const Outpost::ResearchTopicTuning& _topic) { return Unlocks(_topic, _id); });
  return !locked ||
         std::ranges::any_of(_researched, [&](Outpost::ResearchTopicId _topic) { return Unlocks(FindTopic(_tuning, _topic), _id); });
}

template <typename Entry, typename IdType> std::string_view NameOf(const std::vector<Entry>& _list, IdType _id)
{
  const auto found = std::ranges::find(_list, _id, &Entry::id);
  return found != _list.end() ? std::string_view(found->name) : std::string_view("?");
}
} // namespace

double Outpost::Upgrades::FireRateFactor(WeaponId _weapon) const noexcept
{
  const auto found = std::ranges::find(weaponFireRateFactors, _weapon, &std::pair<WeaponId, double>::first);
  return found != weaponFireRateFactors.end() ? found->second : 1.0;
}

double Outpost::Upgrades::FireRateFactor(StructureWeaponId _weapon) const noexcept
{
  const auto found = std::ranges::find(structureWeaponFireRateFactors, _weapon, &std::pair<StructureWeaponId, double>::first);
  return found != structureWeaponFireRateFactors.end() ? found->second : 1.0;
}

Outpost::Upgrades Outpost::UpgradesFrom(const Tuning& _tuning, std::span<const ResearchTopicId> _researched)
{
  // Each rate's percentages are added first, and made a factor once, so that two topics on one rate add (ADR-033).
  struct Percents
  {
    std::int32_t hullHitPoints = 0;
    std::vector<std::pair<WeaponId, std::int32_t>> weaponFireRates;
    std::int32_t miningIncome = 0;
    std::int32_t shipyardBuildSpeed = 0;
    std::int32_t structureHitPoints = 0;
    std::vector<std::pair<StructureWeaponId, std::int32_t>> structureWeaponFireRates;
    std::int32_t shipSpeed = 0;
    std::int32_t constructorRate = 0;
    std::int32_t oreReserve = 0;
  };
  const auto addTo = []<typename IdType>(std::vector<std::pair<IdType, std::int32_t>>& _list, IdType _id, std::int32_t _percent)
  {
    if (const auto found = std::ranges::find(_list, _id, &std::pair<IdType, std::int32_t>::first); found != _list.end())
      found->second += _percent;
    else
      _list.emplace_back(_id, _percent);
  };
  Percents percents;
  for (const ResearchTopicId id : _researched)
  {
    const auto* upgrade = std::get_if<UpgradeEffect>(&FindTopic(_tuning, id).effect);
    if (upgrade == nullptr)
      continue;
    switch (upgrade->target)
    {
    case UpgradeTarget::MiningRig:
      percents.miningIncome += upgrade->percent;
      break;
    case UpgradeTarget::AllHulls:
      percents.hullHitPoints += upgrade->percent;
      break;
    case UpgradeTarget::Weapon:
      addTo(percents.weaponFireRates, upgrade->weapon, upgrade->percent);
      break;
    case UpgradeTarget::Shipyards:
      percents.shipyardBuildSpeed += upgrade->percent;
      break;
    case UpgradeTarget::AllStructures:
      percents.structureHitPoints += upgrade->percent;
      break;
    case UpgradeTarget::StructureWeapon:
      addTo(percents.structureWeaponFireRates, upgrade->structureWeapon, upgrade->percent);
      break;
    case UpgradeTarget::AllShips:
      percents.shipSpeed += upgrade->percent;
      break;
    case UpgradeTarget::Constructors:
      percents.constructorRate += upgrade->percent;
      break;
    case UpgradeTarget::Asteroids:
      percents.oreReserve += upgrade->percent;
      break;
    }
  }

  const auto factor = [](std::int32_t _percent) { return 1.0 + (_percent / 100.0); };
  Upgrades upgrades{.hullHitPointsFactor = factor(percents.hullHitPoints),
                    .miningIncomeFactor = factor(percents.miningIncome),
                    .shipyardBuildSpeedFactor = factor(percents.shipyardBuildSpeed),
                    .structureHitPointsFactor = factor(percents.structureHitPoints),
                    .shipSpeedFactor = factor(percents.shipSpeed),
                    .constructorRateFactor = factor(percents.constructorRate),
                    .oreReserveFactor = factor(percents.oreReserve)};
  upgrades.weaponFireRateFactors.reserve(percents.weaponFireRates.size());
  for (const auto& [weapon, percent] : percents.weaponFireRates)
    upgrades.weaponFireRateFactors.emplace_back(weapon, factor(percent));
  upgrades.structureWeaponFireRateFactors.reserve(percents.structureWeaponFireRates.size());
  for (const auto& [weapon, percent] : percents.structureWeaponFireRates)
    upgrades.structureWeaponFireRateFactors.emplace_back(weapon, factor(percent));
  return upgrades;
}

bool Outpost::IsAvailable(const Tuning& _tuning, std::span<const ResearchTopicId> _researched, HullId _hull)
{
  return IsAvailableComponent(_tuning, _researched, _hull);
}

bool Outpost::IsAvailable(const Tuning& _tuning, std::span<const ResearchTopicId> _researched, DriveId _drive)
{
  return IsAvailableComponent(_tuning, _researched, _drive);
}

bool Outpost::IsAvailable(const Tuning& _tuning, std::span<const ResearchTopicId> _researched, WeaponId _weapon)
{
  return IsAvailableComponent(_tuning, _researched, _weapon);
}

std::string Outpost::EffectText(const Tuning& _tuning, const ResearchTopicTuning& _topic)
{
  return std::visit(
    [&_tuning]<typename Effect>(const Effect& _effect) -> std::string
    {
      if constexpr (std::is_same_v<Effect, UpgradeEffect>)
      {
        switch (_effect.target)
        {
        case UpgradeTarget::MiningRig:
          return std::format("Mining Rig income +{}%", _effect.percent);
        case UpgradeTarget::AllHulls:
          return std::format("Hull hit points +{}%", _effect.percent);
        case UpgradeTarget::Weapon:
          return std::format("{} fire rate +{}%", NameOf(_tuning.weapons, _effect.weapon), _effect.percent);
        case UpgradeTarget::Shipyards:
          return std::format("Shipyard build speed +{}%", _effect.percent);
        case UpgradeTarget::AllStructures:
          return std::format("Structure hit points +{}%", _effect.percent);
        case UpgradeTarget::StructureWeapon:
          return std::format("{} fire rate +{}%", NameOf(_tuning.structureWeapons, _effect.structureWeapon), _effect.percent);
        case UpgradeTarget::AllShips:
          return std::format("Ship speed +{}%", _effect.percent);
        case UpgradeTarget::Constructors:
          return std::format("Constructor build and repair rate +{}%", _effect.percent);
        case UpgradeTarget::Asteroids:
          return std::format("Asteroid ore reserves +{}%", _effect.percent);
        }
        return {};
      }
      else if constexpr (std::is_same_v<Effect, HullId>)
        return std::format("Unlocks the {} hull", NameOf(_tuning.hulls, _effect));
      else if constexpr (std::is_same_v<Effect, DriveId>)
        return std::format("Unlocks the {} drive", NameOf(_tuning.drives, _effect));
      else
        return std::format("Unlocks the {}", NameOf(_tuning.weapons, _effect));
    },
    _topic.effect);
}
