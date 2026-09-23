#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

/// A station for each of two players and whatever modules a test asks for, placed clear of it for MINE.
[[nodiscard]] Outpost::World WithModules(std::vector<Outpost::DesignId> _mine, std::vector<Outpost::DesignId> _theirs = {})
{
  Outpost::World world;
  static_cast<void>(world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Station, MINE));
  static_cast<void>(world.Create(Neuron::Vec2{.x = 4000 * Neuron::FIXED_ONE}, 0, Outpost::DesignId::Station, THEIRS));
  std::int32_t offset = 200;
  for (const Outpost::DesignId design : _mine)
  {
    static_cast<void>(world.Create(Neuron::Vec2{.y = offset * Neuron::FIXED_ONE}, 0, design, MINE));
    offset += 100;
  }
  for (const Outpost::DesignId design : _theirs)
  {
    static_cast<void>(world.Create(Neuron::Vec2{.x = 4000 * Neuron::FIXED_ONE, .y = 200 * Neuron::FIXED_ONE}, 0, design, THEIRS));
  }
  return world;
}

/// Forty ticks of the unload rate's 2,500 thousandths: one full hold of 100 ore.
[[nodiscard]] std::uint32_t CreditsForOneHold(const Outpost::World& _world)
{
  Outpost::BuildSystem build;
  build.Begin(2);
  Outpost::Economy economy;
  economy.Begin();
  const std::uint32_t before = build.Credits(MINE);
  for (int tick = 0; tick < 40; ++tick)
  {
    const std::vector<Outpost::OreDelivery> deliveries{Outpost::OreDelivery{.player = MINE, .milliOre = 2500}};
    economy.Credit(deliveries, _world, build);
  }
  Assert::AreEqual(0u, economy.PendingMilliCredits(MINE), L"a hold left a fraction behind");
  return build.Credits(MINE) - before;
}
} // namespace

/// M2.12, `GameDesign.md` section 5. **The shipyard multiplies the build rate and the ore processor what a cargo is
/// worth**, both as integer percentages, pinned at both levels -- and the build's rounding direction asserted
/// rather than accepted (`OpenQuestions.md` Q56).
TEST_CLASS(TheShipyard)
{
public:
  TEST_METHOD(ItsMultiplierIsPinnedAtBothLevels)
  {
    Assert::AreEqual(100u, Outpost::BuildRateMultiplierPercent(WithModules({}), MINE), L"no shipyard");
    Assert::AreEqual(150u, Outpost::BuildRateMultiplierPercent(WithModules({Outpost::DesignId::ModuleShipyardL1}), MINE));
    Assert::AreEqual(200u, Outpost::BuildRateMultiplierPercent(WithModules({Outpost::DesignId::ModuleShipyardL2}), MINE));
    Assert::AreEqual(100u, Outpost::BuildRateMultiplierPercent(WithModules({Outpost::DesignId::ModuleOreProcessorL2}), MINE),
                     L"an ore processor does not build faster");
  }

  /// **THE BEST OF ITS KIND, NOT THE SUM**, and only your own.
  TEST_METHOD(TheBestShipyardCountsAndOnlyYourOwn)
  {
    const Outpost::World both = WithModules({Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleShipyardL2});
    Assert::AreEqual(200u, Outpost::BuildRateMultiplierPercent(both, MINE));

    const Outpost::World theirs = WithModules({}, {Outpost::DesignId::ModuleShipyardL2});
    Assert::AreEqual(100u, Outpost::BuildRateMultiplierPercent(theirs, MINE));
    Assert::AreEqual(200u, Outpost::BuildRateMultiplierPercent(theirs, THEIRS));
  }

  /// **TICKS ROUND UP** (Q56): a shipyard never builds faster than its stated rate. Every ship divides exactly; a
  /// module is where the rule shows. 400 credits at x1.5 is 266.67 ticks, so 267.
  TEST_METHOD(TheBuildRoundsUp)
  {
    Assert::AreEqual(267u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::ModuleShipyardL1, 150));
    Assert::AreEqual(234u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::ModuleOreProcessorL1, 150), L"233.33");
    Assert::AreEqual(167u, Outpost::BuildSystem::TicksForCost(250, 150), L"an ore processor's upgrade, 166.67");
    Assert::AreEqual(200u, Outpost::BuildSystem::TicksForCost(300, 150), L"a shipyard's upgrade divides exactly");

    // The ships, exactly, at both levels.
    Assert::AreEqual(100u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner, 150));
    Assert::AreEqual(75u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Miner, 200));
    Assert::AreEqual(200u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Fighter, 150));
    Assert::AreEqual(150u, Outpost::BuildSystem::TicksToBuild(Outpost::DesignId::Fighter, 200));
  }

  /// **THROUGH THE INTAKE**, the order a player gives is built at their shipyard's rate -- and at the rate it
  /// started at, whatever happens to the shipyard afterwards.
  TEST_METHOD(AnOrderIsBuiltAtThePlayersRate)
  {
    for (const auto& [design, ticks] :
         {std::pair{Outpost::DesignId::ModuleShipyardL1, 200u}, std::pair{Outpost::DesignId::ModuleShipyardL2, 150u}})
    {
      Outpost::World world = WithModules({design});
      Outpost::BuildSystem build;
      build.Begin(2);
      Outpost::CommandIntake intake;
      const Outpost::Command order{.sequence = 1,
                                   .type = Outpost::CommandType::Build,
                                   .targetX = static_cast<std::int16_t>(Outpost::DesignId::Fighter),
                                   .targetY = 0,
                                   .selection = {}};
      Assert::IsTrue(intake.Apply(world, build, MINE, order) == Outpost::CommandRejection::None);
      Assert::AreEqual(ticks, build.Item(MINE).ticksRequired);
    }
  }
};

