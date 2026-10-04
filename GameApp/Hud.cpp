#include "pch.h"
#include "Hud.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>

namespace
{
using Outpost::Hud;

// Light text, dim where it does nothing. The HUD's panels and buttons are in the windows' look, whose colors, sampled from
// the owner's mockup, follow the windows' code below (ADR-043).
constexpr DirectX::XMFLOAT4 TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f};
constexpr DirectX::XMFLOAT4 DIM_TEXT_COLOR{0.42f, 0.45f, 0.5f, 1.0f};
constexpr DirectX::XMFLOAT4 DISABLED_BUTTON_COLOR{0.05f, 0.06f, 0.08f, 0.85f};
// A designer's name the server would refuse, and an income of nothing.
constexpr DirectX::XMFLOAT4 WARNING_COLOR{1.0f, 0.5f, 0.35f, 1.0f};
// The minimap: the map's square, and its marks in the side's color, an ore asteroid in Ore's gold, darkened, so that the
// map reads without a legend: blue is the player's, red the enemy's, and gold is ore (ADR-043). The camera's view is a
// light outline.
constexpr DirectX::XMFLOAT4 MAP_COLOR{0.02f, 0.04f, 0.07f, 0.95f};
constexpr DirectX::XMFLOAT4 OWN_COLOR{0.35f, 0.65f, 1.0f, 1.0f};
constexpr DirectX::XMFLOAT4 ENEMY_COLOR{1.0f, 0.38f, 0.25f, 1.0f};
constexpr DirectX::XMFLOAT4 NEUTRAL_COLOR{0.45f, 0.42f, 0.4f, 1.0f};
constexpr DirectX::XMFLOAT4 ORE_ASTEROID_COLOR{0.42f, 0.23f, 0.02f, 1.0f};
// An ore asteroid that has run out: dark, and warmed toward rust.
constexpr DirectX::XMFLOAT4 DRY_COLOR{0.24f, 0.15f, 0.11f, 1.0f};
// An asteroid field is only in the way, so it is drawn darker than an ore asteroid, which is worth going to, though a
// field's square is the larger (ADR-040); dark enough that the fields stand back from the ore (ADR-046).
constexpr DirectX::XMFLOAT4 ASTEROID_FIELD_COLOR{0.06f, 0.06f, 0.065f, 1.0f};
constexpr DirectX::XMFLOAT4 VIEW_COLOR{0.85f, 0.9f, 1.0f, 0.8f};
// A sector on the minimap (ADR-056): a faint wash of its holder's color under the marks, an outline of it over the fog,
// and stripes of it while suppressed.
constexpr float SECTOR_WASH_ALPHA = 0.12f;
constexpr float SECTOR_OUTLINE_ALPHA = 0.55f;
constexpr float SECTOR_HATCH_ALPHA = 0.35f;
constexpr float SECTOR_LINE_UNITS = 1.0f;

// Everything below in reference units.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
// A line of text in the title face, and in the name face, as the HUD's panels set them; a figure's line.
constexpr float TITLE_LINE_UNITS = 30.0f;
constexpr float NAME_LINE_UNITS = 22.0f;
constexpr float FIGURE_LINE_UNITS = 16.0f;

// The Ore panel, anchored to the top-left corner: the stockpile as the windows write it, Ore's diamond and the figure in the
// title face from the panel's left, so that the diamond stays put as the figure changes, and the income at its right
// (ADR-046).
constexpr float ORE_PANEL_WIDTH = 260.0f;
constexpr float ORE_PANEL_HEIGHT = 44.0f;

// The selection panel, anchored to the bottom edge's middle, as wide as its longest line within these (ADR-043).
constexpr float SELECTION_PANEL_MIN_WIDTH = 280.0f;
constexpr float SELECTION_PANEL_WIDTH = 560.0f;
// Lines of designs before the rest are counted together.
constexpr size_t SELECTION_DESIGN_LINES = 5;
// The selection's hit points as a bar under its lines: green above half, then amber, then red, as a bar over a damaged
// ship is (ADR-046).
constexpr float HEALTH_BAR_UNITS = 6.0f;
constexpr float HEALTH_BAR_GAP_UNITS = 8.0f;
constexpr float HEALTH_HURT_SHARE = 0.5f;
constexpr float HEALTH_LOW_SHARE = 0.25f;

// The buttons, stacked in a panel anchored to the bottom-right corner; a button's label and cost stand this far in.
constexpr float BUTTON_PANEL_WIDTH = 380.0f;
constexpr float BUTTON_HEIGHT = 34.0f;
constexpr float BUTTON_GAP = 6.0f;
constexpr float BUTTON_INSET = 12.0f;

// The minimap, a square anchored to the bottom-left corner, with the map drawn inside its padding.
constexpr float MINIMAP_SIZE = 260.0f;
constexpr float MINIMAP_PADDING = 8.0f;
// The smallest a mark is drawn, and how wide the view's outline is, so that both stay visible. An ore asteroid's is the
// largest, since it is what the player looks for on the map; a rig's mark is drawn over its asteroid's (ADR-046).
constexpr float SHIP_MARK_UNITS = 3.0f;
constexpr float STRUCTURE_MARK_UNITS = 6.0f;
constexpr float ORE_MARK_UNITS = 8.0f;
constexpr float VIEW_LINE_UNITS = 1.5f;

// The hint, anchored to the top edge's middle.
constexpr float HINT_PANEL_WIDTH = 640.0f;

// The research line, under the Ore panel.
constexpr float RESEARCH_PANEL_WIDTH = 560.0f;
constexpr float RESEARCH_PANEL_GAP = 8.0f;
// The territory, under the research line's place (ADR-056, ADR-057), and the room above and below its lines.
constexpr float TERRITORY_PANEL_WIDTH = 260.0f;
constexpr float TERRITORY_INSET_UNITS = 10.0f;
// The alerts, under the territory (ADR-059), and how large an alert's mark is on the minimap.
constexpr float ALERT_PANEL_WIDTH = 380.0f;
constexpr float ALERT_MARK_UNITS = 14.0f;

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

