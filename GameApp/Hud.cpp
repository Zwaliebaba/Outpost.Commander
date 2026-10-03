#include "pch.h"
#include "Hud.h"

#include <algorithm>
#include <array>
#include <cctype>
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
// An ore asteroid that has run out: the neutral gray, darkened and warmed toward rust.
constexpr DirectX::XMFLOAT4 DRY_COLOR{0.24f, 0.15f, 0.11f, 1.0f};
// An asteroid field is only in the way, so it is drawn darker than an ore asteroid, which is worth going to, though a
// field's square is the larger (ADR-040).
constexpr DirectX::XMFLOAT4 ASTEROID_FIELD_COLOR{0.13f, 0.13f, 0.14f, 1.0f};
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

// A design's name, or the Constructor's for no design.
std::string DesignNameOf(const Outpost::Snapshot& _newest, Outpost::DesignId _design)
{
  if (!_design.IsValid())
    return std::string(CONSTRUCTOR_NAME);
  const auto design = std::ranges::find(_newest.designs, _design, &Outpost::DesignView::id);
  return design != _newest.designs.end() ? design->nameUtf8 : std::string("Unknown design");
}

// A queue of _names as a window shows it, the front job with _permille done, or waiting for Ore while none is.
std::vector<Hud::QueueLine> QueueLinesOf(std::vector<std::string> _names, std::int32_t _permille)
{
  std::vector<Hud::QueueLine> lines;
  lines.reserve(_names.size());
  for (std::string& name : _names)
  {
    const bool front = lines.empty();
    lines.push_back({.name = std::move(name), .front = front, .permille = front ? _permille : 0, .waiting = front && _permille == 0});
  }
  return lines;
}

// The middle dot and the multiplication sign, in UTF-8 (ADR-030).
constexpr std::string_view DOT = " \xC2\xB7 ";
constexpr std::string_view TIMES = "\xC3\x97";
// How many saved designs' chips the designer shows at once.
constexpr std::size_t CHIPS_SHOWN = 3;
// A damage card's rating: Good from two thirds of the best any design does to the hull, Fair from a third.
constexpr float GOOD_SHARE = 2.0f / 3.0f;
constexpr float FAIR_SHARE = 1.0f / 3.0f;

std::string Capitals(std::string_view _text)
{
  std::string capitals(_text);
  for (char& character : capitals)
    character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  return capitals;
}

// The best each of a design's numbers reaches over every design the components make, locked ones included, so that the
// designer's bars keep their scale as research unlocks parts (Phase 1 design §11).
struct Best
{
  double hitPoints = 0.0;
  double armor = 0.0;
  double speed = 0.0;
  double range = 0.0;
  double cost = 0.0;
  double build = 0.0;
  // Damage per second after armor against each hull, in the snapshot's order.
  std::vector<double> damage;
};

Best BestOfAll(const Outpost::Snapshot& _newest)
{
  Best best;
  best.damage.assign(_newest.hulls.size(), 0.0);
  for (const Outpost::HullView& hull : _newest.hulls)
  {
    for (const Outpost::DriveView& drive : _newest.drives)
    {
      for (const Outpost::WeaponView& weapon : _newest.weapons)
      {
        const Outpost::DesignStats stats = Outpost::DesignStatsOf(hull, drive, weapon);
        best.hitPoints = std::max(best.hitPoints, static_cast<double>(stats.hitPointsHundredths));
        best.armor = std::max(best.armor, static_cast<double>(stats.armorHundredths));
        best.speed = std::max(best.speed, static_cast<double>(stats.movement.speedMetersPerSecond));
        best.range = std::max(best.range, static_cast<double>(stats.rangeMeters));
        best.cost = std::max(best.cost, static_cast<double>(stats.cost));
        best.build = std::max(best.build, stats.buildSeconds / _newest.shipyardBuildSpeedFactor);
        for (size_t i = 0; i < _newest.hulls.size(); ++i)
          best.damage[i] = std::max(best.damage[i], Outpost::DamagePerSecond(stats, _newest.hulls[i].armorHundredths));
      }
    }
  }
  return best;
}

float ShareOf(double _value, double _best) noexcept
{
  return _best > 0.0 ? static_cast<float>(std::clamp(_value / _best, 0.0, 1.0)) : 0.0f;
}

// How _preview compares to _current, where higher is better unless _lowerIsBetter.
Hud::Change ChangeOf(double _current, double _preview, bool _lowerIsBetter) noexcept
{
  if (_preview == _current)
    return Hud::Change::Same;
  return (_preview > _current) != _lowerIsBetter ? Hud::Change::Better : Hud::Change::Worse;
}

// The line a locked component's card names its research with: "RESEARCH · LARGE HULL", or "RESEARCH" alone when no topic
// names the component.
template <typename IdType>
std::string LockedBy(const Outpost::Snapshot& _newest, IdType _component, IdType Outpost::ResearchTopicView::*_unlocks)
{
  const auto topic = std::ranges::find(_newest.research, _component, _unlocks);
  return topic != _newest.research.end() ? std::format("RESEARCH{}{}", DOT, Capitals(topic->nameUtf8)) : std::string("RESEARCH");
}

