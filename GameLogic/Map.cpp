#include "pch.h"
#include "Map.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
using Neuron::JsonBound;
using Neuron::JsonObjectReader;

// The MVP is one player against one AI (design §10).
constexpr size_t PLAYER_COUNT = 2;
// Mixed into the match's seed for placement, so that its draws are not the ones the simulation's own generator makes from
// the same seed (ADR-072).
constexpr std::uint64_t PLACEMENT_SEED_SALT = 0x9e3779b97f4a7c15;
// How many places are drawn for one asteroid before its sector is found to have no room for it.
constexpr int PLACEMENT_ATTEMPTS = 10000;

Outpost::PlanePosition ReadPosition(JsonObjectReader& _reader)
{
  return {.xMeters = static_cast<float>(_reader.Number("xMeters", JsonBound::Any)),
          .zMeters = static_cast<float>(_reader.Number("zMeters", JsonBound::Any))};
}

Outpost::OreYield ReadYield(JsonObjectReader& _reader)
{
  const std::string yield = _reader.String("yield");
  if (yield == "home")
    return Outpost::OreYield::Home;
  if (yield == "near")
    return Outpost::OreYield::Near;
  if (yield == "contested")
    return Outpost::OreYield::Contested;
  if (yield != "rich")
    Neuron::JsonFail(_reader.PathOf("yield"), std::format("\"{}\" is not \"home\", \"near\", \"contested\" or \"rich\"", yield));
  return Outpost::OreYield::Rich;
}

Outpost::OreAsteroidPlacement ReadOreAsteroid(JsonObjectReader& _reader)
{
  Outpost::OreAsteroidPlacement asteroid;
  asteroid.position = ReadPosition(_reader);
  asteroid.radiusMeters = static_cast<float>(_reader.Number("radiusMeters", JsonBound::Positive));
  asteroid.yield = ReadYield(_reader);
  asteroid.reserveOre = _reader.Integer("reserve", 1);
  return asteroid;
}

Outpost::OreRule ReadOreRule(JsonObjectReader& _reader)
{
  Outpost::OreRule rule;
  rule.yield = ReadYield(_reader);
  rule.count = _reader.Integer("count", 1);
  rule.radiusMeters = static_cast<float>(_reader.Number("radiusMeters", JsonBound::Positive));
  rule.reserveOre = _reader.Integer("reserve", 1);
  return rule;
}

Outpost::SectorKind ReadSectorKind(JsonObjectReader& _reader)
{
  Outpost::SectorKind kind;
  kind.name = _reader.String("name");
  kind.ore = Neuron::ReadJsonList<Outpost::OreRule>(_reader, "ore", ReadOreRule);
  return kind;
}

Outpost::OutpostRule ReadOutpostRule(JsonObjectReader& _reader)
{
  return {.outpost = _reader.String("outpost"), .kind = _reader.String("kind"), .count = _reader.Integer("count", 1)};
}

Outpost::DerelictRule ReadDerelictRule(JsonObjectReader& _reader)
{
  return {.kind = _reader.String("kind"),
          .count = _reader.Integer("count", 1),
          .ore = _reader.Integer("ore", 1),
          .hull = _reader.Identifier<Outpost::HullId>("hull"),
          .radiusMeters = static_cast<float>(_reader.Number("radiusMeters", JsonBound::Positive))};
}

Outpost::PlacementRules ReadPlacementRules(JsonObjectReader& _reader)
{
  return {.borderMeters = static_cast<float>(_reader.Number("borderMeters", JsonBound::NotNegative)),
          .nodeClearanceMeters = static_cast<float>(_reader.Number("nodeClearanceMeters", JsonBound::NotNegative)),
          .oreSpacingMeters = static_cast<float>(_reader.Number("oreSpacingMeters", JsonBound::NotNegative))};
}

