#pragma once

namespace Outpost
{
// OutpostCommander/Assets/Tuning.json as the game holds it (ADR-008). Every number of design §12 and the research table
// of §8 is here, in the units the file states. Converting them into simulation units, such as seconds into ticks, is the
// simulation's job. A quantity the design counts in whole numbers, such as hit points, armor, damage, Ore and
// percentages, is an integer and the loader rejects a fraction; a rate, a time, a distance or a factor is a double.

// Numbers a Defence gun by, so that a structure can name the one it carries. It never crosses the transport.
using StructureWeaponId = Id<struct StructureWeaponTag>;

struct RulesTuning
{
  std::int32_t tickHz = 0;
  std::int32_t startingOre = 0;
  // Constructors each player starts with, beside its Command Station (design §6).
  std::int32_t startingConstructors = 0;
  // A Mining Rig's income on each of Phase 1 design §8's rings of ore asteroids, home outward.
  double miningRigOrePerSecondHome = 0.0;
  double miningRigOrePerSecondNear = 0.0;
  double miningRigOrePerSecondContested = 0.0;
  double miningRigOrePerSecondRich = 0.0;
  // What a rig earns of its asteroid's rate once the asteroid's reserve has run out, in percent (Phase 1 design §8).
  std::int32_t exhaustedYieldPercent = 0;
  // What each level above the first adds to a structure's hit points, in percent of its kind's base hit points (Phase 3
  // design §4, gate K6). It adds to research's percents (ADR-064).
  std::int32_t levelHitPointsPercent = 0;
};

// How far each side sees under fog of war (ADR-024). An armed ship or structure sees its weapon's range and the margin
// beyond it, so it always sees what it can shoot; one without a weapon sees unarmedMeters. A shooter is seen by the side
// it hits for shotRevealSeconds after each hit.
struct SightTuning
{
  double weaponMarginMeters = 0.0;
  double unarmedMeters = 0.0;
  double shotRevealSeconds = 0.0;
};

// How territory plays on a map with sectors (Phase 2 design §5, §6, §8, ADR-056, ADR-057): what share of its income a
// sector cut off from its holder's home sector earns, in percent, and how near an enemy warship has to be to suppress a
// Relay. Domination: the tickets each player starts with, and every drainIntervalSeconds, the tickets a player who holds
// fewer nodes loses for each node it is behind: drainTicketsPerNodeDifference × difference, whatever the map's nodes.
struct TerritoryTuning
{
  std::int32_t cutOffIncomePercent = 0;
  double suppressionRadiusMeters = 0.0;
  std::int32_t tickets = 0;
  double drainIntervalSeconds = 0.0;
  std::int32_t drainTicketsPerNodeDifference = 0;
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
  // The ship's size (gate G5, decided 2026-10-01): the circle a ship of this hull keeps clear, for movement and formation,
  // and the spacing a Missile Rack's splash reaches across.
  double footprintRadiusMeters = 0.0;
  // How fast the hull turns before its drive's factor (design §7); final, like the radius (owner, 2026-10-01).
  double turnRateDegreesPerSecond = 0.0;
  // What a warship of this hull takes of its player's fleet cap (Phase 4 design §5); 0 when the data sets none.
  std::int32_t commandPoints = 0;
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

// A module, a design's optional fourth component (Phase 2 design §10, ADR-058): how far a ship with it sees, whatever its
// weapon, the factor on its speed, and what it adds to the design's cost. No research unlocks one: every module is
// available from the start.
struct ModuleTuning
{
  ModuleId id;
  std::string name;
  double sightMeters = 0.0;
  double speedFactor = 0.0;
  std::int32_t cost = 0;
};

// The Constructor, the one fixed design (design §7): no weapon, built at the Command Station, and the rates it builds and
// repairs at (gate G8, a provisional baseline the owner set on 2026-10-01).
struct ConstructorTuning
{
  std::int32_t hitPoints = 0;
  std::int32_t armor = 0;
  double speedMetersPerSecond = 0.0;
  std::int32_t cost = 0;
  double buildSeconds = 0.0;
  // Final, as are the hulls' (owner, 2026-10-01).
  double footprintRadiusMeters = 0.0;
  double turnRateDegreesPerSecond = 0.0;
  // One Constructor builds a structure in its buildConstructorSeconds; each further one on the site adds this share of
  // one more.
  double extraConstructorBuildShare = 0.0;
  // Hit points one Constructor restores each second, as a percentage of the target's maximum.
  double repairPercentPerSecond = 0.0;
};

struct StructureWeaponTuning
{
  StructureWeaponId id;
  std::string name;
  std::int32_t damage = 0;
  double fireIntervalSeconds = 0.0;
  double rangeMeters = 0.0;
};

// One level above the first that a structure is upgraded to (Phase 3 design §4, ADR-064): its Ore, paid when the upgrade is
// ordered, and the time it takes to build itself, which no Constructor shortens (owner, 2026-10-04).
struct StructureLevelTuning
{
  std::int32_t cost = 0;
  double buildSeconds = 0.0;
  // A Shipyard's: the hulls it builds from this level on (Phase 3 design §5).
  std::vector<HullId> hulls;
  // A Research Lab's (Phase 3 design §6): the research tier this level opens, or 0; the topics that must be researched
  // before it is ordered, which the file calls "requires"; and how many topics the Lab researches at once from this level
  // on, or 0 where the level does not change it.
  std::int32_t opensTier = 0;
  std::vector<ResearchTopicId> prerequisites;
  std::int32_t researchSlots = 0;
  // A Command Station's (Phase 3 design §7): the nodes its player may hold from this level on, home included, and the
  // Defence guns it carries; 0 where the level does not change them. And the command points its player's warships may
  // take from this level on (Phase 4 design §5), 0 where the level does not change them.
  std::int32_t nodes = 0;
  std::int32_t guns = 0;
  std::int32_t commandPoints = 0;
};

// The most levels a structure has: the owner's models have five (ADR-045).
inline constexpr std::int32_t MAXIMUM_STRUCTURE_LEVEL = 5;

struct StructureTuning
{
  StructureKind kind = StructureKind::CommandStation;
  std::string name;
  std::int32_t hitPoints = 0;
  std::int32_t armor = 0;
  // The circle it blocks and that no other structure may overlap (design §6). Final, as are the hulls' (owner,
  // 2026-10-01).
  double footprintRadiusMeters = 0.0;
  // Both or neither: a structure a Constructor cannot build, the Command Station, has no cost and no build time.
  std::optional<std::int32_t> cost;
  std::optional<double> buildConstructorSeconds;
  // Not valid when the structure has no weapon.
  StructureWeaponId structureWeapon;
  // The levels it is upgraded to, level 2 first; none for a kind that does not grow (Phase 3 design §4).
  std::vector<StructureLevelTuning> levels;
  // A Shipyard's: the hulls it builds at level 1. A Shipyard that names no hull at any level builds every hull at level 1
  // (Phase 3 design §5).
  std::vector<HullId> hulls;
  // A Command Station's at level 1 (Phase 3 design §7): the nodes its player may hold, home included, 0 for no cap; and
  // the Defence guns it carries, 1 when it names none. And the command points its player's warships may take, 0 for no
  // cap (Phase 4 design §5).
  std::int32_t nodes = 0;
  std::int32_t guns = 0;
  std::int32_t commandPoints = 0;

