#pragma once

namespace Outpost
{
// What an ore asteroid yields, as design §5 and the tuning data's rules name it.
enum class OreYield : std::uint8_t
{
  Home,
  Contested
};

// A mineable asteroid: one Mining Rig snaps to it (design §5, §6). It blocks movement as a circle.
struct OreAsteroidPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
  OreYield yield = OreYield::Home;
};

// A non-mineable asteroid field: a circular obstacle, and with its neighbors a chokepoint (design §4).
struct AsteroidFieldPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
};

// OutpostCommander/Assets/Map.json as the game holds it (design §4, ADR-008). The map is a square centered on the origin.
struct Map
{
  float sizeMeters = 0.0f;
  // The narrowest passage the map allows: between any two obstacles, between an obstacle and the edge, and around a
  // start. Every gap being at least this wide is what keeps every part of the map reachable (MapTests).
  float minimumGapMeters = 0.0f;
  // One per player, in player order: the first is player 1's.
  std::vector<PlanePosition> starts;
  std::vector<OreAsteroidPlacement> oreAsteroids;
  std::vector<AsteroidFieldPlacement> asteroidFields;
};

// Reads the text of OutpostCommander/Assets/Map.json. Throws Neuron::Exception on the first problem, naming where it is,
// such as "oreAsteroids[3].radiusMeters". Besides types and ranges it checks that there are two starts, and that every
// obstacle, the edge and every start keep the minimum gap from each other.
[[nodiscard]] Map LoadMap(std::string_view _json);
} // namespace Outpost