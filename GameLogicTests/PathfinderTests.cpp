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
      const std::vector<Outpost::PlanePosition> path = routes.PathFor(start, slot, SHIP_RADIUS_METERS).waypoints;
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
    const std::vector<Outpost::PlanePosition> firstPath = routes.PathFor(first, {400.0f, 0.0f}, SHIP_RADIUS_METERS).waypoints;
    const std::vector<Outpost::PlanePosition> secondPath = routes.PathFor(second, {400.0f, 20.0f}, SHIP_RADIUS_METERS).waypoints;
    ExpectClear(pathfinder, first, firstPath);
    ExpectClear(pathfinder, second, secondPath);
    Assert::IsTrue(firstPath.size() > 1 && secondPath.size() > 1);
    Assert::IsTrue(secondPath.back() == Outpost::PlanePosition{400.0f, 20.0f});
    Assert::IsTrue(std::ranges::find(firstPath, secondPath.front()) != firstPath.end(),
                   L"the second ship did not join the first one's path");
  }

  // ADR-047: ships of a group with a band keep their own lanes round the obstacle, side by side, rather than each passing
  // the route's corner. The band moves off the obstacle, so the innermost lane passes the corner and the rest pass outside.
  TEST_METHOD(ShipsOfAGroupKeepTheirLanesRoundAnObstacle)
  {
    constexpr float LANE_SPACING_METERS = 30.0f;
    Outpost::Pathfinder pathfinder;
    pathfinder.SetObstacles({{.center = {}, .radiusMeters = 100.0f}}, 1000.0f);
    const Outpost::PlanePosition center{-300.0f, 0.0f};
    std::vector<Outpost::PlanePosition> route = pathfinder.FindPath(center, {300.0f, 0.0f}, SHIP_RADIUS_METERS);
    route.pop_back();

    Outpost::GroupRoutes routes(pathfinder, {300.0f, 0.0f}, SHIP_RADIUS_METERS, LANE_SPACING_METERS);
    routes.SearchFrom(center);
    std::vector<Outpost::PlanePosition> laneEnds;
    for (const float laneMeters : {-LANE_SPACING_METERS, 0.0f, LANE_SPACING_METERS})
    {
      // The group travels along +x, so a lane to the left is one towards +z.
      const Outpost::PlanePosition start{-300.0f, laneMeters};
      const Outpost::PlanePosition slot{300.0f, laneMeters};
      const Outpost::GroupRoutes::Way way = routes.PathFor(start, slot, SHIP_RADIUS_METERS, laneMeters);
      Assert::IsTrue(way.waypoints.back() == slot);
      ExpectClear(pathfinder, start, way.waypoints);
      Assert::IsTrue(way.laneEnd.has_value(), L"the ship keeps no lane");
      const Outpost::PlanePosition laneEnd = way.laneEnd.value_or(Outpost::PlanePosition{});
      Assert::IsTrue(std::ranges::find(way.waypoints, laneEnd) != way.waypoints.end());
      laneEnds.push_back(laneEnd);
    }

    // Past the route's last corner, the lanes are a lane apart, the innermost at the corner itself.
    for (std::size_t lane = 0; lane + 1 < laneEnds.size(); ++lane)
      Assert::IsTrue(Outpost::Distance(laneEnds[lane], laneEnds[lane + 1]) >= LANE_SPACING_METERS - 0.01f);
    const float cornerMeters = Outpost::Distance(route.back(), {});
    float innermostMeters = std::numeric_limits<float>::infinity();
    for (const Outpost::PlanePosition laneEnd : laneEnds)
      innermostMeters = std::min(innermostMeters, Outpost::Distance(laneEnd, {}));
    Assert::AreEqual(cornerMeters, innermostMeters, 0.01f);
  }

  // ADR-054: a short line's test tests only the obstacles its grid finds near the line, and answers exactly as a test of
  // every obstacle does: over obstacles of every size, apart, touching and overlapping, for lines anywhere, short and
  // long, grazing obstacles at their grown edge, and points, at every clearance.
  TEST_METHOD(TheGridAnswersAsATestOfEveryObstacleDoes)
  {
    Neuron::Random random(54);
    const auto meters = [&random](float _from, float _to) { return _from + (static_cast<float>(random.NextUnit()) * (_to - _from)); };
    constexpr std::array<float, 6> CLEARANCES{0.0f, 8.0f, 10.0f, 14.0f, 24.0f, 29.0f};
    std::size_t blocked = 0;
    std::size_t clear = 0;
    for (int layout = 0; layout < 40; ++layout)
    {
      std::vector<Outpost::Obstacle> obstacles;
      const std::uint32_t count = 1 + random.NextBelow(160);
      obstacles.reserve(count);
      for (std::uint32_t i = 0; i < count; ++i)
        obstacles.push_back({.center = {meters(-2500.0f, 2500.0f), meters(-2500.0f, 2500.0f)}, .radiusMeters = meters(0.0f, 250.0f)});
      Outpost::Pathfinder pathfinder;
      pathfinder.SetObstacles(obstacles, 2500.0f);
      for (int line = 0; line < 600; ++line)
      {
        const float clearance =
          random.NextBelow(4) == 0 ? meters(0.0f, 60.0f) : CLEARANCES[random.NextBelow(static_cast<std::uint32_t>(CLEARANCES.size()))];
        Outpost::PlanePosition a{meters(-2700.0f, 2700.0f), meters(-2700.0f, 2700.0f)};
        Outpost::PlanePosition b{meters(-2700.0f, 2700.0f), meters(-2700.0f, 2700.0f)};
        switch (random.NextBelow(4))
        {
        case 0:
          // A point.
          b = a;
          break;
        case 1:
        {
          // A line that passes an obstacle at its grown edge, just inside or just outside it.
          const Outpost::Obstacle& obstacle = obstacles[random.NextBelow(count)];
          const float angle = meters(0.0f, 2.0f * std::numbers::pi_v<float>);
          const Outpost::PlaneVector across{std::cos(angle), std::sin(angle)};
          const Outpost::PlaneVector along = Outpost::Perpendicular(across);
          const float edge = obstacle.radiusMeters + clearance + meters(0.48f, 0.5f);
          const Outpost::PlanePosition passing = obstacle.center + across * edge;
          const float halfMeters = random.NextBelow(2) == 0 ? 150.0f : 3000.0f;
          a = passing + along * meters(0.0f, halfMeters);
          b = passing + along * -meters(0.0f, halfMeters);
          break;
        }
        case 2:
          // A short line.
          b = a + Outpost::PlaneVector{meters(-300.0f, 300.0f), meters(-300.0f, 300.0f)};
          break;
        default:
          break;
        }
        const bool expected = pathfinder.IsStraightPathClearOfEveryObstacle(a, b, clearance);
        Assert::AreEqual(expected, pathfinder.IsStraightPathClear(a, b, clearance));
        Assert::AreEqual(pathfinder.IsStraightPathClearOfEveryObstacle(b, a, clearance), pathfinder.IsStraightPathClear(b, a, clearance));
        if (expected)
          ++clear;
        else
          ++blocked;
      }
    }
    // Both answers are common, so neither was the only one tested.
    Assert::IsTrue(blocked > 2000 && clear > 2000, (std::to_wstring(blocked) + L" blocked, " + std::to_wstring(clear) + L" clear").c_str());
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