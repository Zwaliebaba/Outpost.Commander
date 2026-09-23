#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] Neuron::Vec2 At(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE};
}

[[nodiscard]] Outpost::Placement RockAt(std::int32_t _x, std::int32_t _y) noexcept
{
  return Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid, .field = Outpost::FieldKind::Home, .position = At(_x, _y)};
}

/// **THE GEOMETRY THE CYCLE'S TICK COUNT IS DERIVED FROM.** A station at the origin, one rock 2,000 units
/// east, and a `Miner` starting exactly at unloading reach -- 110 + 30 + 20 = 160 units east of the station.
/// The miner travels along the x axis at 100 units a second, five a tick, so every leg is a whole number of
/// ticks and nothing below depends on rounding.
struct Scene
{
  Outpost::World world;
  Outpost::MiningSystem mining;
  Outpost::EntityId station{};
  Outpost::EntityId miner{};

  Scene()
  {
    world.SetField({RockAt(2000, 0)});
    station = world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE);
    miner = world.Create(At(160, 0), 0, Outpost::DesignId::Miner, MINE);
  }

  /// One tick in `TechnicalDesign.md` section 2's order: movement, then mining.
  void Step()
  {
    Outpost::Tick(world);
    mining.Advance(world);
  }

  [[nodiscard]] const Outpost::MineOrder& Mine() const
  {
    return *world.FindMine(miner);
  }
};

[[nodiscard]] Outpost::Command MineCommand(std::uint16_t _sequence, std::uint16_t _rock, std::vector<Outpost::WireIdentity> _selection)
{
  Outpost::Command command{.sequence = _sequence, .type = Outpost::CommandType::Mine, .selection = std::move(_selection)};
  command.AimAtRock(_rock);
  return command;
}

[[nodiscard]] Outpost::WireIdentity Wire(Outpost::EntityId _id) noexcept
{
  return Outpost::PackIdentity(_id.index, _id.generation);
}

/// 1,000 thousandths of ore a tick from one laser, and 2,500 unloading: the design's figures at 20 Hz.
constexpr std::uint32_t FULL_HOLD_MILLI_ORE = 100 * Outpost::MILLI_ORE_PER_ORE;
} // namespace

