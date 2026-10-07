#include "pch.h"
#include "Pathfinder.h"

#include <algorithm>
#include <array>
#include <bit>
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
// A ship of a group keeps its own lane along a shared route (ADR-047): each corner moved across the way by the ship's
// place in the formation, along the corner's mitre, stretched so that the lane stays as far from the route on both legs,
// but no more than this many times, so that a sharp corner does not throw the lane far out.
constexpr float LANE_MITRE_LIMIT = 2.0f;
// The grid over the obstacles that line tests and corners look in (ADR-054): square cells this wide, about as wide as an
// ore asteroid or a structure grown by a ship's clearance, fifty to a side on the 5 km map, or wider when the obstacles
// spread over more than GRID_MOST_CELLS of them. A line that spans more than GRID_SHORT_LINE_CELLS cells along either
// axis tests every obstacle instead: it would look in so many cells that testing every obstacle's box costs less, as
// ADR-032 found.
constexpr float GRID_CELL_METERS = 100.0f;
constexpr std::int32_t GRID_MOST_CELLS = 256;
constexpr float GRID_SHORT_LINE_CELLS = 3.0f;
// A line test looks this much further from its line than an obstacle's clearance and margin reach, which covers the
// rounding of every sum the lookup makes many times over: it is a few hundredths of a meter at most within the limit
// below.
constexpr float GRID_SLACK_METERS = 1.0f;
// The grid answers only for obstacles and lines within this distance of the origin in either axis, far beyond any map;
// anything further out, or not a number, is tested against every obstacle.
constexpr float GRID_LIMIT_METERS = 65536.0f;

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

// The segment a line test is made along, and the box round it.
struct Segment
{
  Outpost::PlanePosition a;
  Outpost::PlanePosition b;
  float minX = 0.0f;
  float maxX = 0.0f;
  float minZ = 0.0f;
  float maxZ = 0.0f;
};

Segment SegmentOf(Outpost::PlanePosition _a, Outpost::PlanePosition _b) noexcept
{
  return {.a = _a,
          .b = _b,
          .minX = std::min(_a.xMeters, _b.xMeters),
          .maxX = std::max(_a.xMeters, _b.xMeters),
          .minZ = std::min(_a.zMeters, _b.zMeters),
          .maxZ = std::max(_a.zMeters, _b.zMeters)};
}

// Whether a ship of _clearanceMeters passes _obstacle along _segment. Every line test asks this of each obstacle it tests,
// and the answer depends on nothing else, so a test answers the same whichever obstacles it tests (ADR-054).
bool Passes(const Outpost::Obstacle& _obstacle, const Segment& _segment, float _clearanceMeters) noexcept
{
  const float grown = _obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS - TOLERANCE_METERS;
  // Most obstacles are nowhere near the segment's box, and the box is cheaper to test.
  const Outpost::PlanePosition center = _obstacle.center;
  if (center.xMeters + grown < _segment.minX || center.xMeters - grown > _segment.maxX || center.zMeters + grown < _segment.minZ ||
      center.zMeters - grown > _segment.maxZ)
    return true;
  return Outpost::DistanceToSegment(center, _segment.a, _segment.b) >= grown;
}

// Whether _obstacle, grown by _clearanceMeters and the margin, covers _corner, which is then no use to a path.
bool Covers(const Outpost::Obstacle& _obstacle, Outpost::PlanePosition _corner, float _clearanceMeters) noexcept
{
  return Outpost::Distance(_corner, _obstacle.center) < _obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS - TOLERANCE_METERS;
}

// Whether two numbers are the same to the bit, as a graph built from one would be built from the other: -0 and 0 are not.
bool SameBits(float _a, float _b) noexcept
{
  return std::bit_cast<std::uint32_t>(_a) == std::bit_cast<std::uint32_t>(_b);
}

bool SameObstacle(const Outpost::Obstacle& _a, const Outpost::Obstacle& _b) noexcept
{
  return SameBits(_a.center.xMeters, _b.center.xMeters) && SameBits(_a.center.zMeters, _b.center.zMeters) &&
         SameBits(_a.radiusMeters, _b.radiusMeters);
}

// Whether the grid may answer for a coordinate or a distance: false beyond its limit, and for a number that is not one.
bool IsWithinGridLimit(float _meters) noexcept
{
  return std::abs(_meters) <= GRID_LIMIT_METERS;
}

// The grid's cell along one axis for a coordinate, those off the grid in its edge cells. It is the one function both
// placing an obstacle in the grid and looking a line up in it use, and it never falls as the coordinate grows: a lookup
// that covers a range of coordinates so covers every cell any of them is in (ADR-054). The cell is clamped to the grid
// before it is truncated, which then rounds down as floor would.
std::int32_t GridCell(float _meters, float _originMeters, float _cellsPerMeter, std::int32_t _cells) noexcept
{
  return static_cast<std::int32_t>(std::clamp((_meters - _originMeters) * _cellsPerMeter, 0.0f, static_cast<float>(_cells - 1)));
}

