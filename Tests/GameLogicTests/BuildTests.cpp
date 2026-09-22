#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] int Code(Outpost::BuildRejection _rejection)
{
  return static_cast<int>(_rejection);
}

/// A world with a station on each anchor, which is what `Host::BeginMatch` makes and what the build
/// system needs to have somewhere to put a ship.
void Seat(Outpost::World& _world, std::size_t _players)
{
  for (const Outpost::Placement& placed : Outpost::GenerateLayout(0, _players))
  {
    static_cast<void>(_world.Create(placed.position, placed.heading, placed.design, placed.owner));
  }
}

/// Runs the build system until the player's item finishes, or gives up. Returns the ticks taken.
[[nodiscard]] std::uint32_t RunToCompletion(Outpost::BuildSystem& _build, Outpost::World& _world, Outpost::PlayerId _player)
{
  for (std::uint32_t tick = 1; tick <= 100000; ++tick)
  {
    _build.Advance(_world);
    if (!_build.Item(_player).active)
    {
      return tick;
    }
  }
  return 0;
}
} // namespace

/// M1.6. `GameDesign.md` section 5: a design selected, credits deducted **when the item starts**, the
/// ship appearing at the spawn point when it finishes.
TEST_CLASS(TheBuild)
{
public:
  TEST_METHOD(APlayerStartsWithAThousandCredits)
  {
    Outpost::BuildSystem build;
    build.Begin(2);
    Assert::AreEqual(1000u, build.Credits(MINE));
    Assert::AreEqual(1000u, build.Credits(THEIRS));
    Assert::AreEqual(0u, build.Credits(Outpost::NO_PLAYER));
    Assert::AreEqual(0u, build.Credits(3), L"a player this match does not have holds nothing");
  }

  /// **CREDITS COME OFF WHEN THE ITEM STARTS**, which is the design's wording and is what makes a
  /// cancel a refund rather than a no-op.
  TEST_METHOD(CreditsAreDeductedAtTheStart)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.Start(world, MINE, Outpost::DesignId::Miner)));
    Assert::AreEqual(1000u - 150u, build.Credits(MINE));
    Assert::IsTrue(build.Item(MINE).active);
    Assert::AreEqual(150u, build.Item(MINE).creditsSpent);
  }

  /// **REFUSED AT THE HOST AND NOT AT THE CLIENT ALONE** -- M1.6's exit criterion, and R19's whole
  /// point. Nothing is deducted and nothing starts.
  TEST_METHOD(AnUnaffordableOrderIsRefusedAtTheHost)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    // Three fighters is 900, which leaves 100 against a fourth at 300.
    for (int order = 0; order < 3; ++order)
    {
      Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.Start(world, MINE, Outpost::DesignId::Fighter)));
      static_cast<void>(RunToCompletion(build, world, MINE));
    }
    Assert::AreEqual(100u, build.Credits(MINE));

    Assert::AreEqual(Code(Outpost::BuildRejection::Unaffordable), Code(build.Start(world, MINE, Outpost::DesignId::Fighter)));
    Assert::AreEqual(100u, build.Credits(MINE), L"a refused order must not have spent anything");
    Assert::IsFalse(build.Item(MINE).active);
  }

  /// A station is placed by the generator and is not a row in a build menu (`GameCore/Design.h`).
  TEST_METHOD(AStationCannotBeOrdered)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    Assert::AreEqual(Code(Outpost::BuildRejection::NotBuildable), Code(build.Start(world, MINE, Outpost::DesignId::Station)));
    Assert::AreEqual(1000u, build.Credits(MINE));
  }

  TEST_METHOD(ADesignThisBuildDoesNotKnowIsRefused)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    Assert::AreEqual(Code(Outpost::BuildRejection::UnknownDesign), Code(build.Start(world, MINE, static_cast<Outpost::DesignId>(77))));
  }

  TEST_METHOD(APlayerWithNoStationCannotBuild)
  {
    Outpost::World world;
    Outpost::BuildSystem build;
    build.Begin(2);

    Assert::AreEqual(Code(Outpost::BuildRejection::NoStation), Code(build.Start(world, MINE, Outpost::DesignId::Miner)));
    Assert::AreEqual(Code(Outpost::BuildRejection::NoPlayer), Code(build.Start(world, Outpost::NO_PLAYER, Outpost::DesignId::Miner)));
  }

  /// **MONOTONIC IN TICKS AND IT REACHES COMPLETION EXACTLY** -- M1.6's exit criterion. The elapsed
  /// count goes up by one a tick, never skips and never overshoots.
  TEST_METHOD(ProgressIsMonotonicAndCompletesExactly)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));

    const std::uint32_t required = build.Item(MINE).ticksRequired;
    Assert::IsTrue(required > 0);

    std::uint32_t previous = 0;
    for (std::uint32_t tick = 1; tick < required; ++tick)
    {
      build.Advance(world);
      const std::uint32_t elapsed = build.Item(MINE).ticksElapsed;
      Assert::AreEqual(previous + 1, elapsed, L"a tick did not advance the item by exactly one");
      previous = elapsed;
      Assert::IsTrue(build.Item(MINE).active, L"it finished early");
    }

    build.Advance(world);
    Assert::IsFalse(build.Item(MINE).active, L"it did not finish on the tick it was due");
  }

  /// The ship arrives, owned, at full hull, in front of the station and clear of it.
  TEST_METHOD(AFinishedShipAppearsInFrontOfTheStation)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    const std::size_t before = world.AliveCount();
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));

    Assert::AreEqual(before + 1, world.AliveCount());

    // The newest entity is the one that just appeared.
    const Outpost::Entity* ship = nullptr;
    for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
    {
      if (world.IsSlotAlive(slot) && (world.EntityInSlot(slot).design == Outpost::DesignId::Miner))
      {
        ship = &world.EntityInSlot(slot);
      }
    }
    Assert::IsNotNull(ship);
    Assert::AreEqual(static_cast<int>(MINE), static_cast<int>(ship->owner));
    Assert::AreEqual(450, static_cast<int>(ship->hullRemaining), L"a new ship is undamaged");

    // Clear of the station: half of each hull's size, which is 110 plus 30.
    const Neuron::Vec2 anchor = Outpost::StartAnchor(2, MINE);
    const std::int64_t offset = Neuron::Sqrt(Neuron::LengthSquared(ship->position - anchor)) / Neuron::FIXED_ONE;
    Assert::AreEqual(static_cast<std::int64_t>(140), offset);

    // Toward the center, because that is where the station faces.
    Assert::IsTrue(Neuron::LengthSquared(ship->position) < Neuron::LengthSquared(anchor), L"it appeared behind the station");
  }

  /// **NO RALLY POINT** (`GameDesign.md` section 5): a new ship sits where it appears.
  TEST_METHOD(ANewShipHasNoOrder)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));

    for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
    {
      if (world.IsSlotAlive(slot) && (world.EntityInSlot(slot).design == Outpost::DesignId::Miner))
      {
        Assert::IsFalse(world.OrderInSlot(slot).active);
      }
    }
  }

  /// Two players build independently, and the completion order is player order rather than whichever
  /// happened to start first (R16).
  TEST_METHOD(TwoPlayersBuildIndependently)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    static_cast<void>(build.Start(world, THEIRS, Outpost::DesignId::Fighter));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));

    Assert::AreEqual(1000u - 300u, build.Credits(THEIRS));
    Assert::AreEqual(1000u - 150u, build.Credits(MINE));
    Assert::IsTrue(build.Item(MINE).ticksRequired < build.Item(THEIRS).ticksRequired);
  }
};

