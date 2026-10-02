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
}

void Outpost::FogOfWar::Update(std::span<const EntityView> _entities, PlayerId _player)
{
  if (m_cellsPerSide == 0)
    return;
  std::vector<bool> inSight(m_explored.size(), false);
  const auto last = static_cast<int>(m_cellsPerSide) - 1;
  // The cell an x or z falls in, clamped to the grid.
  const auto cellOf = [this, last](float _meters)
  { return std::clamp(static_cast<int>(std::floor((_meters + m_halfSizeMeters) / CELL_METERS)), 0, last); };
  for (const EntityView& entity : _entities)
  {
    if (entity.owner != _player || entity.sightMeters <= 0.0f)
      continue;
    const float sight = entity.sightMeters;
    for (int row = cellOf(entity.position.zMeters - sight); row <= cellOf(entity.position.zMeters + sight); ++row)
    {
      const float z = -m_halfSizeMeters + ((static_cast<float>(row) + 0.5f) * CELL_METERS) - entity.position.zMeters;
      for (int column = cellOf(entity.position.xMeters - sight); column <= cellOf(entity.position.xMeters + sight); ++column)
      {
        const float x = -m_halfSizeMeters + ((static_cast<float>(column) + 0.5f) * CELL_METERS) - entity.position.xMeters;
        if ((x * x) + (z * z) <= sight * sight)
          inSight[(static_cast<size_t>(row) * m_cellsPerSide) + static_cast<size_t>(column)] = true;
      }
    }
  }
  for (size_t cell = 0; cell < m_shades.size(); ++cell)
  {
    if (inSight[cell])
      m_explored[cell] = true;
    m_shades[cell] = inSight[cell] ? SEEN_SHADE : m_explored[cell] ? SEEN_BEFORE_SHADE : NEVER_SEEN_SHADE;
  }
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
