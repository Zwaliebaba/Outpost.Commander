#pragma once

namespace Outpost
{
// A ship of this hull and drive: the hull's speed and turn rate times the drive's factors, and the hull's footprint.
// Throws Neuron::Exception when either identifier names nothing in _tuning.
[[nodiscard]] ShipMovement MovementFor(const Tuning& _tuning, HullId _hull, DriveId _drive);

// A component's numbers as a player with these upgrades has it, and whether it is available to the player (ADR-017). A
// hull's hit points are raised by the upgrades' factor and a weapon's fire interval shortened by it (design §8).
[[nodiscard]] HullView ViewOf(const HullTuning& _hull, const Upgrades& _upgrades, bool _available);
[[nodiscard]] DriveView ViewOf(const DriveTuning& _drive, bool _available);
[[nodiscard]] WeaponView ViewOf(const WeaponTuning& _weapon, const Upgrades& _upgrades, bool _available);
// A module is always available (ADR-058).
[[nodiscard]] ModuleView ViewOf(const ModuleTuning& _module);

// The stats of a ship of this hull, drive and weapon, for a player with these upgrades: DesignStatsOf over the
// components' views, as the designer derives them (ADR-017). Throws Neuron::Exception when an identifier names nothing
// in _tuning.
[[nodiscard]] DesignStats DesignStatsFor(const Tuning& _tuning, HullId _hull, DriveId _drive, WeaponId _weapon,
                                         const Upgrades& _upgrades = {});
// The same for a design with its module, if it has one (Phase 2 design §10).
[[nodiscard]] DesignStats DesignStatsFor(const Tuning& _tuning, const DesignComponents& _components, const Upgrades& _upgrades = {});

// A player's saved design (design §7). The server numbers designs as they are saved.
struct ShipDesign
{
  DesignId id;
  PlayerId owner;
  std::string name;
  DesignComponents components;
  DesignStats stats;
  // What every ship built to it starts with (Phase 4 design §10, ADR-075).
  RetreatThreshold retreat = DEFAULT_RETREAT;

  friend bool operator==(const ShipDesign&, const ShipDesign&) = default;
};

// Every design of the components no research topic unlocks, which every player starts with saved (design §7, §8): by
// hull, then drive, then weapon, in the tuning data's order.
[[nodiscard]] std::vector<DesignComponents> StartingDesigns(const Tuning& _tuning);

// A design's name as design §7 writes it, such as "Small+Ion+Mass Driver", with its module after a fourth plus when it
// has one. Throws Neuron::Exception when an identifier names nothing in _tuning.
[[nodiscard]] std::string DesignName(const Tuning& _tuning, const DesignComponents& _components);

// The name a starting design is saved under: the short name the tuning data gives it, such as "Swarm", or its components'
// name (ADR-069).
[[nodiscard]] std::string StartingDesignName(const Tuning& _tuning, const DesignComponents& _components);
} // namespace Outpost