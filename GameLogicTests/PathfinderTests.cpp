#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr float SHIP_RADIUS_METERS = 8.0f;

float PathLength(Outpost::PlanePosition _start, const std::vector<Outpost::PlanePosition>& _path)
{
  float length = 0.0f;
  Outpost::PlanePosition previous = _start;
  for (const Outpost::PlanePosition waypoint : _path)
  {
    length += Outpost::Distance(previous, waypoint);
    previous = waypoint;
  }
  return length;
}

// Every leg of the path keeps the ship's footprint off every obstacle.
void ExpectClear(const Outpost::Pathfinder& _pathfinder, Outpost::PlanePosition _start, const std::vector<Outpost::PlanePosition>& _path)
{
  Outpost::PlanePosition previous = _start;
  for (const Outpost::PlanePosition waypoint : _path)
  {
    for (const Outpost::Obstacle& obstacle : _pathfinder.Obstacles())
      Assert::IsTrue(Outpost::DistanceToSegment(obstacle.center, previous, waypoint) >= obstacle.radiusMeters + SHIP_RADIUS_METERS);
    previous = waypoint;
  }
}
} // namespace

TEST_CLASS(PathfinderTests)
{
public:
  TEST_METHOD(GoesStraightWhenNothingIsInTheWay)
  {
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({{.center = {0.0f, 300.0f}, .radiusMeters = 100.0f}}, 1000.0f);
    const std::vector<Outpost::PlanePosition> path = pathfinder.FindPath({-300.0f, 0.0f}, {300.0f, 0.0f}, SHIP_RADIUS_METERS);
    Assert::AreEqual(size_t{1}, path.size());
    Assert::IsTrue(path[0] == Outpost::PlanePosition{300.0f, 0.0f});
  }

  TEST_METHOD(GoesAroundAnObstacleByAShortWay)
  {
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({{.center = {}, .radiusMeters = 100.0f}}, 1000.0f);
    const Outpost::PlanePosition start{-300.0f, 0.0f};
    const Outpost::PlanePosition goal{300.0f, 0.0f};
    const std::vector<Outpost::PlanePosition> path = pathfinder.FindPath(start, goal, SHIP_RADIUS_METERS);

    Assert::IsTrue(path.size() > 1);
    Assert::IsTrue(path.back() == goal);
    ExpectClear(pathfinder, start, path);
    // The shortest way round a 108 m circle between these points is about 645 m: two tangents and an arc.
    const float length = PathLength(start, path);
    Assert::IsTrue(length > 600.0f && length < 660.0f, std::to_wstring(length).c_str());
  }

  TEST_METHOD(FindsTheGapBetweenTwoObstacles)
  {
    Outpost::Pathfinder pathfinder;
    // A wall with one 60 m gap in it, the map's minimum, between (0, -30) and (0, 30).
    pathfinder.SetObstacles({{.center = {0.0f, -330.0f}, .radiusMeters = 300.0f}, {.center = {0.0f, 330.0f}, .radiusMeters = 300.0f}},
                            700.0f);
    const Outpost::PlanePosition start{-400.0f, 200.0f};
    const std::vector<Outpost::PlanePosition> path = pathfinder.FindPath(start, {400.0f, -200.0f}, SHIP_RADIUS_METERS);
    ExpectClear(pathfinder, start, path);
    Assert::IsTrue(path.back() == Outpost::PlanePosition{400.0f, -200.0f});
  }

  TEST_METHOD(StopsBesideAnObstacleOrderedOntoIt)
  {
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({{.center = {}, .radiusMeters = 100.0f}}, 1000.0f);
    const std::vector<Outpost::PlanePosition> path = pathfinder.FindPath({-300.0f, 0.0f}, {10.0f, 0.0f}, SHIP_RADIUS_METERS);
    Assert::IsTrue(Outpost::Distance(path.back(), {}) >= 100.0f + SHIP_RADIUS_METERS);
  }

  TEST_METHOD(KeepsInsideTheEdge)
  {
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({}, 500.0f);
    const std::vector<Outpost::PlanePosition> path = pathfinder.FindPath({}, {900.0f, -900.0f}, SHIP_RADIUS_METERS);
    Assert::IsTrue(path.back() == Outpost::PlanePosition{500.0f - SHIP_RADIUS_METERS, -500.0f + SHIP_RADIUS_METERS});
  }

  TEST_METHOD(FindsAWayAcrossTheRepositoryMap)
  {
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Outpost::Pathfinder pathfinder;
    std::vector<Outpost::Obstacle> obstacles;
    obstacles.reserve(map.oreAsteroids.size() + map.asteroidFields.size());
    for (const Outpost::OreAsteroidPlacement& asteroid : map.oreAsteroids)
      obstacles.push_back({asteroid.position, asteroid.radiusMeters});
    for (const Outpost::AsteroidFieldPlacement& field : map.asteroidFields)
      obstacles.push_back({field.position, field.radiusMeters});
    pathfinder.SetObstacles(obstacles, map.sizeMeters / 2.0f);

    // Start to start, the longest trip on the map, for the widest hull the map allows.
    const float clearance = map.minimumGapMeters / 2.0f - 1.0f;
    const std::vector<Outpost::PlanePosition> path = pathfinder.FindPath(map.starts[0], map.starts[1], clearance);
    Assert::IsTrue(path.back() == map.starts[1]);
    Outpost::PlanePosition previous = map.starts[0];
    for (const Outpost::PlanePosition waypoint : path)
    {
      Assert::IsTrue(pathfinder.IsStraightPathClear(previous, waypoint, clearance));
      previous = waypoint;
    }
  }
};
} // namespace GameLogicTests
