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
  // A warship's hull, which the client draws it by; no hull for anything else.
  HullId hull;
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
  // A Shipyard's or the Command Station's jobs, front first, and how far the front one has come in thousandths: zero
  // while it waits for the Ore to start (design §5).
  std::vector<JobView> queue;
  std::int32_t jobPermille = 0;
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

// The world as one player may see it after one tick (ADR-002 decision 4). It is built per player so that fog of war can
// be added on the server alone; in the MVP every player sees everything.
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
};
} // namespace Outpost
