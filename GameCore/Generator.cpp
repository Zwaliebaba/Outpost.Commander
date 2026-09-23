// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Generator.h"
#include "Command.h"

namespace Outpost
{

namespace
{
/// **A CAP ON EVERY REJECTION LOOP, SO NONE CAN RUN FOREVER.** The constraints above leave room for many
/// times the rocks asked for, so the cap is never met by a seed anyone runs -- and if a constant moves until
/// it is, the region comes back short and `GeneratorTests` fails on the count rather than the host hanging
/// at match start.
constexpr std::uint32_t MAXIMUM_ATTEMPTS = 4096;

/// The play area's half extent in whole units, from the one constant that states it.
constexpr std::int32_t PLAY_AREA_HALF_EXTENT_UNITS = PLAY_AREA_HALF_EXTENT / Neuron::FIXED_ONE;

/// Every point is kept in whole units until it is placed. **Whole units are what every figure above is
/// stated in**, and a finer grid would buy nothing a player can see while adding eight bits to every
/// squared distance below.
struct Point
{
  std::int32_t x = 0;
  std::int32_t y = 0;
};

[[nodiscard]] constexpr std::int64_t DistanceSquared(const Point& _a, const Point& _b) noexcept
{
  const std::int64_t dx = static_cast<std::int64_t>(_a.x) - _b.x;
  const std::int64_t dy = static_cast<std::int64_t>(_a.y) - _b.y;
  return (dx * dx) + (dy * dy);
}

[[nodiscard]] constexpr std::int64_t Squared(std::int32_t _value) noexcept
{
  return static_cast<std::int64_t>(_value) * _value;
}

/// The whole-unit anchor of player one, from the one function that places it.
[[nodiscard]] Point HomeAnchor(std::size_t _playerCount) noexcept
{
  const Neuron::Vec2 anchor = StartAnchor(_playerCount, 1);
  return Point{.x = anchor.x / Neuron::FIXED_ONE, .y = anchor.y / Neuron::FIXED_ONE};
}

[[nodiscard]] bool InsidePlayArea(const Point& _point, std::int32_t _marginUnits) noexcept
{
  const std::int32_t limit = PLAY_AREA_HALF_EXTENT_UNITS - _marginUnits;
  return (_point.x > -limit) && (_point.x < limit) && (_point.y > -limit) && (_point.y < limit);
}

/// A point uniform over the square of side `2 x _radiusUnits` around _center, which the callers then reject
/// down to a disc or an annulus. **X IS DRAWN BEFORE Y, AND THAT ORDER IS PART OF THE OUTPUT** -- swapping
/// the two draws moves every rock, so it is written once, here.
[[nodiscard]] Point DrawAround(Neuron::Pcg32& _random, const Point& _center, std::int32_t _radiusUnits) noexcept
{
  const std::int32_t x = _random.NextInRange(-_radiusUnits, _radiusUnits);
  const std::int32_t y = _random.NextInRange(-_radiusUnits, _radiusUnits);
  return Point{.x = _center.x + x, .y = _center.y + y};
}

/// Whether a rock may go at _candidate: inside the region by half the spacing, inside the play area, and
/// at least the spacing from every rock already placed. **The placed list is scanned in placement order**,
/// which does not reach the answer -- it is a yes or no -- but it is the order anyway.
[[nodiscard]] bool RockFits(const Point& _candidate, const std::vector<Point>& _placed, std::size_t _playerCount) noexcept
{
  if (!InRegion(_candidate.x, _candidate.y, _playerCount, ASTEROID_SPACING_UNITS / 2) ||
      !InsidePlayArea(_candidate, ASTEROID_SPACING_UNITS))
  {
    return false;
  }
  const std::int64_t spacingSquared = Squared(ASTEROID_SPACING_UNITS);
  for (const Point& other : _placed)
  {
    if (DistanceSquared(_candidate, other) < spacingSquared)
    {
      return false;
    }
  }
  return true;
}

/// _count rocks in the annulus `[_innerUnits, _outerUnits]` around _center, appended to both lists. Returns
/// how many it managed, which is _count for every constant this file ships with.
std::size_t PlaceField(Neuron::Pcg32& _random, const Point& _center, std::int32_t _innerUnits, std::int32_t _outerUnits, std::size_t _count,
                       FieldKind _field, std::size_t _playerCount, std::vector<Point>& _points, std::vector<Placement>& _out)
{
  const std::int64_t innerSquared = Squared(_innerUnits);
  const std::int64_t outerSquared = Squared(_outerUnits);

  std::size_t placedCount = 0;
  std::uint32_t attempts = 0;
  while ((placedCount < _count) && (attempts < MAXIMUM_ATTEMPTS))
  {
    ++attempts;
    const Point candidate = DrawAround(_random, _center, _outerUnits);
    const std::int64_t fromCenter = DistanceSquared(candidate, _center);
    if ((fromCenter < innerSquared) || (fromCenter > outerSquared) || !RockFits(candidate, _points, _playerCount))
    {
      continue;
    }

    _points.push_back(candidate);
    _out.push_back(
      Placement{.kind = PlacedKind::Asteroid,
                .field = _field,
                .owner = NO_PLAYER,
                .position = Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(candidate.x), .y = Neuron::FixedFromWholeUnits(candidate.y)},
                .heading = 0});
    ++placedCount;
  }
  return placedCount;
}

/// A contested cluster's center: toward the middle, deep enough inside the region that every rock of the
/// cluster is too, and clear of the clusters already chosen. False when the attempts run out.
[[nodiscard]] bool ChooseClusterCenter(Neuron::Pcg32& _random, std::size_t _playerCount, const std::vector<Point>& _chosen,
                                       Point& _outCenter) noexcept
{
  const std::int64_t nearestSquared = Squared(CONTESTED_FIELD_NEAREST_UNITS);
  const std::int64_t farthestSquared = Squared(CONTESTED_FIELD_FARTHEST_UNITS);

  // THE CLUSTER'S WHOLE DISC stays inside the region with half the spacing to spare, and two clusters do
  // not overlap -- so a rock's cluster is never ambiguous and M3 can give each its own ore.
  const std::int32_t margin = CONTESTED_FIELD_RADIUS_UNITS + (ASTEROID_SPACING_UNITS / 2);
  const std::int64_t apartSquared = Squared((2 * CONTESTED_FIELD_RADIUS_UNITS) + ASTEROID_SPACING_UNITS);

  for (std::uint32_t attempt = 0; attempt < MAXIMUM_ATTEMPTS; ++attempt)
  {
    const Point candidate = DrawAround(_random, Point{}, CONTESTED_FIELD_FARTHEST_UNITS);
    const std::int64_t fromMiddle = DistanceSquared(candidate, Point{});
    if ((fromMiddle < nearestSquared) || (fromMiddle > farthestSquared) || !InRegion(candidate.x, candidate.y, _playerCount, margin))
    {
      continue;
    }

    bool clear = true;
    for (const Point& other : _chosen)
    {
      if (DistanceSquared(candidate, other) < apartSquared)
      {
        clear = false;
        break;
      }
    }
    if (clear)
    {
      _outCenter = candidate;
      return true;
    }
  }
  return false;
}
} // namespace

