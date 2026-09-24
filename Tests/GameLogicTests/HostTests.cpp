#include "pch.h"

#include <cstdint>
#include <vector>

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
    const Outpost::EntityRecord record = Outpost::RecordOf(world, station.index);

    Assert::AreEqual(Outpost::PackIdentity(station.index, station.generation), record.identity);
    // **THE OWNER IS ITS OWN BYTE** since ADR-024, where it was two team bits in the flags.
    Assert::AreEqual(Outpost::PlayerId{3}, record.owner);
    Assert::AreEqual(std::uint8_t{0}, record.flags, L"an empty hold, and no state yet");
    // **THE DESIGN REACHES THE WIRE, and this pins which byte.** ADR-003 gives it a byte of its own
    // because a design is what a client draws and what research and a designer extend.
    Assert::AreEqual(static_cast<std::uint8_t>(Outpost::DesignId::Station), record.designIdentity);
    Assert::AreEqual(std::uint8_t{0x12}, record.heading, L"the wire heading is the top eight bits");
    Assert::AreEqual(std::uint8_t{100}, record.hullPercentRemaining, L"an undamaged entity reads a hundred");
  }

  /// **THE HOLD REACHES THE WIRE AS CHIPS** (M2.7, Q53): a miner a quarter and a bit full lights two of the
  /// panel's four, and a design that carries no ore lights none whatever its slot says.
  TEST_METHOD(AHoldBecomesItsCargoChips)
  {
    Outpost::World world;
    const Outpost::EntityId miner = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, 1);
    world.MineInSlot(miner.index).cargoMilliOre = 26 * Outpost::MILLI_ORE_PER_ORE;
    Assert::AreEqual(std::uint8_t{2}, Outpost::CargoChipsOf(Outpost::RecordOf(world, miner.index).flags));

    world.MineInSlot(miner.index).cargoMilliOre = 100 * Outpost::MILLI_ORE_PER_ORE;
    Assert::AreEqual(std::uint8_t{4}, Outpost::CargoChipsOf(Outpost::RecordOf(world, miner.index).flags));

    const Outpost::EntityId fighter = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Fighter, 1);
    Assert::AreEqual(std::uint8_t{0}, Outpost::CargoChipsOf(Outpost::RecordOf(world, fighter.index).flags));
  }

  TEST_METHOD(APositionSurvivesToTheWireAndBack)
  {
    Outpost::World world;
    const Outpost::EntityId id = world.Create(Neuron::Vec2{.x = 4096, .y = -8192}, 0, Outpost::DesignId::Miner, 1);
    const Outpost::EntityRecord record = Outpost::RecordOf(world, id.index);
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
    Assert::IsTrue(host.MutableWorld().OrderMoveTo(mover, Neuron::Vec2{.x = 10000, .y = 0}, 7 * 256,
                                                   Outpost::TurnAnglePerTick(Outpost::DesignId::Miner)));

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

  /// A stress match seats as many as it was given: a station and three starting ships each (Q84), and a slot each.
  TEST_METHOD(AStressMatchSeatsEveryPlayer)
  {
    Outpost::Host host;
    host.BeginMatch(Outpost::DEFAULT_MATCH_SEED, 8);
    Assert::AreEqual(std::size_t{8}, host.PlayerCount());
    Assert::AreEqual(std::size_t{8 * 4}, host.CurrentWorld().AliveCount());
    Assert::AreEqual(std::size_t{8}, host.CurrentSessions().PlayerCount());
  }

  /// And a host nobody configured is the MVP's two, which is Q27's answer.
  TEST_METHOD(TheDefaultIsTwoPlayers)
  {
    const Outpost::Host host;
    Assert::AreEqual(std::size_t{2}, host.PlayerCount());
    Assert::AreEqual(std::size_t{2 * 4}, host.CurrentWorld().AliveCount());
  }

  /// **NO ASTEROID IS IN THE WORLD, SO NONE CAN REACH THE WIRE** (M2.3, R23, Q22). An update carries
  /// records of world entities and nothing else (ADR-024), so a match that begins with a station and three
  /// starting ships per player (Q84) and not one entity more sends no map -- and nothing in the world sits where a rock does. M3's
  /// finite ore is where this is expected to change, and it fails here when it does.
  TEST_METHOD(AMatchBeginsWithNoAsteroidInTheWorld)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      Outpost::Host host;
      host.BeginMatch(Outpost::DEFAULT_MATCH_SEED, players);
      Assert::AreEqual(players * 4, host.CurrentWorld().AliveCount(), L"something besides the stations and starting ships was created");

      const std::vector<Outpost::Placement> field = Outpost::GenerateField(Outpost::DEFAULT_MATCH_SEED, players);
      for (std::size_t slot = 0; slot < host.CurrentWorld().SlotCount(); ++slot)
      {
        if (!host.CurrentWorld().IsSlotAlive(slot))
        {
          continue;
        }
        const Neuron::Vec2 position = host.CurrentWorld().EntityInSlot(slot).position;
        for (const Outpost::Placement& rock : field)
        {
          Assert::IsFalse(position == rock.position, L"an entity sits on a rock");
        }
      }
    }
  }

  TEST_METHOD(TheTickPeriodIsTheDesigns)
  {
    Assert::AreEqual(std::int64_t{50}, Outpost::TICK_PERIOD_MILLISECONDS);
  }
};