// The designer's window (task 5.2, design §9; Phase 1 design §11): its target Shipyard, the name, the saved designs, a
// card for each component of each slot, the design's numbers against the best in the game, its damage against each hull,
// and saving, renaming and queuing. A hovered part previews the design it would make.
Outpost::Hud::DesignerPanel DescribeDesigner(const Outpost::Snapshot& _newest, const Outpost::Designer& _designer,
                                             std::optional<Hud::Action> _hovered)
{
  Hud::DesignerPanel panel;
  const Outpost::EntityView* target = _designer.Target(_newest);
  panel.hasShipyard = target != nullptr;
  panel.shipyard = target != nullptr ? std::format("SHIPYARD {:02}", target->shipyardNumber) : std::string("NO SHIPYARD");
  panel.queued = target != nullptr ? static_cast<std::uint32_t>(target->queue.size()) : 0;
  panel.built = target != nullptr ? target->shipsBuilt : 0;
  panel.ore = _newest.ore;
  panel.name = _designer.Name(_newest);
  panel.editing = _designer.IsEditing();
  panel.nameValid = Outpost::IsValidDesignName(panel.name);

  const Outpost::DesignView* match = _designer.Match(_newest);
  const std::optional<Outpost::SaveDesignCommand> save = _designer.SaveCommand(_newest);
  const bool savesNew = save.has_value() && !save->design.IsValid();
  panel.save = {.label = match != nullptr && !save.has_value() ? "SAVED" : "SAVE",
                .action = {.kind = Hud::ActionKind::SaveDesign},
                .enabled = savesNew,
                .selected = match != nullptr && !save.has_value()};
  panel.rename = {.label = "RENAME", .action = {.kind = Hud::ActionKind::SaveDesign}, .enabled = save.has_value() && !savesNew};

  panel.chips.reserve(_newest.designs.size());
  for (const Outpost::DesignView& design : _newest.designs)
  {
    const auto initials = []<typename View>(const std::vector<View>& _views, auto _id)
    {
      const auto view = std::ranges::find(_views, _id, &View::id);
      return view != _views.end() ? Outpost::Abbreviation(view->nameUtf8) : std::string("?");
    };
    panel.chips.push_back(
      {.name = design.nameUtf8,
       .code = std::format("{}{}{}{}{}", initials(_newest.hulls, design.hull), DOT.substr(1, 2), initials(_newest.drives, design.drive),
                           DOT.substr(1, 2), initials(_newest.weapons, design.weapon)),
       .shown = match != nullptr && match->id == design.id,
       .action = {.kind = Hud::ActionKind::LoadDesign, .design = design.id}});
  }
  panel.firstChip = std::min(_designer.FirstChip(), panel.chips.empty() ? 0 : panel.chips.size() - 1);

  // The design the hovered part would make, if a part is hovered.
  Outpost::DesignComponents preview = _designer.Picked();
  std::string previewing;
  if (_hovered.has_value())
  {
    if (_hovered->kind == Hud::ActionKind::PickHull && _hovered->hull != preview.hull)
      preview.hull = _hovered->hull;
    else if (_hovered->kind == Hud::ActionKind::PickDrive && _hovered->drive != preview.drive)
      preview.drive = _hovered->drive;
    else if (_hovered->kind == Hud::ActionKind::PickWeapon && _hovered->weapon != preview.weapon)
      preview.weapon = _hovered->weapon;
  }

  const Outpost::DesignComponents picked = _designer.Picked();
  Hud::SlotRow& hulls = panel.slots[0];
  hulls.label = "HULL";
  hulls.cards.reserve(_newest.hulls.size());
  for (const Outpost::HullView& hull : _newest.hulls)
  {
    if (hull.id == picked.hull)
      hulls.picked = hull.nameUtf8;
    if (hull.id == preview.hull && preview.hull != picked.hull)
      previewing = hull.nameUtf8;
    hulls.cards.push_back(
      {.name = hull.nameUtf8,
       .cost = hull.cost,
       .numbers =
         std::format("{} HP{}ARM {}{}{}m/s", Outpost::WithThousands(WholePoints(hull.hitPointsHundredths)), DOT,
                     Tenths(static_cast<double>(hull.armorHundredths) / Outpost::HUNDREDTHS), DOT, Tenths(hull.speedMetersPerSecond)),
       .picked = hull.id == picked.hull,
       .lockedBy = hull.available ? std::string() : LockedBy(_newest, hull.id, &Outpost::ResearchTopicView::unlocksHull),
       .action = {.kind = Hud::ActionKind::PickHull, .hull = hull.id}});
  }
  Hud::SlotRow& drives = panel.slots[1];
  drives.label = "DRIVE";
  drives.cards.reserve(_newest.drives.size());
  for (const Outpost::DriveView& drive : _newest.drives)
  {
    if (drive.id == picked.drive)
      drives.picked = drive.nameUtf8;
    if (drive.id == preview.drive && preview.drive != picked.drive)
      previewing = drive.nameUtf8;
    drives.cards.push_back(
      {.name = drive.nameUtf8,
       .cost = drive.cost,
       .numbers = std::format("SPD {}{}{}HP {}{}", TIMES, Tenths(drive.speedFactor), DOT, TIMES, Tenths(drive.hitPointsFactor)),
       .picked = drive.id == picked.drive,
       .lockedBy = drive.available ? std::string() : LockedBy(_newest, drive.id, &Outpost::ResearchTopicView::unlocksDrive),
       .action = {.kind = Hud::ActionKind::PickDrive, .drive = drive.id}});
  }
  Hud::SlotRow& weapons = panel.slots[2];
  weapons.label = "WEAPON";
  weapons.cards.reserve(_newest.weapons.size());
  for (const Outpost::WeaponView& weapon : _newest.weapons)
  {
    if (weapon.id == picked.weapon)
      weapons.picked = weapon.nameUtf8;
    if (weapon.id == preview.weapon && preview.weapon != picked.weapon)
      previewing = weapon.nameUtf8;
    weapons.cards.push_back(
      {.name = weapon.nameUtf8,
       .cost = weapon.cost,
       .numbers = std::format("{} dmg / {}s{}{}m", Tenths(static_cast<double>(weapon.damageHundredths) / Outpost::HUNDREDTHS),
                              Tenths(weapon.fireIntervalSeconds), DOT, Tenths(weapon.rangeMeters)),
       .note = weapon.splashRadiusMeters > 0.0 ? std::format("splash {}m", Tenths(weapon.splashRadiusMeters)) : std::string(),
       .picked = weapon.id == picked.weapon,
       .lockedBy = weapon.available ? std::string() : LockedBy(_newest, weapon.id, &Outpost::ResearchTopicView::unlocksWeapon),
       .action = {.kind = Hud::ActionKind::PickWeapon, .weapon = weapon.id}});
  }

  const std::optional<Outpost::DesignStats> stats = _designer.Stats(_newest);
  std::optional<Outpost::DesignStats> previewStats;
  if (!previewing.empty())
  {
    Outpost::Designer previewer;
    previewer.PickHull(preview.hull);
    previewer.PickDrive(preview.drive);
    previewer.PickWeapon(preview.weapon);
    previewStats = previewer.Stats(_newest);
  }
  panel.hint =
    previewStats.has_value() ? std::format("Preview: with {} instead", previewing) : std::string("Hover any part to preview its effect.");

  if (stats.has_value())
  {
    const Best best = BestOfAll(_newest);
    const double factor = _newest.shipyardBuildSpeedFactor;
    // Lower is better for cost and build time.
    const auto bar =
      [&](std::string_view _label, std::string_view _unit, double _best, const auto& _value, const auto& _text, bool _lowerIsBetter = false)
    {
      Hud::StatBar row{
        .label = std::string(_label), .value = _text(*stats), .unit = std::string(_unit), .share = ShareOf(_value(*stats), _best)};
      if (previewStats.has_value())
      {
        row.previewShare = ShareOf(_value(*previewStats), _best);
        row.previewValue = _text(*previewStats);
        row.change = ChangeOf(_value(*stats), _value(*previewStats), _lowerIsBetter);
      }
      panel.bars.push_back(std::move(row));
    };
    panel.bars.reserve(6);
    bar(
      "Hit points", "", best.hitPoints, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.hitPointsHundredths); },
      [](const Outpost::DesignStats& _s) { return Outpost::WithThousands(WholePoints(_s.hitPointsHundredths)); });
    bar(
      "Armor", "", best.armor, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.armorHundredths); },
      [](const Outpost::DesignStats& _s) { return Tenths(static_cast<double>(_s.armorHundredths) / Outpost::HUNDREDTHS); });
    bar(
      "Speed", "m/s", best.speed, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.movement.speedMetersPerSecond); },
      [](const Outpost::DesignStats& _s) { return Tenths(_s.movement.speedMetersPerSecond); });
    bar(
      "Range", "m", best.range, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.rangeMeters); },
      [](const Outpost::DesignStats& _s) { return Tenths(_s.rangeMeters); });
    bar(
      "Cost", "ore", best.cost, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.cost); },
      [](const Outpost::DesignStats& _s) { return std::to_string(_s.cost); }, true);
    bar(
      "Build", "s", best.build, [factor](const Outpost::DesignStats& _s) { return _s.buildSeconds / factor; },
      [factor](const Outpost::DesignStats& _s) { return Tenths(_s.buildSeconds / factor); }, true);

    const Outpost::DesignStats& shown = previewStats.has_value() ? *previewStats : *stats;
    panel.damage.reserve(_newest.hulls.size());
    for (size_t i = 0; i < _newest.hulls.size(); ++i)
    {
      const Outpost::HullView& hull = _newest.hulls[i];
      const double damage = Outpost::DamagePerSecond(shown, hull.armorHundredths);
      const Hud::Change change =
        previewStats.has_value() ? ChangeOf(Outpost::DamagePerSecond(*stats, hull.armorHundredths), damage, false) : Hud::Change::Same;
      const float share = ShareOf(damage, best.damage[i]);
      panel.damage.push_back({.hull = hull.nameUtf8,
                              .armor = std::format("ARM {}", Tenths(static_cast<double>(hull.armorHundredths) / Outpost::HUNDREDTHS)),
                              .perShip = std::format("{:.1f}", damage),
                              .perOre = std::format("{:.1f} / 100 ore", shown.cost > 0 ? damage * 100.0 / shown.cost : 0.0),
                              .share = share,
                              .rating = share >= GOOD_SHARE   ? Hud::Rating::Good
                                        : share >= FAIR_SHARE ? Hud::Rating::Fair
                                                              : Hud::Rating::Poor,
                              .change = change});
    }
  }

  // Queue asks for as many ships as the count, at the target Shipyard; picks that are no saved design yet are saved by
  // it first, and queued once the server has the design (ADR-023).
  const std::uint32_t free =
    target != nullptr ? static_cast<std::uint32_t>(Outpost::QUEUE_LIMIT - std::min(target->queue.size(), Outpost::QUEUE_LIMIT)) : 0;
  panel.count = _designer.Count(_newest);
  panel.canFewer = panel.count > 1;
  panel.canMore = panel.count < free;
  const std::int32_t cost = match != nullptr ? match->cost : stats.has_value() ? stats->cost : 0;
  panel.queueCost = cost * static_cast<std::int32_t>(panel.count);
  panel.queueDetail = stats.has_value() ? std::format("{} s each", Tenths(stats->buildSeconds / _newest.shipyardBuildSpeedFactor)) : "";
  const Outpost::EntityId producer = target != nullptr ? target->id : Outpost::EntityId{};
  if (match != nullptr)
    panel.queue = {.label = "QUEUE",
                   .action = {.kind = Hud::ActionKind::Queue, .producer = producer, .design = match->id, .count = panel.count},
                   .enabled = free > 0 && _newest.ore >= cost};
  else
    panel.queue = {.label = "QUEUE",
                   .action = {.kind = Hud::ActionKind::SaveAndQueue, .producer = producer, .count = panel.count},
                   .enabled = savesNew && free > 0 && stats.has_value() && _newest.ore >= cost};
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

// A floating window's look (ADR-031), after the owner's mockup (Phase 1 design §11): a dark navy body, a hatched title bar
// with the title in the title face, a close box at its right end, and a bracket at each corner.
constexpr DirectX::XMFLOAT4 WINDOW_COLOR{0.009f, 0.013f, 0.024f, 0.97f};
constexpr DirectX::XMFLOAT4 TITLE_HATCH_COLOR{0.03f, 0.042f, 0.08f, 1.0f};
constexpr DirectX::XMFLOAT4 CLOSE_BOX_COLOR{0.014f, 0.02f, 0.041f, 1.0f};
constexpr DirectX::XMFLOAT4 WINDOW_EDGE_COLOR{0.25f, 0.43f, 0.76f, 1.0f};
constexpr float CORNER_UNITS = 12.0f;
// Where the title's and the close box's characters sit in the title bar.
constexpr float TITLE_TEXT_INSET = 5.0f;
constexpr float CLOSE_TEXT_LEFT = 11.0f;
// The multiplication sign, in UTF-8.
constexpr std::string_view CLOSE_MARK = "\xC3\x97";

// Opens a window in the layout: its frame, title bar, close box and corners, its place in the layout's lists, and its
// title, unless the caller draws its own. The body below the title bar is _bodyHeightUnits tall; what goes in it is the caller's, and returns the body's top
// in pixels.
float OpenWindow(Hud::Layout& _layout, Outpost::WindowKind _kind, std::string _title, Outpost::WindowManager::Point _corner,
                 float _widthUnits, float _bodyHeightUnits, float _scale)
{
  const float left = _corner.xUnits * _scale;
  const float top = _corner.yUnits * _scale;
  const float width = _widthUnits * _scale;
  const float titleHeight = Hud::TITLE_BAR_UNITS * _scale;
  Hud::Window window{.kind = _kind,
                     .frame = {left, top, width, titleHeight + (_bodyHeightUnits * _scale), WINDOW_COLOR},
                     .titleBar = {left, top, width - titleHeight, titleHeight, TITLE_HATCH_COLOR, Hud::Fill::Hatched},
                     .closeBox = {left + width - titleHeight, top, titleHeight, titleHeight, CLOSE_BOX_COLOR},
                     .corner = _corner,
                     .firstPanel = _layout.panels.size(),
                     .firstText = _layout.texts.size(),
                     .firstSprite = _layout.sprites.size(),
                     .firstAction = _layout.actions.size()};
  _layout.panels.push_back(window.frame);
  _layout.panels.push_back(window.titleBar);
  _layout.panels.push_back(window.closeBox);
  if (!_title.empty())
    _layout.texts.push_back(
      {std::move(_title), left + (PADDING * _scale), top + (TITLE_TEXT_INSET * _scale), TEXT_COLOR, Hud::Typeface::Title});
  _layout.texts.push_back({std::string(CLOSE_MARK), window.closeBox.left + (CLOSE_TEXT_LEFT * _scale), top + (TITLE_TEXT_INSET * _scale),
                           TEXT_COLOR, Hud::Typeface::Title});
  const float corner = CORNER_UNITS * _scale;
  const Hud::Rect& frame = window.frame;
  for (const bool right : {false, true})
  {
    for (const bool bottom : {false, true})
    {
      _layout.sprites.push_back({.sprite = Hud::Sprite::Corner,
                                 .area = {right ? frame.left + frame.width - corner : frame.left,
                                          bottom ? frame.top + frame.height - corner : frame.top, corner, corner, WINDOW_EDGE_COLOR},
                                 .mirrorX = right,
                                 .mirrorY = bottom});
    }
  }
  _layout.windows.push_back(window);
  return top + titleHeight;
}