TEST_CLASS(TheOreProcessor)
{
public:
  TEST_METHOD(ItsMultiplierIsPinnedAtBothLevels)
  {
    Assert::AreEqual(100u, Outpost::CargoValuePercent(WithModules({}), MINE));
    Assert::AreEqual(125u, Outpost::CargoValuePercent(WithModules({Outpost::DesignId::ModuleOreProcessorL1}), MINE));
    Assert::AreEqual(150u, Outpost::CargoValuePercent(WithModules({Outpost::DesignId::ModuleOreProcessorL2}), MINE));
    Assert::AreEqual(100u, Outpost::CargoValuePercent(WithModules({Outpost::DesignId::ModuleShipyardL2}), MINE));
  }

  /// **A HOLD OF 100 ORE IS 100, 125 AND 150 CREDITS**, exactly: 2,500 thousandths at 125% is 3,125, and the
  /// remainder is carried in hundredths so nothing is lost at either level.
  TEST_METHOD(AHoldIsWorthExactlyItsPercentage)
  {
    Assert::AreEqual(100u, CreditsForOneHold(WithModules({})));
    Assert::AreEqual(125u, CreditsForOneHold(WithModules({Outpost::DesignId::ModuleOreProcessorL1})));
    Assert::AreEqual(150u, CreditsForOneHold(WithModules({Outpost::DesignId::ModuleOreProcessorL2})));
  }

  /// **NOTHING ROUNDS UNTIL A WHOLE CREDIT LEAVES**: three deliveries of 333 thousandths at 125% are 1,248.75
  /// thousandths, and one more of 1 makes exactly 1,250 -- a credit granted and 250 carried.
  TEST_METHOD(AnAwkwardDeliveryIsCarriedNotRounded)
  {
    const Outpost::World world = WithModules({Outpost::DesignId::ModuleOreProcessorL1});
    Outpost::BuildSystem build;
    build.Begin(2);
    Outpost::Economy economy;
    economy.Begin();
    const std::uint32_t before = build.Credits(MINE);

    const std::vector<Outpost::OreDelivery> three{Outpost::OreDelivery{.player = MINE, .milliOre = 333},
                                                  Outpost::OreDelivery{.player = MINE, .milliOre = 333},
                                                  Outpost::OreDelivery{.player = MINE, .milliOre = 333}};
    economy.Credit(three, world, build);
    Assert::AreEqual(before + 1u, build.Credits(MINE));
    Assert::AreEqual(248u, economy.PendingMilliCredits(MINE), L"248.75, reported rounded down and carried in full");

    const std::vector<Outpost::OreDelivery> one{Outpost::OreDelivery{.player = MINE, .milliOre = 1}};
    economy.Credit(one, world, build);
    Assert::AreEqual(250u, economy.PendingMilliCredits(MINE));
  }

  /// **YOUR PROCESSOR PAYS YOU**, and another player's pays them.
  TEST_METHOD(OnlyYourOwnProcessorPaysYou)
  {
    const Outpost::World world = WithModules({}, {Outpost::DesignId::ModuleOreProcessorL2});
    Assert::AreEqual(100u, CreditsForOneHold(world));
    Assert::AreEqual(150u, Outpost::CargoValuePercent(world, THEIRS));
  }
};

/// **WHICH EFFECT A MODULE HAS IS ITS ROW'S** (R24): nothing in the simulation names a shipyard.
TEST_CLASS(TheEffectIsInTheCatalog)
{
public:
  TEST_METHOD(EachModuleComponentNamesItsEffect)
  {
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::ShipyardL1).effect == Outpost::ModuleEffect::BuildRate);
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::ShipyardL2).effect == Outpost::ModuleEffect::BuildRate);
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::OreProcessorL1).effect == Outpost::ModuleEffect::CargoValue);
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::OreProcessorL2).effect == Outpost::ModuleEffect::CargoValue);
    for (const Outpost::ComponentId weapon : {Outpost::ComponentId::None, Outpost::ComponentId::MiningLaser,
                                              Outpost::ComponentId::MassDriver, Outpost::ComponentId::PointDefense})
    {
      Assert::IsTrue(Outpost::Component(weapon).effect == Outpost::ModuleEffect::None);
    }
  }
};

} // namespace GameLogicTests