Outpost::AsteroidFieldPlacement ReadAsteroidField(JsonObjectReader& _reader)
{
  Outpost::AsteroidFieldPlacement field;
  field.position = ReadPosition(_reader);
  field.radiusMeters = static_cast<float>(_reader.Number("radiusMeters", JsonBound::Positive));
  return field;
}

Outpost::SectorPlacement ReadSector(JsonObjectReader& _reader)
{
  Outpost::SectorPlacement sector;
  sector.id = _reader.Integer("id", 1);
  sector.name = _reader.String("name");
  sector.minXMeters = static_cast<float>(_reader.Number("minXMeters", JsonBound::Any));
  sector.maxXMeters = static_cast<float>(_reader.Number("maxXMeters", JsonBound::Any));
  sector.minZMeters = static_cast<float>(_reader.Number("minZMeters", JsonBound::Any));
  sector.maxZMeters = static_cast<float>(_reader.Number("maxZMeters", JsonBound::Any));
  JsonObjectReader node(_reader.Required("node"), _reader.PathOf("node"));
  sector.node = ReadPosition(node);
  node.Finish();
  const std::string adjacentPath = _reader.PathOf("adjacent");
  const Neuron::JsonValue::Array& adjacent = Neuron::ReadJsonArray(_reader.Required("adjacent"), adjacentPath);
  for (size_t i = 0; i < adjacent.size(); ++i)
    sector.adjacent.push_back(Neuron::ReadJsonInteger(adjacent[i], Neuron::JsonElementPath(adjacentPath, i), 1));
  if (_reader.Optional("kind") != nullptr)
    sector.kind = _reader.String("kind");
  return sector;
}

// An obstacle, with where it came from for the error message.
struct Circle
{
  Outpost::PlanePosition position;
  float radiusMeters = 0.0f;
  std::string path;
};

// How far a point is inside the square map's edge.
double EdgeClearance(const Outpost::Map& _map, Outpost::PlanePosition _position) noexcept
{
  const double half = _map.sizeMeters / 2.0;
  return std::min(half - std::abs(static_cast<double>(_position.xMeters)), half - std::abs(static_cast<double>(_position.zMeters)));
}

void CheckGaps(const Outpost::Map& _map)
{
  std::vector<Circle> circles;
  circles.reserve(_map.oreAsteroids.size() + _map.asteroidFields.size());
  for (size_t i = 0; i < _map.oreAsteroids.size(); ++i)
    circles.push_back({_map.oreAsteroids[i].position, _map.oreAsteroids[i].radiusMeters, Neuron::JsonElementPath("oreAsteroids", i)});
  for (size_t i = 0; i < _map.asteroidFields.size(); ++i)
    circles.push_back({_map.asteroidFields[i].position, _map.asteroidFields[i].radiusMeters, Neuron::JsonElementPath("asteroidFields", i)});

  const double gap = _map.minimumGapMeters;
  for (size_t i = 0; i < circles.size(); ++i)
  {
    const Circle& circle = circles[i];
    if (EdgeClearance(_map, circle.position) - circle.radiusMeters < gap)
      Neuron::JsonFail(circle.path, std::format("is closer than {} m to the edge of the map", gap));
    for (size_t j = 0; j < i; ++j)
    {
      const Circle& other = circles[j];
      if (Distance(circle.position, other.position) - circle.radiusMeters - other.radiusMeters < gap)
        Neuron::JsonFail(circle.path, std::format("is closer than {} m to {}", gap, other.path));
    }
  }

  for (size_t i = 0; i < _map.starts.size(); ++i)
  {
    const Outpost::PlanePosition start = _map.starts[i];
    const std::string path = Neuron::JsonElementPath("starts", i);
    if (EdgeClearance(_map, start) < gap)
      Neuron::JsonFail(path, std::format("is closer than {} m to the edge of the map", gap));
    for (const Circle& circle : circles)
    {
      if (Distance(start, circle.position) - circle.radiusMeters < gap)
        Neuron::JsonFail(path, std::format("is closer than {} m to {}", gap, circle.path));
    }
  }
}

