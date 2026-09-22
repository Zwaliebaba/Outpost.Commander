#include "pch.h"

#include "TextLayout.h"

#include <cmath>

namespace Neuron
{

namespace
{
[[nodiscard]] const GlyphEntry* Find(const GlyphTable& _table, wchar_t _character) noexcept
{
  if ((_character < FIRST_CHARACTER) || (_character > LAST_CHARACTER))
  {
    return nullptr;
  }
  const GlyphEntry& entry = _table[static_cast<std::size_t>(_character - FIRST_CHARACTER)];
  return entry.present ? &entry : nullptr;
}

/// Half away from zero, which is what `std::lround` does and what a pixel grid wants: -0.5 and 0.5 go
/// to the same distance from the origin.
[[nodiscard]] float Snap(float _value) noexcept
{
  return static_cast<float>(std::lround(_value));
}
} // namespace

float BaselinePixels(const FaceMetrics& _face, float _boxTopPixels, float _boxHeightPixels) noexcept
{
  // CSS's half-leading: the content area is ascent plus descent, centered in the line box. Negative when
  // the font's content area is taller than the box, which for every text rect in this design it is.
  const float halfLeading = (_boxHeightPixels - (_face.ascentPixels + _face.descentPixels)) * 0.5f;
  return _boxTopPixels + halfLeading + _face.ascentPixels;
}

float MeasureRunPixels(const GlyphTable& _table, const FaceMetrics& _face, std::wstring_view _text, float _trackingEm) noexcept
{
  const float tracking = _trackingEm * _face.emSizePixels;
  float width = 0.0f;
  for (const wchar_t character : _text)
  {
    const GlyphEntry* entry = Find(_table, character);
    if (entry == nullptr)
    {
      // Not in the range or not in the font: it draws nothing and moves the pen nowhere, including by
      // the tracking, so a missing character is a gap of zero rather than a gap of one space.
      continue;
    }
    width += entry->advancePixels + tracking;
  }
  return width;
}

std::size_t LayoutText(const GlyphTable& _table, const FaceMetrics& _face, std::uint32_t _atlasWidthPixels,
                       std::uint32_t _atlasHeightPixels, const TextRun& _run, std::vector<GlyphQuad>& _out)
{
  if ((_atlasWidthPixels == 0) || (_atlasHeightPixels == 0) || _run.text.empty())
  {
    return 0;
  }

  const float width = MeasureRunPixels(_table, _face, _run.text, _run.trackingEm);
  const float boxLeft = static_cast<float>(_run.box.left);
  const float boxRight = static_cast<float>(_run.box.right);

  float pen = boxLeft;
  if (_run.align == TextAlign::Right)
  {
    pen = boxRight - width;
  }
  else if (_run.align == TextAlign::Center)
  {
    pen = boxLeft + ((boxRight - boxLeft - width) * 0.5f);
  }

  const float baseline = Snap(BaselinePixels(_face, static_cast<float>(_run.box.top), static_cast<float>(_run.box.bottom - _run.box.top)));
  const float tracking = _run.trackingEm * _face.emSizePixels;
  const float inverseWidth = 1.0f / static_cast<float>(_atlasWidthPixels);
  const float inverseHeight = 1.0f / static_cast<float>(_atlasHeightPixels);

  std::size_t appended = 0;
  for (const wchar_t character : _run.text)
  {
    const GlyphEntry* entry = Find(_table, character);
    if (entry == nullptr)
    {
      continue;
    }

    if ((entry->slot.widthPixels > 0) && (entry->slot.heightPixels > 0))
    {
      GlyphQuad quad;
      quad.left = Snap(pen) + entry->bearingXPixels;
      quad.top = baseline + entry->bearingYPixels;
      quad.right = quad.left + static_cast<float>(entry->slot.widthPixels);
      quad.bottom = quad.top + static_cast<float>(entry->slot.heightPixels);

      quad.u0 = static_cast<float>(entry->slot.left) * inverseWidth;
      quad.v0 = static_cast<float>(entry->slot.top) * inverseHeight;
      quad.u1 = static_cast<float>(entry->slot.left + entry->slot.widthPixels) * inverseWidth;
      quad.v1 = static_cast<float>(entry->slot.top + entry->slot.heightPixels) * inverseHeight;

      // **CROPPED, NOT SQUEEZED.** The texture coordinate moves by exactly the pixels the edge moved,
      // which is one texel per pixel because nothing here is scaled.
      if (quad.left < boxLeft)
      {
        quad.u0 += (boxLeft - quad.left) * inverseWidth;
        quad.left = boxLeft;
      }
      if (quad.right > boxRight)
      {
        quad.u1 -= (quad.right - boxRight) * inverseWidth;
        quad.right = boxRight;
      }

      if (quad.right > quad.left)
      {
        quad.red = _run.color.red;
        quad.green = _run.color.green;
        quad.blue = _run.color.blue;
        quad.alpha = _run.color.alpha;
        _out.push_back(quad);
        ++appended;
      }
    }

    pen += entry->advancePixels + tracking;
  }

  return appended;
}

bool AppendSolidQuad(const AtlasSlot& _solid, std::uint32_t _atlasWidthPixels, std::uint32_t _atlasHeightPixels, const PhysicalRect& _rect,
                     const QuadColor& _color, std::vector<GlyphQuad>& _out)
{
  if ((_rect.WidthPixels() <= 0) || (_rect.HeightPixels() <= 0) || (_atlasWidthPixels == 0) || (_atlasHeightPixels == 0))
  {
    return false;
  }

  // The middle of the block, at both corners, so every fragment samples one texel of full coverage.
  const float u =
    (static_cast<float>(_solid.left) + (static_cast<float>(_solid.widthPixels) * 0.5f)) / static_cast<float>(_atlasWidthPixels);
  const float v =
    (static_cast<float>(_solid.top) + (static_cast<float>(_solid.heightPixels) * 0.5f)) / static_cast<float>(_atlasHeightPixels);

  _out.push_back(GlyphQuad{.left = static_cast<float>(_rect.left),
                           .top = static_cast<float>(_rect.top),
                           .right = static_cast<float>(_rect.right),
                           .bottom = static_cast<float>(_rect.bottom),
                           .u0 = u,
                           .v0 = v,
                           .u1 = u,
                           .v1 = v,
                           .red = _color.red,
                           .green = _color.green,
                           .blue = _color.blue,
                           .alpha = _color.alpha});
  return true;
}

} // namespace Neuron
