#include "pch.h"
#include "TextMetrics.h"

#include <cmath>

float Neuron::TextMetrics::Width(std::size_t _font, std::string_view _text, float _trackingUnits) const noexcept
{
  if (_font >= m_fonts.size() || m_scale <= 0.0f)
    return 0.0f;
  return m_fonts[_font].Width(_text, std::round(_trackingUnits * m_scale)) / m_scale;
}

std::string Neuron::TextMetrics::Fit(std::size_t _font, std::string_view _text, float _widthUnits, float _trackingUnits) const
{
  if (Width(_font, _text, _trackingUnits) <= _widthUnits)
    return std::string(_text);
  // The longest start of the text that fits with the dots after it, less any space it would end on. ends[n] is where the
  // first n characters end, counting a character of several UTF-8 bytes once.
  constexpr std::string_view DOTS = "...";
  std::vector<std::size_t> ends{0};
  for (std::size_t index = 0; index < _text.size();)
  {
    (void)NextCodePoint(_text, index);
    ends.push_back(index);
  }
  for (std::size_t characters = ends.size() - 1; characters-- > 0;)
  {
    std::string_view start = _text.substr(0, ends[characters]);
    while (!start.empty() && start.back() == ' ')
      start.remove_suffix(1);
    std::string cut = std::string(start) + std::string(DOTS);
    if (Width(_font, cut, _trackingUnits) <= _widthUnits)
      return cut;
  }
  return {};
}

std::vector<std::string> Neuron::TextMetrics::Wrap(std::size_t _font, std::string_view _text, float _widthUnits, float _trackingUnits) const
{
  std::vector<std::string> lines;
  std::string line;
  for (std::size_t start = 0; start < _text.size();)
  {
    const std::size_t space = _text.find(' ', start);
    const std::string_view word = _text.substr(start, space == std::string_view::npos ? std::string_view::npos : space - start);
    start = space == std::string_view::npos ? _text.size() : space + 1;
    if (word.empty())
      continue;
    std::string joined = line.empty() ? std::string(word) : std::format("{} {}", line, word);
    if (Width(_font, joined, _trackingUnits) <= _widthUnits)
    {
      line = std::move(joined);
      continue;
    }
    if (!line.empty())
      lines.push_back(std::move(line));
    line = Fit(_font, word, _widthUnits, _trackingUnits);
  }
  if (!line.empty())
    lines.push_back(std::move(line));
  return lines;
}
