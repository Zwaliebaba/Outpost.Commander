#pragma once

namespace Outpost
{
// What the player has seen of the map, as the client draws it under fog of war (ADR-024): a square grid of cells over the
// map, each clear where the player's own ships and structures see it now, dimmed where they have seen it before, and
// dark where they never have. Which enemies the player sees is the server's to say; this only shades the ground.
class FogOfWar
{
public:
  static constexpr float CELL_METERS = 20.0f;
  // How much of the scene each cell darkens: 0 is clear.
  static constexpr float SEEN_SHADE = 0.0f;
  static constexpr float SEEN_BEFORE_SHADE = 0.55f;
  static constexpr float NEVER_SEEN_SHADE = 0.9f;
  // On the ground, what was seen before is darker than its shade, so that the edge of sight is where the grid and the stars
  // go out; never seen keeps its shade. The minimap keeps SEEN_BEFORE_SHADE, where telling what was explored from what never
  // was matters for scouting. The ground's mask maps one to the other (interface plan 2, task UI1.3).
  static constexpr float GROUND_SEEN_BEFORE_SHADE = 0.8f;

  // Starts over on a map of this side, centered on the origin, none of it seen; empty while the side is zero.
  void Reset(float _mapSizeMeters);

  // What _player's entities among _entities see now, and the sectors it holds that are not suppressed, which it sees whole
  // (ADR-056): those cells become seen, now and for the rest of the match, and Shades says so. A cell is in sight when its
  // center is within an entity's sight or inside such a sector. Only what changed since the last update is worked out
  // again: an entity whose place or sight moved, and a sector that came into or went out of sight (ADR-052).
  void Update(std::span<const EntityView> _entities, PlayerId _player, std::span<const SectorView> _sectors = {});

  [[nodiscard]] std::uint32_t CellsPerSide() const noexcept
  {
    return m_cellsPerSide;
  }

  // The map's corner at its lowest x and z, where the grid starts.
  [[nodiscard]] PlanePosition Origin() const noexcept
  {
    return {.xMeters = -m_halfSizeMeters, .zMeters = -m_halfSizeMeters};
  }

  // Each cell's shade, row by row along x from Origin, rows in order of z: CellsPerSide squared of them.
  [[nodiscard]] std::span<const float> Shades() const noexcept
  {
    return m_shades;
  }

  // A count that moves on whenever Reset or Update changes the shades, so that whoever copies them elsewhere, such as into
  // a texture, knows when to copy them again (ADR-052).
  [[nodiscard]] std::uint64_t Revision() const noexcept
  {
    return m_revision;
  }

  // For each row of cells, whether any of its shades changed since ClearChangedRows, so that whoever copies the shades
  // elsewhere can copy only those rows (ADR-052). Reset marks every row.
  [[nodiscard]] std::span<const std::uint8_t> ChangedRows() const noexcept
  {
    return m_changedRows;
  }

  void ClearChangedRows() noexcept
  {
    std::ranges::fill(m_changedRows, std::uint8_t{0});
  }

  // The shade of the cell that holds _point, or the never-seen shade off the map.
  [[nodiscard]] float ShadeAt(PlanePosition _point) const noexcept;

  // Whether the player has ever seen any of the circle of _radiusMeters round _center: a cell whose center is in it, or
  // the cell that holds its center. Without fog of war it has seen all of the map (ADR-046).
  [[nodiscard]] bool HasSeen(PlanePosition _center, float _radiusMeters) const noexcept;

private:
  // An entity's sight as the last update counted it.
  struct Sight
  {
    EntityId id;
    PlanePosition center;
    float sightMeters = 0.0f;
  };

  // A sector seen whole as the last update counted it.
  struct LitSector
  {
    std::int32_t id = 0;
    float minXMeters = 0.0f;
    float maxXMeters = 0.0f;
    float minZMeters = 0.0f;
    float maxZMeters = 0.0f;
  };

  // The row or column an x or z falls in, clamped to the grid.
  [[nodiscard]] int CellOf(float _meters) const noexcept;
  // The columns of _row whose centers are within the circle, first to last; none when the first is past the last. A row of
  // a circle is one run of cells, so a circle is counted, or moved, row by row (ADR-052).
  [[nodiscard]] std::pair<int, int> CircleRow(PlanePosition _center, float _sightMeters, int _row) const noexcept;
  // Adds _delta to the count of every cell whose center is within the circle or the sector, and notes each cell whose count
  // left or reached zero.
  void CountCircle(PlanePosition _center, float _sightMeters, int _delta);
  // Counts a circle out where it was and in where it is, touching only the cells of each row that are in one and not the
  // other.
  void MoveCircle(const Sight& _from, const Sight& _to);
  void CountSector(const LitSector& _sector, int _delta);
  void Count(std::vector<std::uint16_t>& _counts, size_t _cell, int _delta);

  float m_halfSizeMeters = 0.0f;
  std::uint32_t m_cellsPerSide = 0;
  std::vector<std::uint8_t> m_explored;
  std::vector<float> m_shades;
  std::vector<std::uint8_t> m_changedRows;
  // For each cell, how many of the player's entities see it, and how many of the sectors it sees whole hold it.
  std::vector<std::uint16_t> m_sightCounts;
  std::vector<std::uint16_t> m_sectorCounts;
  // What the counts hold, by identifier, and the cells whose counts left or reached zero in this update.
  std::vector<Sight> m_sights;
  std::vector<LitSector> m_litSectors;
  std::vector<size_t> m_touched;
  std::uint64_t m_revision = 0;
};
} // namespace Outpost