// The designer's window (Phase 1 design §11), laid out as the owner's mockup, GameDesign/Mockups/ShipDesigner.png, which is
// drawn at the reference scale: every place below is in reference units from the window's top-left corner.
constexpr float DESIGNER_WIDTH = 728.0f;
constexpr float DESIGNER_INSET = 24.0f;
// The hatched header runs on below the title bar, which the window is dragged by, to hold the title and the Shipyard.
constexpr float DESIGNER_HEADER = 72.0f;
constexpr float NAME_ROW_TOP = 84.0f;
constexpr float NAME_ROW_HEIGHT = 38.0f;
constexpr float CHIP_ROW_TOP = 132.0f;
constexpr float CHIP_HEIGHT = 26.0f;
constexpr float CHIPS_LEFT = 70.0f;
constexpr float CHIP_WIDTH = 190.0f;
// Room for a chip's name beside its abbreviation; a longer name is cut short.
constexpr std::size_t CHIP_NAME_CHARACTERS = 20;
constexpr float SLOTS_TOP = 168.0f;
constexpr float SLOT_GAP = 10.0f;
constexpr float SLOT_DIVIDER_LEFT = 108.0f;
constexpr float CARDS_LEFT = 118.0f;
constexpr float CARD_WIDTH = 190.0f;
constexpr float CARD_HEIGHT = 80.0f;
constexpr float CARD_GAP = 7.0f;
constexpr std::size_t CARDS_PER_LINE = 3;
constexpr float SECTION_GAP = 20.0f;
constexpr float SECTION_LABEL_HEIGHT = 26.0f;
constexpr float BAR_ROW_HEIGHT = 25.0f;
constexpr float BAR_LEFT = 104.0f;
constexpr float BAR_WIDTH = 116.0f;
constexpr float BAR_VALUE_RIGHT = 290.0f;
constexpr float DAMAGE_LEFT = 364.0f;
constexpr float DAMAGE_CARD_HEIGHT = 96.0f;
constexpr float DAMAGE_CARD_GAP = 8.0f;
constexpr float FOOTER_HEIGHT = 48.0f;
constexpr float SMALL_BUTTON_UNITS = 18.0f;
constexpr float LINE_UNITS = 1.0f;
// Labels in small spaced capitals.
constexpr float LABEL_TRACKING_UNITS = 2.0f;
// About how wide a character is set, for the few figures that stand against a right edge, since the layout is made
// without the fonts: the monospaced faces' advance is three fifths of their size, and Bahnschrift's figures and capitals
// a little narrower.
constexpr float MONO_ADVANCE = 0.6f;
constexpr float CONDENSED_ADVANCE = 0.52f;

// The mockup's colors (gate H6, confirmed at the owner's run), sampled from it and made linear, as the render target
// encodes them to sRGB.
constexpr DirectX::XMFLOAT4 LABEL_COLOR{0.22f, 0.29f, 0.42f, 1.0f};
constexpr DirectX::XMFLOAT4 ROW_LABEL_COLOR{0.41f, 0.49f, 0.61f, 1.0f};
constexpr DirectX::XMFLOAT4 NUMBERS_COLOR{0.65f, 0.77f, 0.93f, 1.0f};
constexpr DirectX::XMFLOAT4 SOFT_COLOR{0.29f, 0.37f, 0.5f, 1.0f};
constexpr DirectX::XMFLOAT4 FAINT_COLOR{0.12f, 0.15f, 0.21f, 1.0f};
constexpr DirectX::XMFLOAT4 CODE_COLOR{0.18f, 0.3f, 0.53f, 1.0f};
constexpr DirectX::XMFLOAT4 ACCENT_COLOR{0.4f, 0.63f, 1.0f, 1.0f};
constexpr DirectX::XMFLOAT4 GOLD_COLOR{0.93f, 0.5f, 0.045f, 1.0f};
constexpr DirectX::XMFLOAT4 FIELD_COLOR{0.005f, 0.007f, 0.011f, 1.0f};
constexpr DirectX::XMFLOAT4 CARD_COLOR{0.014f, 0.02f, 0.041f, 1.0f};
constexpr DirectX::XMFLOAT4 EDGE_COLOR{0.026f, 0.037f, 0.07f, 1.0f};
constexpr DirectX::XMFLOAT4 PICKED_COLOR{0.033f, 0.063f, 0.136f, 1.0f};
constexpr DirectX::XMFLOAT4 PICKED_EDGE_COLOR{0.35f, 0.56f, 0.9f, 1.0f};
constexpr DirectX::XMFLOAT4 LOCKED_COLOR{0.007f, 0.01f, 0.016f, 1.0f};
constexpr DirectX::XMFLOAT4 LOCKED_HATCH_COLOR{0.012f, 0.016f, 0.024f, 1.0f};
constexpr DirectX::XMFLOAT4 LOCKED_TEXT_COLOR{0.15f, 0.19f, 0.26f, 1.0f};
constexpr DirectX::XMFLOAT4 AMBER_COLOR{0.62f, 0.34f, 0.09f, 1.0f};
constexpr DirectX::XMFLOAT4 GOOD_COLOR{0.15f, 0.9f, 0.075f, 1.0f};
constexpr DirectX::XMFLOAT4 FAIR_COLOR{0.9f, 0.46f, 0.026f, 1.0f};
constexpr DirectX::XMFLOAT4 POOR_COLOR{0.75f, 0.08f, 0.044f, 1.0f};
constexpr DirectX::XMFLOAT4 BAR_TRACK_COLOR{0.004f, 0.005f, 0.009f, 1.0f};
constexpr DirectX::XMFLOAT4 BAR_FILL_COLOR{0.17f, 0.3f, 0.58f, 1.0f};
constexpr DirectX::XMFLOAT4 QUEUE_COLOR{0.037f, 0.125f, 0.048f, 1.0f};
constexpr DirectX::XMFLOAT4 QUEUE_HATCH_COLOR{0.053f, 0.153f, 0.067f, 1.0f};
constexpr DirectX::XMFLOAT4 QUEUE_EDGE_COLOR{0.09f, 0.29f, 0.12f, 1.0f};
constexpr DirectX::XMFLOAT4 QUEUE_TEXT_COLOR{0.85f, 1.0f, 0.86f, 1.0f};
constexpr DirectX::XMFLOAT4 SLOT_FILLED_COLOR{0.15f, 0.25f, 0.45f, 1.0f};

// How many characters _text sets, counting a character of several UTF-8 bytes once.
float CharactersOf(std::string_view _text) noexcept
{
  return static_cast<float>(std::ranges::count_if(_text, [](char _byte) { return (static_cast<unsigned char>(_byte) & 0xC0) != 0x80; }));
}

// _text cut to _characters, the last three of them dots, when it is longer.
std::string CutShort(const std::string& _text, std::size_t _characters)
{
  return _text.size() <= _characters ? _text : _text.substr(0, _characters - 3) + "...";
}

// The color of a hovered part's change: green when better, red when worse, and _same when neither.
DirectX::XMFLOAT4 ChangeColor(Hud::Change _change, const DirectX::XMFLOAT4& _same) noexcept
{
  return _change == Hud::Change::Better ? GOOD_COLOR : _change == Hud::Change::Worse ? POOR_COLOR : _same;
}

const DirectX::XMFLOAT4& RatingColor(Hud::Rating _rating) noexcept
{
  return _rating == Hud::Rating::Good ? GOOD_COLOR : _rating == Hud::Rating::Fair ? FAIR_COLOR : POOR_COLOR;
}

// How far the designer's sections reach, from its window's top, in reference units.
struct DesignerExtent
{
  // Where each slot's row starts, and where the rows end.
  std::array<float, 3> slotTops{};
  float slotsEnd = 0.0f;
  float sectionsTop = 0.0f;
  float footerTop = 0.0f;
  float height = 0.0f;
};

float CardLinesOf(const Hud::SlotRow& _slot) noexcept
{
  return static_cast<float>(std::max<std::size_t>(1, (_slot.cards.size() + CARDS_PER_LINE - 1) / CARDS_PER_LINE));
}

DesignerExtent ExtentOf(const Hud::DesignerPanel& _panel) noexcept
{
  DesignerExtent extent;
  float y = SLOTS_TOP;
  for (std::size_t slot = 0; slot < _panel.slots.size(); ++slot)
  {
    extent.slotTops[slot] = y;
    const float lines = CardLinesOf(_panel.slots[slot]);
    y += (lines * CARD_HEIGHT) + ((lines - 1.0f) * CARD_GAP) + SLOT_GAP;
  }
  extent.slotsEnd = y - SLOT_GAP;
  extent.sectionsTop = extent.slotsEnd + SECTION_GAP;
  const float bars = SECTION_LABEL_HEIGHT + (6.0f * BAR_ROW_HEIGHT);
  const float damage = SECTION_LABEL_HEIGHT + DAMAGE_CARD_HEIGHT + (2.0f * PADDING);
  extent.footerTop = extent.sectionsTop + std::max(bars, damage) + PADDING;
  extent.height = extent.footerTop + FOOTER_HEIGHT + SECTION_GAP;
  return extent;
}

// Lays the designer's window out with its top-left corner at _corner.
// Draws into a window of the layout in reference units from the window's top-left corner, as the windows' layouts place
// everything (ADR-031).
class Painter
{
public:
  Painter(Hud::Layout& _layout, Outpost::WindowManager::Point _corner, float _scale) noexcept
    : m_layout(_layout),
      m_originX(_corner.xUnits * _scale),
      m_originY(_corner.yUnits * _scale),
      m_scale(_scale)
  {
  }