// Calls _visit with each corner of the polygon round _obstacle grown by _clearanceMeters and the margin, in order: its
// place, its outward direction, and how far from its tangent a line may turn and still touch the polygon there, squared.
// Every graph's corners come from here, so that a graph built whole and one brought up to date have them to the bit.
template <typename Visit> void ForEachCorner(const Outpost::Obstacle& _obstacle, float _clearanceMeters, const Visit& _visit)
{
  const float grown = _obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS;
  const int corners = grown <= SMALL_RADIUS_METERS ? SMALL_POLYGON_CORNERS : POLYGON_CORNERS;
  // The polygon's corners sit on a circle a little wider than the grown obstacle, so that its edges touch the obstacle
  // rather than cut it.
  const float cornerScale = 1.0f / std::cos(std::numbers::pi_v<float> / static_cast<float>(corners));
  const float touchLimit = std::sin(std::numbers::pi_v<float> / static_cast<float>(corners)) + TANGENT_TOLERANCE;
  for (int corner = 0; corner < corners; ++corner)
  {
    const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(corner) / static_cast<float>(corners);
    const Outpost::PlaneVector normal{std::cos(angle), std::sin(angle)};
    _visit(_obstacle.center + normal * (grown * cornerScale), normal, touchLimit * touchLimit);
  }
}

// A shortest path round convex obstacles bends only at corners, and leaves each corner along a line that touches its
// polygon there: one that cuts into the polygon could be shortened. So only those edges are kept, and whether a line
// touches the polygon at a corner, which a few multiplications tell, is asked before whether anything blocks it, which
// costs a line test (ADR-010). A line touches a regular polygon at a corner when it is within half the polygon's turn at a
// corner of the tangent there. The test is squared, against the squared length of the line, so that it needs no square
// root. It is asked from the lower corner to the higher.
bool TouchesBoth(const Outpost::Pathfinder::Graph& _graph, std::uint32_t _a, std::uint32_t _b) noexcept
{
  const Outpost::PlaneVector between = _graph.nodes[_b] - _graph.nodes[_a];
  const float lengthSquared = Outpost::Dot(between, between);
  const float alongA = Outpost::Dot(between, _graph.normals[_a]);
  if (alongA * alongA > _graph.touchLimitsSquared[_a] * lengthSquared)
    return false;
  const float alongB = Outpost::Dot(between, _graph.normals[_b]);
  return !(alongB * alongB > _graph.touchLimitsSquared[_b] * lengthSquared);
}
} // namespace

void Outpost::Pathfinder::SetObstacles(std::vector<Obstacle> _obstacles, float _halfSizeMeters)
{
  // Every graph is brought up to date when it is next needed, as structures are placed and destroyed; a new edge to the
  // map drops them (ADR-054).
  if (!SameBits(_halfSizeMeters, m_halfSizeMeters))
    m_graphs.clear();
  m_obstacles = std::move(_obstacles);
  m_halfSizeMeters = _halfSizeMeters;
  ++m_version;
  BuildGrid();
}

