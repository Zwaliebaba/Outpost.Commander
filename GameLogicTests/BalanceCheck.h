#pragma once

namespace GameLogicTests
{
// The balance check of design §3, played by the real simulation instead of Tools/BattleModel.py's abstract clumps (task
// 3.4). The MVP's documents and the ADRs call it the Q2 check, after the MVP's question it answers: does ship design
// matter? It follows the model's method: the same stages, budgets, fire modes, criteria (a)-(d) and confidence
// intervals. Two things differ, and both are what the simulation is.
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
  // Ore a side at each stage (Phase 1 design §7): tier 1's, the MVP's every component; the starting components' and tier
  // 1's research; and tiers 2 and 3, whose research is played at their own budgets.
  std::vector<double> budgets{2000, 3000, 4500, 6000, 9000, 12000};
  std::vector<double> earlyBudgets{2000, 3000, 4500};
  std::vector<double> tierTwoBudgets{4500, 6000, 9000, 12000};
  std::vector<double> tierThreeBudgets{6000, 9000, 12000};
  // The last tier played; 1 is the MVP's check.
  std::int32_t lastTier = Outpost::RESEARCH_TIERS;
  // Drives whose case is speed, which a battle between two groups cannot see: (b) reports them when no design worth
  // building uses them, and does not fail on it (Phase 1 design §5, §7; owner, 2026-10-02, gate H4).
  std::vector<std::string> speedDrives{"Pulse"};
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
[[nodiscard]] CheckResult RunBalanceCheck(const Outpost::Tuning& _tuning, const CheckOptions& _options);
} // namespace GameLogicTests