  // The highest level it reaches.
  [[nodiscard]] std::int32_t TopLevel() const noexcept
  {
    return 1 + static_cast<std::int32_t>(levels.size());
  }
};

// What a research upgrade applies to, and which of its rates it raises (design §8: upgrades change rates, never the size
// of a hit or a range). Phase 1's tiers add structures' hit points, a structure weapon's fire rate, every ship's speed,
// the Constructors' build and repair rate, and the ore asteroids hold (Phase 1 design §6, ADR-033).
enum class UpgradeTarget : std::uint8_t
{
  MiningRig,
  AllHulls,
  Weapon,
  Shipyards,
  AllStructures,
  StructureWeapon,
  AllShips,
  Constructors,
  Asteroids
};

enum class UpgradeStat : std::uint8_t
{
  Income,
  HitPoints,
  FireRate,
  BuildSpeed,
  Speed,
  BuildRate,
  OreReserve
};

struct UpgradeEffect
{
  UpgradeTarget target = UpgradeTarget::MiningRig;
  // Valid only when the target is UpgradeTarget::Weapon, and UpgradeTarget::StructureWeapon.
  WeaponId weapon;
  StructureWeaponId structureWeapon;
  UpgradeStat stat = UpgradeStat::Income;
  std::int32_t percent = 0;
};

// An upgrade, or the component the topic unlocks.
using ResearchEffect = std::variant<UpgradeEffect, HullId, DriveId, WeaponId>;

// The research tiers (Phase 1 design §6): tier 1 is open from the start, and each tier after it is opened by a level of the
// Research Lab (Phase 3 design §6).
inline constexpr std::int32_t RESEARCH_TIERS = 3;

struct ResearchTopicTuning
{
  ResearchTopicId id;
  std::string name;
  std::int32_t tier = 1;
  std::int32_t cost = 0;
  double researchSeconds = 0.0;
  // The topics that must be researched first. The file calls them "requires", a keyword in C++.
  std::vector<ResearchTopicId> prerequisites;
  ResearchEffect effect;
};

// A starting design's short name (ADR-069): the hull, drive and weapon no research unlocks, and the name each player's copy
// is saved under, such as "Swarm". A starting design the file does not name takes its components' name (DesignName).
struct StartingDesignTuning
{
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  std::string name;
};

struct Tuning
{
  RulesTuning rules;
  SightTuning sight;
  TerritoryTuning territory;
  std::vector<HullTuning> hulls;
  std::vector<DriveTuning> drives;
  std::vector<WeaponTuning> weapons;
  std::vector<ModuleTuning> modules;
  ConstructorTuning constructor;
  std::vector<StructureWeaponTuning> structureWeapons;
  // One per StructureKind, in the order of the file.
  std::vector<StructureTuning> structures;
  std::vector<ResearchTopicTuning> research;
  // Optional in the file; none names no starting design.
  std::vector<StartingDesignTuning> startingDesigns;
};

// The level a Shipyard must be at to build a hull of _hull (Phase 3 design §5, gate K1): the level that names it, or 1
// when no level does.
[[nodiscard]] std::int32_t ShipyardLevelFor(const Tuning& _tuning, HullId _hull) noexcept;

// The level a Research Lab must be at to research a topic of _tier (Phase 3 design §6, gate K2): the level that opens it,
// or 1 for tier 1.
[[nodiscard]] std::int32_t LabLevelFor(const Tuning& _tuning, std::int32_t _tier) noexcept;

// The highest research tier a Research Lab at _level has opened, 1 at least, and how many topics it researches at once,
// 1 at least (Phase 3 design §6).
[[nodiscard]] std::int32_t OpenTier(const Tuning& _tuning, std::int32_t _level) noexcept;
[[nodiscard]] std::int32_t ResearchSlots(const Tuning& _tuning, std::int32_t _level) noexcept;

// The nodes a player whose Command Station is at _level may hold, home included (Phase 3 design §7, gate K4); level 1's
// for a player without a station. Zero when the data sets no cap.
[[nodiscard]] std::int32_t NodeCap(const Tuning& _tuning, std::int32_t _level) noexcept;

// The command points the warships of a player whose Command Station is at _level may take (Phase 4 design §5, gate L2);
// level 1's for a player without a station. Zero when the data sets no cap.
[[nodiscard]] std::int32_t FleetCap(const Tuning& _tuning, std::int32_t _level) noexcept;

// The Defence guns a structure of _kind carries at _level (Phase 3 design §7, gate K6): a Command Station's from its
// levels, and one for any other armed structure.
[[nodiscard]] std::int32_t StationGuns(const Tuning& _tuning, StructureKind _kind, std::int32_t _level) noexcept;

// Reads the text of OutpostCommander/Assets/Tuning.json. Throws Neuron::Exception on the first problem, naming where it
// is, such as "hulls[1].armor". Besides types and ranges it checks that identifiers are unique, that every reference
// names something that exists, that each structure kind appears exactly once, and that no research topic requires
// itself, even through others. And the tiers hold: a topic requires none of a later tier, and each tier after the first is
// opened by one level of the Research Lab, in order (ADR-033). Only the Command Station, the
// Shipyard and the Research Lab have levels, up to MAXIMUM_STRUCTURE_LEVEL (ADR-064), and a Shipyard that names the hulls
// it builds names each once, at level 1 or at one of its levels. A member the loader does not know is
// an error too, so that a misspelled optional member is not ignored.
[[nodiscard]] Tuning LoadTuning(std::string_view _json);
} // namespace Outpost