bool InRegion(std::int32_t _xUnits, std::int32_t _yUnits, std::size_t _playerCount, std::int32_t _marginUnits) noexcept
{
  if (_playerCount <= 2)
  {
    // The half-plane x < 0, whose one copied edge is the y axis.
    return _xUnits <= -_marginUnits;
  }

  // THE QUARTER AROUND THE NEGATIVE X AXIS: x < 0 and |y| < -x. Its two edges are the diagonals, and a
  // point's distance to the nearer one is `(-x - |y|) / sqrt 2` -- compared squared, so the root never
  // appears: `(-x - |y|)^2 >= 2 x margin^2`, with the difference required positive first.
  const std::int64_t absoluteY = (_yUnits < 0) ? -static_cast<std::int64_t>(_yUnits) : static_cast<std::int64_t>(_yUnits);
  const std::int64_t inset = -static_cast<std::int64_t>(_xUnits) - absoluteY;
  if (inset <= 0)
  {
    return false;
  }
  return (inset * inset) >= (2 * Squared(_marginUnits));
}

std::vector<Placement> GenerateRegion(std::uint64_t _seed, std::size_t _playerCount)
{
  std::vector<Placement> placed;
  if (_playerCount == 0)
  {
    return placed;
  }

  // ONE ENGINE, ONE STREAM, AND EVERY DRAW IN A FIXED ORDER: the home field, then the cluster centers
  // interleaved with their rocks. Reordering those is a different map for every seed, which the pinned
  // table in `GeneratorTests` exists to notice.
  Neuron::Pcg32 random{_seed, GENERATOR_STREAM};

  const std::size_t players = (_playerCount > ANCHOR_COUNT) ? ANCHOR_COUNT : _playerCount;
  placed.reserve(HOME_FIELD_ASTEROID_COUNT + (CONTESTED_FIELD_COUNT * CONTESTED_FIELD_ASTEROID_COUNT));
  std::vector<Point> points;
  points.reserve(placed.capacity());

  static_cast<void>(PlaceField(random, HomeAnchor(players), HOME_FIELD_INNER_RADIUS_UNITS, HOME_FIELD_OUTER_RADIUS_UNITS,
                               HOME_FIELD_ASTEROID_COUNT, FieldKind::Home, players, points, placed));

  std::vector<Point> centers;
  for (std::size_t cluster = 0; cluster < CONTESTED_FIELD_COUNT; ++cluster)
  {
    Point center{};
    if (!ChooseClusterCenter(random, players, centers, center))
    {
      break;
    }
    centers.push_back(center);
    static_cast<void>(PlaceField(random, center, 0, CONTESTED_FIELD_RADIUS_UNITS, CONTESTED_FIELD_ASTEROID_COUNT, FieldKind::Contested,
                                 players, points, placed));
  }

  return placed;
}

std::size_t FieldCopyCount(std::size_t _playerCount) noexcept
{
  if (_playerCount == 0)
  {
    return 0;
  }
  return (_playerCount <= 2) ? 2 : ANCHOR_COUNT;
}

std::vector<Placement> GenerateField(std::uint64_t _seed, std::size_t _playerCount)
{
  const std::vector<Placement> region = GenerateRegion(_seed, _playerCount);
  const std::size_t copies = FieldCopyCount(_playerCount);

  std::vector<Placement> field;
  field.reserve(region.size() * copies);
  field.insert(field.end(), region.begin(), region.end());

  // ANCHOR_COUNT / copies quarter turns per copy: two at two players, one at four. Each copy turns the one
  // before it rather than the region by a multiple, which is the same exact arithmetic StartAnchor does.
  const std::size_t turnsPerCopy = (copies == 0) ? 0 : (ANCHOR_COUNT / copies);
  for (std::size_t copy = 1; copy < copies; ++copy)
  {
    const std::size_t previous = (copy - 1) * region.size();
    for (std::size_t index = 0; index < region.size(); ++index)
    {
      Placement turned = field[previous + index];
      for (std::size_t turn = 0; turn < turnsPerCopy; ++turn)
      {
        turned.position = QuarterTurn(turned.position);
      }
      field.push_back(turned);
    }
  }
  return field;
}

} // namespace Outpost
