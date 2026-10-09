#include "pch.h"
#include "TerritoryMatch.h"

#include <algorithm>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr Outpost::PlayerId RED = TerritoryMatch::RED;
constexpr std::uint32_t TICKS_PER_SECOND = TerritoryMatch::TICKS_PER_SECOND;
// A world's hour, as the tests shorten it, and the sector next to Blue's home.
constexpr std::uint32_t RESTART_SECONDS = 10;
constexpr std::int32_t B1 = 2;

// Sixteen Lance ships of _attacker's in a ring 180 m round _center, inside the Lance's range and clear of a Command
// Station, fighting to the end.
std::vector<Outpost::EntityId> Raid(TerritoryMatch& _match, Outpost::PlayerId _attacker, Outpost::PlanePosition _center)
{
  const Outpost::DesignId lance = _match.World().FindDesign(_attacker, {Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{2}})->id;
  std::vector<Outpost::EntityId> ships;
  ships.reserve(16);
  for (int i = 0; i < 16; ++i)
  {
    const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 8.0f;
    ships.push_back(_match.World().SpawnShip(_attacker, lance,
                                             {_center.xMeters + (180.0f * std::cos(angle)), _center.zMeters + (180.0f * std::sin(angle))}));
  }
  _match.FightToTheEnd(_attacker, ships);
  return ships;
}

// Runs a tick at a time, at most _seconds, until _player's snapshot holds an event of _kind: the tick it came in, if it did.
std::optional<std::uint64_t> RunUntil(TerritoryMatch& _match, Outpost::PlayerId _player, Outpost::EventKind _kind, std::uint32_t _seconds)
{
  for (std::uint32_t tick = 0; tick < _seconds * TICKS_PER_SECOND; ++tick)
  {
    (void)_match.World().Tick({});
    const std::vector<Outpost::EventView> events = _match.World().BuildSnapshot(_player).events;
    if (std::ranges::find(events, _kind, &Outpost::EventView::kind) != events.end())
      return _match.World().CurrentTick();
  }
  return std::nullopt;
}

std::size_t Stations(TerritoryMatch& _match, Outpost::PlayerId _player)
{
  return static_cast<std::size_t>(std::ranges::count_if(_match.World().Entities(),
                                                        [_player](const Outpost::Entity& _entity)
                                                        {
                                                          return _entity.owner == _player &&
                                                                 _entity.kind == Outpost::EntityKind::Structure &&
                                                                 _entity.structure == Outpost::StructureKind::CommandStation;
                                                        }));
}

// Blue's base raided until Blue has lost, and the raiders sent home: the tick it lost on.
std::uint64_t LoseBlue(TerritoryMatch& _match)
{
  const std::vector<Outpost::EntityId> raiders = Raid(_match, RED, _match.Start(BLUE));
  const std::optional<std::uint64_t> lost = RunUntil(_match, BLUE, Outpost::EventKind::EmpireLost, 240);
  Assert::IsTrue(lost.has_value(), L"the raid takes Blue's base");
  (void)_match.World().Tick({{.player = RED, .order = Outpost::MoveCommand{.ships = raiders, .destination = _match.Start(RED)}}});
  return lost.value_or(0);
}

std::int64_t StartingOreHundredths(const TerritoryMatch& _match)
{
  return std::int64_t{_match.TuningData().rules.startingOre} * Outpost::HUNDREDTHS;
}
} // namespace

