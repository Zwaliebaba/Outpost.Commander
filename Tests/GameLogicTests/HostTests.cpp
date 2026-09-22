#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

TEST_CLASS(SnapshotBuilding)
{
public:
  TEST_METHOD(EveryLiveEntityReachesTheSnapshotInIndexOrder)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId first = world.Create(Neuron::Vec2{.x = 640, .y = -640}, 0x1234, Outpost::DesignId::Station, 1);
    const Outpost::EntityId doomed = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, 1);
    const Outpost::EntityId third = world.Create(Neuron::Vec2{.x = 128, .y = 0}, 0, Outpost::DesignId::Miner, 2);
    Assert::IsTrue(world.Destroy(doomed));

    const Outpost::Snapshot snapshot = Outpost::BuildSnapshot(world, intake, build, 42, 7, 2);

    Assert::AreEqual(std::size_t{2}, snapshot.entities.size(), L"a dead slot reached the snapshot");
    Assert::AreEqual(std::uint32_t{42}, snapshot.tick);
    Assert::AreEqual(std::uint16_t{7}, snapshot.sequence);
    Assert::AreEqual(std::size_t{2}, snapshot.players.size());

    Assert::AreEqual(Outpost::PackIdentity(first.index, first.generation), snapshot.entities[0].identity);
    Assert::AreEqual(Outpost::PackIdentity(third.index, third.generation), snapshot.entities[1].identity);
    // **THE DESIGN REACHES THE WIRE, and this pins which byte.** The field has had three tenants:
    // a loose 7 until M1.1, the catalog's hull identity until M1.3, and now the design. ADR-003
    // gives it a byte of its own precisely because a design is what a client draws and what
    // research and a designer extend, and the two bits it had in the flags were four designs
    // forever.
    Assert::AreEqual(static_cast<std::uint8_t>(Outpost::DesignId::Station), snapshot.entities[0].designIdentity);
    Assert::AreEqual(std::uint8_t{0x12}, snapshot.entities[0].heading, L"the wire heading is the top eight bits");
  }

  TEST_METHOD(APositionSurvivesToTheWireAndBack)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    static_cast<void>(world.Create(Neuron::Vec2{.x = 4096, .y = -8192}, 0, Outpost::DesignId::Miner, 1));

    const Outpost::Snapshot snapshot = Outpost::BuildSnapshot(world, intake, build, 0, 0, 2);
    Assert::AreEqual(Neuron::Fixed{4096}, Outpost::DequantizePosition(snapshot.entities[0].positionX));
    Assert::AreEqual(Neuron::Fixed{-8192}, Outpost::DequantizePosition(snapshot.entities[0].positionY));
  }

  TEST_METHOD(ThePlayerBlockCarriesTheAcknowledgment)
  {
    // The whole reliability channel ADR-003 has: a command is repeated until this field reaches
    // its sequence.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, 1);
    Assert::AreEqual(std::uint8_t{0}, static_cast<std::uint8_t>(
                                        intake.Apply(world, build, 1,
                                                     Outpost::Command{.sequence = 31,
                                                                      .type = Outpost::CommandType::MoveTo,
                                                                      .selection = {Outpost::PackIdentity(mine.index, mine.generation)}})));

    const Outpost::Snapshot snapshot = Outpost::BuildSnapshot(world, intake, build, 0, 0, 2);
    Assert::AreEqual(std::uint16_t{31}, snapshot.players[0].lastCommandSequenceApplied);
    Assert::AreEqual(std::uint16_t{0}, snapshot.players[1].lastCommandSequenceApplied);
  }

  TEST_METHOD(TheMvpWorldStillFitsOneDatagram)
  {
    // ADR-003's property, asserted against a world the host actually built rather than against
    // synthetic records: 110 entities is the MVP's count.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    for (int entity = 0; entity < 110; ++entity)
    {
      static_cast<void>(world.Create(Neuron::Vec2{.x = entity * 64, .y = -entity * 64}, 0, Outpost::DesignId::Miner,
                                     static_cast<Outpost::PlayerId>((entity % 2) + 1)));
    }

    const Outpost::Snapshot snapshot = Outpost::BuildSnapshot(world, intake, build, 1, 1, 2);
    Assert::AreEqual(std::size_t{110}, snapshot.entities.size());

    // 1,136 of records and header, plus the fire count byte: the figure M0.9 measured, reached
    // from the other direction.
    // 1,137 is the budget's case, which carries three removals at two bytes each. M0 removes
    // nothing, so the same 110 entities are six bytes lighter.
    Assert::AreEqual(std::size_t{1131}, Outpost::EncodedSize(snapshot));
    Assert::IsTrue(Outpost::EncodedSize(snapshot) <= 1232);
  }

  TEST_METHOD(AnEmptyWorldStillProducesAValidSnapshot)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::Snapshot snapshot = Outpost::BuildSnapshot(world, intake, build, 0, 0, 2);
    Assert::AreEqual(std::size_t{0}, snapshot.entities.size());
    Assert::AreEqual(std::size_t{2}, snapshot.players.size());
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

  TEST_METHOD(TheSequenceDoesNotAdvanceWithNobodyListening)
  {
    // A client watches the sequence advance by exactly one per snapshot it receives, so a
    // sequence that moved while nobody was connected would make the first snapshot after a join
    // look like a gap.
    Outpost::Host host;
    for (int tick = 0; tick < 20; ++tick)
    {
      host.RunOneTick();
    }
    Assert::AreEqual(std::uint16_t{0}, host.SnapshotSequence());
    Assert::AreEqual(std::size_t{0}, host.ClientCount());
  }

  TEST_METHOD(TheTickPeriodIsTheDesigns)
  {
    Assert::AreEqual(std::int64_t{50}, Outpost::TICK_PERIOD_MILLISECONDS);
  }
};

} // namespace GameLogicTests