void CheckSectors(const Outpost::Map& _map)
{
  for (size_t i = 0; i < _map.sectors.size(); ++i)
  {
    for (size_t j = 0; j < i; ++j)
    {
      if (_map.sectors[j].id == _map.sectors[i].id)
        Neuron::JsonFail(Neuron::JsonElementPath("sectors", i) + ".id", std::format("sector {} is listed twice", _map.sectors[i].id));
    }
  }
  const double half = _map.sizeMeters / 2.0;
  for (size_t i = 0; i < _map.sectors.size(); ++i)
  {
    const Outpost::SectorPlacement& sector = _map.sectors[i];
    const std::string path = Neuron::JsonElementPath("sectors", i);
    if (sector.minXMeters >= sector.maxXMeters || sector.minZMeters >= sector.maxZMeters || sector.minXMeters < -half ||
        sector.maxXMeters > half || sector.minZMeters < -half || sector.maxZMeters > half)
      Neuron::JsonFail(path, "is not a rectangle on the map");
    if (!sector.Contains(sector.node))
      Neuron::JsonFail(path + ".node", "is outside its sector");
    const auto blocks = [&sector, &_map](Outpost::PlanePosition _position, float _radiusMeters)
    { return Distance(sector.node, _position) - _radiusMeters < _map.minimumGapMeters; };
    for (const Outpost::OreAsteroidPlacement& asteroid : _map.oreAsteroids)
    {
      if (blocks(asteroid.position, asteroid.radiusMeters))
        Neuron::JsonFail(path + ".node", std::format("is closer than {} m to an ore asteroid", _map.minimumGapMeters));
    }
    for (const Outpost::AsteroidFieldPlacement& field : _map.asteroidFields)
    {
      if (blocks(field.position, field.radiusMeters))
        Neuron::JsonFail(path + ".node", std::format("is closer than {} m to an asteroid field", _map.minimumGapMeters));
    }
    for (const std::int32_t other : sector.adjacent)
    {
      const auto found = std::ranges::find(_map.sectors, other, &Outpost::SectorPlacement::id);
      if (other == sector.id || found == _map.sectors.end())
        Neuron::JsonFail(path + ".adjacent", std::format("names {}, which is not another sector", other));
      if (std::ranges::count(sector.adjacent, other) != 1 || std::ranges::find(found->adjacent, sector.id) == found->adjacent.end())
        Neuron::JsonFail(path + ".adjacent", std::format("names {} once, and {} must name it back", other, other));
    }
  }
}

// The sector across the map's center from _sector, which holds its node turned half a turn; none on a map that is not
// point-symmetric there.
const Outpost::SectorPlacement* MirrorOf(const Outpost::Map& _map, const Outpost::SectorPlacement& _sector) noexcept
{
  const Outpost::PlanePosition across{.xMeters = -_sector.node.xMeters, .zMeters = -_sector.node.zMeters};
  const auto found =
    std::ranges::find_if(_map.sectors, [across](const Outpost::SectorPlacement& _other) { return _other.Contains(across); });
  return found != _map.sectors.end() ? &*found : nullptr;
}

