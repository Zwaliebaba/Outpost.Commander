#include "pch.h"
#include "Hud.h"

#include <algorithm>
#include <array>
#include <cmath>

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
constexpr DirectX::XMFLOAT4 SELECTED_BUTTON_COLOR{0.16f, 0.36f, 0.56f, 0.95f};
// A designer's name the server would refuse.
constexpr DirectX::XMFLOAT4 WARNING_COLOR{1.0f, 0.5f, 0.35f, 1.0f};
// The minimap: the map's square, and its marks in the side's color; the camera's view as a light outline.
constexpr DirectX::XMFLOAT4 MAP_COLOR{0.02f, 0.04f, 0.07f, 0.95f};
constexpr DirectX::XMFLOAT4 OWN_COLOR{0.35f, 0.65f, 1.0f, 1.0f};
constexpr DirectX::XMFLOAT4 ENEMY_COLOR{1.0f, 0.38f, 0.25f, 1.0f};
constexpr DirectX::XMFLOAT4 NEUTRAL_COLOR{0.45f, 0.42f, 0.4f, 1.0f};
// An asteroid field is only in the way, so it is drawn darker than an ore asteroid, which is worth going to, though a
// field's square is the larger (ADR-029).
constexpr DirectX::XMFLOAT4 FIELD_COLOR{0.13f, 0.13f, 0.14f, 1.0f};
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

// The research line, under the Ore panel.
constexpr float RESEARCH_PANEL_WIDTH = 560.0f;
constexpr float RESEARCH_PANEL_GAP = 8.0f;

// The designer, anchored to the top-right corner: a label column, then the picks or the name field.
constexpr float DESIGNER_WIDTH = 660.0f;
constexpr float DESIGNER_LABEL_WIDTH = 96.0f;
constexpr float PICK_WIDTH = 176.0f;
// The table's first column holds the row's label; the hulls follow.
constexpr float TABLE_LABEL_WIDTH = 230.0f;
constexpr float TABLE_COLUMN_WIDTH = 130.0f;
constexpr float ACTION_WIDTH = 220.0f;
// Room a button keeps for its cost at the right of a narrow button.
constexpr float COST_ROOM = 70.0f;

// The match's end, anchored to the top edge's middle under the hint: the outcome, its length, and the way back.
constexpr float BANNER_WIDTH = 520.0f;
// The main menu, centered.
constexpr float MENU_WIDTH = 440.0f;

// A Constructor is of no design; the panel names it so.
constexpr std::string_view CONSTRUCTOR_NAME = "Constructor";

// Hit points as whole points, rounded up, so that a ship with a sliver left does not read as dead.
std::int64_t WholePoints(std::int64_t _hundredths)
{
  return (_hundredths + Outpost::HUNDREDTHS - 1) / Outpost::HUNDREDTHS;
}

// A number as a whole when it is one, and to a tenth otherwise: 78, 32.5.
std::string Tenths(double _value)
{
  const double rounded = std::round(_value * 10.0) / 10.0;
  return rounded == std::round(rounded) ? std::format("{:.0f}", rounded) : std::format("{:.1f}", rounded);
}

// The front topic of the player's Research Lab, and how far it has come (design §8).
std::string ResearchLine(const Outpost::Snapshot& _newest, std::span<const Outpost::EntityView> _entities)
{
  for (const Outpost::EntityView& lab : _entities)
  {
    if (lab.kind != Outpost::EntityKind::Structure || lab.structure != Outpost::StructureKind::ResearchLab || lab.owner != _newest.player ||
        lab.research.empty())
      continue;
    const auto topic = std::ranges::find(_newest.research, lab.research.front(), &Outpost::ResearchTopicView::id);
    const std::string name = topic != _newest.research.end() ? topic->nameUtf8 : std::string("Research");
    std::string line = lab.jobPermille > 0 ? std::format("Researching {}, {}%", name, lab.jobPermille / 10)
                                           : std::format("Researching {}, waiting for Ore", name);
    if (lab.research.size() > 1)
      line += std::format(" (+{} queued)", lab.research.size() - 1);
    return line;
  }
  return {};
}