/// Q35, answered 2026-09-22: **full refund, on both paths.**
TEST_CLASS(CancellingAndReplacing)
{
public:
  TEST_METHOD(ACancelRefundsInFull)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));
    Assert::AreEqual(700u, build.Credits(MINE));

    // Part way through, so the refund is plainly not "nothing was spent yet".
    for (int tick = 0; tick < 20; ++tick)
    {
      build.Advance(world);
    }
    Assert::IsTrue(build.Item(MINE).ticksElapsed > 0);

    Assert::IsTrue(build.Cancel(MINE));
    Assert::AreEqual(1000u, build.Credits(MINE));
    Assert::IsFalse(build.Item(MINE).active);
  }

  /// **THE SHARPER CASE, AND IT IS NOT THE CANCEL.** The design handoff keeps all six build buttons
  /// live while something is building, so a tap on any of them replaces the item -- one tap on a
  /// 96-pixel button a player is already using, where the cancel target has to be aimed at.
  TEST_METHOD(AReplacementRefundsTheDisplacedItem)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));
    for (int tick = 0; tick < 30; ++tick)
    {
      build.Advance(world);
    }

    Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.Start(world, MINE, Outpost::DesignId::Miner)));
    Assert::AreEqual(1000u - 150u, build.Credits(MINE), L"the displaced fighter's credits are gone");
    Assert::IsTrue(build.Item(MINE).design == Outpost::DesignId::Miner);
    Assert::AreEqual(0u, build.Item(MINE).ticksElapsed, L"the new item started over");
  }

  /// **THE REFUND HAPPENS BEFORE THE NEW COST IS CHECKED**, so replacing a design with itself always
  /// works. Checked the other way round it would refuse a replacement the player can obviously afford.
  TEST_METHOD(ReplacingADesignWithItselfAlwaysWorks)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    // Spend down to exactly one fighter's worth.
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));
    Assert::AreEqual(100u, build.Credits(MINE));

    Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.Start(world, MINE, Outpost::DesignId::Fighter)));
    Assert::AreEqual(100u, build.Credits(MINE));
  }

  /// A replacement the player cannot afford cancels what was building and takes the refund with it,
  /// which is what a tap on a grayed button asks for. Nothing is lost either way.
  TEST_METHOD(AnUnaffordableReplacementStillRefunds)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    static_cast<void>(RunToCompletion(build, world, MINE));

    // Six miners at 150 is 900 spent, 100 left, and one more miner started would be 150.
    Assert::AreEqual(100u, build.Credits(MINE));
    Assert::AreEqual(Code(Outpost::BuildRejection::Unaffordable), Code(build.Start(world, MINE, Outpost::DesignId::Miner)));
    Assert::AreEqual(100u, build.Credits(MINE));
  }

  /// A cancel with nothing building is not an error: a tap on a cancel target that has already
  /// completed is ordinary.
  TEST_METHOD(ACancelWithNothingBuildingIsNotAnError)
  {
    Outpost::BuildSystem build;
    build.Begin(2);
    Assert::IsFalse(build.Cancel(MINE));
    Assert::IsFalse(build.Cancel(Outpost::NO_PLAYER));
    Assert::AreEqual(1000u, build.Credits(MINE));
  }
};