void Outpost::Pathfinder::BuildGrid()
{
  m_grid = {};
  m_foundInPass.assign(m_obstacles.size(), 0);
  m_pass = 0;
  if (m_obstacles.empty())
    return;

  // The grid covers the square round every obstacle's circle. Only obstacles within its limit, so that the rounding of
  // every sum a lookup makes stays far within its slack: one further out, or not a number, leaves every line test to
  // test every obstacle.
  float minX = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float minZ = std::numeric_limits<float>::infinity();
  float maxZ = -std::numeric_limits<float>::infinity();
  for (const Obstacle& obstacle : m_obstacles)
  {
    const float radius = obstacle.radiusMeters;
    const PlanePosition center = obstacle.center;
    if (!(radius >= 0.0f) || !IsWithinGridLimit(center.xMeters - radius) || !IsWithinGridLimit(center.xMeters + radius) ||
        !IsWithinGridLimit(center.zMeters - radius) || !IsWithinGridLimit(center.zMeters + radius))
      return;
    minX = std::min(minX, center.xMeters - radius);
    maxX = std::max(maxX, center.xMeters + radius);
    minZ = std::min(minZ, center.zMeters - radius);
    maxZ = std::max(maxZ, center.zMeters + radius);
  }
  // Wide enough that the cells cover every square without one falling past the last cell, which a lookup could miss.
  const float widest = std::max(maxX - minX, maxZ - minZ);
  const float cellMeters = std::max(GRID_CELL_METERS, widest / static_cast<float>(GRID_MOST_CELLS - 1));
  const auto cellsAcross = [cellMeters](float _meters)
  { return std::clamp(static_cast<std::int32_t>(std::ceil(_meters / cellMeters)), 1, GRID_MOST_CELLS); };
  m_grid.origin = {minX, minZ};
  m_grid.cellMeters = cellMeters;
  m_grid.cellsPerMeter = 1.0f / cellMeters;
  m_grid.columns = cellsAcross(maxX - minX);
  m_grid.rows = cellsAcross(maxZ - minZ);

  // Each obstacle in every cell its square overlaps, in the obstacles' order: counted, then placed.
  const auto columns = static_cast<std::size_t>(m_grid.columns);
  const auto cellCount = columns * static_cast<std::size_t>(m_grid.rows);
  const auto forEachCell = [&](const Obstacle& _obstacle, const auto& _visit)
  {
    const PlanePosition center = _obstacle.center;
    const float radius = _obstacle.radiusMeters;
    const std::int32_t lastColumn = GridCell(center.xMeters + radius, m_grid.origin.xMeters, m_grid.cellsPerMeter, m_grid.columns);
    const std::int32_t lastRow = GridCell(center.zMeters + radius, m_grid.origin.zMeters, m_grid.cellsPerMeter, m_grid.rows);
    for (std::int32_t row = GridCell(center.zMeters - radius, m_grid.origin.zMeters, m_grid.cellsPerMeter, m_grid.rows); row <= lastRow;
         ++row)
    {
      for (std::int32_t column = GridCell(center.xMeters - radius, m_grid.origin.xMeters, m_grid.cellsPerMeter, m_grid.columns);
           column <= lastColumn; ++column)
        _visit((static_cast<std::size_t>(row) * columns) + static_cast<std::size_t>(column));
    }
  };
  m_grid.cellStarts.assign(cellCount + 1, 0);
  for (const Obstacle& obstacle : m_obstacles)
    forEachCell(obstacle, [&](std::size_t _cell) { ++m_grid.cellStarts[_cell + 1]; });
  for (std::size_t cell = 0; cell < cellCount; ++cell)
    m_grid.cellStarts[cell + 1] += m_grid.cellStarts[cell];
  m_grid.cellObstacles.resize(m_grid.cellStarts[cellCount]);
  std::vector<std::uint32_t> next(m_grid.cellStarts.begin(), m_grid.cellStarts.end() - 1);
  for (std::uint32_t index = 0; index < m_obstacles.size(); ++index)
    forEachCell(m_obstacles[index], [&](std::size_t _cell) { m_grid.cellObstacles[next[_cell]++] = index; });
}