void CheckSectorKinds(const Outpost::Map& _map)
{
  for (size_t i = 0; i < _map.sectorKinds.size(); ++i)
  {
    for (size_t j = 0; j < i; ++j)
    {
      if (_map.sectorKinds[j].name == _map.sectorKinds[i].name)
        Neuron::JsonFail(Neuron::JsonElementPath("sectorKinds", i) + ".name",
                         std::format("\"{}\" is listed twice", _map.sectorKinds[i].name));
    }
  }
  for (size_t i = 0; i < _map.sectors.size(); ++i)
  {
    const Outpost::SectorPlacement& sector = _map.sectors[i];
    if (sector.kind.empty())
      continue;
    const std::string path = Neuron::JsonElementPath("sectors", i) + ".kind";
    const auto kind = std::ranges::find(_map.sectorKinds, sector.kind, &Outpost::SectorKind::name);
    if (kind == _map.sectorKinds.end())
      Neuron::JsonFail(path, std::format("\"{}\" is not one of the sector kinds", sector.kind));
    const Outpost::SectorPlacement* mirror = MirrorOf(_map, sector);
    if (mirror == nullptr || mirror->kind != sector.kind)
      Neuron::JsonFail(path, "is not the kind of the sector across the map's center");
    if (mirror->id == sector.id && std::ranges::any_of(kind->ore, [](const Outpost::OreRule& _rule) { return _rule.count % 2 != 0; }))
      Neuron::JsonFail(path, "places an odd count of asteroids in a sector that is its own mirror");
  }
}

// The sectors of _kind that draw for themselves and their mirror: the one of each pair with the lower identifier, in the
// map's order.
std::vector<const Outpost::SectorPlacement*> DrawingSectors(const Outpost::Map& _map, std::string_view _kind)
{
  std::vector<const Outpost::SectorPlacement*> sectors;
  for (const Outpost::SectorPlacement& sector : _map.sectors)
  {
    if (sector.kind == _kind && MirrorOf(_map, sector)->id >= sector.id)
      sectors.push_back(&sector);
  }
  return sectors;
}

void CheckOutposts(const Outpost::Map& _map)
{
  for (size_t i = 0; i < _map.outpostRules.size(); ++i)
  {
    const Outpost::OutpostRule& rule = _map.outpostRules[i];
    const std::string path = Neuron::JsonElementPath("outposts", i);
    if (std::ranges::find(_map.sectorKinds, rule.kind, &Outpost::SectorKind::name) == _map.sectorKinds.end())
      Neuron::JsonFail(path + ".kind", std::format("\"{}\" is not one of the sector kinds", rule.kind));
    std::int32_t wanted = 0;
    for (const Outpost::OutpostRule& other : _map.outpostRules)
      wanted += other.kind == rule.kind ? other.count : 0;
    const size_t pairs = DrawingSectors(_map, rule.kind).size();
    if (std::cmp_greater(wanted, pairs))
      Neuron::JsonFail(path + ".count",
                       std::format("asks for {} outposts in \"{}\" sectors, which have room for {}", wanted, rule.kind, pairs));
  }
}

void CheckDerelicts(const Outpost::Map& _map)
{
  for (size_t i = 0; i < _map.derelictRules.size(); ++i)
  {
    const Outpost::DerelictRule& rule = _map.derelictRules[i];
    const std::string path = Neuron::JsonElementPath("derelicts", i);
    if (std::ranges::find(_map.sectorKinds, rule.kind, &Outpost::SectorKind::name) == _map.sectorKinds.end())
      Neuron::JsonFail(path + ".kind", std::format("\"{}\" is not one of the sector kinds", rule.kind));
    std::int32_t wanted = 0;
    for (const Outpost::DerelictRule& other : _map.derelictRules)
      wanted += other.kind == rule.kind ? other.count : 0;
    const size_t pairs = DrawingSectors(_map, rule.kind).size();
    if (std::cmp_greater(wanted, pairs))
      Neuron::JsonFail(path + ".count",
                       std::format("asks for {} derelicts in \"{}\" sectors, which have room for {}", wanted, rule.kind, pairs));
  }
  if (_map.derelictResearchPercent > 100)
    Neuron::JsonFail("derelictResearchPercent", std::format("is at most 100, found {}", _map.derelictResearchPercent));
}

