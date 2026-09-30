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

// What one player may see of one entity. It carries what milestone 2 draws and selects; later tasks add to it, such as
// hit points with combat (task 3.3) and queues with production (task 4.3).
struct EntityView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  // The entity's owner; no player for an asteroid or a field.
  PlayerId owner;
  // A ship's design; no design for anything else.
  DesignId design;
  // Meaningful for a structure only.
  StructureKind structure = StructureKind::CommandStation;
  PlanePosition position;
  // Where the entity faces, counterclockwise from +x when seen from above.
  float headingRadians = 0.0f;
  // The circle it blocks: an asteroid's or a field's now, a ship's once its size is set (task 2.4).
  float radiusMeters = 0.0f;
};

// The world as one player may see it after one tick (ADR-002 decision 4). It is built per player so that fog of war can
// be added on the server alone; in the MVP every player sees everything.
struct Snapshot
{
  std::uint64_t tick = 0;
  PlayerId player;
  std::vector<EntityView> entities;
};
} // namespace Outpost