bool Outpost::Pathfinder::GatherNear(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const
{
  // An obstacle whose circle, grown by the clearance and the margin, reaches the segment has its square within reach of
  // a point of the segment, in both axes. So this looks along the segment's longer axis, u, at every column within reach
  // of the segment, and in each at every cell across, v, within reach of where the segment crosses that column. The
  // slack covers the rounding, which is what lets the grid answer exactly as a test of every obstacle does (ADR-054).
  const float reach = _clearanceMeters + MARGIN_METERS + GRID_SLACK_METERS;
  if (m_grid.columns == 0 || !(_clearanceMeters >= 0.0f) || !IsWithinGridLimit(reach) || !IsWithinGridLimit(_a.xMeters) ||
      !IsWithinGridLimit(_a.zMeters) || !IsWithinGridLimit(_b.xMeters) || !IsWithinGridLimit(_b.zMeters))
    return false;
  if (++m_pass == 0)
  {
    std::ranges::fill(m_foundInPass, 0U);
    m_pass = 1;
  }
  m_near.clear();

  const bool alongX = std::abs(_b.xMeters - _a.xMeters) >= std::abs(_b.zMeters - _a.zMeters);
  const float aU = alongX ? _a.xMeters : _a.zMeters;
  const float aV = alongX ? _a.zMeters : _a.xMeters;
  const float bU = alongX ? _b.xMeters : _b.zMeters;
  const float bV = alongX ? _b.zMeters : _b.xMeters;
  const float originU = alongX ? m_grid.origin.xMeters : m_grid.origin.zMeters;
  const float originV = alongX ? m_grid.origin.zMeters : m_grid.origin.xMeters;
  const std::int32_t cellsU = alongX ? m_grid.columns : m_grid.rows;
  const std::int32_t cellsV = alongX ? m_grid.rows : m_grid.columns;
  // How far the segment moves across for each meter along: at most one, since along is its longer axis.
  const float slope = bU != aU ? (bV - aV) / (bU - aU) : 0.0f;
  const float lowU = std::min(aU, bU);
  const float highU = std::max(aU, bU);
  const std::int32_t lastU = GridCell(highU + reach, originU, m_grid.cellsPerMeter, cellsU);
  for (std::int32_t u = GridCell(lowU - reach, originU, m_grid.cellsPerMeter, cellsU); u <= lastU; ++u)
  {
    // The part of the segment within reach of this column, and the cells across that it is within reach of there.
    const float columnLow = originU + (static_cast<float>(u) * m_grid.cellMeters);
    const float from = std::max(lowU, columnLow - reach);
    const float to = std::min(highU, columnLow + m_grid.cellMeters + reach);
    if (from > to)
      continue;
    const float vFrom = aV + ((from - aU) * slope);
    const float vTo = aV + ((to - aU) * slope);
    const std::int32_t lastV = GridCell(std::max(vFrom, vTo) + reach, originV, m_grid.cellsPerMeter, cellsV);
    for (std::int32_t v = GridCell(std::min(vFrom, vTo) - reach, originV, m_grid.cellsPerMeter, cellsV); v <= lastV; ++v)
    {
      const std::int32_t column = alongX ? u : v;
      const std::int32_t row = alongX ? v : u;
      const std::size_t cell =
        (static_cast<std::size_t>(row) * static_cast<std::size_t>(m_grid.columns)) + static_cast<std::size_t>(column);
      for (std::uint32_t entry = m_grid.cellStarts[cell]; entry < m_grid.cellStarts[cell + 1]; ++entry)
      {
        const std::uint32_t obstacle = m_grid.cellObstacles[entry];
        if (m_foundInPass[obstacle] != m_pass)
        {
          m_foundInPass[obstacle] = m_pass;
          m_near.push_back(obstacle);
        }
      }
    }
  }
  return true;
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
  // Only a short line looks in the grid (ADR-054).
  const float spanMeters = std::max(std::abs(_b.xMeters - _a.xMeters), std::abs(_b.zMeters - _a.zMeters));
  if (!(spanMeters <= GRID_SHORT_LINE_CELLS * m_grid.cellMeters) || !GatherNear(_a, _b, _clearanceMeters))
    return IsStraightPathClearOfEveryObstacle(_a, _b, _clearanceMeters);
  const Segment segment = SegmentOf(_a, _b);
  return std::ranges::all_of(m_near, [&](std::uint32_t _obstacle) { return Passes(m_obstacles[_obstacle], segment, _clearanceMeters); });
}

bool Outpost::Pathfinder::OverlapsObstacle(PlanePosition _point, float _radiusMeters) const
{
  const auto overlaps = [&](const Obstacle& _obstacle)
  { return Distance(_point, _obstacle.center) < _obstacle.radiusMeters + _radiusMeters; };
  // A disc that overlaps an obstacle has the obstacle's square within its radius in both axes, so the grid finds it.
  if (!GatherNear(_point, _point, _radiusMeters))
    return std::ranges::any_of(m_obstacles, overlaps);
  return std::ranges::any_of(m_near, [&](std::uint32_t _obstacle) { return overlaps(m_obstacles[_obstacle]); });
}

bool Outpost::Pathfinder::IsStraightPathClearOfEveryObstacle(PlanePosition _a, PlanePosition _b, float _clearanceMeters) const
{
  const Segment segment = SegmentOf(_a, _b);
  return std::ranges::all_of(m_obstacles, [&](const Obstacle& _obstacle) { return Passes(_obstacle, segment, _clearanceMeters); });
}

bool Outpost::Pathfinder::IsUncovered(PlanePosition _corner, float _clearanceMeters) const
{
  const auto covers = [&](const Obstacle& _obstacle) { return Covers(_obstacle, _corner, _clearanceMeters); };
  if (!GatherNear(_corner, _corner, _clearanceMeters))
    return std::ranges::none_of(m_obstacles, covers);
  return std::ranges::none_of(m_near, [&](std::uint32_t _obstacle) { return covers(m_obstacles[_obstacle]); });
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
  const auto found = std::ranges::find(m_graphs, _clearanceMeters, &std::pair<float, Graph>::first);
  if (found != m_graphs.end() && found->second.version == m_version)
    return found->second;

  const ObservedPart building(m_observer, TickPart::GraphBuild);
  if (std::ranges::find(m_clearances, _clearanceMeters) == m_clearances.end())
    m_clearances.push_back(_clearanceMeters);
  // A graph built over the obstacles there were is brought up to date; one never built is built over them all, which is
  // extending an empty graph over every obstacle (ADR-054).
  Graph& graph = found != m_graphs.end() ? (found->second = UpToDate(found->second, _clearanceMeters))
                                         : m_graphs.emplace_back(_clearanceMeters, Extended(Graph{}, _clearanceMeters)).second;
  graph.version = m_version;
  return graph;
}

Outpost::Pathfinder::Graph Outpost::Pathfinder::UpToDate(const Graph& _graph, float _clearanceMeters) const
{
  // The obstacles there are now, matched in order against the graph's: the graph's that are not matched are gone, and
  // those after the last matched are added. A structure destroyed leaves a gap, and one placed comes last.
  std::vector<bool> removed(_graph.obstacles.size(), false);
  std::size_t matched = 0;
  std::size_t removedCount = 0;
  for (std::size_t index = 0; index < _graph.obstacles.size(); ++index)
  {
    if (matched < m_obstacles.size() && SameObstacle(_graph.obstacles[index], m_obstacles[matched]))
      ++matched;
    else
    {
      removed[index] = true;
      ++removedCount;
    }
  }
  if (removedCount == 0)
    return Extended(_graph, _clearanceMeters);
  // With most of them gone, building whole costs less than testing again every line that ran near one.
  if (removedCount * 2 > _graph.obstacles.size())
    return Extended(Graph{}, _clearanceMeters);
  Graph reduced = Reduced(_graph, removed, _clearanceMeters);
  return matched == m_obstacles.size() ? reduced : Extended(reduced, _clearanceMeters);
}

Outpost::Pathfinder::Graph Outpost::Pathfinder::Extended(const Graph& _graph, float _clearanceMeters) const
{
  const auto added = std::span<const Obstacle>(m_obstacles).subspan(_graph.obstacles.size());
  Graph graph;
  graph.obstacles = m_obstacles;

  // The corners round every obstacle, in the obstacles' order, that the map's edge and no obstacle cover. A corner of the
  // graph's own obstacles that none of them covered stays unless an added obstacle covers it, in the order it had, and
  // the added obstacles' corners follow theirs, as they would in a graph built over every obstacle at once.
  constexpr std::uint32_t GONE = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> kept(_graph.nodes.size(), GONE);
  for (std::uint32_t node = 0; node < _graph.nodes.size(); ++node)
  {
    if (std::ranges::any_of(added, [&](const Obstacle& _obstacle) { return Covers(_obstacle, _graph.nodes[node], _clearanceMeters); }))
      continue;
    kept[node] = static_cast<std::uint32_t>(graph.nodes.size());
    graph.nodes.push_back(_graph.nodes[node]);
    graph.normals.push_back(_graph.normals[node]);
    graph.touchLimitsSquared.push_back(_graph.touchLimitsSquared[node]);
  }
  const auto keptNodes = static_cast<std::uint32_t>(graph.nodes.size());
  for (const Obstacle& obstacle : added)
  {
    ForEachCorner(obstacle, _clearanceMeters,
                  [&](PlanePosition _node, PlaneVector _normal, float _touchLimitSquared)
                  {
                    if (!IsInsideEdge(_node, _clearanceMeters) || !IsUncovered(_node, _clearanceMeters))
                      return;
                    graph.nodes.push_back(_node);
                    graph.normals.push_back(_normal);
                    graph.touchLimitsSquared.push_back(_touchLimitSquared);
                  });
  }

  // Only lines that touch both corners' polygons are edges (TouchesBoth). Each line is tested from its lower corner to its
  // higher, and each corner's edges are in the order of the corners they reach.
  graph.edges.resize(graph.nodes.size());
  // An edge between two corners the graph had stands as long as no added obstacle blocks it: whether its line touches
  // both polygons depends on its corners alone, and the graph's own obstacles let it pass already (ADR-054).
  for (std::uint32_t node = 0; node < _graph.nodes.size(); ++node)
  {
    if (kept[node] == GONE)
      continue;
    for (const auto& [neighbor, length] : _graph.edges[node])
    {
      if (neighbor < node || kept[neighbor] == GONE)
        continue;
      const Segment segment = SegmentOf(_graph.nodes[node], _graph.nodes[neighbor]);
      if (!std::ranges::all_of(added, [&](const Obstacle& _obstacle) { return Passes(_obstacle, segment, _clearanceMeters); }))
        continue;
      graph.edges[kept[node]].emplace_back(kept[neighbor], length);
      graph.edges[kept[neighbor]].emplace_back(kept[node], length);
    }
  }
  // Every line to an added obstacle's corner is tested whole.
  for (std::uint32_t a = 0; a < graph.nodes.size(); ++a)
  {
    for (std::uint32_t b = std::max(a + 1, keptNodes); b < graph.nodes.size(); ++b)
    {
      if (!TouchesBoth(graph, a, b) || !IsStraightPathClear(graph.nodes[a], graph.nodes[b], _clearanceMeters))
        continue;
      const float length = Distance(graph.nodes[a], graph.nodes[b]);
      graph.edges[a].emplace_back(b, length);
      graph.edges[b].emplace_back(a, length);
    }
  }
  return graph;
}

Outpost::Pathfinder::Graph Outpost::Pathfinder::Reduced(const Graph& _graph, const std::vector<bool>& _removed,
                                                        float _clearanceMeters) const
{
  constexpr std::uint32_t GONE = std::numeric_limits<std::uint32_t>::max();
  Graph graph;
  std::vector<Obstacle> gone;
  // The corners of the obstacles that stay, in the obstacles' order, as a whole build makes them. The graph's own corners
  // are met in the same order, so each corner made is matched against the next of them: one that matches stays as it was,
  // and any other is one the removed obstacles covered, which a whole build now keeps if nothing else covers it.
  std::vector<std::uint32_t> oldOf;
  std::uint32_t next = 0;
  for (std::size_t index = 0; index < _graph.obstacles.size(); ++index)
  {
    const Obstacle& obstacle = _graph.obstacles[index];
    if (_removed[index])
      gone.push_back(obstacle);
    else
      graph.obstacles.push_back(obstacle);
    ForEachCorner(obstacle, _clearanceMeters,
                  [&](PlanePosition _node, PlaneVector _normal, float _touchLimitSquared)
                  {
                    const bool wasNode = next < _graph.nodes.size() && SameBits(_graph.nodes[next].xMeters, _node.xMeters) &&
                                         SameBits(_graph.nodes[next].zMeters, _node.zMeters) &&
                                         SameBits(_graph.normals[next].xMeters, _normal.xMeters) &&
                                         SameBits(_graph.normals[next].zMeters, _normal.zMeters);
                    const std::uint32_t old = wasNode ? next++ : GONE;
                    if (_removed[index] || (!wasNode && (!IsInsideEdge(_node, _clearanceMeters) || !IsUncovered(_node, _clearanceMeters))))
                      return;
                    oldOf.push_back(old);
                    graph.nodes.push_back(_node);
                    graph.normals.push_back(_normal);
                    graph.touchLimitsSquared.push_back(_touchLimitSquared);
                  });
  }

  // An edge the graph had stands: taking obstacles away blocks nothing. A line between two of its corners that was no edge
  // is tested again only when it runs near a removed obstacle, since nothing else that stopped it has changed; and a line
  // to a corner that came back is tested whole.
  const auto count = static_cast<std::uint32_t>(graph.nodes.size());
  graph.edges.resize(count);
  std::vector<std::uint32_t> newOf(_graph.nodes.size(), GONE);
  for (std::uint32_t node = 0; node < count; ++node)
  {
    if (oldOf[node] != GONE)
      newOf[oldOf[node]] = node;
  }
  // How many of each corner's edges it had, which come first and in order.
  std::vector<std::size_t> hadEdges(count, 0);
  for (std::uint32_t node = 0; node < count; ++node)
  {
    if (oldOf[node] == GONE)
      continue;
    for (const auto& [neighbor, length] : _graph.edges[oldOf[node]])
    {
      if (newOf[neighbor] != GONE)
        graph.edges[node].emplace_back(newOf[neighbor], length);
    }
    hadEdges[node] = graph.edges[node].size();
  }

  // The lines to test, each from its lower corner to its higher, that touch both corners' polygons (TouchesBoth). A line
  // between two corners outside a circle crosses it only when the corners lie nearly opposite each other round its center:
  // when the angle between them, seen from the center, falls short of a half turn by no more than the sum of each one's
  // asin(radius / distance), its narrowness. So the corners are sorted by their angle round each removed obstacle, in
  // bands by their narrowness, each band no more than twice as narrow as the one before; and each corner looks across
  // into every band only as far as its own narrowness and the band's widest allow, widened for rounding. The corners that
  // came back are paired with every corner. The exact test then decides.
  constexpr std::size_t BANDS = 16;
  constexpr float WINDOW_SLACK_RADIANS = 1e-3f;
  constexpr float PI = std::numbers::pi_v<float>;
  std::array<float, BANDS> bandWidest{};
  bandWidest[0] = PI / 2.0f;
  for (std::size_t band = 1; band < BANDS; ++band)
    bandWidest[band] = std::ldexp(1.0f, -static_cast<int>(band));
  std::vector<std::pair<std::uint32_t, std::uint32_t>> lines;
  std::array<std::vector<std::pair<float, std::uint32_t>>, BANDS> byAngle;
  std::vector<float> angles(count, 0.0f);
  std::vector<float> narrowness(count, 0.0f);
  const auto consider = [&](std::uint32_t _a, std::uint32_t _b)
  {
    if (TouchesBoth(graph, std::min(_a, _b), std::max(_a, _b)))
      lines.emplace_back(std::min(_a, _b), std::max(_a, _b));
  };
  for (const Obstacle& obstacle : gone)
  {
    const float grown = obstacle.radiusMeters + _clearanceMeters + MARGIN_METERS;
    for (auto& band : byAngle)
      band.clear();
    for (std::uint32_t node = 0; node < count; ++node)
    {
      if (oldOf[node] == GONE)
        continue;
      const PlaneVector away = graph.nodes[node] - obstacle.center;
      const float distance = std::hypot(away.xMeters, away.zMeters);
      narrowness[node] = distance > grown ? std::asin(grown / distance) : PI / 2.0f;
      angles[node] = std::atan2(away.zMeters, away.xMeters);
      std::size_t band = 0;
      while (band + 1 < BANDS && narrowness[node] <= bandWidest[band + 1])
        ++band;
      byAngle[band].emplace_back(angles[node], node);
    }
    for (auto& band : byAngle)
      std::ranges::sort(band);
    for (std::uint32_t a = 0; a < count; ++a)
    {
      if (oldOf[a] == GONE)
        continue;
      for (std::size_t band = 0; band < BANDS; ++band)
      {
        const auto& sorted = byAngle[band];
        if (sorted.empty())
          continue;
        const float halfWidth = narrowness[a] + bandWidest[band] + WINDOW_SLACK_RADIANS;
        const float opposite = angles[a] + PI;
        // The window round the opposite side, which may wrap past either end of the angles.
        for (const float shift : {-2.0f * PI, 0.0f, 2.0f * PI})
        {
          const float low = opposite - halfWidth + shift;
          const float high = opposite + halfWidth + shift;
          if (high < -PI - WINDOW_SLACK_RADIANS || low > PI + WINDOW_SLACK_RADIANS)
            continue;
          const auto from = std::ranges::lower_bound(sorted, low, {}, &std::pair<float, std::uint32_t>::first);
          const auto to = std::ranges::upper_bound(sorted, high, {}, &std::pair<float, std::uint32_t>::first);
          for (auto other = from; other < to; ++other)
          {
            if (other->second > a)
              consider(a, other->second);
          }
        }
      }
    }
  }
  for (std::uint32_t a = 0; a < count; ++a)
  {
    if (oldOf[a] != GONE)
      continue;
    for (std::uint32_t b = 0; b < count; ++b)
    {
      if (b != a && (oldOf[b] != GONE || b > a))
        consider(a, b);
    }
  }
  std::ranges::sort(lines);
  const auto [last, end] = std::ranges::unique(lines);
  lines.erase(last, end);

  std::vector<bool> extended(count, false);
  for (const auto& [a, b] : lines)
  {
    if (oldOf[a] != GONE && oldOf[b] != GONE)
    {
      const auto had = std::span(graph.edges[a]).first(hadEdges[a]);
      const auto edge = std::ranges::lower_bound(had, b, {}, &std::pair<std::uint32_t, float>::first);
      if (edge != had.end() && edge->first == b)
        continue;
      const Segment segment = SegmentOf(graph.nodes[a], graph.nodes[b]);
      if (std::ranges::all_of(gone, [&](const Obstacle& _obstacle) { return Passes(_obstacle, segment, _clearanceMeters); }))
        continue;
    }
    if (!IsStraightPathClear(graph.nodes[a], graph.nodes[b], _clearanceMeters))
      continue;
    const float length = Distance(graph.nodes[a], graph.nodes[b]);
    graph.edges[a].emplace_back(b, length);
    graph.edges[b].emplace_back(a, length);
    extended[a] = true;
    extended[b] = true;
  }
  // Each corner's edges in the order of the corners they reach, as a whole build makes them.
  for (std::vector<std::pair<std::uint32_t, float>>& edges : graph.edges)
    std::ranges::sort(edges, {}, &std::pair<std::uint32_t, float>::first);
  return graph;
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
    const auto found = std::ranges::find(m_graphs, clearance, &std::pair<float, Graph>::first);
    if (found == m_graphs.end() || found->second.version != m_version)
    {
      (void)GraphFor(clearance);
      return true;
    }
  }
  return false;
}

