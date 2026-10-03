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

// What one player may see of one entity. It carries what the client draws and selects.
struct EntityView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  // The entity's owner; no player for an asteroid or a field.
  PlayerId owner;
  // A ship's design; no design for anything else.
  DesignId design;
  // A warship's hull, which the client draws it by, its drive, which the client colors its exhaust by (ADR-019), and its
  // weapon; none for anything else. A player sees the components of every ship it sees, so the AI can answer the designs
  // it meets (design §10, ADR-020, ADR-024).
  HullId hull;
  DriveId drive;
  WeaponId weapon;
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
  // An enemy structure out of the player's sight, as the player last saw it there (ADR-024). It may have changed, or be
  // gone: the player learns which once it sees the place again.
  bool remembered = false;
  // How far the entity sees under fog of war, which the client draws the fog by; the owner's only, and zero without fog
  // (ADR-024).
  float sightMeters = 0.0f;
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
  // Its tier, and whether it is the gateway that opens it (Phase 1 design §6).
  std::int32_t tier = 1;
  bool gateway = false;
};

// One of the player's saved designs (design §7), as the selection panel and, later, the designer show it.
struct DesignView
{
  DesignId id;
  std::string nameUtf8;
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  // In whole Ore, paid when a Shipyard starts building one (design §5).
  std::int32_t cost = 0;
};

// What the client needs to know of a kind of structure to name it, draw it and place it (design §6).
struct StructureTypeView
{
  StructureKind structure = StructureKind::CommandStation;
  std::string nameUtf8;
  float radiusMeters = 0.0f;
  // Whether a Constructor builds it, and for how much Ore; the Command Station is not built.
  bool buildable = false;
  std::int32_t cost = 0;
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
};

// An entity destroyed in the tick, where it was, so the client can show it go after it has left the snapshot (task 3.5).
struct DestroyedView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
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
  // and what a Constructor costs. Empty and zero when the server has no tuning data.
  float mapSizeMeters = 0.0f;
  std::vector<StructureTypeView> structureTypes;
  std::int32_t constructorCost = 0;
  // The player's components and research topics, in the tuning data's order, with the player's research applied, and how
  // much faster than the base rate its Shipyards build (design §8). Empty and 1 when the server has no tuning data.
  std::vector<HullView> hulls;
  std::vector<DriveView> drives;
  std::vector<WeaponView> weapons;
  std::vector<ResearchTopicView> research;
  double shipyardBuildSpeedFactor = 1.0;
  // The match is over once a player has lost its Command Station (design §6): the winner is the player who still has
  // one, and no player when both fell in the same tick. The world runs on after it (owner, 2026-10-01).
  bool matchOver = false;
  PlayerId winner;
  std::uint64_t matchEndedTick = 0;
  // The match is played under fog of war, which the client draws (ADR-024).
  bool fogOfWar = false;
};
} // namespace Outpost