/// Q47's build rate, and the arithmetic the figure was chosen by.
TEST_CLASS(TheBuildRate)
{
public:
  /// A Miner is 150 credits at 20 a second, which is 7.5 seconds -- 150 ticks. A Fighter is twice
  /// that. Both divide exactly, so neither figure depends on the rounding.
  TEST_METHOD(TheTwoShippedDesignsTakeSevenAndAHalfAndFifteenSeconds)
  {
    Assert::AreEqual(150u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner));
    Assert::AreEqual(300u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Fighter));

    Assert::AreEqual(20u, Outpost::BuildSystem::TICKS_PER_SECOND);
    Assert::AreEqual(20u, Outpost::BuildSystem::BUILD_RATE_CREDITS_PER_SECOND);
  }

  /// **THE OPENING IS WHAT THE RATE WAS CHOSEN BY.** A station starts with 1,000 credits and a
  /// running economy is about six miners; spending that bank on six of them takes 45 seconds against
  /// a match of five minutes, so the first minute is spending what you started with. That is the
  /// derivation, and it is asserted rather than left in a comment.
  TEST_METHOD(TheOpeningBankBuysSixMinersInUnderAMinute)
  {
    const std::uint32_t miners = STARTING_CREDITS_FOR_TEST / Outpost::Derive(Outpost::DesignId::Miner).cost;
    Assert::AreEqual(6u, miners);

    const std::uint32_t ticks = miners * Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner);
    const std::uint32_t seconds = ticks / Outpost::BuildSystem::TICKS_PER_SECOND;
    Assert::AreEqual(45u, seconds);
    Assert::IsTrue(seconds < 60);
  }

  /// `GameDesign.md` section 5's shipyard levels, through the parameter M2's modules will pass.
  /// **Nothing passes anything but 100 yet** and the mechanism is pinned so M2 inherits it working.
  TEST_METHOD(AShipyardMultiplierShortensTheBuild)
  {
    const std::uint32_t plain = Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Fighter, 100);
    const std::uint32_t first = Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Fighter, 150);
    const std::uint32_t second = Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Fighter, 200);

    Assert::AreEqual(300u, plain);
    Assert::AreEqual(200u, first);
    Assert::AreEqual(150u, second);

    // The catalog's own figures, so a change to either level is caught here.
    Assert::AreEqual(150, static_cast<int>(Outpost::Component(Outpost::ComponentId::ShipyardL1).multiplierPercent));
    Assert::AreEqual(200, static_cast<int>(Outpost::Component(Outpost::ComponentId::ShipyardL2).multiplierPercent));
  }

  /// A design with no cost would otherwise complete before it started.
  TEST_METHOD(EveryDesignTakesAtLeastOneTick)
  {
    for (const Outpost::DesignEntry& design : Outpost::Designs())
    {
      Assert::IsTrue(Outpost::BuildSystem::TicksToBuild(design.id) >= 1);
    }
    Assert::AreEqual(1u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner, 1000000));
  }

  /// A multiplier of zero would divide by zero. It reads as "no shipyard" instead.
  TEST_METHOD(AZeroMultiplierIsNoShipyard)
  {
    Assert::AreEqual(Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner, 100),
                     Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner, 0));
  }