Outpost::GroupRoutes::GroupRoutes(const Pathfinder& _pathfinder, PlanePosition _destination, float _widestClearanceMeters,
                                  float _bandHalfWidthMeters, std::vector<Route> _routes)
  : m_pathfinder(_pathfinder),
    m_widestClearanceMeters(_widestClearanceMeters),
    m_bandHalfWidthMeters(_bandHalfWidthMeters),
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
  {
    std::vector<Band> bands = BandsAlong(_center, corners, m_widestClearanceMeters);
    m_routes.push_back({std::move(corners), m_widestClearanceMeters, std::move(bands)});
  }
}

Outpost::GroupRoutes::Way Outpost::GroupRoutes::PathFor(PlanePosition _start, PlanePosition _slot, float _clearanceMeters,
                                                        float _laneMeters)
{
  const PlanePosition goal = m_pathfinder.Clear(_slot, _clearanceMeters);
  // A ship in an obstacle's margin has to leave it first, which the full search knows how to do.
  const bool inMargin = Distance(m_pathfinder.Clear(_start, _clearanceMeters), _start) > 0.0f;
  if (!inMargin)
  {
    if (m_pathfinder.IsStraightPathClear(_start, goal, _clearanceMeters))
      return {.waypoints = {goal}};
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
      if (std::optional<Way> way = Join(route, _start, goal, _clearanceMeters, _laneMeters);
          way.has_value() && PathLength(_start, way->waypoints) <= limitMeters)
        return std::move(*way);
    }
    if (m_pathfinder.IsStraightPathClear(_start, m_destination, _clearanceMeters) &&
        m_pathfinder.IsStraightPathClear(m_destination, goal, _clearanceMeters) &&
        Distance(_start, m_destination) + Distance(m_destination, goal) <= limitMeters)
      return {.waypoints = {m_destination, goal}};
  }

  std::vector<PlanePosition> path = m_pathfinder.FindPath(_start, goal, _clearanceMeters);
  if (path.size() > 1)
  {
    std::vector<PlanePosition> corners(path.begin(), path.end() - 1);
    std::vector<Band> bands = BandsAlong(_start, corners, _clearanceMeters);
    m_routes.push_back({std::move(corners), _clearanceMeters, std::move(bands)});
  }
  return {.waypoints = std::move(path)};
}

