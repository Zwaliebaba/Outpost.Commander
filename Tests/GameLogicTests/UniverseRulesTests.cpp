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
// A universe's hour, as the tests shorten it: long enough for a raid's ships, sent home, to leave Blue's home sector, whose
// edge is a kilometer from its start. The sector next to Blue's home, and Blue's home.
constexpr std::uint32_t RESTART_SECONDS = 45;
constexpr std::int32_t B1 = 2;
constexpr std::int32_t A1 = 1;

// Sixteen Lance ships of _attacker's in a ring 180 m round _center, inside the Lance's range and clear of a Command
// Station, fighting to the end.
std::vector<Outpost::EntityId> Raid(TerritoryMatch& _match, Outpost::PlayerId _attacker, Outpost::PlanePosition _center)
{
  const Outpost::DesignId lance =
    _match.SimulationState().FindDesign(_attacker, {Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{2}})->id;
  std::vector<Outpost::EntityId> ships;
  ships.reserve(16);
  for (int i = 0; i < 16; ++i)
  {
    const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 8.0f;
    ships.push_back(_match.SimulationState().SpawnShip(
      _attacker, lance, {_center.xMeters + (180.0f * std::cos(angle)), _center.zMeters + (180.0f * std::sin(angle))}));
  }
  _match.FightToTheEnd(_attacker, ships);
  return ships;
}

// Runs a tick at a time, at most _seconds, until _player's snapshot holds an event of _kind: the tick it came in, if it did.
std::optional<std::uint64_t> RunUntil(TerritoryMatch& _match, Outpost::PlayerId _player, Outpost::EventKind _kind, std::uint32_t _seconds)
{
  for (std::uint32_t tick = 0; tick < _seconds * TICKS_PER_SECOND; ++tick)
  {
    (void)_match.SimulationState().Tick({});
    const std::vector<Outpost::EventView> events = _match.SimulationState().BuildSnapshot(_player).events;
    if (std::ranges::find(events, _kind, &Outpost::EventView::kind) != events.end())
      return _match.SimulationState().CurrentTick();
  }
  return std::nullopt;
}