private:
  static constexpr std::uint32_t STARTING_CREDITS_FOR_TEST = Outpost::STARTING_CREDITS;
};

/// ADR-003's two per-player build bytes -- Q21's whole answer to the queue that had no wire record.
TEST_CLASS(TheBuildOnTheWire)
{
public:
  /// **ZERO MEANS NOTHING IS BUILDING, SO A DESIGN IS ITS IDENTITY PLUS ONE.** `DesignId::Miner` is
  /// zero, so the raw value could not have been used: a player building a miner would read as a
  /// player building nothing.
  TEST_METHOD(NothingBuildingIsZeroAndADesignIsItsIdentityPlusOne)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    Assert::AreEqual(std::uint8_t{0}, build.WireBuildingDesign(MINE));
    Assert::AreEqual(std::uint8_t{0}, build.WireProgressPercent(MINE));

    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Miner));
    Assert::AreEqual(static_cast<std::uint8_t>(static_cast<std::uint8_t>(Outpost::DesignId::Miner) + 1), build.WireBuildingDesign(MINE));
    Assert::AreEqual(std::uint8_t{1}, build.WireBuildingDesign(MINE));
  }

  /// **0 TO 99 WHILE BUILDING, AND NEVER 100.** The item is gone on the tick it completes, so a
  /// hundred would mean "finished and still here", which is not a state this system has.
  TEST_METHOD(ProgressRunsToNinetyNineAndThenTheItemIsGone)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));

    std::uint8_t highest = 0;
    std::uint8_t previous = 0;
    const std::uint32_t required = build.Item(MINE).ticksRequired;
    for (std::uint32_t tick = 1; tick < required; ++tick)
    {
      build.Advance(world);
      const std::uint8_t percent = build.WireProgressPercent(MINE);
      Assert::IsTrue(percent >= previous, L"the percentage went backwards");
      Assert::IsTrue(percent <= 99);
      previous = percent;
      highest = percent;
    }
    Assert::AreEqual(std::uint8_t{99}, highest);

    build.Advance(world);
    Assert::AreEqual(std::uint8_t{0}, build.WireProgressPercent(MINE));
    Assert::AreEqual(std::uint8_t{0}, build.WireBuildingDesign(MINE));
  }

  /// The block a snapshot carries, end to end.
  TEST_METHOD(TheSnapshotCarriesTheCreditsAndTheItem)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);
    static_cast<void>(build.Start(world, THEIRS, Outpost::DesignId::Fighter));
    build.Advance(world);

    const Outpost::Snapshot snapshot = Outpost::BuildSnapshot(world, intake, build, 0, 0, 2);
    Assert::AreEqual(static_cast<std::size_t>(2), snapshot.players.size());
    Assert::AreEqual(1000u, snapshot.players[0].credits);
    Assert::AreEqual(700u, snapshot.players[1].credits);
    Assert::AreEqual(std::uint8_t{0}, snapshot.players[0].buildingDesign);
    Assert::AreEqual(static_cast<std::uint8_t>(static_cast<std::uint8_t>(Outpost::DesignId::Fighter) + 1),
                     snapshot.players[1].buildingDesign);
  }
};

