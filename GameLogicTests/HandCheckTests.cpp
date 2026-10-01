#include "pch.h"
#include "MatchArena.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = MatchArena::BLUE;
constexpr Outpost::PlayerId RED = MatchArena::RED;
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::WeaponId MASS_DRIVER{1};
// The raid gathers out of every gun's reach and closes on its target, as a raid would.
constexpr float RAID_START_METERS = 400.0f;
constexpr float RAID_SPACING_METERS = 25.0f;
constexpr std::uint32_t LIMIT_SECONDS = 180;

struct Outcome
{
  // Seconds until the structure fell, or until the last raider did; the other is zero.
  double structureFellSeconds = 0.0;
  double raidFellSeconds = 0.0;
  std::int32_t structureHitPointsLeft = 0;
  size_t raidersLeft = 0;
};

// _raiders Small+Ion+Mass Driver ships attack the structure from RAID_START_METERS until one side is gone.
Outcome Raid(MatchArena& _arena, Outpost::EntityId _structure, size_t _raiders)
{
  std::vector<Outpost::EntityId> raid;
  raid.reserve(_raiders);
  for (size_t i = 0; i < _raiders; ++i)
  {
    const float across = (static_cast<float>(i) - (static_cast<float>(_raiders - 1) / 2.0f)) * RAID_SPACING_METERS;
    raid.push_back(_arena.Ship(RED, SMALL, MASS_DRIVER, {RAID_START_METERS, across}));
  }
  (void)_arena.Tick({Order(RED, Outpost::AttackCommand{.ships = raid, .target = _structure})});

  Outcome outcome;
  for (std::uint32_t tick = 1; tick <= LIMIT_SECONDS * MatchArena::TICKS_PER_SECOND; ++tick)
  {
    _arena.Run(1);
    const auto alive = static_cast<size_t>(
      std::ranges::count_if(raid, [&_arena](Outpost::EntityId _id) { return _arena.World().FindEntity(_id) != nullptr; }));
    const double seconds = static_cast<double>(tick) / MatchArena::TICKS_PER_SECOND;
    if (_arena.World().FindEntity(_structure) == nullptr)
    {
      outcome.structureFellSeconds = seconds;
      outcome.raidersLeft = alive;
      return outcome;
    }
    if (alive == 0)
    {
      outcome.raidFellSeconds = seconds;
      outcome.structureHitPointsLeft = _arena.Get(_structure).hitPointsHundredths / Outpost::HUNDREDTHS;
      return outcome;
    }
  }
  return outcome;
}

void Report(std::string_view _scenario, const Outcome& _outcome)
{
  Logger::WriteMessage(std::format("{}: structure fell at {:.1f} s with {} raiders left; raid fell at {:.1f} s with {} hit points left\n",
                                   _scenario, _outcome.structureFellSeconds, _outcome.raidersLeft, _outcome.raidFellSeconds,
                                   _outcome.structureHitPointsLeft)
                         .c_str());
}
} // namespace

// Task 4.6: design §6's hand estimates of the structure numbers, played by the simulation. Their figures are recorded in
// design §6 and §12; a number that misses its intent is raised with the owner, not retuned silently.
TEST_CLASS(HandCheckTests)
{
public:
  // "A lone platform outlasts a raid of five Small+Ion+Mass Driver ships and destroys it."
  TEST_METHOD(ALonePlatformBeatsFiveRaiders)
  {
    MatchArena arena;
    const Outpost::EntityId platform = arena.Structure(BLUE, Outpost::StructureKind::DefensePlatform, {0.0f, 0.0f});
    const Outcome outcome = Raid(arena, platform, 5);
    Report("Defence Platform against five", outcome);
    Assert::IsTrue(outcome.raidFellSeconds > 0.0, L"the raid destroyed the platform");
  }

  // "Armed and armoured, it needs about 70 s against them and its gun destroys all seven in under a minute."
  TEST_METHOD(TheArmedStationBeatsSevenRaiders)
  {
    MatchArena arena;
    const Outpost::EntityId station = arena.Structure(BLUE, Outpost::StructureKind::CommandStation, {0.0f, 0.0f});
    const Outcome outcome = Raid(arena, station, 7);
    Report("Armed Command Station against seven", outcome);
    Assert::IsTrue(outcome.raidFellSeconds > 0.0 && outcome.raidFellSeconds < 60.0, L"the gun did not destroy the raid within a minute");
  }

  // "Unarmed and unarmoured, it would fall to seven Small+Ion+Mass Driver ships in about 20 s": the reason the station
  // is armed. A stand-in with the station's hit points and no gun or armor.
  TEST_METHOD(AnUnarmedStationFallsToSevenRaiders)
  {
    MatchArena arena;
    const Outpost::StructureTuning& tuning = arena.StructureData(Outpost::StructureKind::CommandStation);
    const Outpost::EntityId station =
      arena.World().SpawnStructure(BLUE, Outpost::StructureKind::Shipyard, {0.0f, 0.0f}, static_cast<float>(tuning.footprintRadiusMeters),
                                   tuning.hitPoints * Outpost::HUNDREDTHS, 0);
    const Outcome outcome = Raid(arena, station, 7);
    Report("Unarmed Command Station against seven", outcome);
    Assert::IsTrue(outcome.structureFellSeconds > 0.0, L"the raid did not destroy the unarmed station");
  }
};
} // namespace GameLogicTests