Outpost::Map ReadMap(std::string_view _json)
{
  const Neuron::JsonValue document = Neuron::ParseJson(_json);
  JsonObjectReader root(document, "");

  Outpost::Map map;
  map.sizeMeters = static_cast<float>(root.Number("sizeMeters", JsonBound::Positive));
  map.minimumGapMeters = static_cast<float>(root.Number("minimumGapMeters", JsonBound::Positive));
  map.starts = Neuron::ReadJsonList<Outpost::PlanePosition>(root, "starts", ReadPosition);
  map.oreAsteroids = Neuron::ReadJsonList<Outpost::OreAsteroidPlacement>(root, "oreAsteroids", ReadOreAsteroid);
  map.asteroidFields = Neuron::ReadJsonList<Outpost::AsteroidFieldPlacement>(root, "asteroidFields", ReadAsteroidField);
  if (root.Optional("sectors") != nullptr)
    map.sectors = Neuron::ReadJsonList<Outpost::SectorPlacement>(root, "sectors", ReadSector);
  if (root.Optional("sectorKinds") != nullptr)
  {
    map.sectorKinds = Neuron::ReadJsonList<Outpost::SectorKind>(root, "sectorKinds", ReadSectorKind);
    JsonObjectReader placement(root.Required("placement"), "placement");
    map.placement = ReadPlacementRules(placement);
    placement.Finish();
    if (root.Optional("outposts") != nullptr)
      map.outpostRules = Neuron::ReadJsonList<Outpost::OutpostRule>(root, "outposts", ReadOutpostRule);
    if (root.Optional("derelicts") != nullptr)
    {
      map.derelictRules = Neuron::ReadJsonList<Outpost::DerelictRule>(root, "derelicts", ReadDerelictRule);
      map.derelictResearchPercent = root.Integer("derelictResearchPercent", 0);
    }
  }
  root.Finish();

  if (map.starts.size() != PLAYER_COUNT)
    Neuron::JsonFail("starts", std::format("has {} starts; the MVP has {} players", map.starts.size(), PLAYER_COUNT));
  CheckGaps(map);
  CheckSectors(map);
  CheckSectorKinds(map);
  CheckOutposts(map);
  CheckDerelicts(map);
  return map;
}
} // namespace

