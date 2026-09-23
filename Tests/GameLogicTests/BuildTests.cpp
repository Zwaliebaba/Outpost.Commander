#include "pch.h"

#include <utility>
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

/// The player's station, which every placement is measured from.
[[nodiscard]] const Outpost::Entity& StationOf(const Outpost::World& _world, Outpost::PlayerId _player)
{
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    const Outpost::Entity& entity = _world.EntityInSlot(slot);
    if (_world.IsSlotAlive(slot) && (entity.owner == _player) && (entity.design == Outpost::DesignId::Station))
    {
      return entity;
    }
  }
  return _world.EntityInSlot(0);
}

/// A point offset from the player's station, in whole world units.
[[nodiscard]] Neuron::Vec2 NearStation(const Outpost::World& _world, Outpost::PlayerId _player, std::int32_t _dx, std::int32_t _dy)
{
  const Neuron::Vec2 station = StationOf(_world, _player).position;
  return Neuron::Vec2{.x = station.x + (_dx * Neuron::FIXED_ONE), .y = station.y + (_dy * Neuron::FIXED_ONE)};
}

/// The one module this player owns, or `NO_ENTITY`.
[[nodiscard]] Outpost::EntityId OnlyModuleOf(const Outpost::World& _world, Outpost::PlayerId _player)
{
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    const Outpost::Entity& entity = _world.EntityInSlot(slot);
    if (_world.IsSlotAlive(slot) && (entity.owner == _player) && Outpost::IsModule(entity.design))
    {
      return entity.id;
    }
  }
  return Outpost::NO_ENTITY;
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

  /// `GameDesign.md` section 5's shipyard levels, through the parameter the intake passes since M2.12
  /// (`ModuleEffectTests` pins where it comes from and which way it rounds).
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

  /// The block an update carries, end to end -- each player's own, since ADR-024 sends nobody else's.
  TEST_METHOD(EachPlayersOwnBlockCarriesTheCreditsAndTheItem)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);
    static_cast<void>(build.Start(world, THEIRS, Outpost::DesignId::Fighter));
    build.Advance(world);

    const Outpost::PlayerBlock mine = Outpost::PlayerBlockFor(intake, build, MINE);
    const Outpost::PlayerBlock theirs = Outpost::PlayerBlockFor(intake, build, THEIRS);
    Assert::AreEqual(1000u, mine.credits);
    Assert::AreEqual(700u, theirs.credits);
    Assert::AreEqual(std::uint8_t{0}, mine.buildingDesign);
    Assert::AreEqual(static_cast<std::uint8_t>(static_cast<std::uint8_t>(Outpost::DesignId::Fighter) + 1), theirs.buildingDesign);
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