// The share of its hit points something has left; none for what has no hit points at all.
float HealthShare(std::int64_t _hundredths, std::int64_t _maxHundredths) noexcept
{
  return _maxHundredths > 0 ? static_cast<float>(_hundredths) / static_cast<float>(_maxHundredths) : 0.0f;
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

// A design's code, its components' initials joined by middle dots, "S·I·MD", with its module's after them when it has
// one, "S·I·MD·SA" (Phase 1 design §5, Phase 2 design §10). The designer's chips and the production window both show it.
std::string DesignCodeOf(const Outpost::Snapshot& _newest, const Outpost::DesignView& _design)
{
  const auto initials = []<typename View>(const std::vector<View>& _views, auto _id)
  {
    const auto view = std::ranges::find(_views, _id, &View::id);
    return view != _views.end() ? Outpost::Abbreviation(view->nameUtf8) : std::string("?");
  };
  const std::string_view dot = DOT.substr(1, 2);
  std::string code = std::format("{}{}{}{}{}", initials(_newest.hulls, _design.hull), dot, initials(_newest.drives, _design.drive), dot,
                                 initials(_newest.weapons, _design.weapon));
  if (_design.module.IsValid())
    code += std::format("{}{}", dot, initials(_newest.modules, _design.module));
  return code;
}

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
  double sensor = 0.0;
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
  // A module adds its cost and its sight to any design (Phase 2 design §10).
  double moduleCost = 0.0;
  for (const Outpost::ModuleView& module : _newest.modules)
  {
    moduleCost = std::max(moduleCost, static_cast<double>(module.cost));
    best.sensor = std::max(best.sensor, module.sightMeters);
  }
  best.cost += moduleCost;
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
    panel.chips.push_back({.name = design.nameUtf8,
                           .code = DesignCodeOf(_newest, design),
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
    else if (_hovered->kind == Hud::ActionKind::PickModule && _hovered->module != preview.module)
      preview.module = _hovered->module;
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
  // The module row: no module first, which every design starts with, then each module (Phase 2 design §10).
  Hud::SlotRow& modules = panel.slots[3];
  modules.label = "MODULE";
  modules.cards.reserve(_newest.modules.size() + 1);
  modules.picked = "None";
  modules.cards.push_back({.name = "None",
                           .cost = 0,
                           .numbers = "No module",
                           .picked = !picked.module.IsValid(),
                           .action = {.kind = Hud::ActionKind::PickModule}});
  if (!preview.module.IsValid() && picked.module.IsValid())
    previewing = "no module";
  for (const Outpost::ModuleView& module : _newest.modules)
  {
    if (module.id == picked.module)
      modules.picked = module.nameUtf8;
    if (module.id == preview.module && preview.module != picked.module)
      previewing = module.nameUtf8;
    modules.cards.push_back(
      {.name = module.nameUtf8,
       .cost = module.cost,
       .numbers = std::format("SIGHT {}m{}SPD {}{}", Tenths(module.sightMeters), DOT, TIMES, Tenths(module.speedFactor)),
       .picked = module.id == picked.module,
       .lockedBy = module.available ? std::string() : std::string("RESEARCH"),
       .action = {.kind = Hud::ActionKind::PickModule, .module = module.id}});
  }

  const std::optional<Outpost::DesignStats> stats = _designer.Stats(_newest);
  std::optional<Outpost::DesignStats> previewStats;
  if (!previewing.empty())
  {
    Outpost::Designer previewer;
    previewer.PickHull(preview.hull);
    previewer.PickDrive(preview.drive);
    previewer.PickWeapon(preview.weapon);
    previewer.PickModule(preview.module);
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
    panel.bars.reserve(7);
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
    // How far a module lets the ship see; none without one, when its weapon sets its sight (Phase 2 design §10).
    if (best.sensor > 0.0)
    {
      bar(
        "Sensors", "m", best.sensor, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.moduleSightMeters); },
        [](const Outpost::DesignStats& _s) { return _s.moduleSightMeters > 0.0f ? Tenths(_s.moduleSightMeters) : std::string("-"); });
    }
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

// A bracket at each corner of _frame, as a window has.
void AddCorners(Hud::Layout& _layout, const Hud::Rect& _frame, float _scale)
{
  const float corner = CORNER_UNITS * _scale;
  for (const bool right : {false, true})
  {
    for (const bool bottom : {false, true})
    {
      _layout.sprites.push_back({.sprite = Hud::Sprite::Corner,
                                 .area = {right ? _frame.left + _frame.width - corner : _frame.left,
                                          bottom ? _frame.top + _frame.height - corner : _frame.top, corner, corner, WINDOW_EDGE_COLOR},
                                 .mirrorX = right,
                                 .mirrorY = bottom});
    }
  }
}

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
  AddCorners(_layout, window.frame, _scale);
  _layout.windows.push_back(window);
  return top + titleHeight;
}

// A panel of the HUD, in pixels, in a window's look though it is anchored and has no title bar (ADR-043): a window's body
// and a bracket at each corner. It returns the body.
Hud::Rect Frame(Hud::Layout& _layout, float _left, float _top, float _width, float _height, float _scale)
{
  const Hud::Rect body = _layout.panels.emplace_back(Hud::Rect{_left, _top, _width, _height, WINDOW_COLOR});
  AddCorners(_layout, body, _scale);
  return body;
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
constexpr float SLOTS_TOP = 168.0f;
constexpr float SLOT_GAP = 10.0f;
constexpr float SLOT_DIVIDER_LEFT = 108.0f;
constexpr float CARDS_LEFT = 118.0f;
constexpr float CARD_WIDTH = 190.0f;
constexpr float CARD_HEIGHT = 80.0f;
constexpr float CARD_GAP = 7.0f;
constexpr std::size_t CARDS_PER_LINE = 3;
// A part card's lines under its name: its numbers, any note, such as a weapon's splash, and at its foot the research that
// unlocks it. A row whose cards have notes is taller, so that a note and that line never meet (ADR-056).
constexpr float CARD_NUMBERS_TOP = 34.0f;
constexpr float CARD_NOTE_TOP = 50.0f;
constexpr float CARD_NOTE_ROOM = 14.0f;
constexpr float CARD_LOCK_FROM_FOOT = 19.0f;
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
// Ore's diamond beside a figure: this share of the figure's size, and this far from it.
constexpr float ORE_MARK_SHARE = 0.6f;
constexpr float ORE_MARK_GAP_UNITS = 5.0f;
// The title face's size, which Hud::Typefaces gives it, for the diamond beside a figure in it.
constexpr float TITLE_FACE_UNITS = 22.0f;
// The room a line cut short to fit keeps from what stands beside it, such as a cost (ADR-056).
constexpr float FIT_GAP_UNITS = 6.0f;
// The box a window's header holds the player's Ore in.
constexpr float ORE_BOX_WIDTH = 116.0f;

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
  std::array<float, std::tuple_size_v<decltype(Hud::DesignerPanel::slots)>> slotTops{};
  float slotsEnd = 0.0f;
  float sectionsTop = 0.0f;
  float footerTop = 0.0f;
  float height = 0.0f;
};

float CardLinesOf(const Hud::SlotRow& _slot) noexcept
{
  return static_cast<float>(std::max<std::size_t>(1, (_slot.cards.size() + CARDS_PER_LINE - 1) / CARDS_PER_LINE));
}

// How tall a slot's cards are: taller when any has a note, so that every card of the row stays the same.
float CardHeightOf(const Hud::SlotRow& _slot) noexcept
{
  const bool noted = std::ranges::any_of(_slot.cards, [](const Hud::PartCard& _card) { return !_card.note.empty(); });
  return noted ? CARD_HEIGHT + CARD_NOTE_ROOM : CARD_HEIGHT;
}

DesignerExtent ExtentOf(const Hud::DesignerPanel& _panel) noexcept
{
  DesignerExtent extent;
  float y = SLOTS_TOP;
  for (std::size_t slot = 0; slot < _panel.slots.size(); ++slot)
  {
    extent.slotTops[slot] = y;
    const float lines = CardLinesOf(_panel.slots[slot]);
    y += (lines * CardHeightOf(_panel.slots[slot])) + ((lines - 1.0f) * CARD_GAP) + SLOT_GAP;
  }
  extent.slotsEnd = y - SLOT_GAP;
  extent.sectionsTop = extent.slotsEnd + SECTION_GAP;
  const float bars = SECTION_LABEL_HEIGHT + (6.0f * BAR_ROW_HEIGHT);
  const float damage = SECTION_LABEL_HEIGHT + DAMAGE_CARD_HEIGHT + (2.0f * PADDING);
  extent.footerTop = extent.sectionsTop + std::max(bars, damage) + PADDING;
  extent.height = extent.footerTop + FOOTER_HEIGHT + SECTION_GAP;
  return extent;
}

// Draws into a window of the layout in reference units from the window's top-left corner, as the windows' layouts place
// everything (ADR-031), measuring its text with the fonts it is drawn in (ADR-056).
class Painter
{
public:
  Painter(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, Outpost::WindowManager::Point _corner, float _scale) noexcept
    : m_layout(_layout),
      m_metrics(_metrics),
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

  [[nodiscard]] const Hud::TextMetrics& Metrics() const noexcept
  {
    return m_metrics;
  }

  // How wide _text is set in _face, in reference units, with _trackingPixels more between each two characters.
  [[nodiscard]] float Width(std::string_view _text, Hud::Typeface _face, float _trackingPixels = 0.0f) const noexcept
  {
    return m_metrics.Width(_face, _text, _trackingPixels / m_scale);
  }

  // _text as _face sets it within _widthUnits, cut short when it does not fit.
  [[nodiscard]] std::string Fit(std::string_view _text, Hud::Typeface _face, float _widthUnits, float _trackingPixels = 0.0f) const
  {
    return m_metrics.Fit(_face, _text, _widthUnits, _trackingPixels / m_scale);
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

  // A line that ends at _right.
  void RightText(std::string _text, float _right, float _top, const DirectX::XMFLOAT4& _color, Hud::Typeface _face,
                 float _tracking = 0.0f)
  {
    const float left = _right - Width(_text, _face, _tracking);
    Text(std::move(_text), left, _top, _color, _face, _tracking);
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

  // How wide an amount of Ore is set, its diamond and its figure, in _face at _sizeUnits.
  [[nodiscard]] float DiamondAndFigureWidth(std::int32_t _ore, Hud::Typeface _face, float _sizeUnits) const
  {
    return std::round(_sizeUnits * ORE_MARK_SHARE) + ORE_MARK_GAP_UNITS + Width(Outpost::WithThousands(_ore), _face);
  }

  // An amount of Ore that ends at _right: Ore's diamond, then the figure, in _face at _sizeUnits.
  void DiamondAndFigure(std::int32_t _ore, float _right, float _top, Hud::Typeface _face, float _sizeUnits, const DirectX::XMFLOAT4& _color)
  {
    DiamondAndFigureFrom(_ore, _right - DiamondAndFigureWidth(_ore, _face, _sizeUnits), _top, _face, _sizeUnits, _color);
  }

  // An amount of Ore that starts at _left: Ore's diamond, then the figure, in _face at _sizeUnits.
  void DiamondAndFigureFrom(std::int32_t _ore, float _left, float _top, Hud::Typeface _face, float _sizeUnits,
                            const DirectX::XMFLOAT4& _color)
  {
    const float mark = std::round(_sizeUnits * ORE_MARK_SHARE);
    Sprite(Hud::Sprite::OreMark, _left, _top + ((_sizeUnits * 1.25f) - mark) / 2.0f, mark, _color);
    Text(Outpost::WithThousands(_ore), _left + mark + ORE_MARK_GAP_UNITS, _top, _color, _face);
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
    Panel(_right - ORE_BOX_WIDTH, 38.0f, ORE_BOX_WIDTH, 30.0f, FIELD_COLOR);
    Outline(_right - ORE_BOX_WIDTH, 38.0f, ORE_BOX_WIDTH, 30.0f, EDGE_COLOR);
    DiamondAndFigure(_ore, _right - 10.0f, 39.0f, Hud::Typeface::Title, TITLE_FACE_UNITS, GOLD_COLOR);
  }

private:
  Hud::Layout& m_layout;
  const Hud::TextMetrics& m_metrics;
  float m_originX;
  float m_originY;
  float m_scale;
};

// A button of the HUD, in pixels, in the look of a window's card (ADR-043): its face and edge, its label in the name face,
// any cost after a '|' as Ore's diamond and the figure at its right, and its place among the actions when it does
// something. The label is cut short only where it would meet the cost or the note.
void AddButton(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, float _scale, const Hud::Rect& _area, const Hud::Button& _button)
{
  Painter paint(_layout, _metrics, {.xUnits = _area.left / _scale, .yUnits = _area.top / _scale}, _scale);
  const float width = _area.width / _scale;
  const float height = _area.height / _scale;
  const Hud::Rect face = paint.Panel(0.0f, 0.0f, width, height,
                                     _button.selected  ? PICKED_COLOR
                                     : _button.enabled ? CARD_COLOR
                                                       : FIELD_COLOR);
  if (_button.enabled)
    paint.Press(face, _button.action);
  paint.Outline(0.0f, 0.0f, width, height, _button.selected ? PICKED_EDGE_COLOR : EDGE_COLOR);
  const size_t split = _button.label.find('|');
  const std::string label = _button.label.substr(0, split);
  const DirectX::XMFLOAT4& labelColor = _button.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR;
  const float labelTop = (height - NAME_LINE_UNITS) / 2.0f;
  const float figureTop = (height - FIGURE_LINE_UNITS) / 2.0f;
  const float room = width - (2.0f * BUTTON_INSET);
  std::int32_t cost = 0;
  const std::string figure = split == std::string::npos ? std::string() : _button.label.substr(split + 1);
  const bool costed = split != std::string::npos && std::from_chars(figure.data(), figure.data() + figure.size(), cost).ec == std::errc{};
  if (split != std::string::npos && !_button.enabled && !_button.note.empty())
  {
    paint.RightText(_button.note, width - BUTTON_INSET, figureTop, LABEL_COLOR, Hud::Typeface::Label);
    paint.Text(paint.Fit(label, Hud::Typeface::Name, room - paint.Width(_button.note, Hud::Typeface::Label) - FIT_GAP_UNITS), BUTTON_INSET,
               labelTop, labelColor, Hud::Typeface::Name);
    return;
  }
  if (!costed)
  {
    paint.Text(paint.Fit(label, Hud::Typeface::Name, room), BUTTON_INSET, labelTop, labelColor, Hud::Typeface::Name);
    return;
  }
  const float costWidth = paint.DiamondAndFigureWidth(cost, Hud::Typeface::Figure, 13.0f);
  paint.Text(paint.Fit(label, Hud::Typeface::Name, room - costWidth - FIT_GAP_UNITS), BUTTON_INSET, labelTop, labelColor,
             Hud::Typeface::Name);
  paint.DiamondAndFigure(cost, width - BUTTON_INSET, figureTop, Hud::Typeface::Figure, 13.0f, _button.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
}

void LayDesigner(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, const Hud::DesignerPanel& _panel, Outpost::WindowManager::Point _corner,
                 float _scale)
{
  const DesignerExtent extent = ExtentOf(_panel);
  (void)OpenWindow(_layout, Outpost::WindowKind::Designer, std::string(), _corner, DESIGNER_WIDTH, extent.height - Hud::TITLE_BAR_UNITS,
                   _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
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
  const std::string count = std::format("{}/{}", _panel.name.size(), Outpost::DESIGN_NAME_LIMIT);
  const float nameRoom = fieldWidth - 66.0f - 12.0f - paint.Width(count, Hud::Typeface::Figure) - FIT_GAP_UNITS;
  paint.Text(paint.Fit(_panel.editing ? _panel.name + "_" : _panel.name, Hud::Typeface::Name, nameRoom), DESIGNER_INSET + 66.0f,
             NAME_ROW_TOP + 9.0f, _panel.nameValid ? TEXT_COLOR : WARNING_COLOR, Hud::Typeface::Name);
  paint.RightText(count, DESIGNER_INSET + fieldWidth - 12.0f, NAME_ROW_TOP + 12.0f, FAINT_COLOR, Hud::Typeface::Figure);
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
    const float nameRoom = CHIP_WIDTH - 16.0f - paint.Width(chip.code, Hud::Typeface::Detail) - FIT_GAP_UNITS;
    paint.Text(paint.Fit(chip.name, Hud::Typeface::Label, nameRoom), left + 8.0f, CHIP_ROW_TOP + 6.0f, TEXT_COLOR, Hud::Typeface::Label);
    paint.RightText(chip.code, left + CHIP_WIDTH - 8.0f, CHIP_ROW_TOP + 7.0f, CODE_COLOR, Hud::Typeface::Detail);
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
    const float cardHeight = CardHeightOf(row);
    paint.Text(row.label, DESIGNER_INSET, top + 22.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
    paint.Text(paint.Fit(row.picked, Hud::Typeface::Name, SLOT_DIVIDER_LEFT - DESIGNER_INSET - FIT_GAP_UNITS), DESIGNER_INSET, top + 38.0f,
               ACCENT_COLOR, Hud::Typeface::Name);
    paint.Panel(SLOT_DIVIDER_LEFT, top + 6.0f, LINE_UNITS, (lines * cardHeight) + ((lines - 1.0f) * CARD_GAP) - 12.0f, EDGE_COLOR);
    for (std::size_t i = 0; i < row.cards.size(); ++i)
    {
      const Hud::PartCard& card = row.cards[i];
      const std::size_t line = i / CARDS_PER_LINE;
      const std::size_t column = i % CARDS_PER_LINE;
      const float left = CARDS_LEFT + (static_cast<float>(column) * (CARD_WIDTH + CARD_GAP));
      const float cardTop = top + (static_cast<float>(line) * (cardHeight + CARD_GAP));
      const bool locked = card.IsLocked();
      const Hud::Rect face = paint.Panel(left, cardTop, CARD_WIDTH, cardHeight,
                                         card.picked ? PICKED_COLOR
                                         : locked    ? LOCKED_COLOR
                                                     : CARD_COLOR);
      if (locked)
        paint.Panel(left, cardTop, CARD_WIDTH, cardHeight, LOCKED_HATCH_COLOR, Hud::Fill::Hatched);
      else
        paint.Press(face, card.action);
      paint.Outline(left, cardTop, CARD_WIDTH, cardHeight, card.picked ? PICKED_EDGE_COLOR : EDGE_COLOR);
      const float inner = CARD_WIDTH - 24.0f;
      const float costWidth = paint.DiamondAndFigureWidth(card.cost, Hud::Typeface::Figure, 13.0f);
      const DirectX::XMFLOAT4& nameColor = locked ? LOCKED_TEXT_COLOR : TEXT_COLOR;
      paint.Text(paint.Fit(card.name, Hud::Typeface::Name, inner - costWidth - FIT_GAP_UNITS), left + 12.0f, cardTop + 8.0f, nameColor,
                 Hud::Typeface::Name);
      paint.DiamondAndFigure(card.cost, left + CARD_WIDTH - 12.0f, cardTop + 11.0f, Hud::Typeface::Figure, 13.0f,
                             locked ? LOCKED_TEXT_COLOR : GOLD_COLOR);
      const DirectX::XMFLOAT4& numbersColor = locked ? LOCKED_TEXT_COLOR : NUMBERS_COLOR;
      paint.Text(paint.Fit(card.numbers, Hud::Typeface::Detail, inner), left + 12.0f, cardTop + CARD_NUMBERS_TOP, numbersColor,
                 Hud::Typeface::Detail);
      if (!card.note.empty())
      {
        paint.Text(paint.Fit(card.note, Hud::Typeface::Detail, inner), left + 12.0f, cardTop + CARD_NOTE_TOP, numbersColor,
                   Hud::Typeface::Detail);
      }
      if (locked)
      {
        const float lockTop = cardTop + cardHeight - CARD_LOCK_FROM_FOOT;
        paint.Sprite(Hud::Sprite::Checkbox, left + 12.0f, lockTop + 2.0f, 10.0f, AMBER_COLOR);
        paint.Text(paint.Fit(card.lockedBy, Hud::Typeface::Label, CARD_WIDTH - 28.0f - 12.0f), left + 28.0f, lockTop, AMBER_COLOR,
                   Hud::Typeface::Label);
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
                    Hud::Typeface::Figure);
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
    const float armorWidth = paint.Width(card.armor, Hud::Typeface::Label);
    paint.Text(paint.Fit(card.hull, Hud::Typeface::Name, cardWidth - 18.0f - armorWidth - FIT_GAP_UNITS), left + 10.0f, cardsTop + 8.0f,
               TEXT_COLOR, Hud::Typeface::Name);
    paint.RightText(card.armor, left + cardWidth - 8.0f, cardsTop + 11.0f, LABEL_COLOR, Hud::Typeface::Label);
    paint.Text(card.perShip, left + 10.0f, cardsTop + 28.0f, ChangeColor(card.change, TEXT_COLOR), Hud::Typeface::LargeFigure);
    paint.Panel(left + 10.0f, cardsTop + 66.0f, cardWidth - 20.0f, 4.0f, BAR_TRACK_COLOR);
    paint.Panel(left + 10.0f, cardsTop + 66.0f, (cardWidth - 20.0f) * card.share, 4.0f, RatingColor(card.rating));
    paint.Text(paint.Fit(card.perOre, Hud::Typeface::Detail, cardWidth - 20.0f), left + 10.0f, cardsTop + 76.0f, SOFT_COLOR,
               Hud::Typeface::Detail);
  }
  paint.Text(paint.Fit(_panel.hint, Hud::Typeface::Detail, damageWidth), DAMAGE_LEFT, cardsTop + DAMAGE_CARD_HEIGHT + 10.0f, LABEL_COLOR,
             Hud::Typeface::Detail);

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
// top-left corner: a hatched header with the window's subject and the Ore, the cards that add to the queue, and under them
// the queue, a row for each job. The cards stay where they are as the queue grows, so that one can be clicked again and
// again, and the window grows at its foot (ADR-043). Both windows are as wide, so that a design's name fits its card.
constexpr float QUEUE_WINDOW_WIDTH = 560.0f;
constexpr float WINDOW_INSET = 24.0f;
// A section's label, and how far under it what it labels starts.
constexpr float SECTION_TOP = 84.0f;
constexpr float SECTION_BODY_GAP = 20.0f;
constexpr float QUEUE_ROW_HEIGHT = 30.0f;
constexpr float QUEUE_ROW_GAP = 4.0f;
constexpr float QUEUE_BAR_WIDTH = 120.0f;
constexpr float OPTION_HEIGHT = 48.0f;
// A topic card: its name, then a line for each line of its effect and for each prerequisite neither researched nor queued,
// or one for its tier and time. Every card is as tall as the most lines any topic needs, and two at least (ADR-056).
constexpr float TOPIC_LINES_TOP = 28.0f;
constexpr float TOPIC_LINE_UNITS = 16.0f;
constexpr float TOPIC_FOOT_UNITS = 4.0f;
constexpr std::size_t TOPIC_LEAST_LINES = 2;
constexpr float CARD_SPACING = 8.0f;
// A card's text stands this far in from its edges.
constexpr float CARD_INSET = 12.0f;
// Where the production and research windows stand at first: under the Ore panel and the research line.
constexpr float WINDOWS_TOP = 128.0f;
constexpr float CARDS_TOP = SECTION_TOP + SECTION_BODY_GAP;

// The foot of _lines lines of cards _cardHeight tall, at least one.
float CardsBottom(std::size_t _lines, float _cardHeight) noexcept
{
  const auto lines = static_cast<float>(std::max<std::size_t>(_lines, 1));
  return CARDS_TOP + (lines * (_cardHeight + CARD_SPACING)) - CARD_SPACING;
}

// The queue's label stands under cards whose foot is at _cardsBottom; the window ends under its _jobs rows.
float QueueTop(float _cardsBottom) noexcept
{
  return _cardsBottom + SECTION_GAP;
}

float QueueWindowHeight(float _cardsBottom, std::size_t _jobs) noexcept
{
  return QueueTop(_cardsBottom) + SECTION_BODY_GAP + (static_cast<float>(_jobs) * (QUEUE_ROW_HEIGHT + QUEUE_ROW_GAP)) + SECTION_GAP;
}

// A window's header: its subject in the title face, and the Ore.
void Header(Painter& _paint, const std::string& _subject, std::int32_t _ore, float _widthUnits)
{
  _paint.HeaderBand(_widthUnits, DESIGNER_HEADER);
  const float room = _widthUnits - (2.0f * WINDOW_INSET) - ORE_BOX_WIDTH - FIT_GAP_UNITS;
  _paint.Text(_paint.Fit(_subject, Hud::Typeface::Title, room), WINDOW_INSET, 40.0f, TEXT_COLOR, Hud::Typeface::Title);
  _paint.OreBox(_ore, _widthUnits - WINDOW_INSET);
}

// A queue under its label at _top: a row for each job, with its name, and the front one's progress or its wait for Ore.
// The label counts the jobs against the queue's limit, which says how many more it takes.
void QueueRows(Painter& _paint, const std::vector<Hud::QueueLine>& _queue, float _widthUnits, float _top)
{
  _paint.Text(std::format("QUEUE{}{} / {}", DOT, _queue.size(), Outpost::QUEUE_LIMIT), WINDOW_INSET, _top, LABEL_COLOR,
              Hud::Typeface::Label, _paint.Tracking());
  const float width = _widthUnits - (2.0f * WINDOW_INSET);
  for (std::size_t slot = 0; slot < _queue.size(); ++slot)
  {
    const float top = _top + SECTION_BODY_GAP + (static_cast<float>(slot) * (QUEUE_ROW_HEIGHT + QUEUE_ROW_GAP));
    _paint.Panel(WINDOW_INSET, top, width, QUEUE_ROW_HEIGHT, CARD_COLOR);
    _paint.Outline(WINDOW_INSET, top, width, QUEUE_ROW_HEIGHT, EDGE_COLOR);
    _paint.Text(std::to_string(slot + 1), WINDOW_INSET + 10.0f, top + 7.0f, LABEL_COLOR, Hud::Typeface::Figure);
    const Hud::QueueLine& line = _queue[slot];
    const float right = WINDOW_INSET + width - 10.0f;
    const float barLeft = right - QUEUE_BAR_WIDTH - 44.0f;
    // The name runs up to the front job's progress or its wait for Ore.
    constexpr std::string_view WAITING = "WAITING FOR ORE";
    const float nameLeft = WINDOW_INSET + 32.0f;
    const float nameRight = line.waiting ? right - _paint.Width(WAITING, Hud::Typeface::Label) : line.front ? barLeft : right;
    _paint.Text(_paint.Fit(line.name, Hud::Typeface::Name, nameRight - nameLeft - FIT_GAP_UNITS), nameLeft, top + 5.0f, TEXT_COLOR,
                Hud::Typeface::Name);
    if (line.waiting)
      _paint.RightText(std::string(WAITING), right, top + 9.0f, AMBER_COLOR, Hud::Typeface::Label);
    else if (line.front)
    {
      _paint.Panel(barLeft, top + 12.0f, QUEUE_BAR_WIDTH, 6.0f, BAR_TRACK_COLOR);
      _paint.Panel(barLeft, top + 12.0f,
                   QUEUE_BAR_WIDTH * static_cast<float>(std::clamp(line.permille, 0, Outpost::PERMILLE)) /
                     static_cast<float>(Outpost::PERMILLE),
                   6.0f, BAR_FILL_COLOR);
      _paint.RightText(std::format("{}%", line.permille / 10), right, top + 7.0f, TEXT_COLOR, Hud::Typeface::Figure);
    }
  }
}

std::size_t OptionLines(const Hud::ProductionPanel& _panel) noexcept
{
  return (_panel.options.size() + 1) / 2;
}

float ProductionHeight(const Hud::ProductionPanel& _panel) noexcept
{
  return QueueWindowHeight(CardsBottom(OptionLines(_panel), OPTION_HEIGHT), _panel.queue.size());
}

// The production window: the producer with arrows to the others, a card for each thing it builds, and its queue.
void LayProduction(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, const Hud::ProductionPanel& _panel,
                   Outpost::WindowManager::Point _corner, float _scale)
{
  (void)OpenWindow(_layout, Outpost::WindowKind::Production, std::string(), _corner, QUEUE_WINDOW_WIDTH,
                   ProductionHeight(_panel) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.SmallButton("<", WINDOW_INSET, 9.0f, _panel.canStep, {.kind = Hud::ActionKind::PreviousProducer});
  paint.SmallButton(">", WINDOW_INSET + SMALL_BUTTON_UNITS + 4.0f, 9.0f, _panel.canStep, {.kind = Hud::ActionKind::NextProducer});
  paint.Text("PRODUCTION", WINDOW_INSET + (2.0f * SMALL_BUTTON_UNITS) + 12.0f, 11.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  Header(paint, _panel.producer, _panel.ore, QUEUE_WINDOW_WIDTH);

  paint.Text("BUILD", WINDOW_INSET, SECTION_TOP, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  const float width = (QUEUE_WINDOW_WIDTH - (2.0f * WINDOW_INSET) - CARD_SPACING) / 2.0f;
  const float inner = width - (2.0f * CARD_INSET);
  for (std::size_t i = 0; i < _panel.options.size(); ++i)
  {
    const Hud::QueueOption& option = _panel.options[i];
    const std::size_t line = i / 2;
    const std::size_t column = i % 2;
    const float left = WINDOW_INSET + (static_cast<float>(column) * (width + CARD_SPACING));
    const float cardTop = CARDS_TOP + (static_cast<float>(line) * (OPTION_HEIGHT + CARD_SPACING));
    const Hud::Rect face = paint.Panel(left, cardTop, width, OPTION_HEIGHT, option.enabled ? CARD_COLOR : FIELD_COLOR);
    if (option.enabled)
      paint.Press(face, option.action);
    paint.Outline(left, cardTop, width, OPTION_HEIGHT, EDGE_COLOR);
    // A design's name is cut short only where it would run past its card, and its abbreviation where it would meet the cost.
    paint.Text(paint.Fit(option.name, Hud::Typeface::Name, inner), left + CARD_INSET, cardTop + 6.0f,
               option.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Name);
    const float costWidth = paint.DiamondAndFigureWidth(option.cost, Hud::Typeface::Figure, 13.0f);
    paint.Text(paint.Fit(option.detail, Hud::Typeface::Detail, inner - costWidth - FIT_GAP_UNITS), left + CARD_INSET, cardTop + 28.0f,
               option.enabled ? CODE_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
    paint.DiamondAndFigure(option.cost, left + width - CARD_INSET, cardTop + 28.0f, Hud::Typeface::Figure, 13.0f,
                           option.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
  }
  if (!_panel.hint.empty())
  {
    paint.Text(paint.Fit(_panel.hint, Hud::Typeface::Detail, QUEUE_WINDOW_WIDTH - (2.0f * WINDOW_INSET)), WINDOW_INSET, CARDS_TOP + 16.0f,
               LABEL_COLOR, Hud::Typeface::Detail);
  }
  QueueRows(paint, _panel.queue, QUEUE_WINDOW_WIDTH, QueueTop(CardsBottom(OptionLines(_panel), OPTION_HEIGHT)));
}

std::size_t TopicLines(const Hud::ResearchPanel& _panel) noexcept
{
  const std::size_t shown = std::min(Hud::TOPICS_SHOWN, _panel.topics.size() - std::min(_panel.firstTopic, _panel.topics.size()));
  return (shown + Hud::TOPIC_COLUMNS - 1) / Hud::TOPIC_COLUMNS;
}

// How wide a topic card is: the research window's width shared among its columns.
float TopicWidth() noexcept
{
  const auto columns = static_cast<float>(Hud::TOPIC_COLUMNS);
  return (QUEUE_WINDOW_WIDTH - (2.0f * WINDOW_INSET) - ((columns - 1.0f) * CARD_SPACING)) / columns;
}

// A topic's effect as its card sets it, broken into lines that fit the card.
std::vector<std::string> EffectLines(const Hud::TopicCard& _topic, const Hud::TextMetrics& _metrics)
{
  return _metrics.Wrap(Hud::Typeface::Detail, _topic.effect, TopicWidth() - (2.0f * CARD_INSET));
}

// How tall every topic card is: as many lines as any topic needs under its name, its effect's and then its prerequisites'
// or its tier's, so that the cards keep their places as the window scrolls.
float TopicHeight(const Hud::ResearchPanel& _panel, const Hud::TextMetrics& _metrics)
{
  std::size_t lines = TOPIC_LEAST_LINES;
  for (const Hud::TopicCard& topic : _panel.topics)
    lines = std::max(lines, EffectLines(topic, _metrics).size() + std::max<std::size_t>(1, topic.needs.size()));
  return TOPIC_LINES_TOP + (static_cast<float>(lines) * TOPIC_LINE_UNITS) + TOPIC_FOOT_UNITS;
}

// The research window: the Research Lab, a card for each topic not researched or queued yet, a page of them at a time with
// arrows to scroll by a row, and its queue.
void LayResearch(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, const Hud::ResearchPanel& _panel,
                 Outpost::WindowManager::Point _corner, float _scale)
{
  const float topicHeight = TopicHeight(_panel, _metrics);
  const float cardsBottom = CardsBottom(TopicLines(_panel), topicHeight);
  (void)OpenWindow(_layout, Outpost::WindowKind::Research, std::string(), _corner, QUEUE_WINDOW_WIDTH,
                   QueueWindowHeight(cardsBottom, _panel.queue.size()) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.Text("RESEARCH", WINDOW_INSET, 11.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  Header(paint, _panel.lab, _panel.ore, QUEUE_WINDOW_WIDTH);

  paint.Text("TOPICS", WINDOW_INSET, SECTION_TOP, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  if (_panel.topics.size() > Hud::TOPICS_SHOWN)
  {
    const float arrowsLeft = QUEUE_WINDOW_WIDTH - WINDOW_INSET - (2.0f * SMALL_BUTTON_UNITS) - 4.0f;
    const float arrowsTop = SECTION_TOP - 4.0f;
    paint.SmallButton("<", arrowsLeft, arrowsTop, _panel.firstTopic > 0, {.kind = Hud::ActionKind::PreviousTopics});
    paint.SmallButton(">", arrowsLeft + SMALL_BUTTON_UNITS + 4.0f, arrowsTop, _panel.firstTopic + Hud::TOPICS_SHOWN < _panel.topics.size(),
                      {.kind = Hud::ActionKind::NextTopics});
  }
  const float width = TopicWidth();
  for (std::size_t i = _panel.firstTopic, shown = 0; i < _panel.topics.size() && shown < Hud::TOPICS_SHOWN; ++i, ++shown)
  {
    const Hud::TopicCard& topic = _panel.topics[i];
    const std::size_t line = shown / Hud::TOPIC_COLUMNS;
    const std::size_t column = shown % Hud::TOPIC_COLUMNS;
    const float left = WINDOW_INSET + (static_cast<float>(column) * (width + CARD_SPACING));
    const float cardTop = CARDS_TOP + (static_cast<float>(line) * (topicHeight + CARD_SPACING));
    const bool blocked = !topic.needs.empty();
    const Hud::Rect face = paint.Panel(left, cardTop, width, topicHeight,
                                       blocked         ? LOCKED_COLOR
                                       : topic.enabled ? CARD_COLOR
                                                       : FIELD_COLOR);
    if (blocked)
      paint.Panel(left, cardTop, width, topicHeight, LOCKED_HATCH_COLOR, Hud::Fill::Hatched);
    if (topic.enabled)
      paint.Press(face, topic.action);
    // A gateway, which opens its tier, is edged in gold.
    paint.Outline(left, cardTop, width, topicHeight, topic.gateway ? GOLD_COLOR : EDGE_COLOR);
    const DirectX::XMFLOAT4& color = topic.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR;
    const float costWidth = paint.DiamondAndFigureWidth(topic.cost, Hud::Typeface::Figure, 13.0f);
    paint.Text(paint.Fit(topic.name, Hud::Typeface::Name, width - (2.0f * CARD_INSET) - costWidth - FIT_GAP_UNITS), left + CARD_INSET,
               cardTop + 6.0f, color, Hud::Typeface::Name);
    paint.DiamondAndFigure(topic.cost, left + width - CARD_INSET, cardTop + 9.0f, Hud::Typeface::Figure, 13.0f,
                           topic.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
    float lineTop = cardTop + TOPIC_LINES_TOP;
    for (std::string& effect : EffectLines(topic, _metrics))
    {
      paint.Text(std::move(effect), left + CARD_INSET, lineTop, topic.enabled ? NUMBERS_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
      lineTop += TOPIC_LINE_UNITS;
    }
    // Each prerequisite on a line of its own, with its box: "NEEDS · IMPROVED EXTRACTION", then "+ HULL PLATING".
    for (std::size_t need = 0; need < topic.needs.size(); ++need)
    {
      paint.Sprite(Hud::Sprite::Checkbox, left + CARD_INSET, lineTop + 2.0f, 10.0f, AMBER_COLOR);
      const std::string needs = need == 0 ? std::format("NEEDS{}{}", DOT, topic.needs[need]) : std::format("+ {}", topic.needs[need]);
      paint.Text(paint.Fit(needs, Hud::Typeface::Label, width - 28.0f - CARD_INSET), left + 28.0f, lineTop, AMBER_COLOR, Hud::Typeface::Label);
      lineTop += TOPIC_LINE_UNITS;
    }
    if (!blocked)
      paint.Text(topic.time, left + CARD_INSET, lineTop, topic.enabled ? LABEL_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
  }
  QueueRows(paint, _panel.queue, QUEUE_WINDOW_WIDTH, QueueTop(cardsBottom));
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
  // A domination says so: the side that ran out of tickets held less of the map (Phase 2 design §8).
  const std::string_view how = _newest.ending == MatchEnding::Domination ? "By domination. " : "";
  return Outcome{.title = std::string(title), .detail = std::format("{}Match length {}", how, MinutesAndSeconds(seconds))};
}

Hud::Layout Hud::LayMenu(const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const float scale = Scale(_widthPixels, _heightPixels);
  Layout layout{.fontPixels = FONT_UNITS * scale, .panels = {}, .texts = {}, .actions = {}, .minimap = {}, .mapSizeMeters = 0.0f};
  const std::array<Button, 2> buttons{
    {{.label = "Start skirmish", .action = {.kind = ActionKind::StartSkirmish}}, {.label = "Quit", .action = {.kind = ActionKind::Quit}}}};
  const auto count = static_cast<float>(buttons.size());
  const float heightUnits =
    (2.0f * PADDING) + TITLE_LINE_UNITS + NAME_LINE_UNITS + BUTTON_GAP + (count * BUTTON_HEIGHT) + ((count - 1.0f) * BUTTON_GAP);
  const WindowManager::Point corner{.xUnits = (static_cast<float>(_widthPixels) / scale / 2.0f) - (MENU_WIDTH / 2.0f),
                                    .yUnits = (static_cast<float>(_heightPixels) / scale / 2.0f) - (heightUnits / 2.0f)};
  (void)Frame(layout, corner.xUnits * scale, corner.yUnits * scale, MENU_WIDTH * scale, heightUnits * scale, scale);
  Painter paint(layout, _metrics, corner, scale);
  paint.Text("Outpost Commander", PADDING, PADDING, GOLD_COLOR, Typeface::Title);
  paint.Text("A skirmish against the AI", PADDING, PADDING + TITLE_LINE_UNITS, DIM_TEXT_COLOR, Typeface::Name);
  float top = PADDING + TITLE_LINE_UNITS + NAME_LINE_UNITS + BUTTON_GAP;
  for (const Button& button : buttons)
  {
    AddButton(layout, _metrics, scale, paint.Area(PADDING, top, MENU_WIDTH - (2.0f * PADDING), BUTTON_HEIGHT, CARD_COLOR), button);
    top += BUTTON_HEIGHT + BUTTON_GAP;
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

  const auto sideOf = [&_newest](PlayerId _owner) {
    return !_owner.IsValid() ? Side::Neutral : _owner == _newest.player ? Side::Own : Side::Enemy;
  };
  if (!_newest.sectors.empty())
  {
    Territory territory{.nodes = static_cast<std::int32_t>(_newest.sectors.size())};
    content.sectors.reserve(_newest.sectors.size());
    for (const SectorView& sector : _newest.sectors)
    {
      const Side side = sideOf(sector.holder);
      territory.ownNodes += side == Side::Own ? 1 : 0;
      territory.enemyNodes += side == Side::Enemy ? 1 : 0;
      content.sectors.push_back({.minXMeters = sector.minXMeters,
                                 .maxXMeters = sector.maxXMeters,
                                 .minZMeters = sector.minZMeters,
                                 .maxZMeters = sector.maxZMeters,
                                 .side = side,
                                 .suppressed = sector.suppressed});
    }
    // Domination's tickets, which both players see (ADR-057).
    for (const TicketsView& tickets : _newest.tickets)
    {
      if (tickets.player == _newest.player)
        territory.ownTickets = tickets.tickets;
      else
        territory.enemyTickets = tickets.tickets;
    }
    content.territory = territory;
  }

  content.marks.reserve(_entities.size());
  for (const EntityView& entity : _entities)
  {
    const Side side = sideOf(entity.owner);
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
      content.selectionHealth = HealthShare(structure->hitPointsHundredths, structure->maxHitPointsHundredths);
      // A Mining Rig's asteroid's Ore left, as far as the player knows it (Phase 1 design §8).
      if (structure->structure == StructureKind::MiningRig && structure->oreReserveHundredths.has_value())
      {
        content.selection.push_back(*structure->oreReserveHundredths > 0
                                      ? std::format("Ore left {}", WithThousands(WholePoints(*structure->oreReserveHundredths)))
                                      : std::string("Ore run out: it earns a trickle"));
      }
      // The sector a Relay or a rig of the player's stands in, and what its sector earns (ADR-056).
      const SectorView* sector = FindSector(_newest.sectors, structure->position);
      const bool ownTerritorial = structure->owner == _newest.player &&
                                  (structure->structure == StructureKind::Relay || structure->structure == StructureKind::MiningRig);
      if (sector != nullptr && ownTerritorial)
      {
        if (sector->holder != _newest.player)
          content.selection.push_back(std::format("{}: not held, it earns nothing", sector->nameUtf8));
        else if (sector->suppressed)
          content.selection.push_back(std::format("{}: suppressed, it earns nothing", sector->nameUtf8));
        else if (sector->cutOff)
          content.selection.push_back(std::format("{}: cut off, it earns half", sector->nameUtf8));
        else
          content.selection.push_back(std::format("{}: held", sector->nameUtf8));
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
  bool holding = false;
  bool patrolling = false;
  for (const EntityId id : _selected)
  {
    const auto ship = std::ranges::find(_entities, id, &EntityView::id);
    if (ship == _entities.end() || ship->kind != EntityKind::Ship)
      continue;
    constructors = constructors || ship->role == ShipRole::Constructor;
    holding = holding || ship->standing == StandingOrder::HoldSector;
    patrolling = patrolling || ship->standing == StandingOrder::Patrol;
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
      content.selection.push_back(std::format("{} {} {}", byDesign[i].second, TIMES, nameOf(byDesign[i].first)));
    if (byDesign.size() > SELECTION_DESIGN_LINES)
      content.selection.push_back(std::format("and {} more designs", byDesign.size() - SELECTION_DESIGN_LINES));
  }
  content.selection.push_back(
    std::format("Hit points {} / {}", WithThousands(WholePoints(hitPoints)), WithThousands(WholePoints(maxHitPoints))));
  content.selectionHealth = HealthShare(hitPoints, maxHitPoints);
  // A standing order the selection keeps (ADR-059).
  if (holding)
    content.selection.emplace_back("Holding a sector");
  if (patrolling)
    content.selection.emplace_back("On patrol");

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
      // A Relay holds a sector, and a map without them has none to hold (ADR-056).
      if (!type.buildable || (type.structure == StructureKind::Relay && _newest.sectors.empty()))
        continue;
      const bool allowed = !(type.structure == StructureKind::ResearchLab && hasLab);
      content.buttons.push_back({.label = std::format("{}|{}", type.nameUtf8, type.cost),
                                 .action = {.kind = ActionKind::Build, .structure = type.structure},
                                 .enabled = allowed && _newest.ore >= type.cost,
                                 .note = allowed ? std::string() : std::string("ONE PER PLAYER")});
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
  panel.options.reserve(_newest.designs.size());
  for (const DesignView& design : _newest.designs)
  {
    panel.options.push_back({.name = design.nameUtf8,
                             .detail = DesignCodeOf(_newest, design),
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
    std::vector<std::string> needs;
    for (const ResearchTopicId prerequisite : topic.prerequisites)
    {
      if (known(prerequisite))
        continue;
      const ResearchTopicView* view = topicOf(prerequisite);
      needs.push_back(view != nullptr ? Capitals(view->nameUtf8) : std::string("?"));
    }
    const bool enabled = canResearch && needs.empty() && _newest.ore >= topic.cost;
    panel.topics.push_back(
      {.name = topic.nameUtf8,
       .effect = topic.effectUtf8,
       .cost = topic.cost,
       .time = std::format("TIER {}{}{} s", topic.tier, DOT, Tenths(topic.researchSeconds)),
       .needs = std::move(needs),
       .action = {.kind = ActionKind::Research, .producer = found != nullptr ? found->id : EntityId{}, .topic = topic.id},
       .enabled = enabled,
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

Hud::Layout Hud::Lay(const Content& _content, const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                     std::span<const PlanePosition> _view, const WindowManager* _windows)
{
  const float scale = Scale(_widthPixels, _heightPixels);
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  Layout layout{.fontPixels = FONT_UNITS * scale, .panels = {}, .texts = {}, .actions = {}, .minimap = {}, .mapSizeMeters = 0.0f};

  // Every panel of the HUD is in the windows' look (ADR-043), and is laid out in reference units from its own top-left
  // corner, as a window's content is.
  const float screenWidthUnits = width / scale;
  const float screenHeightUnits = height / scale;
  const auto frame = [&layout, &_metrics, scale](WindowManager::Point _corner, float _widthUnits, float _heightUnits)
  {
    (void)Frame(layout, _corner.xUnits * scale, _corner.yUnits * scale, _widthUnits * scale, _heightUnits * scale, scale);
    return Painter(layout, _metrics, _corner, scale);
  };
  const auto addButton = [&layout, &_metrics, scale](const Rect& _area, const Button& _button)
  { AddButton(layout, _metrics, scale, _area, _button); };

  // Top-left anchor: the Ore, as the windows write it, and what the rigs earn each second, in the warning's color when they
  // earn nothing, since then nothing the player spends comes back.
  {
    Painter paint = frame({.xUnits = MARGIN, .yUnits = MARGIN}, ORE_PANEL_WIDTH, ORE_PANEL_HEIGHT);
    paint.DiamondAndFigureFrom(_content.ore, PADDING, (ORE_PANEL_HEIGHT - TITLE_LINE_UNITS) / 2.0f, Typeface::Title, TITLE_FACE_UNITS,
                               GOLD_COLOR);
    const std::int32_t income = _content.oreIncomeHundredthsPerSecond;
    const std::string incomeText = income % HUNDREDTHS == 0 ? std::format("+{}/s", income / HUNDREDTHS)
                                                            : std::format("+{:.1f}/s", static_cast<double>(income) / HUNDREDTHS);
    paint.RightText(incomeText, ORE_PANEL_WIDTH - PADDING, ((ORE_PANEL_HEIGHT - FIGURE_LINE_UNITS) / 2.0f) + 2.0f,
                    income > 0 ? NUMBERS_COLOR : WARNING_COLOR, Typeface::Figure);
  }

  // Under the Ore: the research under way.
  const float underOreUnits = MARGIN + ORE_PANEL_HEIGHT + RESEARCH_PANEL_GAP;

  // Under the research's place: the nodes each side holds, and each side's tickets, the player's in its color and the
  // enemy's in theirs (ADR-056, ADR-057).
  if (_content.territory.has_value())
  {
    const Territory& territory = *_content.territory;
    const bool tickets = territory.ownTickets.has_value() && territory.enemyTickets.has_value();
    const float heightUnits = (2.0f * TERRITORY_INSET_UNITS) + (tickets ? 2.0f : 1.0f) * NAME_LINE_UNITS;
    Painter paint =
      frame({.xUnits = MARGIN, .yUnits = underOreUnits + ORE_PANEL_HEIGHT + RESEARCH_PANEL_GAP}, TERRITORY_PANEL_WIDTH, heightUnits);
    constexpr float ADVANCE = 13.0f * MONO_ADVANCE;
    const auto row = [&](std::string _label, const std::string& _own, const std::string& _enemy, float _top)
    {
      paint.Text(std::move(_label), PADDING, _top, TEXT_COLOR, Typeface::Name);
      const float figureTop = _top + ((NAME_LINE_UNITS - FIGURE_LINE_UNITS) / 2.0f) + 2.0f;
      paint.RightText(_enemy, TERRITORY_PANEL_WIDTH - PADDING, figureTop, ENEMY_COLOR, Typeface::Figure, ADVANCE);
      paint.RightText(std::format("{} : ", _own), TERRITORY_PANEL_WIDTH - PADDING - (CharactersOf(_enemy) * ADVANCE), figureTop, OWN_COLOR,
                      Typeface::Figure, ADVANCE);
    };
    row(std::format("Nodes of {}", territory.nodes), std::to_string(territory.ownNodes), std::to_string(territory.enemyNodes),
        TERRITORY_INSET_UNITS);
    if (tickets)
    {
      row("Tickets", WithThousands(territory.ownTickets.value_or(0)), WithThousands(territory.enemyTickets.value_or(0)),
          TERRITORY_INSET_UNITS + NAME_LINE_UNITS);
    }
  }

  // Under the territory: the alerts, newest first, in the warning's color (ADR-059).
  if (!_content.alerts.empty())
  {
    const bool tickets = _content.territory.has_value() && _content.territory->ownTickets.has_value();
    const float territoryUnits = _content.territory.has_value()
                                   ? (2.0f * TERRITORY_INSET_UNITS) + ((tickets ? 2.0f : 1.0f) * NAME_LINE_UNITS) + RESEARCH_PANEL_GAP
                                   : 0.0f;
    const float topUnits = underOreUnits + ORE_PANEL_HEIGHT + RESEARCH_PANEL_GAP + territoryUnits;
    const float heightUnits = (2.0f * TERRITORY_INSET_UNITS) + (static_cast<float>(_content.alerts.size()) * NAME_LINE_UNITS);
    Painter paint = frame({.xUnits = MARGIN, .yUnits = topUnits}, ALERT_PANEL_WIDTH, heightUnits);
    for (size_t line = 0; line < _content.alerts.size(); ++line)
    {
      paint.Text(_content.alerts[line].first, PADDING, TERRITORY_INSET_UNITS + (static_cast<float>(line) * NAME_LINE_UNITS),
                 line == 0 ? WARNING_COLOR : TEXT_COLOR, Typeface::Name);
    }
  }
  if (!_content.research.empty())
  {
    Painter paint = frame({.xUnits = MARGIN, .yUnits = underOreUnits}, RESEARCH_PANEL_WIDTH, ORE_PANEL_HEIGHT);
    paint.Text(paint.Fit(_content.research, Typeface::Name, RESEARCH_PANEL_WIDTH - (2.0f * PADDING)), PADDING,
               (ORE_PANEL_HEIGHT - NAME_LINE_UNITS) / 2.0f, TEXT_COLOR, Typeface::Name);
  }

  // Top-middle anchor: what a click on the ground will do.
  if (!_content.hint.empty())
  {
    Painter paint = frame({.xUnits = (screenWidthUnits - HINT_PANEL_WIDTH) / 2.0f, .yUnits = MARGIN}, HINT_PANEL_WIDTH, ORE_PANEL_HEIGHT);
    paint.Text(paint.Fit(_content.hint, Typeface::Name, HINT_PANEL_WIDTH - (2.0f * PADDING)), PADDING,
               (ORE_PANEL_HEIGHT - NAME_LINE_UNITS) / 2.0f, TEXT_COLOR, Typeface::Name);
  }

  // Top-middle anchor, under the hint: how the match ended, and the way back to the menu.
  if (_content.outcome.has_value())
  {
    const float heightUnits = (2.0f * PADDING) + TITLE_LINE_UNITS + NAME_LINE_UNITS + BUTTON_GAP + BUTTON_HEIGHT;
    Painter paint = frame({.xUnits = (screenWidthUnits - BANNER_WIDTH) / 2.0f, .yUnits = underOreUnits}, BANNER_WIDTH, heightUnits);
    const float room = BANNER_WIDTH - (2.0f * PADDING);
    paint.Text(paint.Fit(_content.outcome->title, Typeface::Title, room), PADDING, PADDING, GOLD_COLOR, Typeface::Title);
    paint.Text(paint.Fit(_content.outcome->detail, Typeface::Name, room), PADDING, PADDING + TITLE_LINE_UNITS, TEXT_COLOR, Typeface::Name);
    addButton(paint.Area(PADDING, PADDING + TITLE_LINE_UNITS + NAME_LINE_UNITS + BUTTON_GAP, BANNER_WIDTH - (2.0f * PADDING), BUTTON_HEIGHT,
                         CARD_COLOR),
              {.label = "Back to menu", .action = {.kind = ActionKind::BackToMenu}});
  }

  // Bottom-middle anchor: the selection, its first line in the title face and the rest in the name face, in a panel as
  // wide as its longest line, within its bounds; a line longer than the widest panel holds is cut short.
  if (!_content.selection.empty())
  {
    const auto faceOf = [](size_t _line) { return _line == 0 ? Typeface::Title : Typeface::Name; };
    float textUnits = 0.0f;
    for (size_t line = 0; line < _content.selection.size(); ++line)
      textUnits = std::max(textUnits, _metrics.Width(faceOf(line), _content.selection[line]));
    const float widthUnits = std::clamp(textUnits + (2.0f * PADDING), SELECTION_PANEL_MIN_WIDTH, SELECTION_PANEL_WIDTH);
    const float linesUnits = TITLE_LINE_UNITS + (NAME_LINE_UNITS * static_cast<float>(_content.selection.size() - 1));
    const float barUnits = _content.selectionHealth.has_value() ? HEALTH_BAR_GAP_UNITS + HEALTH_BAR_UNITS : 0.0f;
    const float heightUnits = (2.0f * PADDING) + linesUnits + barUnits;
    Painter paint = frame({.xUnits = (screenWidthUnits - widthUnits) / 2.0f, .yUnits = screenHeightUnits - MARGIN - heightUnits},
                          widthUnits, heightUnits);
    const float room = widthUnits - (2.0f * PADDING);
    paint.Text(paint.Fit(_content.selection.front(), Typeface::Title, room), PADDING, PADDING, TEXT_COLOR, Typeface::Title);
    for (size_t line = 1; line < _content.selection.size(); ++line)
    {
      paint.Text(paint.Fit(_content.selection[line], Typeface::Name, room), PADDING,
                 PADDING + TITLE_LINE_UNITS + (NAME_LINE_UNITS * static_cast<float>(line - 1)), NUMBERS_COLOR, Typeface::Name);
    }
    if (_content.selectionHealth.has_value())
    {
      const float share = std::clamp(*_content.selectionHealth, 0.0f, 1.0f);
      const float barTop = PADDING + linesUnits + HEALTH_BAR_GAP_UNITS;
      const float barWidth = widthUnits - (2.0f * PADDING);
      paint.Panel(PADDING, barTop, barWidth, HEALTH_BAR_UNITS, BAR_TRACK_COLOR);
      if (share > 0.0f)
      {
        paint.Panel(PADDING, barTop, barWidth * share, HEALTH_BAR_UNITS,
                    share > HEALTH_HURT_SHARE  ? GOOD_COLOR
                    : share > HEALTH_LOW_SHARE ? FAIR_COLOR
                                               : POOR_COLOR);
      }
    }
  }

  // Bottom-right anchor: the buttons, stacked upward from the corner. A label holds the name and the cost, split at '|'.
  if (!_content.buttons.empty())
  {
    const auto count = static_cast<float>(_content.buttons.size());
    const float heightUnits = (2.0f * PADDING) + (count * BUTTON_HEIGHT) + ((count - 1.0f) * BUTTON_GAP);
    Painter paint = frame({.xUnits = screenWidthUnits - MARGIN - BUTTON_PANEL_WIDTH, .yUnits = screenHeightUnits - MARGIN - heightUnits},
                          BUTTON_PANEL_WIDTH, heightUnits);
    for (size_t i = 0; i < _content.buttons.size(); ++i)
    {
      addButton(paint.Area(PADDING, PADDING + (static_cast<float>(i) * (BUTTON_HEIGHT + BUTTON_GAP)), BUTTON_PANEL_WIDTH - (2.0f * PADDING),
                           BUTTON_HEIGHT, CARD_COLOR),
                _content.buttons[i]);
    }
  }

  // Bottom-left anchor: the minimap, with every mark and the camera's view.
  if (_content.mapSizeMeters > 0.0f)
  {
    const float size = MINIMAP_SIZE * scale;
    const float left = MARGIN * scale;
    const float top = height - ((MARGIN + MINIMAP_SIZE) * scale);
    (void)Frame(layout, left, top, size, size, scale);
    const float inner = (MINIMAP_SIZE - (2.0f * MINIMAP_PADDING)) * scale;
    layout.minimap = {left + (MINIMAP_PADDING * scale), top + (MINIMAP_PADDING * scale), inner, inner, MAP_COLOR};
    layout.mapSizeMeters = _content.mapSizeMeters;
    layout.panels.push_back(layout.minimap);

    const float pixelsPerMeter = inner / _content.mapSizeMeters;
    // A sector's square on the minimap, in pixels.
    const auto sectorRect = [&layout](const SectorMark& _sector, DirectX::XMFLOAT4 _color, Fill _fill)
    {
      const DirectX::XMFLOAT2 low = layout.MinimapPixelOf({.xMeters = _sector.minXMeters, .zMeters = _sector.maxZMeters});
      const DirectX::XMFLOAT2 high = layout.MinimapPixelOf({.xMeters = _sector.maxXMeters, .zMeters = _sector.minZMeters});
      return Rect{low.x, low.y, high.x - low.x, high.y - low.y, _color, _fill};
    };
    const auto sideColor = [](Side _side, float _alpha)
    {
      DirectX::XMFLOAT4 color = _side == Side::Own ? OWN_COLOR : ENEMY_COLOR;
      color.w = _alpha;
      return color;
    };
    for (const SectorMark& sector : _content.sectors)
    {
      if (sector.side != Side::Neutral)
        layout.panels.push_back(sectorRect(sector, sideColor(sector.side, SECTOR_WASH_ALPHA), Fill::Solid));
    }
    // The neutral marks first, so that a rig's shows over its asteroid's.
    std::vector<const Mark*> marks;
    marks.reserve(_content.marks.size());
    for (const bool sided : {false, true})
    {
      for (const Mark& mark : _content.marks)
      {
        if ((mark.side != Side::Neutral) == sided)
          marks.push_back(&mark);
      }
    }
    for (const Mark* marked : marks)
    {
      const Mark& mark = *marked;
      const float smallest = (mark.kind == EntityKind::Ship       ? SHIP_MARK_UNITS
                              : mark.kind == EntityKind::Asteroid ? ORE_MARK_UNITS
                                                                  : STRUCTURE_MARK_UNITS) *
                             scale;
      const float side = std::max(smallest, 2.0f * mark.radiusMeters * pixelsPerMeter);
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(mark.position);
      const DirectX::XMFLOAT4& color = mark.dry                                 ? DRY_COLOR
                                       : mark.side == Side::Own                 ? OWN_COLOR
                                       : mark.side == Side::Enemy               ? ENEMY_COLOR
                                       : mark.kind == EntityKind::AsteroidField ? ASTEROID_FIELD_COLOR
                                       : mark.kind == EntityKind::Asteroid      ? ORE_ASTEROID_COLOR
                                                                                : NEUTRAL_COLOR;
      layout.panels.push_back({at.x - (side / 2.0f), at.y - (side / 2.0f), side, side, color});
    }

    // The fog over the marks (ADR-024), filled by the client from the fog's texture across the whole map (ADR-052).
    if (_content.fog)
    {
      layout.panels.push_back(
        {layout.minimap.left, layout.minimap.top, layout.minimap.width, layout.minimap.height, {0.0f, 0.0f, 0.0f, 1.0f}, Fill::Fog});
    }

    // Over the fog, since both sides know who holds what: each held sector's outline, and stripes over a suppressed one.
    const float sectorLine = SECTOR_LINE_UNITS * scale;
    for (const SectorMark& sector : _content.sectors)
    {
      if (sector.side == Side::Neutral)
        continue;
      const Rect area = sectorRect(sector, sideColor(sector.side, SECTOR_OUTLINE_ALPHA), Fill::Solid);
      layout.panels.push_back({area.left, area.top, area.width, sectorLine, area.color});
      layout.panels.push_back({area.left, area.top + area.height - sectorLine, area.width, sectorLine, area.color});
      layout.panels.push_back({area.left, area.top, sectorLine, area.height, area.color});
      layout.panels.push_back({area.left + area.width - sectorLine, area.top, sectorLine, area.height, area.color});
      if (sector.suppressed)
        layout.panels.push_back(sectorRect(sector, sideColor(sector.side, SECTOR_HATCH_ALPHA), Fill::Hatched));
    }

    // Each alert's place, as an outlined square over everything else (ADR-059).
    for (const auto& [text, position] : _content.alerts)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(position);
      const float half = ALERT_MARK_UNITS * scale / 2.0f;
      const float side = 2.0f * half;
      layout.panels.push_back({at.x - half, at.y - half, side, sectorLine, WARNING_COLOR});
      layout.panels.push_back({at.x - half, at.y + half - sectorLine, side, sectorLine, WARNING_COLOR});
      layout.panels.push_back({at.x - half, at.y - half, sectorLine, side, WARNING_COLOR});
      layout.panels.push_back({at.x + half - sectorLine, at.y - half, sectorLine, side, WARNING_COLOR});
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
      LayDesigner(layout, _metrics, *_content.designer, corner, scale);
    }
    // Production and research, at first side by side under the Ore and the research line, clear of the designer.
    else if (kind == WindowKind::Production && _content.production.has_value())
    {
      LayProduction(layout, _metrics, *_content.production, place(kind, {.xUnits = MARGIN, .yUnits = WINDOWS_TOP}, QUEUE_WINDOW_WIDTH),
                    scale);
    }
    else if (kind == WindowKind::Research && _content.laboratory.has_value())
    {
      const WindowManager::Point corner =
        place(kind, {.xUnits = (2.0f * MARGIN) + QUEUE_WINDOW_WIDTH, .yUnits = WINDOWS_TOP}, QUEUE_WINDOW_WIDTH);
      LayResearch(layout, _metrics, *_content.laboratory, corner, scale);
    }
  }
  return layout;
}
