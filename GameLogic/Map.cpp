#include "pch.h"
#include "Map.h"

#include <algorithm>
#include <cmath>

namespace
{
using Neuron::JsonBound;
using Neuron::JsonObjectReader;

// The MVP is one player against one AI (design §10).
constexpr size_t PLAYER_COUNT = 2;

Outpost::PlanePosition ReadPosition(JsonObjectReader& _reader)
{
  return {.xMeters = static_cast<float>(_reader.Number("xMeters", JsonBound::Any)),
          .zMeters = static_cast<float>(_reader.Number("zMeters", JsonBound::Any))};
}

Outpost::OreAsteroidPlacement ReadOreAsteroid(JsonObjectReader& _reader)
{
  Outpost::OreAsteroidPlacement asteroid;
  asteroid.position = ReadPosition(_reader);
  asteroid.radiusMeters = static_cast<float>(_reader.Number("radiusMeters", JsonBound::Positive));
  const std::string yield = _reader.String("yield");
  if (yield == "home")
    asteroid.yield = Outpost::OreYield::Home;
  else if (yield == "near")
    asteroid.yield = Outpost::OreYield::Near;
  else if (yield == "contested")
    asteroid.yield = Outpost::OreYield::Contested;
  else if (yield == "rich")
    asteroid.yield = Outpost::OreYield::Rich;
  else
    Neuron::JsonFail(_reader.PathOf("yield"), std::format("\"{}\" is not \"home\", \"near\", \"contested\" or \"rich\"", yield));
  asteroid.reserveOre = _reader.Integer("reserve", 1);
  return asteroid;
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
  root.Finish();

  if (map.starts.size() != PLAYER_COUNT)
    Neuron::JsonFail("starts", std::format("has {} starts; the MVP has {} players", map.starts.size(), PLAYER_COUNT));
  CheckGaps(map);
  CheckSectors(map);
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