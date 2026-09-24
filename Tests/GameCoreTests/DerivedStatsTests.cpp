#include "pch.h"

#include <array>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

namespace
{
constexpr std::array<Outpost::ComponentId, Outpost::MAX_COMPONENT_SLOTS> EMPTY{};

[[nodiscard]] std::array<Outpost::ComponentId, Outpost::MAX_COMPONENT_SLOTS> Fitted(Outpost::ComponentId _component, std::size_t _count)
{
  std::array<Outpost::ComponentId, Outpost::MAX_COMPONENT_SLOTS> slots{};
  for (std::size_t i = 0; (i < _count) && (i < Outpost::MAX_COMPONENT_SLOTS); ++i)
  {
    slots[i] = _component;
  }
  return slots;
}
} // namespace

/// M1.2: the two shipped designs reproduce `GameDesign.md` section 6's table. **These are the three
/// anchors Q46's figures were chosen against**, so a change to the catalog that breaks one of them
/// is a balance change rather than a refactor.
TEST_CLASS(TheShippedDesigns)
{
public:
  TEST_METHOD(TheMinerIs150CreditsAt100UnitsASecond)
  {
    const Outpost::DerivedStats miner = Outpost::Derive(Outpost::DesignId::Miner);
    Assert::AreEqual(150u, miner.cost);
    Assert::AreEqual(100u, miner.speedUnitsPerSecond);
    Assert::AreEqual(450u, miner.hullPoints);
    // Q59: half a circle in 1.4 seconds.
    Assert::AreEqual(23400u, miner.turnAnglePerSecond);
  }

  TEST_METHOD(TheFighterIs300CreditsAt140UnitsASecond)
  {
    const Outpost::DerivedStats fighter = Outpost::Derive(Outpost::DesignId::Fighter);
    Assert::AreEqual(300u, fighter.cost);
    Assert::AreEqual(140u, fighter.speedUnitsPerSecond);
    Assert::AreEqual(600u, fighter.hullPoints);
    // Q59: half a circle in a second, to within eight units of 32,768.
    Assert::AreEqual(32760u, fighter.turnAnglePerSecond);
  }

  /// `GameDesign.md` section 6 cut the battleship at **2,400 credits against an income of about 15
  /// a second**. That figure is as binding as the other two: it is the arithmetic the design used
  /// to decide the MVP would not contain one, and a catalog where it no longer sums to 2,400 has
  /// quietly reopened that decision.
  TEST_METHOD(TheCutBattleshipStillCosts2400)
  {
    const Outpost::DerivedStats battleship =
      Outpost::Derive(Outpost::HullId::Cruiser, Outpost::DriveId::BurnDrive, Fitted(Outpost::ComponentId::MassDriver, 4));
    Assert::AreEqual(2400u, battleship.cost);
  }

  /// A station is a row in the same table, with no drive and two mounts.
  TEST_METHOD(TheStationIsADesignWithNoDrive)
  {
    const Outpost::DerivedStats station = Outpost::Derive(Outpost::DesignId::Station);
    Assert::AreEqual(0u, station.speedUnitsPerSecond);
    Assert::AreEqual(0u, station.turnAnglePerSecond);
    Assert::AreEqual(8000u, station.hullPoints);
    // Two point-defense mounts at 60 a second each.
    Assert::AreEqual(120u, station.damagePerSecond);
  }

  /// **BOTH SHIPPED DIVISIONS ARE EXACT**, which is a property of Q46's numbers rather than of the
  /// derivation. It is pinned because a later catalog change could make the speeds depend on which
  /// way the truncation happens to fall, and that is the kind of thing nobody notices.
  TEST_METHOD(NeitherShippedSpeedDependsOnRounding)
  {
    const Outpost::DerivedStats miner = Outpost::Derive(Outpost::DesignId::Miner);
    const Outpost::DerivedStats fighter = Outpost::Derive(Outpost::DesignId::Fighter);

    Assert::AreEqual(0u, Outpost::Drive(Outpost::DriveId::IonDrive).thrust % miner.mass);
    Assert::AreEqual(0u, static_cast<std::uint32_t>(Outpost::Drive(Outpost::DriveId::BurnDrive).thrust) % fighter.mass);
  }
};

