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

// The paths of the ships of one group order, sharing searches rather than making one per ship (ADR-010). The group searches
// once, from its center to its destination, and each ship joins that route at the furthest corner it can see. A ship that
// sees no route searches for itself, and its path becomes a route the ships after it may join, so a cluster of ships cut off
// from the group pays for one search rather than one each.
class GroupRoutes
{
public:
  // _destination is moved clear of obstacles for _widestClearanceMeters, the clearance of the group's widest ship.
  GroupRoutes(const Pathfinder& _pathfinder, PlanePosition _destination, float _widestClearanceMeters);

  // Where the group is going, clear of obstacles: every slot is laid out around it.
  [[nodiscard]] PlanePosition Destination() const noexcept
  {
    return m_destination;
  }

  // Searches the group's route from _center, its ships' mean position.
  void SearchFrom(PlanePosition _center);

  // The waypoints from _start to _slot for a ship of _clearanceMeters, not including _start: straight when nothing is in the
  // way, otherwise along a route and, when the route's end does not see the slot, through the destination.
  [[nodiscard]] std::vector<PlanePosition> PathFor(PlanePosition _start, PlanePosition _slot, float _clearanceMeters);

private:
  struct Route
  {
    // The corners of a path, not including its end.
    std::vector<PlanePosition> corners;
    // The clearance it was searched with; a ship needing more may not join it.
    float clearanceMeters = 0.0f;
  };

  [[nodiscard]] std::optional<std::vector<PlanePosition>> Join(const Route& _route, PlanePosition _start, PlanePosition _goal,
                                                               float _clearanceMeters) const;

  const Pathfinder& m_pathfinder;
  float m_widestClearanceMeters = 0.0f;
  PlanePosition m_destination;
  std::vector<Route> m_routes;
};
} // namespace Outpost
