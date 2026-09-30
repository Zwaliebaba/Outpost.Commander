#pragma once

namespace Outpost
{
enum class EntityKind : std::uint8_t
{
  Ship,
  Structure,
  Asteroid
};

// What one player may see of one entity. It carries what milestone 2 draws and selects; later tasks add to it, such as
// hit points with combat (task 3.3) and queues with production (task 4.3).
struct EntityView
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  // The entity's owner; no player for an asteroid.
  PlayerId owner;
  // A ship's design; no design for a structure or an asteroid.
  DesignId design;
  // Meaningful for a structure only.
  StructureKind structure = StructureKind::CommandStation;
  PlanePosition position;
  // Where the entity faces, counterclockwise from +x when seen from above.
  float headingRadians = 0.0f;
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
