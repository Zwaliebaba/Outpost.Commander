#include "pch.h"
#include "Pathfinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <queue>

namespace
{
// Corners of the polygon around each grown obstacle. More corners hug the circle closer but cost the graph more: sixteen
// keep a path within 2% of a large circle's radius, and eight within 5 m of a circle no wider than SMALL_RADIUS_METERS,
// such as a structure's (ADR-010).
constexpr int POLYGON_CORNERS = 16;
constexpr int SMALL_POLYGON_CORNERS = 8;
constexpr float SMALL_RADIUS_METERS = 60.0f;
// Kept between a ship's footprint and an obstacle on top of the ship's radius, so that a path never grazes one.
constexpr float MARGIN_METERS = 0.5f;
// A segment may pass this close inside a grown circle and still count as clear: the polygon's own edges touch it.
constexpr float TOLERANCE_METERS = 0.01f;
// A line within this much of touching a corner's polygon still counts as touching it, so that rounding never drops an edge
// a shortest path needs.
constexpr float TANGENT_TOLERANCE = 1e-3f;
// Clear() pushes a point out of one obstacle at a time; a point wedged between two needs a few rounds.
constexpr int CLEARING_ROUNDS = 4;
// How many of the corners the start sees a search sorts at a time (ADR-032): about what one search takes off its queue.
constexpr std::size_t START_BATCH = 64;
// A ship of a group takes a shared route only when it is at most this many times the straight line to its slot. A
// longer one is a detour, and since the group keeps the pace of its slowest ship, one ship's detour slows them all.
constexpr float DETOUR_LIMIT = 1.5f;
// The bound on a way along a route adds its lengths in another order than the way's own length does, so it is let past
// the limit by this share before a route is passed over untried: a route it passes over is then always one whose way
// would have been longer than the limit.
constexpr float ROUNDING_ALLOWANCE = 1e-4f;

float PathLength(Outpost::PlanePosition _from, const std::vector<Outpost::PlanePosition>& _path) noexcept
{
  float meters = 0.0f;
  Outpost::PlanePosition previous = _from;
  for (const Outpost::PlanePosition waypoint : _path)
  {
    meters += Outpost::Distance(previous, waypoint);
    previous = waypoint;
  }
  return meters;
}
} // namespace

void Outpost::Pathfinder::SetObstacles(std::vector<Obstacle> _obstacles, float _halfSizeMeters)
{
  m_obstacles = std::move(_obstacles);
  m_halfSizeMeters = _halfSizeMeters;
  m_graphs.clear();
}

bool Outpost::Pathfinder::IsInsideEdge(PlanePosition _position, float _clearanceMeters) const noexcept
{
  if (m_halfSizeMeters <= 0.0f)
    return true;
  const float limit = m_halfSizeMeters - _clearanceMeters;
  return std::abs(_position.xMeters) <= limit && std::abs(_position.zMeters) <= limit;
}

