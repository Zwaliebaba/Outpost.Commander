#include "pch.h"
#include "Hud.h"

#include <algorithm>

namespace
{
using Outpost::Hud;

// Placeholder looks until the full HUD (task 4.5): dark translucent panels and light text.
constexpr DirectX::XMFLOAT4 PANEL_COLOR{0.01f, 0.015f, 0.03f, 0.78f};
constexpr DirectX::XMFLOAT4 TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f};
constexpr DirectX::XMFLOAT4 ORE_COLOR{1.0f, 0.8f, 0.3f, 1.0f};

// Everything below in reference units.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
constexpr float LINE_STEP = 26.0f;

// The Ore panel, anchored to the top-left corner.
constexpr float ORE_PANEL_WIDTH = 240.0f;
constexpr float ORE_PANEL_HEIGHT = 44.0f;
constexpr float ORE_VALUE_LEFT = 76.0f;

// The selection panel, anchored to the bottom edge's middle.
constexpr float SELECTION_PANEL_WIDTH = 560.0f;
// Lines of designs before the rest are counted together.
constexpr size_t SELECTION_DESIGN_LINES = 5;

// Hit points as whole points, rounded up, so that a ship with a sliver left does not read as dead.
std::int64_t WholePoints(std::int64_t _hundredths)
{
  return (_hundredths + Outpost::HUNDREDTHS - 1) / Outpost::HUNDREDTHS;
}
} // namespace

std::string Outpost::WithThousands(std::int64_t _value)
{
  const std::string digits = std::to_string(_value < 0 ? -_value : _value);
  std::string grouped;
  for (size_t i = 0; i < digits.size(); ++i)
  {
    if (i > 0 && (digits.size() - i) % 3 == 0)
      grouped += ',';
    grouped += digits[i];
  }
  return _value < 0 ? "-" + grouped : grouped;
}

bool Outpost::Hud::Layout::Covers(float _xPixels, float _yPixels) const noexcept
{
  return std::ranges::any_of(panels,
                             [&](const Rect& _panel)
                             {
                               return _xPixels >= _panel.left && _xPixels < _panel.left + _panel.width && _yPixels >= _panel.top &&
                                      _yPixels < _panel.top + _panel.height;
                             });
}

Outpost::Hud::Content Outpost::Hud::Describe(const Snapshot& _newest, std::span<const EntityView> _entities,
                                             std::span<const EntityId> _selected)
{
  Content content{.ore = _newest.ore, .selection = {}};

  // Count the selected ships by design, keeping the designs in the order first seen.
  std::vector<std::pair<DesignId, size_t>> byDesign;
  std::int64_t hitPoints = 0;
  std::int64_t maxHitPoints = 0;
  for (const EntityId id : _selected)
  {
    const auto ship = std::ranges::find(_entities, id, &EntityView::id);
    if (ship == _entities.end())
      continue;
    hitPoints += ship->hitPointsHundredths;
    maxHitPoints += ship->maxHitPointsHundredths;
    const auto counted = std::ranges::find(byDesign, ship->design, &std::pair<DesignId, size_t>::first);
    if (counted == byDesign.end())
      byDesign.emplace_back(ship->design, 1);
    else
      ++counted->second;
  }
  if (byDesign.empty())
    return content;

  const auto nameOf = [&_newest](DesignId _design) -> std::string
  {
    const auto design = std::ranges::find(_newest.designs, _design, &DesignView::id);
    return design != _newest.designs.end() ? design->nameUtf8 : std::string("Unknown design");
  };
  // Most numerous first; ties keep the order first seen.
  std::ranges::stable_sort(byDesign, std::greater<>{}, &std::pair<DesignId, size_t>::second);

  size_t ships = 0;
  for (const auto& [design, count] : byDesign)
    ships += count;
  if (ships == 1)
  {
    content.selection.push_back(nameOf(byDesign.front().first));
  }
  else
  {
    content.selection.push_back(std::format("{} ships", ships));
    for (size_t i = 0; i < byDesign.size() && i < SELECTION_DESIGN_LINES; ++i)
      content.selection.push_back(std::format("{} x {}", byDesign[i].second, nameOf(byDesign[i].first)));
    if (byDesign.size() > SELECTION_DESIGN_LINES)
      content.selection.push_back(std::format("and {} more designs", byDesign.size() - SELECTION_DESIGN_LINES));
  }
  content.selection.push_back(
    std::format("Hit points {} / {}", WithThousands(WholePoints(hitPoints)), WithThousands(WholePoints(maxHitPoints))));
  return content;
}

float Outpost::Hud::Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  return std::min(static_cast<float>(_widthPixels) / REFERENCE_WIDTH_UNITS, static_cast<float>(_heightPixels) / REFERENCE_HEIGHT_UNITS);
}

Outpost::Hud::Layout Outpost::Hud::Lay(const Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const float scale = Scale(_widthPixels, _heightPixels);
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  Layout layout{.fontPixels = FONT_UNITS * scale, .panels = {}, .texts = {}};

  // Top-left anchor: reference units from the screen's top-left corner.
  const auto topLeft = [scale](float _x, float _y) { return std::pair{_x * scale, _y * scale}; };
  const auto [oreLeft, oreTop] = topLeft(MARGIN, MARGIN);
  layout.panels.push_back({oreLeft, oreTop, ORE_PANEL_WIDTH * scale, ORE_PANEL_HEIGHT * scale, PANEL_COLOR});
  const float textTop = oreTop + ((ORE_PANEL_HEIGHT - LINE_STEP) / 2.0f * scale);
  layout.texts.push_back({"Ore", oreLeft + (PADDING * scale), textTop, TEXT_COLOR});
  layout.texts.push_back({WithThousands(_content.ore), oreLeft + (ORE_VALUE_LEFT * scale), textTop, ORE_COLOR});

  if (_content.selection.empty())
    return layout;

  // Bottom-middle anchor: reference units from the bottom edge's middle.
  const float panelHeight = (2.0f * PADDING) + (LINE_STEP * static_cast<float>(_content.selection.size()));
  const float left = (width / 2.0f) - (SELECTION_PANEL_WIDTH / 2.0f * scale);
  const float top = height - ((MARGIN + panelHeight) * scale);
  layout.panels.push_back({left, top, SELECTION_PANEL_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
  for (size_t line = 0; line < _content.selection.size(); ++line)
    layout.texts.push_back(
      {_content.selection[line], left + (PADDING * scale), top + ((PADDING + (LINE_STEP * static_cast<float>(line))) * scale), TEXT_COLOR});
  return layout;
}