std::vector<Outpost::GroupRoutes::Band> Outpost::GroupRoutes::BandsAlong(PlanePosition _from, const std::vector<PlanePosition>& _corners,
                                                                         float _clearanceMeters) const
{
  if (m_bandHalfWidthMeters <= 0.0f)
    return {};
  std::vector<Band> bands;
  bands.reserve(_corners.size());
  for (std::size_t corner = 0; corner < _corners.size(); ++corner)
  {
    const PlanePosition at = _corners[corner];
    const PlanePosition before = corner > 0 ? _corners[corner - 1] : _from;
    const PlanePosition onward = corner + 1 < _corners.size() ? _corners[corner + 1] : m_destination;
    const PlaneVector out = Normalized(onward - at, {1.0f, 0.0f});
    const PlaneVector in = Normalized(at - before, out);
    const PlaneVector mitre = Normalized(Perpendicular(in) + Perpendicular(out), Perpendicular(out));
    const PlaneVector across = mitre * (1.0f / std::max(Dot(mitre, Perpendicular(out)), 1.0f / LANE_MITRE_LIMIT));
    // A corner hugs the obstacle the route turns round, so the band centered on it would run into the obstacle: it moves
    // across by its half width to whichever side is clear, and its edge passes the corner. A band that fits on neither
    // side stays centered, and its ships fall back towards the corner one by one.
    Band band{.middle = at, .across = across};
    for (const float shiftMeters : {0.0f, m_bandHalfWidthMeters, -m_bandHalfWidthMeters})
    {
      const PlanePosition middle = at + across * shiftMeters;
      if (IsClearAt(middle + across * m_bandHalfWidthMeters, _clearanceMeters) &&
          IsClearAt(middle - across * m_bandHalfWidthMeters, _clearanceMeters))
      {
        band.middle = middle;
        break;
      }
    }
    bands.push_back(band);
  }
  return bands;
}

