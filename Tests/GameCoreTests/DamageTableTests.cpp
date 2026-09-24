#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

namespace
{
/// Every mount a design carries that shoots, one entry per mount.
[[nodiscard]] std::vector<Outpost::ComponentId> Weapons(Outpost::DesignId _design)
{
  std::vector<Outpost::ComponentId> weapons;
  const Outpost::DesignEntry& design = Outpost::Design(_design);
  for (std::size_t slot = 0; slot < Outpost::Hull(design.hull).slotCount; ++slot)
  {
    if (Outpost::Component(design.slots[slot]).damagePerSecond > 0)
    {
      weapons.push_back(design.slots[slot]);
    }
  }
  return weapons;
}

/// **TICKS FOR _count ATTACKERS OF ONE DESIGN TO KILL ONE TARGET**, through ADR-014 exactly: every mount keeps
/// its own remainder and takes whole points off the hull as they accumulate. This is the rule M3.2 applies in
/// the tick, run here in isolation so that section 7's rows are pinned against the arithmetic rather than
/// against a match.
[[nodiscard]] std::uint32_t TicksToKill(Outpost::DesignId _attacker, std::size_t _count, Outpost::DesignId _target)
{
  std::vector<Outpost::ComponentId> mounts;
  for (std::size_t attacker = 0; attacker < _count; ++attacker)
  {
    for (const Outpost::ComponentId weapon : Weapons(_attacker))
    {
      mounts.push_back(weapon);
    }
  }
  std::vector<std::uint32_t> remainders(mounts.size(), 0);

  std::int64_t hull = Outpost::Derive(_target).hullPoints;
  std::uint32_t ticks = 0;
  while (hull > 0)
  {
    ++ticks;
    for (std::size_t mount = 0; mount < mounts.size(); ++mount)
    {
      hull -= Outpost::Accumulate(remainders[mount], Outpost::DamagePerInterval(mounts[mount], _target));
    }
  }
  return ticks;
}

/// Section 7 states times in seconds to one decimal place; this is a tick count in those units.
[[nodiscard]] double Seconds(std::uint32_t _ticks) noexcept
{
  return static_cast<double>(_ticks) / static_cast<double>(Outpost::TICKS_PER_SECOND);
}
} // namespace

