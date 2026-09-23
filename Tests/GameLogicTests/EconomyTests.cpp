#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
[[nodiscard]] Outpost::OreDelivery Delivery(Outpost::PlayerId _player, std::uint32_t _milliOre) noexcept
{
  return Outpost::OreDelivery{.player = _player, .milliOre = _milliOre};
}
} // namespace

/// M2.7. **Credits accrue on unload, and a hold is worth exactly what it carried.**
TEST_CLASS(TheEconomy)
{
public:
  /// **ONE ORE, ONE CREDIT** -- the design's "100 credits of capacity" -- until M2.12's processor multiplies it.
  TEST_METHOD(AnOreIsACredit)
  {
    Assert::AreEqual(1u, Outpost::CREDITS_PER_ORE);
  }

  /// **FORTY TICKS OF 2,500 THOUSANDTHS ARE EXACTLY A HUNDRED CREDITS**: the half a credit each tick leaves
  /// is carried and completes on the next, so nothing is lost to the division and nothing is granted early.
  TEST_METHOD(AHoldUnloadedOverFortyTicksIsExactlyAHundredCredits)
  {
    const Outpost::World world;
    Outpost::BuildSystem build;
    build.Begin(2);
    Outpost::Economy economy;
    economy.Begin();

    const std::uint32_t before = build.Credits(1);
    for (int tick = 0; tick < 40; ++tick)
    {
      const std::vector<Outpost::OreDelivery> deliveries{Delivery(1, 2500)};
      economy.Credit(deliveries, world, build);
      Assert::IsTrue(economy.PendingMilliCredits(1) < Outpost::MILLI_ORE_PER_ORE, L"a whole credit was held back");
    }
    Assert::AreEqual(before + 100u, build.Credits(1));
    Assert::AreEqual(0u, economy.PendingMilliCredits(1));
  }

  /// **EACH PLAYER'S REMAINDER IS THEIR OWN**: half a credit from one player does not complete another's.
  TEST_METHOD(RemaindersDoNotCrossPlayers)
  {
    const Outpost::World world;
    Outpost::BuildSystem build;
    build.Begin(2);
    Outpost::Economy economy;
    economy.Begin();
    const std::uint32_t one = build.Credits(1);
    const std::uint32_t two = build.Credits(2);

    const std::vector<Outpost::OreDelivery> deliveries{Delivery(1, 500), Delivery(2, 500)};
    economy.Credit(deliveries, world, build);
    Assert::AreEqual(one, build.Credits(1));
    Assert::AreEqual(two, build.Credits(2));
    Assert::AreEqual(500u, economy.PendingMilliCredits(1));
    Assert::AreEqual(500u, economy.PendingMilliCredits(2));
  }

  /// A delivery with no player -- which nothing produces -- is dropped rather than indexed.
  TEST_METHOD(ADeliveryWithNoPlayerIsIgnored)
  {
    const Outpost::World world;
    Outpost::BuildSystem build;
    build.Begin(2);
    Outpost::Economy economy;
    economy.Begin();
    const std::vector<Outpost::OreDelivery> deliveries{Delivery(Outpost::NO_PLAYER, 5000)};
    economy.Credit(deliveries, world, build);
    Assert::AreEqual(0u, economy.PendingMilliCredits(Outpost::NO_PLAYER));
  }

  /// **THE LOOP, END TO END**: a miner's full cycle through the mining system and the economy lands exactly one
  /// hold's credits in its owner's balance -- `GameDesign.md` section 4's claim that credits climb because
  /// ships did work.
  TEST_METHOD(ACycleThroughTheLoopPaysItsOwner)
  {
    Outpost::World world;
    world.SetField({Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid, .position = Neuron::Vec2{.x = 2000 * Neuron::FIXED_ONE}}});
    static_cast<void>(world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Station, 1));
    const Outpost::EntityId miner = world.Create(Neuron::Vec2{.x = 160 * Neuron::FIXED_ONE}, 0, Outpost::DesignId::Miner, 1);
    Assert::IsTrue(world.OrderMine(miner, 0));

    Outpost::BuildSystem build;
    build.Begin(2);
    Outpost::MiningSystem mining;
    Outpost::Economy economy;
    economy.Begin();
    const std::uint32_t before = build.Credits(1);

    // One cycle and the first leg of the next: 795 ticks, as `MiningTests` derives them.
    for (int tick = 0; tick < 800; ++tick)
    {
      Outpost::Tick(world);
      mining.Advance(world);
      economy.Credit(mining.Deliveries(), world, build);
    }
    Assert::AreEqual(before + 100u, build.Credits(1));
    Assert::AreEqual(before, build.Credits(2), L"somebody else was paid for player one's ore");
  }
};

} // namespace GameLogicTests
