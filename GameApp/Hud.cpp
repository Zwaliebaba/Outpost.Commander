#include "pch.h"
#include "Hud.h"

#include <algorithm>

namespace
{
using Outpost::Hud;

// Dark translucent panels and light text; a button is a lighter panel, dim when it does nothing.
constexpr DirectX::XMFLOAT4 PANEL_COLOR{0.01f, 0.015f, 0.03f, 0.78f};
constexpr DirectX::XMFLOAT4 TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f};
constexpr DirectX::XMFLOAT4 DIM_TEXT_COLOR{0.42f, 0.45f, 0.5f, 1.0f};
constexpr DirectX::XMFLOAT4 ORE_COLOR{1.0f, 0.8f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 BUTTON_COLOR{0.08f, 0.16f, 0.26f, 0.92f};
constexpr DirectX::XMFLOAT4 DISABLED_BUTTON_COLOR{0.05f, 0.06f, 0.08f, 0.85f};
// The minimap: the map's square, and its marks in the side's color; the camera's view as a light outline.
constexpr DirectX::XMFLOAT4 MAP_COLOR{0.02f, 0.04f, 0.07f, 0.95f};
constexpr DirectX::XMFLOAT4 OWN_COLOR{0.35f, 0.65f, 1.0f, 1.0f};
constexpr DirectX::XMFLOAT4 ENEMY_COLOR{1.0f, 0.38f, 0.25f, 1.0f};
constexpr DirectX::XMFLOAT4 NEUTRAL_COLOR{0.45f, 0.42f, 0.4f, 1.0f};
constexpr DirectX::XMFLOAT4 VIEW_COLOR{0.85f, 0.9f, 1.0f, 0.8f};

// Everything below in reference units.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
constexpr float LINE_STEP = 26.0f;

// The Ore panel, anchored to the top-left corner.
constexpr float ORE_PANEL_WIDTH = 300.0f;
constexpr float ORE_PANEL_HEIGHT = 44.0f;
constexpr float ORE_VALUE_LEFT = 76.0f;
constexpr float ORE_INCOME_LEFT = 196.0f;

// The selection panel, anchored to the bottom edge's middle.
constexpr float SELECTION_PANEL_WIDTH = 560.0f;
// Lines of designs before the rest are counted together.
constexpr size_t SELECTION_DESIGN_LINES = 5;

// The buttons, stacked in a panel anchored to the bottom-right corner.
constexpr float BUTTON_PANEL_WIDTH = 380.0f;
constexpr float BUTTON_HEIGHT = 34.0f;
constexpr float BUTTON_GAP = 6.0f;
// The cost, right of the label.
constexpr float BUTTON_COST_LEFT = 290.0f;

// The minimap, a square anchored to the bottom-left corner, with the map drawn inside its padding.
constexpr float MINIMAP_SIZE = 260.0f;
constexpr float MINIMAP_PADDING = 8.0f;
// The smallest a mark is drawn, and how wide the view's outline is, so that both stay visible.
constexpr float SHIP_MARK_UNITS = 3.0f;
constexpr float STRUCTURE_MARK_UNITS = 6.0f;
constexpr float VIEW_LINE_UNITS = 1.5f;

// The hint, anchored to the top edge's middle.
constexpr float HINT_PANEL_WIDTH = 640.0f;

// A Constructor is of no design; the panel names it so.
constexpr std::string_view CONSTRUCTOR_NAME = "Constructor";

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
  return std::ranges::any_of(panels, [&](const Rect& _panel) { return _panel.Contains(_xPixels, _yPixels); });
}

std::optional<Outpost::Hud::Action> Outpost::Hud::Layout::ActionAt(float _xPixels, float _yPixels) const noexcept
{
  for (const auto& [area, action] : actions)
  {
    if (area.Contains(_xPixels, _yPixels))
      return action;
  }
  return std::nullopt;
}

std::optional<Outpost::PlanePosition> Outpost::Hud::Layout::MapPointAt(float _xPixels, float _yPixels) const noexcept
{
  if (mapSizeMeters <= 0.0f || !minimap.Contains(_xPixels, _yPixels))
    return std::nullopt;
  // The map's +x runs right and its +z up, as the default camera shows it.
  const float half = mapSizeMeters / 2.0f;
  return PlanePosition{.xMeters = -half + ((_xPixels - minimap.left) / minimap.width * mapSizeMeters),
                       .zMeters = half - ((_yPixels - minimap.top) / minimap.height * mapSizeMeters)};
}