/// M3.1: `GameDesign.md` section 7's damage table and ADR-014's rule for applying it.
TEST_CLASS(TheDamageTable)
{
public:
  TEST_METHOD(TheSixModifiersAreSectionSevensTable)
  {
    Assert::AreEqual(70u, Outpost::ModifierPercent(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Light));
    Assert::AreEqual(60u, Outpost::ModifierPercent(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Medium));
    Assert::AreEqual(25u, Outpost::ModifierPercent(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Heavy));
    Assert::AreEqual(120u, Outpost::ModifierPercent(Outpost::ComponentId::PointDefense, Outpost::SizeClass::Light));
    Assert::AreEqual(90u, Outpost::ModifierPercent(Outpost::ComponentId::PointDefense, Outpost::SizeClass::Medium));
    Assert::AreEqual(30u, Outpost::ModifierPercent(Outpost::ComponentId::PointDefense, Outpost::SizeClass::Heavy));
  }

  TEST_METHOD(EveryWeaponAgainstEverySizeClassIsExactEveryTick)
  {
    // ADR-014: `dps x modifier x 5` ten-thousandths a tick, a whole number for every cell. 8,750 is 0.875 of a
    // point, which is what a mass driver takes from a Scout hull.
    Assert::AreEqual(8750u, Outpost::ShipDamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Light));
    Assert::AreEqual(7500u, Outpost::ShipDamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Medium));
    Assert::AreEqual(3125u, Outpost::ShipDamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Heavy));
    Assert::AreEqual(36000u, Outpost::ShipDamagePerInterval(Outpost::ComponentId::PointDefense, Outpost::SizeClass::Light));
    Assert::AreEqual(27000u, Outpost::ShipDamagePerInterval(Outpost::ComponentId::PointDefense, Outpost::SizeClass::Medium));
    Assert::AreEqual(9000u, Outpost::ShipDamagePerInterval(Outpost::ComponentId::PointDefense, Outpost::SizeClass::Heavy));
  }

  TEST_METHOD(NothingThatDoesNotShootDoesDamage)
  {
    for (const Outpost::ComponentId component : {Outpost::ComponentId::None, Outpost::ComponentId::MiningLaser,
                                                 Outpost::ComponentId::ShipyardL1, Outpost::ComponentId::OreProcessorL2})
    {
      for (const Outpost::SizeClass size : {Outpost::SizeClass::Light, Outpost::SizeClass::Medium, Outpost::SizeClass::Heavy})
      {
        Assert::AreEqual(0u, Outpost::ShipDamagePerInterval(component, size));
      }
      Assert::AreEqual(0u, Outpost::StructureDamagePerInterval(component, 300));
    }
  }

  TEST_METHOD(TheTwoModelsAgreeWhereSectionSevenChoseTheNumber)
  {
    // **THE IDENTITY SECTION 7 PICKED 300 TO PRESERVE**: a hit value of 300 quarters what lands, and so does
    // the Large column for a mass driver. A test that stops seeing this is a balance change made by accident.
    Assert::AreEqual(Outpost::ShipDamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::SizeClass::Heavy),
                     Outpost::StructureDamagePerInterval(Outpost::ComponentId::MassDriver, 300));
  }

  TEST_METHOD(TheCurveMultipliesBeforeItDivides)
  {
    // `base x (100 / (100 + hitValue))` in integers is zero for any hit value above zero. Multiplying first is
    // what makes a mass driver take 0.3125 of a point a tick off a station rather than nothing.
    Assert::AreEqual(3125u, Outpost::StructureDamagePerInterval(Outpost::ComponentId::MassDriver, 300));
    Assert::AreEqual(12500u, Outpost::StructureDamagePerInterval(Outpost::ComponentId::MassDriver, 0),
                     L"a hit value of 0 is no mitigation");
    Assert::AreEqual(6250u, Outpost::StructureDamagePerInterval(Outpost::ComponentId::MassDriver, 100), L"100 halves what lands");
    Assert::IsTrue(Outpost::StructureDamagePerInterval(Outpost::ComponentId::MassDriver, 10000) > 0, L"no hit value makes a base immune");
  }

  TEST_METHOD(ADesignPicksItsModelByItsDerivedHitValue)
  {
    // A station and every module frame are structures because their hulls carry a hit value, not because
    // anything names them (R24).
    Assert::AreEqual(300u, Outpost::Derive(Outpost::DesignId::Station).hitValue);
    Assert::AreEqual(300u, Outpost::Derive(Outpost::DesignId::ModuleShipyardL1).hitValue);
    Assert::AreEqual(0u, Outpost::Derive(Outpost::DesignId::Fighter).hitValue);

    Assert::AreEqual(3125u, Outpost::DamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::DesignId::Station));
    Assert::AreEqual(8750u, Outpost::DamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::DesignId::Miner));
    Assert::AreEqual(7500u, Outpost::DamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::DesignId::Fighter));
  }

  TEST_METHOD(AccumulationKeepsTheFractionAndLosesNothing)
  {
    // 0.875 of a point a tick: nothing on the first, one on the second, and exactly seven after eight.
    std::uint32_t remainder = 0;
    Assert::AreEqual(0u, Outpost::Accumulate(remainder, 8750));
    Assert::AreEqual(1u, Outpost::Accumulate(remainder, 8750));
    std::uint32_t points = 1;
    for (int tick = 2; tick < 8; ++tick)
    {
      points += Outpost::Accumulate(remainder, 8750);
    }
    Assert::AreEqual(7u, points);
    Assert::AreEqual(0u, remainder, L"eight ticks of 0.875 is exactly seven points");
  }

  TEST_METHOD(TheCadenceIsOneNamedConstant)
  {
    Assert::AreEqual(1u, Outpost::DAMAGE_INTERVAL_TICKS, L"ADR-014: damage accumulates every tick");
    Assert::AreEqual(20u, Outpost::TICKS_PER_SECOND);
  }
};

