#include "pch.h"

#include "CapturePath.h"

#include <cmath>

namespace Neuron
{

namespace
{

[[nodiscard]] float Between(float _from, float _to, float _fraction) noexcept
{
  return _from + (_to - _from) * _fraction;
}

} // namespace

CaptureVantage VantageAt(std::span<const CaptureVantage> _script, std::uint32_t _tick) noexcept
{
  if (_script.empty())
  {
    return {};
  }
  // The last vantage whose tick has arrived. A linear walk: a script is a handful of entries, and
  // a search would be more code than the thing it searches.
  std::size_t index = 0;
  while (index + 1 < _script.size() && _script[index + 1].tick <= _tick)
  {
    ++index;
  }
  CaptureVantage held = _script[index];
  held.tick = _tick;
  if (index + 1 >= _script.size() || _tick <= _script[index].tick)
  {
    // Past the last vantage, or not yet at this one: held as written. The second case is the tick
    // before the script's first, which is the opening pose rather than an extrapolation backwards.
    return held;
  }
  const CaptureVantage& next = _script[index + 1];
  const std::uint32_t span = next.tick - _script[index].tick;
  const float fraction = static_cast<float>(_tick - _script[index].tick) / static_cast<float>(span);
  held.aimX = Between(_script[index].aimX, next.aimX, fraction);
  held.aimZ = Between(_script[index].aimZ, next.aimZ, fraction);
  held.bearingRadians = Between(_script[index].bearingRadians, next.bearingRadians, fraction);
  held.setbackWorldUnits = Between(_script[index].setbackWorldUnits, next.setbackWorldUnits, fraction);
  held.elevationWorldUnits = Between(_script[index].elevationWorldUnits, next.elevationWorldUnits, fraction);
  // follow and fog are the vantage's own and not the pair's: a frame is fogged one way or the
  // other, and a vantage either looks at the fighting or does not.
  return held;
}

CapturePose PoseOf(const CaptureVantage& _vantage, float _groundHeightWorldUnits) noexcept
{
  CapturePose pose{};
  pose.atX = _vantage.aimX;
  pose.atY = _groundHeightWorldUnits;
  pose.atZ = _vantage.aimZ;
  pose.eyeX = _vantage.aimX + std::sin(_vantage.bearingRadians) * _vantage.setbackWorldUnits;
  pose.eyeY = _groundHeightWorldUnits + _vantage.elevationWorldUnits;
  pose.eyeZ = _vantage.aimZ - std::cos(_vantage.bearingRadians) * _vantage.setbackWorldUnits;
  return pose;
}

bool ActionCenter(std::span<const RenderInstance> _instances, float& _outX, float& _outZ) noexcept
{
  float sumX = 0.0f;
  float sumZ = 0.0f;
  std::uint32_t count = 0;
  for (const RenderInstance& instance : _instances)
  {
    if (instance.kind != RenderInstanceKind::Projectile)
    {
      continue;
    }
    sumX += instance.x;
    sumZ += instance.z;
    ++count;
  }
  if (count == 0)
  {
    return false;
  }
  _outX = sumX / static_cast<float>(count);
  _outZ = sumZ / static_cast<float>(count);
  return true;
}

} // namespace Neuron
