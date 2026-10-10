#include "pch.h"
#include "ScreenSegment.h"

#include <cmath>

std::optional<std::array<DirectX::XMFLOAT2, 4>> Neuron::ScreenSegment::Corners() const noexcept
{
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float length = std::hypot(dx, dy);
  if (length <= 0.0f)
    return std::nullopt;
  // Half the width along the segment and across it; across points to the right of the walk, on a screen whose y runs down.
  const float half = widthPixels / 2.0f;
  const float alongX = dx / length * half;
  const float alongY = dy / length * half;
  const float acrossX = -alongY;
  const float acrossY = alongX;
  return std::array<DirectX::XMFLOAT2, 4>{{{from.x - alongX - acrossX, from.y - alongY - acrossY},
                                           {to.x + alongX - acrossX, to.y + alongY - acrossY},
                                           {to.x + alongX + acrossX, to.y + alongY + acrossY},
                                           {from.x - alongX + acrossX, from.y - alongY + acrossY}}};
}
