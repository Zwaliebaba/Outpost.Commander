// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Design.h"

namespace Outpost
{

namespace
{
/// ADR-006's design table, and the whole of what the game builds. **Ten rows and no types.**
constexpr std::array<DesignEntry, 10> DESIGNS{{
  {.id = DesignId::Miner,
   .hull = HullId::Scout,
   .drive = DriveId::IonDrive,
   .slots = {ComponentId::MiningLaser, ComponentId::None, ComponentId::None, ComponentId::None},
   .buildable = true},

  {.id = DesignId::Fighter,
   .hull = HullId::Frigate,
   .drive = DriveId::BurnDrive,
   .slots = {ComponentId::MassDriver, ComponentId::MassDriver, ComponentId::None, ComponentId::None},
   .buildable = true},

  // THE STATION IS A ROW IN THE SAME TABLE, which is what makes `GameDesign.md` section 5 possible
  // without a second kind of thing in the simulation: no drive, two point-defense mounts, and the
  // derivation handles it with no branch (ADR-015, R24).
  {.id = DesignId::Station,
   .hull = HullId::Station,
   .drive = DriveId::None,
   .slots = {ComponentId::PointDefense, ComponentId::PointDefense, ComponentId::None, ComponentId::None},

   // NOT BUILDABLE. A station is placed by the generator (`GameCore/Layout.h`), and a build menu
   // that could order one would be a second station on top of the first.
   .buildable = false},

  // === THE MODULES (M2.9, ADR-015). ============================================================
  //
  // **A MODULE IS A COMPOSITION AND THE DERIVATION DID NOT CHANGE FOR IT**, which is the thing this step
  // proves: a frame, no drive, one component in its one slot. Its cost is the frame's -- nothing -- plus the
  // component's, which is `GameDesign.md` section 5's price for that level. Not buildable, because a module
  // is placed by a tap rather than queued (M2.11).
  {.id = DesignId::ModuleShipyardL1,
   .hull = HullId::ModuleFrame,
   .drive = DriveId::None,
   .slots = {ComponentId::ShipyardL1, ComponentId::None, ComponentId::None, ComponentId::None}},
  {.id = DesignId::ModuleShipyardL2,
   .hull = HullId::ModuleFrame,
   .drive = DriveId::None,
   .slots = {ComponentId::ShipyardL2, ComponentId::None, ComponentId::None, ComponentId::None}},
  {.id = DesignId::ModuleOreProcessorL1,
   .hull = HullId::ModuleFrame,
   .drive = DriveId::None,
   .slots = {ComponentId::OreProcessorL1, ComponentId::None, ComponentId::None, ComponentId::None}},
  {.id = DesignId::ModuleOreProcessorL2,
   .hull = HullId::ModuleFrame,
   .drive = DriveId::None,
   .slots = {ComponentId::OreProcessorL2, ComponentId::None, ComponentId::None, ComponentId::None}},

  // === THE DEPOT (M3.9, Q69). ==================================================================
  {.id = DesignId::Depot,
   .hull = HullId::DepotFrame,
   .drive = DriveId::None,
   .slots = {ComponentId::None, ComponentId::None, ComponentId::None, ComponentId::None}},

  // === THE CRUISER (M4.4, Q75). ================================================================
  //
  // **A TABLE ROW, WHICH IS ADR-006'S CLAIM.** 2,080 for the hull, 80 for the drive and 60 a gun is 2,400; mass 60, 10
  // and four fives is 90, and 5,600 over 90 is 62 units a second. Nobody wrote either figure down here.
  {.id = DesignId::Cruiser,
   .hull = HullId::Cruiser,
   .drive = DriveId::BurnDrive,
   .slots = {ComponentId::HeavyDriver, ComponentId::HeavyDriver, ComponentId::HeavyDriver, ComponentId::HeavyDriver},
   .buildable = true},

  // === THE RESEARCH STATION (M4.4b, Q85). ========================================================
  {.id = DesignId::ModuleResearchStationL1,
   .hull = HullId::ModuleFrame,
   .drive = DriveId::None,
   .slots = {ComponentId::ResearchStationL1, ComponentId::None, ComponentId::None, ComponentId::None}},
}};

// AN IDENTITY IS AN INDEX: a row added without its enumerator, or the reverse, refuses to compile.
static_assert(DESIGNS.size() == static_cast<std::size_t>(DesignId::ModuleResearchStationL1) + 1);
} // namespace

std::span<const DesignEntry> Designs() noexcept
{
  return DESIGNS;
}

const DesignEntry& Design(DesignId _id) noexcept
{
  return DESIGNS[static_cast<std::size_t>(_id)];
}

std::uint8_t RequiredUnlocks(DesignId _design) noexcept
{
  if (static_cast<std::size_t>(_design) >= DESIGNS.size())
  {
    return 0;
  }
  std::uint8_t required = 0;
  for (const ComponentId slot : Design(_design).slots)
  {
    required = static_cast<std::uint8_t>(required | Component(slot).unlockBit);
  }
  return required;
}

bool DesignUnlocked(DesignId _design, std::uint8_t _unlocked) noexcept
{
  const std::uint8_t required = RequiredUnlocks(_design);
  return (required & _unlocked) == required;
}

std::uint8_t RequiredShipyardLevel(DesignId _design) noexcept
{
  if (static_cast<std::size_t>(_design) >= DESIGNS.size())
  {
    return 0;
  }
  return Hull(Design(_design).hull).shipyardLevelRequired;
}

std::uint8_t ShipyardLevelOf(std::span<const DesignId> _modules) noexcept
{
  std::uint8_t level = 0;
  for (const DesignId module : _modules)
  {
    if (static_cast<std::size_t>(module) >= DESIGNS.size())
    {
      continue;
    }
    for (const ComponentId slot : Design(module).slots)
    {
      level = (Component(slot).shipyardLevel > level) ? Component(slot).shipyardLevel : level;
    }
  }
  return level;
}

} // namespace Outpost
