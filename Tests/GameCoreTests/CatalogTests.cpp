#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

/// ADR-006 and R24: the catalog is tables referred to by identity, and the properties below are
/// what "by identity" has to mean if research is ever going to be a gate over a set of them.
TEST_CLASS(CatalogIdentities)
{
public:
  /// M1.1's first "done when". Every identity resolves to exactly one entry, and the entry it
  /// resolves to is the one that names it -- which is what makes the lookup an index rather than a
  /// search, and what a table that grew out of step with its enumerator would break.
  TEST_METHOD(EveryIdentityResolvesToTheEntryThatNamesIt)
  {
    for (std::size_t index = 0; index < Outpost::Hulls().size(); ++index)
    {
      const auto id = static_cast<Outpost::HullId>(index);
      Assert::IsTrue(Outpost::Hull(id).id == id);
    }
    for (std::size_t index = 0; index < Outpost::Drives().size(); ++index)
    {
      const auto id = static_cast<Outpost::DriveId>(index);
      Assert::IsTrue(Outpost::Drive(id).id == id);
    }
    for (std::size_t index = 0; index < Outpost::Components().size(); ++index)
    {
      const auto id = static_cast<Outpost::ComponentId>(index);
      Assert::IsTrue(Outpost::Component(id).id == id);
    }
  }

  /// M1.1's second. No entry is reachable by anything other than its identity, which here means no
  /// identity appears twice -- two rows claiming one identity would make the lookup return the
  /// first and leave the second permanently unreachable.
  TEST_METHOD(NoIdentityAppearsTwice)
  {
    for (const auto& a : Outpost::Hulls())
    {
      std::size_t seen = 0;
      for (const auto& b : Outpost::Hulls())
      {
        if (a.id == b.id)
        {
          ++seen;
        }
      }
      Assert::AreEqual(static_cast<std::size_t>(1), seen);
    }
    for (const auto& a : Outpost::Components())
    {
      std::size_t seen = 0;
      for (const auto& b : Outpost::Components())
      {
        if (a.id == b.id)
        {
          ++seen;
        }
      }
      Assert::AreEqual(static_cast<std::size_t>(1), seen);
    }
  }

  /// M1.1's third, and the one that is a statement about M4 rather than about M1. `GameDesign.md`
  /// section 6 and section 10 keep the `Cruiser` precisely so that reinstating a heavy design is a
  /// table row; a catalog holding only what the MVP builds would make it a change to the model.
  TEST_METHOD(TheCatalogHoldsTheCruiserAlthoughNothingBuildsIt)
  {
    const Outpost::HullEntry& cruiser = Outpost::Hull(Outpost::HullId::Cruiser);
    Assert::AreEqual(4, static_cast<int>(cruiser.slotCount));
    Assert::AreEqual(3000, static_cast<int>(cruiser.hullPoints));
    Assert::IsTrue(cruiser.sizeClass == Outpost::SizeClass::Heavy);
  }

  /// `GameDesign.md` section 6's table, as figures rather than prose. These are the numbers the
  /// derivation at M1.2 sums, so a typo here is a balance change nothing else would catch.
  TEST_METHOD(TheHullTableIsTheDesignsTable)
  {
    Assert::AreEqual(450, static_cast<int>(Outpost::Hull(Outpost::HullId::Scout).hullPoints));
    Assert::AreEqual(600, static_cast<int>(Outpost::Hull(Outpost::HullId::Frigate).hullPoints));
    Assert::AreEqual(8000, static_cast<int>(Outpost::Hull(Outpost::HullId::Station).hullPoints));
    Assert::AreEqual(1500, static_cast<int>(Outpost::Hull(Outpost::HullId::ModuleFrame).hullPoints));

    Assert::AreEqual(1, static_cast<int>(Outpost::Hull(Outpost::HullId::Scout).slotCount));
    Assert::AreEqual(2, static_cast<int>(Outpost::Hull(Outpost::HullId::Frigate).slotCount));
    Assert::AreEqual(2, static_cast<int>(Outpost::Hull(Outpost::HullId::Station).slotCount));
    Assert::AreEqual(1, static_cast<int>(Outpost::Hull(Outpost::HullId::ModuleFrame).slotCount));
  }

  /// **ONLY THE TWO BASE STRUCTURES CARRY A HIT VALUE** (`GameDesign.md` section 6). The dash in
  /// that table is a zero here and it means the hull is damaged through section 7's size-class
  /// table instead -- the two mitigation models are the cost section 7 names, and a third hull
  /// quietly gaining a hit value would change what a mass driver does to a fleet.
  TEST_METHOD(OnlyTheBaseStructuresHaveAHitValue)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      const bool isBaseStructure = (hull.id == Outpost::HullId::Station) || (hull.id == Outpost::HullId::ModuleFrame);
      if (isBaseStructure)
      {
        Assert::AreEqual(300, static_cast<int>(hull.hitValue));
      }
      else
      {
        Assert::AreEqual(0, static_cast<int>(hull.hitValue));
      }
    }
  }

  /// A station and a module frame are hulls with no drive, which is R24's whole point: they are not
  /// a second kind of thing in the simulation, they are an absent component.
  TEST_METHOD(AbsenceIsAnIdentityRatherThanANull)
  {
    Assert::IsTrue(Outpost::Drive(Outpost::DriveId::None).id == Outpost::DriveId::None);
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::None).id == Outpost::ComponentId::None);
  }
};