std::size_t Stations(TerritoryMatch& _match, Outpost::PlayerId _player)
{
  return static_cast<std::size_t>(std::ranges::count_if(_match.SimulationState().Entities(),
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
  (void)_match.SimulationState().Tick({{.player = RED, .order = Outpost::MoveCommand{.ships = raiders, .destination = _match.Start(RED)}}});
  return lost.value_or(0);
}

// Runs past the hour Blue lost on, and a little more: Blue waits, since its start is not free.
void RunPastTheHour(TerritoryMatch& _match, std::uint64_t _lost)
{
  while (_match.SimulationState().CurrentTick() < _lost + ((std::uint64_t{RESTART_SECONDS} + 5) * TICKS_PER_SECOND))
    (void)_match.SimulationState().Tick({});
  const Outpost::Snapshot waiting = _match.SimulationState().BuildSnapshot(BLUE);
  Assert::AreEqual(std::size_t{0}, Stations(_match, BLUE), L"its start is held");
  Assert::IsTrue(waiting.restartTick.value_or(0) < waiting.tick, L"and it waits past the hour");
}

std::int64_t StartingOreHundredths(const TerritoryMatch& _match)
{
  return std::int64_t{_match.TuningData().rules.startingOre} * Outpost::HUNDREDTHS;
}
} // namespace

// ADR-084 decisions 2 to 4: a universe has no domination and no end. A player who loses is told so, and its seat restarts
// at its start an hour later, once the start is free, with its research, its designs and what of its own still stands.
TEST_CLASS(UniverseRulesTests)
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
    Assert::IsTrue(match.SimulationState().OreHundredths(BLUE) < StartingOreHundredths(match), L"the site is paid for");
    const std::size_t designs = match.SimulationState().BuildSnapshot(BLUE).designs.size();

    const std::uint64_t lost = LoseBlue(match);
    Assert::IsFalse(match.SimulationState().MatchOver(), L"no match ends in a universe");
    Assert::IsTrue(match.SimulationState().BuildSnapshot(RED).events.empty() ||
                     std::ranges::none_of(match.SimulationState().BuildSnapshot(RED).events,
                                          [](const Outpost::EventView& _event) { return _event.kind == Outpost::EventKind::EmpireLost; }),
                   L"Red is told nothing of Blue's loss");
    const Outpost::Snapshot fallen = match.SimulationState().BuildSnapshot(BLUE);
    Assert::AreEqual(lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND), fallen.restartTick.value_or(0), L"an hour on");
    Assert::IsTrue(fallen.tickets.empty(), L"no tickets: domination is off");
    Assert::IsFalse(match.SimulationState().BuildSnapshot(RED).restartTick.has_value());

    while (match.SimulationState().CurrentTick() + 1 < fallen.restartTick.value_or(0))
      (void)match.SimulationState().Tick({});
    Assert::AreEqual(std::size_t{0}, Stations(match, BLUE), L"not before the hour");
    (void)match.SimulationState().Tick({});
    const Outpost::Snapshot back = match.SimulationState().BuildSnapshot(BLUE);
    Assert::IsTrue(
      std::ranges::any_of(back.events, [](const Outpost::EventView& _event) { return _event.kind == Outpost::EventKind::EmpireRestarted; }),
      L"on the hour");
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE));
    const auto station =
      std::ranges::find_if(match.SimulationState().Entities(), [](const Outpost::Entity& _entity)
                           { return _entity.owner == BLUE && _entity.structure == Outpost::StructureKind::CommandStation; });
    Assert::AreEqual(0.0f, Outpost::Distance(station->position, match.Start(BLUE)), L"at its start");
    Assert::IsTrue(
      match.Constructors(BLUE).size() >=
        static_cast<std::size_t>(match.TuningData().rules.startingConstructors) +
          std::ranges::count_if(constructors, [&](Outpost::EntityId _id) { return match.SimulationState().FindEntity(_id) != nullptr; }),
      L"the starting Constructors, beside those that still stand");
    Assert::AreEqual(StartingOreHundredths(match), match.SimulationState().OreHundredths(BLUE), L"its Ore raised to the starting Ore");
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
    while (match.SimulationState().CurrentTick() + 1 < lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND))
      (void)match.SimulationState().Tick({});
    const std::int64_t bank = match.SimulationState().OreHundredths(BLUE);
    Assert::IsTrue(bank > StartingOreHundredths(match), L"Blue has earned more than it started with");
    (void)match.SimulationState().Tick({});
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE));
    Assert::IsTrue(match.SimulationState().OreHundredths(BLUE) >= bank, L"and keeps it");
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
    const Outpost::Snapshot waiting = match.SimulationState().BuildSnapshot(BLUE);
    Assert::IsTrue(waiting.restartTick.value_or(0) < waiting.tick, L"and it waits past the hour");
    Assert::AreEqual(lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND), waiting.restartTick.value_or(0));

    (void)Raid(match, BLUE, match.SimulationState().FindEntity(yard)->position);
    const std::optional<std::uint64_t> restarted = RunUntil(match, BLUE, Outpost::EventKind::EmpireRestarted, 240);
    Assert::IsTrue(restarted.has_value(), L"once its start is clear");
    Assert::IsNull(match.SimulationState().FindEntity(yard), L"cleared by Blue's ships");
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE));
  }

  // Another player's warship in Blue's home sector holds Blue's start, as a structure does (owner, 2026-10-25): Blue waits
  // past the hour while it stands there, and restarts in the first tick that finds it gone, the tick after the move that
  // takes it out of the sector.
  TEST_METHOD(WaitsWhileAnEnemyWarshipIsInItsStart)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    const std::uint64_t lost = LoseBlue(match);
    const Outpost::PlanePosition start = match.Start(BLUE);
    const Outpost::EntityId warship = match.Warship(RED, {start.xMeters + 400.0f, start.zMeters + 400.0f});
    RunPastTheHour(match, lost);
    Assert::AreEqual(lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND),
                     match.SimulationState().BuildSnapshot(BLUE).restartTick.value_or(0));

    (void)match.SimulationState().Tick(
      {{.player = RED, .order = Outpost::MoveCommand{.ships = {warship}, .destination = match.Start(RED)}}});
    bool left = false;
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && !left; ++tick)
    {
      (void)match.SimulationState().Tick({});
      Assert::AreEqual(std::size_t{0}, Stations(match, BLUE), L"not while it is in the sector, nor in the tick it leaves");
      left = !match.Placement(A1).Contains(match.SimulationState().FindEntity(warship)->position);
    }
    Assert::IsTrue(left, L"the warship leaves");
    (void)match.SimulationState().Tick({});
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE), L"restarted in the tick after it left");
    Assert::IsTrue(std::ranges::any_of(match.SimulationState().BuildSnapshot(BLUE).events, [](const Outpost::EventView& _event)
                                       { return _event.kind == Outpost::EventKind::EmpireRestarted; }));
  }

  // A warship destroyed in Blue's home sector frees Blue's start in the tick it is destroyed: the destroyed leave before
  // the server looks at the starts.
  TEST_METHOD(RestartsInTheTickTheEnemyWarshipIsDestroyed)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    const std::uint64_t lost = LoseBlue(match);
    const Outpost::PlanePosition start = match.Start(BLUE);
    const Outpost::EntityId warship = match.Warship(RED, {start.xMeters + 400.0f, start.zMeters + 400.0f});
    RunPastTheHour(match, lost);

    (void)Raid(match, BLUE, match.SimulationState().FindEntity(warship)->position);
    std::optional<std::uint64_t> destroyed;
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && !destroyed.has_value(); ++tick)
    {
      Assert::AreEqual(std::size_t{0}, Stations(match, BLUE), L"not while it stands");
      (void)match.SimulationState().Tick({});
      if (match.SimulationState().FindEntity(warship) == nullptr)
        destroyed = match.SimulationState().CurrentTick();
    }
    Assert::IsTrue(destroyed.has_value(), L"Blue's ships destroy it");
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE), L"and Blue restarts in that tick");
    Assert::IsTrue(std::ranges::any_of(match.SimulationState().BuildSnapshot(BLUE).events, [](const Outpost::EventView& _event)
                                       { return _event.kind == Outpost::EventKind::EmpireRestarted; }));
  }

  // Neither a pirate's warship nor another player's Constructor holds a start: with both in Blue's home sector, Blue
  // restarts on the hour. The match has its pirates, for their designs.
  TEST_METHOD(APirateOrAConstructorDoesNotHoldAStart)
  {
    TerritoryMatch match(true, RESTART_SECONDS);
    const std::uint64_t lost = LoseBlue(match);
    const Outpost::PlanePosition start = match.Start(BLUE);
    const auto outpostShip =
      std::ranges::find_if(match.SimulationState().Entities(), [](const Outpost::Entity& _entity)
                           { return _entity.owner == Outpost::PIRATES && _entity.kind == Outpost::EntityKind::Ship; });
    Assert::IsTrue(outpostShip != match.SimulationState().Entities().end(), L"the match has pirates");
    const Outpost::EntityId pirate =
      match.SimulationState().SpawnShip(Outpost::PIRATES, outpostShip->design, {start.xMeters - 700.0f, start.zMeters - 700.0f});
    // Behind the start from Red's raiders on their way home, and out of the pirate's reach, which takes either for an enemy.
    const Outpost::EntityId constructor = match.SimulationState().SpawnConstructor(RED, {start.xMeters + 700.0f, start.zMeters - 700.0f});
    const std::uint64_t hour = lost + (std::uint64_t{RESTART_SECONDS} * TICKS_PER_SECOND);
    while (match.SimulationState().CurrentTick() < hour)
      (void)match.SimulationState().Tick({});
    const Outpost::Entity* pirateShip = match.SimulationState().FindEntity(pirate);
    const Outpost::Entity* redConstructor = match.SimulationState().FindEntity(constructor);
    Assert::IsTrue(pirateShip != nullptr && match.Placement(A1).Contains(pirateShip->position), L"the pirate is in Blue's home sector");
    Assert::IsTrue(redConstructor != nullptr && match.Placement(A1).Contains(redConstructor->position), L"and so is Red's Constructor");
    Assert::AreEqual(std::size_t{1}, Stations(match, BLUE), L"Blue restarts on the hour");
    Assert::IsTrue(std::ranges::any_of(match.SimulationState().BuildSnapshot(BLUE).events, [](const Outpost::EventView& _event)
                                       { return _event.kind == Outpost::EventKind::EmpireRestarted; }));
  }

  // A save keeps a lost seat waiting (ADR-077): a universe saved while Blue waits, and loaded, restarts it in the same tick as
  // the universe that never stopped, and is that universe after.
  TEST_METHOD(ASavedLostSeatRestartsOnTheSameTick)
  {
    TerritoryMatch match(false, RESTART_SECONDS);
    (void)LoseBlue(match);
    const Outpost::UniverseIdentity identity{.seed = 3, .ticksPerSecond = TICKS_PER_SECOND, .dataHash = 1};
    const std::vector<std::byte> saved = Outpost::EncodeUniverse(match.SimulationState(), identity);
    TerritoryMatch loaded(false, RESTART_SECONDS);
    (void)Outpost::DecodeUniverse(saved, identity, loaded.SimulationState());
    Assert::IsTrue(loaded.SimulationState() == match.SimulationState());
    const std::optional<std::uint64_t> ran = RunUntil(match, BLUE, Outpost::EventKind::EmpireRestarted, RESTART_SECONDS + 5);
    const std::optional<std::uint64_t> again = RunUntil(loaded, BLUE, Outpost::EventKind::EmpireRestarted, RESTART_SECONDS + 5);
    Assert::IsTrue(ran.has_value() && ran == again, L"restarted on the same tick");
    Assert::IsTrue(loaded.SimulationState() == match.SimulationState());
  }

  // A match is not a universe: it still ends when a player loses, and keeps its domination's tickets.
  TEST_METHOD(AMatchStillEnds)
  {
    TerritoryMatch match;
    Assert::IsFalse(match.SimulationState().BuildSnapshot(BLUE).tickets.empty());
    (void)Raid(match, RED, match.Start(BLUE));
    for (std::uint32_t tick = 0; tick < 240 * TICKS_PER_SECOND && !match.SimulationState().MatchOver(); ++tick)
      (void)match.SimulationState().Tick({});
    Assert::IsTrue(match.SimulationState().MatchOver());
    Assert::IsTrue(match.SimulationState().Winner() == RED);
    Assert::IsFalse(match.SimulationState().BuildSnapshot(BLUE).restartTick.has_value());
  }
};
} // namespace GameLogicTests
