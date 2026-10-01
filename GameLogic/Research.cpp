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

Outpost::Upgrades Outpost::UpgradesFrom(const Tuning& _tuning, std::span<const ResearchTopicId> _researched)
{
  Upgrades upgrades;
  for (const ResearchTopicId id : _researched)
  {
    const auto* upgrade = std::get_if<UpgradeEffect>(&FindTopic(_tuning, id).effect);
    if (upgrade == nullptr)
      continue;
    const double factor = 1.0 + (upgrade->percent / 100.0);
    switch (upgrade->target)
    {
    case UpgradeTarget::MiningRig:
      upgrades.miningIncomeFactor *= factor;
      break;
    case UpgradeTarget::AllHulls:
      upgrades.hullHitPointsFactor *= factor;
      break;
    case UpgradeTarget::Weapon:
      if (const auto found = std::ranges::find(upgrades.weaponFireRateFactors, upgrade->weapon, &std::pair<WeaponId, double>::first);
          found != upgrades.weaponFireRateFactors.end())
        found->second *= factor;
      else
        upgrades.weaponFireRateFactors.emplace_back(upgrade->weapon, factor);
      break;
    case UpgradeTarget::Shipyards:
      upgrades.shipyardBuildSpeedFactor *= factor;
      break;
    }
  }
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
