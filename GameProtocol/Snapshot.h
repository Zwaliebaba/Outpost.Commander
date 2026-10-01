#pragma once

namespace Outpost
{
enum class EntityKind : std::uint8_t
{
  Ship,
  Structure,
  // An ore asteroid: a Mining Rig can be built on it.
  Asteroid,
  // A non-mineable asteroid field: only an obstacle (design §4).
  AsteroidField
};

// What one player may see of one entity. It carries what the client draws and selects; later tasks add to it, such as
// queues with production (task 4.3).
struct EntityView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  // The entity's owner; no player for an asteroid or a field.
  PlayerId owner;
  // A ship's design; no design for anything else.
  DesignId design;
  // A ship's hull, which the client draws it by; no hull for anything else.
  HullId hull;
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
};

// One of the player's saved designs (design §7), as the selection panel and, later, the designer show it.
struct DesignView
{
  DesignId id;
  std::string nameUtf8;
  HullId hull;
  DriveId drive;
  WeaponId weapon;
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
  // The player's own: its Ore and its saved designs.
  std::int32_t ore = 0;
  std::vector<DesignView> designs;
};
} // namespace Outpost
