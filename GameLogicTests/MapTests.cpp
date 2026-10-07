#include "pch.h"
#include "RepositoryData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
struct Obstacle
{
  Outpost::PlanePosition position;
  float radiusMeters = 0.0f;
};

std::vector<Obstacle> Obstacles(const Outpost::Map& _map)
{
  std::vector<Obstacle> obstacles;
  obstacles.reserve(_map.oreAsteroids.size() + _map.asteroidFields.size());
  for (const Outpost::OreAsteroidPlacement& asteroid : _map.oreAsteroids)
    obstacles.push_back({asteroid.position, asteroid.radiusMeters});
  for (const Outpost::AsteroidFieldPlacement& field : _map.asteroidFields)
    obstacles.push_back({field.position, field.radiusMeters});
  return obstacles;
}

// Where a disc of _clearanceMeters can stand, on a square grid over the map, and which of those places can be reached
// from a start. Built independently of the loader's own gap check, so it tests that check's promise.
class ReachabilityGrid
{
public:
  static constexpr double CELL_METERS = 10.0;

  ReachabilityGrid(const Outpost::Map& _map, double _clearanceMeters)
    : m_half(_map.sizeMeters / 2.0),
      m_cells(static_cast<int>(_map.sizeMeters / CELL_METERS)),
      m_free(static_cast<size_t>(m_cells) * static_cast<size_t>(m_cells)),
      m_reached(m_free.size())
  {
    const std::vector<Obstacle> obstacles = Obstacles(_map);
    for (int row = 0; row < m_cells; ++row)
    {
      for (int column = 0; column < m_cells; ++column)
      {
        const Outpost::PlanePosition center = Center(column, row);
        bool free = m_half - std::abs(center.xMeters) >= _clearanceMeters && m_half - std::abs(center.zMeters) >= _clearanceMeters;
        for (const Obstacle& obstacle : obstacles)
          free = free && Distance(center, obstacle.position) - obstacle.radiusMeters >= _clearanceMeters;
        m_free[Index(column, row)] = free;
      }
    }
  }

  // Marks every free cell connected to the one holding _start. Returns false when the start's own cell is blocked.
  bool FloodFrom(Outpost::PlanePosition _start)
  {
    const int startColumn = ToCell(_start.xMeters);
    const int startRow = ToCell(_start.zMeters);
    if (!m_free[Index(startColumn, startRow)])
      return false;
    std::ranges::fill(m_reached, false);
    std::deque<std::pair<int, int>> open{{startColumn, startRow}};
    m_reached[Index(startColumn, startRow)] = true;
    while (!open.empty())
    {
      const auto [column, row] = open.front();
      open.pop_front();
      for (const auto& [dx, dz] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
      {
        const int nextColumn = column + dx;
        const int nextRow = row + dz;
        if (nextColumn < 0 || nextRow < 0 || nextColumn >= m_cells || nextRow >= m_cells)
          continue;
        const size_t next = Index(nextColumn, nextRow);
        if (m_free[next] && !m_reached[next])
        {
          m_reached[next] = true;
          open.emplace_back(nextColumn, nextRow);
        }
      }
    }
    return true;
  }

  // Whether the last flood reached a place from which _obstacle is within _reachMeters of its edge.
  [[nodiscard]] bool Reached(const Obstacle& _obstacle, double _reachMeters) const
  {
    for (int row = 0; row < m_cells; ++row)
    {
      for (int column = 0; column < m_cells; ++column)
      {
        if (m_reached[Index(column, row)] && Distance(Center(column, row), _obstacle.position) - _obstacle.radiusMeters <= _reachMeters)
          return true;
      }
    }
    return false;
  }

private:
  [[nodiscard]] size_t Index(int _column, int _row) const
  {
    return static_cast<size_t>(_row) * static_cast<size_t>(m_cells) + static_cast<size_t>(_column);
  }

  [[nodiscard]] int ToCell(float _meters) const
  {
    return std::clamp(static_cast<int>((_meters + m_half) / CELL_METERS), 0, m_cells - 1);
  }

  [[nodiscard]] Outpost::PlanePosition Center(int _column, int _row) const
  {
    return {.xMeters = static_cast<float>(-m_half + (_column + 0.5) * CELL_METERS),
            .zMeters = static_cast<float>(-m_half + (_row + 0.5) * CELL_METERS)};
  }

  double m_half = 0.0;
  int m_cells = 0;
  std::vector<bool> m_free;
  std::vector<bool> m_reached;
};

// A small map that loads, for the error cases to break one thing at a time.
constexpr std::string_view MINIMAL_MAP = R"({
  "sizeMeters": 1000,
  "minimumGapMeters": 50,
  "starts": [ { "xMeters": -300, "zMeters": -300 }, { "xMeters": 300, "zMeters": 300 } ],
  "oreAsteroids": [ { "xMeters": -300, "zMeters": -100, "radiusMeters": 40, "yield": "home", "reserve": 7500 },
                    { "xMeters": 0, "zMeters": 200, "radiusMeters": 40, "yield": "contested", "reserve": 12000 } ],
  "asteroidFields": [ { "xMeters": 0, "zMeters": 0, "radiusMeters": 100 } ]
})";

