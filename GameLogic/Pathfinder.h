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

  // Builds the graph again for one clearance that had one before the obstacles last changed, the first such, and says
  // whether there was one to build: the simulation spreads the rebuilding over quiet ticks (ADR-032).
  bool PrepareNext() const;

  // Whether a ship of _clearanceMeters can travel straight from _a to _b.
  [[nodiscard]] bool IsStraightPathClear(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const;

  // Tells _observer when a graph is built, as TickPart::GraphBuild (task 8.1); nullptr for no one. The simulation sets it
  // for one tick at a time.
  void Observe(TickObserver* _observer) noexcept
  {
    m_observer = _observer;
  }

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
  // Every clearance a graph has been built for, in the order first built, which outlives the graphs themselves.
  mutable std::vector<float> m_clearances;
  TickObserver* m_observer = nullptr;
};

// The paths of the ships of one group order, sharing searches rather than making one per ship (ADR-010). The group searches
// once, from its center to its destination, and each ship joins that route at the furthest corner it can see. A ship that
// sees no route searches for itself, and its path becomes a route the ships after it may join, so a cluster of ships cut off
// from the group pays for one search rather than one each.
class GroupRoutes
{
public:
  // Where the group's band of lanes crosses a route at one of its corners (ADR-047): a ship's lane passes the corner at
  // middle + across * its place across the band, in meters. across lies along the corner's mitre, and is stretched so
  // that a lane keeps its distance from the route on both legs.
  struct Band
  {
    PlanePosition middle;
    PlaneVector across;

    friend bool operator==(const Band&, const Band&) = default;
  };

  // A route the group's ships may join: the corners of a path, not including its end, and the clearance it was searched
  // with, which a ship needing more may not join. It has a band at each corner when the group keeps lanes, and none when
  // its ships share the corners themselves.
  struct Route
  {
    std::vector<PlanePosition> corners;
    float clearanceMeters = 0.0f;
    std::vector<Band> bands;

    friend bool operator==(const Route&, const Route&) = default;
  };

  // _destination is moved clear of obstacles for _widestClearanceMeters, the clearance of the group's widest ship. A group
  // keeps a band _bandHalfWidthMeters either side of its middle along its routes, each ship in its own lane; at 0 its
  // ships share the routes' corners. A group whose paths are planned over two ticks (ADR-032) starts the second with the
  // _routes it found in the first.
  GroupRoutes(const Pathfinder& _pathfinder, PlanePosition _destination, float _widestClearanceMeters, float _bandHalfWidthMeters = 0.0f,
              std::vector<Route> _routes = {});

  // Where the group is going, clear of obstacles: every slot is laid out around it.
  [[nodiscard]] PlanePosition Destination() const noexcept
  {
    return m_destination;
  }

  // Searches the group's route from _center, its ships' mean position.
  void SearchFrom(PlanePosition _center);

  // A ship's way: its waypoints, and the last of those that are its lane (ADR-047), when it keeps one.
  struct Way
  {
    std::vector<PlanePosition> waypoints;
    std::optional<PlanePosition> laneEnd;
  };

  // The way from _start to _slot for a ship of _clearanceMeters, not including _start: straight when nothing is in the
  // way, otherwise along a route and, when the route's end does not see the slot, through the destination. Along a route
  // with bands the ship keeps the lane _laneMeters to the left of the band's middle, or as near it as the way is clear.
  [[nodiscard]] Way PathFor(PlanePosition _start, PlanePosition _slot, float _clearanceMeters, float _laneMeters = 0.0f);

  // The routes found so far, the group's own first and then those its ships' searches made.
  [[nodiscard]] std::vector<Route> TakeRoutes() noexcept
  {
    return std::move(m_routes);
  }

private:
  [[nodiscard]] std::optional<Way> Join(const Route& _route, PlanePosition _start, PlanePosition _goal, float _clearanceMeters,
                                        float _laneMeters) const;
  // The ship's lane along _route from its _corner on, from _start to _next: each corner moved _laneMeters across the band
  // from its middle, or towards the corner as far as the way needs (ADR-047). Nothing when the route has no bands, or the
  // lane cannot be made clear.
  [[nodiscard]] std::optional<std::vector<PlanePosition>> Lane(const Route& _route, std::size_t _corner, PlanePosition _start,
                                                               float _laneMeters, PlanePosition _next, float _clearanceMeters) const;
  // The band at each of _corners, a route from _from searched for _clearanceMeters; none when the group keeps no lanes.
  [[nodiscard]] std::vector<Band> BandsAlong(PlanePosition _from, const std::vector<PlanePosition>& _corners, float _clearanceMeters) const;
  // Whether a ship of _clearanceMeters may stand at _position: clear of every obstacle and inside the map's edge.
  [[nodiscard]] bool IsClearAt(PlanePosition _position, float _clearanceMeters) const;
  // The shortest any way from _start along _route to _goal can be, whatever it sees: no line test is made.
  [[nodiscard]] static float ShortestJoinMeters(const Route& _route, PlanePosition _start, PlanePosition _goal) noexcept;

  const Pathfinder& m_pathfinder;
  float m_widestClearanceMeters = 0.0f;
  float m_bandHalfWidthMeters = 0.0f;
  PlanePosition m_destination;
  std::vector<Route> m_routes;
};
} // namespace Outpost