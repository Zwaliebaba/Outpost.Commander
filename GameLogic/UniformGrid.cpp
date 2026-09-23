#include "pch.h"

#include "UniformGrid.h"

#include <algorithm>

namespace Outpost
{

std::int32_t UniformGrid::AxisCell(std::int64_t _coordinate) noexcept
{
  const std::int64_t shifted = _coordinate + PLAY_AREA_HALF_EXTENT;
  if (shifted < 0)
  {
    return 0;
  }
  const std::int64_t cell = shifted / CELL_SIZE;
  return (cell >= CELLS_PER_SIDE) ? (CELLS_PER_SIDE - 1) : static_cast<std::int32_t>(cell);
}

std::size_t UniformGrid::CellOf(const Neuron::Vec2& _position) noexcept
{
  return (static_cast<std::size_t>(AxisCell(_position.y)) * CELLS_PER_SIDE) + static_cast<std::size_t>(AxisCell(_position.x));
}

std::int64_t UniformGrid::DistanceSquared(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  const std::int64_t dx = static_cast<std::int64_t>(_a.x) - _b.x;
  const std::int64_t dy = static_cast<std::int64_t>(_a.y) - _b.y;
  return (dx * dx) + (dy * dy);
}

void UniformGrid::Rebuild(const World& _world)
{
  // A COUNTING SORT IN TWO PASSES, BOTH IN INDEX ORDER. The first counts each cell, the prefix sums turn
  // the counts into start offsets, and the second drops each entity at its cell's next free place -- so
  // within a cell the entries are in index order, which is the world's order and nothing else's.
  m_fill.fill(0);
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (_world.IsSlotAlive(slot))
    {
      ++m_fill[CellOf(_world.EntityInSlot(slot).position)];
    }
  }

  m_cellStart[0] = 0;
  for (std::size_t cell = 0; cell < CELL_COUNT; ++cell)
  {
    m_cellStart[cell + 1] = m_cellStart[cell] + m_fill[cell];
  }

  m_entries.resize(m_cellStart[CELL_COUNT]);
  m_fill.fill(0);
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    const std::size_t cell = CellOf(entity.position);
    m_entries[m_cellStart[cell] + m_fill[cell]] = entity.id;
    ++m_fill[cell];
  }
}

void UniformGrid::Query(const World& _world, const Neuron::Vec2& _center, Neuron::Fixed _radius, std::vector<EntityId>& _out) const
{
  _out.clear();
  if (_radius < 0)
  {
    return;
  }

  // THE CELLS UNDER THE CIRCLE'S BOX, clamped to the grid. Clamping is what finds an entity outside the
  // square: `Rebuild` put it in the edge cell, and a box that reaches past the edge includes that cell.
  const std::int32_t firstColumn = AxisCell(static_cast<std::int64_t>(_center.x) - _radius);
  const std::int32_t lastColumn = AxisCell(static_cast<std::int64_t>(_center.x) + _radius);
  const std::int32_t firstRow = AxisCell(static_cast<std::int64_t>(_center.y) - _radius);
  const std::int32_t lastRow = AxisCell(static_cast<std::int64_t>(_center.y) + _radius);

  const std::int64_t radiusSquared = static_cast<std::int64_t>(_radius) * _radius;
  for (std::int32_t row = firstRow; row <= lastRow; ++row)
  {
    for (std::int32_t column = firstColumn; column <= lastColumn; ++column)
    {
      const std::size_t cell = (static_cast<std::size_t>(row) * CELLS_PER_SIDE) + static_cast<std::size_t>(column);
      for (std::uint32_t entry = m_cellStart[cell]; entry < m_cellStart[cell + 1]; ++entry)
      {
        const Entity* entity = _world.Find(m_entries[entry]);
        if ((entity != nullptr) && (DistanceSquared(entity->position, _center) <= radiusSquared))
        {
          _out.push_back(entity->id);
        }
      }
    }
  }

  // **THE LINE THIS CLASS EXISTS AROUND.** What came before is cell order, which is layout. What leaves is
  // identity order, which is the match's. `EntityId`'s ordering is total -- index, then generation -- and
  // identities are unique, so no two compare equal and `std::sort`'s instability has nothing to act on.
  std::sort(_out.begin(), _out.end());
}

} // namespace Outpost