/// **SECTION 7'S RAID ARITHMETIC, REPRODUCED THROUGH THE RULE** (ADR-014: within one tick of each row). These
/// are the numbers the design's balance argument rests on, so a rounding change breaks a test before it breaks
/// a match.
TEST_CLASS(TheRaidArithmetic)
{
public:
  TEST_METHOD(OneFighterKillsOneMinerIn12Point9Seconds)
  {
    const std::uint32_t ticks = TicksToKill(Outpost::DesignId::Fighter, 1, Outpost::DesignId::Miner);
    Assert::AreEqual(258u, ticks);
    Assert::AreEqual(12.9, Seconds(ticks), 0.05);
  }

  TEST_METHOD(OneFighterKillsOneFighterIn20Seconds)
  {
    const std::uint32_t ticks = TicksToKill(Outpost::DesignId::Fighter, 1, Outpost::DesignId::Fighter);
    Assert::AreEqual(400u, ticks);
    Assert::AreEqual(20.0, Seconds(ticks), 0.05);
  }

  TEST_METHOD(DefenseIsSlowerThanOffenseByAboutOnePointSix)
  {
    // Section 7 fixed a 5x that solved the game; 1.6 is what made a raid an exchange.
    const double ratio = static_cast<double>(TicksToKill(Outpost::DesignId::Fighter, 1, Outpost::DesignId::Fighter)) /
                         static_cast<double>(TicksToKill(Outpost::DesignId::Fighter, 1, Outpost::DesignId::Miner));
    Assert::AreEqual(1.6, ratio, 0.05);
  }

  TEST_METHOD(ThreeFightersKillSixMinersIn25Point7Seconds)
  {
    // **SECTION 7'S FIGURE IS THE POOLED RATE, AND A REAL FIGHT PAYS OVERKILL ON TOP OF IT.** Six Scout hulls at
    // the rule's 5.25 points a tick from three fighters is 514.3 ticks, which is section 7's 25.7 seconds.
    const std::uint32_t perTick = 3 * 2 * Outpost::DamagePerInterval(Outpost::ComponentId::MassDriver, Outpost::DesignId::Miner);
    const std::uint32_t sixHulls = 6 * Outpost::Derive(Outpost::DesignId::Miner).hullPoints;
    const std::uint32_t pooledTicks = ((sixHulls * Outpost::DAMAGE_UNITS_PER_POINT) + perTick - 1) / perTick;
    Assert::AreEqual(515u, pooledTicks);
    Assert::AreEqual(25.7, Seconds(pooledTicks), 0.1);

    // Killed one at a time, every kill ends mid-tick and the rest of that tick's damage lands on a dead hull:
    // 86 ticks a miner, 516 in all, 25.8 seconds. Which of the two a match sees is M3.2's targeting.
    Assert::AreEqual(516u, 6 * TicksToKill(Outpost::DesignId::Fighter, 3, Outpost::DesignId::Miner));
  }

  TEST_METHOD(TenFightersBringDownAStationInAboutAMinute)
  {
    // `GameDesign.md` section 5: "ten fighters standing off at 500 units take about a minute".
    const std::uint32_t ticks = TicksToKill(Outpost::DesignId::Fighter, 10, Outpost::DesignId::Station);
    Assert::AreEqual(1280u, ticks);
    Assert::AreEqual(64.0, Seconds(ticks), 0.05);
  }

  TEST_METHOD(OneFighterTakesAboutTenAndAHalfMinutesOverAStation)
  {
    const std::uint32_t ticks = TicksToKill(Outpost::DesignId::Fighter, 1, Outpost::DesignId::Station);
    Assert::AreEqual(12800u, ticks);
    Assert::AreEqual(10.67, Seconds(ticks) / 60.0, 0.01);
  }

  TEST_METHOD(AModuleTakesOneFighterTwoMinutesAndThreeFortySeconds)
  {
    // Section 7: "one fighter needs about two minutes to kill a module and three need forty seconds".
    Assert::AreEqual(2400u, TicksToKill(Outpost::DesignId::Fighter, 1, Outpost::DesignId::ModuleShipyardL1));
    Assert::AreEqual(800u, TicksToKill(Outpost::DesignId::Fighter, 3, Outpost::DesignId::ModuleOreProcessorL1));
  }
};

} // namespace GameCoreTests
