#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr Outpost::HullId MEDIUM{2};
constexpr Outpost::DriveId ION{1};
constexpr Outpost::WeaponId LANCE{2};

// A match on the repository's map with both bases placed, and a raid of Lance ships of _attacker's beside _defender's
// Command Station.
class RaidedMatch
{
public:
  RaidedMatch(Outpost::PlayerId _attacker, Outpost::PlayerId _defender)
    : m_map(Outpost::LoadMap(ReadRepositoryMap())),
      m_server(Outpost::LoadTuning(ReadRepositoryTuning()), m_map, {.seed = 5})
  {
    m_server.World().PlaceStartingBases(m_map);
    const Outpost::PlanePosition station = m_map.starts[_defender.value - 1];
    const Outpost::DesignId line = m_server.World().FindDesign(_attacker, {MEDIUM, ION, LANCE})->id;
    // In a ring 180 m out, inside the Lance's 220 m and clear of the station's footprint.
    for (int i = 0; i < 16; ++i)
    {
      const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 8.0f;
      (void)m_server.World().SpawnShip(_attacker, line,
                                       {station.xMeters + (180.0f * std::cos(angle)), station.zMeters + (180.0f * std::sin(angle))});
    }
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_server.World();
  }

  // Runs until the match is over or _seconds have passed.
  void RunUntilOver(std::uint32_t _seconds)
  {
    for (std::uint32_t tick = 0; tick < _seconds * 20 && !World().MatchOver(); ++tick)
      (void)World().Tick({});
  }

private:
  Outpost::Map m_map;
  Outpost::InProcessServer m_server;
};
} // namespace

TEST_CLASS(MatchOutcomeTests)
{
public:
  // Design §6, task 6.2: losing the Command Station loses the match, and the other player wins; every snapshot says so.
  TEST_METHOD(LosingTheCommandStationLosesTheMatch)
  {
    RaidedMatch match(RED, BLUE);
    Assert::IsFalse(match.World().MatchOver());
    Assert::IsFalse(match.World().BuildSnapshot(BLUE).matchOver);
    match.RunUntilOver(120);
    Assert::IsTrue(match.World().MatchOver(), L"the station survived two minutes of sixteen Lances");
    Assert::IsTrue(match.World().Winner() == RED);

    const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    const Outpost::Snapshot red = match.World().BuildSnapshot(RED);
    Assert::IsTrue(blue.matchOver && red.matchOver);
    Assert::IsTrue(blue.winner == RED && red.winner == RED);
    Assert::AreEqual(match.World().CurrentTick(), blue.matchEndedTick, L"it ended on the tick the station fell");
  }

  // Owner, 2026-10-01: the world runs on after the match ends, and the outcome stands.
  TEST_METHOD(TheWorldRunsOnAndTheOutcomeStands)
  {
    RaidedMatch match(BLUE, RED);
    match.RunUntilOver(120);
    Assert::IsTrue(match.World().Winner() == BLUE);
    const std::uint64_t ended = match.World().BuildSnapshot(BLUE).matchEndedTick;
    for (int tick = 0; tick < 100; ++tick)
      (void)match.World().Tick({});
    const Outpost::Snapshot later = match.World().BuildSnapshot(RED);
    Assert::IsTrue(later.tick > ended);
    Assert::IsTrue(later.matchOver && later.winner == BLUE);
    Assert::AreEqual(ended, later.matchEndedTick);
  }

  // A world without bases, as the movement and combat tests and the measurement loads run, never ends.
  TEST_METHOD(AWorldWithoutBasesNeverEnds)
  {
    Outpost::Simulation simulation(1, 20);
    simulation.PlaceMap({.sizeMeters = 2000.0f, .minimumGapMeters = 60.0f, .starts = {}, .oreAsteroids = {}, .asteroidFields = {}});
    for (int tick = 0; tick < 20; ++tick)
      (void)simulation.Tick({});
    Assert::IsFalse(simulation.MatchOver());
  }

  // Design §10: a player sees the design of every ship it sees as its components, so the AI can answer the player's
  // designs (ADR-024).
  TEST_METHOD(SnapshotsShowEveryShipsComponents)
  {
    RaidedMatch match(RED, BLUE);
    // Under fog of war what a player sees is settled at the end of each tick (ADR-024). The ring stands inside the
    // station's sight.
    (void)match.World().Tick({});
    const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    size_t seen = 0;
    for (const Outpost::EntityView& entity : blue.entities)
    {
      if (entity.kind != Outpost::EntityKind::Ship || entity.owner != RED || entity.role != Outpost::ShipRole::Warship)
        continue;
      Assert::IsTrue(entity.hull == MEDIUM && entity.drive == ION && entity.weapon == LANCE);
      ++seen;
    }
    Assert::AreEqual(size_t{16}, seen);
  }
};
} // namespace GameLogicTests
