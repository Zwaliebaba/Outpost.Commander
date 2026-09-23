#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

TEST_CLASS(RecordBuilding)
{
public:
  /// `RecordOf` is the one place the host quantizes (ADR-024): every field of the twelve-byte record,
  /// from one entity.
  TEST_METHOD(AnEntityBecomesItsRecord)
  {
    Outpost::World world;
    const Outpost::EntityId station = world.Create(Neuron::Vec2{.x = 640, .y = -640}, 0x1234, Outpost::DesignId::Station, 3);
    const Outpost::EntityRecord record = Outpost::RecordOf(*world.Find(station));

    Assert::AreEqual(Outpost::PackIdentity(station.index, station.generation), record.identity);
    // **THE OWNER IS ITS OWN BYTE** since ADR-024, where it was two team bits in the flags.
    Assert::AreEqual(Outpost::PlayerId{3}, record.owner);
    Assert::AreEqual(std::uint8_t{0}, record.flags, L"nothing is in the flags at M1");
    // **THE DESIGN REACHES THE WIRE, and this pins which byte.** ADR-003 gives it a byte of its own
    // because a design is what a client draws and what research and a designer extend.
    Assert::AreEqual(static_cast<std::uint8_t>(Outpost::DesignId::Station), record.designIdentity);
    Assert::AreEqual(std::uint8_t{0x12}, record.heading, L"the wire heading is the top eight bits");
    Assert::AreEqual(std::uint8_t{100}, record.hullPercentRemaining, L"an undamaged entity reads a hundred");
  }

  TEST_METHOD(APositionSurvivesToTheWireAndBack)
  {
    Outpost::World world;
    const Outpost::EntityId id = world.Create(Neuron::Vec2{.x = 4096, .y = -8192}, 0, Outpost::DesignId::Miner, 1);
    const Outpost::EntityRecord record = Outpost::RecordOf(*world.Find(id));
    Assert::AreEqual(Neuron::Fixed{4096}, Outpost::DequantizePosition(record.positionX));
    Assert::AreEqual(Neuron::Fixed{-8192}, Outpost::DequantizePosition(record.positionY));
  }

  /// The whole reliability channel ADR-003 has: a command is repeated until this field reaches its
  /// sequence. **Each player's own block carries its own** -- an update carries nobody else's.
  TEST_METHOD(EachPlayersBlockCarriesItsOwnAcknowledgment)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, 1);
    Assert::AreEqual(std::uint8_t{0}, static_cast<std::uint8_t>(
                                        intake.Apply(world, build, 1,
                                                     Outpost::Command{.sequence = 31,
                                                                      .type = Outpost::CommandType::MoveTo,
                                                                      .selection = {Outpost::PackIdentity(mine.index, mine.generation)}})));

    Assert::AreEqual(std::uint16_t{31}, Outpost::PlayerBlockFor(intake, build, 1).lastCommandSequenceApplied);
    Assert::AreEqual(std::uint16_t{0}, Outpost::PlayerBlockFor(intake, build, 2).lastCommandSequenceApplied);
  }
};

TEST_CLASS(TheHostLoop)
{
public:
  TEST_METHOD(ATickAdvancesTheSimulation)
  {
    Outpost::Host host;
    const Outpost::EntityId mover = host.MutableWorld().Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, 1);
    Assert::IsTrue(host.MutableWorld().OrderMoveTo(mover, Neuron::Vec2{.x = 10000, .y = 0}, 7 * 256));

    const Neuron::Vec2 before = host.CurrentWorld().Find(mover)->position;
    host.RunOneTick();
    Assert::IsFalse(host.CurrentWorld().Find(mover)->position == before, L"a tick did not move anything");
  }

  /// **TO SESSIONS, NOT TO THE AIR** (ADR-013). With nobody seated the host sends nothing and the
  /// accumulator has no client to track, so the first update after a join is its sequence zero.
  TEST_METHOD(NothingIsSentWithNobodySeated)
  {
    Outpost::Host host;
    for (int tick = 0; tick < 20; ++tick)
    {
      host.RunOneTick();
    }
    Assert::AreEqual(std::uint64_t{0}, host.UpdatesSent());
    Assert::AreEqual(std::size_t{0}, host.CurrentAccumulator().ClientCount());
    Assert::AreEqual(std::size_t{0}, host.ClientCount());
  }

  /// **FOUR IS THE GAME'S NUMBER; PAST IT ONLY UNDER THE STRESS SWITCH** (ADR-023), and never zero.
  TEST_METHOD(ThePlayerCountIsAllowedByTheSwitch)
  {
    Assert::IsFalse(Outpost::PlayerCountAllowed(0, false));
    Assert::IsFalse(Outpost::PlayerCountAllowed(0, true));
    for (std::size_t players = 1; players <= 4; ++players)
    {
      Assert::IsTrue(Outpost::PlayerCountAllowed(players, false));
      Assert::IsTrue(Outpost::PlayerCountAllowed(players, true));
    }
    Assert::IsFalse(Outpost::PlayerCountAllowed(5, false), L"a real match was configured past the design");
    Assert::IsTrue(Outpost::PlayerCountAllowed(5, true));
    Assert::IsTrue(Outpost::PlayerCountAllowed(254, true));
    Assert::IsFalse(Outpost::PlayerCountAllowed(255, true), L"past what a PlayerId can name");
  }

  /// A stress match seats as many as it was given: a station each, and a slot each.
  TEST_METHOD(AStressMatchSeatsEveryPlayer)
  {
    Outpost::Host host;
    host.BeginMatch(Outpost::DEFAULT_MATCH_SEED, 8);
    Assert::AreEqual(std::size_t{8}, host.PlayerCount());
    Assert::AreEqual(std::size_t{8}, host.CurrentWorld().AliveCount());
    Assert::AreEqual(std::size_t{8}, host.CurrentSessions().PlayerCount());
  }

  /// And a host nobody configured is the MVP's two, which is Q27's answer.
  TEST_METHOD(TheDefaultIsTwoPlayers)
  {
    const Outpost::Host host;
    Assert::AreEqual(std::size_t{2}, host.PlayerCount());
    Assert::AreEqual(std::size_t{2}, host.CurrentWorld().AliveCount());
  }

  TEST_METHOD(TheTickPeriodIsTheDesigns)
  {
    Assert::AreEqual(std::int64_t{50}, Outpost::TICK_PERIOD_MILLISECONDS);
  }
};

} // namespace GameLogicTests