std::string Replace(std::string_view _from, std::string_view _to)
{
  std::string text(MINIMAL_MAP);
  const size_t at = text.find(_from);
  Assert::IsTrue(at != std::string::npos && text.find(_from, at + 1) == std::string::npos);
  text.replace(at, _from.size(), _to);
  return text;
}

void ExpectLoadError(const std::string& _text, std::string_view _where)
{
  std::string message;
  try
  {
    (void)Outpost::LoadMap(_text);
  }
  catch (const Neuron::Exception& error)
  {
    message = error.what();
  }
  Assert::IsFalse(message.empty(), L"the map loaded");
  Assert::IsTrue(message.starts_with("Map: ") && message.find(_where) != std::string::npos,
                 std::wstring(message.begin(), message.end()).c_str());
}
} // namespace

TEST_CLASS(MapTests)
{
public:
  // Phase 4 design §6: 10 km a side, the starts in opposite corners 1 km in from the edges, and Phase 1's four rings of
  // ore, each richer and further out than the last, 459,000 Ore in all.
  TEST_METHOD(TheRepositoryMapHasPhaseFoursShape)
  {
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Assert::AreEqual(10000.0f, map.sizeMeters);
    Assert::AreEqual(size_t{2}, map.starts.size());
    Assert::AreEqual(-4000.0f, map.starts[0].xMeters);
    Assert::AreEqual(-4000.0f, map.starts[0].zMeters);

    // Each ring's count, by the start it is nearer, and its reserve.
    struct Ring
    {
      Outpost::OreYield yield;
      int nearFirst = 0;
      int nearSecond = 0;
      int reserveOre = 0;
    };
    std::array<Ring, 4> rings{{{Outpost::OreYield::Home, 3, 3, 7500},
                               {Outpost::OreYield::Near, 7, 7, 9000},
                               {Outpost::OreYield::Contested, 8, 8, 12000},
                               {Outpost::OreYield::Rich, 2, 2, 24000}}};
    std::int64_t total = 0;
    for (const Outpost::OreAsteroidPlacement& asteroid : map.oreAsteroids)
    {
      Ring& ring = *std::ranges::find(rings, asteroid.yield, &Ring::yield);
      Assert::AreEqual(ring.reserveOre, asteroid.reserveOre.value_or(0));
      total += asteroid.reserveOre.value_or(0);
      const float first = Distance(asteroid.position, map.starts[0]);
      const float second = Distance(asteroid.position, map.starts[1]);
      // A contested or rich asteroid as far from one start as from the other counts for neither.
      if (first < second - 1.0f)
        --ring.nearFirst;
      else if (second < first - 1.0f)
        --ring.nearSecond;
      else
      {
        --ring.nearFirst;
        --ring.nearSecond;
      }
    }
    for (const Ring& ring : rings)
    {
      Assert::AreEqual(0, ring.nearFirst);
      Assert::AreEqual(0, ring.nearSecond);
    }
    Assert::AreEqual(std::int64_t{459000}, total);
    Assert::IsFalse(map.asteroidFields.empty());
  }

  // Phase 4 design §6: the map is laid out in 25 sectors of 2 km for Phase 2's territory. They tile the map, each
  // holds its node, every asteroid and start lies in one, two are adjacent exactly when they share a border, and every
  // sector can be reached from both starts' sectors.
  TEST_METHOD(TheRepositoryMapIsLaidOutInSectors)
  {
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Assert::AreEqual(size_t{25}, map.sectors.size());
    double area = 0.0;
    for (const Outpost::SectorPlacement& sector : map.sectors)
      area += static_cast<double>(sector.maxXMeters - sector.minXMeters) * (sector.maxZMeters - sector.minZMeters);
    Assert::AreEqual(static_cast<double>(map.sizeMeters) * map.sizeMeters, area, 1.0, L"the sectors cover the map");

    const auto inside = [&map](Outpost::PlanePosition _position)
    {
      return std::ranges::count_if(map.sectors,
                                   [_position](const Outpost::SectorPlacement& _sector) { return _sector.Contains(_position); });
    };
    for (const Outpost::OreAsteroidPlacement& asteroid : map.oreAsteroids)
      Assert::AreEqual(std::ptrdiff_t{1}, inside(asteroid.position));
    for (const Outpost::PlanePosition start : map.starts)
      Assert::AreEqual(std::ptrdiff_t{1}, inside(start));

    for (const Outpost::SectorPlacement& sector : map.sectors)
    {
      for (const Outpost::SectorPlacement& other : map.sectors)
      {
        if (&other == &sector)
          continue;
        const bool overlapX = sector.minXMeters < other.maxXMeters && other.minXMeters < sector.maxXMeters;
        const bool overlapZ = sector.minZMeters < other.maxZMeters && other.minZMeters < sector.maxZMeters;
        Assert::IsFalse(overlapX && overlapZ, L"two sectors overlap");
        const bool shareX = (sector.maxXMeters == other.minXMeters || other.maxXMeters == sector.minXMeters) && overlapZ;
        const bool shareZ = (sector.maxZMeters == other.minZMeters || other.maxZMeters == sector.minZMeters) && overlapX;
        Assert::AreEqual(shareX || shareZ, std::ranges::find(sector.adjacent, other.id) != sector.adjacent.end(),
                         L"adjacency is sharing a border");
      }
    }

    for (const Outpost::PlanePosition start : map.starts)
    {
      const auto home =
        std::ranges::find_if(map.sectors, [start](const Outpost::SectorPlacement& _sector) { return _sector.Contains(start); });
      std::vector<std::int32_t> reached{home->id};
      for (size_t next = 0; next < reached.size(); ++next)
      {
        const auto sector = std::ranges::find(map.sectors, reached[next], &Outpost::SectorPlacement::id);
        for (const std::int32_t other : sector->adjacent)
        {
          if (std::ranges::find(reached, other) == reached.end())
            reached.push_back(other);
        }
      }
      Assert::AreEqual(map.sectors.size(), reached.size());
    }
  }

  // Neither start is favored: turning the map half a turn about its center gives the same map, with the starts swapped.
  TEST_METHOD(TheRepositoryMapIsPointSymmetric)
  {
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    auto mirrored = [](Outpost::PlanePosition _position) { return Outpost::PlanePosition{-_position.xMeters, -_position.zMeters}; };

    Assert::IsTrue(mirrored(map.starts[0]) == map.starts[1]);
    for (const Outpost::OreAsteroidPlacement& asteroid : map.oreAsteroids)
    {
      Assert::IsTrue(std::ranges::any_of(map.oreAsteroids,
                                         [&](const Outpost::OreAsteroidPlacement& _other)
                                         {
                                           return _other.position == mirrored(asteroid.position) &&
                                                  _other.radiusMeters == asteroid.radiusMeters && _other.yield == asteroid.yield &&
                                                  _other.reserveOre == asteroid.reserveOre;
                                         }));
    }
    for (const Outpost::AsteroidFieldPlacement& field : map.asteroidFields)
    {
      Assert::IsTrue(
        std::ranges::any_of(map.asteroidFields, [&](const Outpost::AsteroidFieldPlacement& _other)
                            { return _other.position == mirrored(field.position) && _other.radiusMeters == field.radiusMeters; }));
    }
  }

  // A ship as wide as the narrowest passage the map promises gets from either start to the side of every asteroid and
  // every field (plan task 2.3).
  TEST_METHOD(EveryAsteroidCanBeReachedFromBothStarts)
  {
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    // Just under half the minimum gap, so that a passage exactly as wide as the gap still lets the disc through on the grid.
    const double clearance = map.minimumGapMeters / 2.0 - ReachabilityGrid::CELL_METERS;
    ReachabilityGrid grid(map, clearance);
    for (const Outpost::PlanePosition start : map.starts)
    {
      Assert::IsTrue(grid.FloodFrom(start), L"a start is blocked");
      for (const Obstacle& obstacle : Obstacles(map))
        Assert::IsTrue(grid.Reached(obstacle, clearance + 2.0 * ReachabilityGrid::CELL_METERS));
    }
  }

  TEST_METHOD(LoadsAMinimalMap)
  {
    const Outpost::Map map = Outpost::LoadMap(MINIMAL_MAP);
    Assert::AreEqual(50.0f, map.minimumGapMeters);
    Assert::IsTrue(map.starts[1] == Outpost::PlanePosition{.xMeters = 300.0f, .zMeters = 300.0f});
    Assert::IsTrue(map.oreAsteroids[1].yield == Outpost::OreYield::Contested);
    // Phase 1 design §8: every ore asteroid holds a reserve.
    Assert::AreEqual(12000, map.oreAsteroids[1].reserveOre.value_or(0));
    Assert::AreEqual(100.0f, map.asteroidFields[0].radiusMeters);
  }

  TEST_METHOD(RejectsABrokenMap)
  {
    ExpectLoadError(Replace("\"radiusMeters\": 100", "\"radiusMeters\": 150"), "asteroidFields[0]: is closer than 50 m to oreAsteroids[1]");
    ExpectLoadError(Replace("\"xMeters\": -300, \"zMeters\": -100", "\"xMeters\": -440, \"zMeters\": -100"),
                    "oreAsteroids[0]: is closer than 50 m to the edge");
    ExpectLoadError(Replace("{ \"xMeters\": -300, \"zMeters\": -300 }", "{ \"xMeters\": -300, \"zMeters\": -160 }"),
                    "starts[0]: is closer than 50 m to oreAsteroids[0]");
    ExpectLoadError(Replace("{ \"xMeters\": 300, \"zMeters\": 300 } ]",
                            "{ \"xMeters\": 300, \"zMeters\": 300 }, { \"xMeters\": 0, \"zMeters\": -300 } ]"),
                    "starts: has 3 starts");
    ExpectLoadError(Replace("\"yield\": \"home\"", "\"yield\": \"poor\""), "oreAsteroids[0].yield");
    ExpectLoadError(Replace("\"reserve\": 7500", "\"reserve\": 0"), "oreAsteroids[0].reserve");
    ExpectLoadError(Replace(", \"reserve\": 7500", ""), "oreAsteroids[0]: has no \"reserve\"");
    ExpectLoadError(Replace("\"sizeMeters\": 1000,", "\"sizeMeters\": 1000, \"sizeMetres\": 1000,"), "sizeMetres");
    ExpectLoadError(Replace("\"minimumGapMeters\": 50,", ""), "the file: has no \"minimumGapMeters\"");
  }

  // The loader's checks of the sectors (Phase 1 design §8).
  TEST_METHOD(RejectsBrokenSectors)
  {
    constexpr std::string_view SECTORS = R"(], "sectors": [
      { "id": 1, "name": "West", "minXMeters": -500, "maxXMeters": 0, "minZMeters": -500, "maxZMeters": 500,
        "node": { "xMeters": -300, "zMeters": 300 }, "adjacent": [2] },
      { "id": 2, "name": "East", "minXMeters": 0, "maxXMeters": 500, "minZMeters": -500, "maxZMeters": 500,
        "node": { "xMeters": 300, "zMeters": -300 }, "adjacent": [1] } ] })";
    std::string text(MINIMAL_MAP);
    text.replace(text.rfind(']'), std::string::npos, SECTORS);
    const Outpost::Map map = Outpost::LoadMap(text);
    Assert::AreEqual(size_t{2}, map.sectors.size());
    Assert::IsTrue(map.sectors[1].adjacent == std::vector<std::int32_t>{1});

    const auto broken = [&text](std::string_view _from, std::string_view _to)
    {
      std::string changed = text;
      changed.replace(changed.find(_from), _from.size(), _to);
      return changed;
    };
    ExpectLoadError(broken("\"id\": 2", "\"id\": 1"), "sectors[1].id");
    ExpectLoadError(broken("\"xMeters\": 300, \"zMeters\": -300", "\"xMeters\": -300, \"zMeters\": -300"), "sectors[1].node: is outside");
    ExpectLoadError(broken("\"xMeters\": -300, \"zMeters\": 300", "\"xMeters\": -120, \"zMeters\": 0"), "sectors[0].node: is closer");
    ExpectLoadError(broken("\"adjacent\": [1]", "\"adjacent\": []"), "sectors[0].adjacent");
    ExpectLoadError(broken("\"adjacent\": [2]", "\"adjacent\": [3]"), "sectors[0].adjacent");
    ExpectLoadError(broken("\"maxXMeters\": 500", "\"maxXMeters\": 600"), "sectors[1]: is not a rectangle");
  }

  TEST_METHOD(ThePlacedMapIsInTheSnapshot)
  {
    const Outpost::Map map = Outpost::LoadMap(MINIMAL_MAP);
    Outpost::Simulation simulation(1, 20);
    simulation.PlaceMap(map);
    const Outpost::Snapshot snapshot = simulation.BuildSnapshot(Outpost::PlayerId{1});
    Assert::AreEqual(size_t{3}, snapshot.entities.size());
    Assert::IsTrue(snapshot.entities[0].kind == Outpost::EntityKind::Asteroid);
    Assert::IsFalse(snapshot.entities[0].owner.IsValid());
    Assert::AreEqual(40.0f, snapshot.entities[0].radiusMeters);
    Assert::IsTrue(snapshot.entities[2].kind == Outpost::EntityKind::AsteroidField);
    Assert::IsTrue(snapshot.entities[2].position == Outpost::PlanePosition{});
    Assert::IsTrue(simulation.FindEntity(snapshot.entities[1].id)->oreYield == Outpost::OreYield::Contested);

    // An asteroid is not a ship, so no order can move it.
    const std::vector<Outpost::CommandResult> results = simulation.Tick(
      {{.player = Outpost::PlayerId{1}, .order = Outpost::MoveCommand{.ships = {snapshot.entities[0].id}, .destination = {}}}});
    Assert::IsTrue(results[0] == Outpost::CommandResult::NotAShip);
  }
};
} // namespace GameLogicTests