// Phase 5 design §8 (gate H6): a world has no domination and no end. A player who loses is told so, and its seat restarts
// at its start an hour later, once the start is free, with its research, its designs and what of its own still stands.
TEST_CLASS(WorldRulesTests)
{
public:
  // The loss is Blue's alone and ends nothing; on the hour Blue has its Command Station and the starting Constructors again,
  // and the Ore it spent on a Shipyard's site raised back to the starting Ore (owner, 2026-10-08).
  TEST_METHOD(ALostSeatRestartsOnTheHour)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    const std::vector<Outpost::EntityId> constructors = match.Constructors(BLUE);
    const Outpost::PlanePosition behind{match.Start(BLUE).xMeters - 500.0f, match.Start(BLUE).zMeters - 500.0f};
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Shipyard, behind) == Outpost::CommandResult::Applied);
    Assert::IsTrue(match.World().OreHundredths(BLUE) < StartingOreHundredths(match), L"the site is paid for");
    const std::size_t designs = match.World().BuildSnapshot(BLUE).designs.size();

    const std::uint64_t lost = LoseBlue(match);
    Assert::IsFalse(match.World().MatchOver(), L"no match ends in a world");
    Assert::IsTrue(match.World().BuildSnapshot(RED).events.empty() ||
                     std::ranges::none_of(match.World().BuildSnapshot(RED).events,
                                          [](const Outpost::EventView& _event) { return _event.kind == Outpost::EventKind::EmpireLost; }),
                   L"Red is told nothing of Blue's loss");
    const Outpost::Snapshot fallen = match.World().BuildSnapshot(BLUE);
    Assert::AreEqual(lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND), fallen.restartTick.value_or(0), L"an hour on");
    Assert::IsTrue(fallen.tickets.empty(), L"no tickets: domination is off");
    Assert::IsFalse(match.World().BuildSnapshot(RED).restartTick.has_value());

    while (match.World().CurrentTick() + 1 < fallen.restartTick.value_or(0))
      (void)match.World().Tick({});
    Assert::AreEqual(std::size_t{0}, Stations(match, BLUE), L"not before the hour");
    (void)match.World().Tick({});
    const Outpost::Snapshot back = match.World().BuildSnapshot(BLUE);
    Assert::IsTrue(
      std::ranges::any_of(back.events, [](const Outpost::EventView& _event) { return _event.kind == Outpost::EventKind::EmpireRestarted; }),
      L"on the hour");
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE));
    const auto station =
      std::ranges::find_if(match.World().Entities(), [](const Outpost::Entity& _entity)
                           { return _entity.owner == BLUE && _entity.structure == Outpost::StructureKind::CommandStation; });
    Assert::AreEqual(0.0f, Outpost::Distance(station->position, match.Start(BLUE)), L"at its start");
    Assert::IsTrue(match.Constructors(BLUE).size() >=
                     static_cast<std::size_t>(match.TuningData().rules.startingConstructors) +
                       std::ranges::count_if(constructors, [&](Outpost::EntityId _id) { return match.World().FindEntity(_id) != nullptr; }),
                   L"the starting Constructors, beside those that still stand");
    Assert::AreEqual(StartingOreHundredths(match), match.World().OreHundredths(BLUE), L"its Ore raised to the starting Ore");
    Assert::AreEqual(designs, back.designs.size(), L"its designs kept");
    Assert::IsFalse(back.restartTick.has_value(), L"and it stands again");
  }

  // A bank larger than the starting Ore is kept, as its research is (owner, 2026-10-08): Blue's rig in a sector it still
  // holds earns all the while it waits.
  TEST_METHOD(ARestartedSeatKeepsItsBank)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    (void)match.Relay(BLUE, B1);
    (void)match.Structure(BLUE, Outpost::StructureKind::MiningRig, match.AsteroidIn(B1));
    match.Run(120 * TICKS_PER_SECOND);
    const std::uint64_t lost = LoseBlue(match);
    while (match.World().CurrentTick() + 1 < lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND))
      (void)match.World().Tick({});
    const std::int64_t bank = match.World().OreHundredths(BLUE);
    Assert::IsTrue(bank > StartingOreHundredths(match), L"Blue has earned more than it started with");
    (void)match.World().Tick({});
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE));
    Assert::IsTrue(match.World().OreHundredths(BLUE) >= bank, L"and keeps it");
  }

  // A start is free while no other player has a structure in its sector or holds it: Blue waits past the hour while Red's
  // Shipyard stands in Blue's home sector, and restarts in the tick Blue's ships have destroyed it.
  TEST_METHOD(WaitsWhileItsStartIsTaken)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    const std::uint64_t lost = LoseBlue(match);
    const Outpost::PlanePosition start = match.Start(BLUE);
    const Outpost::EntityId yard = match.Structure(RED, Outpost::StructureKind::Shipyard, {start.xMeters + 250.0f, start.zMeters + 250.0f});
    match.Run((RESTART_SECONDS + 5) * TICKS_PER_SECOND);
    Assert::AreEqual(std::size_t{0}, Stations(match, BLUE), L"its start is taken");
    const Outpost::Snapshot waiting = match.World().BuildSnapshot(BLUE);
    Assert::IsTrue(waiting.restartTick.value_or(0) < waiting.tick, L"and it waits past the hour");
    Assert::AreEqual(lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND), waiting.restartTick.value_or(0));

    (void)Raid(match, BLUE, match.World().FindEntity(yard)->position);
    const std::optional<std::uint64_t> restarted = RunUntil(match, BLUE, Outpost::EventKind::EmpireRestarted, 240);
    Assert::IsTrue(restarted.has_value(), L"once its start is clear");
    Assert::IsNull(match.World().FindEntity(yard), L"cleared by Blue's ships");
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE));
  }

  // A save keeps a lost seat waiting (ADR-077): a world saved while Blue waits, and loaded, restarts it in the same tick as
  // the world that never stopped, and is that world after.
  TEST_METHOD(ASavedLostSeatRestartsOnTheSameTick)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    (void)LoseBlue(match);
    const Outpost::WorldIdentity identity{.seed = 3, .ticksPerSecond = TICKS_PER_SECOND, .dataHash = 1};
    const std::vector<std::byte> saved = Outpost::EncodeWorld(match.World(), identity);
    TerritoryMatch loaded(false, RESTART_SECONDS);
    (void)Outpost::DecodeWorld(saved, identity, loaded.World());
    Assert::IsTrue(loaded.World() == match.World());
    const std::optional<std::uint64_t> ran = RunUntil(match, BLUE, Outpost::EventKind::EmpireRestarted, RESTART_SECONDS + 5);
    const std::optional<std::uint64_t> again = RunUntil(loaded, BLUE, Outpost::EventKind::EmpireRestarted, RESTART_SECONDS + 5);
    Assert::IsTrue(ran.has_value() && ran == again, L"restarted on the same tick");
    Assert::IsTrue(loaded.World() == match.World());
  }

  // A match is not a world: it still ends when a player loses, and keeps its domination's tickets.
  TEST_METHOD(AMatchStillEnds)
  {
    TerritoryMatch match;
    Assert::IsFalse(match.World().BuildSnapshot(BLUE).tickets.empty());
    (void)Raid(match, RED, match.Start(BLUE));
    for (std::uint32_t tick = 0; tick < 240 * TICKS_PER_SECOND && !match.World().MatchOver(); ++tick)
      (void)match.World().Tick({});
    Assert::IsTrue(match.World().MatchOver());
    Assert::IsTrue(match.World().Winner() == RED);
    Assert::IsFalse(match.World().BuildSnapshot(BLUE).restartTick.has_value());
  }
};
} // namespace GameLogicTests
