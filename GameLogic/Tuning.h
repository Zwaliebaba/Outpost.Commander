#pragma once

namespace Outpost
{
// Data/Tuning.json as the game holds it (ADR-008). Every number of design §12 and the research table of §8 is here, in
// the units the file states. Converting them into simulation units, such as seconds into ticks, is the simulation's job.
// A quantity the design counts in whole numbers, such as hit points, armor, damage, Ore and percentages, is an integer
// and the loader rejects a fraction; a rate, a time, a distance or a factor is a double.

// Numbers a Defence gun by, so that a structure can name the one it carries. It never crosses the transport.
using StructureWeaponId = Id<struct StructureWeaponTag>;

struct RulesTuning
{
  std::int32_t tickHz = 0;
  std::int32_t startingOre = 0;
  double miningRigOrePerSecondHome = 0.0;
  double miningRigOrePerSecondContested = 0.0;
  double aiReviewIntervalSeconds = 0.0;
};

struct HullTuning
{
  HullId id;
  std::string name;
  std::int32_t hitPoints = 0;
  std::int32_t armor = 0;
  double speedMetersPerSecond = 0.0;
  std::int32_t cost = 0;
  double buildSeconds = 0.0;
  // Provisional until ship sizes are set (G5): the circle a ship of this hull keeps clear, for movement and formation.
  double footprintRadiusMeters = 0.0;
  // Provisional, like the radius: how fast the hull turns before its drive's factor (design §7).
  double turnRateDegreesPerSecond = 0.0;
};

struct DriveTuning
{
  DriveId id;
  std::string name;
  double speedFactor = 0.0;
  double hitPointsFactor = 0.0;
  double turnRateFactor = 0.0;
  std::int32_t cost = 0;
};

struct WeaponTuning
{
  WeaponId id;
  std::string name;
  std::int32_t damage = 0;
  double fireIntervalSeconds = 0.0;
  double rangeMeters = 0.0;
  double splashRadiusMeters = 0.0;
  std::int32_t cost = 0;
};

struct StructureWeaponTuning
{
  StructureWeaponId id;
  std::string name;
  std::int32_t damage = 0;
  double fireIntervalSeconds = 0.0;
  double rangeMeters = 0.0;
};

struct StructureTuning
{
  StructureKind kind = StructureKind::CommandStation;
  std::string name;
  std::int32_t hitPoints = 0;
  std::int32_t armor = 0;
  // Both or neither: a structure a Constructor cannot build, the Command Station, has no cost and no build time.
  std::optional<std::int32_t> cost;
  std::optional<double> buildConstructorSeconds;
  // Not valid when the structure has no weapon.
  StructureWeaponId structureWeapon;
};

// What a research upgrade applies to, and which of its rates it raises (design §8: upgrades change rates, never the size
// of a hit or a range).
enum class UpgradeTarget : std::uint8_t
{
  MiningRig,
  AllHulls,
  Weapon,
  Shipyards
};

enum class UpgradeStat : std::uint8_t
{
  Income,
  HitPoints,
  FireRate,
  BuildSpeed
};

struct UpgradeEffect
{
  UpgradeTarget target = UpgradeTarget::MiningRig;
  // Valid only when the target is UpgradeTarget::Weapon.
  WeaponId weapon;
  UpgradeStat stat = UpgradeStat::Income;
  std::int32_t percent = 0;
};

// An upgrade, or the component the topic unlocks.
using ResearchEffect = std::variant<UpgradeEffect, HullId, DriveId, WeaponId>;

struct ResearchTopicTuning
{
  ResearchTopicId id;
  std::string name;
  std::int32_t cost = 0;
  double researchSeconds = 0.0;
  // The topics that must be researched first. The file calls them "requires", a keyword in C++.
  std::vector<ResearchTopicId> prerequisites;
  ResearchEffect effect;
};

struct Tuning
{
  RulesTuning rules;
  std::vector<HullTuning> hulls;
  std::vector<DriveTuning> drives;
  std::vector<WeaponTuning> weapons;
  std::vector<StructureWeaponTuning> structureWeapons;
  // One per StructureKind, in the order of the file.
  std::vector<StructureTuning> structures;
  std::vector<ResearchTopicTuning> research;
};

// Reads the text of Data/Tuning.json. Throws Neuron::Exception on the first problem, naming where it is, such as
// "hulls[1].armor". Besides types and ranges it checks that identifiers are unique, that every reference names something
// that exists, that each structure kind appears exactly once, and that no research topic requires itself, even through
// others. A member the loader does not know is an error too, so that a misspelled optional member is not ignored.
[[nodiscard]] Tuning LoadTuning(std::string_view _json);
} // namespace Outpost
