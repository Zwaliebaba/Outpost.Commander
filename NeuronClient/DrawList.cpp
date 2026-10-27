#include "pch.h"
#include "DrawList.h"

#include <algorithm>
#include <cmath>

namespace
{
// Where layer _layer's share of a list starts, its layer's first for a layer begun, or the list's end past the last one.
std::size_t LayerStart(const Neuron::DrawList& _list, std::size_t _layer, std::size_t Neuron::CanvasLayer::*_first,
                       std::size_t _total) noexcept
{
  if (_layer == 0)
    return 0;
  return _layer - 1 < _list.layers.size() ? _list.layers[_layer - 1].*_first : _total;
}

Neuron::CanvasSpan LayerSpan(const Neuron::DrawList& _list, std::size_t _layer, std::size_t Neuron::CanvasLayer::*_first,
                             std::size_t _total) noexcept
{
  return {.first = LayerStart(_list, _layer, _first, _total), .end = LayerStart(_list, _layer + 1, _first, _total)};
}
} // namespace

void Neuron::DrawList::BeginLayer(const CanvasRect& _frame, bool _overlay)
{
  layers.push_back({.frame = _frame,
                    .overlay = _overlay,
                    .firstPanel = panels.size(),
                    .firstLine = lines.size(),
                    .firstText = texts.size(),
                    .firstSprite = sprites.size(),
                    .firstRegion = regions.size()});
}

std::size_t Neuron::DrawList::LayerAt(float _xPixels, float _yPixels) const noexcept
{
  for (std::size_t layer = layers.size(); layer-- > 0;)
  {
    if (layers[layer].frame.Contains(_xPixels, _yPixels))
      return layer + 1;
  }
  return 0;
}

Neuron::CanvasSpan Neuron::DrawList::PanelsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &CanvasLayer::firstPanel, panels.size());
}

Neuron::CanvasSpan Neuron::DrawList::LinesOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &CanvasLayer::firstLine, lines.size());
}

Neuron::CanvasSpan Neuron::DrawList::TextsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &CanvasLayer::firstText, texts.size());
}

Neuron::CanvasSpan Neuron::DrawList::SpritesOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &CanvasLayer::firstSprite, sprites.size());
}

Neuron::CanvasSpan Neuron::DrawList::RegionsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &CanvasLayer::firstRegion, regions.size());
}

bool Neuron::DrawList::Covers(float _xPixels, float _yPixels) const noexcept
{
  return std::ranges::any_of(panels, [&](const CanvasRect& _panel) { return _panel.Contains(_xPixels, _yPixels); });
}

const Neuron::CanvasRegion* Neuron::DrawList::HitAt(float _xPixels, float _yPixels) const noexcept
{
  const CanvasSpan span = RegionsOf(LayerAt(_xPixels, _yPixels));
  for (std::size_t i = span.first; i < span.end; ++i)
  {
    if (!regions[i].hoverOnly && regions[i].area.Contains(_xPixels, _yPixels))
      return &regions[i];
  }
  return nullptr;
}

const Neuron::CanvasRegion* Neuron::DrawList::PointedAt(float _xPixels, float _yPixels) const noexcept
{
  if (const CanvasRegion* hit = HitAt(_xPixels, _yPixels))
    return hit;
  const CanvasSpan span = RegionsOf(LayerAt(_xPixels, _yPixels));
  for (std::size_t i = span.first; i < span.end; ++i)
  {
    if (regions[i].hoverOnly && regions[i].area.Contains(_xPixels, _yPixels))
      return &regions[i];
  }
  return nullptr;
}

const Neuron::CanvasRegion* Neuron::DrawList::ScrollAt(float _xPixels, float _yPixels) const noexcept
{
  const CanvasSpan span = RegionsOf(LayerAt(_xPixels, _yPixels));
  for (std::size_t i = span.end; i-- > span.first;)
  {
    if (regions[i].scrolls && regions[i].area.Contains(_xPixels, _yPixels))
      return &regions[i];
  }
  return nullptr;
}

const Neuron::CanvasRect* Neuron::DrawList::ClipOf(CanvasSpan CanvasClip::*_list, std::size_t _index) const noexcept
{
  for (const CanvasClip& clip : clips)
  {
    if (_index >= (clip.*_list).first && _index < (clip.*_list).end)
      return &clip.area;
  }
  return nullptr;
}

std::optional<Neuron::CanvasQuad> Neuron::ClipQuad(const CanvasQuad& _quad, const CanvasRect& _clip) noexcept
{
  const float left = std::max(_quad.left, _clip.left);
  const float top = std::max(_quad.top, _clip.top);
  const float right = std::min(_quad.right, _clip.left + _clip.width);
  const float bottom = std::min(_quad.bottom, _clip.top + _clip.height);
  if (left >= right || top >= bottom)
    return std::nullopt;
  // Each coordinate where the cut edge stands between the quad's own, which a quad of no width or height never needs.
  const auto across = [&_quad](float _x) { return _quad.right > _quad.left ? (_x - _quad.left) / (_quad.right - _quad.left) : 0.0f; };
  const auto down = [&_quad](float _y) { return _quad.bottom > _quad.top ? (_y - _quad.top) / (_quad.bottom - _quad.top) : 0.0f; };
  return CanvasQuad{.left = left,
                    .top = top,
                    .right = right,
                    .bottom = bottom,
                    .u0 = std::lerp(_quad.u0, _quad.u1, across(left)),
                    .v0 = std::lerp(_quad.v0, _quad.v1, down(top)),
                    .u1 = std::lerp(_quad.u0, _quad.u1, across(right)),
                    .v1 = std::lerp(_quad.v0, _quad.v1, down(bottom))};
}

std::optional<std::pair<DirectX::XMFLOAT2, DirectX::XMFLOAT2>> Neuron::ClipToRect(DirectX::XMFLOAT2 _from, DirectX::XMFLOAT2 _to,
                                                                                  const CanvasRect& _rect) noexcept
{
  const float dx = _to.x - _from.x;
  const float dy = _to.y - _from.y;
  // How far along the line it enters the rectangle and leaves it, each edge taken as a bound on the share along: the left,
  // the right, the top and the bottom.
  float enter = 0.0f;
  float leave = 1.0f;
  const std::array<std::pair<float, float>, 4> edges{{{-dx, _from.x - _rect.left},
                                                      {dx, _rect.left + _rect.width - _from.x},
                                                      {-dy, _from.y - _rect.top},
                                                      {dy, _rect.top + _rect.height - _from.y}}};
  for (const auto& [toward, room] : edges)
  {
    if (toward == 0.0f)
    {
      if (room < 0.0f)
        return std::nullopt;
      continue;
    }
    const float share = room / toward;
    if (toward < 0.0f)
      enter = std::max(enter, share);
    else
      leave = std::min(leave, share);
  }
  if (enter > leave)
    return std::nullopt;
  return std::pair{DirectX::XMFLOAT2{_from.x + (enter * dx), _from.y + (enter * dy)},
                   DirectX::XMFLOAT2{_from.x + (leave * dx), _from.y + (leave * dy)}};
}
