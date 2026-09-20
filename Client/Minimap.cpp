#include "pch.h"

#include "Minimap.h"

#include <algorithm>
#include <cmath>

namespace Neuron
{

namespace
{

/// How bright explored-but-unseen ground is drawn against visible ground (§9.2: "the terrain colour
/// at half brightness where explored, at full where visible").
constexpr std::uint32_t EXPLORED_NUMERATOR = 1;
constexpr std::uint32_t EXPLORED_DENOMINATOR = 2;
/// And a ghost structure, for the same reason and by the same rule.
constexpr std::uint32_t GHOST_NUMERATOR = 1;
constexpr std::uint32_t GHOST_DENOMINATOR = 2;

[[nodiscard]] constexpr std::uint32_t Dimmed(std::uint32_t _color, std::uint32_t _numerator, std::uint32_t _denominator) noexcept
{
  const std::uint32_t red = ((_color & 0xFFu) * _numerator) / _denominator;
  const std::uint32_t green = (((_color >> 8) & 0xFFu) * _numerator) / _denominator;
  const std::uint32_t blue = (((_color >> 16) & 0xFFu) * _numerator) / _denominator;
  return red | (green << 8) | (blue << 16) | (_color & 0xFF000000u);
}

void Plot(std::vector<std::uint8_t>& _pixels, std::int32_t _x, std::int32_t _y, std::uint32_t _color) noexcept
{
  if (_x < 0 || _y < 0 || static_cast<std::uint32_t>(_x) >= MINIMAP_PIXELS || static_cast<std::uint32_t>(_y) >= MINIMAP_PIXELS)
  {
    return; // Clipped to the map, which §9.2 asks for by name of the frustum and is true of all of it.
  }
  const std::size_t at = ((static_cast<std::size_t>(_y) * MINIMAP_PIXELS) + static_cast<std::size_t>(_x)) * 4;
  _pixels[at] = static_cast<std::uint8_t>(_color & 0xFFu);
  _pixels[at + 1] = static_cast<std::uint8_t>((_color >> 8) & 0xFFu);
  _pixels[at + 2] = static_cast<std::uint8_t>((_color >> 16) & 0xFFu);
  _pixels[at + 3] = 0xFF;
}

void Block(std::vector<std::uint8_t>& _pixels, std::int32_t _x, std::int32_t _y, std::int32_t _size, std::uint32_t _color) noexcept
{
  for (std::int32_t row = 0; row < _size; ++row)
  {
    for (std::int32_t column = 0; column < _size; ++column)
    {
      Plot(_pixels, _x + column, _y + row, _color);
    }
  }
}

/// A line by Bresenham, for the frustum's four edges. Integer throughout, so the same four corners
/// give the same pixels on every machine.
void Line(std::vector<std::uint8_t>& _pixels, std::int32_t _fromX, std::int32_t _fromY, std::int32_t _toX, std::int32_t _toY,
          std::uint32_t _color) noexcept
{
  std::int32_t x = _fromX;
  std::int32_t y = _fromY;
  const std::int32_t spanX = std::abs(_toX - _fromX);
  const std::int32_t spanY = -std::abs(_toY - _fromY);
  const std::int32_t stepX = _fromX < _toX ? 1 : -1;
  const std::int32_t stepY = _fromY < _toY ? 1 : -1;
  std::int32_t error = spanX + spanY;
  for (;;)
  {
    Plot(_pixels, x, y, _color);
    if (x == _toX && y == _toY)
    {
      return;
    }
    const std::int32_t twice = 2 * error;
    if (twice >= spanY)
    {
      error += spanY;
      x += stepX;
    }
    if (twice <= spanX)
    {
      error += spanX;
      y += stepY;
    }
  }
}

/// The colour of a commander, or white when the palette does not carry that seat. White rather than
/// black: a seat with no colour is a content fault and a white dot on the map says so, where a
/// black one is indistinguishable from unexplored ground.
[[nodiscard]] std::uint32_t ColorOf(std::span<const std::uint32_t> _palette, std::uint8_t _index) noexcept
{
  return _index < _palette.size() ? _palette[_index] : 0xFFFFFFFFu;
}

} // namespace

void BuildMinimap(const MinimapView& _view, std::vector<std::uint8_t>& _outPixels)
{
  // OPAQUE BLACK AND NOT TRANSPARENT BLACK. Unexplored ground is the map's own picture of nothing,
  // and a transparent pixel would let whatever is behind the panel show through it - which on a
  // frame where the world is drawn behind the interface is the landscape itself, seen through the
  // hole the fog is meant to be.
  _outPixels.assign(MINIMAP_BYTES, 0);
  for (std::size_t at = 3; at < _outPixels.size(); at += 4)
  {
    _outPixels[at] = 0xFF;
  }
  if (_view.fog == nullptr || _view.fog->cellsPerSide == 0 || _view.fog->cells.empty())
  {
    return; // Before the first frame: a black map, which is what unexplored looks like anyway.
  }
  const std::uint32_t cells = _view.fog->cellsPerSide;
  // WHOLE PIXELS A CELL AND NEVER A FRACTION (§9.2). A Small landscape is two; a landscape bigger
  // than the map is one, and its far edge is simply not drawn, which is the honest thing to do
  // until the milestone that ships those landscapes says what it wants instead.
  const std::int32_t perCell = static_cast<std::int32_t>(std::max<std::uint32_t>(1, MINIMAP_PIXELS / std::max<std::uint32_t>(cells, 1)));

  // 1. The fog.
  const std::uint32_t explored = Dimmed(_view.terrainColor, EXPLORED_NUMERATOR, EXPLORED_DENOMINATOR);
  for (std::uint32_t cellY = 0; cellY < cells; ++cellY)
  {
    for (std::uint32_t cellX = 0; cellX < cells; ++cellX)
    {
      const std::size_t at = (static_cast<std::size_t>(cellY) * cells) + cellX;
      if (at >= _view.fog->cells.size())
      {
        continue;
      }
      const auto shade = static_cast<FogShade>(_view.fog->cells[at]);
      if (shade == FogShade::Unexplored)
      {
        continue; // Black, which the fill already is.
      }
      Block(_outPixels, static_cast<std::int32_t>(cellX) * perCell, static_cast<std::int32_t>(cellY) * perCell, perCell,
            shade == FogShade::Visible ? _view.terrainColor : explored);
    }
  }

  // 2 and 3. The structures, then the devices OVER them, which is the order §9.2 lists and matters
  // where a truck stands on its own factory: a device under a structure's block would be a unit the
  // commander cannot find on the map.
  const float pixelsPerWorldUnit = static_cast<float>(perCell) / std::max(_view.worldUnitsPerCell, 1.0f);
  const auto plot = [&](const MinimapBlip& _blip)
  {
    const std::int32_t x = static_cast<std::int32_t>(_blip.x * pixelsPerWorldUnit);
    const std::int32_t y = static_cast<std::int32_t>(_blip.z * pixelsPerWorldUnit);
    std::uint32_t color = ColorOf(_view.commanderColors, _blip.colorIndex);
    if (_blip.ghost)
    {
      color = Dimmed(color, GHOST_NUMERATOR, GHOST_DENOMINATOR);
    }
    // 4. The selection, in accent, OVER whatever colour the object had.
    const std::uint32_t drawn = _blip.selected ? _view.accentColor : color;
    if (_blip.cellsX == 0 || _blip.cellsY == 0)
    {
      Block(_outPixels, x, y, 1, drawn);
      return;
    }
    // A structure covers its footprint: two pixels a cell on a Small landscape, one on a bigger
    // one, and its position is the MIDDLE of the footprint as the render view carries it.
    const std::int32_t width = static_cast<std::int32_t>(_blip.cellsX) * perCell;
    const std::int32_t height = static_cast<std::int32_t>(_blip.cellsY) * perCell;
    for (std::int32_t row = 0; row < height; ++row)
    {
      for (std::int32_t column = 0; column < width; ++column)
      {
        Plot(_outPixels, x - (width / 2) + column, y - (height / 2) + row, drawn);
      }
    }
  };
  for (const MinimapBlip& blip : _view.blips)
  {
    if (blip.cellsX != 0 && blip.cellsY != 0)
    {
      plot(blip);
    }
  }
  for (const MinimapBlip& blip : _view.blips)
  {
    if (blip.cellsX == 0 || blip.cellsY == 0)
    {
      plot(blip);
    }
  }

  // 5. The camera's quadrilateral, last and over everything.
  const bool anyFrustum = std::any_of(_view.frustumGround.begin(), _view.frustumGround.end(), [](float _value) { return _value != 0.0f; });
  if (anyFrustum)
  {
    std::array<std::int32_t, 4> cornerX{};
    std::array<std::int32_t, 4> cornerY{};
    for (std::size_t corner = 0; corner < 4; ++corner)
    {
      cornerX[corner] = static_cast<std::int32_t>(_view.frustumGround[corner * 2] * pixelsPerWorldUnit);
      cornerY[corner] = static_cast<std::int32_t>(_view.frustumGround[(corner * 2) + 1] * pixelsPerWorldUnit);
    }
    for (std::size_t corner = 0; corner < 4; ++corner)
    {
      const std::size_t next = (corner + 1) % 4;
      Line(_outPixels, cornerX[corner], cornerY[corner], cornerX[next], cornerY[next], _view.borderColor);
    }
  }
}

MinimapPoint WorldOfMinimap(std::int32_t _pixelX, std::int32_t _pixelY, std::uint32_t _cellsPerSide, float _worldUnitsPerCell) noexcept
{
  MinimapPoint point{};
  if (_pixelX < 0 || _pixelY < 0 || static_cast<std::uint32_t>(_pixelX) >= MINIMAP_PIXELS ||
      static_cast<std::uint32_t>(_pixelY) >= MINIMAP_PIXELS || _cellsPerSide == 0)
  {
    return point;
  }
  const float perCell = static_cast<float>(std::max<std::uint32_t>(1, MINIMAP_PIXELS / std::max<std::uint32_t>(_cellsPerSide, 1)));
  // THE MIDDLE OF THE PIXEL AND NOT ITS CORNER, which is the same rule Replica/Picking.cpp casts
  // through: a click lands on what was drawn there, and what was drawn there was sampled at the
  // middle. Half a cell is 32 world units, which is half a device's length.
  point.x = (static_cast<float>(_pixelX) + 0.5f) / perCell * _worldUnitsPerCell;
  point.z = (static_cast<float>(_pixelY) + 0.5f) / perCell * _worldUnitsPerCell;
  point.inside =
    point.x <= static_cast<float>(_cellsPerSide) * _worldUnitsPerCell && point.z <= static_cast<float>(_cellsPerSide) * _worldUnitsPerCell;
  return point;
}

} // namespace Neuron
