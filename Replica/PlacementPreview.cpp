#include "pch.h"

#include "PlacementPreview.h"

#include "Plan.h"

namespace Outpost
{

Footprint PreviewFootprint(const ContentTree& _content, std::uint32_t _structureRow, std::uint32_t _cellX, std::uint32_t _cellY) noexcept
{
  if (_structureRow >= _content.structures.structures.size())
  {
    // One cell, as Sim/Placement.h's FootprintOf answers for a structure whose row is unknown: it
    // is where the thing certainly is and never claims ground it may not hold.
    return {_cellX, _cellY, 1, 1};
  }
  return FootprintAt(_content.structures.structures[_structureRow], _cellX, _cellY);
}

PlacementFault PreviewPlacement(std::uint32_t _structureRow, std::uint32_t _cellX, std::uint32_t _cellY, const PreviewQuery& _query)
{
  if (_query.structures == nullptr || _query.landscape == nullptr || _query.content == nullptr || !_query.landscape->Created())
  {
    return PlacementFault::OffLandscape; // No tables, or the join's definition has not arrived.
  }
  if (_structureRow >= _query.content->structures.structures.size())
  {
    return PlacementFault::OffLandscape;
  }
  const StructureDesc& row = _query.content->structures.structures[_structureRow];
  const Footprint footprint = FootprintAt(row, _cellX, _cellY);

  // THE GROUND'S OWN THREE RULES, from Sim/Placement.h's function and not from a copy of it, so
  // the ghost and the order cannot disagree about a cliff edge. It is asked ONCE and its answer
  // held, because the order the faults are reported in puts Occupied between OffLandscape and the
  // other two.
  const PlacementFault ground = CheckFootprintGround(*_query.landscape, footprint);
  if (ground == PlacementFault::OffLandscape)
  {
    return ground;
  }

  // WHAT STANDS THERE, as this commander knows it. A GHOST COUNTS: a building he has seen and
  // walked away from is still there as far as he knows, and one he could build through would put
  // his factory inside somebody's base the moment he scouted it again. A PLAN DOES NOT, which is
  // GameDesign.md §5's rule and the simulation's: two commanders may plan the same ground and the
  // first to begin it gets it.
  for (const auto& [id, structure] : *_query.structures)
  {
    if (!Occupies(structure.state.phase))
    {
      continue;
    }
    if (Overlaps(footprint, PreviewFootprint(*_query.content, structure.state.design, structure.state.cellX, structure.state.cellY)))
    {
      return PlacementFault::Occupied;
    }
  }
  // FEATURES ARE NOT HERE AND CANNOT BE. Nothing sends their footprints, so a rock the commander
  // can see is one the host will refuse him and the client cannot - the same kind of difference as
  // the building he has never scouted, and the same answer: the refusal comes back and
  // Design/Interface.md §6's warning line says so.

  if (ground != PlacementFault::Accepted)
  {
    return ground; // Water, or too steep.
  }

  // "A structure may be placed anywhere the commander has explored" (GameDesign.md §5). Explored,
  // not visible: a base built where a scout once walked is the whole point of the rule.
  const std::span<const FogState> fog = _query.fog;
  const std::uint32_t cellsPerSide = _query.fogCellsPerSide;
  if (!fog.empty() && cellsPerSide > 0)
  {
    for (std::uint32_t y = footprint.cellY; y < footprint.cellY + footprint.cellsY; ++y)
    {
      for (std::uint32_t x = footprint.cellX; x < footprint.cellX + footprint.cellsX; ++x)
      {
        const std::size_t at = static_cast<std::size_t>(y) * cellsPerSide + x;
        if (x >= cellsPerSide || y >= cellsPerSide || at >= fog.size() || fog[at] == FogState::Unexplored)
        {
          return PlacementFault::Unexplored;
        }
      }
    }
  }

  // An extractor stands on a deposit and nowhere else (GameDesign.md §4). Its footprint is one
  // cell, so the deposit is under its origin. No deposit field is an absence and not permission
  // withheld, which is how CheckPlacement reads a null one too.
  if (row.role == StructureRole::Extractor && _query.deposits != nullptr && !_query.deposits->Has(footprint.cellX, footprint.cellY))
  {
    return PlacementFault::NotOnDeposit;
  }
  return PlacementFault::Accepted;
}

} // namespace Outpost
