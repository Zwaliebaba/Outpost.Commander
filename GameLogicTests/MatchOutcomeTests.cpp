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
constexpr std::int32_t SHIPYARD_HIT_POINTS = 2500;
constexpr float SHIPYARD_RADIUS_METERS = 40.0f;

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
    Raid(_attacker, m_map.starts[_defender.value - 1]);
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_server.World();
  }

  // Sixteen Lance ships of _attacker's in a ring 180 m round _center: inside the Lance's 220 m, and clear of a Command
  // Station's footprint.
  void Raid(Outpost::PlayerId _attacker, Outpost::PlanePosition _center)
  {
    const Outpost::DesignId line = World().FindDesign(_attacker, {MEDIUM, ION, LANCE})->id;
    for (int i = 0; i < 16; ++i)
    {
      const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 8.0f;
      (void)World().SpawnShip(_attacker, line,
                              {_center.xMeters + (180.0f * std::cos(angle)), _center.zMeters + (180.0f * std::sin(angle))});
    }
  }

  // A place _meters behind _player's start, away from the map's center, out of the raid's sight.
  [[nodiscard]] Outpost::PlanePosition Behind(Outpost::PlayerId _player, float _meters) const
  {
    const Outpost::PlanePosition start = m_map.starts[_player.value - 1];
    const float fromCenter = std::hypot(start.xMeters, start.zMeters);
    return {start.xMeters + (_meters * start.xMeters / fromCenter), start.zMeters + (_meters * start.zMeters / fromCenter)};
  }

  [[nodiscard]] bool HasStation(Outpost::PlayerId _player)
  {
    const Outpost::Snapshot view = World().BuildSnapshot(_player);
    return std::ranges::any_of(view.entities,
                               [_player](const Outpost::EntityView& _entity)
                               {
                                 return _entity.kind == Outpost::EntityKind::Structure && _entity.owner == _player &&
                                        _entity.structure == Outpost::StructureKind::CommandStation;
                               });
  }

  [[nodiscard]] std::vector<Outpost::EntityId> Constructors(Outpost::PlayerId _player)
  {
    std::vector<Outpost::EntityId> constructors;
    for (const Outpost::EntityView& entity : World().BuildSnapshot(_player).entities)
    {
      if (entity.owner == _player && entity.kind == Outpost::EntityKind::Ship && entity.role == Outpost::ShipRole::Constructor)
        constructors.push_back(entity.id);
    }
    return constructors;
  }

  // Runs until the match is over or _seconds have passed.
  void RunUntilOver(std::uint32_t _seconds)
  {
    for (std::uint32_t tick = 0; tick < _seconds * 20 && !World().MatchOver(); ++tick)
      (void)World().Tick({});
  }

  // Runs until _player's Command Station has fallen or _seconds have passed.
  void RunUntilStationFalls(Outpost::PlayerId _player, std::uint32_t _seconds)
  {
    for (std::uint32_t tick = 0; tick < _seconds * 20 && HasStation(_player); ++tick)
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
  // Phase 1 design §4: a player with no finished Shipyard that loses its Command Station loses the match, and the other
  // player wins; every snapshot says so.
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
    Assert::IsTrue(blue.ending == Outpost::MatchEnding::LostProduction);
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

  // Phase 1 design §4: a finished Shipyard keeps a player in the match once its Command Station has fallen, and is shown
  // to the opponent through fog of war from then on; losing it as well loses the match.
  TEST_METHOD(TheLastShipyardKeepsAPlayerIn)
  {
    RaidedMatch match(RED, BLUE);
    const Outpost::EntityId yard = match.World().SpawnStructure(BLUE, Outpost::StructureKind::Shipyard, match.Behind(BLUE, 600.0f),
                                                                SHIPYARD_RADIUS_METERS, SHIPYARD_HIT_POINTS * Outpost::HUNDREDTHS);
    const auto shown = [&match, yard]
    {
      const Outpost::Snapshot red = match.World().BuildSnapshot(RED);
      const auto found = std::ranges::find(red.entities, yard, &Outpost::EntityView::id);
      return found != red.entities.end() ? std::optional<bool>(found->remembered) : std::nullopt;
    };
    (void)match.World().Tick({});
    Assert::IsFalse(shown().has_value(), L"Red sees the Shipyard while Blue still has its Command Station");

    match.RunUntilStationFalls(BLUE, 120);
    Assert::IsFalse(match.HasStation(BLUE), L"the station survived two minutes of sixteen Lances");
    Assert::IsFalse(match.World().MatchOver(), L"the Shipyard keeps Blue in the match");
    (void)match.World().Tick({});
    const std::optional<bool> remembered = shown();
    Assert::IsTrue(remembered.has_value(), L"Blue's last Shipyard is not shown to Red");
    Assert::IsTrue(remembered.value_or(false), L"it is shown as a remembered structure, out of Red's sight");

    match.Raid(RED, match.Behind(BLUE, 600.0f));
    match.RunUntilOver(120);
    Assert::IsTrue(match.World().MatchOver(), L"the Shipyard survived two minutes of sixteen Lances");
    Assert::IsTrue(match.World().Winner() == RED);
  }

  // Phase 1 design §4: a Shipyard still under construction keeps no one in the match.
  TEST_METHOD(AShipyardSiteDoesNotCount)
  {
    RaidedMatch match(RED, BLUE);
    const std::vector<Outpost::EntityId> constructors = match.Constructors(BLUE);
    const Outpost::BuildStructureCommand build{
      .constructors = constructors, .structure = Outpost::StructureKind::Shipyard, .position = match.Behind(BLUE, 600.0f)};
    Assert::IsTrue(match.World().Tick({{.player = BLUE, .order = build}}).front() == Outpost::CommandResult::Applied);
    (void)match.World().Tick({{.player = BLUE, .order = Outpost::StopCommand{.ships = constructors}}});

    match.RunUntilOver(120);
    Assert::IsTrue(match.World().MatchOver(), L"the site kept Blue in the match");
    Assert::IsTrue(match.World().Winner() == RED);
    const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    Assert::IsTrue(std::ranges::any_of(blue.entities,
                                       [](const Outpost::EntityView& _entity) {
                                         return _entity.owner == BLUE && _entity.structure == Outpost::StructureKind::Shipyard &&
                                                _entity.builtPermille < Outpost::PERMILLE;
                                       }),
                   L"the site still stands, unfinished");
  }

  // Gate H5: a lost Command Station is lost for good, as no Constructor can build one.
  TEST_METHOD(ALostCommandStationIsLostForGood)
  {
    RaidedMatch match(RED, BLUE);
    const std::vector<Outpost::EntityId> constructors = match.Constructors(BLUE);
    const Outpost::BuildStructureCommand build{
      .constructors = constructors, .structure = Outpost::StructureKind::CommandStation, .position = match.Behind(BLUE, 600.0f)};
    Assert::IsTrue(match.World().Tick({{.player = BLUE, .order = build}}).front() == Outpost::CommandResult::NotBuildable);
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
