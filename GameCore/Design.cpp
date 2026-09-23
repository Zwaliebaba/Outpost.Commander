// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Design.h"

namespace Outpost
{

namespace
{
/// ADR-006's design table, and the whole of what the MVP builds. **Seven rows and no types.**
constexpr std::array<DesignEntry, 7> DESIGNS{{
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
}};

// AN IDENTITY IS AN INDEX: a row added without its enumerator, or the reverse, refuses to compile.
static_assert(DESIGNS.size() == static_cast<std::size_t>(DesignId::ModuleOreProcessorL2) + 1);
} // namespace

std::span<const DesignEntry> Designs() noexcept
{
  return DESIGNS;
}

const DesignEntry& Design(DesignId _id) noexcept
{
  return DESIGNS[static_cast<std::size_t>(_id)];
}

} // namespace Outpost
