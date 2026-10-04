#pragma once

namespace Outpost
{
// What an ore asteroid yields, as the tuning data's rules name it: Phase 1 design §8's rings, home outward.
enum class OreYield : std::uint8_t
{
  Home,
  Near,
  Contested,
  Rich
};

// A mineable asteroid: one Mining Rig snaps to it (design §5, §6). It blocks movement as a circle.
struct OreAsteroidPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
  OreYield yield = OreYield::Home;
  // The Ore it holds before its rig earns only the trickle (Phase 1 design §8). Every asteroid of the map data has one;
  // one placed without, as a test may, never runs out.
  std::optional<std::int32_t> reserveOre;
};

// A non-mineable asteroid field: a circular obstacle, and with its neighbors a chokepoint (design §4).
struct AsteroidFieldPlacement
{
  PlanePosition position;
  float radiusMeters = 0.0f;
};

// An area of the map with one node site, where territory is claimed (Phase 1 design §8; Phase 2 design §4, ADR-056). A
// sector is a rectangle, and two sectors are adjacent when they share a border.
struct SectorPlacement
{
  std::int32_t id = 0;
  std::string name;
  float minXMeters = 0.0f;
  float maxXMeters = 0.0f;
  float minZMeters = 0.0f;
  float maxZMeters = 0.0f;
  PlanePosition node;
  std::vector<std::int32_t> adjacent;

  // Whether _position is in the sector, its borders included.
  [[nodiscard]] bool Contains(PlanePosition _position) const noexcept
  {
    return _position.xMeters >= minXMeters && _position.xMeters <= maxXMeters && _position.zMeters >= minZMeters &&
           _position.zMeters <= maxZMeters;
  }

  friend bool operator==(const SectorPlacement&, const SectorPlacement&) = default;
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
  // None in a map without them.
  std::vector<SectorPlacement> sectors;
};

// Reads the text of OutpostCommander/Assets/Map.json. Throws Neuron::Exception on the first problem, naming where it is,
// such as "oreAsteroids[3].radiusMeters". Every ore asteroid states its reserve; sectors are optional. Besides types and
// ranges it checks that every sector's identifier is unique, its rectangle is on the map with its node inside it, its
// node keeps the minimum gap from every obstacle, and its adjacency names other sectors and is returned by them. It
// checks that there are two starts, and that every obstacle, the edge and every start keep the minimum gap from each
// other.
[[nodiscard]] Map LoadMap(std::string_view _json);
} // namespace Outpost