/// M2.11. **A module is built through the station's one queue and appears where it was placed**; an L2 comes
/// about by upgrading an L1 in place, at the difference (`OpenQuestions.md` Q54).
TEST_CLASS(BuildingModules)
{
public:
  TEST_METHOD(APlacedModuleAppearsAtItsSiteWhenItFinishes)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);

    const Neuron::Vec2 site = NearStation(world, MINE, 250, 250);
    Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.StartModule(world, MINE, Outpost::DesignId::ModuleShipyardL1, site)));
    Assert::AreEqual(600u, build.Credits(MINE), L"section 5's 400, deducted at the start");
    Assert::AreEqual(400u, build.Item(MINE).ticksRequired, L"400 credits at 20 a second is 20 seconds");

    Assert::AreEqual(400u, RunToCompletion(build, world, MINE));
    const Outpost::Entity* module = world.Find(OnlyModuleOf(world, MINE));
    Assert::IsNotNull(module);
    Assert::IsTrue(module->design == Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(module->position == site);
    Assert::AreEqual(StationOf(world, MINE).heading, module->heading, L"it faces the way the station does");
  }

  /// **THE EXIT CRITERION: A REFUSED PLACEMENT LEAVES THE CREDITS UNSPENT** -- and what was building still
  /// building, because the site is judged before the queue is touched.
  TEST_METHOD(ARefusedPlacementSpendsNothingAndCancelsNothing)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.Start(world, MINE, Outpost::DesignId::Fighter)));
    const Outpost::BuildItem before = build.Item(MINE);

    for (const Neuron::Vec2 site : {NearStation(world, MINE, 0, 0), NearStation(world, MINE, 401, 0)})
    {
      Assert::AreEqual(Code(Outpost::BuildRejection::IllegalSite),
                       Code(build.StartModule(world, MINE, Outpost::DesignId::ModuleOreProcessorL1, site)));
      Assert::AreEqual(700u, build.Credits(MINE));
      Assert::IsTrue(build.Item(MINE) == before);
    }
  }

  TEST_METHOD(OnlyAFirstLevelModuleIsPlaced)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Neuron::Vec2 site = NearStation(world, MINE, 300, 0);

    Assert::AreEqual(Code(Outpost::BuildRejection::NotUpgradeable),
                     Code(build.StartModule(world, MINE, Outpost::DesignId::ModuleShipyardL2, site)),
                     L"an L2 is upgraded into, not placed");
    Assert::AreEqual(Code(Outpost::BuildRejection::NotBuildable), Code(build.StartModule(world, MINE, Outpost::DesignId::Miner, site)));
    Assert::AreEqual(Code(Outpost::BuildRejection::UnknownDesign),
                     Code(build.StartModule(world, MINE, static_cast<Outpost::DesignId>(200), site)));
    Assert::AreEqual(1000u, build.Credits(MINE));
  }

  /// **FOUR TO A STATION**, and the host is what holds that line.
  TEST_METHOD(AFifthModuleIsRefused)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Neuron::Angle heading = StationOf(world, MINE).heading;
    for (const auto& [dx, dy] : {std::pair{250, 250}, std::pair{-250, 250}, std::pair{-250, -250}, std::pair{250, -250}})
    {
      static_cast<void>(world.Create(NearStation(world, MINE, dx, dy), heading, Outpost::DesignId::ModuleOreProcessorL1, MINE));
    }

    Assert::AreEqual(Code(Outpost::BuildRejection::IllegalSite),
                     Code(build.StartModule(world, MINE, Outpost::DesignId::ModuleShipyardL1, NearStation(world, MINE, 0, 350))));
    Assert::AreEqual(1000u, build.Credits(MINE));
  }

  /// **ANOTHER PLAYER'S MODULES DO NOT CROWD YOURS**: the site is checked against your own.
  TEST_METHOD(OnlyYourOwnModulesBlockASite)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Neuron::Vec2 site = NearStation(world, MINE, 300, 0);
    static_cast<void>(world.Create(site, 0, Outpost::DesignId::ModuleShipyardL1, THEIRS));

    Assert::AreEqual(Code(Outpost::BuildRejection::None), Code(build.StartModule(world, MINE, Outpost::DesignId::ModuleShipyardL1, site)));
  }

  /// **AN UPGRADE PAYS THE DIFFERENCE AND CHANGES THE MODULE IN PLACE** (Q54): the same identity at the same
  /// point, a new level, 300 credits and the build time of 300.
  TEST_METHOD(AnUpgradePaysTheDifferenceAndKeepsTheModule)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Neuron::Vec2 site = NearStation(world, MINE, -300, 0);
    static_cast<void>(build.StartModule(world, MINE, Outpost::DesignId::ModuleShipyardL1, site));
    static_cast<void>(RunToCompletion(build, world, MINE));
    const Outpost::EntityId module = OnlyModuleOf(world, MINE);

    Assert::AreEqual(Code(Outpost::BuildRejection::None),
                     Code(build.StartUpgrade(world, MINE, module, Outpost::DesignId::ModuleShipyardL2)));
    Assert::AreEqual(300u, build.Credits(MINE));
    Assert::IsTrue(build.Item(MINE).design == Outpost::DesignId::ModuleShipyardL2, L"the wire shows the level it becomes");
    Assert::AreEqual(300u, build.Item(MINE).ticksRequired);
    Assert::IsTrue(world.Find(module)->design == Outpost::DesignId::ModuleShipyardL1, L"not until it finishes");

    Assert::AreEqual(300u, RunToCompletion(build, world, MINE));
    Assert::IsTrue(world.Find(module)->design == Outpost::DesignId::ModuleShipyardL2);
    Assert::IsTrue(world.Find(module)->position == site);
    Assert::IsTrue(OnlyModuleOf(world, MINE) == module, L"no second entity");
  }

  TEST_METHOD(AnUpgradeOfTheWrongThingIsRefusedAndSpendsNothing)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Outpost::EntityId mine = world.Create(NearStation(world, MINE, 300, 0), 0, Outpost::DesignId::ModuleShipyardL1, MINE);
    const Outpost::EntityId theirs = world.Create(NearStation(world, THEIRS, 300, 0), 0, Outpost::DesignId::ModuleShipyardL1, THEIRS);

    Assert::AreEqual(Code(Outpost::BuildRejection::NotUpgradeable),
                     Code(build.StartUpgrade(world, MINE, mine, Outpost::DesignId::ModuleOreProcessorL2)), L"the other kind");
    Assert::AreEqual(Code(Outpost::BuildRejection::NotUpgradeable),
                     Code(build.StartUpgrade(world, MINE, theirs, Outpost::DesignId::ModuleShipyardL2)), L"somebody else's");
    Assert::AreEqual(Code(Outpost::BuildRejection::NotUpgradeable),
                     Code(build.StartUpgrade(world, MINE, StationOf(world, MINE).id, Outpost::DesignId::ModuleShipyardL2)));
    Assert::AreEqual(Code(Outpost::BuildRejection::NotUpgradeable),
                     Code(build.StartUpgrade(world, MINE, Outpost::NO_ENTITY, Outpost::DesignId::ModuleShipyardL2)));
    Assert::AreEqual(1000u, build.Credits(MINE));
    Assert::IsFalse(build.Item(MINE).active);
  }

  /// **A MODULE THAT DIED WHILE ITS UPGRADE BUILT IS A REFUND**, as a station that died is.
  TEST_METHOD(AnUpgradeWhoseModuleIsGoneIsRefunded)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Outpost::EntityId module = world.Create(NearStation(world, MINE, 300, 0), 0, Outpost::DesignId::ModuleOreProcessorL1, MINE);
    static_cast<void>(build.StartUpgrade(world, MINE, module, Outpost::DesignId::ModuleOreProcessorL2));
    Assert::AreEqual(750u, build.Credits(MINE));
    static_cast<void>(world.Destroy(module));

    static_cast<void>(RunToCompletion(build, world, MINE));
    Assert::AreEqual(1000u, build.Credits(MINE));
    Assert::AreEqual(std::uint64_t{1}, build.StrandedCount());
  }

  /// Through the intake, both orders: the point and design of a placement, the identity and level of an upgrade.
  TEST_METHOD(BothModuleOrdersArriveThroughTheIntake)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);

    const Neuron::Vec2 site = NearStation(world, MINE, 0, -300);
    const Outpost::Command place{.sequence = 1,
                                 .type = Outpost::CommandType::PlaceModule,
                                 .targetX = Outpost::QuantizePosition(site.x),
                                 .targetY = Outpost::QuantizePosition(site.y),
                                 .placedDesign = static_cast<std::uint8_t>(Outpost::DesignId::ModuleOreProcessorL1)};
    Assert::IsTrue(intake.Apply(world, build, MINE, place) == Outpost::CommandRejection::None);
    Assert::IsTrue(build.Item(MINE).design == Outpost::DesignId::ModuleOreProcessorL1);
    static_cast<void>(RunToCompletion(build, world, MINE));
    const Outpost::EntityId module = OnlyModuleOf(world, MINE);
    Assert::IsTrue(module.IsValid());

    Outpost::Command upgrade{.sequence = 2, .type = Outpost::CommandType::UpgradeModule};
    upgrade.AimAtUpgrade(Outpost::PackIdentity(module.index, module.generation), Outpost::DesignId::ModuleOreProcessorL2);
    Assert::IsTrue(intake.Apply(world, build, MINE, upgrade) == Outpost::CommandRejection::None);
    Assert::IsTrue(build.Item(MINE).upgrade == module);

    // A refused placement is understood, so it still acknowledges -- and spends nothing.
    const std::uint32_t before = build.Credits(MINE);
    const Outpost::Command refused{.sequence = 3,
                                   .type = Outpost::CommandType::PlaceModule,
                                   .targetX = Outpost::QuantizePosition(site.x),
                                   .targetY = Outpost::QuantizePosition(site.y),
                                   .placedDesign = static_cast<std::uint8_t>(Outpost::DesignId::ModuleShipyardL1)};
    Assert::IsTrue(intake.Apply(world, build, MINE, refused) == Outpost::CommandRejection::BuildRefused);
    Assert::AreEqual(std::uint16_t{3}, intake.LastAppliedSequence(MINE));
    Assert::AreEqual(before, build.Credits(MINE));
  }

  /// **THE EXIT CRITERION: IT SURVIVES A RECONNECT**, which is the snapshot carrying it. A client that
  /// rejoins is a fresh accumulator's first fill, and the module is in it at its design and its site.
  TEST_METHOD(AModuleIsInTheSnapshotARejoiningClientIsSent)
  {
    Outpost::World world;
    Seat(world, 2);
    Outpost::BuildSystem build;
    build.Begin(2);
    const Neuron::Vec2 site = NearStation(world, MINE, 200, -300);
    static_cast<void>(build.StartModule(world, MINE, Outpost::DesignId::ModuleShipyardL1, site));
    static_cast<void>(RunToCompletion(build, world, MINE));
    const Outpost::EntityId module = OnlyModuleOf(world, MINE);

    Outpost::Accumulator rejoined;
    bool found = false;
    for (const Outpost::Update& update : rejoined.Fill(world, MINE, Outpost::PlayerBlock{}, 1))
    {
      for (const Outpost::EntityRecord& record : update.records)
      {
        if (record.identity == Outpost::PackIdentity(module.index, module.generation))
        {
          found = true;
          Assert::AreEqual(static_cast<int>(Outpost::DesignId::ModuleShipyardL1), static_cast<int>(record.designIdentity));
          Assert::AreEqual(Outpost::QuantizePosition(site.x), record.positionX);
          Assert::AreEqual(Outpost::QuantizePosition(site.y), record.positionY);
        }
      }
    }
    Assert::IsTrue(found);
  }
};

} // namespace GameLogicTests
