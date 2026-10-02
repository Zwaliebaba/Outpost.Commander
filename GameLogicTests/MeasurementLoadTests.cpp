#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
TEST_CLASS(MeasurementLoadTests)
{
public:
  // Task 2.7: 200 ships and 40 structures, the starting bases counted, each clear of every obstacle and of each other.
  TEST_METHOD(PlacesTwoHundredShipsAndFortyStructures)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    server.World().PlaceStartingBases(map);
    Outpost::PlaceMeasurementLoad(server.World(), map, tuning);

    std::vector<Outpost::Entity> placed;
    size_t ships = 0;
    size_t structures = 0;
    for (const Outpost::Entity& entity : server.World().Entities())
    {
      if (entity.kind == Outpost::EntityKind::Ship)
        ++ships;
      else if (entity.kind == Outpost::EntityKind::Structure)
        ++structures;
      else
        continue;
      placed.push_back(entity);
    }
    Assert::AreEqual(Outpost::MEASUREMENT_SHIPS, ships);
    Assert::AreEqual(Outpost::MEASUREMENT_STRUCTURES, structures);

    for (size_t i = 0; i < placed.size(); ++i)
    {
      for (const Outpost::OreAsteroidPlacement& asteroid : map.oreAsteroids)
        Assert::IsTrue(Outpost::Distance(placed[i].position, asteroid.position) >= asteroid.radiusMeters + placed[i].radiusMeters);
      for (const Outpost::AsteroidFieldPlacement& field : map.asteroidFields)
        Assert::IsTrue(Outpost::Distance(placed[i].position, field.position) >= field.radiusMeters + placed[i].radiusMeters);
      for (size_t j = 0; j < i; ++j)
        Assert::IsTrue(Outpost::Distance(placed[i].position, placed[j].position) >= placed[i].radiusMeters + placed[j].radiusMeters);
    }
  }

  TEST_METHOD(ReportsOneDurationPerTick)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(Outpost::PlayerId{1});
    server.Advance(150ms);
    const std::vector<Outpost::TickTiming> timings = server.TakeTickTimings();
    Assert::AreEqual(size_t{3}, timings.size());
    for (const Outpost::TickTiming& timing : timings)
    {
      Assert::IsTrue(timing.total.count() > 0);
      // Every tick builds a snapshot for its one player, inside the tick.
      Assert::IsTrue(timing.Part(Outpost::TickPart::Snapshots).count() > 0);
      Assert::IsTrue(timing.Part(Outpost::TickPart::Snapshots) <= timing.total);
    }
    // Taken once.
    Assert::IsTrue(server.TakeTickTimings().empty());
  }

  // Task 8.1: an order tick on the measurement load names where its time went: the ships' paths, inside its commands,
  // inside the tick.
  TEST_METHOD(TimesTheOrdersOfATick)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    server.World().PlaceStartingBases(map);
    Outpost::PlaceMeasurementLoad(server.World(), map, tuning);
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(Outpost::PlayerId{1});
    server.Advance(50ms);
    (void)server.TakeTickTimings();

    Outpost::MoveCommand move{.destination = {.xMeters = 600.0f, .zMeters = 600.0f}};
    for (const Outpost::Entity& entity : server.World().Entities())
    {
      if (entity.kind == Outpost::EntityKind::Ship && entity.owner == Outpost::PlayerId{1})
        move.ships.push_back(entity.id);
    }
    Assert::IsTrue(move.ships.size() > 1);
    blue->Send({.order = move});
    server.Advance(50ms);
    const std::vector<Outpost::TickTiming> timings = server.TakeTickTimings();
    Assert::AreEqual(size_t{1}, timings.size());
    const Outpost::TickTiming& order = timings.front();
    Assert::IsTrue(order.Part(Outpost::TickPart::ShipPaths).count() > 0);
    Assert::IsTrue(order.Part(Outpost::TickPart::GroupRoute) + order.Part(Outpost::TickPart::ShipPaths) <=
                   order.Part(Outpost::TickPart::Commands));
    Assert::IsTrue(order.Part(Outpost::TickPart::Commands) <= order.total);
  }
};
} // namespace GameLogicTests