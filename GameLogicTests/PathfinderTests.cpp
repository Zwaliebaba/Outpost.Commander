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

  // ADR-010: ships of a group join the route the group searched from its center, rather than each searching its own.
  TEST_METHOD(ShipsOfAGroupJoinItsRoute)
  {
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({{.center = {}, .radiusMeters = 100.0f}}, 1000.0f);
    const Outpost::PlanePosition center{-300.0f, 0.0f};
    std::vector<Outpost::PlanePosition> route = pathfinder.FindPath(center, {300.0f, 0.0f}, SHIP_RADIUS_METERS);
    route.pop_back();

    Outpost::GroupRoutes routes(pathfinder, {300.0f, 0.0f}, SHIP_RADIUS_METERS);
    routes.SearchFrom(center);
    for (const Outpost::PlanePosition start : {Outpost::PlanePosition{-320.0f, 20.0f}, Outpost::PlanePosition{-280.0f, -20.0f}})
    {
      const Outpost::PlanePosition slot{300.0f, 30.0f};
      const std::vector<Outpost::PlanePosition> path = routes.PathFor(start, slot, SHIP_RADIUS_METERS);
      Assert::IsTrue(path.back() == slot);
      ExpectClear(pathfinder, start, path);
      // Every waypoint but the slot is a corner of the group's route.
      for (size_t i = 0; i + 1 < path.size(); ++i)
        Assert::IsTrue(std::ranges::find(route, path[i]) != route.end());
    }
  }

  // ADR-010: a ship with no route to join searches for itself, and a ship beside it then joins its path.
  TEST_METHOD(AShipWithNoRouteToJoinSearchesAndLeavesOne)
  {
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({{.center = {}, .radiusMeters = 100.0f}}, 1000.0f);
    // The group's center sees its destination, so the group's route has no corners.
    Outpost::GroupRoutes routes(pathfinder, {400.0f, 0.0f}, SHIP_RADIUS_METERS);
    routes.SearchFrom({0.0f, 400.0f});

    // Both ships are on the far side of the obstacle from their slots.
    const Outpost::PlanePosition first{-400.0f, 0.0f};
    const Outpost::PlanePosition second{-410.0f, 10.0f};
    const std::vector<Outpost::PlanePosition> firstPath = routes.PathFor(first, {400.0f, 0.0f}, SHIP_RADIUS_METERS);
    const std::vector<Outpost::PlanePosition> secondPath = routes.PathFor(second, {400.0f, 20.0f}, SHIP_RADIUS_METERS);
    ExpectClear(pathfinder, first, firstPath);
    ExpectClear(pathfinder, second, secondPath);
    Assert::IsTrue(firstPath.size() > 1 && secondPath.size() > 1);
    Assert::IsTrue(secondPath.back() == Outpost::PlanePosition{400.0f, 20.0f});
    Assert::IsTrue(std::ranges::find(firstPath, secondPath.front()) != firstPath.end(),
                   L"the second ship did not join the first one's path");
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