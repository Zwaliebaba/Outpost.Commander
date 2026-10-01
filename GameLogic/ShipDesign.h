#pragma once

namespace Outpost
{
// How a ship moves: from its hull and drive (design §7, §12), set when it is spawned.
struct ShipMovement
{
  float speedMetersPerSecond = 0.0f;
  float turnRateRadiansPerSecond = 0.0f;
  // Its footprint: the circle it keeps clear of obstacles and of other ships.
  float radiusMeters = 0.0f;

  friend constexpr bool operator==(const ShipMovement&, const ShipMovement&) = default;
};

// A ship of this hull and drive: the hull's speed and turn rate times the drive's factors, and the hull's footprint.
// Throws Neuron::Exception when either identifier names nothing in _tuning.
[[nodiscard]] ShipMovement MovementFor(const Tuning& _tuning, HullId _hull, DriveId _drive);

// Hit points, armor and damage count in hundredths of a point (ADR-014): a drive's factor and the armor rule's quarter of
// a hit then stay whole numbers, which the tuning data's integers are not once they are multiplied.
inline constexpr std::int32_t HUNDREDTHS = 100;

// The damage one hit does to a target, in hundredths: max(damage × 0.25, damage − armor) (design §7). The quarter is
// rounded down to a hundredth, which no number in the tuning data needs.
[[nodiscard]] constexpr std::int32_t HitHundredths(std::int32_t _damageHundredths, std::int32_t _armorHundredths) noexcept
{
  return std::max(_damageHundredths / 4, _damageHundredths - _armorHundredths);
}

// What every ship of one hull, drive and weapon is (design §7): the numbers the designer shows and combat uses, derived
// from the tuning data.
struct DesignStats
{
  ShipMovement movement;
  // The hull's hit points times the drive's factor.
  std::int32_t hitPointsHundredths = 0;
  std::int32_t armorHundredths = 0;
  // The hull's, the drive's and the weapon's, added.
  std::int32_t cost = 0;
  double buildSeconds = 0.0;
  // A hit before armor, and how often it comes.
  std::int32_t damageHundredths = 0;
  double fireIntervalSeconds = 0.0;
  float rangeMeters = 0.0f;

  friend bool operator==(const DesignStats&, const DesignStats&) = default;
};

// The stats of a ship of this hull, drive and weapon. Throws Neuron::Exception when an identifier names nothing in
// _tuning.
[[nodiscard]] DesignStats DesignStatsFor(const Tuning& _tuning, HullId _hull, DriveId _drive, WeaponId _weapon);

// Damage per second against a target of _armorHundredths, after armor (design §9: what the designer shows).
[[nodiscard]] double DamagePerSecond(const DesignStats& _stats, std::int32_t _armorHundredths) noexcept;

// One hull, drive and weapon.
struct DesignComponents
{
  HullId hull;
  DriveId drive;
  WeaponId weapon;

  friend constexpr bool operator==(const DesignComponents&, const DesignComponents&) = default;
};

// A player's saved design (design §7). The server numbers designs as they are saved.
struct ShipDesign
{
  DesignId id;
  PlayerId owner;
  std::string name;
  DesignComponents components;
  DesignStats stats;

  friend bool operator==(const ShipDesign&, const ShipDesign&) = default;
};

// Every design of the components no research topic unlocks, which every player starts with saved (design §7, §8): by
// hull, then drive, then weapon, in the tuning data's order.
[[nodiscard]] std::vector<DesignComponents> StartingDesigns(const Tuning& _tuning);

// A design's name as design §7 writes it, such as "Small+Ion+Mass Driver". Throws Neuron::Exception when an identifier
// names nothing in _tuning.
[[nodiscard]] std::string DesignName(const Tuning& _tuning, const DesignComponents& _components);
} // namespace Outpost
