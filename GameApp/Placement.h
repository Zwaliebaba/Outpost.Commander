#pragma once

namespace Outpost
{
// Where the player's ghost of a structure would stand, before the order is sent (task 4.2). It applies the server's
// rules to what the snapshot shows (ADR-016): the footprint stays inside the map and overlaps no asteroid, field or
// structure, and a Mining Rig snaps to a free ore asteroid within RIG_SNAP_METERS of the cursor. The server decides; this
// only lets the ghost show green or red, and a rig on its asteroid.
struct GhostPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
  bool valid = false;
};

// The ghost of _type with the cursor at _cursor on the ground, over the snapshot's _entities on a map _mapSizeMeters
// across.
[[nodiscard]] GhostPlacement PlaceGhost(const StructureTypeView& _type, PlanePosition _cursor, std::span<const EntityView> _entities,
                                        float _mapSizeMeters);
} // namespace Outpost
