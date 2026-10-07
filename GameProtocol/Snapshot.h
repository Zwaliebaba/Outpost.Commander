#pragma once

namespace Outpost
{
// Hit points, armor and damage count in hundredths of a point (ADR-014): a drive's factor and the armor rule's quarter of
// a hit then stay whole numbers, which the tuning data's integers are not once they are multiplied.
inline constexpr std::int32_t HUNDREDTHS = 100;

enum class EntityKind : std::uint8_t
{
  Ship,
  Structure,
  // An ore asteroid: a Mining Rig can be built on it.
  Asteroid,
  // A non-mineable asteroid field: only an obstacle (design §4).
  AsteroidField
};

// What a ship is for. A warship is of a saved design; a Constructor is the one fixed design, which builds and repairs and
// has no weapon (design §7).
enum class ShipRole : std::uint8_t
{
  Warship,
  Constructor
};

// One job in a Shipyard's or the Command Station's queue (design §6): a ship of one of the player's designs, or a
// Constructor.
struct JobView
{
  ShipRole role = ShipRole::Warship;
  // The design of a warship; no design for a Constructor.
  DesignId design;

  friend bool operator==(const JobView&, const JobView&) = default;
};

// Jobs a Shipyard's or the Command Station's queue holds (design §6).
inline constexpr size_t QUEUE_LIMIT = 5;

// How far a structure's construction or a queue's front job has come, in thousandths.
inline constexpr std::int32_t PERMILLE = 1000;

// A standing order a warship keeps until given another (Phase 2 design §9, ADR-059).
enum class StandingOrder : std::uint8_t
{
  None,
  HoldSector,
  Patrol
};

// What one player may see of one entity. It carries what the client draws and selects.
struct EntityView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  // The entity's owner; no player for an asteroid or a field.
  PlayerId owner;
  // A ship's design; no design for anything else.
  DesignId design;
  // A warship's hull, which the client draws it by, its drive, which the client colors its exhaust by (ADR-019), its
  // weapon and its module, if it has one (Phase 2 design §10); none for anything else. A player sees the components of
  // every ship it sees, so the AI can answer the designs it meets (design §10, ADR-020, ADR-024).
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  ModuleId module;
  // Meaningful for a ship only.
  ShipRole role = ShipRole::Warship;
  // Meaningful for a structure only.
  StructureKind structure = StructureKind::CommandStation;
  PlanePosition position;
  // Where the entity faces, counterclockwise from +x when seen from above.
  float headingRadians = 0.0f;
  // The circle it blocks: an asteroid's or a field's, or a ship's footprint (ADR-010).
  float radiusMeters = 0.0f;
  // Hit points left and at full strength, in hundredths (ADR-014). Both are zero for what combat cannot touch: an asteroid,
  // a field, and a ship or structure placed without any.
  std::int32_t hitPointsHundredths = 0;
  std::int32_t maxHitPointsHundredths = 0;
  // A structure's construction, in thousandths: PERMILLE once it is built, as everything else is. A structure under
  // construction does nothing but stand there and block (design §6).
  std::int32_t builtPermille = PERMILLE;
  // A structure's level, from 1 (Phase 3 design §4, ADR-064); 1 for anything else. An enemy structure the player remembers
  // keeps the level it was last seen at.
  std::int32_t level = 1;
  // The next level's construction, in thousandths, while a finished structure is being upgraded; none otherwise. It is
  // shown as construction is, to whoever sees the structure.
  std::optional<std::int32_t> upgradePermille;
  // A finished Shipyard's number among its owner's, 1 for the first finished, never reused; and how many ships it has
  // built this match (Phase 1 design §11). The owner's only; zero for anything else.
  std::uint32_t shipyardNumber = 0;
  std::uint32_t shipsBuilt = 0;
  // A Shipyard's or the Command Station's jobs, front first, and how far the front one has come in thousandths: zero
  // while it waits for the Ore to start (design §5).
  std::vector<JobView> queue;
  // A Research Lab's topics, front first; the front one's progress is jobPermille, as a queue's is (design §8). Under fog
  // of war a player sees only its own queues (ADR-024).
  std::vector<ResearchTopicId> research;
  std::int32_t jobPermille = 0;
  // A Research Lab with a second slot: how far the second topic of its queue has come, in thousandths, while it is
  // researched beside the first (Phase 3 design §6); zero otherwise.
  std::int32_t secondJobPermille = 0;
  // An enemy structure out of the player's sight, as the player last saw it there (ADR-024). It may have changed, or be
  // gone: the player learns which once it sees the place again.
  bool remembered = false;
  // How far the entity sees under fog of war, which the client draws the fog by; the owner's only, and zero without fog
  // (ADR-024).
  float sightMeters = 0.0f;
  // A ship's standing order, which only its owner sees (ADR-059).
  StandingOrder standing = StandingOrder::None;
  // An ore asteroid's Ore left, in hundredths, and a Mining Rig's asteroid's, as the player knows it: under fog of war,
  // as it last saw it. None for one it has never seen, or that never runs out (Phase 1 design §8).
  std::optional<std::int64_t> oreReserveHundredths;

