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

    std::vector<Outpost::EntityView> placed;
    size_t ships = 0;
    size_t structures = 0;
    for (const Outpost::EntityView& entity : server.World().BuildSnapshot(Outpost::PlayerId{1}).entities)
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
    const std::vector<std::chrono::nanoseconds> durations = server.TakeTickDurations();
    Assert::AreEqual(size_t{3}, durations.size());
    for (const std::chrono::nanoseconds duration : durations)
      Assert::IsTrue(duration.count() > 0);
    // Taken once.
    Assert::IsTrue(server.TakeTickDurations().empty());
  }
};
} // namespace GameLogicTests