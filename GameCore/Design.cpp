// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Design.h"

namespace Outpost
{

namespace
{
/// ADR-006's design table, and the whole of what the MVP builds. **Three rows and no types.**
constexpr std::array<DesignEntry, 3> DESIGNS{{
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
}};

static_assert(DESIGNS.size() == static_cast<std::size_t>(DesignId::Station) + 1);
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
