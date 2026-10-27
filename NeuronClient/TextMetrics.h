#pragma once

namespace Neuron
{
// How wide the canvas's text is set, as the atlas it is drawn from sets it (ADR-061; ADR-100): the atlas's fonts, by index,
// rasterized for a back buffer at _scale pixels to a reference unit. A layout measures every line it places with it, so
// that a line is cut short, wrapped or given room by what it takes on the screen. It reads the fonts where they are, so
// they must outlive it.
class TextMetrics
{
public:
  TextMetrics(std::span<const GlyphAtlas::Font> _fonts, float _scale) noexcept
    : m_fonts(_fonts),
      m_scale(_scale)
  {
  }

  // How wide _text is set in font _font, with _trackingUnits more between each two characters, in reference units, as
  // UiPipeline::DrawText sets it: each character at its whole-pixel advance, and the tracking at a whole pixel. Nothing for
  // a font the atlas does not have.
  [[nodiscard]] float Width(std::size_t _font, std::string_view _text, float _trackingUnits = 0.0f) const noexcept;

  // _text as font _font sets it within _widthUnits: whole when it fits, and otherwise cut short with three dots, less any
  // space it would end on.
  [[nodiscard]] std::string Fit(std::size_t _font, std::string_view _text, float _widthUnits, float _trackingUnits = 0.0f) const;

  // _text broken between its words into lines that font _font sets within _widthUnits, with _trackingUnits more between
  // each two characters; a word wider than that is cut short.
  [[nodiscard]] std::vector<std::string> Wrap(std::size_t _font, std::string_view _text, float _widthUnits,
                                              float _trackingUnits = 0.0f) const;

  // The fonts, and the scale they were rasterized at.
  [[nodiscard]] std::span<const GlyphAtlas::Font> Fonts() const noexcept
  {
    return m_fonts;
  }
  [[nodiscard]] float Scale() const noexcept
  {
    return m_scale;
  }

private:
  std::span<const GlyphAtlas::Font> m_fonts;
  float m_scale = 1.0f;
};
} // namespace Neuron