  friend bool operator==(const EntityView&, const EntityView&) = default;
};

// A hull as its player has it: its numbers after the player's research, and whether the player may build it yet (design
// §7, §8). The designer derives a design's stats from these with DesignStatsOf, as the server does (ADR-017).
struct HullView
{
  HullId id;
  std::string nameUtf8;
  std::int32_t hitPointsHundredths = 0;
  std::int32_t armorHundredths = 0;
  double speedMetersPerSecond = 0.0;
  double turnRateDegreesPerSecond = 0.0;
  double footprintRadiusMeters = 0.0;
  std::int32_t cost = 0;
  double buildSeconds = 0.0;
  // False until research unlocks it.
  bool available = false;
  // The level a Shipyard must be at to build it (Phase 3 design §5).
  std::int32_t shipyardLevel = 1;
  // What a warship of this hull takes of its player's fleet cap (Phase 4 design §5); 0 when the match sets no cap.
  std::int32_t commandPoints = 0;
};

struct DriveView
{
  DriveId id;
  std::string nameUtf8;
  double speedFactor = 0.0;
  double hitPointsFactor = 0.0;
  double turnRateFactor = 0.0;
  std::int32_t cost = 0;
  bool available = false;
};

struct WeaponView
{
  WeaponId id;
  std::string nameUtf8;
  std::int32_t damageHundredths = 0;
  double fireIntervalSeconds = 0.0;
  double rangeMeters = 0.0;
  // Zero for a weapon without splash.
  double splashRadiusMeters = 0.0;
  std::int32_t cost = 0;
  bool available = false;
};

// A module (Phase 2 design §10, ADR-058): what it does to a ship of a design that carries it. The Sensor Array is the
// first: it sees sightMeters, whatever its weapon, and is slowed by its speed factor.
struct ModuleView
{
  ModuleId id;
  std::string nameUtf8;
  // How far a ship with it sees under fog of war, when that is further than its weapon lets it; zero for none.
  double sightMeters = 0.0;
  double speedFactor = 1.0;
  std::int32_t cost = 0;
  bool available = false;
};

// A research topic (design §8): what it does in words, what it costs, what it needs first, and whether the player has
// it.
struct ResearchTopicView
{
  ResearchTopicId id;
  std::string nameUtf8;
  std::string effectUtf8;
  std::int32_t cost = 0;
  double researchSeconds = 0.0;
  std::vector<ResearchTopicId> prerequisites;
  bool researched = false;
  // The component the topic unlocks, if it unlocks one, which the designer names on the component while it is locked
  // (Phase 1 design §11).
  HullId unlocksHull;
  DriveId unlocksDrive;
  WeaponId unlocksWeapon;
  // Its tier (Phase 1 design §6), which a level of the Research Lab opens (Phase 3 design §6).
  std::int32_t tier = 1;
};

// One of the player's saved designs (design §7), as the selection panel and, later, the designer show it.
struct DesignView
{
  DesignId id;
  std::string nameUtf8;
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  // None for a design without a module (Phase 2 design §10).
  ModuleId module;
  // In whole Ore, paid when a Shipyard starts building one (design §5).
  std::int32_t cost = 0;
};

