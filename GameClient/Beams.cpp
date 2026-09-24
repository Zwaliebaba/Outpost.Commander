#include "pch.h"

#include "Beams.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace Outpost
{

namespace
{
/// **ONE LOOK A KIND OF BEAM** (Q81): a color, a width on the glass in scene-target pixels, and a floor in world
/// units so a beam seen close is not a hair against the hulls it joins. Not tuned; the owner's eye is the gate.
struct BeamLook
{
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  float widthPixels = 2.0f;
  float minimumWidthUnits = 4.0f;
};

/// A mass driver's slug: hot orange and the heaviest line.
inline constexpr BeamLook MASS_DRIVER_LOOK{.red = 1.0f, .green = 0.55f, .blue = 0.15f, .widthPixels = 3.0f, .minimumWidthUnits = 8.0f};

/// Point defense: pale blue, thin, and fast to read as many small shots.
inline constexpr BeamLook POINT_DEFENSE_LOOK{.red = 0.55f, .green = 0.85f, .blue = 1.0f, .widthPixels = 2.0f, .minimumWidthUnits = 4.0f};

/// Anything else that fires: white, so a weapon without a look is visible rather than missing.
inline constexpr BeamLook OTHER_WEAPON_LOOK{.red = 1.0f, .green = 1.0f, .blue = 1.0f, .widthPixels = 2.0f, .minimumWidthUnits = 4.0f};

/// The mining laser: green, steady, a little dimmer than a shot, since it is on for as long as the hold fills.
inline constexpr BeamLook MINING_LOOK{.red = 0.25f, .green = 0.9f, .blue = 0.45f, .widthPixels = 3.0f, .minimumWidthUnits = 6.0f};

/// Unloading: the ore's amber, flowing into the station.
inline constexpr BeamLook UNLOADING_LOOK{.red = 0.95f, .green = 0.7f, .blue = 0.2f, .widthPixels = 4.0f, .minimumWidthUnits = 10.0f};

/// The slack on a mining ship's reach when finding its rock: a record's position is on the wire's quarter-unit
/// grid and interpolated, so a miner exactly at its reach can read a little past it.
inline constexpr float MINING_REACH_SLACK_UNITS = 50.0f;

[[nodiscard]] const BeamLook& LookForWeapon(std::uint8_t _weapon) noexcept
{
  switch (static_cast<ComponentId>(_weapon))
  {
  case ComponentId::MassDriver:
    return MASS_DRIVER_LOOK;
  case ComponentId::PointDefense:
    return POINT_DEFENSE_LOOK;
  default:
    return OTHER_WEAPON_LOOK;
  }
}

/// What a record's design derives to, or nothing for a byte the catalog does not hold -- a newer host's
/// design must not index past the table, as `HitTest.cpp` guards for the same reason.
[[nodiscard]] DerivedStats StatsOf(const EntityRecord& _record) noexcept
{
  return (_record.designIdentity < Designs().size()) ? Derive(static_cast<DesignId>(_record.designIdentity)) : DerivedStats{};
}

[[nodiscard]] float WorldX(const EntityRecord& _record) noexcept
{
  return static_cast<float>(DequantizePosition(_record.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
}

[[nodiscard]] float WorldY(const EntityRecord& _record) noexcept
{
  return static_cast<float>(DequantizePosition(_record.positionY)) / static_cast<float>(Neuron::FIXED_ONE);
}

[[nodiscard]] const EntityRecord* FindDrawn(std::span<const EntityRecord> _drawn, WireIdentity _identity) noexcept
{
  for (const EntityRecord& record : _drawn)
  {
    if (record.identity == _identity)
    {
      return &record;
    }
  }
  return nullptr;
}

[[nodiscard]] Neuron::BeamInstance MakeBeam(float _fromX, float _fromY, float _fromZ, float _toX, float _toY, float _toZ,
                                            const BeamLook& _look, float _brightness, float _unitsPerPixel) noexcept
{
  Neuron::BeamInstance beam;
  beam.from[0] = _fromX;
  beam.from[1] = _fromY;
  beam.from[2] = _fromZ;
  beam.to[0] = _toX;
  beam.to[1] = _toY;
  beam.to[2] = _toZ;
  beam.widthUnits = std::max(_look.minimumWidthUnits, _look.widthPixels * _unitsPerPixel);
  beam.color[0] = _look.red * _brightness;
  beam.color[1] = _look.green * _brightness;
  beam.color[2] = _look.blue * _brightness;
  return beam;
}
} // namespace

TracerSet::TracerSet()
{
  m_tracers.reserve(MAX_TRACERS);
}

void TracerSet::Note(std::span<const FireEvent> _events, std::uint64_t _nowMilliseconds) noexcept
{
  for (const FireEvent& event : _events)
  {
    const bool repeat = std::any_of(
      m_tracers.begin(), m_tracers.end(), [&](const Tracer& _held)
      { return (_held.event == event) && ((_nowMilliseconds - _held.arrivedMilliseconds) < TRACER_REPEAT_WINDOW_MILLISECONDS); });
    if (!repeat && (m_tracers.size() < MAX_TRACERS))
    {
      m_tracers.push_back(Tracer{.event = event, .arrivedMilliseconds = _nowMilliseconds});
    }
  }
}

void TracerSet::Expire(std::uint64_t _nowMilliseconds) noexcept
{
  std::erase_if(
    m_tracers, [&](const Tracer& _held)
    { return _nowMilliseconds >= (_held.arrivedMilliseconds + INTERPOLATION_DELAY_MILLISECONDS + TRACER_LIFETIME_MILLISECONDS); });
}

void TracerSet::Clear() noexcept
{
  m_tracers.clear();
}

float UnitsPerPixelAtFocus(const CameraPose& _camera, std::uint32_t _heightPixels) noexcept
{
  if (_heightPixels == 0)
  {
    return 0.0f;
  }
  const float halfAngle = (VERTICAL_FIELD_OF_VIEW_DEGREES * std::numbers::pi_v<float> / 180.0f) * 0.5f;
  return (2.0f * _camera.distance * std::tan(halfAngle)) / static_cast<float>(_heightPixels);
}

void BuildBeams(const TracerSet& _tracers, std::span<const EntityRecord> _drawn, std::span<const RockPickPoint> _rocks,
                std::uint64_t _nowMilliseconds, float _unitsPerPixel, std::vector<Neuron::BeamInstance>& _outBeams)
{
  _outBeams.clear();

  // === SHOTS. ======================================================================================
  for (const Tracer& tracer : _tracers.Tracers())
  {
    const std::uint64_t shownFrom = tracer.arrivedMilliseconds + INTERPOLATION_DELAY_MILLISECONDS;
    if ((_nowMilliseconds < shownFrom) || (_nowMilliseconds >= (shownFrom + TRACER_LIFETIME_MILLISECONDS)))
    {
      continue;
    }
    const EntityRecord* shooter = FindDrawn(_drawn, tracer.event.shooter);
    const EntityRecord* target = FindDrawn(_drawn, tracer.event.target);
    if ((shooter == nullptr) || (target == nullptr))
    {
      continue;
    }

    // Full at the start and gone at the end, linearly.
    const float age = static_cast<float>(_nowMilliseconds - shownFrom) / static_cast<float>(TRACER_LIFETIME_MILLISECONDS);
    _outBeams.push_back(MakeBeam(WorldX(*shooter), WorldY(*shooter), 0.0f, WorldX(*target), WorldY(*target), 0.0f,
                                 LookForWeapon(tracer.event.weapon), 1.0f - age, _unitsPerPixel));
  }

  // === MINING AND UNLOADING. =======================================================================
  for (const EntityRecord& worker : _drawn)
  {
    const Activity activity = ActivityOf(worker.flags);
    if (activity == Activity::None)
    {
      continue;
    }
    const float workerX = WorldX(worker);
    const float workerY = WorldY(worker);

    if (activity == Activity::Extracting)
    {
      const float reachUnits = static_cast<float>(StatsOf(worker).miningRangeUnits) + MINING_REACH_SLACK_UNITS;
      const RockPickPoint* nearest = nullptr;
      float nearestSquared = reachUnits * reachUnits;
      for (const RockPickPoint& rock : _rocks)
      {
        const float dx = rock.worldX - workerX;
        const float dy = rock.worldY - workerY;
        const float squared = (dx * dx) + (dy * dy);
        if (squared <= nearestSquared)
        {
          nearestSquared = squared;
          nearest = &rock;
        }
      }
      if (nearest != nullptr)
      {
        _outBeams.push_back(
          MakeBeam(workerX, workerY, 0.0f, nearest->worldX, nearest->worldY, nearest->liftUnits, MINING_LOOK, 1.0f, _unitsPerPixel));
      }
      continue;
    }

    // UNLOADING: into the owner's nearest acceptor, which is where the host sends a miner to unload.
    const EntityRecord* nearest = nullptr;
    float nearestSquared = std::numeric_limits<float>::max();
    for (const EntityRecord& candidate : _drawn)
    {
      if ((candidate.owner != worker.owner) || !StatsOf(candidate).acceptsOre)
      {
        continue;
      }
      const float dx = WorldX(candidate) - workerX;
      const float dy = WorldY(candidate) - workerY;
      const float squared = (dx * dx) + (dy * dy);
      if (squared < nearestSquared)
      {
        nearestSquared = squared;
        nearest = &candidate;
      }
    }
    if (nearest != nullptr)
    {
      _outBeams.push_back(MakeBeam(workerX, workerY, 0.0f, WorldX(*nearest), WorldY(*nearest), 0.0f, UNLOADING_LOOK, 1.0f, _unitsPerPixel));
    }
  }
}

} // namespace Outpost