/// M1.2's properties, asserted over the whole catalog rather than over the three rows that ship.
TEST_CLASS(DerivationProperties)
{
public:
  /// **A HULL WITH NO DRIVE DERIVES A SPEED OF ZERO RATHER THAN DIVIDING BY SOMETHING**, which is
  /// M1.2's own wording. Every hull, with no drive and nothing fitted.
  TEST_METHOD(NoDriveIsNoSpeedForEveryHull)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      const Outpost::DerivedStats stats = Outpost::Derive(hull.id, Outpost::DriveId::None, EMPTY);
      Assert::AreEqual(0u, stats.speedUnitsPerSecond);
    }
  }

  /// ADR-006: "a cruiser with four plasma cannons is slower than an empty one **because of
  /// arithmetic, not because anyone wrote it down**". Asserted as a property over every hull that
  /// moves and every drive that works, rather than as a value on one design.
  TEST_METHOD(AddingAComponentLowersTheSpeed)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      for (const Outpost::DriveId drive : {Outpost::DriveId::IonDrive, Outpost::DriveId::BurnDrive})
      {
        const Outpost::DerivedStats empty = Outpost::Derive(hull.id, drive, EMPTY);
        if (empty.speedUnitsPerSecond == 0)
        {
          continue;
        }

        for (std::size_t fitted = 1; fitted <= hull.slotCount; ++fitted)
        {
          const Outpost::DerivedStats loaded = Outpost::Derive(hull.id, drive, Fitted(Outpost::ComponentId::MassDriver, fitted));
          Assert::IsTrue(loaded.speedUnitsPerSecond < empty.speedUnitsPerSecond, L"a loaded hull must be slower than an empty one");
          Assert::IsTrue(loaded.turnAnglePerSecond < empty.turnAnglePerSecond, L"a loaded hull must turn slower than an empty one");
          Assert::IsTrue(loaded.mass > empty.mass);
        }
      }
    }
  }

  /// `TechnicalDesign.md` section 8: **every catalog combination is pinned, including every
  /// `Cruiser` one, which no MVP design uses.** This walks all of them and asserts the derivation
  /// is total -- no combination crashes, and every one that has thrust and mass has a speed.
  TEST_METHOD(EveryCatalogCombinationDerives)
  {
    std::size_t combinations = 0;
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      for (const Outpost::DriveEntry& drive : Outpost::Drives())
      {
        for (const Outpost::ComponentEntry& component : Outpost::Components())
        {
          for (std::size_t fitted = 0; fitted <= hull.slotCount; ++fitted)
          {
            const Outpost::DerivedStats stats = Outpost::Derive(hull.id, drive.id, Fitted(component.id, fitted));
            ++combinations;

            const bool moves = (drive.thrust > 0) && (stats.mass > 0);
            Assert::AreEqual(moves, stats.speedUnitsPerSecond > 0);
            Assert::IsTrue(stats.cost >= hull.cost);
          }
        }
      }
    }
    // Three drives x eight components x the sum over hulls of (slotCount + 1), which is
    // 2 + 3 + 5 + 3 + 2 + 1 = 16 for a Scout, Frigate, Cruiser, Station, ModuleFrame and M3.9's slotless DepotFrame.
    // 3 x 8 x 16.
    // The count is asserted so that a catalog row added without a thought cannot quietly shrink
    // this sweep -- which is the only thing making "every combination" mean anything.
    Assert::AreEqual(static_cast<std::size_t>(384), combinations);
  }

  /// A component past the hull's slot count is not the hull's business. A malformed design must not
  /// be able to carry weight it cannot mount.
  TEST_METHOD(ComponentsPastTheSlotCountAreIgnored)
  {
    // A Scout has one slot; this fills all four.
    const Outpost::DerivedStats overfilled =
      Outpost::Derive(Outpost::HullId::Scout, Outpost::DriveId::IonDrive, Fitted(Outpost::ComponentId::MassDriver, 4));
    const Outpost::DerivedStats correct =
      Outpost::Derive(Outpost::HullId::Scout, Outpost::DriveId::IonDrive, Fitted(Outpost::ComponentId::MassDriver, 1));

    Assert::AreEqual(correct.mass, overfilled.mass);
    Assert::AreEqual(correct.cost, overfilled.cost);
    Assert::AreEqual(correct.damagePerSecond, overfilled.damagePerSecond);
  }

  /// Q32: capacity and extraction rate are derived and summed over the hull's slots, so a two-slot
  /// miner is a table row rather than a mechanic.
  TEST_METHOD(MiningSumsOverTheSlots)
  {
    const Outpost::DerivedStats one =
      Outpost::Derive(Outpost::HullId::Frigate, Outpost::DriveId::IonDrive, Fitted(Outpost::ComponentId::MiningLaser, 1));
    const Outpost::DerivedStats two =
      Outpost::Derive(Outpost::HullId::Frigate, Outpost::DriveId::IonDrive, Fitted(Outpost::ComponentId::MiningLaser, 2));

    Assert::AreEqual(20u, one.orePerSecond);
    Assert::AreEqual(100u, one.oreCapacity);
    Assert::AreEqual(40u, two.orePerSecond);
    Assert::AreEqual(200u, two.oreCapacity);
    Assert::AreEqual(0u, two.damagePerSecond);
  }

  /// **THE MINING RANGE IS THE LONGEST TOOL'S, NOT A SUM** (M2.6), and only a tool that extracts has one:
  /// two lasers still reach 200, and a mass driver's 600 is not a mining range.
  TEST_METHOD(TheMiningRangeIsTheLongestMiningTool)
  {
    Assert::AreEqual(200u, Outpost::Derive(Outpost::DesignId::Miner).miningRangeUnits);
    Assert::AreEqual(
      200u,
      Outpost::Derive(Outpost::HullId::Frigate, Outpost::DriveId::IonDrive, Fitted(Outpost::ComponentId::MiningLaser, 2)).miningRangeUnits);
    Assert::AreEqual(0u, Outpost::Derive(Outpost::DesignId::Fighter).miningRangeUnits);
    Assert::AreEqual(0u, Outpost::Derive(Outpost::DesignId::Station).miningRangeUnits);
  }

  /// **ONLY THE STATION ACCEPTS ORE**, because its hull row says so -- which is what makes a mining factory
  /// later a row and not a branch (`GameDesign.md` section 4).
  /// M3.9: and the depot, which is what it is for (Q69).
  TEST_METHOD(OnlyTheStationAndTheDepotAcceptOre)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      Assert::AreEqual((hull.id == Outpost::HullId::Station) || (hull.id == Outpost::HullId::DepotFrame), hull.acceptsOre);
    }
    Assert::IsTrue(Outpost::Derive(Outpost::DesignId::Depot).acceptsOre);
    Assert::IsTrue(Outpost::Derive(Outpost::DesignId::Station).acceptsOre);
    Assert::IsFalse(Outpost::Derive(Outpost::DesignId::Miner).acceptsOre);
  }

  /// **EVERY MODULE AT EVERY LEVEL, PINNED BY LITERAL** (M2.9, ADR-015). A module is a `ModuleFrame` with one
  /// component and no drive, and the derivation needed no change to price it -- which is what this step proves.
  /// The literals are `GameDesign.md` section 5's table, so a level whose cost, hull or component moves fails
  /// here until this is updated with it.
  TEST_METHOD(EveryModuleIsPinnedAtEveryLevel)
  {
    struct Expected
    {
      Outpost::DesignId design;
      Outpost::ComponentId component;
      std::uint32_t cost;
      std::uint16_t multiplierPercent;
    };
    const Expected modules[] = {
      {Outpost::DesignId::ModuleShipyardL1, Outpost::ComponentId::ShipyardL1, 400, 150},
      {Outpost::DesignId::ModuleShipyardL2, Outpost::ComponentId::ShipyardL2, 700, 200},
      {Outpost::DesignId::ModuleOreProcessorL1, Outpost::ComponentId::OreProcessorL1, 350, 125},
      {Outpost::DesignId::ModuleOreProcessorL2, Outpost::ComponentId::OreProcessorL2, 600, 150},
    };

    for (const Expected& module : modules)
    {
      const Outpost::DesignEntry& design = Outpost::Design(module.design);
      Assert::IsTrue(design.hull == Outpost::HullId::ModuleFrame, L"a module is not on a module frame");
      Assert::IsTrue(design.drive == Outpost::DriveId::None, L"a module has a drive");
      Assert::IsTrue(design.slots[0] == module.component, L"a level carries the wrong component");
      Assert::IsFalse(design.buildable, L"a module is placed by a tap, not queued");
      Assert::AreEqual(static_cast<int>(module.multiplierPercent), static_cast<int>(Outpost::Component(design.slots[0]).multiplierPercent));

      const Outpost::DerivedStats stats = Outpost::Derive(module.design);
      Assert::AreEqual(module.cost, stats.cost, L"a module's price moved");
      Assert::AreEqual(1500u, stats.hullPoints);
      Assert::AreEqual(0u, stats.mass);
      Assert::AreEqual(0u, stats.speedUnitsPerSecond, L"a module moved");
      Assert::AreEqual(0u, stats.damagePerSecond);
      Assert::AreEqual(0u, stats.oreCapacity);
      Assert::IsFalse(stats.acceptsOre, L"a module accepts ore: an ore processor multiplies deliveries, it does not take them");
    }
  }

  /// **THE FRAME COSTS NOTHING**, so a module's price is its component's and nothing is counted twice.
  TEST_METHOD(TheModuleFrameAddsNoCost)
  {
    Assert::AreEqual(0, static_cast<int>(Outpost::Hull(Outpost::HullId::ModuleFrame).cost));
    Assert::AreEqual(1, static_cast<int>(Outpost::Hull(Outpost::HullId::ModuleFrame).slotCount));
  }

  /// `GameDesign.md` section 6's relations, asserted rather than left to a reader comparing rows.
  TEST_METHOD(TheBurnDriveIsMoreThrustForMoreMassAndMoreCost)
  {
    const Outpost::DriveEntry& ion = Outpost::Drive(Outpost::DriveId::IonDrive);
    const Outpost::DriveEntry& burn = Outpost::Drive(Outpost::DriveId::BurnDrive);

    Assert::IsTrue(burn.thrust > ion.thrust);
    Assert::IsTrue(burn.mass > ion.mass);
    Assert::IsTrue(burn.cost > ion.cost);
  }

  /// The question section 7 wants M4 to ask -- does speed counter mass -- needs the heavy hull to
  /// actually be the slow one. It is, by arithmetic.
  TEST_METHOD(TheCruiserIsTheSlowestThingThatMoves)
  {
    const Outpost::DerivedStats cruiser =
      Outpost::Derive(Outpost::HullId::Cruiser, Outpost::DriveId::BurnDrive, Fitted(Outpost::ComponentId::MassDriver, 4));
    Assert::IsTrue(cruiser.speedUnitsPerSecond < Outpost::Derive(Outpost::DesignId::Fighter).speedUnitsPerSecond);
    Assert::IsTrue(cruiser.speedUnitsPerSecond < Outpost::Derive(Outpost::DesignId::Miner).speedUnitsPerSecond);
  }
};

} // namespace GameCoreTests
