#pragma once

namespace Outpost
{
// Where the player's ghost of a structure would stand, before the order is sent. It applies the server's
// rules to what the snapshot shows (ADR-016): the footprint stays inside the map and overlaps no asteroid, field, structure
// or derelict (ADR-074), and a Mining Rig snaps to a free ore asteroid within RIG_SNAP_METERS of the cursor. On a map with
// sectors (ADR-056), a Relay snaps to the node site of the sector under the cursor, which must be adjacent to one the
// player holds, a rig's asteroid and what NeedsHeldSector names must be in a sector the player holds (ADR-056 decision 5); and a Relay waits
// while the player is at its Command Station's node cap (ADR-064 decision 11), or while pirates guard the sector
// (ADR-073 decision 7). The server decides; this only lets the ghost show green or red, and a rig on its asteroid or a
// Relay on its node.
struct GhostPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
  bool valid = false;
};

// Whether a structure of _kind stands only in a sector its player holds, on a map with sectors (ADR-056 decision 5): a
// Shipyard and a Repair Bay, so that production follows territory, and a Research Lab and a Defence Platform, so that
// neither creeps into another player's sectors. A Mining Rig's asteroid must be in one too, which PlaceGhost judges by
// the asteroid.
[[nodiscard]] constexpr bool NeedsHeldSector(StructureKind _kind) noexcept
{
  return _kind == StructureKind::Shipyard || _kind == StructureKind::RepairBay || _kind == StructureKind::ResearchLab ||
         _kind == StructureKind::DefensePlatform;
}

// The ghost of _type with the cursor at _cursor on the ground, over the snapshot's _entities on a map _mapSizeMeters
// across, for _player, with the snapshot's _sectors and the player's _nodeCap, or none for zero.
[[nodiscard]] GhostPlacement PlaceGhost(const StructureTypeView& _type, PlanePosition _cursor, std::span<const EntityView> _entities,
                                        float _mapSizeMeters, std::span<const SectorView> _sectors = {}, PlayerId _player = {},
                                        std::int32_t _nodeCap = 0);

// Whether _player may claim _sector's node now with a Relay _relayRadiusMeters in radius, by what the snapshot shows: the
// node is free and nothing stands on it, a sector next to it is the player's, no pirate guards it, and the player is under
// its _nodeCap (ADR-056, ADR-073, ADR-064 decision 11). The Relay's ghost is green, and the world and the minimap mark a node
// the player could claim, by this one rule (ADR-081 decision 2).
[[nodiscard]] bool CanClaim(const SectorView& _sector, float _relayRadiusMeters, std::span<const EntityView> _entities,
                            float _mapSizeMeters, std::span<const SectorView> _sectors, PlayerId _player, std::int32_t _nodeCap);

// The nodes _player holds, with those it has taken with a Relay still under construction, which its Command Station's
// level caps (ADR-056 decision 4), as the server counts them.
[[nodiscard]] std::int32_t NodesTaken(std::span<const SectorView> _sectors, std::span<const EntityView> _entities,
                                      PlayerId _player) noexcept;

// Whether _player may claim no further node: it has taken _nodeCap of them. Never for a cap of zero, which is a map
// without territory.
[[nodiscard]] bool AtNodeCap(std::span<const SectorView> _sectors, std::span<const EntityView> _entities, PlayerId _player,
                             std::int32_t _nodeCap) noexcept;
} // namespace Outpost