bool Outpost::Pathfinder::IsStraightPathClear(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const
{
  const float minX = std::min(_a.xMeters, _b.xMeters);
  const float maxX = std::max(_a.xMeters, _b.xMeters);
  const float minZ = std::min(_a.zMeters, _b.zMeters);
  const float maxZ = std::max(_a.zMeters, _b.zMeters);
  return std::ranges::all_of(m_obstacles,
                             [&](const Obstacle& _obstacle)
                             {
                               const float grown = _obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS - TOLERANCE_METERS;
                               // Most obstacles are nowhere near the segment's box, and the box is cheaper to test.
                               const PlanePosition center = _obstacle.center;
                               if (center.xMeters + grown < minX || center.xMeters - grown > maxX || center.zMeters + grown < minZ ||
                                   center.zMeters - grown > maxZ)
                                 return true;
                               return DistanceToSegment(center, _a, _b) >= grown;
                             });
}

Outpost::PlanePosition Outpost::Pathfinder::InsideEdge(PlanePosition _position, float _clearanceMeters) const noexcept
{
  if (m_halfSizeMeters <= 0.0f)
    return _position;
  const float limit = m_halfSizeMeters - _clearanceMeters;
  return {std::clamp(_position.xMeters, -limit, limit), std::clamp(_position.zMeters, -limit, limit)};
}

Outpost::PlanePosition Outpost::Pathfinder::Clear(PlanePosition _position, float _clearanceMeters) const
{
  PlanePosition position = _position;
  for (int round = 0; round < CLEARING_ROUNDS; ++round)
  {
    bool moved = false;
    for (const Obstacle& obstacle : m_obstacles)
    {
      const float grown = obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS;
      if (Distance(position, obstacle.center) < grown)
      {
        const PlaneVector away = Normalized(position - obstacle.center, {1.0f, 0.0f});
        position = obstacle.center + away * grown;
        moved = true;
      }
    }
    position = InsideEdge(position, _clearanceMeters);
    if (!moved)
      break;
  }
  return position;
}

const Outpost::Pathfinder::Graph& Outpost::Pathfinder::GraphFor(float _clearanceMeters) const
{
  if (const auto found = std::ranges::find(m_graphs, _clearanceMeters, &std::pair<float, Graph>::first); found != m_graphs.end())
    return found->second;

  const ObservedPart building(m_observer, TickPart::GraphBuild);
  if (std::ranges::find(m_clearances, _clearanceMeters) == m_clearances.end())
    m_clearances.push_back(_clearanceMeters);
  Graph graph;
  // Each corner's outward direction, and how far from its tangent a line may turn and still touch its polygon there, as
  // a squared sine: within half the polygon's turn at a corner.
  std::vector<PlaneVector> normals;
  std::vector<float> touchLimitsSquared;
  for (const Obstacle& obstacle : m_obstacles)
  {
    const float grown = obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS;
    const int corners = grown <= SMALL_RADIUS_METERS ? SMALL_POLYGON_CORNERS : POLYGON_CORNERS;
    // The polygon's corners sit on a circle a little wider than the grown obstacle, so that its edges touch the obstacle
    // rather than cut it.
    const float cornerScale = 1.0f / std::cos(std::numbers::pi_v<float> / static_cast<float>(corners));
    const float touchLimit = std::sin(std::numbers::pi_v<float> / static_cast<float>(corners)) + TANGENT_TOLERANCE;
    for (int corner = 0; corner < corners; ++corner)
    {
      const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(corner) / static_cast<float>(corners);
      const PlaneVector normal{std::cos(angle), std::sin(angle)};
      const PlanePosition node = obstacle.center + normal * (grown * cornerScale);
      const bool free =
        IsInsideEdge(node, _clearanceMeters) &&
        std::ranges::none_of(
          m_obstacles, [&](const Obstacle& _other)
          { return Distance(node, _other.center) < _other.radiusMeters + _clearanceMeters + MARGIN_METERS - TOLERANCE_METERS; });
      if (free)
      {
        graph.nodes.push_back(node);
        normals.push_back(normal);
        touchLimitsSquared.push_back(touchLimit * touchLimit);
      }
    }
  }

  // A shortest path round convex obstacles bends only at corners, and leaves each corner along a line that touches its
  // polygon there: one that cuts into the polygon could be shortened. So only those edges are kept, and whether a line
  // touches the polygon at a corner, which a few multiplications tell, is asked before whether anything blocks it,
  // which costs a pass over every obstacle (ADR-010). A line touches a regular polygon at a corner when it is within
  // half the polygon's turn at a corner of the tangent there. The test is squared, against the squared length of the
  // line, so that it needs no square root.
  graph.edges.resize(graph.nodes.size());
  for (std::uint32_t a = 0; a < graph.nodes.size(); ++a)
  {
    for (std::uint32_t b = a + 1; b < graph.nodes.size(); ++b)
    {
      const PlaneVector between = graph.nodes[b] - graph.nodes[a];
      const float lengthSquared = Dot(between, between);
      const float alongA = Dot(between, normals[a]);
      if (alongA * alongA > touchLimitsSquared[a] * lengthSquared)
        continue;
      const float alongB = Dot(between, normals[b]);
      if (alongB * alongB > touchLimitsSquared[b] * lengthSquared)
        continue;
      if (!IsStraightPathClear(graph.nodes[a], graph.nodes[b], _clearanceMeters))
        continue;
      const float length = Distance(graph.nodes[a], graph.nodes[b]);
      graph.edges[a].emplace_back(b, length);
      graph.edges[b].emplace_back(a, length);
    }
  }
  return m_graphs.emplace_back(_clearanceMeters, std::move(graph)).second;
}

std::vector<Outpost::PlanePosition> Outpost::Pathfinder::FindPath(PlanePosition _start, PlanePosition _goal, float _clearanceMeters) const
{
  const PlanePosition goal = Clear(_goal, _clearanceMeters);
  // A ship pushed into an obstacle's margin leaves it first, by the shortest way out.
  const PlanePosition start = Clear(_start, _clearanceMeters);
  std::vector<PlanePosition> path;
  if (Distance(start, _start) > 0.0f)
    path.push_back(start);

  if (IsStraightPathClear(start, goal, _clearanceMeters))
  {
    path.push_back(goal);
    return path;
  }

  // A* over the graph's corners plus the start and the goal, which join it wherever they see a corner. The straight-line
  // distance to the goal never overestimates, so the first time the goal comes off the queue its path is the shortest.
  // Whether a corner sees the goal is asked only of the corners the search reaches. Whether the start sees a corner is
  // asked lazily too: every corner is first given the straight line from the start, which no way round can beat, and the
  // line is checked only when the corner comes off the queue. Most corners never do, and asking all of them up front was
  // most of a search's cost. Ties on the queue go to the lower node, so the path does not depend on the queue's
  // implementation.
  const Graph& graph = GraphFor(_clearanceMeters);
  const auto cornerCount = static_cast<std::uint32_t>(graph.nodes.size());
  const std::uint32_t startNode = cornerCount;
  const std::uint32_t goalNode = cornerCount + 1;
  const std::uint32_t nodeCount = cornerCount + 2;
  auto position = [&](std::uint32_t _node) { return _node == startNode ? start : _node == goalNode ? goal : graph.nodes[_node]; };

  constexpr float UNREACHED = std::numeric_limits<float>::infinity();
  constexpr std::uint32_t NONE = std::numeric_limits<std::uint32_t>::max();
  std::vector<float> distance(nodeCount, UNREACHED);
  std::vector<std::uint32_t> previous(nodeCount, NONE);
  std::vector<bool> done(nodeCount, false);
  // Corners whose distance is the straight line from the start, not yet checked for an obstacle in the way.
  std::vector<bool> unchecked(nodeCount, false);
  using Entry = std::pair<float, std::uint32_t>;
  // The start is done first: every corner gets the straight line from it, unchecked. A search takes only a few dozen of
  // those entries off the queue, so rather than heaping them all they wait in a list, of which the smallest are sorted a
  // batch at a time; the queue holds only what relaxing pushes. The next entry is the smaller of the two fronts, which
  // takes entries in the very order one queue would (ADR-032).
  distance[startNode] = 0.0f;
  done[startNode] = true;
  std::vector<Entry> fromStart;
  fromStart.reserve(cornerCount);
  for (std::uint32_t corner = 0; corner < cornerCount; ++corner)
  {
    distance[corner] = Distance(start, graph.nodes[corner]);
    previous[corner] = startNode;
    unchecked[corner] = true;
    fromStart.emplace_back(distance[corner] + Distance(graph.nodes[corner], goal), corner);
  }
  // fromStart[next, sorted) is the sorted batch; what follows it is not sorted yet, and every entry there is larger.
  std::size_t next = 0;
  std::size_t sorted = 0;
  const auto sortBatch = [&]
  {
    const std::size_t end = std::min(fromStart.size(), sorted + START_BATCH);
    std::ranges::nth_element(fromStart.begin() + static_cast<std::ptrdiff_t>(sorted),
                             fromStart.begin() + static_cast<std::ptrdiff_t>(end - 1), fromStart.end());
    std::sort(fromStart.begin() + static_cast<std::ptrdiff_t>(sorted), fromStart.begin() + static_cast<std::ptrdiff_t>(end));
    sorted = end;
  };
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

  auto relax = [&](std::uint32_t _from, std::uint32_t _to, float _length)
  {
    if (distance[_from] + _length < distance[_to])
    {
      distance[_to] = distance[_from] + _length;
      previous[_to] = _from;
      open.emplace(distance[_to] + Distance(position(_to), goal), _to);
    }
  };

  while (true)
  {
    if (next == sorted && sorted < fromStart.size())
      sortBatch();
    const bool fromList = next < sorted && (open.empty() || fromStart[next] < open.top());
    if (!fromList && open.empty())
      break;
    std::uint32_t current = 0;
    if (fromList)
      current = fromStart[next++].second;
    else
    {
      current = open.top().second;
      open.pop();
    }
    if (done[current] || distance[current] == UNREACHED)
      continue;
    if (unchecked[current] && previous[current] == startNode)
    {
      unchecked[current] = false;
      if (!IsStraightPathClear(start, graph.nodes[current], _clearanceMeters))
      {
        // The start does not see it after all. Its straight-line distance may have turned away a longer way in from a
        // corner already done, so those ways are offered again.
        distance[current] = UNREACHED;
        previous[current] = NONE;
        for (const auto& [neighbor, length] : graph.edges[current])
        {
          if (done[neighbor])
            relax(neighbor, current, length);
        }
        continue;
      }
    }
    done[current] = true;
    if (current == goalNode)
      break;

    for (const auto& [neighbor, length] : graph.edges[current])
      relax(current, neighbor, length);
    if (IsStraightPathClear(graph.nodes[current], goal, _clearanceMeters))
      relax(current, goalNode, Distance(graph.nodes[current], goal));
  }

  if (previous[goalNode] == NONE)
  {
    // No way round: head straight for the goal and let the obstacles push the ship aside.
    path.push_back(goal);
    return path;
  }

  std::vector<PlanePosition> reversed;
  for (std::uint32_t node = goalNode; node != startNode; node = previous[node])
    reversed.push_back(position(node));
  path.insert(path.end(), reversed.rbegin(), reversed.rend());
  return path;
}

bool Outpost::Pathfinder::PrepareNext() const
{
  for (const float clearance : m_clearances)
  {
    if (std::ranges::find(m_graphs, clearance, &std::pair<float, Graph>::first) == m_graphs.end())
    {
      (void)GraphFor(clearance);
      return true;
    }
  }
  return false;
}

Outpost::GroupRoutes::GroupRoutes(const Pathfinder& _pathfinder, PlanePosition _destination, float _widestClearanceMeters,
                                  std::vector<Route> _routes)
  : m_pathfinder(_pathfinder),
    m_widestClearanceMeters(_widestClearanceMeters),
    m_destination(_pathfinder.Clear(_destination, _widestClearanceMeters)),
    m_routes(std::move(_routes))
{
}

void Outpost::GroupRoutes::SearchFrom(PlanePosition _center)
{
  // The path's last waypoint is the destination itself; each ship ends at its own slot instead.
  std::vector<PlanePosition> corners = m_pathfinder.FindPath(_center, m_destination, m_widestClearanceMeters);
  corners.pop_back();
  if (!corners.empty())
    m_routes.push_back({std::move(corners), m_widestClearanceMeters});
}

std::vector<Outpost::PlanePosition> Outpost::GroupRoutes::PathFor(PlanePosition _start, PlanePosition _slot, float _clearanceMeters)
{
  const PlanePosition goal = m_pathfinder.Clear(_slot, _clearanceMeters);
  // A ship in an obstacle's margin has to leave it first, which the full search knows how to do.
  const bool inMargin = Distance(m_pathfinder.Clear(_start, _clearanceMeters), _start) > 0.0f;
  if (!inMargin)
  {
    if (m_pathfinder.IsStraightPathClear(_start, goal, _clearanceMeters))
      return {goal};
    // The first route that is not a detour a search would save, and then the way through the destination.
    const float limitMeters = DETOUR_LIMIT * Distance(_start, goal);
    for (const Route& route : m_routes)
    {
      if (route.clearanceMeters < _clearanceMeters)
        continue;
      // No way along a route is shorter than the straight line to one of its corners, the route from there and the
      // straight line from its end to the slot. A route that cannot come within the limit even so is not tried: its line
      // tests are most of what a ship's planning costs, and their answer could only be a detour (ADR-032).
      if (ShortestJoinMeters(route, _start, goal) > limitMeters * (1.0f + ROUNDING_ALLOWANCE))
        continue;
      if (std::optional<std::vector<PlanePosition>> path = Join(route, _start, goal, _clearanceMeters);
          path.has_value() && PathLength(_start, *path) <= limitMeters)
        return std::move(*path);
    }
    if (m_pathfinder.IsStraightPathClear(_start, m_destination, _clearanceMeters) &&
        m_pathfinder.IsStraightPathClear(m_destination, goal, _clearanceMeters) &&
        Distance(_start, m_destination) + Distance(m_destination, goal) <= limitMeters)
      return {m_destination, goal};
  }

  std::vector<PlanePosition> path = m_pathfinder.FindPath(_start, goal, _clearanceMeters);
  if (path.size() > 1)
    m_routes.push_back({{path.begin(), path.end() - 1}, _clearanceMeters});
  return path;
}

float Outpost::GroupRoutes::ShortestJoinMeters(const Route& _route, PlanePosition _start, PlanePosition _goal) noexcept
{
  float shortest = std::numeric_limits<float>::infinity();
  float onward = 0.0f;
  for (std::size_t corner = _route.corners.size(); corner-- > 0;)
  {
    if (corner + 1 < _route.corners.size())
      onward += Distance(_route.corners[corner], _route.corners[corner + 1]);
    shortest = std::min(shortest, Distance(_start, _route.corners[corner]) + onward);
  }
  return shortest + Distance(_route.corners.back(), _goal);
}

std::optional<std::vector<Outpost::PlanePosition>> Outpost::GroupRoutes::Join(const Route& _route, PlanePosition _start,
                                                                              PlanePosition _goal, float _clearanceMeters) const
{
  // Every leg of the route keeps at least this ship's clearance. Its end must see the goal, or see the destination when the
  // destination sees the goal.
  std::vector<PlanePosition> tail;
  if (!m_pathfinder.IsStraightPathClear(_route.corners.back(), _goal, _clearanceMeters))
  {
    if (!m_pathfinder.IsStraightPathClear(_route.corners.back(), m_destination, _clearanceMeters) ||
        !m_pathfinder.IsStraightPathClear(m_destination, _goal, _clearanceMeters))
      return std::nullopt;
    tail.push_back(m_destination);
  }
  tail.push_back(_goal);

  for (size_t corner = _route.corners.size(); corner-- > 0;)
  {
    if (!m_pathfinder.IsStraightPathClear(_start, _route.corners[corner], _clearanceMeters))
      continue;
    std::vector<PlanePosition> path(_route.corners.begin() + static_cast<std::ptrdiff_t>(corner), _route.corners.end());
    path.insert(path.end(), tail.begin(), tail.end());
    return path;
  }
  return std::nullopt;
}