bool Outpost::GroupRoutes::IsClearAt(PlanePosition _position, float _clearanceMeters) const
{
  return m_pathfinder.IsStraightPathClear(_position, _position, _clearanceMeters) &&
         m_pathfinder.InsideEdge(_position, _clearanceMeters) == _position;
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

std::optional<Outpost::GroupRoutes::Way> Outpost::GroupRoutes::Join(const Route& _route, PlanePosition _start, PlanePosition _goal,
                                                                    float _clearanceMeters, float _laneMeters) const
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
    Way way;
    if (std::optional<std::vector<PlanePosition>> lane = Lane(_route, corner, _start, _laneMeters, tail.front(), _clearanceMeters))
    {
      way.waypoints = std::move(*lane);
      way.laneEnd = way.waypoints.back();
    }
    else
      way.waypoints.assign(_route.corners.begin() + static_cast<std::ptrdiff_t>(corner), _route.corners.end());
    way.waypoints.insert(way.waypoints.end(), tail.begin(), tail.end());
    return way;
  }
  return std::nullopt;
}

std::optional<std::vector<Outpost::PlanePosition>> Outpost::GroupRoutes::Lane(const Route& _route, std::size_t _corner,
                                                                              PlanePosition _start, float _laneMeters, PlanePosition _next,
                                                                              float _clearanceMeters) const
{
  if (_route.bands.size() != _route.corners.size())
    return std::nullopt;
  const float laneMeters = std::clamp(_laneMeters, -m_bandHalfWidthMeters, m_bandHalfWidthMeters);

  std::vector<PlanePosition> lane;
  lane.reserve(_route.corners.size() - _corner);
  PlanePosition from = _start;
  for (std::size_t corner = _corner; corner < _route.corners.size(); ++corner)
  {
    // The lane where it is clear; halfway back to the route's corner where that is; the corner itself where neither is.
    const PlanePosition at = _route.corners[corner];
    const Band& band = _route.bands[corner];
    const PlaneVector toLane = (band.middle + band.across * laneMeters) - at;
    std::optional<PlanePosition> chosen;
    for (const float share : {1.0f, 0.5f, 0.0f})
    {
      const PlanePosition candidate = at + toLane * share;
      // The line test ends at the candidate, so it also says the candidate is clear of every obstacle; only the map's
      // edge is left to test.
      if (m_pathfinder.InsideEdge(candidate, _clearanceMeters) == candidate &&
          m_pathfinder.IsStraightPathClear(from, candidate, _clearanceMeters))
      {
        chosen = candidate;
        break;
      }
      if (toLane == PlaneVector{})
        break;
    }
    if (!chosen.has_value())
      return std::nullopt;
    lane.push_back(*chosen);
    from = *chosen;
  }
  if (!m_pathfinder.IsStraightPathClear(from, _next, _clearanceMeters))
    return std::nullopt;
  return lane;
}