/// M3.8. **The match restarts on the tick it ends**, without the host being restarted.
TEST_CLASS(TheRestart)
{
public:
  /// A station lost ends the match, and the same tick begins the next on the next seed, with fresh stations.
  TEST_METHOD(AMatchThatEndsIsReplacedOnTheSameTick)
  {
    Outpost::Host host;
    host.BeginMatch(Outpost::DEFAULT_MATCH_SEED, 2);
    host.RunOneTick();
    Assert::AreEqual(std::uint16_t{0}, host.LastEnded().matchNumber);

    Outpost::World& world = host.MutableWorld();
    for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
    {
      if (world.IsSlotAlive(slot) && (world.EntityInSlot(slot).owner == 2) &&
          (world.EntityInSlot(slot).design == Outpost::DesignId::Station))
      {
        world.EntityInSlot(slot).hullRemaining = 0;
      }
    }
    host.RunOneTick();

    Assert::AreEqual(std::uint16_t{1}, host.LastEnded().matchNumber);
    Assert::AreEqual(Outpost::PlayerId{1}, host.LastEnded().winner);
    Assert::IsFalse(host.LastEnded().onClock);
    Assert::AreEqual(Outpost::Host::NextMatchSeed(Outpost::DEFAULT_MATCH_SEED), host.MatchSeed(),
                     L"the next match is not on the next seed");
    Assert::IsFalse(host.CurrentVictory().Outcome().over, L"the next match began already over");

    std::size_t stations = 0;
    for (std::size_t slot = 0; slot < host.CurrentWorld().SlotCount(); ++slot)
    {
      if (host.CurrentWorld().IsSlotAlive(slot) && (host.CurrentWorld().EntityInSlot(slot).design == Outpost::DesignId::Station))
      {
        ++stations;
        Assert::AreEqual(static_cast<std::uint16_t>(Outpost::Derive(Outpost::DesignId::Station).hullPoints),
                         host.CurrentWorld().EntityInSlot(slot).hullRemaining);
      }
    }
    Assert::AreEqual(std::size_t{2}, stations, L"the next match does not have both stations");
  }

  /// The run of seeds is the same on every host, and never the same map twice in a row.
  TEST_METHOD(TheNextSeedIsAFunctionOfThisOne)
  {
    const std::uint64_t next = Outpost::Host::NextMatchSeed(Outpost::DEFAULT_MATCH_SEED);
    Assert::AreEqual(next, Outpost::Host::NextMatchSeed(Outpost::DEFAULT_MATCH_SEED));
    Assert::AreNotEqual(Outpost::DEFAULT_MATCH_SEED, next);
    Assert::AreNotEqual(next, Outpost::Host::NextMatchSeed(next));
  }
};

} // namespace GameLogicTests