/// M2.6. **The economy is a loop, and a loop is pinned by its tick count.**
TEST_CLASS(TheMiningLoop)
{
public:
  /// **THE FULL CYCLE IN THE TICKS THE DESIGN'S FIGURES IMPLY, THEN THE NEXT ONE WITH NO FURTHER ORDER.**
  /// Out: 1,640 units to come within the laser's 200 of the rock, at 5 a tick, is 328 ticks, and it extracts
  /// on the tick it arrives. Extracting: 100 ore at 20 a second is 100 ticks. Back: 1,640 units to come within
  /// reach of the station, 328 ticks, and it unloads on the tick it arrives. Unloading: 100 ore at 50 a second
  /// is 40 ticks. Then out again -- 794 ticks a cycle, and every one of them is arithmetic on section 4.
  ///
  /// **PLUS ONE TICK, ONCE, AT THE START.** Mining runs after movement, so a fresh order's first heading is
  /// set on its first tick and the miner starts moving on the second -- where a move order, headed at intake,
  /// moves on the tick it lands. Every later leg is re-issued inside the pass that ended the last one, so the
  /// offset never recurs.
  TEST_METHOD(ACycleTakesTheDesignsTicksAndTheNextStartsByItself)
  {
    Scene scene;
    Assert::IsTrue(scene.world.OrderMine(scene.miner, 0));

    std::uint32_t firstExtract = 0;
    std::uint32_t full = 0;
    std::uint32_t firstUnload = 0;
    std::uint32_t empty = 0;
    std::uint32_t secondExtract = 0;
    std::uint32_t extractTicks = 0;
    std::uint32_t unloadTicks = 0;
    std::uint64_t deliveredMilliOre = 0;

    std::uint32_t previousCargo = 0;
    for (std::uint32_t tick = 1; tick <= 1200; ++tick)
    {
      scene.Step();
      const std::uint32_t cargo = scene.Mine().cargoMilliOre;
      if (cargo > previousCargo)
      {
        ++extractTicks;
        if (firstExtract == 0)
        {
          firstExtract = tick;
        }
        else if ((empty != 0) && (secondExtract == 0))
        {
          secondExtract = tick;
        }
        if ((cargo == FULL_HOLD_MILLI_ORE) && (full == 0))
        {
          full = tick;
        }
      }
      if (cargo < previousCargo)
      {
        ++unloadTicks;
        if (firstUnload == 0)
        {
          firstUnload = tick;
        }
        if ((cargo == 0) && (empty == 0))
        {
          empty = tick;
        }
      }
      for (const Outpost::OreDelivery& delivery : scene.mining.Deliveries())
      {
        deliveredMilliOre += delivery.milliOre;
        Assert::IsTrue(delivery.acceptor == scene.station);
        Assert::AreEqual(static_cast<int>(MINE), static_cast<int>(delivery.player));
      }
      previousCargo = cargo;
    }

    Assert::AreEqual(1u + 328u, firstExtract, L"the outbound leg, after the one tick the order takes to head out");
    Assert::AreEqual(firstExtract + 99u, full, L"a hundred ticks of extraction, the arrival tick among them");
    Assert::AreEqual(full + 328u, firstUnload, L"the return leg");
    Assert::AreEqual(firstUnload + 39u, empty, L"forty ticks of unloading, the arrival tick among them");
    Assert::AreEqual(empty + 328u, secondExtract, L"the next cycle, with no order since the first");
    Assert::AreEqual(794u, secondExtract - firstExtract, L"a cycle");
    Assert::AreEqual(40u, unloadTicks);
    Assert::AreEqual(std::uint64_t{FULL_HOLD_MILLI_ORE}, deliveredMilliOre, L"one hold delivered, whole");
    Assert::IsTrue(scene.Mine().phase == Outpost::MiningPhase::Extracting, L"still mining: a mine order does not complete");
  }

  /// **EXTRACTION STOPS EXACTLY AT A FULL HOLD.** A miner starting half a tick short of full takes the half
  /// and not the whole tick's worth, and leaves for the station on that tick.
  TEST_METHOD(ExtractionStopsExactlyAtAFullHold)
  {
    Scene scene;
    const Outpost::EntityId closeMiner = scene.world.Create(At(1850, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(scene.world.OrderMine(closeMiner, 0));
    for (std::size_t slot = 0; slot < scene.world.SlotCount(); ++slot)
    {
      if (scene.world.EntityInSlot(slot).id == closeMiner)
      {
        scene.world.MineInSlot(slot).cargoMilliOre = FULL_HOLD_MILLI_ORE - 500;
      }
    }

    scene.Step();
    const Outpost::MineOrder& mine = *scene.world.FindMine(closeMiner);
    Assert::AreEqual(FULL_HOLD_MILLI_ORE, mine.cargoMilliOre, L"overshot, or fell short of, a full hold");
    Assert::IsTrue(mine.phase == Outpost::MiningPhase::ToUnload);
    Assert::IsTrue(scene.world.FindOrder(closeMiner)->active, L"a full miner leaves on the tick it fills");
  }

  /// **ORDERED ELSEWHERE MID-CYCLE, A MINER ABANDONS CLEANLY AND KEEPS WHAT IT CARRIES** -- and a second
  /// mine order picks the cycle up with the cargo aboard, so it fills in the fifty ticks it has left.
  TEST_METHOD(AMoveOrderAbandonsTheCycleAndKeepsTheCargo)
  {
    Scene scene;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);

    Assert::IsTrue(intake.Apply(scene.world, build, MINE, MineCommand(1, 0, {Wire(scene.miner)})) == Outpost::CommandRejection::None);
    for (std::uint32_t tick = 0; tick < 1 + 328 + 49; ++tick)
    {
      scene.Step();
    }
    Assert::AreEqual(50u * Outpost::MILLI_ORE_PER_ORE, scene.Mine().cargoMilliOre);

    Outpost::Command away{.sequence = 2, .type = Outpost::CommandType::MoveTo, .selection = {Wire(scene.miner)}};
    away.targetX = Outpost::QuantizePosition(At(1000, 1000).x);
    away.targetY = Outpost::QuantizePosition(At(1000, 1000).y);
    Assert::IsTrue(intake.Apply(scene.world, build, MINE, away) == Outpost::CommandRejection::None);
    Assert::IsTrue(scene.Mine().phase == Outpost::MiningPhase::None);

    for (std::uint32_t tick = 0; tick < 200; ++tick)
    {
      scene.Step();
    }
    Assert::AreEqual(50u * Outpost::MILLI_ORE_PER_ORE, scene.Mine().cargoMilliOre, L"the cargo was lost or changed off the loop");
    Assert::IsTrue(scene.mining.Deliveries().empty());

    Assert::IsTrue(intake.Apply(scene.world, build, MINE, MineCommand(3, 0, {Wire(scene.miner)})) == Outpost::CommandRejection::None);
    std::uint32_t extractTicks = 0;
    std::uint32_t previous = scene.Mine().cargoMilliOre;
    for (std::uint32_t tick = 0; tick < 600; ++tick)
    {
      scene.Step();
      if (scene.Mine().cargoMilliOre > previous)
      {
        ++extractTicks;
      }
      previous = scene.Mine().cargoMilliOre;
      if (scene.Mine().phase == Outpost::MiningPhase::ToUnload)
      {
        break;
      }
    }
    Assert::AreEqual(50u, extractTicks, L"the second order did not start from the cargo aboard");
  }

  /// **A FULL HOLD GOES STRAIGHT TO UNLOAD**, rather than sitting at the rock extracting nothing.
  TEST_METHOD(AMineOrderWithAFullHoldGoesToUnloadFirst)
  {
    Scene scene;
    Assert::IsTrue(scene.world.OrderMine(scene.miner, 0));
    for (std::size_t slot = 0; slot < scene.world.SlotCount(); ++slot)
    {
      if (scene.world.EntityInSlot(slot).id == scene.miner)
      {
        scene.world.MineInSlot(slot).cargoMilliOre = FULL_HOLD_MILLI_ORE;
      }
    }
    scene.Step();
    Assert::IsTrue(scene.Mine().phase == Outpost::MiningPhase::Unloading, L"at reach and full: it unloads at once");
    Assert::AreEqual(FULL_HOLD_MILLI_ORE - 2500, scene.Mine().cargoMilliOre);
  }
};

/// Where it unloads: a query, not a constant.
TEST_CLASS(TheUnloadTarget)
{
public:
  /// The nearest acceptor the miner's OWNER owns -- a nearer one belonging to somebody else is not a place
  /// to unload -- and of two at exactly equal distance, the lower identity.
  TEST_METHOD(TheNearestOwnedAcceptorWinsAndATieGoesToTheLowerIdentity)
  {
    Outpost::World world;
    const Outpost::EntityId west = world.Create(At(-1000, 0), 0, Outpost::DesignId::Station, MINE);
    static_cast<void>(world.Create(At(1000, 0), 0, Outpost::DesignId::Station, MINE));
    static_cast<void>(world.Create(At(100, 0), 0, Outpost::DesignId::Station, THEIRS));
    static_cast<void>(world.Create(At(0, 50), 0, Outpost::DesignId::Miner, MINE));

    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    std::vector<Outpost::EntityId> scratch;
    Assert::IsTrue(Outpost::FindUnloadTarget(world, grid, MINE, At(0, 0), scratch) == west);
    Assert::IsFalse(Outpost::FindUnloadTarget(world, grid, 3, At(0, 0), scratch).IsValid(), L"a player with no station has nowhere");
  }

  /// **REACH IS TOUCHING PLUS Q51's SLACK**, from the catalog's sizes: 110 + 30 + 20 for a `Scout` at a
  /// station.
  TEST_METHOD(ReachIsTheTwoHalfSizesAndTheSlack)
  {
    Assert::AreEqual(160 * Neuron::FIXED_ONE, Outpost::UnloadReach(Outpost::DesignId::Miner, Outpost::DesignId::Station));
    Assert::AreEqual(20, Outpost::UNLOAD_SLACK_UNITS);
    Assert::AreEqual(50u, Outpost::UNLOAD_ORE_PER_SECOND);
  }

  /// **NOWHERE TO UNLOAD: THE MINER WAITS, FULL**, and goes the moment somewhere exists -- the economy
  /// failing where a player can see it (`GameDesign.md` section 4).
  TEST_METHOD(AMinerWithNowhereToUnloadWaitsFull)
  {
    Scene scene;
    Assert::IsTrue(scene.world.Destroy(scene.station));
    Assert::IsTrue(scene.world.OrderMine(scene.miner, 0));
    for (std::uint32_t tick = 0; tick < 600; ++tick)
    {
      scene.Step();
    }
    Assert::AreEqual(FULL_HOLD_MILLI_ORE, scene.Mine().cargoMilliOre);
    Assert::IsTrue(scene.Mine().phase == Outpost::MiningPhase::ToUnload);
    Assert::IsFalse(scene.world.FindOrder(scene.miner)->active, L"a miner with nowhere to go should hold still");

    static_cast<void>(scene.world.Create(At(1600, 800), 0, Outpost::DesignId::Station, MINE));
    scene.Step();
    Assert::IsTrue(scene.world.FindOrder(scene.miner)->active, L"a new station was not found");
  }
};

/// What the host accepts as a mine order.
TEST_CLASS(TheMineCommand)
{
public:
  /// A rock the field does not have is refused whole, and nothing is ordered.
  TEST_METHOD(ARockPastTheFieldIsRefused)
  {
    Scene scene;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);
    Assert::IsTrue(intake.Apply(scene.world, build, MINE, MineCommand(1, 1, {Wire(scene.miner)})) == Outpost::CommandRejection::NoSuchRock);
    Assert::IsTrue(scene.Mine().phase == Outpost::MiningPhase::None);
  }

  /// **ONLY WHAT CAN MINE TAKES THE ORDER**: a fighter in the selection is skipped and the miner is not, and
  /// the command is still accepted.
  TEST_METHOD(AShipWithNoMiningToolIsSkipped)
  {
    Scene scene;
    const Outpost::EntityId fighter = scene.world.Create(At(300, 300), 0, Outpost::DesignId::Fighter, MINE);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);
    Assert::IsTrue(intake.Apply(scene.world, build, MINE, MineCommand(1, 0, {Wire(scene.miner), Wire(fighter)})) ==
                   Outpost::CommandRejection::None);
    Assert::IsTrue(scene.Mine().phase == Outpost::MiningPhase::ToOre);
    Assert::IsTrue(scene.world.FindMine(fighter)->phase == Outpost::MiningPhase::None);
  }

  /// Somebody else's miner is refused the way any foreign identity is.
  TEST_METHOD(AForeignMinerIsRefused)
  {
    Scene scene;
    const Outpost::EntityId theirs = scene.world.Create(At(-300, 0), 0, Outpost::DesignId::Miner, THEIRS);
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    build.Begin(2);
    Assert::IsTrue(intake.Apply(scene.world, build, MINE, MineCommand(1, 0, {Wire(theirs)})) == Outpost::CommandRejection::NotOwned);
    Assert::IsTrue(scene.world.FindMine(theirs)->phase == Outpost::MiningPhase::None);
  }
};

} // namespace GameLogicTests
