#include "pch.h"
#include "TerritoryMarks.h"

#include <algorithm>
#include <cmath>
#include <tuple>

Outpost::TerritoryMarks Outpost::MarkTerritory(const Snapshot& _newest, std::span<const EntityView> _entities)
{
  TerritoryMarks marks;
  if (_newest.sectors.empty())
    return marks;

  // The lattice: every side of every sector, each once where two sectors share it.
  std::vector<std::tuple<float, float, float, float>> drawn;
  const auto addBorder = [&](PlanePosition _from, PlanePosition _to)
  {
    // The same side seen from either sector, its ends in one order.
    if (std::tie(_to.xMeters, _to.zMeters) < std::tie(_from.xMeters, _from.zMeters))
      std::swap(_from, _to);
    const std::tuple<float, float, float, float> key{_from.xMeters, _from.zMeters, _to.xMeters, _to.zMeters};
    if (std::ranges::find(drawn, key) != drawn.end())
      return;
    drawn.push_back(key);
    marks.lines.push_back({.from = _from, .to = _to});
  };
  for (const SectorView& sector : _newest.sectors)
  {
    const PlanePosition southWest{.xMeters = sector.minXMeters, .zMeters = sector.minZMeters};
    const PlanePosition southEast{.xMeters = sector.maxXMeters, .zMeters = sector.minZMeters};
    const PlanePosition northEast{.xMeters = sector.maxXMeters, .zMeters = sector.maxZMeters};
    const PlanePosition northWest{.xMeters = sector.minXMeters, .zMeters = sector.maxZMeters};
    addBorder(southWest, southEast);
    addBorder(southEast, northEast);
    addBorder(northWest, northEast);
    addBorder(southWest, northWest);
  }

  // Inside each held or guarded sector, its outline in its holder's look.
  for (const SectorView& sector : _newest.sectors)
  {
    const bool guarded = sector.guarded && !sector.holder.IsValid();
    if (!sector.holder.IsValid() && !guarded)
      continue;
    const TerritoryMarks::Pattern pattern = sector.suppressed ? TerritoryMarks::Pattern::Dashed
                                            : sector.cutOff   ? TerritoryMarks::Pattern::Dotted
                                                              : TerritoryMarks::Pattern::Solid;
    const TerritoryMarks::Look look = guarded ? TerritoryMarks::Look::Guarded : TerritoryMarks::Look::Held;
    const PlayerId holder = guarded ? PIRATES : sector.holder;
    const float west = sector.minXMeters + TERRITORY_INSET_METERS;
    const float east = sector.maxXMeters - TERRITORY_INSET_METERS;
    const float south = sector.minZMeters + TERRITORY_INSET_METERS;
    const float north = sector.maxZMeters - TERRITORY_INSET_METERS;
    const std::array<PlanePosition, 4> corners{{{.xMeters = west, .zMeters = south},
                                                {.xMeters = east, .zMeters = south},
                                                {.xMeters = east, .zMeters = north},
                                                {.xMeters = west, .zMeters = north}}};
    for (size_t i = 0; i < corners.size(); ++i)
      marks.lines.push_back(
        {.from = corners[i], .to = corners[(i + 1) % corners.size()], .look = look, .holder = holder, .pattern = pattern});
  }

  // A mark on every node: its holder's, the pirates' while they guard it, the player's where it could claim it now.
  const auto relay = std::ranges::find(_newest.structureTypes, StructureKind::Relay, &StructureTypeView::structure);
  for (const SectorView& sector : _newest.sectors)
  {
    TerritoryMarks::Node& node = marks.nodes.emplace_back(TerritoryMarks::Node{.position = sector.node});
    if (sector.holder.IsValid())
    {
      node.look = TerritoryMarks::Look::Held;
      node.holder = sector.holder;
    }
    else if (sector.guarded)
    {
      node.look = TerritoryMarks::Look::Guarded;
      node.holder = PIRATES;
    }
    else if (relay != _newest.structureTypes.end() && relay->buildable &&
             CanClaim(sector, relay->radiusMeters, _entities, _newest.mapSizeMeters, _newest.sectors, _newest.player, _newest.nodeCap))
    {
      node.look = TerritoryMarks::Look::Claimable;
      node.holder = _newest.player;
    }
  }
  return marks;
}