DirectX::XMFLOAT2 Outpost::Hud::Layout::MinimapPixelOf(PlanePosition _point) const noexcept
{
  const float half = mapSizeMeters / 2.0f;
  const float x = std::clamp((_point.xMeters + half) / mapSizeMeters, 0.0f, 1.0f);
  const float y = std::clamp((half - _point.zMeters) / mapSizeMeters, 0.0f, 1.0f);
  return {minimap.left + (x * minimap.width), minimap.top + (y * minimap.height)};
}

Outpost::Hud::Content Outpost::Hud::Describe(const Snapshot& _newest, std::span<const EntityView> _entities,
                                             std::span<const EntityId> _selected, std::optional<StructureKind> _placing)
{
  Content content{.ore = _newest.ore,
                  .oreIncomeHundredthsPerSecond = _newest.oreIncomeHundredthsPerSecond,
                  .selection = {},
                  .buttons = {},
                  .hint = {},
                  .mapSizeMeters = _newest.mapSizeMeters,
                  .marks = {}};

  content.marks.reserve(_entities.size());
  for (const EntityView& entity : _entities)
  {
    const Side side = !entity.owner.IsValid() ? Side::Neutral : entity.owner == _newest.player ? Side::Own : Side::Enemy;
    content.marks.push_back({.position = entity.position, .radiusMeters = entity.radiusMeters, .side = side, .kind = entity.kind});
  }

  const auto typeOf = [&_newest](StructureKind _kind) -> const StructureTypeView*
  {
    const auto type = std::ranges::find(_newest.structureTypes, _kind, &StructureTypeView::structure);
    return type != _newest.structureTypes.end() ? &*type : nullptr;
  };
  if (_placing.has_value())
  {
    const StructureTypeView* type = typeOf(*_placing);
    content.hint = std::format("Placing {}: left-click to build, right-click to cancel", type != nullptr ? type->nameUtf8 : "a structure");
  }

  const auto nameOf = [&_newest](DesignId _design) -> std::string
  {
    if (!_design.IsValid())
      return std::string(CONSTRUCTOR_NAME);
    const auto design = std::ranges::find(_newest.designs, _design, &DesignView::id);
    return design != _newest.designs.end() ? design->nameUtf8 : std::string("Unknown design");
  };

  // A structure is selected on its own (PlayerControls): its kind, its state, its queue and what it can make.
  if (_selected.size() == 1)
  {
    const auto structure = std::ranges::find(_entities, _selected.front(), &EntityView::id);
    if (structure != _entities.end() && structure->kind == EntityKind::Structure)
    {
      const StructureTypeView* type = typeOf(structure->structure);
      content.selection.push_back(type != nullptr ? type->nameUtf8 : std::string("Structure"));
      if (structure->builtPermille < PERMILLE)
        content.selection.push_back(std::format("Under construction, {}%", structure->builtPermille / 10));
      content.selection.push_back(std::format("Hit points {} / {}", WithThousands(WholePoints(structure->hitPointsHundredths)),
                                              WithThousands(WholePoints(structure->maxHitPointsHundredths))));
      for (size_t i = 0; i < structure->queue.size(); ++i)
      {
        const JobView& job = structure->queue[i];
        const std::string name = nameOf(job.role == ShipRole::Constructor ? DesignId{} : job.design);
        if (i > 0)
          content.selection.push_back(std::format("{}. {}", i + 1, name));
        else if (structure->jobPermille > 0)
          content.selection.push_back(std::format("1. {}, {}%", name, structure->jobPermille / 10));
        else
          content.selection.push_back(std::format("1. {}, waiting for Ore", name));
      }

      const bool canQueue = structure->builtPermille >= PERMILLE && structure->queue.size() < QUEUE_LIMIT;
      if (structure->structure == StructureKind::CommandStation)
      {
        content.buttons.push_back({.label = std::string(CONSTRUCTOR_NAME),
                                   .action = {.kind = ActionKind::Queue, .producer = structure->id},
                                   .enabled = canQueue && _newest.ore >= _newest.constructorCost});
        content.buttons.back().label += std::format("|{}", _newest.constructorCost);
      }
      else if (structure->structure == StructureKind::Shipyard)
      {
        content.buttons.reserve(_newest.designs.size());
        for (const DesignView& design : _newest.designs)
          content.buttons.push_back({.label = std::format("{}|{}", design.nameUtf8, design.cost),
                                     .action = {.kind = ActionKind::Queue, .producer = structure->id, .design = design.id},
                                     .enabled = canQueue && _newest.ore >= design.cost});
      }
      return content;
    }
  }

  // Count the selected ships by design, keeping the designs in the order first seen.
  std::vector<std::pair<DesignId, size_t>> byDesign;
  std::int64_t hitPoints = 0;
  std::int64_t maxHitPoints = 0;
  bool constructors = false;
  for (const EntityId id : _selected)
  {
    const auto ship = std::ranges::find(_entities, id, &EntityView::id);
    if (ship == _entities.end() || ship->kind != EntityKind::Ship)
      continue;
    constructors = constructors || ship->role == ShipRole::Constructor;
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
    content.selection.reserve(byDesign.size() + 3);
    content.selection.push_back(std::format("{} ships", ships));
    for (size_t i = 0; i < byDesign.size() && i < SELECTION_DESIGN_LINES; ++i)
      content.selection.push_back(std::format("{} x {}", byDesign[i].second, nameOf(byDesign[i].first)));
    if (byDesign.size() > SELECTION_DESIGN_LINES)
      content.selection.push_back(std::format("and {} more designs", byDesign.size() - SELECTION_DESIGN_LINES));
  }
  content.selection.push_back(
    std::format("Hit points {} / {}", WithThousands(WholePoints(hitPoints)), WithThousands(WholePoints(maxHitPoints))));

  // Constructors offer every structure they build (design §6); one Research Lab a player.
  if (constructors)
  {
    const bool hasLab = std::ranges::any_of(_entities,
                                            [&_newest](const EntityView& _entity) {
                                              return _entity.kind == EntityKind::Structure &&
                                                     _entity.structure == StructureKind::ResearchLab && _entity.owner == _newest.player;
                                            });
    content.buttons.reserve(_newest.structureTypes.size());
    for (const StructureTypeView& type : _newest.structureTypes)
    {
      if (!type.buildable)
        continue;
      const bool allowed = !(type.structure == StructureKind::ResearchLab && hasLab);
      content.buttons.push_back({.label = std::format("{}|{}", type.nameUtf8, type.cost),
                                 .action = {.kind = ActionKind::Build, .structure = type.structure},
                                 .enabled = allowed && _newest.ore >= type.cost});
    }
  }
  return content;
}