  // Spaced capitals' tracking, in pixels.
  [[nodiscard]] float Tracking() const noexcept
  {
    return LABEL_TRACKING_UNITS * m_scale;
  }

  [[nodiscard]] Hud::Rect Area(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color,
                               Hud::Fill _fill = Hud::Fill::Solid) const noexcept
  {
    return {m_originX + (_left * m_scale), m_originY + (_top * m_scale), _width * m_scale, _height * m_scale, _color, _fill};
  }

  Hud::Rect Panel(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color, Hud::Fill _fill = Hud::Fill::Solid)
  {
    return m_layout.panels.emplace_back(Area(_left, _top, _width, _height, _color, _fill));
  }

  void Text(std::string _text, float _left, float _top, const DirectX::XMFLOAT4& _color, Hud::Typeface _face, float _tracking = 0.0f)
  {
    m_layout.texts.push_back({std::move(_text), m_originX + (_left * m_scale), m_originY + (_top * m_scale), _color, _face, _tracking});
  }

  // A figure that ends at _right, at about _advance units a character.
  void RightText(std::string _text, float _right, float _top, const DirectX::XMFLOAT4& _color, Hud::Typeface _face, float _advance)
  {
    const float left = _right - (CharactersOf(_text) * _advance);
    Text(std::move(_text), left, _top, _color, _face);
  }

  void Sprite(Hud::Sprite _sprite, float _left, float _top, float _size, const DirectX::XMFLOAT4& _color)
  {
    m_layout.sprites.push_back({.sprite = _sprite, .area = Area(_left, _top, _size, _size, _color)});
  }

  // The edge round a lit rectangle.
  void Outline(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color)
  {
    Panel(_left, _top, _width, LINE_UNITS, _color);
    Panel(_left, _top + _height - LINE_UNITS, _width, LINE_UNITS, _color);
    Panel(_left, _top, LINE_UNITS, _height, _color);
    Panel(_left + _width - LINE_UNITS, _top, LINE_UNITS, _height, _color);
  }

  void Press(const Hud::Rect& _rect, const Hud::Action& _action)
  {
    m_layout.actions.emplace_back(_rect, _action);
  }

  // A small square button with a mark on it, such as an arrow.
  void SmallButton(std::string _mark, float _left, float _top, bool _enabled, const Hud::Action& _action)
  {
    const Hud::Rect face = Panel(_left, _top, SMALL_BUTTON_UNITS, SMALL_BUTTON_UNITS, _enabled ? CARD_COLOR : DISABLED_BUTTON_COLOR);
    if (_enabled)
      Press(face, _action);
    Text(std::move(_mark), _left + 5.0f, _top + 1.0f, _enabled ? TEXT_COLOR : DIM_TEXT_COLOR, Hud::Typeface::Label);
  }

  // An amount of Ore that ends at _right: Ore's diamond, then the figure, in _face at _sizeUnits.
  void DiamondAndFigure(std::int32_t _ore, float _right, float _top, Hud::Typeface _face, float _sizeUnits, const DirectX::XMFLOAT4& _color)
  {
    const std::string figure = std::to_string(_ore);
    const float width = CharactersOf(figure) * _sizeUnits * (_face == Hud::Typeface::Title ? CONDENSED_ADVANCE : MONO_ADVANCE);
    const float mark = std::round(_sizeUnits * 0.6f);
    Sprite(Hud::Sprite::OreMark, _right - width - mark - 5.0f, _top + ((_sizeUnits * 1.25f) - mark) / 2.0f, mark, _color);
    Text(figure, _right - width, _top, _color, _face);
  }

  // The hatched band under the title bar that a window's header runs on into, to _bottomUnits, and its edge.
  void HeaderBand(float _widthUnits, float _bottomUnits)
  {
    Panel(0.0f, Hud::TITLE_BAR_UNITS, _widthUnits, _bottomUnits - Hud::TITLE_BAR_UNITS, TITLE_HATCH_COLOR, Hud::Fill::Hatched);
    Panel(0.0f, _bottomUnits - LINE_UNITS, _widthUnits, LINE_UNITS, EDGE_COLOR);
  }

  // The player's Ore in a box whose right edge is at _right, as the mockup's header holds it.
  void OreBox(std::int32_t _ore, float _right)
  {
    constexpr float WIDTH = 116.0f;
    Panel(_right - WIDTH, 38.0f, WIDTH, 30.0f, FIELD_COLOR);
    Outline(_right - WIDTH, 38.0f, WIDTH, 30.0f, EDGE_COLOR);
    DiamondAndFigure(_ore, _right - 10.0f, 39.0f, Hud::Typeface::Title, 22.0f, GOLD_COLOR);
  }

private:
  Hud::Layout& m_layout;
  float m_originX;
  float m_originY;
  float m_scale;
};