// The designer beside a selected Shipyard (task 5.2, design §9): a pick for each slot, the live stats, and the actions.
Outpost::Hud::DesignerPanel DescribeDesigner(const Outpost::Snapshot& _newest, const Outpost::Designer& _designer,
                                             const Outpost::EntityView& _yard)
{
  Hud::DesignerPanel panel;
  panel.name = _designer.Name(_newest);
  panel.editing = _designer.IsEditing();
  panel.nameValid = Outpost::IsValidDesignName(panel.name);

  const Outpost::DesignComponents picked = _designer.Picked();
  for (const Outpost::HullView& hull : _newest.hulls)
    panel.hulls.push_back({.label = hull.nameUtf8,
                           .action = {.kind = Hud::ActionKind::PickHull, .hull = hull.id},
                           .enabled = hull.available,
                           .selected = hull.id == picked.hull});
  for (const Outpost::DriveView& drive : _newest.drives)
    panel.drives.push_back({.label = drive.nameUtf8,
                            .action = {.kind = Hud::ActionKind::PickDrive, .drive = drive.id},
                            .enabled = drive.available,
                            .selected = drive.id == picked.drive});
  for (const Outpost::WeaponView& weapon : _newest.weapons)
    panel.weapons.push_back({.label = weapon.nameUtf8,
                             .action = {.kind = Hud::ActionKind::PickWeapon, .weapon = weapon.id},
                             .enabled = weapon.available,
                             .selected = weapon.id == picked.weapon});

  const std::optional<Outpost::DesignStats> stats = _designer.Stats(_newest);
  if (stats.has_value())
  {
    panel.summary.push_back(
      std::format("Hit points {}   Armor {}   Speed {} m/s", Outpost::WithThousands(WholePoints(stats->hitPointsHundredths)),
                  Tenths(static_cast<double>(stats->armorHundredths) / Outpost::HUNDREDTHS), Tenths(stats->movement.speedMetersPerSecond)));
    const std::string splash = stats->splashRadiusMeters > 0.0f ? std::format("   Splash {} m", Tenths(stats->splashRadiusMeters)) : "";
    panel.summary.push_back(std::format("Range {} m{}   Cost {}   Build {} s", Tenths(stats->rangeMeters), splash, stats->cost,
                                        Tenths(stats->buildSeconds / _newest.shipyardBuildSpeedFactor)));
    std::vector<std::string> header{"Damage/s after armor vs"};
    std::vector<std::string> perShip{"per ship"};
    std::vector<std::string> perOre{"per 100 Ore"};
    for (const Outpost::HullView& hull : _newest.hulls)
    {
      const double damage = Outpost::DamagePerSecond(*stats, hull.armorHundredths);
      header.push_back(hull.nameUtf8);
      perShip.push_back(std::format("{:.1f}", damage));
      perOre.push_back(std::format("{:.1f}", stats->cost > 0 ? damage * 100.0 / stats->cost : 0.0));
    }
    panel.table = {std::move(header), std::move(perShip), std::move(perOre)};
  }

  const Outpost::DesignView* match = _designer.Match(_newest);
  const bool canSave = _designer.SaveCommand(_newest).has_value();
  panel.actions.push_back(
    {.label = match != nullptr ? "Rename" : "Save design", .action = {.kind = Hud::ActionKind::SaveDesign}, .enabled = canSave});
  const bool canQueue = _yard.builtPermille >= Outpost::PERMILLE && _yard.queue.size() < Outpost::QUEUE_LIMIT;
  if (match != nullptr)
  {
    panel.actions.push_back({.label = std::format("Queue|{}", match->cost),
                             .action = {.kind = Hud::ActionKind::Queue, .producer = _yard.id, .design = match->id},
                             .enabled = canQueue && _newest.ore >= match->cost});
  }
  else
  {
    // Picks that are no saved design yet are saved by their Queue, and queued once the server has the design (ADR-023).
    panel.actions.push_back({.label = stats.has_value() ? std::format("Queue|{}", stats->cost) : std::string("Queue"),
                             .action = {.kind = Hud::ActionKind::SaveAndQueue, .producer = _yard.id},
                             .enabled = canSave && canQueue && stats.has_value() && _newest.ore >= stats->cost});
  }
  return panel;
}
// A button: its face, its label and any cost after a '|', and its place among the actions when it does something.
void AddButton(Hud::Layout& _layout, float _scale, const Hud::Rect& _area, const Hud::Button& _button)
{
  Hud::Rect face = _area;
  face.color = _button.selected ? SELECTED_BUTTON_COLOR : _button.enabled ? BUTTON_COLOR : DISABLED_BUTTON_COLOR;
  _layout.panels.push_back(face);
  if (_button.enabled)
    _layout.actions.emplace_back(face, _button.action);
  const size_t split = _button.label.find('|');
  const float labelTop = face.top + ((BUTTON_HEIGHT - LINE_STEP) / 2.0f * _scale);
  const DirectX::XMFLOAT4& color = _button.enabled ? TEXT_COLOR : DIM_TEXT_COLOR;
  _layout.texts.push_back({_button.label.substr(0, split), face.left + (PADDING * _scale), labelTop, color});
  if (split != std::string::npos)
  {
    const float costLeft = std::min(BUTTON_COST_LEFT * _scale, face.width - (COST_ROOM * _scale));
    _layout.texts.push_back(
      {_button.label.substr(split + 1), face.left + costLeft, labelTop, _button.enabled ? ORE_COLOR : DIM_TEXT_COLOR});
  }
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

std::string Outpost::MinutesAndSeconds(std::uint64_t _seconds)
{
  const std::uint64_t hours = _seconds / 3600;
  const std::uint64_t minutes = (_seconds / 60) % 60;
  const std::uint64_t seconds = _seconds % 60;
  return hours > 0 ? std::format("{}:{:02}:{:02}", hours, minutes, seconds) : std::format("{}:{:02}", minutes, seconds);
}

std::optional<Hud::Outcome> Hud::DescribeOutcome(const Snapshot& _newest, std::uint32_t _ticksPerSecond)
{
  if (!_newest.matchOver)
    return std::nullopt;
  const std::string_view title = !_newest.winner.IsValid() ? "Draw" : _newest.winner == _newest.player ? "Victory" : "Defeat";
  const std::uint64_t seconds = _ticksPerSecond > 0 ? _newest.matchEndedTick / _ticksPerSecond : 0;
  return Outcome{.title = std::string(title), .detail = std::format("Match length {}", MinutesAndSeconds(seconds))};
}

Hud::Layout Hud::LayMenu(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const float scale = Scale(_widthPixels, _heightPixels);
  Layout layout{.fontPixels = FONT_UNITS * scale, .panels = {}, .texts = {}, .actions = {}, .minimap = {}, .mapSizeMeters = 0.0f};
  const std::array<Button, 2> buttons{
    {{.label = "Start skirmish", .action = {.kind = ActionKind::StartSkirmish}}, {.label = "Quit", .action = {.kind = ActionKind::Quit}}}};
  const auto count = static_cast<float>(buttons.size());
  const float panelHeight = (2.0f * PADDING) + (2.0f * LINE_STEP) + BUTTON_GAP + (count * BUTTON_HEIGHT) + ((count - 1.0f) * BUTTON_GAP);
  const float left = (static_cast<float>(_widthPixels) / 2.0f) - (MENU_WIDTH / 2.0f * scale);
  const float top = (static_cast<float>(_heightPixels) / 2.0f) - (panelHeight / 2.0f * scale);
  layout.panels.push_back({left, top, MENU_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
  const float inner = left + (PADDING * scale);
  layout.texts.push_back({"Outpost Commander", inner, top + (PADDING * scale), ORE_COLOR});
  layout.texts.push_back({"A skirmish against the AI", inner, top + ((PADDING + LINE_STEP) * scale), DIM_TEXT_COLOR});
  float y = top + ((PADDING + (2.0f * LINE_STEP) + BUTTON_GAP) * scale);
  for (const Button& button : buttons)
  {
    AddButton(layout, scale, {inner, y, (MENU_WIDTH - (2.0f * PADDING)) * scale, BUTTON_HEIGHT * scale}, button);
    y += (BUTTON_HEIGHT + BUTTON_GAP) * scale;
  }
  return layout;
}

bool Hud::Layout::Covers(float _xPixels, float _yPixels) const noexcept
{
  return std::ranges::any_of(panels, [&](const Rect& _panel) { return _panel.Contains(_xPixels, _yPixels); });
}

std::optional<Hud::Action> Hud::Layout::ActionAt(float _xPixels, float _yPixels) const noexcept
{
  for (const auto& [area, action] : actions)
  {
    if (area.Contains(_xPixels, _yPixels))
      return action;
  }
  return std::nullopt;
}

std::optional<Outpost::PlanePosition> Hud::Layout::MapPointAt(float _xPixels, float _yPixels) const noexcept
{
  if (mapSizeMeters <= 0.0f || !minimap.Contains(_xPixels, _yPixels))
    return std::nullopt;
  // The map's +x runs right and its +z up, as the default camera shows it.
  const float half = mapSizeMeters / 2.0f;
  return PlanePosition{.xMeters = -half + ((_xPixels - minimap.left) / minimap.width * mapSizeMeters),
                       .zMeters = half - ((_yPixels - minimap.top) / minimap.height * mapSizeMeters)};
}

DirectX::XMFLOAT2 Hud::Layout::MinimapPixelOf(PlanePosition _point) const noexcept
{
  const float half = mapSizeMeters / 2.0f;
  const float x = std::clamp((_point.xMeters + half) / mapSizeMeters, 0.0f, 1.0f);
  const float y = std::clamp((half - _point.zMeters) / mapSizeMeters, 0.0f, 1.0f);
  return {minimap.left + (x * minimap.width), minimap.top + (y * minimap.height)};
}

Outpost::Hud::Content Outpost::Hud::Describe(const Snapshot& _newest, std::span<const EntityView> _entities,
                                             std::span<const EntityId> _selected, std::optional<StructureKind> _placing,
                                             const Designer* _designer)
{
  Content content{.ore = _newest.ore,
                  .oreIncomeHundredthsPerSecond = _newest.oreIncomeHundredthsPerSecond,
                  .selection = {},
                  .buttons = {},
                  .hint = {},
                  .research = ResearchLine(_newest, _entities),
                  .designer = std::nullopt,
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
      const auto topicOf = [&_newest](ResearchTopicId _topic) -> const ResearchTopicView*
      {
        const auto topic = std::ranges::find(_newest.research, _topic, &ResearchTopicView::id);
        return topic != _newest.research.end() ? &*topic : nullptr;
      };
      for (size_t i = 0; i < structure->research.size(); ++i)
      {
        const ResearchTopicView* topic = topicOf(structure->research[i]);
        const std::string name = topic != nullptr ? topic->nameUtf8 : std::string("Unknown topic");
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
        {
          content.buttons.push_back({.label = std::format("{}|{}", design.nameUtf8, design.cost),
                                     .action = {.kind = ActionKind::Queue, .producer = structure->id, .design = design.id},
                                     .enabled = canQueue && _newest.ore >= design.cost});
        }
        if (_designer != nullptr && structure->owner == _newest.player && structure->builtPermille >= PERMILLE)
          content.designer = DescribeDesigner(_newest, *_designer, *structure);
      }
      else if (structure->structure == StructureKind::ResearchLab && structure->owner == _newest.player)
      {
        // Each topic not researched or queued yet, and what it does; one whose prerequisites are neither is dim (design §8).
        const bool canResearch = structure->builtPermille >= PERMILLE && structure->research.size() < QUEUE_LIMIT;
        const auto known = [&](ResearchTopicId _topic)
        {
          const ResearchTopicView* topic = topicOf(_topic);
          return (topic != nullptr && topic->researched) || std::ranges::find(structure->research, _topic) != structure->research.end();
        };
        for (const ResearchTopicView& topic : _newest.research)
        {
          if (known(topic.id))
            continue;
          content.selection.push_back(std::format("{}: {}", topic.nameUtf8, topic.effectUtf8));
          content.buttons.push_back(
            {.label = std::format("{}|{}", topic.nameUtf8, topic.cost),
             .action = {.kind = ActionKind::Research, .producer = structure->id, .topic = topic.id},
             .enabled = canResearch && std::ranges::all_of(topic.prerequisites, known) && _newest.ore >= topic.cost});
        }
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
    content.selection.push_back(nameOf(byDesign.front().first));
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

float Hud::Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  return std::min(static_cast<float>(_widthPixels) / REFERENCE_WIDTH_UNITS, static_cast<float>(_heightPixels) / REFERENCE_HEIGHT_UNITS);
}

Hud::Layout Hud::Lay(const Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels, std::span<const PlanePosition> _view)
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

  // Under the Ore: the research under way.
  if (!_content.research.empty())
  {
    const float top = oreTop + ((ORE_PANEL_HEIGHT + RESEARCH_PANEL_GAP) * scale);
    layout.panels.push_back({oreLeft, top, RESEARCH_PANEL_WIDTH * scale, ORE_PANEL_HEIGHT * scale, PANEL_COLOR});
    layout.texts.push_back(
      {_content.research, oreLeft + (PADDING * scale), top + ((ORE_PANEL_HEIGHT - LINE_STEP) / 2.0f * scale), TEXT_COLOR});
  }

  const auto addButton = [&layout, scale](const Rect& _area, const Button& _button) { AddButton(layout, scale, _area, _button); };

  // Top-middle anchor: what a click on the ground will do.
  if (!_content.hint.empty())
  {
    const float left = (width / 2.0f) - (HINT_PANEL_WIDTH / 2.0f * scale);
    layout.panels.push_back({left, oreTop, HINT_PANEL_WIDTH * scale, ORE_PANEL_HEIGHT * scale, PANEL_COLOR});
    layout.texts.push_back({_content.hint, left + (PADDING * scale), textTop, TEXT_COLOR});
  }

  // Top-middle anchor, under the hint: how the match ended, and the way back to the menu.
  if (_content.outcome.has_value())
  {
    const float panelHeight = (2.0f * PADDING) + (2.0f * LINE_STEP) + BUTTON_GAP + BUTTON_HEIGHT;
    const float left = (width / 2.0f) - (BANNER_WIDTH / 2.0f * scale);
    const float top = oreTop + ((ORE_PANEL_HEIGHT + RESEARCH_PANEL_GAP) * scale);
    layout.panels.push_back({left, top, BANNER_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
    const float inner = left + (PADDING * scale);
    layout.texts.push_back({_content.outcome->title, inner, top + (PADDING * scale), ORE_COLOR});
    layout.texts.push_back({_content.outcome->detail, inner, top + ((PADDING + LINE_STEP) * scale), TEXT_COLOR});
    addButton({inner, top + ((PADDING + (2.0f * LINE_STEP) + BUTTON_GAP) * scale), (BANNER_WIDTH - (2.0f * PADDING)) * scale,
               BUTTON_HEIGHT * scale},
              {.label = "Back to menu", .action = {.kind = ActionKind::BackToMenu}});
  }

  // Bottom-middle anchor: the selection.
  if (!_content.selection.empty())
  {
    const float panelHeight = (2.0f * PADDING) + (LINE_STEP * static_cast<float>(_content.selection.size()));
    const float left = (width / 2.0f) - (SELECTION_PANEL_WIDTH / 2.0f * scale);
    const float top = height - ((MARGIN + panelHeight) * scale);
    layout.panels.push_back({left, top, SELECTION_PANEL_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
    for (size_t line = 0; line < _content.selection.size(); ++line)
    {
      layout.texts.push_back({_content.selection[line], left + (PADDING * scale),
                              top + ((PADDING + (LINE_STEP * static_cast<float>(line))) * scale), TEXT_COLOR});
    }
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
      addButton({left + (PADDING * scale), top + ((PADDING + (static_cast<float>(i) * (BUTTON_HEIGHT + BUTTON_GAP))) * scale),
                 (BUTTON_PANEL_WIDTH - (2.0f * PADDING)) * scale, BUTTON_HEIGHT * scale},
                _content.buttons[i]);
  }

  // Top-right anchor: the designer, a row at a time.
  if (_content.designer.has_value())
  {
    const DesignerPanel& designer = *_content.designer;
    const float rowStep = BUTTON_HEIGHT + BUTTON_GAP;
    const float rows = 5.0f + static_cast<float>(designer.table.size());
    const float panelHeight = (2.0f * PADDING) + (rows * rowStep) + (static_cast<float>(designer.summary.size()) * LINE_STEP);
    const float left = width - ((MARGIN + DESIGNER_WIDTH) * scale);
    const float top = MARGIN * scale;
    layout.panels.push_back({left, top, DESIGNER_WIDTH * scale, panelHeight * scale, PANEL_COLOR});
    const float inner = left + (PADDING * scale);
    const float fieldLeft = inner + (DESIGNER_LABEL_WIDTH * scale);
    float y = top + (PADDING * scale);
    const auto labelAt = [&](std::string _text, float _left, float _rowTop, const DirectX::XMFLOAT4& _color)
    { layout.texts.push_back({std::move(_text), _left, _rowTop + ((BUTTON_HEIGHT - LINE_STEP) / 2.0f * scale), _color}); };

    // The name, a field that takes typing once clicked.
    labelAt("Name", inner, y, TEXT_COLOR);
    const Rect field{fieldLeft, y, (DESIGNER_WIDTH - (2.0f * PADDING) - DESIGNER_LABEL_WIDTH) * scale, BUTTON_HEIGHT * scale,
                     designer.editing ? SELECTED_BUTTON_COLOR : BUTTON_COLOR};
    layout.panels.push_back(field);
    layout.actions.emplace_back(field, Action{.kind = ActionKind::EditName});
    labelAt(designer.editing ? designer.name + "_" : designer.name, field.left + (PADDING * scale), y,
            designer.nameValid ? TEXT_COLOR : WARNING_COLOR);
    y += rowStep * scale;

    const std::array<std::pair<std::string_view, const std::vector<Button>*>, 3> slots{
      {{"Hull", &designer.hulls}, {"Drive", &designer.drives}, {"Weapon", &designer.weapons}}};
    for (const auto& [label, picks] : slots)
    {
      labelAt(std::string(label), inner, y, TEXT_COLOR);
      for (size_t i = 0; i < picks->size(); ++i)
        addButton({fieldLeft + (static_cast<float>(i) * (PICK_WIDTH + BUTTON_GAP) * scale), y, PICK_WIDTH * scale, BUTTON_HEIGHT * scale},
                  (*picks)[i]);
      y += rowStep * scale;
    }

    for (const std::string& line : designer.summary)
    {
      layout.texts.push_back({line, inner, y, TEXT_COLOR});
      y += LINE_STEP * scale;
    }
    for (size_t row = 0; row < designer.table.size(); ++row)
    {
      const std::vector<std::string>& cells = designer.table[row];
      for (size_t column = 0; column < cells.size(); ++column)
      {
        const float cellLeft =
          column == 0 ? inner : inner + ((TABLE_LABEL_WIDTH + (static_cast<float>(column - 1) * TABLE_COLUMN_WIDTH)) * scale);
        labelAt(cells[column], cellLeft, y, row == 0 || column == 0 ? DIM_TEXT_COLOR : TEXT_COLOR);
      }
      y += rowStep * scale;
    }

    for (size_t i = 0; i < designer.actions.size(); ++i)
      addButton({inner + (static_cast<float>(i) * (ACTION_WIDTH + BUTTON_GAP) * scale), y, ACTION_WIDTH * scale, BUTTON_HEIGHT * scale},
                designer.actions[i]);
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
      const DirectX::XMFLOAT4& color = mark.side == Side::Own                   ? OWN_COLOR
                                       : mark.side == Side::Enemy               ? ENEMY_COLOR
                                       : mark.kind == EntityKind::AsteroidField ? FIELD_COLOR
                                                                                : NEUTRAL_COLOR;
      layout.panels.push_back({at.x - (side / 2.0f), at.y - (side / 2.0f), side, side, color});
    }

    // The fog over the marks, one rectangle for each run of cells of one shade along a row (ADR-024). Rows go up the
    // minimap as z grows.
    const size_t cells = _content.fogCellsPerSide;
    if (cells > 0 && _content.fogShades.size() == cells * cells)
    {
      const float cellPixels = inner / static_cast<float>(cells);
      for (size_t row = 0; row < cells; ++row)
      {
        const float rowTop = layout.minimap.top + (static_cast<float>(cells - row - 1) * cellPixels);
        const float* shades = &_content.fogShades[row * cells];
        size_t start = 0;
        for (size_t column = 1; column <= cells; ++column)
        {
          if (column < cells && shades[column] == shades[start])
            continue;
          if (shades[start] > 0.0f)
          {
            layout.panels.push_back({layout.minimap.left + (static_cast<float>(start) * cellPixels),
                                     rowTop,
                                     static_cast<float>(column - start) * cellPixels,
                                     cellPixels,
                                     {0.0f, 0.0f, 0.0f, shades[start]}});
          }
          start = column;
        }
      }
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
