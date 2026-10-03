#include "pch.h"
#include "RepositoryData.h"

#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
namespace
{
struct Census
{
  std::array<size_t, 2> ships{};
  std::array<size_t, 2> structures{};
};

Census Count(const Outpost::Simulation& _simulation)
{
  Census census;
  for (const Outpost::Entity& entity : _simulation.Entities())
  {
    if (!entity.owner.IsValid())
      continue;
    const size_t side = entity.owner.value - 1;
    if (entity.kind == Outpost::EntityKind::Ship && entity.role == Outpost::ShipRole::Warship)
      ++census.ships[side];
    else if (entity.kind == Outpost::EntityKind::Ship)
      continue;
    else if (entity.kind == Outpost::EntityKind::Structure)
      ++census.structures[side];
  }
  return census;
}
} // namespace

TEST_CLASS(StressLoadTests)
{
public:
  // Task 3.7: 200 armed ships and 40 structures that can be destroyed, fighting, and kept at full size as ships are lost.
  TEST_METHOD(KeepsTwoHundredShipsFighting)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Outpost::InProcessServer server(tuning, map, {.seed = 3, .stressLoad = true});
    server.World().PlaceStartingBases(map);
    server.StartStressLoad();
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(Outpost::PlayerId{1});

    Census census = Count(server.World());
    Assert::AreEqual(Outpost::STRESS_STRUCTURES_PER_PLAYER, census.structures[0]);
    Assert::AreEqual(Outpost::STRESS_STRUCTURES_PER_PLAYER, census.structures[1]);
    for (const Outpost::Entity& entity : server.World().Entities())
    {
      if (entity.owner.IsValid())
        Assert::IsTrue(entity.maxHitPointsHundredths > 0, L"everything in the scene can be destroyed");
    }
    for (const size_t ships : census.ships)
    {
      // Until the first tick, no warships: the load adds them as the tick starts.
      Assert::AreEqual(size_t{0}, ships);
    }

    size_t destroyed = 0;
    size_t shots = 0;
    std::vector<std::chrono::nanoseconds> ticks;
    for (int second = 0; second < 60; ++second)
    {
      // A second in five steps, since one Advance runs at most five ticks (ADR-009).
      for (int step = 0; step < 5; ++step)
        server.Advance(200ms);
      for (const Outpost::Snapshot& snapshot : blue->Receive())
      {
        destroyed += snapshot.destroyed.size();
        shots += snapshot.shots.size();
      }
      for (const Outpost::TickTiming& tick : server.TakeTickTimings())
        ticks.push_back(tick.total);
      census = Count(server.World());
      // Losses in the last tick are made good before the next.
      Assert::IsTrue(census.ships[0] > Outpost::STRESS_SHIPS_PER_PLAYER - 20 && census.ships[1] > Outpost::STRESS_SHIPS_PER_PLAYER - 20,
                     (std::to_wstring(census.ships[0]) + L" and " + std::to_wstring(census.ships[1]) + L" ships").c_str());
    }
    Assert::IsTrue(shots > 1000, std::to_wstring(shots).c_str());
    Assert::IsTrue(destroyed > 20, std::to_wstring(destroyed).c_str());

    std::ranges::sort(ticks);
    Logger::WriteMessage(
      std::format("{} ticks, {} shots, {} destroyed; tick median {:.3f} ms, 99th percentile {:.3f} ms, slowest {:.3f} ms", ticks.size(),
                  shots, destroyed, std::chrono::duration<double, std::milli>(ticks[ticks.size() / 2]).count(),
                  std::chrono::duration<double, std::milli>(ticks[ticks.size() * 99 / 100]).count(),
                  std::chrono::duration<double, std::milli>(ticks.back()).count())
        .c_str());
  }
};
} // namespace GameLogicTests