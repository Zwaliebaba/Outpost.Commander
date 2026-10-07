#include "pch.h"
#include "FogOfWar.h"

#include <algorithm>
#include <cmath>

void Outpost::FogOfWar::Reset(float _mapSizeMeters)
{
  m_halfSizeMeters = std::max(0.0f, _mapSizeMeters / 2.0f);
  m_cellsPerSide = static_cast<std::uint32_t>(std::ceil(2.0f * m_halfSizeMeters / CELL_METERS));
  const size_t cells = size_t{m_cellsPerSide} * m_cellsPerSide;
  m_explored.assign(cells, 0);
  m_shades.assign(cells, NEVER_SEEN_SHADE);
  m_changedRows.assign(m_cellsPerSide, 1);
  m_sightCounts.assign(cells, 0);
  m_sectorCounts.assign(cells, 0);
  m_sights.clear();
  m_litSectors.clear();
  ++m_revision;
}

void Outpost::FogOfWar::Update(std::span<const EntityView> _entities, PlayerId _player, std::span<const SectorView> _sectors)
{
  if (m_cellsPerSide == 0)
    return;
  m_touched.clear();

  // The sectors seen whole: one that left sight, or changed, is counted out, and one that came into sight counted in.
  std::vector<LitSector> lit;
  for (const SectorView& sector : _sectors)
  {
    if (sector.holder == _player && !sector.suppressed)
      lit.push_back({.id = sector.id,
                     .minXMeters = sector.minXMeters,
                     .maxXMeters = sector.maxXMeters,
                     .minZMeters = sector.minZMeters,
                     .maxZMeters = sector.maxZMeters});
  }
  const auto sameSector = [](const LitSector& _a, const LitSector& _b)
  {
    return _a.id == _b.id && _a.minXMeters == _b.minXMeters && _a.maxXMeters == _b.maxXMeters && _a.minZMeters == _b.minZMeters &&
           _a.maxZMeters == _b.maxZMeters;
  };
  for (const LitSector& old : m_litSectors)
  {
    if (std::ranges::none_of(lit, [&](const LitSector& _now) { return sameSector(old, _now); }))
      CountSector(old, -1);
  }
  for (const LitSector& now : lit)
  {
    if (std::ranges::none_of(m_litSectors, [&](const LitSector& _old) { return sameSector(_old, now); }))
      CountSector(now, 1);
  }
  m_litSectors = std::move(lit);

  // The entities that see: one whose place or sight moved is counted out where it was and in where it is.
  std::vector<Sight> sights;
  for (const EntityView& entity : _entities)
  {
    if (entity.owner == _player && entity.sightMeters > 0.0f)
      sights.push_back({.id = entity.id, .center = entity.position, .sightMeters = entity.sightMeters});
  }
  std::ranges::sort(sights, {}, &Sight::id);
  auto old = m_sights.begin();
  for (const Sight& now : sights)
  {
    for (; old != m_sights.end() && old->id < now.id; ++old)
      CountCircle(old->center, old->sightMeters, -1);
    if (old != m_sights.end() && old->id == now.id)
    {
      const bool same = old->center == now.center && old->sightMeters == now.sightMeters;
      if (!same)
        MoveCircle(*old, now);
      ++old;
    }
    else
      CountCircle(now.center, now.sightMeters, 1);
  }
  for (; old != m_sights.end(); ++old)
    CountCircle(old->center, old->sightMeters, -1);
  m_sights = std::move(sights);

  // Only the cells whose counts left or reached zero can change their shade.
  bool changed = false;
  for (const size_t cell : m_touched)
  {
    const bool inSight = m_sightCounts[cell] > 0 || m_sectorCounts[cell] > 0;
    if (inSight)
      m_explored[cell] = 1;
    const float shade = inSight ? SEEN_SHADE : m_explored[cell] != 0 ? SEEN_BEFORE_SHADE : NEVER_SEEN_SHADE;
    if (shade == m_shades[cell])
      continue;
    m_shades[cell] = shade;
    m_changedRows[cell / m_cellsPerSide] = 1;
    changed = true;
  }
  if (changed)
    ++m_revision;
}

