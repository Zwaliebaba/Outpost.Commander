#include "pch.h"
#include "FogOfWar.h"

#include <algorithm>
#include <cmath>

void Outpost::FogOfWar::Reset(float _mapSizeMeters)
{
  m_halfSizeMeters = std::max(0.0f, _mapSizeMeters / 2.0f);
  m_cellsPerSide = static_cast<std::uint32_t>(std::ceil(2.0f * m_halfSizeMeters / CELL_METERS));
  const size_t cells = size_t{m_cellsPerSide} * m_cellsPerSide;
  m_explored.assign(cells, false);
  m_shades.assign(cells, NEVER_SEEN_SHADE);
  ++m_revision;
}

void Outpost::FogOfWar::Update(std::span<const EntityView> _entities, PlayerId _player)
{
  if (m_cellsPerSide == 0)
    return;
  std::vector<bool>& inSight = m_inSight;
  inSight.assign(m_explored.size(), false);
  for (const EntityView& entity : _entities)
  {
    if (entity.owner != _player || entity.sightMeters <= 0.0f)
      continue;
    const float sight = entity.sightMeters;
    for (int row = CellOf(entity.position.zMeters - sight); row <= CellOf(entity.position.zMeters + sight); ++row)
    {
      const float z = -m_halfSizeMeters + ((static_cast<float>(row) + 0.5f) * CELL_METERS) - entity.position.zMeters;
      for (int column = CellOf(entity.position.xMeters - sight); column <= CellOf(entity.position.xMeters + sight); ++column)
      {
        const float x = -m_halfSizeMeters + ((static_cast<float>(column) + 0.5f) * CELL_METERS) - entity.position.xMeters;
        if ((x * x) + (z * z) <= sight * sight)
          inSight[(static_cast<size_t>(row) * m_cellsPerSide) + static_cast<size_t>(column)] = true;
      }
    }
  }
  bool changed = false;
  for (size_t cell = 0; cell < m_shades.size(); ++cell)
  {
    if (inSight[cell])
      m_explored[cell] = true;
    const float shade = inSight[cell] ? SEEN_SHADE : m_explored[cell] ? SEEN_BEFORE_SHADE : NEVER_SEEN_SHADE;
    changed = changed || shade != m_shades[cell];
    m_shades[cell] = shade;
  }
  if (changed)
    ++m_revision;
}

float Outpost::FogOfWar::ShadeAt(PlanePosition _point) const noexcept
{
  const float x = (_point.xMeters + m_halfSizeMeters) / CELL_METERS;
  const float z = (_point.zMeters + m_halfSizeMeters) / CELL_METERS;
  const auto side = static_cast<float>(m_cellsPerSide);
  if (m_cellsPerSide == 0 || x < 0.0f || z < 0.0f || x >= side || z >= side)
    return NEVER_SEEN_SHADE;
  return m_shades[(static_cast<size_t>(z) * m_cellsPerSide) + static_cast<size_t>(x)];
}

bool Outpost::FogOfWar::HasSeen(PlanePosition _center, float _radiusMeters) const noexcept
{
  if (m_cellsPerSide == 0)
    return true;
  const auto explored = [this](int _row, int _column)
  { return static_cast<bool>(m_explored[(static_cast<size_t>(_row) * m_cellsPerSide) + static_cast<size_t>(_column)]); };
  if (explored(CellOf(_center.zMeters), CellOf(_center.xMeters)))
    return true;
  for (int row = CellOf(_center.zMeters - _radiusMeters); row <= CellOf(_center.zMeters + _radiusMeters); ++row)
  {
    const float z = -m_halfSizeMeters + ((static_cast<float>(row) + 0.5f) * CELL_METERS) - _center.zMeters;
    for (int column = CellOf(_center.xMeters - _radiusMeters); column <= CellOf(_center.xMeters + _radiusMeters); ++column)
    {
      const float x = -m_halfSizeMeters + ((static_cast<float>(column) + 0.5f) * CELL_METERS) - _center.xMeters;
      if ((x * x) + (z * z) <= _radiusMeters * _radiusMeters && explored(row, column))
        return true;
    }
  }
  return false;
}

int Outpost::FogOfWar::CellOf(float _meters) const noexcept
{
  return std::clamp(static_cast<int>(std::floor((_meters + m_halfSizeMeters) / CELL_METERS)), 0, static_cast<int>(m_cellsPerSide) - 1);
}
