#include "pch.h"

#include "HullBar.h"

#include <cmath>

namespace Outpost
{

namespace
{
constexpr float AUTHORED_WIDTH = static_cast<float>(Neuron::INTERFACE_AUTHORED_WIDTH);
constexpr float AUTHORED_HEIGHT = static_cast<float>(Neuron::INTERFACE_AUTHORED_HEIGHT);

/// Normalized projection coordinates, y up, as authored pixels, y down: `AuthoredToNormalized`, inverted.
void ToAuthored(float _screenX, float _screenY, float& _outX, float& _outY) noexcept
{
  _outX = (_screenX + 1.0f) * 0.5f * AUTHORED_WIDTH;
  _outY = (1.0f - _screenY) * 0.5f * AUTHORED_HEIGHT;
}
} // namespace

std::vector<HullBarPlacement> PlaceHullBars(std::span<const EntityRecord> _drawn, const CameraPose& _camera, float _aspectRatio)
{
  std::vector<HullBarPlacement> bars;
  for (const EntityRecord& record : _drawn)
  {
    if ((record.hullPercentRemaining >= 100) || (record.designIdentity >= Designs().size()))
    {
      continue;
    }
    const float worldX = static_cast<float>(DequantizePosition(record.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
    const float worldY = static_cast<float>(DequantizePosition(record.positionY)) / static_cast<float>(Neuron::FIXED_ONE);
    const float halfSize = static_cast<float>(Hull(Design(static_cast<DesignId>(record.designIdentity)).hull).sizeUnits) * 0.5f;

    float centerX = 0.0f;
    float centerY = 0.0f;
    float topX = 0.0f;
    float topY = 0.0f;
    if (!WorldToScreen(_camera, _aspectRatio, worldX, worldY, 0.0f, centerX, centerY) ||
        !WorldToScreen(_camera, _aspectRatio, worldX, worldY, halfSize, topX, topY))
    {
      continue;
    }
    float pixelX = 0.0f;
    float pixelY = 0.0f;
    float pixelTopX = 0.0f;
    float pixelTopY = 0.0f;
    ToAuthored(centerX, centerY, pixelX, pixelY);
    ToAuthored(topX, topY, pixelTopX, pixelTopY);
    if ((pixelX < 0.0f) || (pixelX > AUTHORED_WIDTH) || (pixelY < 0.0f) || (pixelY > AUTHORED_HEIGHT))
    {
      continue;
    }

    const float halfHeight = std::abs(pixelY - pixelTopY);
    bars.push_back(HullBarPlacement{.x = static_cast<std::int32_t>(std::lround(pixelX)) - (HULL_BAR_WIDTH_PIXELS / 2),
                                    .y = static_cast<std::int32_t>(std::lround(pixelY - halfHeight)) - HULL_BAR_LIFT_PIXELS,
                                    .percentRemaining = record.hullPercentRemaining});
  }
  return bars;
}

} // namespace Outpost