float Outpost::Hud::Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  return std::min(static_cast<float>(_widthPixels) / REFERENCE_WIDTH_UNITS, static_cast<float>(_heightPixels) / REFERENCE_HEIGHT_UNITS);
}

Outpost::Hud::Layout Outpost::Hud::Lay(const Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                       std::span<const PlanePosition> _view)
{
  const float scale = Scale(_widthPixels, _heightPixels);
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  Layout layout{.fontPixels = FONT_UNITS * scale, .panels = {}, .texts = {}, .actions = {}, .minimap = {}, .mapSizeMeters = 0.0f};

  // Top-left anchor: the Ore, and what the rigs earn each second.
  const float oreLeft = MARGIN * scale;
  const float oreTop = MARGIN * scale;
  layout.panels.push_back({oreLeft, oreTop, ORE_PANEL_WIDTH * scale, ORE_PANEL_HEIGHT * scale, PANEL_COLOR});
  const float textTop = oreTop + ((ORE_PANEL_HEIGHT - LINE_STEP) / 2.0f * scale);
  layout.texts.push_back({"Ore", oreLeft + (PADDING * scale), textTop, TEXT_COLOR});
  layout.texts.push_back({WithThousands(_content.ore), oreLeft + (ORE_VALUE_LEFT * scale), textTop, ORE_COLOR});
  const std::int32_t income = _content.oreIncomeHundredthsPerSecond;
  const std::string incomeText = income % HUNDREDTHS == 0 ? std::format("+{}/s", income / HUNDREDTHS)
                                                          : std::format("+{:.1f}/s", static_cast<double>(income) / HUNDREDTHS);
  layout.texts.push_back({incomeText, oreLeft + (ORE_INCOME_LEFT * scale), textTop, TEXT_COLOR});

  // Top-middle anchor: what a click on the ground will do.
  if (!_content.hint.empty())
  {
    const float left = (width / 2.0f) - (HINT_PANEL_WIDTH / 2.0f * scale);
    layout.panels.push_back({left, oreTop, HINT_PANEL_WIDTH * scale, ORE_PANEL_HEIGHT * scale, PANEL_COLOR});
    layout.texts.push_back({_content.hint, left + (PADDING * scale), textTop, TEXT_COLOR});
  }

  // Bottom-middle anchor: the selection.
  if (!_content.selection.empty())
  {
    const float panelHeight = (2.0f * PADDING) + (LINE_STEP * static_cast<float>(_content.selection.size()));
    const float left = (width / 2.0f) - (SELECTION_PANEL_WIDTH / 2.0f * scale);
    const float top = height - ((MARGIN + panelHeight) * scale);
    layout.panels.push_back({left, top, SELECTION_PANEL_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
    for (size_t line = 0; line < _content.selection.size(); ++line)
      layout.texts.push_back({_content.selection[line], left + (PADDING * scale),
                              top + ((PADDING + (LINE_STEP * static_cast<float>(line))) * scale), TEXT_COLOR});
  }

  // Bottom-right anchor: the buttons, stacked upward from the corner. A label holds the name and the cost, split at '|'.
  if (!_content.buttons.empty())
  {
    const auto count = static_cast<float>(_content.buttons.size());
    const float panelHeight = (2.0f * PADDING) + (count * BUTTON_HEIGHT) + ((count - 1.0f) * BUTTON_GAP);
    const float left = width - ((MARGIN + BUTTON_PANEL_WIDTH) * scale);
    const float top = height - ((MARGIN + panelHeight) * scale);
    layout.panels.push_back({left, top, BUTTON_PANEL_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
    for (size_t i = 0; i < _content.buttons.size(); ++i)
    {
      const Button& button = _content.buttons[i];
      const Rect area{left + (PADDING * scale), top + ((PADDING + (static_cast<float>(i) * (BUTTON_HEIGHT + BUTTON_GAP))) * scale),
                      (BUTTON_PANEL_WIDTH - (2.0f * PADDING)) * scale, BUTTON_HEIGHT * scale,
                      button.enabled ? BUTTON_COLOR : DISABLED_BUTTON_COLOR};
      layout.panels.push_back(area);
      if (button.enabled)
        layout.actions.emplace_back(area, button.action);
      const size_t split = button.label.find('|');
      const float labelTop = area.top + ((BUTTON_HEIGHT - LINE_STEP) / 2.0f * scale);
      const DirectX::XMFLOAT4& color = button.enabled ? TEXT_COLOR : DIM_TEXT_COLOR;
      layout.texts.push_back({button.label.substr(0, split), area.left + (PADDING * scale), labelTop, color});
      if (split != std::string::npos)
        layout.texts.push_back(
          {button.label.substr(split + 1), area.left + (BUTTON_COST_LEFT * scale), labelTop, button.enabled ? ORE_COLOR : DIM_TEXT_COLOR});
    }
  }

  // Bottom-left anchor: the minimap, with every mark and the camera's view.
  if (_content.mapSizeMeters > 0.0f)
  {
    const float size = MINIMAP_SIZE * scale;
    const float left = MARGIN * scale;
    const float top = height - ((MARGIN + MINIMAP_SIZE) * scale);
    layout.panels.push_back({left, top, size, size, PANEL_COLOR});
    const float inner = (MINIMAP_SIZE - (2.0f * MINIMAP_PADDING)) * scale;
    layout.minimap = {left + (MINIMAP_PADDING * scale), top + (MINIMAP_PADDING * scale), inner, inner, MAP_COLOR};
    layout.mapSizeMeters = _content.mapSizeMeters;
    layout.panels.push_back(layout.minimap);

    const float pixelsPerMeter = inner / _content.mapSizeMeters;
    for (const Mark& mark : _content.marks)
    {
      const float smallest = (mark.kind == EntityKind::Ship ? SHIP_MARK_UNITS : STRUCTURE_MARK_UNITS) * scale;
      const float side = std::max(smallest, 2.0f * mark.radiusMeters * pixelsPerMeter);
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(mark.position);
      const DirectX::XMFLOAT4& color = mark.side == Side::Own ? OWN_COLOR : mark.side == Side::Enemy ? ENEMY_COLOR : NEUTRAL_COLOR;
      layout.panels.push_back({at.x - (side / 2.0f), at.y - (side / 2.0f), side, side, color});
    }

    // The view's outline, as the box around the ground the camera shows.
    if (!_view.empty())
    {
      DirectX::XMFLOAT2 low = layout.MinimapPixelOf(_view.front());
      DirectX::XMFLOAT2 high = low;
      for (const PlanePosition corner : _view)
      {
        const DirectX::XMFLOAT2 pixel = layout.MinimapPixelOf(corner);
        low = {std::min(low.x, pixel.x), std::min(low.y, pixel.y)};
        high = {std::max(high.x, pixel.x), std::max(high.y, pixel.y)};
      }
      const float line = VIEW_LINE_UNITS * scale;
      layout.panels.push_back({low.x, low.y, high.x - low.x, line, VIEW_COLOR});
      layout.panels.push_back({low.x, high.y - line, high.x - low.x, line, VIEW_COLOR});
      layout.panels.push_back({low.x, low.y, line, high.y - low.y, VIEW_COLOR});
      layout.panels.push_back({high.x - line, low.y, line, high.y - low.y, VIEW_COLOR});
    }
  }
  return layout;
}