Outpost::Map Outpost::LoadMap(std::string_view _json)
{
  try
  {
    return ReadMap(_json);
  }
  catch (const Neuron::Exception& error)
  {
    throw Neuron::Exception(std::format("Map: {}", error.what()));
  }
}
Outpost::Map Outpost::PlaceContent(Map _map, std::uint64_t _seed, std::span<const ResearchTopicId> _topics)
{
  if (_map.sectorKinds.empty())
    return _map;
  Neuron::Random random(_seed ^ PLACEMENT_SEED_SALT);
  const float gap = _map.minimumGapMeters;
  const PlacementRules& rules = _map.placement;
  // Whether an ore asteroid of _radiusMeters at _position keeps its gap from the edge, every obstacle and every start, and
  // its spacing from every ore asteroid.
  const auto isClear = [&_map, gap, &rules](PlanePosition _position, float _radiusMeters)
  {
    if (EdgeClearance(_map, _position) - _radiusMeters < gap)
      return false;
    for (const OreAsteroidPlacement& asteroid : _map.oreAsteroids)
    {
      const float meters = Distance(_position, asteroid.position);
      if (meters - _radiusMeters - asteroid.radiusMeters < gap || meters < rules.oreSpacingMeters)
        return false;
    }
    for (const AsteroidFieldPlacement& field : _map.asteroidFields)
    {
      if (Distance(_position, field.position) - _radiusMeters - field.radiusMeters < gap)
        return false;
    }
    return std::ranges::none_of(_map.starts, [&](PlanePosition _start) { return Distance(_position, _start) - _radiusMeters < gap; });
  };

  for (const SectorPlacement& sector : _map.sectors)
  {
    if (sector.kind.empty())
      continue;
    // A pair is drawn from its sector with the lower identifier (CheckSectorKinds found every mirror).
    const SectorPlacement& mirror = *MirrorOf(_map, sector);
    if (mirror.id < sector.id)
      continue;
    const bool ownMirror = mirror.id == sector.id;
    const SectorKind& kind = *std::ranges::find(_map.sectorKinds, sector.kind, &SectorKind::name);
    for (const OreRule& rule : kind.ore)
    {
      const float inset = rules.borderMeters + rule.radiusMeters;
      const float widthMeters = sector.maxXMeters - sector.minXMeters - (2.0f * inset);
      const float depthMeters = sector.maxZMeters - sector.minZMeters - (2.0f * inset);
      const std::int32_t draws = ownMirror ? rule.count / 2 : rule.count;
      for (std::int32_t draw = 0; draw < draws; ++draw)
      {
        bool placed = false;
        for (int attempt = 0; attempt < PLACEMENT_ATTEMPTS && !placed && widthMeters > 0.0f && depthMeters > 0.0f; ++attempt)
        {
          const PlanePosition position{.xMeters = sector.minXMeters + inset + (static_cast<float>(random.NextUnit()) * widthMeters),
                                       .zMeters = sector.minZMeters + inset + (static_cast<float>(random.NextUnit()) * depthMeters)};
          const PlanePosition across{.xMeters = -position.xMeters, .zMeters = -position.zMeters};
          if (Distance(position, sector.node) < rules.nodeClearanceMeters || !isClear(position, rule.radiusMeters))
            continue;
          const float apartMeters = Distance(position, across);
          if (apartMeters - (2.0f * rule.radiusMeters) < gap || apartMeters < rules.oreSpacingMeters || !isClear(across, rule.radiusMeters))
            continue;
          for (const PlanePosition at : {position, across})
            _map.oreAsteroids.push_back(
              {.position = at, .radiusMeters = rule.radiusMeters, .yield = rule.yield, .reserveOre = rule.reserveOre});
          placed = true;
        }
        if (!placed)
          throw Neuron::Exception(std::format("Map: sector {} has no room for all of its kind's asteroids", sector.name));
      }
    }
  }

  // Then the outposts, after every asteroid, so that a seed places the same asteroids whatever outposts the map has. Each
  // rule draws its pairs from those of its kind that no earlier rule took (CheckOutposts found enough).
  std::vector<std::int32_t> taken;
  for (const OutpostRule& rule : _map.outpostRules)
  {
    std::vector<const SectorPlacement*> free = DrawingSectors(_map, rule.kind);
    std::erase_if(free, [&taken](const SectorPlacement* _sector) { return std::ranges::find(taken, _sector->id) != taken.end(); });
    if (std::cmp_less(free.size(), rule.count))
      throw Neuron::Exception(std::format("Map: too few \"{}\" sectors are left for the outposts it places", rule.kind));
    for (std::int32_t draw = 0; draw < rule.count; ++draw)
    {
      const auto index = static_cast<std::ptrdiff_t>(random.NextBelow(static_cast<std::uint32_t>(free.size())));
      const SectorPlacement& sector = *free[static_cast<size_t>(index)];
      free.erase(free.begin() + index);
      taken.push_back(sector.id);
      _map.outposts.push_back({.sector = sector.id, .outpost = rule.outpost});
      if (const SectorPlacement& mirror = *MirrorOf(_map, sector); mirror.id != sector.id)
        _map.outposts.push_back({.sector = mirror.id, .outpost = rule.outpost});
    }
  }

  // Last the derelicts, so that a seed places the same asteroids and outposts whatever derelicts the map has (ADR-074).
  // Each rule draws its pairs as the outposts' do, and each derelict its place as an asteroid does: inside its sector by
  // the border and its radius, clear of the node, and the minimum gap from every obstacle, start and other derelict, it
  // and its mirror. Then the map's share of the pairs, drawn, each name a topic, drawn, both of the pair the same one.
  std::vector<std::int32_t> salvaged;
  const auto isClearOfAll = [&](PlanePosition _position, float _radiusMeters)
  {
    if (EdgeClearance(_map, _position) - _radiusMeters < gap)
      return false;
    const auto apart = [&](PlanePosition _other, float _otherRadius)
    { return Distance(_position, _other) - _radiusMeters - _otherRadius >= gap; };
    return std::ranges::all_of(_map.oreAsteroids,
                               [&](const OreAsteroidPlacement& _asteroid) { return apart(_asteroid.position, _asteroid.radiusMeters); }) &&
           std::ranges::all_of(_map.asteroidFields,
                               [&](const AsteroidFieldPlacement& _field) { return apart(_field.position, _field.radiusMeters); }) &&
           std::ranges::all_of(_map.derelicts,
                               [&](const DerelictPlacement& _derelict) { return apart(_derelict.position, _derelict.radiusMeters); }) &&
           std::ranges::all_of(_map.starts, [&](PlanePosition _start) { return apart(_start, 0.0f); });
  };
  std::vector<size_t> pairs;
  for (const DerelictRule& rule : _map.derelictRules)
  {
    std::vector<const SectorPlacement*> free = DrawingSectors(_map, rule.kind);
    std::erase_if(free, [&salvaged](const SectorPlacement* _sector) { return std::ranges::find(salvaged, _sector->id) != salvaged.end(); });
    if (std::cmp_less(free.size(), rule.count))
      throw Neuron::Exception(std::format("Map: too few \"{}\" sectors are left for the derelicts it places", rule.kind));
    for (std::int32_t draw = 0; draw < rule.count; ++draw)
    {
      const auto index = static_cast<std::ptrdiff_t>(random.NextBelow(static_cast<std::uint32_t>(free.size())));
      const SectorPlacement& sector = *free[static_cast<size_t>(index)];
      free.erase(free.begin() + index);
      salvaged.push_back(sector.id);
      const float inset = rules.borderMeters + rule.radiusMeters;
      const float widthMeters = sector.maxXMeters - sector.minXMeters - (2.0f * inset);
      const float depthMeters = sector.maxZMeters - sector.minZMeters - (2.0f * inset);
      bool placed = false;
      for (int attempt = 0; attempt < PLACEMENT_ATTEMPTS && !placed && widthMeters > 0.0f && depthMeters > 0.0f; ++attempt)
      {
        const PlanePosition position{.xMeters = sector.minXMeters + inset + (static_cast<float>(random.NextUnit()) * widthMeters),
                                     .zMeters = sector.minZMeters + inset + (static_cast<float>(random.NextUnit()) * depthMeters)};
        const PlanePosition across{.xMeters = -position.xMeters, .zMeters = -position.zMeters};
        if (Distance(position, sector.node) < rules.nodeClearanceMeters || !isClearOfAll(position, rule.radiusMeters) ||
            Distance(position, across) - (2.0f * rule.radiusMeters) < gap || !isClearOfAll(across, rule.radiusMeters))
          continue;
        pairs.push_back(_map.derelicts.size());
        for (const PlanePosition at : {position, across})
          _map.derelicts.push_back({.position = at, .ore = rule.ore, .hull = rule.hull, .radiusMeters = rule.radiusMeters});
        placed = true;
      }
      if (!placed)
        throw Neuron::Exception(std::format("Map: sector {} has no room for its derelict", sector.name));
    }
  }
  if (!_topics.empty())
  {
    const auto researching = static_cast<size_t>(std::lround(static_cast<double>(pairs.size()) * _map.derelictResearchPercent / 100.0));
    for (size_t draw = 0; draw < researching; ++draw)
    {
      const auto index = static_cast<std::ptrdiff_t>(random.NextBelow(static_cast<std::uint32_t>(pairs.size())));
      const size_t first = pairs[static_cast<size_t>(index)];
      pairs.erase(pairs.begin() + index);
      const ResearchTopicId topic = _topics[random.NextBelow(static_cast<std::uint32_t>(_topics.size()))];
      _map.derelicts[first].topic = topic;
      _map.derelicts[first + 1].topic = topic;
    }
  }
  return _map;
}
