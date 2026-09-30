#include "pch.h"
#include "Pathfinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <queue>

namespace
{
// Corners of the polygon around each grown obstacle. More corners hug the circle closer; sixteen keep a path within 2%
// of the circle's radius.
constexpr int POLYGON_CORNERS = 16;
// Kept between a ship's footprint and an obstacle on top of the ship's radius, so that a path never grazes one.
constexpr float MARGIN_METERS = 0.5f;
// A segment may pass this close inside a grown circle and still count as clear: the polygon's own edges touch it.
constexpr float TOLERANCE_METERS = 0.01f;
// Clear() pushes a point out of one obstacle at a time; a point wedged between two needs a few rounds.
constexpr int CLEARING_ROUNDS = 4;
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
  return std::ranges::all_of(m_obstacles,
                             [&](const Obstacle& _obstacle)
                             {
                               const float grown = _obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS;
                               return DistanceToSegment(_obstacle.center, _a, _b) >= grown - TOLERANCE_METERS;
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

  Graph graph;
  // The polygon's corners sit on a circle a little wider than the grown obstacle, so that its edges touch the obstacle
  // rather than cut it.
  const float cornerScale = 1.0f / std::cos(std::numbers::pi_v<float> / POLYGON_CORNERS);
  for (const Obstacle& obstacle : m_obstacles)
  {
    const float grown = obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS;
    for (int corner = 0; corner < POLYGON_CORNERS; ++corner)
    {
      const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(corner) / POLYGON_CORNERS;
      const PlanePosition node = obstacle.center + PlaneVector{std::cos(angle), std::sin(angle)} * (grown * cornerScale);
      const bool free =
        IsInsideEdge(node, _clearanceMeters) &&
        std::ranges::none_of(
          m_obstacles, [&](const Obstacle& _other)
          { return Distance(node, _other.center) < _other.radiusMeters + _clearanceMeters + MARGIN_METERS - TOLERANCE_METERS; });
      if (free)
        graph.nodes.push_back(node);
    }
  }

  graph.edges.resize(graph.nodes.size());
  for (std::uint32_t a = 0; a < graph.nodes.size(); ++a)
  {
    for (std::uint32_t b = a + 1; b < graph.nodes.size(); ++b)
    {
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
  // Whether a corner sees the goal is asked only of the corners the search reaches. Ties on the queue go to the lower
  // node, so the path does not depend on the queue's implementation.
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
  using Entry = std::pair<float, std::uint32_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
  distance[startNode] = 0.0f;
  open.emplace(Distance(start, goal), startNode);

  auto relax = [&](std::uint32_t _from, std::uint32_t _to, float _length)
  {
    if (distance[_from] + _length < distance[_to])
    {
      distance[_to] = distance[_from] + _length;
      previous[_to] = _from;
      open.emplace(distance[_to] + Distance(position(_to), goal), _to);
    }
  };

  while (!open.empty())
  {
    const std::uint32_t current = open.top().second;
    open.pop();
    if (done[current])
      continue;
    done[current] = true;
    if (current == goalNode)
      break;

    if (current == startNode)
    {
      for (std::uint32_t corner = 0; corner < cornerCount; ++corner)
      {
        if (IsStraightPathClear(start, graph.nodes[corner], _clearanceMeters))
          relax(startNode, corner, Distance(start, graph.nodes[corner]));
      }
      continue;
    }
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
