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
// sector is a rectangle, and two sectors are adjacent when they share a border. Its kind names what the match's seed
// places in it (ADR-072); a sector without one has nothing placed.
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
  std::string kind;

  // Whether _position is in the sector, its borders included.
  [[nodiscard]] bool Contains(PlanePosition _position) const noexcept
  {
    return _position.xMeters >= minXMeters && _position.xMeters <= maxXMeters && _position.zMeters >= minZMeters &&
           _position.zMeters <= maxZMeters;
  }

  friend bool operator==(const SectorPlacement&, const SectorPlacement&) = default;
};

// Ore asteroids of one yield that the seed places in each sector of a kind, with the radius and reserve each has
// (ADR-072).
struct OreRule
{
  OreYield yield = OreYield::Home;
  std::int32_t count = 0;
  float radiusMeters = 0.0f;
  std::int32_t reserveOre = 0;

  friend bool operator==(const OreRule&, const OreRule&) = default;
};

// What the seed places in every sector of one kind (Phase 4 design §7, ADR-072).
struct SectorKind
{
  std::string name;
  std::vector<OreRule> ore;

  friend bool operator==(const SectorKind&, const SectorKind&) = default;
};

// How far what the seed places keeps from its sector's borders, from the sector's node, and an ore asteroid's center from
// another's, on top of the map's minimum gap between obstacles (ADR-072).
struct PlacementRules
{
  float borderMeters = 0.0f;
  float nodeClearanceMeters = 0.0f;
  float oreSpacingMeters = 0.0f;
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
  // What the seed places, by the sectors' kinds; none in a map that lists every asteroid (ADR-072).
  std::vector<SectorKind> sectorKinds;
  PlacementRules placement;
};

// Reads the text of OutpostCommander/Assets/Map.json. Throws Neuron::Exception on the first problem, naming where it is,
// such as "oreAsteroids[3].radiusMeters". Every ore asteroid states its reserve; sectors are optional. Besides types and
// ranges it checks that every sector's identifier is unique, its rectangle is on the map with its node inside it, its
// node keeps the minimum gap from every obstacle, and its adjacency names other sectors and is returned by them. It
// checks that there are two starts, and that every obstacle, the edge and every start keep the minimum gap from each
// other.
// Its sector kinds are checked too: each named once, each sector's kind one of them, and the map's kinds point-symmetric,
// the sector across the center from each being of the same kind, with an even count of each yield in a sector that is
// its own mirror.
[[nodiscard]] Map LoadMap(std::string_view _json);

// _map with the ore asteroids its sector kinds place, drawn from _seed (Phase 4 design §7, ADR-072). Each pair of sectors
// across the center from each other is drawn once, the one with the lower identifier, and the other gets the same
// asteroids turned half a turn about the center; a sector that is its own mirror gets half its count drawn and their
// mirrors. Each asteroid is drawn inside its sector, the placement's border and its radius in from the edges, the node
// clearance from its node, the minimum gap from every obstacle and start, and the ore spacing from every ore asteroid. The
// same map and seed place the same asteroids on the same build (ADR-009). Throws Neuron::Exception when a sector has no
// room for what its kind places. A map without sector kinds comes back as it is.
[[nodiscard]] Map PlaceContent(Map _map, std::uint64_t _seed);
} // namespace Outpost