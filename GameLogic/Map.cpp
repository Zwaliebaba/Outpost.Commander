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
  else if (yield == "contested")
    asteroid.yield = Outpost::OreYield::Contested;
  else
    Neuron::JsonFail(_reader.PathOf("yield"), std::format("\"{}\" is not \"home\" or \"contested\"", yield));
  return asteroid;
}

Outpost::AsteroidFieldPlacement ReadAsteroidField(JsonObjectReader& _reader)
{
  Outpost::AsteroidFieldPlacement field;
  field.position = ReadPosition(_reader);
  field.radiusMeters = static_cast<float>(_reader.Number("radiusMeters", JsonBound::Positive));
  return field;
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
  root.Finish();

  if (map.starts.size() != PLAYER_COUNT)
    Neuron::JsonFail("starts", std::format("has {} starts; the MVP has {} players", map.starts.size(), PLAYER_COUNT));
  CheckGaps(map);
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
