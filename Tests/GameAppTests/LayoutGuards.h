#pragma once

namespace GameAppTests
{
// The interface's fonts as the game rasterizes them for a back buffer of this size, kept for the run, since a TextMetrics reads
// them where they are (ADR-061). A font that is not installed throws, as ADR-030 has it: the tests never guess a width.
inline std::span<const Neuron::GlyphAtlas::Font> FontsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor = 1.0f)
{
  static std::map<float, std::vector<Neuron::GlyphAtlas::Font>> g_fontsByScale;
  const float scale = Outpost::Hud::Scale(_widthPixels, _heightPixels, _factor);
  auto found = g_fontsByScale.find(scale);
  if (found == g_fontsByScale.end())
  {
    found =
      g_fontsByScale.emplace(scale, Neuron::RasterizeUiAtlas(Outpost::Hud::Typefaces(), Outpost::Hud::Sprites(), scale).glyphs.fonts).first;
  }
  return found->second;
}

inline Outpost::Hud::TextMetrics MetricsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor = 1.0f)
{
  return {FontsFor(_widthPixels, _heightPixels, _factor), Outpost::Hud::Scale(_widthPixels, _heightPixels, _factor)};
}

// The box a text's line is set in, in pixels, as UiPipeline::DrawText sets it: from its pen, rounded to a pixel, as wide as
// its characters' advances and its tracking, and a line of its font tall.
inline Neuron::CanvasRect LineBoxOf(const Neuron::CanvasText& _text, std::span<const Neuron::GlyphAtlas::Font> _fonts)
{
  const Neuron::GlyphAtlas::Font& font = _fonts[_text.font];
  return {.left = std::round(_text.left),
          .top = std::round(_text.top),
          .width = font.Width(_text.text, std::round(_text.trackingPixels)),
          .height = font.lineHeight};
}

// _rect cut to _clip, if it has one, as UiPipeline draws what a scroll view holds (ADR-100): of no size where none of it is
// inside.
inline Neuron::CanvasRect CutTo(Neuron::CanvasRect _rect, const Neuron::CanvasRect* _clip)
{
  if (_clip == nullptr)
    return _rect;
  const float left = std::max(_rect.left, _clip->left);
  const float top = std::max(_rect.top, _clip->top);
  _rect.width = std::max(0.0f, std::min(_rect.left + _rect.width, _clip->left + _clip->width) - left);
  _rect.height = std::max(0.0f, std::min(_rect.top + _rect.height, _clip->top + _clip->height) - top);
  _rect.left = left;
  _rect.top = top;
  return _rect;
}

// What of the text and the panel of these places in _layout is drawn: its line's box, and the panel, each cut to its clip.
inline Neuron::CanvasRect DrawnBoxOf(const Neuron::DrawList& _layout, std::size_t _text, std::span<const Neuron::GlyphAtlas::Font> _fonts)
{
  return CutTo(LineBoxOf(_layout.texts[_text], _fonts), _layout.ClipOf(&Neuron::CanvasClip::texts, _text));
}

inline Neuron::CanvasRect DrawnPanelOf(const Neuron::DrawList& _layout, std::size_t _panel)
{
  return CutTo(_layout.panels[_panel], _layout.ClipOf(&Neuron::CanvasClip::panels, _panel));
}

// WCAG's relative luminance of a linear color, which the render target encodes to sRGB, as the review computed it.
inline float LuminanceOf(const DirectX::XMFLOAT3& _color) noexcept
{
  return (0.2126f * _color.x) + (0.7152f * _color.y) + (0.0722f * _color.z);
}

// WCAG's contrast ratio between two linear colors.
inline float ContrastOf(const DirectX::XMFLOAT3& _first, const DirectX::XMFLOAT3& _second) noexcept
{
  const float first = LuminanceOf(_first) + 0.05f;
  const float second = LuminanceOf(_second) + 0.05f;
  return std::max(first, second) / std::min(first, second);
}

// What a text can stand on: the panels of its layer under its first pixel, laid over each other in the order they are
// drawn, at their alpha, over black, each as far as it is drawn. A hatched panel covers some pixels and not others, so it
// is taken both ways.
inline std::vector<DirectX::XMFLOAT3> GroundsOf(const Neuron::DrawList& _layout, std::size_t _layer, const Neuron::CanvasText& _text)
{
  const Neuron::CanvasSpan panels = _layout.PanelsOf(_layer);
  const float x = std::round(_text.left) + 0.5f;
  const float y = std::round(_text.top) + 0.5f;
  std::vector<DirectX::XMFLOAT3> grounds{{0.0f, 0.0f, 0.0f}};
  for (std::size_t p = panels.first; p < panels.end; ++p)
  {
    const Neuron::CanvasRect panel = DrawnPanelOf(_layout, p);
    if (!panel.Contains(x, y))
      continue;
    const float alpha = panel.color.w;
    const std::size_t count = grounds.size();
    for (std::size_t g = 0; g < count; ++g)
    {
      const DirectX::XMFLOAT3 under = grounds[g];
      const DirectX::XMFLOAT3 covered{(panel.color.x * alpha) + (under.x * (1.0f - alpha)),
                                      (panel.color.y * alpha) + (under.y * (1.0f - alpha)),
                                      (panel.color.z * alpha) + (under.z * (1.0f - alpha))};
      if (panel.fill == Neuron::CanvasFill::Solid)
        grounds[g] = covered;
      else
        grounds.push_back(covered);
    }
  }
  return grounds;
}

