#pragma once

namespace Outpost
{
// What a player's finished research does to its rates (design §8, Phase 1 design §6), as factors on the base numbers of
// the tuning data. Upgrades change rates, never the size of a hit or a range: hull and structure hit points, a weapon's
// and a structure weapon's fire rate, a Mining Rig's income, a Shipyard's build speed, every ship's speed, the
// Constructors' build and repair rate, and how much ore an asteroid holds. Two topics on one rate add their percentages
// (ADR-033): +15% and +15% make +30%.
struct Upgrades
{
  double hullHitPointsFactor = 1.0;
  // One entry per upgraded weapon; a weapon not listed fires at its base rate.
  std::vector<std::pair<WeaponId, double>> weaponFireRateFactors;
  double miningIncomeFactor = 1.0;
  double shipyardBuildSpeedFactor = 1.0;
  double structureHitPointsFactor = 1.0;
  std::vector<std::pair<StructureWeaponId, double>> structureWeaponFireRateFactors;
  double shipSpeedFactor = 1.0;
  double constructorRateFactor = 1.0;
  // Divides what a rig of the player's draws from its asteroid's reserve for the Ore it earns (Phase 1 design §8).
  double oreReserveFactor = 1.0;

  [[nodiscard]] double FireRateFactor(WeaponId _weapon) const noexcept;
  [[nodiscard]] double FireRateFactor(StructureWeaponId _weapon) const noexcept;

  friend bool operator==(const Upgrades&, const Upgrades&) = default;
};

// The upgrades of every topic in _researched. Throws Neuron::Exception when a topic names nothing in _tuning.
[[nodiscard]] Upgrades UpgradesFrom(const Tuning& _tuning, std::span<const ResearchTopicId> _researched);

// Whether a player who has researched _researched may build with this component: it is one no topic unlocks, or one a
// topic in _researched unlocks (design §7, §8).
[[nodiscard]] bool IsAvailable(const Tuning& _tuning, std::span<const ResearchTopicId> _researched, HullId _hull);
[[nodiscard]] bool IsAvailable(const Tuning& _tuning, std::span<const ResearchTopicId> _researched, DriveId _drive);
[[nodiscard]] bool IsAvailable(const Tuning& _tuning, std::span<const ResearchTopicId> _researched, WeaponId _weapon);

// What a topic does, in the words the HUD shows, such as "Hull hit points +15%" or "Unlocks the Large hull".
[[nodiscard]] std::string EffectText(const Tuning& _tuning, const ResearchTopicTuning& _topic);
} // namespace Outpost
