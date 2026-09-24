#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] Neuron::Vec2 Units(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(_x), .y = Neuron::FixedFromWholeUnits(_y)};
}

[[nodiscard]] Outpost::WireIdentity Wire(Outpost::EntityId _id) noexcept
{
  return Outpost::PackIdentity(_id.index, _id.generation);
}
} // namespace

/// M3.4. **Zero hull is death, on the tick it is reached.**
TEST_CLASS(TheDeathSystem)
{
public:
  /// Everything at zero goes, in slot order, and everything above zero stays.
  TEST_METHOD(ZeroHullDiesAndNothingElseDoes)
  {
    Outpost::World world;
    const Outpost::EntityId first = world.Create(Units(0, 0), 0, Outpost::DesignId::Miner, MINE);
    const Outpost::EntityId survivor = world.Create(Units(500, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId second = world.Create(Units(1000, 0), 0, Outpost::DesignId::Station, THEIRS);
    world.Find(first)->hullRemaining = 0;
    world.Find(survivor)->hullRemaining = 1;
    world.Find(second)->hullRemaining = 0;

    Outpost::DeathSystem deaths;
    deaths.Advance(world);

    Assert::AreEqual(std::size_t{2}, deaths.Died().size());
    Assert::IsTrue(deaths.Died()[0].id == first);
    Assert::IsTrue(deaths.Died()[1].id == second);
    Assert::IsTrue(deaths.Died()[1].design == Outpost::DesignId::Station, L"a station dies like anything else (R24)");
    Assert::AreEqual(THEIRS, deaths.Died()[1].owner);
    Assert::IsFalse(world.IsAlive(first));
    Assert::IsFalse(world.IsAlive(second));
    Assert::IsTrue(world.IsAlive(survivor), L"one point of hull is alive");
    Assert::AreEqual(std::size_t{1}, world.AliveCount());

    // And nothing dies twice.
    deaths.Advance(world);
    Assert::AreEqual(std::size_t{0}, deaths.Died().size());
  }

  /// M3.1's 258 ticks for a Fighter on a Miner, through the whole tick: the Miner is gone on the tick its hull
  /// reaches zero and not one later.
  TEST_METHOD(AMinerKilledInTheTickIsGoneThatTick)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    Outpost::DeathSystem deaths;
    static_cast<void>(world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE));
    const Outpost::EntityId miner = world.Create(Units(300, 0), 0, Outpost::DesignId::Miner, THEIRS);

    for (std::uint32_t tick = 0; tick < 257; ++tick)
    {
      weapons.Advance(world, tick);
      deaths.Advance(world);
      Assert::IsTrue(world.IsAlive(miner), L"it died early");
    }
    weapons.Advance(world, 257);
    deaths.Advance(world);
    Assert::IsFalse(world.IsAlive(miner), L"it outlived section 7's 12.9 seconds");
  }

  /// An attack order on something that died ends, rather than chasing an identity that no longer resolves.
  TEST_METHOD(AnAttackOnTheDeadEnds)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    Outpost::DeathSystem deaths;
    const Outpost::EntityId fighter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId target = world.Create(Units(3000, 0), 0, Outpost::DesignId::Miner, THEIRS);
    const std::vector<Outpost::EntityId> fleet{fighter};
    Assert::AreEqual(std::size_t{1}, Outpost::OrderAttack(world, fleet, target, world.NewOrderGroup()));
    Assert::IsNotNull(world.FindAttack(fighter));

    world.Find(target)->hullRemaining = 0;
    deaths.Advance(world);
    Outpost::Tick(world);
    weapons.Advance(world, 0);

    const Outpost::AttackOrder* attack = world.FindAttack(fighter);
    Assert::IsTrue((attack == nullptr) || !attack->active, L"the order outlived its target");
  }

  /// **A DEATH REACHES A CLIENT AS A REMOVAL**, with no system telling the accumulator: it finds the dead slot.
  TEST_METHOD(ADeathIsSentAsARemoval)
  {
    Outpost::World world;
    const Outpost::EntityId doomed = world.Create(Units(0, 0), 0, Outpost::DesignId::Miner, THEIRS);
    static_cast<void>(world.Create(Units(500, 0), 0, Outpost::DesignId::Fighter, MINE));
    Outpost::Accumulator accumulator;
    static_cast<void>(accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 1));

    world.Find(doomed)->hullRemaining = 0;
    Outpost::DeathSystem deaths;
    deaths.Advance(world);

    const std::vector<Outpost::Update> updates = accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 2);
    Assert::AreEqual(std::size_t{1}, updates.front().removals.size());
    Assert::AreEqual(Wire(doomed), updates.front().removals.front());
  }
};

} // namespace GameLogicTests