std::pair<int, int> Outpost::FogOfWar::CircleRow(PlanePosition _center, float _sightMeters, int _row) const noexcept
{
  const float z = -m_halfSizeMeters + ((static_cast<float>(_row) + 0.5f) * CELL_METERS) - _center.zMeters;
  const float rest = (_sightMeters * _sightMeters) - (z * z);
  if (_row < CellOf(_center.zMeters - _sightMeters) || _row > CellOf(_center.zMeters + _sightMeters) || rest < 0.0f)
    return {1, 0};
  // Whether a column's center is within the circle: the very test of each cell the fog has always made.
  const auto inside = [&](int _column)
  {
    const float x = -m_halfSizeMeters + ((static_cast<float>(_column) + 0.5f) * CELL_METERS) - _center.xMeters;
    return (x * x) + (z * z) <= _sightMeters * _sightMeters;
  };
  // The run's ends, from the circle's half-width on this row, then moved a cell at a time until the test agrees, within
  // the columns the circle's square reaches.
  const int lowest = CellOf(_center.xMeters - _sightMeters);
  const int highest = CellOf(_center.xMeters + _sightMeters);
  const float halfWidth = std::sqrt(rest);
  int first = std::clamp(CellOf(_center.xMeters - halfWidth), lowest, highest);
  int last = std::clamp(CellOf(_center.xMeters + halfWidth), lowest, highest);
  while (first > lowest && inside(first - 1))
    --first;
  while (first <= last && !inside(first))
    ++first;
  while (last < highest && inside(last + 1))
    ++last;
  while (last >= first && !inside(last))
    --last;
  return {first, last};
}

void Outpost::FogOfWar::CountCircle(PlanePosition _center, float _sightMeters, int _delta)
{
  for (int row = CellOf(_center.zMeters - _sightMeters); row <= CellOf(_center.zMeters + _sightMeters); ++row)
  {
    const auto [first, last] = CircleRow(_center, _sightMeters, row);
    for (int column = first; column <= last; ++column)
      Count(m_sightCounts, (static_cast<size_t>(row) * m_cellsPerSide) + static_cast<size_t>(column), _delta);
  }
}

void Outpost::FogOfWar::MoveCircle(const Sight& _from, const Sight& _to)
{
  const int firstRow = std::min(CellOf(_from.center.zMeters - _from.sightMeters), CellOf(_to.center.zMeters - _to.sightMeters));
  const int lastRow = std::max(CellOf(_from.center.zMeters + _from.sightMeters), CellOf(_to.center.zMeters + _to.sightMeters));
  for (int row = firstRow; row <= lastRow; ++row)
  {
    const auto [fromFirst, fromLast] = CircleRow(_from.center, _from.sightMeters, row);
    const auto [toFirst, toLast] = CircleRow(_to.center, _to.sightMeters, row);
    const size_t start = static_cast<size_t>(row) * m_cellsPerSide;
    for (int column = fromFirst; column <= fromLast; ++column)
    {
      if (column < toFirst || column > toLast)
        Count(m_sightCounts, start + static_cast<size_t>(column), -1);
    }
    for (int column = toFirst; column <= toLast; ++column)
    {
      if (column < fromFirst || column > fromLast)
        Count(m_sightCounts, start + static_cast<size_t>(column), 1);
    }
  }
}

void Outpost::FogOfWar::CountSector(const LitSector& _sector, int _delta)
{
  for (int row = CellOf(_sector.minZMeters); row <= CellOf(_sector.maxZMeters); ++row)
  {
    const float z = -m_halfSizeMeters + ((static_cast<float>(row) + 0.5f) * CELL_METERS);
    for (int column = CellOf(_sector.minXMeters); column <= CellOf(_sector.maxXMeters); ++column)
    {
      const float x = -m_halfSizeMeters + ((static_cast<float>(column) + 0.5f) * CELL_METERS);
      // A cell whose center is inside the sector, its borders included, as SectorView::Contains has it.
      if (x >= _sector.minXMeters && x <= _sector.maxXMeters && z >= _sector.minZMeters && z <= _sector.maxZMeters)
        Count(m_sectorCounts, (static_cast<size_t>(row) * m_cellsPerSide) + static_cast<size_t>(column), _delta);
    }
  }
}

void Outpost::FogOfWar::Count(std::vector<std::uint16_t>& _counts, size_t _cell, int _delta)
{
  const std::uint16_t before = _counts[_cell];
  _counts[_cell] = static_cast<std::uint16_t>(before + _delta);
  if (before == 0 || _counts[_cell] == 0)
    m_touched.push_back(_cell);
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
  { return m_explored[(static_cast<size_t>(_row) * m_cellsPerSide) + static_cast<size_t>(_column)] != 0; };
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
