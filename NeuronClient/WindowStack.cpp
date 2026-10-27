#include "pch.h"
#include "WindowStack.h"

#include <algorithm>

Neuron::CanvasPoint Neuron::KeepOnScreen(CanvasPoint _corner, float _widthUnits, float _screenWidthUnits, float _screenHeightUnits,
                                         float _keptUnits, float _titleBarUnits) noexcept
{
  const float kept = std::min(_keptUnits, _widthUnits);
  return {.xUnits = std::clamp(_corner.xUnits, kept - _widthUnits, std::max(kept - _widthUnits, _screenWidthUnits - kept)),
          .yUnits = std::clamp(_corner.yUnits, 0.0f, std::max(0.0f, _screenHeightUnits - _titleBarUnits))};
}
