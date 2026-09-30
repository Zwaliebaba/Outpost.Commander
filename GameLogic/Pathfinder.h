#pragma once

namespace Outpost
{
// A circle that blocks movement: an asteroid or a field now, a structure later (design §4, §6).
struct Obstacle
{
  PlanePosition center;
  float radiusMeters = 0.0f;

  friend constexpr bool operator==(const Obstacle&, const Obstacle&) = default;
};

// Shortest paths around circular obstacles for a ship that must keep a clearance, its footprint radius (task 2.4).
// Each obstacle is grown by the clearance and ringed by a polygon whose edges touch the grown circle; a path runs
// straight between the polygons' corners wherever nothing blocks it. The graph of those corners depends only on the
// obstacles and the clearance, so it is built once per clearance and kept.
class Pathfinder
{
public:
  // Replaces the obstacles and the map's edge: a square of _halfSizeMeters around the origin, or none when it is 0.
  void SetObstacles(std::vector<Obstacle> _obstacles, float _halfSizeMeters);

  [[nodiscard]] const std::vector<Obstacle>& Obstacles() const noexcept
  {
    return m_obstacles;
  }

  // The waypoints from _start to _goal, not including _start. The goal is moved clear first, so a ship ordered onto an
  // asteroid stops beside it. When no path exists, the path is the clear goal alone and the ship heads straight for it.
  [[nodiscard]] std::vector<PlanePosition> FindPath(PlanePosition _start, PlanePosition _goal, float _clearanceMeters) const;

  // _position moved out of every obstacle grown by _clearanceMeters, and inside the edge by as much.
  [[nodiscard]] PlanePosition Clear(PlanePosition _position, float _clearanceMeters) const;

  // _position moved inside the edge by _clearanceMeters; unchanged when the map has no edge.
  [[nodiscard]] PlanePosition InsideEdge(PlanePosition _position, float _clearanceMeters) const noexcept;

  // Builds the graph for _clearanceMeters now rather than on the first path that needs it, so that the first order of a
  // match does not pay for it.
  void Prepare(float _clearanceMeters) const
  {
    (void)GraphFor(_clearanceMeters);
  }

  // Whether a ship of _clearanceMeters can travel straight from _a to _b.
  [[nodiscard]] bool IsStraightPathClear(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const;

private:
  struct Graph
  {
    std::vector<PlanePosition> nodes;
    // For each node, the nodes it sees and how far away they are.
    std::vector<std::vector<std::pair<std::uint32_t, float>>> edges;
  };

  [[nodiscard]] const Graph& GraphFor(float _clearanceMeters) const;
  [[nodiscard]] bool IsInsideEdge(PlanePosition _position, float _clearanceMeters) const noexcept;

  std::vector<Obstacle> m_obstacles;
  float m_halfSizeMeters = 0.0f;
  // Built on first use for each clearance; a function of the obstacles alone, so it holds no state of its own. A vector
  // searched in order rather than a map: there are only as many clearances as hulls, and moving a vector cannot throw,
  // where moving MSVC's std::map can.
  mutable std::vector<std::pair<float, Graph>> m_graphs;
};
} // namespace Outpost