/// `GameDesign.md` section 6's component tables, and the rules stated in their prose.
TEST_CLASS(CatalogComponents)
{
public:
  TEST_METHOD(TheRangesAreTheDesignsRanges)
  {
    Assert::AreEqual(200, static_cast<int>(Outpost::Component(Outpost::ComponentId::MiningLaser).rangeUnits));
    Assert::AreEqual(600, static_cast<int>(Outpost::Component(Outpost::ComponentId::MassDriver).rangeUnits));
    Assert::AreEqual(400, static_cast<int>(Outpost::Component(Outpost::ComponentId::PointDefense).rangeUnits));
  }

  /// Q10, as two numbers: point defense reaches 400 and a mass driver reaches 600, so a fighter can
  /// stand off and shell the station untouched. **The station kills a loiterer, not a besieger**,
  /// and that is deliberate -- with the heavy design cut, a station whose defense outranged the
  /// fighter would be unkillable.
  TEST_METHOD(AMassDriverOutrangesPointDefense)
  {
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::MassDriver).rangeUnits >
                   Outpost::Component(Outpost::ComponentId::PointDefense).rangeUnits);
  }

  /// "Does no damage" is in the design's own words, and it is what makes a miner a miner rather
  /// than a cheap warship.
  TEST_METHOD(AMiningLaserDoesNoDamage)
  {
    const Outpost::ComponentEntry& laser = Outpost::Component(Outpost::ComponentId::MiningLaser);
    Assert::AreEqual(0, static_cast<int>(laser.damagePerSecond));
    Assert::AreEqual(20, static_cast<int>(laser.orePerSecond));
    Assert::AreEqual(100, static_cast<int>(laser.oreCapacity));
  }

  /// **STATION SLOTS ONLY**, read off the catalog rather than by naming the component. Build
  /// validation at M2 asks the entry; if it asked for `PointDefense` by name, the rule would have
  /// to be written again for the next component that carries it.
  TEST_METHOD(OnlyPointDefenseIsRestrictedToStationSlots)
  {
    for (const Outpost::ComponentEntry& component : Outpost::Components())
    {
      const bool expected = (component.id == Outpost::ComponentId::PointDefense);
      Assert::AreEqual(expected, component.stationSlotsOnly);
    }
  }

  /// In hundredths because the simulation is integers (R16): x1.5, x2.0, +25%, +50%.
  TEST_METHOD(TheModuleMultipliersAreHundredths)
  {
    Assert::AreEqual(150, static_cast<int>(Outpost::Component(Outpost::ComponentId::ShipyardL1).multiplierPercent));
    Assert::AreEqual(200, static_cast<int>(Outpost::Component(Outpost::ComponentId::ShipyardL2).multiplierPercent));
    Assert::AreEqual(125, static_cast<int>(Outpost::Component(Outpost::ComponentId::OreProcessorL1).multiplierPercent));
    Assert::AreEqual(150, static_cast<int>(Outpost::Component(Outpost::ComponentId::OreProcessorL2).multiplierPercent));
  }

  TEST_METHOD(TheModuleCostsAreTheDesignsCosts)
  {
    Assert::AreEqual(400, static_cast<int>(Outpost::Component(Outpost::ComponentId::ShipyardL1).cost));
    Assert::AreEqual(700, static_cast<int>(Outpost::Component(Outpost::ComponentId::ShipyardL2).cost));
    Assert::AreEqual(350, static_cast<int>(Outpost::Component(Outpost::ComponentId::OreProcessorL1).cost));
    Assert::AreEqual(600, static_cast<int>(Outpost::Component(Outpost::ComponentId::OreProcessorL2).cost));
  }

  /// A LEVEL IS ITS OWN IDENTITY (Q31), which is free under R24: upgrading replaces one catalog row
  /// with the next, and research gating a level is the gate it already needs over any component.
  TEST_METHOD(EachUpgradeLevelIsItsOwnIdentity)
  {
    Assert::IsTrue(Outpost::ComponentId::ShipyardL1 != Outpost::ComponentId::ShipyardL2);
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::ShipyardL2).cost > Outpost::Component(Outpost::ComponentId::ShipyardL1).cost);
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::OreProcessorL2).multiplierPercent >
                   Outpost::Component(Outpost::ComponentId::OreProcessorL1).multiplierPercent);
  }

  /// `ResearchStationL1` is deliberately absent: section 6 gives it a dash for a cost and says it
  /// is designed at M4 when there is research for it to do. Q30 is the argument -- a module that
  /// costs credits and does nothing is the mistake the heavy design already made. **Section 6's own
  /// count agrees**: "four module components" against five rows in its table.
  TEST_METHOD(TheCatalogHoldsFourModuleComponents)
  {
    std::size_t modules = 0;
    for (const Outpost::ComponentEntry& component : Outpost::Components())
    {
      if (component.cost > 0)
      {
        ++modules;
      }
    }
    Assert::AreEqual(static_cast<std::size_t>(4), modules);
  }
};

} // namespace GameCoreTests