// What the client needs to know of a kind of structure to name it, draw it and place it (design §6).
// One level above the first that a kind of structure is upgraded to (Phase 3 design §4, ADR-064): what the upgrade costs,
// how long one Constructor takes to build it, and the structure's full hit points once it is in, with the player's
// research.
struct StructureLevelView
{
  std::int32_t cost = 0;
  double buildSeconds = 0.0;
  std::int32_t maxHitPointsHundredths = 0;
  // A Research Lab's level (Phase 3 design §6): the tier it opens, or 0; how many topics the Lab researches at once from
  // it on, or 0 where it does not change that; and the topics that must be researched before it is ordered.
  std::int32_t opensTier = 0;
  std::int32_t researchSlots = 0;
  std::vector<ResearchTopicId> prerequisites;
  // A Command Station's level (Phase 3 design §7): the nodes its player may hold from it on, and its Defence guns; 0 where
  // it does not change them. And the command points its player's warships may take from it on (Phase 4 design §5), 0
  // where it does not change them.
  std::int32_t nodes = 0;
  std::int32_t guns = 0;
  std::int32_t commandPoints = 0;

  friend bool operator==(const StructureLevelView&, const StructureLevelView&) = default;
};

struct StructureTypeView
{
  StructureKind structure = StructureKind::CommandStation;
  std::string nameUtf8;
  float radiusMeters = 0.0f;
  // Whether a Constructor builds it, and for how much Ore; the Command Station is not built.
  bool buildable = false;
  std::int32_t cost = 0;
  // The levels it is upgraded to, level 2 first; none for a kind that does not grow.
  std::vector<StructureLevelView> levels;
};

// One of the map's sectors and who holds it (Phase 2 design §4–§6, ADR-056). Every player sees every sector's holder,
// fog of war or not: the territory is the map both sides play for.
struct SectorView
{
  std::int32_t id = 0;
  std::string nameUtf8;
  float minXMeters = 0.0f;
  float maxXMeters = 0.0f;
  float minZMeters = 0.0f;
  float maxZMeters = 0.0f;
  // Its node site, where a Relay stands, or the Command Station in a home sector.
  PlanePosition node;
  std::vector<std::int32_t> adjacent;
  // The owner of the finished Relay or the Command Station on its node; no player while the node is free.
  PlayerId holder;
  // Its Relay has an enemy warship within the suppression radius and none of its holder's: it earns nothing and the Relay
  // sees only as a structure does, but the sector is still held.
  bool suppressed = false;
  // It is held but no longer linked to its holder's home sector through the sectors its holder holds: it earns the
  // tuning data's share.
  bool cutOff = false;
  // A pirate structure stands in it, so that it cannot be claimed (Phase 4 design §8). Every player sees it, as it sees
  // the holder, though the pirates themselves are under fog of war like an enemy (ADR-073).
  bool guarded = false;

  // Whether _position is in the sector, its borders included, as the map's sector is.
  [[nodiscard]] bool Contains(PlanePosition _position) const noexcept
  {
    return _position.xMeters >= minXMeters && _position.xMeters <= maxXMeters && _position.zMeters >= minZMeters &&
           _position.zMeters <= maxZMeters;
  }

  friend bool operator==(const SectorView&, const SectorView&) = default;
};

// The first of _sectors that holds _position, or nullptr.
[[nodiscard]] inline const SectorView* FindSector(std::span<const SectorView> _sectors, PlanePosition _position) noexcept
{
  const auto found = std::ranges::find_if(_sectors, [_position](const SectorView& _sector) { return _sector.Contains(_position); });
  return found != _sectors.end() ? &*found : nullptr;
}

// How a match ended (Phase 2 design §8): a player lost its Command Station and every finished Shipyard (Phase 1 design
// §4), or a player's tickets ran out because it held fewer nodes (ADR-057).
enum class MatchEnding : std::uint8_t
{
  LostProduction,
  Domination
};

// One player's tickets (ADR-057).
struct TicketsView
{
  PlayerId player;
  std::int32_t tickets = 0;

  friend bool operator==(const TicketsView&, const TicketsView&) = default;
};