// A text of a layout called _name, as a guard names it.
inline std::wstring Named(const std::wstring& _name, const Neuron::CanvasText& _text)
{
  return std::format(L"{}: \"{}\"", _name, std::wstring(winrt::to_hstring(_text.text)));
}

// ADR-061 decision 5: every text of _layout, called _name, that does not end inside the smallest panel it starts in, measured
// with _fonts, which it is drawn in, or that runs into a panel of its layer it does not start in, but a line two pixels thin
// or less, such as a divider its line's box may graze; one a line.
inline std::wstring TextsPastTheirPanels(const Neuron::DrawList& _layout, std::span<const Neuron::GlyphAtlas::Font> _fonts,
                                         const std::wstring& _name)
{
  std::wstring offenders;
  for (std::size_t layer = 0; layer < _layout.LayerCount(); ++layer)
  {
    const Neuron::CanvasSpan texts = _layout.TextsOf(layer);
    const Neuron::CanvasSpan panels = _layout.PanelsOf(layer);
    for (std::size_t t = texts.first; t < texts.end; ++t)
    {
      const Neuron::CanvasText& text = _layout.texts[t];
      const Neuron::CanvasRect box = DrawnBoxOf(_layout, t, _fonts);
      if (text.text.empty() || box.height <= 0.0f)
        continue;
      Neuron::CanvasRect smallest{};
      float smallestArea = std::numeric_limits<float>::infinity();
      for (std::size_t p = panels.first; p < panels.end; ++p)
      {
        const Neuron::CanvasRect panel = DrawnPanelOf(_layout, p);
        if (panel.Contains(box.left + 0.5f, box.top + 0.5f) && panel.width * panel.height < smallestArea)
        {
          smallest = panel;
          smallestArea = panel.width * panel.height;
        }
      }
      if (std::isinf(smallestArea))
        offenders += std::format(L"{} starts in no panel\n", Named(_name, text));
      else if (const float past = box.left + box.width - smallest.left - smallest.width; past > 0.5f)
        offenders += std::format(L"{} runs {} px past its panel\n", Named(_name, text), past);
      for (std::size_t p = panels.first; p < panels.end; ++p)
      {
        const Neuron::CanvasRect panel = DrawnPanelOf(_layout, p);
        if (panel.width <= 2.0f || panel.height <= 2.0f || panel.Contains(box.left + 0.5f, box.top + 0.5f))
          continue;
        const float across = std::min(box.left + box.width, panel.left + panel.width) - std::max(box.left, panel.left);
        const float down = std::min(box.top + box.height, panel.top + panel.height) - std::max(box.top, panel.top);
        if (across > 0.5f && down > 0.5f)
          offenders += std::format(L"{} runs {} px into a panel it does not start in\n", Named(_name, text), across);
      }
    }
  }
  return offenders;
}

// ADR-061 decision 5: every two texts of a layer of _layout whose line boxes meet; one pair a line.
inline std::wstring TextsThatMeet(const Neuron::DrawList& _layout, std::span<const Neuron::GlyphAtlas::Font> _fonts,
                                  const std::wstring& _name)
{
  std::wstring offenders;
  for (std::size_t layer = 0; layer < _layout.LayerCount(); ++layer)
  {
    const Neuron::CanvasSpan texts = _layout.TextsOf(layer);
    for (std::size_t a = texts.first; a < texts.end; ++a)
    {
      const Neuron::CanvasRect first = DrawnBoxOf(_layout, a, _fonts);
      for (std::size_t b = a + 1; b < texts.end; ++b)
      {
        const Neuron::CanvasRect second = DrawnBoxOf(_layout, b, _fonts);
        const bool meet = first.width > 0.0f && second.width > 0.0f && first.height > 0.0f && second.height > 0.0f &&
                          first.left < second.left + second.width && second.left < first.left + first.width &&
                          first.top < second.top + second.height && second.top < first.top + first.height;
        if (meet)
        {
          offenders +=
            std::format(L"{} meets \"{}\"\n", Named(_name, _layout.texts[a]), std::wstring(winrt::to_hstring(_layout.texts[b].text)));
        }
      }
    }
  }
  return offenders;
}

// ADR-062 decision 3: every text of _layout that stands at less than 4.5:1 against what it is drawn on, the panels under its
// first pixel drawn, with a hatched panel's stripes and the gaps between them both, at the least contrast it meets.
inline std::wstring TextsUnderContrast(const Neuron::DrawList& _layout, std::span<const Neuron::GlyphAtlas::Font> _fonts,
                                       const std::wstring& _name)
{
  std::wstring offenders;
  for (std::size_t layer = 0; layer < _layout.LayerCount(); ++layer)
  {
    const Neuron::CanvasSpan texts = _layout.TextsOf(layer);
    for (std::size_t t = texts.first; t < texts.end; ++t)
    {
      const Neuron::CanvasText& text = _layout.texts[t];
      const Neuron::CanvasRect box = DrawnBoxOf(_layout, t, _fonts);
      if (text.text.empty() || box.height <= 0.0f)
        continue;
      const DirectX::XMFLOAT3 ink{text.color.x, text.color.y, text.color.z};
      float least = std::numeric_limits<float>::max();
      // Its first pixel drawn, which a scroll view may cut below its line's top.
      for (const DirectX::XMFLOAT3& ground : GroundsOf(_layout, layer, {.text = text.text, .left = box.left, .top = box.top}))
        least = std::min(least, ContrastOf(ink, ground));
      if (least < 4.5f)
        offenders += std::format(L"{} stands at {:.2f}:1\n", Named(_name, text), least);
    }
  }
  return offenders;
}
} // namespace GameAppTests
