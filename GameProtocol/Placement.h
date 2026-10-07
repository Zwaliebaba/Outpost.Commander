#pragma once

namespace Outpost
{
// Where the player's ghost of a structure would stand, before the order is sent (task 4.2). It applies the server's
// rules to what the snapshot shows (ADR-016): the footprint stays inside the map and overlaps no asteroid, field or
// structure, and a Mining Rig snaps to a free ore asteroid within RIG_SNAP_METERS of the cursor. On a map with sectors
// (ADR-056), a Relay snaps to the node site of the sector under the cursor, which must be adjacent to one the player
// holds, a rig's asteroid and a Shipyard must be in a sector the player holds (Phase 4 design §6); and a Relay waits while
// the player is at its Command Station's node cap (Phase 3 design §7), or while pirates guard the sector (Phase 4 design
// §8). The server decides; this only lets the ghost show green or red, and a rig on its
// asteroid or a Relay on its node.
struct GhostPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
  bool valid = false;
};

// The ghost of _type with the cursor at _cursor on the ground, over the snapshot's _entities on a map _mapSizeMeters
// across, for _player, with the snapshot's _sectors and the player's _nodeCap, or none for zero.
[[nodiscard]] GhostPlacement PlaceGhost(const StructureTypeView& _type, PlanePosition _cursor, std::span<const EntityView> _entities,
                                        float _mapSizeMeters, std::span<const SectorView> _sectors = {}, PlayerId _player = {},
                                        std::int32_t _nodeCap = 0);

// The nodes _player holds, with those it has taken with a Relay still under construction, which its Command Station's
// level caps (Phase 3 design §7), as the server counts them.
[[nodiscard]] std::int32_t NodesTaken(std::span<const SectorView> _sectors, std::span<const EntityView> _entities,
                                      PlayerId _player) noexcept;

// Whether _player may claim no further node: it has taken _nodeCap of them. Never for a cap of zero, which is a map
// without territory.
[[nodiscard]] bool AtNodeCap(std::span<const SectorView> _sectors, std::span<const EntityView> _entities, PlayerId _player,
                             std::int32_t _nodeCap) noexcept;
} // namespace Outpost