void LayDesigner(Hud::Layout& _layout, const Hud::DesignerPanel& _panel, Outpost::WindowManager::Point _corner, float _scale)
{
  const DesignerExtent extent = ExtentOf(_panel);
  (void)OpenWindow(_layout, Outpost::WindowKind::Designer, std::string(), _corner, DESIGNER_WIDTH, extent.height - Hud::TITLE_BAR_UNITS,
                   _scale);
  Painter paint(_layout, _corner, _scale);
  const float tracking = paint.Tracking();

  // The header: the target Shipyard with its arrows, the title, the Shipyard's queue and ships built, and the Ore.
  paint.HeaderBand(DESIGNER_WIDTH, DESIGNER_HEADER);
  paint.SmallButton("<", DESIGNER_INSET, 9.0f, _panel.hasShipyard, {.kind = Hud::ActionKind::PreviousShipyard});
  paint.SmallButton(">", DESIGNER_INSET + SMALL_BUTTON_UNITS + 4.0f, 9.0f, _panel.hasShipyard, {.kind = Hud::ActionKind::NextShipyard});
  paint.Text(std::format("{}{}DESIGNER", _panel.shipyard, DOT), DESIGNER_INSET + (2.0f * SMALL_BUTTON_UNITS) + 12.0f, 11.0f, LABEL_COLOR,
             Hud::Typeface::Label, tracking);
  paint.Text("SHIP DESIGN", DESIGNER_INSET, 40.0f, TEXT_COLOR, Hud::Typeface::Title);
  constexpr float SLOTS_LEFT = 412.0f;
  constexpr float SLOT_SIZE = 30.0f;
  paint.Text(std::format("QUEUE{}{} BUILT", DOT, _panel.built), SLOTS_LEFT, 12.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
  for (std::size_t slot = 0; slot < Outpost::QUEUE_LIMIT; ++slot)
  {
    paint.Panel(SLOTS_LEFT + (static_cast<float>(slot) * (SLOT_SIZE + 3.0f)), 40.0f, SLOT_SIZE, 24.0f,
                slot < _panel.queued ? SLOT_FILLED_COLOR : FIELD_COLOR);
  }
  paint.OreBox(_panel.ore, DESIGNER_WIDTH - DESIGNER_INSET);

  // The name, its count against the limit, and Save.
  const float fieldWidth = 604.0f;
  const Hud::Rect field = paint.Panel(DESIGNER_INSET, NAME_ROW_TOP, fieldWidth, NAME_ROW_HEIGHT, FIELD_COLOR);
  paint.Press(field, {.kind = Hud::ActionKind::EditName});
  paint.Outline(DESIGNER_INSET, NAME_ROW_TOP, fieldWidth, NAME_ROW_HEIGHT, _panel.editing ? PICKED_EDGE_COLOR : EDGE_COLOR);
  paint.Text("NAME", DESIGNER_INSET + 12.0f, NAME_ROW_TOP + 13.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
  paint.Text(_panel.editing ? _panel.name + "_" : _panel.name, DESIGNER_INSET + 66.0f, NAME_ROW_TOP + 9.0f,
             _panel.nameValid ? TEXT_COLOR : WARNING_COLOR, Hud::Typeface::Name);
  paint.RightText(std::format("{}/{}", _panel.name.size(), Outpost::DESIGN_NAME_LIMIT), DESIGNER_INSET + fieldWidth - 12.0f,
                  NAME_ROW_TOP + 12.0f, FAINT_COLOR, Hud::Typeface::Figure, 13.0f * MONO_ADVANCE);
  {
    constexpr float SAVE_LEFT = 636.0f;
    constexpr float SAVE_WIDTH = 68.0f;
    const Hud::Button& save = _panel.save;
    const Hud::Rect face = paint.Panel(SAVE_LEFT, NAME_ROW_TOP, SAVE_WIDTH, NAME_ROW_HEIGHT, save.enabled ? CARD_COLOR : FIELD_COLOR);
    paint.Outline(SAVE_LEFT, NAME_ROW_TOP, SAVE_WIDTH, NAME_ROW_HEIGHT, save.selected ? QUEUE_EDGE_COLOR : EDGE_COLOR);
    if (save.enabled)
      paint.Press(face, save.action);
    paint.Text(save.label, SAVE_LEFT + 12.0f, NAME_ROW_TOP + 13.0f,
               save.selected  ? QUEUE_EDGE_COLOR
               : save.enabled ? TEXT_COLOR
                              : DIM_TEXT_COLOR,
               Hud::Typeface::Label, tracking);
  }

  // The saved designs, as many as fit from the first shown, and the arrows that scroll them.
  paint.Text("SAVED", DESIGNER_INSET, CHIP_ROW_TOP + 7.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
  for (std::size_t i = _panel.firstChip, column = 0; i < _panel.chips.size() && column < CHIPS_SHOWN; ++i, ++column)
  {
    const Hud::DesignChip& chip = _panel.chips[i];
    const float left = CHIPS_LEFT + (static_cast<float>(column) * (CHIP_WIDTH + 6.0f));
    paint.Press(paint.Panel(left, CHIP_ROW_TOP, CHIP_WIDTH, CHIP_HEIGHT, chip.shown ? PICKED_COLOR : CARD_COLOR), chip.action);
    paint.Outline(left, CHIP_ROW_TOP, CHIP_WIDTH, CHIP_HEIGHT, chip.shown ? PICKED_EDGE_COLOR : EDGE_COLOR);
    paint.Text(CutShort(chip.name, CHIP_NAME_CHARACTERS), left + 8.0f, CHIP_ROW_TOP + 6.0f, TEXT_COLOR, Hud::Typeface::Label);
    paint.RightText(chip.code, left + CHIP_WIDTH - 8.0f, CHIP_ROW_TOP + 7.0f, CODE_COLOR, Hud::Typeface::Detail, 11.0f * MONO_ADVANCE);
  }
  if (_panel.chips.size() > CHIPS_SHOWN)
  {
    const float arrowsLeft = DESIGNER_WIDTH - DESIGNER_INSET - (2.0f * SMALL_BUTTON_UNITS) - 4.0f;
    paint.SmallButton("<", arrowsLeft, CHIP_ROW_TOP + 4.0f, _panel.firstChip > 0, {.kind = Hud::ActionKind::PreviousDesigns});
    paint.SmallButton(">", arrowsLeft + SMALL_BUTTON_UNITS + 4.0f, CHIP_ROW_TOP + 4.0f,
                      _panel.firstChip + CHIPS_SHOWN < _panel.chips.size(), {.kind = Hud::ActionKind::NextDesigns});
  }

  // A row for each slot: its label and pick, then its cards, three to a line.
  for (std::size_t slot = 0; slot < _panel.slots.size(); ++slot)
  {
    const Hud::SlotRow& row = _panel.slots[slot];
    const float top = extent.slotTops[slot];
    const float lines = CardLinesOf(row);
    paint.Text(row.label, DESIGNER_INSET, top + 22.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
    paint.Text(row.picked, DESIGNER_INSET, top + 38.0f, ACCENT_COLOR, Hud::Typeface::Name);
    paint.Panel(SLOT_DIVIDER_LEFT, top + 6.0f, LINE_UNITS, (lines * CARD_HEIGHT) + ((lines - 1.0f) * CARD_GAP) - 12.0f, EDGE_COLOR);
    for (std::size_t i = 0; i < row.cards.size(); ++i)
    {
      const Hud::PartCard& card = row.cards[i];
      const std::size_t line = i / CARDS_PER_LINE;
      const std::size_t column = i % CARDS_PER_LINE;
      const float left = CARDS_LEFT + (static_cast<float>(column) * (CARD_WIDTH + CARD_GAP));
      const float cardTop = top + (static_cast<float>(line) * (CARD_HEIGHT + CARD_GAP));
      const bool locked = card.IsLocked();
      const Hud::Rect face = paint.Panel(left, cardTop, CARD_WIDTH, CARD_HEIGHT,
                                         card.picked ? PICKED_COLOR
                                         : locked    ? LOCKED_COLOR
                                                     : CARD_COLOR);
      if (locked)
        paint.Panel(left, cardTop, CARD_WIDTH, CARD_HEIGHT, LOCKED_HATCH_COLOR, Hud::Fill::Hatched);
      else
        paint.Press(face, card.action);
      paint.Outline(left, cardTop, CARD_WIDTH, CARD_HEIGHT, card.picked ? PICKED_EDGE_COLOR : EDGE_COLOR);
      const DirectX::XMFLOAT4& nameColor = locked ? LOCKED_TEXT_COLOR : TEXT_COLOR;
      paint.Text(card.name, left + 12.0f, cardTop + 8.0f, nameColor, Hud::Typeface::Name);
      paint.DiamondAndFigure(card.cost, left + CARD_WIDTH - 12.0f, cardTop + 11.0f, Hud::Typeface::Figure, 13.0f,
                             locked ? LOCKED_TEXT_COLOR : GOLD_COLOR);
      paint.Text(card.numbers, left + 12.0f, cardTop + 34.0f, locked ? LOCKED_TEXT_COLOR : NUMBERS_COLOR, Hud::Typeface::Detail);
      if (!card.note.empty())
        paint.Text(card.note, left + 12.0f, cardTop + 48.0f, locked ? LOCKED_TEXT_COLOR : NUMBERS_COLOR, Hud::Typeface::Detail);
      if (locked)
      {
        paint.Sprite(Hud::Sprite::Checkbox, left + 12.0f, cardTop + 63.0f, 10.0f, AMBER_COLOR);
        paint.Text(card.lockedBy, left + 28.0f, cardTop + 61.0f, AMBER_COLOR, Hud::Typeface::Label);
      }
    }
  }
  paint.Panel(DESIGNER_INSET, extent.slotsEnd + (SECTION_GAP / 2.0f), DESIGNER_WIDTH - (2.0f * DESIGNER_INSET), LINE_UNITS, EDGE_COLOR);

  // Performance: each number's bar against the best in the game; while a part is hovered, the change it would make.
  const float sections = extent.sectionsTop;
  paint.Text("PERFORMANCE", DESIGNER_INSET, sections, LABEL_COLOR, Hud::Typeface::Label, tracking);
  for (std::size_t i = 0; i < _panel.bars.size(); ++i)
  {
    const Hud::StatBar& bar = _panel.bars[i];
    const float top = sections + SECTION_LABEL_HEIGHT + (static_cast<float>(i) * BAR_ROW_HEIGHT);
    paint.Text(bar.label, DESIGNER_INSET, top + 2.0f, ROW_LABEL_COLOR, Hud::Typeface::Label);
    paint.Panel(BAR_LEFT, top + 6.0f, BAR_WIDTH, 7.0f, BAR_TRACK_COLOR);
    if (bar.previewShare.has_value() && bar.change != Hud::Change::Same)
    {
      // The part of the bar the hovered part adds or takes away, in the change's color, under what both share.
      paint.Panel(BAR_LEFT, top + 6.0f, BAR_WIDTH * std::max(bar.share, *bar.previewShare), 7.0f, ChangeColor(bar.change, BAR_FILL_COLOR));
      paint.Panel(BAR_LEFT, top + 6.0f, BAR_WIDTH * std::min(bar.share, *bar.previewShare), 7.0f, BAR_FILL_COLOR);
    }
    else
      paint.Panel(BAR_LEFT, top + 6.0f, BAR_WIDTH * bar.share, 7.0f, BAR_FILL_COLOR);
    const bool previewing = bar.previewShare.has_value();
    paint.RightText(previewing ? bar.previewValue : bar.value, BAR_VALUE_RIGHT, top, ChangeColor(bar.change, TEXT_COLOR),
                    Hud::Typeface::Figure, 13.0f * MONO_ADVANCE);
    paint.Text(bar.unit, BAR_VALUE_RIGHT + 6.0f, top + 2.0f, LABEL_COLOR, Hud::Typeface::Label);
  }

  // Damage per second after armor against each hull, a card for each.
  const float damageWidth = DESIGNER_WIDTH - DESIGNER_INSET - DAMAGE_LEFT;
  paint.Text(std::format("DAMAGE / S AFTER ARMOR{}VS HULL", DOT), DAMAGE_LEFT, sections, LABEL_COLOR, Hud::Typeface::Label, tracking);
  const auto cards = static_cast<float>(std::max<std::size_t>(1, _panel.damage.size()));
  const float cardWidth = (damageWidth - ((cards - 1.0f) * DAMAGE_CARD_GAP)) / cards;
  const float cardsTop = sections + SECTION_LABEL_HEIGHT;
  for (std::size_t i = 0; i < _panel.damage.size(); ++i)
  {
    const Hud::DamageCard& card = _panel.damage[i];
    const float left = DAMAGE_LEFT + (static_cast<float>(i) * (cardWidth + DAMAGE_CARD_GAP));
    paint.Panel(left, cardsTop, cardWidth, DAMAGE_CARD_HEIGHT, FIELD_COLOR);
    paint.Outline(left, cardsTop, cardWidth, DAMAGE_CARD_HEIGHT, EDGE_COLOR);
    paint.Text(card.hull, left + 10.0f, cardsTop + 8.0f, TEXT_COLOR, Hud::Typeface::Name);
    paint.RightText(card.armor, left + cardWidth - 8.0f, cardsTop + 11.0f, LABEL_COLOR, Hud::Typeface::Label, 12.0f * CONDENSED_ADVANCE);
    paint.Text(card.perShip, left + 10.0f, cardsTop + 28.0f, ChangeColor(card.change, TEXT_COLOR), Hud::Typeface::LargeFigure);
    paint.Panel(left + 10.0f, cardsTop + 66.0f, cardWidth - 20.0f, 4.0f, BAR_TRACK_COLOR);
    paint.Panel(left + 10.0f, cardsTop + 66.0f, (cardWidth - 20.0f) * card.share, 4.0f, RatingColor(card.rating));
    paint.Text(card.perOre, left + 10.0f, cardsTop + 76.0f, SOFT_COLOR, Hud::Typeface::Detail);
  }
  paint.Text(_panel.hint, DAMAGE_LEFT, cardsTop + DAMAGE_CARD_HEIGHT + 10.0f, LABEL_COLOR, Hud::Typeface::Detail);

  // The bottom row: Rename, the stepper, and Queue with the build time of one ship and the cost of all.
  const float footer = extent.footerTop;
  {
    const Hud::Button& rename = _panel.rename;
    const Hud::Rect face = paint.Panel(DESIGNER_INSET, footer, 90.0f, FOOTER_HEIGHT, rename.enabled ? CARD_COLOR : FIELD_COLOR);
    paint.Outline(DESIGNER_INSET, footer, 90.0f, FOOTER_HEIGHT, EDGE_COLOR);
    if (rename.enabled)
      paint.Press(face, rename.action);
    paint.Text(rename.label, DESIGNER_INSET + 14.0f, footer + 16.0f, rename.enabled ? TEXT_COLOR : DIM_TEXT_COLOR, Hud::Typeface::Label,
               tracking);
  }
  constexpr float STEPPER_LEFT = 346.0f;
  constexpr float STEP_WIDTH = 36.0f;
  constexpr float COUNT_WIDTH = 42.0f;
  const auto step = [&](std::string _mark, float _left, bool _enabled, Hud::ActionKind _kind)
  {
    const Hud::Rect face = paint.Panel(_left, footer, STEP_WIDTH, FOOTER_HEIGHT, _enabled ? CARD_COLOR : FIELD_COLOR);
    paint.Outline(_left, footer, STEP_WIDTH, FOOTER_HEIGHT, EDGE_COLOR);
    if (_enabled)
      paint.Press(face, {.kind = _kind});
    paint.Text(std::move(_mark), _left + 13.0f, footer + 13.0f, _enabled ? TEXT_COLOR : DIM_TEXT_COLOR, Hud::Typeface::Name);
  };
  step("-", STEPPER_LEFT, _panel.canFewer, Hud::ActionKind::FewerShips);
  paint.Panel(STEPPER_LEFT + STEP_WIDTH, footer, COUNT_WIDTH, FOOTER_HEIGHT, FIELD_COLOR);
  paint.Outline(STEPPER_LEFT + STEP_WIDTH, footer, COUNT_WIDTH, FOOTER_HEIGHT, EDGE_COLOR);
  paint.Text(std::format("{}{}", TIMES, _panel.count), STEPPER_LEFT + STEP_WIDTH + 10.0f, footer + 13.0f, TEXT_COLOR, Hud::Typeface::Name);
  step("+", STEPPER_LEFT + STEP_WIDTH + COUNT_WIDTH, _panel.canMore, Hud::ActionKind::MoreShips);
  {
    constexpr float QUEUE_LEFT = 468.0f;
    const float queueWidth = DESIGNER_WIDTH - DESIGNER_INSET - QUEUE_LEFT;
    const Hud::Button& queue = _panel.queue;
    const Hud::Rect face = paint.Panel(QUEUE_LEFT, footer, queueWidth, FOOTER_HEIGHT, queue.enabled ? QUEUE_COLOR : FIELD_COLOR);
    if (queue.enabled)
    {
      paint.Panel(QUEUE_LEFT, footer, queueWidth, FOOTER_HEIGHT, QUEUE_HATCH_COLOR, Hud::Fill::Hatched);
      paint.Press(face, queue.action);
    }
    paint.Outline(QUEUE_LEFT, footer, queueWidth, FOOTER_HEIGHT, queue.enabled ? QUEUE_EDGE_COLOR : EDGE_COLOR);
    const DirectX::XMFLOAT4& color = queue.enabled ? QUEUE_TEXT_COLOR : DIM_TEXT_COLOR;
    paint.Text(queue.label, QUEUE_LEFT + 18.0f, footer + 3.0f, color, Hud::Typeface::Title, tracking);
    paint.Text(_panel.queueDetail, QUEUE_LEFT + 18.0f, footer + 30.0f, color, Hud::Typeface::Detail);
    paint.DiamondAndFigure(_panel.queueCost, QUEUE_LEFT + queueWidth - 16.0f, footer + 10.0f, Hud::Typeface::Title, 22.0f,
                           queue.enabled ? GOLD_COLOR : DIM_TEXT_COLOR);
  }
}

// The production and research windows (Phase 1 design §12), in the designer's look, in reference units from the window's
// top-left corner: a hatched header with the window's subject and the Ore, the queue as five rows, and the cards that add
// to it.
constexpr float PRODUCTION_WIDTH = 480.0f;
constexpr float RESEARCH_WIDTH = 560.0f;
constexpr float WINDOW_INSET = 24.0f;
constexpr float QUEUE_LABEL_TOP = 84.0f;
constexpr float QUEUE_ROWS_TOP = 104.0f;
constexpr float QUEUE_ROW_HEIGHT = 30.0f;
constexpr float QUEUE_ROW_GAP = 4.0f;
constexpr float QUEUE_BAR_WIDTH = 120.0f;
constexpr float OPTION_HEIGHT = 48.0f;
constexpr float TOPIC_HEIGHT = 64.0f;
constexpr float CARD_SPACING = 8.0f;
// Where the production and research windows stand at first: under the Ore panel and the research line.
constexpr float WINDOWS_TOP = 128.0f;

// Where the cards under the queue start: under its five rows and a label.
constexpr float CardsTop() noexcept
{
  return QUEUE_ROWS_TOP + (static_cast<float>(Outpost::QUEUE_LIMIT) * (QUEUE_ROW_HEIGHT + QUEUE_ROW_GAP)) + 12.0f + SECTION_LABEL_HEIGHT;
}

// The height of _lines lines of cards _cardHeight tall, at least one, and the window's margin under them.
float CardsHeight(std::size_t _lines, float _cardHeight) noexcept
{
  const auto lines = static_cast<float>(std::max<std::size_t>(_lines, 1));
  return (lines * (_cardHeight + CARD_SPACING)) - CARD_SPACING + SECTION_GAP;
}

// A window's header: its subject in the title face, and the Ore.
void Header(Painter& _paint, std::string _subject, std::int32_t _ore, float _widthUnits)
{
  _paint.HeaderBand(_widthUnits, DESIGNER_HEADER);
  _paint.Text(std::move(_subject), WINDOW_INSET, 40.0f, TEXT_COLOR, Hud::Typeface::Title);
  _paint.OreBox(_ore, _widthUnits - WINDOW_INSET);
}

// A queue as five rows under its label: each job's name, the front one's progress or its wait for Ore, and the free slots
// empty.
void QueueRows(Painter& _paint, const std::vector<Hud::QueueLine>& _queue, float _widthUnits)
{
  _paint.Text(std::format("QUEUE{}{} / {}", DOT, _queue.size(), Outpost::QUEUE_LIMIT), WINDOW_INSET, QUEUE_LABEL_TOP, LABEL_COLOR,
              Hud::Typeface::Label, _paint.Tracking());
  const float width = _widthUnits - (2.0f * WINDOW_INSET);
  for (std::size_t slot = 0; slot < Outpost::QUEUE_LIMIT; ++slot)
  {
    const float top = QUEUE_ROWS_TOP + (static_cast<float>(slot) * (QUEUE_ROW_HEIGHT + QUEUE_ROW_GAP));
    const bool filled = slot < _queue.size();
    _paint.Panel(WINDOW_INSET, top, width, QUEUE_ROW_HEIGHT, filled ? CARD_COLOR : FIELD_COLOR);
    _paint.Outline(WINDOW_INSET, top, width, QUEUE_ROW_HEIGHT, EDGE_COLOR);
    _paint.Text(std::to_string(slot + 1), WINDOW_INSET + 10.0f, top + 7.0f, filled ? LABEL_COLOR : FAINT_COLOR, Hud::Typeface::Figure);
    if (!filled)
      continue;
    const Hud::QueueLine& line = _queue[slot];
    _paint.Text(line.name, WINDOW_INSET + 32.0f, top + 5.0f, TEXT_COLOR, Hud::Typeface::Name);
    const float right = WINDOW_INSET + width - 10.0f;
    if (line.waiting)
      _paint.RightText("WAITING FOR ORE", right, top + 9.0f, AMBER_COLOR, Hud::Typeface::Label, 12.0f * CONDENSED_ADVANCE);
    else if (line.front)
    {
      const float barLeft = right - QUEUE_BAR_WIDTH - 44.0f;
      _paint.Panel(barLeft, top + 12.0f, QUEUE_BAR_WIDTH, 6.0f, BAR_TRACK_COLOR);
      _paint.Panel(barLeft, top + 12.0f,
                   QUEUE_BAR_WIDTH * static_cast<float>(std::clamp(line.permille, 0, Outpost::PERMILLE)) /
                     static_cast<float>(Outpost::PERMILLE),
                   6.0f, BAR_FILL_COLOR);
      _paint.RightText(std::format("{}%", line.permille / 10), right, top + 7.0f, TEXT_COLOR, Hud::Typeface::Figure, 13.0f * MONO_ADVANCE);
    }
  }
}

float ProductionHeight(const Hud::ProductionPanel& _panel) noexcept
{
  return CardsTop() + CardsHeight((_panel.options.size() + 1) / 2, OPTION_HEIGHT);
}

// The production window: the producer with arrows to the others, its queue, and a card for each thing it builds.
void LayProduction(Hud::Layout& _layout, const Hud::ProductionPanel& _panel, Outpost::WindowManager::Point _corner, float _scale)
{
  (void)OpenWindow(_layout, Outpost::WindowKind::Production, std::string(), _corner, PRODUCTION_WIDTH,
                   ProductionHeight(_panel) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _corner, _scale);
  paint.SmallButton("<", WINDOW_INSET, 9.0f, _panel.canStep, {.kind = Hud::ActionKind::PreviousProducer});
  paint.SmallButton(">", WINDOW_INSET + SMALL_BUTTON_UNITS + 4.0f, 9.0f, _panel.canStep, {.kind = Hud::ActionKind::NextProducer});
  paint.Text("PRODUCTION", WINDOW_INSET + (2.0f * SMALL_BUTTON_UNITS) + 12.0f, 11.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  Header(paint, _panel.producer, _panel.ore, PRODUCTION_WIDTH);
  QueueRows(paint, _panel.queue, PRODUCTION_WIDTH);

  const float top = CardsTop();
  paint.Text("BUILD", WINDOW_INSET, top - SECTION_LABEL_HEIGHT + 4.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  const float width = (PRODUCTION_WIDTH - (2.0f * WINDOW_INSET) - CARD_SPACING) / 2.0f;
  for (std::size_t i = 0; i < _panel.options.size(); ++i)
  {
    const Hud::QueueOption& option = _panel.options[i];
    const std::size_t line = i / 2;
    const std::size_t column = i % 2;
    const float left = WINDOW_INSET + (static_cast<float>(column) * (width + CARD_SPACING));
    const float cardTop = top + (static_cast<float>(line) * (OPTION_HEIGHT + CARD_SPACING));
    const Hud::Rect face = paint.Panel(left, cardTop, width, OPTION_HEIGHT, option.enabled ? CARD_COLOR : FIELD_COLOR);
    if (option.enabled)
      paint.Press(face, option.action);
    paint.Outline(left, cardTop, width, OPTION_HEIGHT, EDGE_COLOR);
    paint.Text(CutShort(option.name, CHIP_NAME_CHARACTERS), left + 12.0f, cardTop + 6.0f, option.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR,
               Hud::Typeface::Name);
    paint.Text(option.detail, left + 12.0f, cardTop + 28.0f, option.enabled ? CODE_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
    paint.DiamondAndFigure(option.cost, left + width - 12.0f, cardTop + 28.0f, Hud::Typeface::Figure, 13.0f,
                           option.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
  }
  if (!_panel.hint.empty())
    paint.Text(_panel.hint, WINDOW_INSET, top + 16.0f, LABEL_COLOR, Hud::Typeface::Detail);
}

float ResearchHeight(const Hud::ResearchPanel& _panel) noexcept
{
  const std::size_t shown = std::min(Hud::TOPICS_SHOWN, _panel.topics.size() - std::min(_panel.firstTopic, _panel.topics.size()));
  return CardsTop() + CardsHeight((shown + Hud::TOPIC_COLUMNS - 1) / Hud::TOPIC_COLUMNS, TOPIC_HEIGHT);
}

// The research window: the Research Lab, its queue, and a card for each topic not researched or queued yet, a page of
// them at a time with arrows to scroll by a row.
void LayResearch(Hud::Layout& _layout, const Hud::ResearchPanel& _panel, Outpost::WindowManager::Point _corner, float _scale)
{
  (void)OpenWindow(_layout, Outpost::WindowKind::Research, std::string(), _corner, RESEARCH_WIDTH,
                   ResearchHeight(_panel) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _corner, _scale);
  paint.Text("RESEARCH", WINDOW_INSET, 11.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  Header(paint, _panel.lab, _panel.ore, RESEARCH_WIDTH);
  QueueRows(paint, _panel.queue, RESEARCH_WIDTH);

  const float top = CardsTop();
  paint.Text("TOPICS", WINDOW_INSET, top - SECTION_LABEL_HEIGHT + 4.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  if (_panel.topics.size() > Hud::TOPICS_SHOWN)
  {
    const float arrowsLeft = RESEARCH_WIDTH - WINDOW_INSET - (2.0f * SMALL_BUTTON_UNITS) - 4.0f;
    const float arrowsTop = top - SECTION_LABEL_HEIGHT;
    paint.SmallButton("<", arrowsLeft, arrowsTop, _panel.firstTopic > 0, {.kind = Hud::ActionKind::PreviousTopics});
    paint.SmallButton(">", arrowsLeft + SMALL_BUTTON_UNITS + 4.0f, arrowsTop, _panel.firstTopic + Hud::TOPICS_SHOWN < _panel.topics.size(),
                      {.kind = Hud::ActionKind::NextTopics});
  }
  const auto columns = static_cast<float>(Hud::TOPIC_COLUMNS);
  const float width = (RESEARCH_WIDTH - (2.0f * WINDOW_INSET) - ((columns - 1.0f) * CARD_SPACING)) / columns;
  for (std::size_t i = _panel.firstTopic, shown = 0; i < _panel.topics.size() && shown < Hud::TOPICS_SHOWN; ++i, ++shown)
  {
    const Hud::TopicCard& topic = _panel.topics[i];
    const std::size_t line = shown / Hud::TOPIC_COLUMNS;
    const std::size_t column = shown % Hud::TOPIC_COLUMNS;
    const float left = WINDOW_INSET + (static_cast<float>(column) * (width + CARD_SPACING));
    const float cardTop = top + (static_cast<float>(line) * (TOPIC_HEIGHT + CARD_SPACING));
    const bool blocked = !topic.needs.empty();
    const Hud::Rect face = paint.Panel(left, cardTop, width, TOPIC_HEIGHT,
                                       blocked         ? LOCKED_COLOR
                                       : topic.enabled ? CARD_COLOR
                                                       : FIELD_COLOR);
    if (blocked)
      paint.Panel(left, cardTop, width, TOPIC_HEIGHT, LOCKED_HATCH_COLOR, Hud::Fill::Hatched);
    if (topic.enabled)
      paint.Press(face, topic.action);
    // A gateway, which opens its tier, is edged in gold.
    paint.Outline(left, cardTop, width, TOPIC_HEIGHT, topic.gateway ? GOLD_COLOR : EDGE_COLOR);
    const DirectX::XMFLOAT4& color = topic.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR;
    paint.Text(topic.name, left + 12.0f, cardTop + 6.0f, color, Hud::Typeface::Name);
    paint.DiamondAndFigure(topic.cost, left + width - 12.0f, cardTop + 9.0f, Hud::Typeface::Figure, 13.0f,
                           topic.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
    paint.Text(topic.effect, left + 12.0f, cardTop + 28.0f, topic.enabled ? NUMBERS_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
    if (blocked)
    {
      paint.Sprite(Hud::Sprite::Checkbox, left + 12.0f, cardTop + 46.0f, 10.0f, AMBER_COLOR);
      paint.Text(topic.needs, left + 28.0f, cardTop + 44.0f, AMBER_COLOR, Hud::Typeface::Label);
    }
    else
      paint.Text(topic.time, left + 12.0f, cardTop + 44.0f, topic.enabled ? LABEL_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
  }
}

// Where a layer's share of a list starts: the window's first, for a window, or the list's end, past the last window.
std::size_t LayerStart(const Hud::Layout& _layout, std::size_t _window, std::size_t Hud::Window::*_first, std::size_t _total) noexcept
{
  return _window < _layout.windows.size() ? _layout.windows[_window].*_first : _total;
}

Hud::Span LayerSpan(const Hud::Layout& _layout, std::size_t _layer, std::size_t Hud::Window::*_first, std::size_t _total) noexcept
{
  return {.first = _layer == 0 ? 0 : LayerStart(_layout, _layer - 1, _first, _total), .end = LayerStart(_layout, _layer, _first, _total)};
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
  const Span span = ActionsOf(LayerAt(_xPixels, _yPixels));
  for (std::size_t i = span.first; i < span.end; ++i)
  {
    if (actions[i].first.Contains(_xPixels, _yPixels))
      return actions[i].second;
  }
  return std::nullopt;
}

std::size_t Hud::Layout::LayerAt(float _xPixels, float _yPixels) const noexcept
{
  for (std::size_t window = windows.size(); window-- > 0;)
  {
    if (windows[window].frame.Contains(_xPixels, _yPixels))
      return window + 1;
  }
  return 0;
}

const Hud::Window* Hud::Layout::WindowAt(float _xPixels, float _yPixels) const noexcept
{
  const std::size_t layer = LayerAt(_xPixels, _yPixels);
  return layer == 0 ? nullptr : &windows[layer - 1];
}

Hud::Span Hud::Layout::PanelsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &Window::firstPanel, panels.size());
}

Hud::Span Hud::Layout::TextsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &Window::firstText, texts.size());
}

Hud::Span Hud::Layout::SpritesOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &Window::firstSprite, sprites.size());
}

Hud::Span Hud::Layout::ActionsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &Window::firstAction, actions.size());
}

Outpost::WindowManager::Point Hud::KeepOnScreen(WindowManager::Point _corner, float _widthUnits, float _screenWidthUnits,
                                                float _screenHeightUnits) noexcept
{
  const float kept = std::min(WINDOW_KEPT_ON_SCREEN_UNITS, _widthUnits);
  return {.xUnits = std::clamp(_corner.xUnits, kept - _widthUnits, std::max(kept - _widthUnits, _screenWidthUnits - kept)),
          .yUnits = std::clamp(_corner.yUnits, 0.0f, std::max(0.0f, _screenHeightUnits - TITLE_BAR_UNITS))};
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
                                             const Designer* _designer, std::optional<Action> _hovered)
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
  if (_designer != nullptr)
    content.designer = DescribeDesigner(_newest, *_designer, _hovered);

  content.marks.reserve(_entities.size());
  for (const EntityView& entity : _entities)
  {
    const Side side = !entity.owner.IsValid() ? Side::Neutral : entity.owner == _newest.player ? Side::Own : Side::Enemy;
    content.marks.push_back({.position = entity.position,
                             .radiusMeters = entity.radiusMeters,
                             .side = side,
                             .kind = entity.kind,
                             .dry = entity.kind == EntityKind::Asteroid && entity.oreReserveHundredths == 0});
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

  const auto nameOf = [&_newest](DesignId _design) { return DesignNameOf(_newest, _design); };

  // A structure is selected on its own (PlayerControls): its kind, its state, and the windows it opens.
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
      // A Mining Rig's asteroid's Ore left, as far as the player knows it (Phase 1 design §8).
      if (structure->structure == StructureKind::MiningRig && structure->oreReserveHundredths.has_value())
      {
        content.selection.push_back(*structure->oreReserveHundredths > 0
                                      ? std::format("Ore left {}", WithThousands(WholePoints(*structure->oreReserveHundredths)))
                                      : std::string("Ore run out: it earns a trickle"));
      }
      // Its queue and what it can add to it are its windows' (Phase 1 design §12; owner, 2026-10-03): its panel offers to
      // open them, once it is the player's own and finished.
      if (structure->owner == _newest.player && structure->builtPermille >= PERMILLE)
      {
        const Action production{.kind = ActionKind::OpenProduction, .producer = structure->id};
        if (structure->structure == StructureKind::CommandStation)
          content.buttons.push_back({.label = "Production", .action = production});
        else if (structure->structure == StructureKind::Shipyard)
        {
          content.buttons.push_back({.label = "Production", .action = production});
          content.buttons.push_back({.label = "Ship designer", .action = {.kind = ActionKind::OpenDesigner, .producer = structure->id}});
        }
        else if (structure->structure == StructureKind::ResearchLab)
          content.buttons.push_back({.label = "Research", .action = {.kind = ActionKind::OpenResearch, .producer = structure->id}});
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

Hud::ProductionPanel Hud::DescribeProduction(const Snapshot& _newest, const EntityView* _producer)
{
  ProductionPanel panel{.producer = "NO PRODUCER", .ore = _newest.ore, .hint = "Build a Shipyard to make warships."};
  panel.canStep = ProductionTarget::Producers(_newest).size() > 1;
  if (_producer == nullptr)
    return panel;
  panel.hasProducer = true;
  panel.hint.clear();
  const bool station = _producer->structure == StructureKind::CommandStation;
  panel.producer = station ? std::string("COMMAND STATION") : std::format("SHIPYARD {:02}", _producer->shipyardNumber);

  std::vector<std::string> names;
  names.reserve(_producer->queue.size());
  for (const JobView& job : _producer->queue)
    names.push_back(DesignNameOf(_newest, job.role == ShipRole::Constructor ? DesignId{} : job.design));
  panel.queue = QueueLinesOf(std::move(names), _producer->jobPermille);

  // A Constructor at the Command Station; each saved design at a Shipyard, with its abbreviation (design §5).
  const bool room = _producer->queue.size() < QUEUE_LIMIT;
  if (station)
  {
    panel.options.push_back({.name = std::string(CONSTRUCTOR_NAME),
                             .cost = _newest.constructorCost,
                             .action = {.kind = ActionKind::Queue, .producer = _producer->id},
                             .enabled = room && _newest.ore >= _newest.constructorCost});
    return panel;
  }
  const auto abbreviationOf = []<typename View>(const std::vector<View>& _views, auto _id)
  {
    const auto view = std::ranges::find(_views, _id, &View::id);
    return view != _views.end() ? Abbreviation(view->nameUtf8) : std::string("?");
  };
  const std::string_view dot = DOT.substr(1, 2);
  panel.options.reserve(_newest.designs.size());
  for (const DesignView& design : _newest.designs)
  {
    panel.options.push_back(
      {.name = design.nameUtf8,
       .detail = std::format("{}{}{}{}{}", abbreviationOf(_newest.hulls, design.hull), dot, abbreviationOf(_newest.drives, design.drive),
                             dot, abbreviationOf(_newest.weapons, design.weapon)),
       .cost = design.cost,
       .action = {.kind = ActionKind::Queue, .producer = _producer->id, .design = design.id},
       .enabled = room && _newest.ore >= design.cost});
  }
  if (panel.options.empty())
    panel.hint = "Save a design in the ship designer to build it here.";
  return panel;
}

Hud::ResearchPanel Hud::DescribeResearch(const Snapshot& _newest, std::span<const EntityView> _entities, std::size_t _firstTopic)
{
  ResearchPanel panel{.lab = "NO RESEARCH LAB", .ore = _newest.ore};
  // The player's Research Lab, of which it has one at most (design §6).
  const auto lab = std::ranges::find_if(_entities,
                                        [&_newest](const EntityView& _entity)
                                        {
                                          return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::ResearchLab &&
                                                 _entity.owner == _newest.player && !_entity.remembered;
                                        });
  const EntityView* found = lab != _entities.end() ? &*lab : nullptr;
  const bool built = found != nullptr && found->builtPermille >= PERMILLE;
  if (found != nullptr)
  {
    panel.hasLab = true;
    panel.lab = built ? std::string("RESEARCH LAB") : std::format("RESEARCH LAB{}{}% BUILT", DOT, found->builtPermille / 10);
  }

  const auto topicOf = [&_newest](ResearchTopicId _topic) -> const ResearchTopicView*
  {
    const auto topic = std::ranges::find(_newest.research, _topic, &ResearchTopicView::id);
    return topic != _newest.research.end() ? &*topic : nullptr;
  };
  if (found != nullptr)
  {
    std::vector<std::string> names;
    names.reserve(found->research.size());
    for (const ResearchTopicId topic : found->research)
    {
      const ResearchTopicView* view = topicOf(topic);
      names.push_back(view != nullptr ? view->nameUtf8 : std::string("Unknown topic"));
    }
    panel.queue = QueueLinesOf(std::move(names), found->jobPermille);
  }

  // Each topic not researched or queued yet, and what it does; one whose prerequisites are neither is dim (design §8).
  const auto known = [&](ResearchTopicId _topic)
  {
    const ResearchTopicView* topic = topicOf(_topic);
    return (topic != nullptr && topic->researched) ||
           (found != nullptr && std::ranges::find(found->research, _topic) != found->research.end());
  };
  const bool canResearch = built && found->research.size() < QUEUE_LIMIT;
  for (const ResearchTopicView& topic : _newest.research)
  {
    if (known(topic.id))
      continue;
    std::string needs;
    for (const ResearchTopicId prerequisite : topic.prerequisites)
    {
      if (known(prerequisite))
        continue;
      const ResearchTopicView* view = topicOf(prerequisite);
      needs += std::format("{}{}", needs.empty() ? std::string("NEEDS") + std::string(DOT) : std::string(" + "),
                           view != nullptr ? Capitals(view->nameUtf8) : std::string("?"));
    }
    panel.topics.push_back(
      {.name = topic.nameUtf8,
       .effect = topic.effectUtf8,
       .cost = topic.cost,
       .time = std::format("TIER {}{}{} s", topic.tier, DOT, Tenths(topic.researchSeconds)),
       .needs = needs,
       .action = {.kind = ActionKind::Research, .producer = found != nullptr ? found->id : EntityId{}, .topic = topic.id},
       .enabled = canResearch && needs.empty() && _newest.ore >= topic.cost,
       .tier = topic.tier,
       .gateway = topic.gateway});
  }
  // Tier by tier, each in the tuning data's order (Phase 1 design §6).
  std::ranges::stable_sort(panel.topics, {}, &TopicCard::tier);
  panel.firstTopic = StepTopics(_firstTopic, 0, panel.topics.size());
  return panel;
}

std::size_t Hud::StepTopics(std::size_t _firstTopic, int _step, std::size_t _topics) noexcept
{
  // The rows past the last that fills the window are never the first shown.
  const std::size_t rows = (_topics + TOPIC_COLUMNS - 1) / TOPIC_COLUMNS;
  const std::size_t shownRows = TOPICS_SHOWN / TOPIC_COLUMNS;
  const auto lastFirstRow = static_cast<std::ptrdiff_t>(rows > shownRows ? rows - shownRows : 0);
  const auto row = static_cast<std::ptrdiff_t>(_firstTopic / TOPIC_COLUMNS) + _step;
  return static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(row, 0, lastFirstRow)) * TOPIC_COLUMNS;
}

std::vector<Neuron::FontDesc> Hud::Typefaces()
{
  // Weights as DirectWrite counts them: 600 is semibold, 700 bold. Sizes are the mockup's, in reference units.
  const std::vector<std::wstring> condensed{L"Bahnschrift"};
  const std::vector<std::wstring> figures{L"Cascadia Mono", L"Consolas"};
  return {
    {.families = {L"Segoe UI"}, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = FONT_UNITS},
    {.families = condensed, .weight = 700, .stretch = Neuron::FontStretch::SemiCondensed, .emUnits = 22.0f},
    {.families = condensed, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = 12.0f},
    {.families = condensed, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = 16.0f},
    {.families = figures, .weight = 400, .stretch = Neuron::FontStretch::Normal, .emUnits = 13.0f},
    {.families = figures, .weight = 700, .stretch = Neuron::FontStretch::Normal, .emUnits = 28.0f},
    {.families = figures, .weight = 400, .stretch = Neuron::FontStretch::Normal, .emUnits = 11.0f},
  };
}

std::vector<Neuron::SpriteDesc> Hud::Sprites()
{
  return {
    {.shape = Neuron::SpriteShape::Diamond, .sizeUnits = 12.0f},
    {.shape = Neuron::SpriteShape::Checkbox, .sizeUnits = 10.0f},
    {.shape = Neuron::SpriteShape::CornerBracket, .sizeUnits = 12.0f},
  };
}

float Hud::Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  return std::min(static_cast<float>(_widthPixels) / REFERENCE_WIDTH_UNITS, static_cast<float>(_heightPixels) / REFERENCE_HEIGHT_UNITS);
}

Hud::Layout Hud::Lay(const Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels, std::span<const PlanePosition> _view,
                     const WindowManager* _windows)
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
      const DirectX::XMFLOAT4& color = mark.dry                                 ? DRY_COLOR
                                       : mark.side == Side::Own                 ? OWN_COLOR
                                       : mark.side == Side::Enemy               ? ENEMY_COLOR
                                       : mark.kind == EntityKind::AsteroidField ? ASTEROID_FIELD_COLOR
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

  // The floating windows (ADR-031), over everything else, back to front: those the manager has open, where it left them,
  // or, without a manager, every window the content has at its default place.
  const float screenWidthUnits = width / scale;
  const float screenHeightUnits = height / scale;
  std::vector<WindowKind> backToFront{WindowKind::Production, WindowKind::Research, WindowKind::Designer};
  if (_windows != nullptr)
    backToFront.assign(_windows->FrontToBack().rbegin(), _windows->FrontToBack().rend());
  const auto place = [&](WindowKind _kind, WindowManager::Point _default, float _widthUnits)
  {
    const std::optional<WindowManager::Point> moved = _windows != nullptr ? _windows->PositionOf(_kind) : std::nullopt;
    return KeepOnScreen(moved.value_or(_default), _widthUnits, screenWidthUnits, screenHeightUnits);
  };
  for (const WindowKind kind : backToFront)
  {
    // The designer, at first in the top-right corner.
    if (kind == WindowKind::Designer && _content.designer.has_value())
    {
      const WindowManager::Point corner =
        place(kind, {.xUnits = screenWidthUnits - MARGIN - DESIGNER_WIDTH, .yUnits = MARGIN}, DESIGNER_WIDTH);
      LayDesigner(layout, *_content.designer, corner, scale);
    }
    // Production and research, at first side by side under the Ore and the research line, clear of the designer.
    else if (kind == WindowKind::Production && _content.production.has_value())
      LayProduction(layout, *_content.production, place(kind, {.xUnits = MARGIN, .yUnits = WINDOWS_TOP}, PRODUCTION_WIDTH), scale);
    else if (kind == WindowKind::Research && _content.laboratory.has_value())
    {
      const WindowManager::Point corner =
        place(kind, {.xUnits = (2.0f * MARGIN) + PRODUCTION_WIDTH, .yUnits = WINDOWS_TOP}, RESEARCH_WIDTH);
      LayResearch(layout, *_content.laboratory, corner, scale);
    }
  }
  return layout;
}
