#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr Outpost::DesignId SWARM{1};
constexpr std::uint32_t TICKS_PER_SECOND = 20;
// A Small hull on an Ion drive, as the repository's tuning data makes it.
constexpr Outpost::ShipMovement SMALL_ION{.speedMetersPerSecond = 78.0f, .turnRateRadiansPerSecond = 3.927f, .radiusMeters = 8.0f};

Outpost::Command Move(Outpost::PlayerId _player, std::vector<Outpost::EntityId> _ships, Outpost::PlanePosition _destination)
{
  return {.player = _player, .order = Outpost::MoveCommand{.ships = std::move(_ships), .destination = _destination}};
}

Outpost::Command Stop(Outpost::PlayerId _player, std::vector<Outpost::EntityId> _ships)
{
  return {.player = _player, .order = Outpost::StopCommand{.ships = std::move(_ships)}};
}

std::wstring Describe(Outpost::CommandResult _result)
{
  return std::to_wstring(static_cast<int>(_result));
}

void ExpectResults(const std::vector<Outpost::CommandResult>& _expected, const std::vector<Outpost::CommandResult>& _actual)
{
  Assert::AreEqual(_expected.size(), _actual.size());
  for (size_t i = 0; i < _expected.size(); ++i)
    Assert::IsTrue(_expected[i] == _actual[i], (L"command " + std::to_wstring(i) + L" gave " + Describe(_actual[i])).c_str());
}
} // namespace

TEST_CLASS(SimulationTests)
{
public:
  TEST_METHOD(AppliesACommandAtTheNextTick)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, SWARM, SMALL_ION, {.xMeters = 10.0f, .zMeters = 20.0f});
    Assert::IsFalse(simulation.FindEntity(ship)->destination.has_value());

    ExpectResults({Outpost::CommandResult::Applied}, simulation.Tick({Move(BLUE, {ship}, {.xMeters = 300.0f, .zMeters = -40.0f})}));
    Assert::AreEqual(std::uint64_t{1}, simulation.CurrentTick());
    Assert::IsTrue(simulation.FindEntity(ship)->destination == Outpost::PlanePosition{.xMeters = 300.0f, .zMeters = -40.0f});

    ExpectResults({Outpost::CommandResult::Applied}, simulation.Tick({Stop(BLUE, {ship})}));
    Assert::IsFalse(simulation.FindEntity(ship)->destination.has_value());
  }

  TEST_METHOD(RejectsAnInvalidCommandAndChangesNothing)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    const Outpost::EntityId blue = simulation.SpawnShip(BLUE, SWARM, SMALL_ION, {});
    const Outpost::EntityId red = simulation.SpawnShip(RED, SWARM, SMALL_ION, {});
    const Outpost::Simulation before = simulation;

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<Outpost::CommandResult> results = simulation.Tick({
      Move(BLUE, {}, {}),
      Move(BLUE, {Outpost::EntityId{99}}, {}),
      Move(BLUE, {red}, {}),
      Move(BLUE, {blue, red}, {}),
      Move(BLUE, {blue}, {.xMeters = nan}),
      Stop(RED, {blue}),
      {.player = BLUE, .order = Outpost::AttackCommand{.ships = {blue}, .target = Outpost::EntityId{99}}},
      // A ship placed without hit points is out of combat, so it is not a target.
      {.player = BLUE, .order = Outpost::AttackCommand{.ships = {blue}, .target = red}},
      {.player = BLUE, .order = Outpost::AttackCommand{.ships = {blue}, .target = blue}},
      {.player = BLUE, .order = Outpost::QueueShipCommand{.producer = blue, .design = SWARM}},
    });
    ExpectResults({Outpost::CommandResult::NoShips, Outpost::CommandResult::UnknownEntity, Outpost::CommandResult::NotOwned,
                   Outpost::CommandResult::NotOwned, Outpost::CommandResult::InvalidPosition, Outpost::CommandResult::NotOwned,
                   Outpost::CommandResult::UnknownTarget, Outpost::CommandResult::NotAnEnemy, Outpost::CommandResult::NotAnEnemy,
                   Outpost::CommandResult::NotYetSupported},
                  results);

    // A command naming two ships, one of them not the sender's, moves neither.
    Assert::IsTrue(simulation.FindEntity(blue)->destination == before.FindEntity(blue)->destination);
    Assert::IsTrue(simulation.FindEntity(red)->destination == before.FindEntity(red)->destination);
  }

  TEST_METHOD(SnapshotsShowEveryEntityAtTheCurrentTick)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    const Outpost::EntityId blue = simulation.SpawnShip(BLUE, SWARM, SMALL_ION, {.xMeters = 1.0f, .zMeters = 2.0f});
    (void)simulation.SpawnShip(RED, SWARM, SMALL_ION, {.xMeters = 100.0f});
    (void)simulation.Tick({});

    const Outpost::Snapshot snapshot = simulation.BuildSnapshot(RED);
    Assert::AreEqual(std::uint64_t{1}, snapshot.tick);
    Assert::IsTrue(snapshot.player == RED);
    Assert::AreEqual(size_t{2}, snapshot.entities.size());
    Assert::IsTrue(snapshot.entities[0].id == blue);
    Assert::IsTrue(snapshot.entities[0].owner == BLUE);
    Assert::IsTrue(snapshot.entities[0].position == Outpost::PlanePosition{.xMeters = 1.0f, .zMeters = 2.0f});
  }

  // ADR-009: the same seed and the same commands at the same ticks give the same state, on the same build.
  TEST_METHOD(ReplaysFromASeedAndACommandLog)
  {
    auto play = [](std::uint64_t _seed)
    {
      Outpost::Simulation simulation(_seed, TICKS_PER_SECOND);
      const Outpost::EntityId first = simulation.SpawnShip(BLUE, SWARM, SMALL_ION, {});
      const Outpost::EntityId second = simulation.SpawnShip(BLUE, SWARM, SMALL_ION, {});
      const Outpost::EntityId enemy = simulation.SpawnShip(RED, SWARM, SMALL_ION, {});
      for (int tick = 0; tick < 100; ++tick)
      {
        std::vector<Outpost::Command> commands;
        if (tick % 7 == 0)
        {
          commands.push_back(
            Move(BLUE, {first, second}, {.xMeters = static_cast<float>(tick) * 3.5f, .zMeters = static_cast<float>(-tick) * 1.25f}));
        }
        if (tick % 11 == 0)
          commands.push_back(Move(RED, {enemy, first}, {}));
        if (tick % 13 == 0)
          commands.push_back(Stop(BLUE, {second}));
        (void)simulation.Tick(commands);
      }
      return simulation;
    };

    Assert::IsTrue(play(1234) == play(1234));
    // The seed is part of the state, so another seed gives another match.
    Assert::IsFalse(play(1234) == play(4321));
  }
};
} // namespace GameLogicTests