// A shot fired in the tick. Hits are instant (design §7), so this is presentation only: where the shot went from and to,
// at the moment it was fired (task 3.5).
struct ShotView
{
  EntityId shooter;
  EntityId target;
  WeaponId weapon;
  PlanePosition from;
  PlanePosition to;
  // How far its splash reached around `to`; zero for a weapon without splash.
  float splashRadiusMeters = 0.0f;
  // Which of the shooter's guns fired it: 0 for a ship's and a structure's first, and 1 on for a Command Station's further
  // Defence guns (Phase 3 design §7), which the client draws from hardpoints of their own.
  std::uint8_t gun = 0;
};

// An entity destroyed in the tick, where it was, so the client can show it go after it has left the snapshot (task 3.5).
struct DestroyedView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  // Meaningful for a structure only, such as a Mining Rig, which the client alerts its player to (ADR-059).
  StructureKind structure = StructureKind::CommandStation;
  PlayerId owner;
  HullId hull;
  PlanePosition position;
  float headingRadians = 0.0f;
  float radiusMeters = 0.0f;
};

// The world as one player may see it after one tick (ADR-002 decision 4). Under fog of war it holds the player's own
// entities, the asteroids and fields, the enemy entities the player sees and the enemy structures it remembers, and the
// shots and destructions it sees (ADR-024).
struct Snapshot
{
  std::uint64_t tick = 0;
  PlayerId player;
  std::vector<EntityView> entities;
  // What happened in this tick that the entities alone do not show.
  std::vector<ShotView> shots;
  std::vector<DestroyedView> destroyed;
  // The player's own: its Ore, whole, what its Mining Rigs earn each second in hundredths of an Ore, and its saved
  // designs.
  std::int32_t ore = 0;
  std::int32_t oreIncomeHundredthsPerSecond = 0;
  std::vector<DesignView> designs;
  // The match's rules the client shows: the map, a square of this side centered on the origin; every kind of structure;
  // and what a Constructor costs, and how long the Command Station takes to build one (ADR-068). Empty and zero when the
  // server has no tuning data.
  float mapSizeMeters = 0.0f;
  std::vector<StructureTypeView> structureTypes;
  std::int32_t constructorCost = 0;
  double constructorBuildSeconds = 0.0;
  // The player's components and research topics, in the tuning data's order, with the player's research applied, and how
  // much faster than the base rate its Shipyards build (design §8). Empty and 1 when the server has no tuning data.
  std::vector<HullView> hulls;
  std::vector<DriveView> drives;
  std::vector<WeaponView> weapons;
  std::vector<ModuleView> modules;
  std::vector<ResearchTopicView> research;
  double shipyardBuildSpeedFactor = 1.0;
  // The highest research tier the player's finished Research Lab has opened by its level, and 1 without one (Phase 3
  // design §6).
  std::int32_t researchTier = 1;
  // On a map with territory, the nodes the player may hold, home included, by its Command Station's level, or level 1's
  // without one (Phase 3 design §7); zero otherwise.
  std::int32_t nodeCap = 0;
  // The command points the player's warships take, those its Shipyards have started included, and the most they may take
  // by its Command Station's level, or level 1's without one (Phase 4 design §5); the cap is zero when the match sets none.
  std::int32_t commandPoints = 0;
  std::int32_t fleetCap = 0;
  // The match is over once a player has neither a Command Station nor a finished Shipyard (Phase 1 design §4): the winner
  // is the player who still has one, and no player when both lost theirs in the same tick. The world runs on after it
  // (owner, 2026-10-01).
  bool matchOver = false;
  PlayerId winner;
  std::uint64_t matchEndedTick = 0;
  // How it ended, once it has.
  MatchEnding ending = MatchEnding::LostProduction;
  // The match is played under fog of war, which the client draws (ADR-024).
  bool fogOfWar = false;
  // The map's sectors, in the map's order, and who holds each (ADR-056); none on a map without them, which plays without
  // territory. With them, every player's tickets, in player order, which both players see (ADR-057), and the tickets each
  // started with.
  std::vector<SectorView> sectors;
  std::vector<TicketsView> tickets;
  std::int32_t startingTickets = 0;
};
} // namespace Outpost