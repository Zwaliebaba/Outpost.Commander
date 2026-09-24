#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
inline constexpr Outpost::PlayerId MINE = 1;
inline constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] std::uint8_t Code(Outpost::CommandRejection _rejection) noexcept
{
  return static_cast<std::uint8_t>(_rejection);
}

[[nodiscard]] Outpost::WireIdentity Wire(Outpost::EntityId _id) noexcept
{
  return Outpost::PackIdentity(_id.index, _id.generation);
}

[[nodiscard]] Outpost::Command MoveTo(std::uint16_t _sequence, std::vector<Outpost::WireIdentity> _selection)
{
  return Outpost::Command{
    .sequence = _sequence, .type = Outpost::CommandType::MoveTo, .targetX = 100, .targetY = 100, .selection = std::move(_selection)};
}
} // namespace

TEST_CLASS(CommandValidation)
{
public:
  TEST_METHOD(AValidOrderIsApplied)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(1, {Wire(mine)}))));
    Assert::IsTrue(world.FindOrder(mine)->active, L"an accepted move should have produced an order");
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(MINE));
  }

  TEST_METHOD(AForeignEntityIsRejected)
  {
    // Q24's first check. At M0 there is one entity and this has nothing to reject; it is written
    // anyway, which is the point of the step.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    static_cast<void>(world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE));
    const Outpost::EntityId theirs = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, THEIRS);

    Assert::AreEqual(Code(Outpost::CommandRejection::NotOwned), Code(intake.Apply(world, build, MINE, MoveTo(1, {Wire(theirs)}))));
    Assert::IsFalse(world.FindOrder(theirs)->active, L"a rejected command must not move anything");
    // **ACKNOWLEDGED, AND STILL REFUSED** (Q24 as amended after the 2026-09-23 review): the host understood the
    // order and will never apply it, so a sequence that did not advance would have the client resend it forever.
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(MINE), L"a refusal past the sequence check must acknowledge");
  }

  TEST_METHOD(AnOverLongSelectionIsRejected)
  {
    // Q24's second, and the one ADR-003's 5.5x amplification argument is about: a packet can name
    // far more identities than the sender owns entities, from an ordinary bug and no attacker.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    std::vector<Outpost::WireIdentity> selection;
    for (int repeat = 0; repeat < 200; ++repeat)
    {
      selection.push_back(Wire(mine));
    }

    Assert::AreEqual(Code(Outpost::CommandRejection::SelectionTooLong), Code(intake.Apply(world, build, MINE, MoveTo(1, selection))));
    Assert::IsFalse(world.FindOrder(mine)->active);
  }

  TEST_METHOD(AStaleGenerationIsSkipped)
  {
    // Q24's third, as amended. The slot is reused, so the index still resolves -- and the generation is what
    // stops it resolving to the wrong ship. The sender meant the ship that died, so the identity is skipped:
    // the new occupant is not ordered, and the command is accepted as the no-op it now is.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId dead = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);
    const Outpost::WireIdentity staleWire = Wire(dead);
    Assert::IsTrue(world.Destroy(dead));

    const Outpost::EntityId reborn = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);
    Assert::AreEqual(dead.index, reborn.index, L"the test needs the slot to have been reused");

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(1, {staleWire}))));
    Assert::IsFalse(world.FindOrder(reborn)->active, L"the new occupant must not have taken the dead one's order");
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(MINE));
  }

  /// **THE REVIEW'S M4** (2026-09-23): a retreat tapped during a raid names a ship that died on the host a
  /// moment before the client heard. The survivors are ordered and the dead one is skipped -- where the old
  /// all-or-nothing rule refused the order whole, never acknowledged it, and left the survivors where they were.
  TEST_METHOD(ADeadShipInASelectionIsSkippedAndTheRestAreOrdered)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId a = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId b = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId c = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::WireIdentity cWire = Wire(c);
    Assert::IsTrue(world.Destroy(c));

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(3, {Wire(a), Wire(b), cWire}))));
    Assert::IsTrue(world.FindOrder(a)->active);
    Assert::IsTrue(world.FindOrder(b)->active);
    Assert::AreEqual(std::uint16_t{3}, intake.LastAppliedSequence(MINE));
  }

  /// A selection that has shrunk by its dead is not an over-long one: the bound counts live identities, so
  /// ten selected with seven dead is three live against three owned, and the three are ordered.
  TEST_METHOD(TheLengthBoundCountsOnlyLiveShips)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    std::vector<Outpost::WireIdentity> selection;
    std::vector<Outpost::EntityId> survivors;
    for (int ship = 0; ship < 10; ++ship)
    {
      const Outpost::EntityId id = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Fighter, MINE);
      selection.push_back(Wire(id));
      if (ship < 7)
      {
        Assert::IsTrue(world.Destroy(id));
      }
      else
      {
        survivors.push_back(id);
      }
    }

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(1, selection))));
    for (const Outpost::EntityId id : survivors)
    {
      Assert::IsTrue(world.FindOrder(id)->active);
    }
  }

  /// Everything in it died: nothing to order, and the order is accepted as a no-op so the client stops sending it.
  TEST_METHOD(AnAllDeadSelectionIsAcceptedAsANoOp)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    static_cast<void>(world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE));
    const Outpost::EntityId gone = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::WireIdentity goneWire = Wire(gone);
    Assert::IsTrue(world.Destroy(gone));
    const std::uint64_t before = Outpost::StateHash(world);

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(4, {goneWire}))));
    Assert::AreEqual(before, Outpost::StateHash(world));
    Assert::AreEqual(std::uint16_t{4}, intake.LastAppliedSequence(MINE));
  }

  TEST_METHOD(AnOutOfMapTargetIsClampedRatherThanApplied)
  {
    // Q24's fourth. A wire target cannot leave the play area, so this reaches the clamp through
    // the value the decoder would produce at the extreme: the order lands on the edge, not
    // outside it.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Outpost::Command command = MoveTo(1, {Wire(mine)});
    command.targetX = 32767;
    command.targetY = -32768;
    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, command)));

    const Outpost::MoveOrder* order = world.FindOrder(mine);
    Assert::IsTrue(order->destination.x <= Outpost::PLAY_AREA_HALF_EXTENT);
    Assert::IsTrue(order->destination.y >= -Outpost::PLAY_AREA_HALF_EXTENT);
  }

  TEST_METHOD(AWrappedSequenceIsStillNewer)
  {
    // Q24's fifth, and the one an "at or below" comparison gets exactly backwards. After 65,535
    // the next sequence is 0, and a host that read that as older would ignore every command for
    // the rest of the match.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(65535, {Wire(mine)}))));
    Assert::AreEqual(std::uint16_t{65535}, intake.LastAppliedSequence(MINE));

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(0, {Wire(mine)}))));
    Assert::AreEqual(std::uint16_t{0}, intake.LastAppliedSequence(MINE));

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(1, {Wire(mine)}))));
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(MINE));
  }

  TEST_METHOD(AnOldSequenceAcrossTheWrapIsStillOld)
  {
    // The other half of the same check: 65,000 arriving after the wrap is genuinely old and must
    // stay rejected, or a reordered packet would undo the match.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(5, {Wire(mine)}))));
    Assert::AreEqual(Code(Outpost::CommandRejection::AlreadyApplied), Code(intake.Apply(world, build, MINE, MoveTo(65000, {Wire(mine)}))));
    Assert::AreEqual(std::uint16_t{5}, intake.LastAppliedSequence(MINE));
  }

  TEST_METHOD(AResentCommandIsIgnoredWithNoSideEffect)
  {
    // ADR-003 repeats every command in every packet until the snapshot acknowledges it, so this
    // is the ORDINARY case rather than a fault -- and it has to change nothing at all.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(7, {Wire(mine)}))));
    for (int tick = 0; tick < 5; ++tick)
    {
      Outpost::Tick(world);
    }
    const Neuron::Vec2 moved = world.Find(mine)->position;
    const std::uint64_t before = Outpost::StateHash(world);

    for (int resend = 0; resend < 10; ++resend)
    {
      Assert::AreEqual(Code(Outpost::CommandRejection::AlreadyApplied), Code(intake.Apply(world, build, MINE, MoveTo(7, {Wire(mine)}))));
    }

    Assert::IsTrue(world.Find(mine)->position == moved, L"a resent command moved something");
    Assert::AreEqual(before, Outpost::StateHash(world), L"a resent command changed the state");
    Assert::AreEqual(std::uint16_t{7}, intake.LastAppliedSequence(MINE));
  }

  TEST_METHOD(AnEmptySelectionAndAnUnknownPlayerAreRefused)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Assert::AreEqual(Code(Outpost::CommandRejection::Empty), Code(intake.Apply(world, build, MINE, MoveTo(1, {}))));
    Assert::AreEqual(Code(Outpost::CommandRejection::Empty), Code(intake.Apply(world, build, Outpost::NO_PLAYER, MoveTo(1, {Wire(mine)}))));
    // PAST WHAT A `PlayerId` CAN SEAT. It was 99 while the capacity was four; ADR-023 made 99 a player a
    // stress run can have, so the one number that can never be seated is the one past the capacity.
    Assert::AreEqual(Code(Outpost::CommandRejection::Empty),
                     Code(intake.Apply(world, build, static_cast<Outpost::PlayerId>(Outpost::MAX_PLAYERS + 1), MoveTo(1, {Wire(mine)}))));
  }

  TEST_METHOD(AForeignIdentityInASelectionAppliesNoneOfIt)
  {
    // A command that took for some ships and not others would leave the match in a state no
    // sequence number describes, and the client would never learn which half took.
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);
    const Outpost::EntityId alsoMine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);
    static_cast<void>(world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE));
    const Outpost::EntityId theirs = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, THEIRS);

    // THREE owned, so the selection-length bound passes and the ownership check is the one under
    // test. With two owned this fired SelectionTooLong first, which is correct and tests nothing.
    Assert::AreEqual(Code(Outpost::CommandRejection::NotOwned),
                     Code(intake.Apply(world, build, MINE, MoveTo(1, {Wire(mine), Wire(alsoMine), Wire(theirs)}))));
    Assert::IsFalse(world.FindOrder(mine)->active, L"the valid part of a refused command was applied");
    Assert::IsFalse(world.FindOrder(alsoMine)->active);
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(MINE), L"refused, and acknowledged");
  }

  TEST_METHOD(EachPlayerHasItsOwnSequence)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);
    const Outpost::EntityId theirs = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, THEIRS);

    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, MINE, MoveTo(50, {Wire(mine)}))));
    Assert::AreEqual(Code(Outpost::CommandRejection::None), Code(intake.Apply(world, build, THEIRS, MoveTo(1, {Wire(theirs)}))));
    Assert::AreEqual(std::uint16_t{50}, intake.LastAppliedSequence(MINE));
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(THEIRS));
  }

  TEST_METHOD(APacketAppliesEveryCommandItCarries)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId mine = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);

    Outpost::CommandPacket packet{};
    packet.player = MINE;
    packet.commands.push_back(MoveTo(1, {Wire(mine)}));
    packet.commands.push_back(MoveTo(2, {Wire(mine)}));
    packet.commands.push_back(MoveTo(2, {Wire(mine)}));

    Assert::AreEqual(std::size_t{2}, intake.ApplyPacket(world, build, packet), L"the repeat should not have counted");
    Assert::AreEqual(std::uint16_t{2}, intake.LastAppliedSequence(MINE));
  }
};

} // namespace GameLogicTests
