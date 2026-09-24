#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;

[[nodiscard]] Neuron::Vec2 At(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE};
}

[[nodiscard]] Outpost::Placement RockAt(std::int32_t _x, std::int32_t _y, Outpost::FieldKind _kind = Outpost::FieldKind::Home) noexcept
{
  return Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid, .field = _kind, .position = At(_x, _y)};
}

/// Movement and mining, as the host runs them.
void Step(Outpost::World& _world, Outpost::MiningSystem& _mining)
{
  Outpost::Tick(_world);
  _mining.Advance(_world);
}
} // namespace

/// M3.9, `OpenQuestions.md` Q69. **A rock runs out**, and its miners move on.
TEST_CLASS(TheFiniteOre)
{
public:
  /// 200 at home and 600 contested, from the field kind the generator gave the rock.
  TEST_METHOD(ARockStartsWithItsFieldsOre)
  {
    Outpost::World world;
    world.SetField({RockAt(0, 0), RockAt(100, 0, Outpost::FieldKind::Contested)});
    Assert::AreEqual(200u * Outpost::MILLI_ORE_PER_ORE, world.OreLeftMilliOre(0));
    Assert::AreEqual(600u * Outpost::MILLI_ORE_PER_ORE, world.OreLeftMilliOre(1));
    Assert::AreEqual(0u, world.OreLeftMilliOre(2), L"a rock the field does not have holds nothing");
  }

  /// **WHAT THE ROCK GIVES UP IS WHAT THE HOLD GAINS**, and never more than it had.
  TEST_METHOD(ExtractionTakesFromTheRock)
  {
    Outpost::World world;
    world.SetField({RockAt(2000, 0)});
    static_cast<void>(world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE));
    const Outpost::EntityId miner = world.Create(At(1900, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(world.OrderMine(miner, 0));
    Outpost::MiningSystem mining;
    for (int tick = 0; tick < 20; ++tick)
    {
      Step(world, mining);
    }
    Assert::AreEqual(200u * Outpost::MILLI_ORE_PER_ORE, world.OreLeftMilliOre(0) + world.FindMine(miner)->cargoMilliOre,
                     L"ore was made or lost between the rock and the hold");
  }

  /// **A SPENT ROCK SENDS ITS MINER TO THE NEAREST ROCK WITH ORE LEFT**, ties to the lower index, not to one that is
  /// spent however near.
  TEST_METHOD(ASpentRockRetargetsTheNearestWithOre)
  {
    Outpost::World world;
    world.SetField({RockAt(2000, 0), RockAt(2300, 0), RockAt(3500, 0), RockAt(2600, 0)});
    static_cast<void>(world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE));
    static_cast<void>(world.TakeOre(0, 200u * Outpost::MILLI_ORE_PER_ORE));
    static_cast<void>(world.TakeOre(1, 200u * Outpost::MILLI_ORE_PER_ORE));
    const Outpost::EntityId miner = world.Create(At(1900, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(world.OrderMine(miner, 0));

    Outpost::MiningSystem mining;
    mining.Advance(world);
    Assert::AreEqual(std::uint16_t{3}, world.FindMine(miner)->rock, L"it did not move on to the nearest rock with ore");
    Assert::IsTrue(world.FindMine(miner)->phase == Outpost::MiningPhase::ToOre);
  }

  /// With the whole field spent, a miner carrying ore takes it home, and one carrying nothing stops.
  TEST_METHOD(ASpentFieldSendsTheMinerHomeAndStops)
  {
    Outpost::World world;
    world.SetField({RockAt(2000, 0)});
    static_cast<void>(world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE));
    static_cast<void>(world.TakeOre(0, 200u * Outpost::MILLI_ORE_PER_ORE));
    const Outpost::EntityId empty = world.Create(At(1900, 0), 0, Outpost::DesignId::Miner, MINE);
    const Outpost::EntityId laden = world.Create(At(1900, 200), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(world.OrderMine(empty, 0));
    Assert::IsTrue(world.OrderMine(laden, 0));
    for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
    {
      if (world.IsSlotAlive(slot) && (world.EntityInSlot(slot).id == laden))
      {
        world.MineInSlot(slot).cargoMilliOre = 40u * Outpost::MILLI_ORE_PER_ORE;
      }
    }

    Outpost::MiningSystem mining;
    mining.Advance(world);
    Assert::IsTrue(world.FindMine(empty)->phase == Outpost::MiningPhase::None, L"an empty miner kept looking for ore");
    Assert::IsTrue(world.FindMine(laden)->phase == Outpost::MiningPhase::ToUnload, L"a laden miner did not take its ore home");
  }

  /// The hash sees what a rock has left, so two hosts that mined differently disagree.
  TEST_METHOD(TheHashSeesTheOreLeft)
  {
    Outpost::World first;
    first.SetField({RockAt(2000, 0)});
    Outpost::World second;
    second.SetField({RockAt(2000, 0)});
    static_cast<void>(second.TakeOre(0, 1));
    Assert::AreNotEqual(Outpost::StateHash(first), Outpost::StateHash(second));
  }
};

/// M3.9, Q69. **The forward depot**: placed, built, and unloaded at.
TEST_CLASS(TheDepot)
{
public:
  /// A legal site is charged 300 and builds a depot there; an illegal one is refused and charges nothing.
  TEST_METHOD(ADepotIsPlacedAndBuilt)
  {
    Outpost::World world;
    world.SetField({RockAt(0, 3000, Outpost::FieldKind::Contested)});
    static_cast<void>(world.Create(At(-6000, 0), 0, Outpost::DesignId::Station, MINE));
    static_cast<void>(world.Create(At(6000, 0), 0, Outpost::DesignId::Station, 2));
    Outpost::BuildSystem build;
    build.Begin(2);
    build.Grant(MINE, 1000);
    const std::uint32_t before = build.Credits(MINE);

    Assert::IsTrue(build.StartModule(world, MINE, Outpost::DesignId::Depot, At(-5000, 0)) == Outpost::BuildRejection::IllegalSite);
    Assert::AreEqual(before, build.Credits(MINE), L"a refused depot was charged");

    Assert::IsTrue(build.StartModule(world, MINE, Outpost::DesignId::Depot, At(0, 2400)) == Outpost::BuildRejection::None);
    Assert::AreEqual(before - 300u, build.Credits(MINE));
    for (int tick = 0; tick < 400; ++tick)
    {
      build.Advance(world);
    }
    bool built = false;
    for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
    {
      const Outpost::Entity& entity = world.EntityInSlot(slot);
      if (world.IsSlotAlive(slot) && (entity.design == Outpost::DesignId::Depot))
      {
        built = true;
        Assert::IsTrue(entity.position == At(0, 2400), L"the depot is not where it was placed");
      }
    }
    Assert::IsTrue(built, L"no depot was built");
  }

  /// **A THIRD IS REFUSED**, counting one still in the queue.
  TEST_METHOD(TwoAPlayerCountingTheQueued)
  {
    Outpost::World world;
    world.SetField({RockAt(0, 3000, Outpost::FieldKind::Contested)});
    static_cast<void>(world.Create(At(-6000, 0), 0, Outpost::DesignId::Station, MINE));
    Outpost::BuildSystem build;
    build.Begin(2);
    build.Grant(MINE, 2000);
    Assert::IsTrue(build.StartModule(world, MINE, Outpost::DesignId::Depot, At(0, 2400)) == Outpost::BuildRejection::None);
    Assert::IsTrue(build.StartModule(world, MINE, Outpost::DesignId::Depot, At(300, 2400)) == Outpost::BuildRejection::None);
    Assert::IsTrue(build.StartModule(world, MINE, Outpost::DesignId::Depot, At(-300, 2400)) == Outpost::BuildRejection::IllegalSite,
                   L"a third depot was accepted");
  }

  /// **A MINER UNLOADS AT THE NEAREST ACCEPTOR**, and a depot is one: nothing in the mining loop names it.
  TEST_METHOD(AMinerUnloadsAtTheNearerDepot)
  {
    Outpost::World world;
    world.SetField({RockAt(0, 3000, Outpost::FieldKind::Contested)});
    static_cast<void>(world.Create(At(-6000, 0), 0, Outpost::DesignId::Station, MINE));
    const Outpost::EntityId depot = world.Create(At(0, 2400), 0, Outpost::DesignId::Depot, MINE);
    const Outpost::EntityId miner = world.Create(At(0, 2900), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(world.OrderMine(miner, 0));
    Outpost::MiningSystem mining;
    for (int tick = 0; (tick < 600) && (world.FindMine(miner)->phase != Outpost::MiningPhase::Unloading); ++tick)
    {
      Step(world, mining);
    }
    Assert::IsTrue(world.FindMine(miner)->phase == Outpost::MiningPhase::Unloading, L"it never unloaded");
    Assert::IsTrue(world.FindMine(miner)->unloadTarget == depot, L"it flew past the depot to the station");
  }
};

/// The owner, 2026-09-24 (`OpenQuestions.md` Q82). **One miner to a rock**: a miner keeps its rock until it is spent,
/// and a group spreads out rather than queueing.
TEST_CLASS(OneMinerToARock)
{
public:
  /// A mine order for three: the nearest takes the tapped rock, and the other two the nearest rocks nobody has.
  TEST_METHOD(AGroupOrderSpreadsAcrossTheField)
  {
    Outpost::World world;
    world.SetField({RockAt(2000, 0), RockAt(2200, 0), RockAt(2000, 300), RockAt(5000, 0)});
    static_cast<void>(world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE));
    std::vector<Outpost::EntityId> miners;
    for (std::int32_t index = 0; index < 3; ++index)
    {
      miners.push_back(world.Create(At(1000 + (index * 100), 0), 0, Outpost::DesignId::Miner, MINE));
    }
    Outpost::Command order{.sequence = 1, .type = Outpost::CommandType::Mine};
    for (const Outpost::EntityId id : miners)
    {
      order.selection.push_back(Outpost::PackIdentity(id.index, id.generation));
    }
    order.AimAtRock(0);
    Outpost::BuildSystem build;
    build.Begin(2);
    Outpost::CommandIntake intake;
    Assert::IsTrue(intake.Apply(world, build, MINE, order) == Outpost::CommandRejection::None);

    std::vector<std::uint16_t> rocks;
    for (const Outpost::EntityId id : miners)
    {
      rocks.push_back(world.FindMine(id)->rock);
    }
    Assert::AreEqual(std::uint16_t{0}, rocks[2], L"the miner nearest the tapped rock did not take it");
    Assert::IsTrue((rocks[0] != rocks[1]) && (rocks[1] != rocks[2]) && (rocks[0] != rocks[2]), L"two miners share a rock");
    for (const std::uint16_t rock : rocks)
    {
      Assert::AreNotEqual(std::uint16_t{3}, rock, L"a far rock was taken while near ones were free");
    }
  }

  /// **A SPENT ROCK'S MINER GOES TO A ROCK NOBODY HAS**, even past a nearer one another miner is working.
  TEST_METHOD(ARetargetSkipsARockAnotherMinerHas)
  {
    Outpost::World world;
    world.SetField({RockAt(2000, 0), RockAt(2200, 0), RockAt(2600, 0)});
    static_cast<void>(world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE));
    static_cast<void>(world.TakeOre(0, 200u * Outpost::MILLI_ORE_PER_ORE));
    const Outpost::EntityId other = world.Create(At(2100, 300), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(world.OrderMine(other, 1));
    const Outpost::EntityId miner = world.Create(At(1900, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(world.OrderMine(miner, 0));

    Outpost::MiningSystem mining;
    mining.Advance(world);
    Assert::AreEqual(std::uint16_t{2}, world.FindMine(miner)->rock, L"it joined the miner at the nearer rock");
    Assert::AreEqual(std::uint16_t{1}, world.FindMine(other)->rock, L"the other miner was moved");
  }

  /// With every rock with ore taken, two share, at the least-worked and then the nearest.
  TEST_METHOD(OnlyAFullFieldIsShared)
  {
    Outpost::World world;
    world.SetField({RockAt(2000, 0), RockAt(2600, 0)});
    const std::vector<std::uint32_t> claims{1, 1};
    std::uint16_t rock = 99;
    Assert::IsTrue(Outpost::BestRock(world, claims, At(1900, 0), rock));
    Assert::AreEqual(std::uint16_t{0}, rock);
    const std::vector<std::uint32_t> lopsided{2, 1};
    Assert::IsTrue(Outpost::BestRock(world, lopsided, At(1900, 0), rock));
    Assert::AreEqual(std::uint16_t{1}, rock, L"the more crowded rock was chosen for being nearer");
  }
};

} // namespace GameLogicTests
