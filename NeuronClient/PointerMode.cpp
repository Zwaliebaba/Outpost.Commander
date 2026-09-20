#include "pch.h"

#include "PointerMode.h"

namespace Neuron
{

Aim AimedBy(const Aim& _aim, std::int32_t _countsX, std::int32_t _countsY) noexcept
{
  return {_aim.yawRadians + static_cast<float>(_countsX) * AIM_RADIANS_PER_COUNT,
          _aim.pitchRadians - static_cast<float>(_countsY) * AIM_RADIANS_PER_COUNT};
}

} // namespace Neuron
