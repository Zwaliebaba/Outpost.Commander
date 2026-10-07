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
// obstacles and the clearance, so it is built once per clearance and kept: extended when obstacles are added, and
// reduced when they are taken away.
class Pathfinder
{
public:
  // The corners for one clearance: those round every obstacle, in the obstacles' order, that the map's edge and no other
  // obstacle cover, and for each corner the corners it sees.
  struct Graph
  {
    std::vector<PlanePosition> nodes;
    // For each node, the nodes it sees, in the order of their indices, and how far away they are.
    std::vector<std::vector<std::pair<std::uint32_t, float>>> edges;
    // Each node's outward direction from its obstacle, and how far from its tangent a line may turn and still touch its
    // polygon there, as a squared sine: within half the polygon's turn at a corner. A line from a corner of an obstacle
    // added later is tested against them.
    std::vector<PlaneVector> normals;
    std::vector<float> touchLimitsSquared;
    // The obstacles the graph is built over, in order, and the version of the pathfinder's obstacles they were. Once the
    // obstacles change, the graph is brought up to date before it is next used: reduced by those taken away and extended
    // over those added (ADR-054).
    std::vector<Obstacle> obstacles;
    std::uint64_t version = 0;
  };

  // Replaces the obstacles and the map's edge: a square of _halfSizeMeters around the origin, or none when it is 0. While
  // the edge is unchanged, the graphs built are kept, and each is brought up to date when next needed: reduced by the
  // obstacles taken away and extended over those added, which is cheaper than a whole build; otherwise they are dropped
  // (ADR-054).
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

  // Brings up to date the graph of one clearance that has had one, the first such that needs it, and says whether there
  // was one: the simulation spreads the work over quiet ticks (ADR-032, ADR-054).
  bool PrepareNext() const;

  // The graph for _clearanceMeters, built first, or brought up to date with the obstacles taken away and added since it
  // was built, when it needs to be. Valid until the obstacles next change. Paths are searched over it, and tests compare a
  // graph brought up to date with one built whole (ADR-054).
  [[nodiscard]] const Graph& GraphFor(float _clearanceMeters) const;

  // Whether a ship of _clearanceMeters can travel straight from _a to _b. A short line tests only the obstacles the grid
  // over them finds near it, which gives the same answer as testing every obstacle, as a longer line does (ADR-054).
  [[nodiscard]] bool IsStraightPathClear(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const;

  // Whether a disc of _radiusMeters at _point overlaps an obstacle: whether its center is nearer than their radii together
  // to any obstacle's. The grid answers where it can, as a test of every obstacle would (ADR-054).
  [[nodiscard]] bool OverlapsObstacle(PlanePosition _point, float _radiusMeters) const;

  // The same answer from a test of every obstacle, which is what IsStraightPathClear makes for a longer line, or where
  // its grid cannot answer exactly, and what tests hold it to.
  [[nodiscard]] bool IsStraightPathClearOfEveryObstacle(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const;

  // Tells _observer when a graph is built, as TickPart::GraphBuild (task 8.1); nullptr for no one. The simulation sets it
  // for one tick at a time.
  void Observe(TickObserver* _observer) noexcept
  {
    m_observer = _observer;
  }

private:
  // A uniform grid over the obstacles (ADR-054): each cell lists the obstacles whose circle's square overlaps it, so that
  // a line test visits only the obstacles near its line. It has no cells when it cannot answer exactly for the
  // obstacles, and every line test then tests every obstacle.
  struct ObstacleGrid
  {
    PlanePosition origin;
    float cellMeters = 0.0f;
    float cellsPerMeter = 0.0f;
    std::int32_t columns = 0;
    std::int32_t rows = 0;
    // Cell (column, row) holds cellObstacles from cellStarts[row * columns + column] up to the next cell's start.
    std::vector<std::uint32_t> cellStarts;
    std::vector<std::uint32_t> cellObstacles;
  };

  // _graph extended over the obstacles added since it was built, which are those after its own: the very graph a build
  // over every obstacle makes, corner for corner and edge for edge, in the same order and to the bit (ADR-054). An empty
  // graph extended is a whole build.
  [[nodiscard]] Graph Extended(const Graph& _graph, float _clearanceMeters) const;
  // _graph without the obstacles _removed marks, one flag for each of its own: the graph a build over the rest makes, to
  // the bit. A corner the removed obstacles covered comes back, and only a line that ran near one of them is tested again
  // (ADR-054).
  [[nodiscard]] Graph Reduced(const Graph& _graph, const std::vector<bool>& _removed, float _clearanceMeters) const;
  // _graph brought up to date with the obstacles: reduced by those taken away since it was built and extended over those
  // added, or built whole when most of its obstacles are gone.
  [[nodiscard]] Graph UpToDate(const Graph& _graph, float _clearanceMeters) const;
  [[nodiscard]] bool IsInsideEdge(PlanePosition _position, float _clearanceMeters) const noexcept;
  // Whether no obstacle grown by _clearanceMeters covers _corner.
  [[nodiscard]] bool IsUncovered(PlanePosition _corner, float _clearanceMeters) const;
  void BuildGrid();
  // Puts in m_near, once each, every obstacle that could block a ship of _clearanceMeters on the segment from _a to _b,
  // or stand on it when the two are one point, and some others near it. False when the grid cannot answer exactly for
  // these numbers: the caller then tests every obstacle.
  [[nodiscard]] bool GatherNear(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const;

  std::vector<Obstacle> m_obstacles;
  // Counts the changes of the obstacles, so that a graph knows whether it is up to date without comparing them.
  std::uint64_t m_version = 1;
  float m_halfSizeMeters = 0.0f;
  ObstacleGrid m_grid;
  // GatherNear's findings, and the pass each obstacle was last found in, so that an obstacle spanning several cells is
  // found once with nothing cleared between passes. Scratch for the server's thread alone, as the graphs are.
  mutable std::vector<std::uint32_t> m_near;
  mutable std::vector<std::uint32_t> m_foundInPass;
  mutable std::uint32_t m_pass = 0;
  // Built on first use for each clearance, and extended when obstacles are added; a function of the obstacles alone, so it
  // holds no state of its own. A vector searched in order rather than a map: there are only as many clearances as hulls,
  // and moving a vector cannot throw, where moving MSVC's std::map can.
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