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

  // Starts over on a map of this side, centered on the origin, none of it seen; empty while the side is zero.
  void Reset(float _mapSizeMeters);

  // What _player's entities among _entities see now: those cells become seen, now and for the rest of the match, and
  // Shades says so. A cell is in sight when its center is within an entity's sight.
  void Update(std::span<const EntityView> _entities, PlayerId _player);

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

  // The shade of the cell that holds _point, or the never-seen shade off the map.
  [[nodiscard]] float ShadeAt(PlanePosition _point) const noexcept;

  // Whether the player has ever seen any of the circle of _radiusMeters round _center: a cell whose center is in it, or
  // the cell that holds its center. Without fog of war it has seen all of the map (ADR-046).
  [[nodiscard]] bool HasSeen(PlanePosition _center, float _radiusMeters) const noexcept;

private:
  // The row or column an x or z falls in, clamped to the grid.
  [[nodiscard]] int CellOf(float _meters) const noexcept;

  float m_halfSizeMeters = 0.0f;
  std::uint32_t m_cellsPerSide = 0;
  std::vector<bool> m_explored;
  std::vector<float> m_shades;
};
} // namespace Outpost
