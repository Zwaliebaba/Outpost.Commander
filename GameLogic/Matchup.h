#pragma once

namespace Outpost
{
// One group of a matchup's fleet: ships of one design, by its components and module, and how many (horizon §9, ADR-083).
struct MatchupGroup
{
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  // None for a design without a module.
  ModuleId module;
  std::int32_t count = 0;
};

// A battle matchup: a fleet for each of the two players, which fight with no bases on the map, starting this far apart.
struct Matchup
{
  std::string name;
  float distanceMeters = 0.0f;
  std::array<std::vector<MatchupGroup>, 2> sides;
};

// The matchups file, Matchups.json beside the tuning data and the map. Each names components _tuning has, and each side
// has a ship at least. Throws Neuron::Exception naming what is wrong.
[[nodiscard]] std::vector<Matchup> LoadMatchups(std::string_view _json, const Tuning& _tuning);
} // namespace Outpost
