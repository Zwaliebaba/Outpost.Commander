#pragma once

namespace GameLogicTests
{
// Design §3's Q2 check, played by the real simulation instead of Tools/BattleModel.py's abstract clumps (task 3.4). It
// follows the model's method: the same stages, budgets, fire modes, criteria (a)-(d) and confidence intervals. Two things
// differ, and both are what the simulation is.
//   - Armies are whole ships. The model fields the Ore left over as a fractional ship; the simulation cannot. Battle k
//     of n spends the budget at the center of the k-th of n equal slices of the ±15% window, and buys as many whole
//     ships as that affords. The Ore left over is not fielded.
//   - Each side is a grid of real ships with real footprints, which close by attack-move, stand at their own range and
//     fire under the simulation's targeting, forced to spread or focus fire. The model's clump puts every ship in range
//     of every enemy at once.

// A hull, drive or weapon as the check perturbs it: every number the model moves by 5% is a double here.
struct CheckHull
{
  Outpost::HullId id;
  std::string name;
  double hitPoints = 0.0;
  double armor = 0.0;
  double speedMetersPerSecond = 0.0;
  double cost = 0.0;
};

struct CheckDrive
{
  Outpost::DriveId id;
  std::string name;
  double speedFactor = 0.0;
  double hitPointsFactor = 0.0;
  double cost = 0.0;
};

struct CheckWeapon
{
  Outpost::WeaponId id;
  std::string name;
  double damage = 0.0;
  double fireIntervalSeconds = 0.0;
  double rangeMeters = 0.0;
  double cost = 0.0;
  double splashRadiusMeters = 0.0;
};

struct CheckParts
{
  std::vector<CheckHull> hulls;
  std::vector<CheckDrive> drives;
  std::vector<CheckWeapon> weapons;
};

// One design as the check fields it.
struct CheckDesign
{
  // The model's short code, such as "S+I+MD".
  std::string code;
  std::string hull;
  std::string drive;
  std::string weapon;
  Outpost::DesignComponents components;
  Outpost::DesignStats stats;
  // The stats' cost as a double, since the robustness sweep moves it by 5%.
  double cost = 0.0;
};

enum class FireMode : std::uint8_t
{
  Spread,
  Focus
};

struct CheckOptions
{
  std::vector<double> budgets{2000, 3000, 4500, 6000, 9000, 12000};
  std::vector<double> earlyBudgets{2000, 3000, 4500};
  std::uint32_t battles = 60;
  std::uint32_t robustBattles = 30;
  std::uint32_t maxBattles = 480;
  // Skip criterion (c), the robustness sweep.
  bool quick = false;
  // Worker threads; zero for one per hardware thread.
  std::uint32_t threads = 0;
  // Play every battle under fog of war with this sight (ADR-024). Nothing a ship fires at depends on it, so the verdicts
  // are the same with it and without.
  std::optional<Outpost::SightTuning> fog;
};

struct CheckResult
{
  // "PASS", "FAIL", "UNSURE" or "NOT RUN" for each of (a)-(d).
  std::array<std::string, 4> verdicts;
  std::string report;
  [[nodiscard]] bool Passed() const;
};

// The hulls, drives and weapons of the tuning data, as the check moves them.
[[nodiscard]] CheckParts PartsFrom(const Outpost::Tuning& _tuning);

// The parts of PartsFrom that no research topic unlocks, or that a topic of _tier or an earlier one unlocks.
[[nodiscard]] CheckParts PartsThrough(const Outpost::Tuning& _tuning, std::int32_t _tier);

// Every design of the parts: every hull, drive and weapon, the Missile Rack's splash included (task 5.3).
[[nodiscard]] std::vector<CheckDesign> DesignsFrom(const Outpost::Tuning& _tuning, const CheckParts& _parts);

// One battle: battle _battle of _battles between _a and _b, at _budgetOre each give or take the window, under fog of war
// with _fog's sight when there is one. +1 when _a wins, -1 when _b wins, 0 for a draw, which is both sides destroyed in
// one tick or both standing after ten minutes.
[[nodiscard]] int Fight(const CheckDesign& _a, const CheckDesign& _b, double _budgetOre, FireMode _mode, std::uint32_t _battle,
                        std::uint32_t _battles, const std::optional<Outpost::SightTuning>& _fog = std::nullopt);

// The check, against _tuning.
[[nodiscard]] CheckResult RunQ2Check(const Outpost::Tuning& _tuning, const CheckOptions& _options);
} // namespace GameLogicTests