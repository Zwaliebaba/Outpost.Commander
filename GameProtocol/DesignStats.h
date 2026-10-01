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

// The damage one hit does to a target, in hundredths: max(damage × 0.25, damage − armor) (design §7). The quarter is
// rounded down to a hundredth, which no number in the tuning data needs.
[[nodiscard]] constexpr std::int32_t HitHundredths(std::int32_t _damageHundredths, std::int32_t _armorHundredths) noexcept
{
  return std::max(_damageHundredths / 4, _damageHundredths - _armorHundredths);
}

// One hull, drive and weapon.
struct DesignComponents
{
  HullId hull;
  DriveId drive;
  WeaponId weapon;

  friend constexpr bool operator==(const DesignComponents&, const DesignComponents&) = default;
};

// What every ship of one hull, drive and weapon is (design §7): the numbers the designer shows and combat uses. The server
// and the designer derive them with the same function, DesignStatsOf, from the same component numbers (ADR-017).
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
  // Every other enemy whose center is this close to the target's takes the same hit, after its own armor; zero for a
  // weapon without splash (design §7, ADR-014).
  float splashRadiusMeters = 0.0f;

  friend bool operator==(const DesignStats&, const DesignStats&) = default;
};

// The stats of a ship of this hull, drive and weapon, from the numbers as the player has them (design §7).
[[nodiscard]] inline DesignStats DesignStatsOf(const HullView& _hull, const DriveView& _drive, const WeaponView& _weapon) noexcept
{
  constexpr double RADIANS_PER_DEGREE = std::numbers::pi / 180.0;
  return {.movement = {.speedMetersPerSecond = static_cast<float>(_hull.speedMetersPerSecond * _drive.speedFactor),
                       .turnRateRadiansPerSecond =
                         static_cast<float>(_hull.turnRateDegreesPerSecond * _drive.turnRateFactor * RADIANS_PER_DEGREE),
                       .radiusMeters = static_cast<float>(_hull.footprintRadiusMeters)},
          .hitPointsHundredths = static_cast<std::int32_t>(std::llround(_hull.hitPointsHundredths * _drive.hitPointsFactor)),
          .armorHundredths = _hull.armorHundredths,
          .cost = _hull.cost + _drive.cost + _weapon.cost,
          .buildSeconds = _hull.buildSeconds,
          .damageHundredths = _weapon.damageHundredths,
          .fireIntervalSeconds = _weapon.fireIntervalSeconds,
          .rangeMeters = static_cast<float>(_weapon.rangeMeters),
          .splashRadiusMeters = static_cast<float>(_weapon.splashRadiusMeters)};
}

// Damage per second against a target of _armorHundredths, after armor (design §9: what the designer shows).
[[nodiscard]] inline double DamagePerSecond(const DesignStats& _stats, std::int32_t _armorHundredths) noexcept
{
  if (_stats.fireIntervalSeconds <= 0.0)
    return 0.0;
  return static_cast<double>(HitHundredths(_stats.damageHundredths, _armorHundredths)) / HUNDREDTHS / _stats.fireIntervalSeconds;
}
} // namespace Outpost