/// M1.6 made a build an ORDER, which is what puts the affordability check on the host.
TEST_CLASS(BuildOrders)
{
public:
  TEST_METHOD(ABuildOrderStartsAnItem)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);

    const Outpost::Command order{.sequence = 1,
                                 .type = Outpost::CommandType::Build,
                                 .targetX = static_cast<std::int16_t>(Outpost::DesignId::Fighter),
                                 .targetY = 0,
                                 .selection = {}};
    Assert::IsTrue(intake.Apply(world, build, MINE, order) == Outpost::CommandRejection::None);
    Assert::IsTrue(build.Item(MINE).active);
    Assert::IsTrue(build.Item(MINE).design == Outpost::DesignId::Fighter);
  }

  TEST_METHOD(ACancelOrderRefunds)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);
    static_cast<void>(build.Start(world, MINE, Outpost::DesignId::Fighter));

    const Outpost::Command order{.sequence = 1, .type = Outpost::CommandType::CancelBuild, .targetX = 0, .targetY = 0, .selection = {}};
    Assert::IsTrue(intake.Apply(world, build, MINE, order) == Outpost::CommandRejection::None);
    Assert::AreEqual(1000u, build.Credits(MINE));
  }

  /// **THE ACKNOWLEDGMENT ADVANCES EVEN ON A REFUSAL**, for the reason an `Attack` that resolves
  /// nothing does: the host understood the order, and a sequence that did not advance would have the
  /// client repeat it forever.
  TEST_METHOD(ARefusedBuildStillAcknowledges)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);

    const Outpost::Command order{.sequence = 9,
                                 .type = Outpost::CommandType::Build,
                                 .targetX = static_cast<std::int16_t>(Outpost::DesignId::Station),
                                 .targetY = 0,
                                 .selection = {}};
    Assert::IsTrue(intake.Apply(world, build, MINE, order) == Outpost::CommandRejection::BuildRefused);
    Assert::AreEqual(std::uint16_t{9}, intake.LastAppliedSequence(MINE));
  }

  /// **THE SELECTION HAS TO MATCH THE TYPE, IN BOTH DIRECTIONS.** A build carrying identities is the
  /// amplification Q24 is about, arriving through a door that did not exist when Q24 was written.
  TEST_METHOD(ABuildCarryingASelectionIsMalformed)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);

    const Outpost::Command order{.sequence = 1,
                                 .type = Outpost::CommandType::Build,
                                 .targetX = static_cast<std::int16_t>(Outpost::DesignId::Miner),
                                 .targetY = 0,
                                 .selection = {1, 2, 3}};
    Assert::IsTrue(intake.Apply(world, build, MINE, order) == Outpost::CommandRejection::Empty);
    Assert::IsFalse(build.Item(MINE).active);
  }

  /// Rubbish in the high byte still names the design the client meant.
  TEST_METHOD(OnlyTheLowByteOfTheTargetNamesTheDesign)
  {
    const Outpost::Command order{.sequence = 1,
                                 .type = Outpost::CommandType::Build,
                                 .targetX = static_cast<std::int16_t>(0x7F00 | static_cast<int>(Outpost::DesignId::Fighter)),
                                 .targetY = 0,
                                 .selection = {}};
    Assert::AreEqual(static_cast<std::uint8_t>(Outpost::DesignId::Fighter), order.TargetDesign());
  }

  /// The two new types round trip on the wire like the other two.
  TEST_METHOD(BothNewTypesRoundTrip)
  {
    for (const Outpost::CommandType type : {Outpost::CommandType::Build, Outpost::CommandType::CancelBuild})
    {
      Assert::IsTrue(Outpost::IsKnown(type));
      Assert::IsFalse(Outpost::ActsOnSelection(type));

      Outpost::CommandPacket packet{.sequence = 3, .player = MINE, .commands = {}};
      packet.commands.push_back(Outpost::Command{.sequence = 1, .type = type, .targetX = 1, .targetY = 0, .selection = {}});

      std::vector<std::byte> bytes(Outpost::EncodedSize(packet));
      Neuron::ByteWriter writer{bytes};
      Assert::IsTrue(Outpost::Encode(packet, writer));

      Neuron::ByteReader reader{bytes};
      Outpost::CommandPacket read;
      Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::CommandFault::None);
      Assert::AreEqual(static_cast<std::size_t>(1), read.commands.size());
      Assert::IsTrue(read.commands[0].type == type);
    }
  }
};

} // namespace GameLogicTests
