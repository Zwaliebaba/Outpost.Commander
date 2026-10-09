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
// map reads without a legend: blue is the player's, red the enemy's, violet the pirates' (ADR-073), and gold is ore
// (ADR-043). An ore asteroid's mark is an outlined square, so that it differs from an enemy's filled one in shape as well
// as in brightness (ADR-068). The camera's view is a light outline.
constexpr DirectX::XMFLOAT4 MAP_COLOR{0.02f, 0.04f, 0.07f, 0.95f};
constexpr DirectX::XMFLOAT4 OWN_COLOR{0.35f, 0.65f, 1.0f, 1.0f};
constexpr DirectX::XMFLOAT4 ENEMY_COLOR{1.0f, 0.38f, 0.25f, 1.0f};
constexpr DirectX::XMFLOAT4 PIRATE_COLOR{0.78f, 0.45f, 1.0f, 1.0f};
// A derelict, light gray, a filled square like a structure's, whoever's it was (ADR-074).
constexpr DirectX::XMFLOAT4 DERELICT_COLOR{0.62f, 0.62f, 0.66f, 1.0f};
constexpr DirectX::XMFLOAT4 NEUTRAL_COLOR{0.45f, 0.42f, 0.4f, 1.0f};
constexpr DirectX::XMFLOAT4 ORE_ASTEROID_COLOR{0.42f, 0.23f, 0.02f, 1.0f};
// An ore asteroid that has run out: darker than one with ore, and warmed toward rust, at 3:1 against the map (ADR-068).
constexpr DirectX::XMFLOAT4 DRY_COLOR{0.32f, 0.2f, 0.147f, 1.0f};
// An asteroid field is only in the way, so it is drawn darker than an ore asteroid, which is worth going to, though a
// field's square is the larger (ADR-040); dark enough that the fields stand back from the ore (ADR-046), and at 2:1
// against the map, so that it is seen (ADR-068).
constexpr DirectX::XMFLOAT4 ASTEROID_FIELD_COLOR{0.13f, 0.13f, 0.14f, 1.0f};
constexpr DirectX::XMFLOAT4 VIEW_COLOR{0.85f, 0.9f, 1.0f, 0.8f};
// A sector on the minimap (ADR-056): a faint wash of its holder's color under the marks, an outline of it over the fog,
// and stripes of it while suppressed.
constexpr float SECTOR_WASH_ALPHA = 0.12f;
constexpr float SECTOR_OUTLINE_ALPHA = 0.55f;
constexpr float SECTOR_HATCH_ALPHA = 0.35f;
constexpr float SECTOR_LINE_UNITS = 1.0f;
// The territory over the fog (interface plan 2, task UI1.4): every sector's border in LATTICE_COLOR, at 2:1 against the map,
// under the held sectors' outlines; a cut-off sector's outline in dashes of CUT_OFF_DASH_UNITS with CUT_OFF_GAP_UNITS between
// them, where a suppressed one keeps its hatching; a dot of NODE_MARK_UNITS on every node, in its holder's color or
// FREE_NODE_COLOR; and round a node the player could claim now an outline of CLAIMABLE_MARK_UNITS in its own color, a dot in
// a square where an ore asteroid's outline is empty.
constexpr DirectX::XMFLOAT4 LATTICE_COLOR{0.11f, 0.13f, 0.17f, 1.0f};
constexpr float CUT_OFF_DASH_UNITS = 6.0f;
constexpr float CUT_OFF_GAP_UNITS = 4.0f;
constexpr float NODE_MARK_UNITS = 4.0f;
constexpr DirectX::XMFLOAT4 FREE_NODE_COLOR{0.45f, 0.48f, 0.55f, 1.0f};
constexpr float CLAIMABLE_MARK_UNITS = 12.0f;
// A structure or derelict the player only remembers is a cross MEMORY_MARK_UNITS across, its arms MEMORY_ARM_UNITS thick,
// drawn over the fog in its side's color (interface plan 2, task UI1.2).
constexpr float MEMORY_MARK_UNITS = 8.0f;
constexpr float MEMORY_ARM_UNITS = 2.0f;

// Everything below in reference units.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
// A line of text in the title face, and in the name face, as the HUD's panels set them; a figure's line (ADR-062).
constexpr float TITLE_LINE_UNITS = 30.0f;
constexpr float NAME_LINE_UNITS = 22.0f;
constexpr float FIGURE_LINE_UNITS = 17.0f;

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
// The bar along a button's foot that shows the work it started, and its inset from the button's edges.
constexpr float BUTTON_BAR_UNITS = 3.0f;
constexpr float BUTTON_BAR_INSET = 2.0f;

// The minimap, a square anchored to the bottom-left corner, with the map drawn inside its padding: about 36 m a unit on the
// 10 km map (ADR-068).
constexpr float MINIMAP_SIZE = 300.0f;
constexpr float MINIMAP_PADDING = 8.0f;
// The smallest a mark is drawn, and how wide the view's outline is, so that both stay visible. An ore asteroid's is the
// largest, since it is what the player looks for on the map; a rig's mark is drawn over its asteroid's (ADR-046).
constexpr float SHIP_MARK_UNITS = 3.0f;
constexpr float STRUCTURE_MARK_UNITS = 6.0f;
constexpr float ORE_MARK_UNITS = 8.0f;
constexpr float VIEW_LINE_UNITS = 1.5f;
// The line an ore asteroid's mark is outlined with (ADR-068).
constexpr float ORE_OUTLINE_UNITS = 2.0f;

// The hint, anchored to the top edge's middle.
constexpr float HINT_PANEL_WIDTH = 640.0f;

// The status panel under the Ore (ADR-066): as wide as its longest line within these, and the room above and below its
// lines, which the territory's and the alerts' panels keep too. Its place is kept for two lines, whether or not it has them,
// so that what stands under it does not move as a Lab or a Shipyard is finished.
constexpr float STATUS_PANEL_MIN_WIDTH = 260.0f;
constexpr float STATUS_PANEL_WIDTH = 560.0f;
constexpr float STATUS_LINES_KEPT = 2.0f;
constexpr float PANEL_GAP = 8.0f;
constexpr float TERRITORY_INSET_UNITS = 10.0f;
constexpr float STATUS_PANEL_KEPT_UNITS = (2.0f * TERRITORY_INSET_UNITS) + (STATUS_LINES_KEPT * NAME_LINE_UNITS);
// The territory, under the status panel's place (ADR-056, ADR-057).
constexpr float TERRITORY_PANEL_WIDTH = 260.0f;
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

// The name of a Research Lab's topic.
std::string TopicNameOf(const Outpost::Snapshot& _newest, Outpost::ResearchTopicId _topic)
{
  const auto topic = std::ranges::find(_newest.research, _topic, &Outpost::ResearchTopicView::id);
  return topic != _newest.research.end() ? topic->nameUtf8 : std::string("Research");
}

// The name of the front job of a producer's queue, which is not empty.
std::string FrontJobNameOf(const Outpost::Snapshot& _newest, const Outpost::EntityView& _producer)
{
  const Outpost::JobView& job = _producer.queue.front();
  return DesignNameOf(_newest, job.role == Outpost::ShipRole::Constructor ? Outpost::DesignId{} : job.design);
}

// Whether _producer's front job waits for its player's fleet cap rather than for Ore (Phase 4 design §5): a warship not yet
// started whose hull takes more command points than are left under the cap.
bool WaitsOnFleetCap(const Outpost::Snapshot& _newest, const Outpost::EntityView& _producer)
{
  if (_newest.fleetCap <= 0 || _producer.queue.empty() || _producer.jobPermille > 0 ||
      _producer.queue.front().role != Outpost::ShipRole::Warship)
    return false;
  const auto design = std::ranges::find(_newest.designs, _producer.queue.front().design, &Outpost::DesignView::id);
  if (design == _newest.designs.end())
    return false;
  const auto hull = std::ranges::find(_newest.hulls, design->hull, &Outpost::HullView::id);
  return hull != _newest.hulls.end() && _newest.commandPoints + hull->commandPoints > _newest.fleetCap;
}

// What a producer or the Research Lab is doing, for its selection panel (ADR-066): "Building Swarm · 62% · +2 queued",
// "Building Swarm · waiting for Ore", or "Idle" with nothing queued. _verb names the work and _front the front job, and
// _waitingFor what a job not yet started waits for: Ore, or the fleet cap (Phase 4 design §5).
std::string WorkLine(std::string_view _verb, const std::string& _front, std::int32_t _permille, std::size_t _queued,
                     std::string_view _waitingFor = "Ore")
{
  if (_queued == 0)
    return "Idle";
  std::string line = _permille > 0 ? std::format("{} {}{}{}%", _verb, _front, DOT, _permille / 10)
                                   : std::format("{} {}{}waiting for {}", _verb, _front, DOT, _waitingFor);
  if (_queued > 1)
    line += std::format("{}+{} queued", DOT, _queued - 1);
  return line;
}

// The status panel's lines (ADR-066): what the player's finished Research Lab is doing, and how many of its finished
// Shipyards are building, waiting for Ore, waiting for the fleet cap and idle, and the fleet against its cap (Phase 4
// design §5). A line that reports an idle Lab or Shipyard says IDLE. A click on the Lab's line opens research, and on the
// Shipyards' opens production at the first idle Shipyard, or at the first if none is.
std::vector<Hud::StatusLine> StatusLinesOf(const Outpost::Snapshot& _newest, std::span<const Outpost::EntityView> _entities)
{
  const auto own = [&_newest](const Outpost::EntityView& _entity, Outpost::StructureKind _kind)
  {
    return _entity.kind == Outpost::EntityKind::Structure && _entity.structure == _kind && _entity.owner == _newest.player &&
           !_entity.remembered && _entity.builtPermille >= Outpost::PERMILLE;
  };
  std::vector<Hud::StatusLine> lines;
  const auto lab = std::ranges::find_if(_entities, [&own](const Outpost::EntityView& _entity)
                                        { return own(_entity, Outpost::StructureKind::ResearchLab); });
  if (lab != _entities.end())
  {
    const Hud::Action open{.kind = Hud::ActionKind::OpenResearch, .producer = lab->id};
    if (lab->research.empty())
      lines.push_back({.text = "Research Lab IDLE", .idle = true, .action = open});
    else
    {
      const std::string name = TopicNameOf(_newest, lab->research.front());
      std::string text = lab->jobPermille > 0 ? std::format("Researching {}, {}%", name, lab->jobPermille / 10)
                                              : std::format("Researching {}, waiting for Ore", name);
      if (lab->research.size() > 1)
        text += std::format(" (+{} queued)", lab->research.size() - 1);
      lines.push_back({.text = std::move(text), .action = open});
    }
  }

  std::vector<const Outpost::EntityView*> shipyards;
  for (const Outpost::EntityView& entity : _entities)
  {
    if (own(entity, Outpost::StructureKind::Shipyard))
      shipyards.push_back(&entity);
  }
  if (shipyards.empty())
    return lines;
  std::ranges::sort(shipyards, {}, &Outpost::EntityView::shipyardNumber);
  std::size_t building = 0;
  std::size_t waiting = 0;
  std::size_t capped = 0;
  const Outpost::EntityView* firstIdle = nullptr;
  std::size_t idle = 0;
  for (const Outpost::EntityView* shipyard : shipyards)
  {
    if (shipyard->queue.empty())
    {
      firstIdle = firstIdle != nullptr ? firstIdle : shipyard;
      ++idle;
    }
    else if (shipyard->jobPermille > 0)
      ++building;
    else if (WaitsOnFleetCap(_newest, *shipyard))
      ++capped;
    else
      ++waiting;
  }
  std::string text = std::format("Shipyards: {} building", building);
  if (waiting > 0)
    text += std::format(", {} waiting for Ore", waiting);
  if (capped > 0)
    text += std::format(", {} waiting for the fleet cap", capped);
  if (idle > 0)
    text += std::format(", {} IDLE", idle);
  if (_newest.fleetCap > 0)
    text += std::format("{}fleet {} / {}", DOT, _newest.commandPoints, _newest.fleetCap);
  lines.push_back(
    {.text = std::move(text),
     .idle = idle > 0,
     .action = {.kind = Hud::ActionKind::OpenProduction, .producer = (firstIdle != nullptr ? firstIdle : shipyards.front())->id}});
  return lines;
}
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

// _text with its first letter a capital, to open a line.
std::string Capitalized(std::string _text)
{
  if (!_text.empty())
    _text.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(_text.front())));
  return _text;
}

// How long ago the player last saw _entity, which it only remembers, "last seen 4:12 ago", counted at _ticksPerSecond from
// the tick the server says it was last seen in (interface plan 2, task UI1.2); "last seen earlier" when that is not known.
std::string LastSeen(const Outpost::Snapshot& _newest, const Outpost::EntityView& _entity, std::uint32_t _ticksPerSecond)
{
  if (_ticksPerSecond == 0 || _entity.lastSeenTick == 0 || _entity.lastSeenTick > _newest.tick)
    return "last seen earlier";
  return std::format("last seen {} ago", Outpost::MinutesAndSeconds((_newest.tick - _entity.lastSeenTick) / _ticksPerSecond));
}

// A ship's retreat as the HUD writes it (Phase 4 design §10).
std::string RetreatWords(Outpost::RetreatThreshold _retreat)
{
  switch (_retreat)
  {
  case Outpost::RetreatThreshold::Half:
    return "Retreat at 50%";
  case Outpost::RetreatThreshold::Quarter:
    return "Retreat at 25%";
  case Outpost::RetreatThreshold::Never:
    break;
  }
  return "Never retreat";
}

// The retreat a press steps to: from a quarter to half to never, and round.
Outpost::RetreatThreshold NextRetreat(Outpost::RetreatThreshold _retreat) noexcept
{
  switch (_retreat)
  {
  case Outpost::RetreatThreshold::Quarter:
    return Outpost::RetreatThreshold::Half;
  case Outpost::RetreatThreshold::Half:
    return Outpost::RetreatThreshold::Never;
  case Outpost::RetreatThreshold::Never:
    break;
  }
  return Outpost::RetreatThreshold::Quarter;
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
          best.damage[i] =
            std::max(best.damage[i], Outpost::FormationDamagePerSecond(stats, _newest.hulls[i].armorHundredths,
                                                                       static_cast<float>(_newest.hulls[i].footprintRadiusMeters)));
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

// A design's damage against a hull rated by its share of the best any design does to it (Phase 1 design §11).
Hud::Rating RatingOf(float _share) noexcept
{
  return _share >= GOOD_SHARE ? Hud::Rating::Good : _share >= FAIR_SHARE ? Hud::Rating::Fair : Hud::Rating::Poor;
}

// A saved design's stats with the player's components as they are now; none when the snapshot lacks one of them.
std::optional<Outpost::DesignStats> StatsOf(const Outpost::Snapshot& _newest, const Outpost::DesignView& _design)
{
  const auto hull = std::ranges::find(_newest.hulls, _design.hull, &Outpost::HullView::id);
  const auto drive = std::ranges::find(_newest.drives, _design.drive, &Outpost::DriveView::id);
  const auto weapon = std::ranges::find(_newest.weapons, _design.weapon, &Outpost::WeaponView::id);
  const auto module = std::ranges::find(_newest.modules, _design.module, &Outpost::ModuleView::id);
  if (hull == _newest.hulls.end() || drive == _newest.drives.end() || weapon == _newest.weapons.end() ||
      (_design.module.IsValid() && module == _newest.modules.end()))
    return std::nullopt;
  return Outpost::DesignStatsOf(*hull, *drive, *weapon, module != _newest.modules.end() ? &*module : nullptr);
}

// The change from _current to _preview as a signed number in ASCII (ADR-030), "+252", "-26" or "+4.5": whole and grouped in
// thousands when _whole, and otherwise to a tenth as Tenths writes it. Both are rounded as their figures are first, so that
// the change is the difference of the two figures shown (task 15.3).
std::string SignedChange(double _current, double _preview, bool _whole)
{
  const double step = _whole ? 1.0 : 10.0;
  const double change = (std::round(_preview * step) - std::round(_current * step)) / step;
  const char sign = change < 0.0 || (change == 0.0 && _preview < _current) ? '-' : '+';
  return _whole ? std::format("{}{}", sign, Outpost::WithThousands(std::llround(std::abs(change))))
                : std::format("{}{}", sign, Tenths(std::abs(change)));
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
  // A saved design whose name is kept and whose retreat changed is updated, not renamed.
  panel.rename = {.label = match != nullptr && save.has_value() && save->nameUtf8 == match->nameUtf8 ? "UPDATE" : "RENAME",
                  .action = {.kind = Hud::ActionKind::SaveDesign},
                  .enabled = save.has_value() && !savesNew};
  panel.retreat = {.label = Capitals(RetreatWords(_designer.Retreat(_newest))), .action = {.kind = Hud::ActionKind::StepRetreat}};

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
    // Lower is better for cost and build time. _shown is a number as its figure shows it, whole when _whole and to a tenth
    // otherwise, which a preview's change is counted in.
    const auto bar = [&](std::string_view _label, std::string_view _unit, double _best, const auto& _value, const auto& _text,
                         const auto& _shown, bool _whole, bool _lowerIsBetter = false)
    {
      Hud::StatBar row{
        .label = std::string(_label), .value = _text(*stats), .unit = std::string(_unit), .share = ShareOf(_value(*stats), _best)};
      if (previewStats.has_value())
      {
        row.previewShare = ShareOf(_value(*previewStats), _best);
        row.change = ChangeOf(_value(*stats), _value(*previewStats), _lowerIsBetter);
        row.previewValue = row.change == Hud::Change::Same
                             ? _text(*previewStats)
                             : std::format("{} ({})", _text(*previewStats), SignedChange(_shown(*stats), _shown(*previewStats), _whole));
      }
      panel.bars.push_back(std::move(row));
    };
    panel.bars.reserve(7);
    const auto hitPoints = [](const Outpost::DesignStats& _s) { return static_cast<double>(WholePoints(_s.hitPointsHundredths)); };
    const auto armor = [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.armorHundredths) / Outpost::HUNDREDTHS; };
    const auto speed = [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.movement.speedMetersPerSecond); };
    const auto range = [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.rangeMeters); };
    const auto sight = [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.moduleSightMeters); };
    const auto cost = [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.cost); };
    const auto build = [factor](const Outpost::DesignStats& _s) { return _s.buildSeconds / factor; };
    bar(
      "Hit points", "", best.hitPoints, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.hitPointsHundredths); },
      [](const Outpost::DesignStats& _s) { return Outpost::WithThousands(WholePoints(_s.hitPointsHundredths)); }, hitPoints, true);
    bar(
      "Armor", "", best.armor, [](const Outpost::DesignStats& _s) { return static_cast<double>(_s.armorHundredths); },
      [&armor](const Outpost::DesignStats& _s) { return Tenths(armor(_s)); }, armor, false);
    bar("Speed", "m/s", best.speed, speed, [&speed](const Outpost::DesignStats& _s) { return Tenths(speed(_s)); }, speed, false);
    bar("Range", "m", best.range, range, [&range](const Outpost::DesignStats& _s) { return Tenths(range(_s)); }, range, false);
    // How far a module lets the ship see; none without one, when its weapon sets its sight (Phase 2 design §10).
    if (best.sensor > 0.0)
    {
      bar(
        "Sensors", "m", best.sensor, sight,
        [&sight](const Outpost::DesignStats& _s) { return sight(_s) > 0.0 ? Tenths(sight(_s)) : std::string("-"); }, sight, false);
    }
    bar("Cost", "ore", best.cost, cost, [](const Outpost::DesignStats& _s) { return std::to_string(_s.cost); }, cost, true, true);
    bar("Build", "s", best.build, build, [&build](const Outpost::DesignStats& _s) { return Tenths(build(_s)); }, build, false, true);

    const Outpost::DesignStats& shown = previewStats.has_value() ? *previewStats : *stats;
    panel.damage.reserve(_newest.hulls.size());
    for (size_t i = 0; i < _newest.hulls.size(); ++i)
    {
      // Against a formation of the hull, so that a splash counts every ship it reaches (ADR-014).
      const Outpost::HullView& hull = _newest.hulls[i];
      const auto radius = static_cast<float>(hull.footprintRadiusMeters);
      const double damage = Outpost::FormationDamagePerSecond(shown, hull.armorHundredths, radius);
      const double current = Outpost::FormationDamagePerSecond(*stats, hull.armorHundredths, radius);
      const std::int32_t reached = Outpost::ShipsReached(shown.splashRadiusMeters, radius);
      const Hud::Change change = previewStats.has_value() ? ChangeOf(current, damage, false) : Hud::Change::Same;
      const float share = ShareOf(damage, best.damage[i]);
      // The change per ship to a tenth, as the figure is written: "+4.5" (task 15.3).
      const double tenths = (std::round(damage * 10.0) - std::round(current * 10.0)) / 10.0;
      panel.damage.push_back({.hull = hull.nameUtf8,
                              .armor = std::format("ARM {}", Tenths(static_cast<double>(hull.armorHundredths) / Outpost::HUNDREDTHS)),
                              .perShip = std::format("{:.1f}", damage),
                              .perShipChange = change == Hud::Change::Same
                                                 ? std::string()
                                                 : std::format("{}{:.1f}", damage < current ? '-' : '+', std::abs(tenths)),
                              .reach = reached > 1 ? std::format("{}{} ships", TIMES, reached) : std::string(),
                              .perOre = std::format("{:.1f} / 100 ore", shown.cost > 0 ? damage * 100.0 / shown.cost : 0.0),
                              .share = share,
                              .rating = RatingOf(share),
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
  // A Shipyard builds the hulls of its level (Phase 3 design §5): above it, Queue is dim and says the level the hull needs.
  // The design is still saved.
  const auto pickedHull = std::ranges::find(_newest.hulls, picked.hull, &Outpost::HullView::id);
  const bool leveled = target == nullptr || pickedHull == _newest.hulls.end() || target->level >= pickedHull->shipyardLevel;
  if (!leveled)
    panel.queueDetail = std::format("Needs Shipyard L{}", pickedHull->shipyardLevel);
  const Outpost::EntityId producer = target != nullptr ? target->id : Outpost::EntityId{};
  if (match != nullptr)
    panel.queue = {.label = "QUEUE",
                   .action = {.kind = Hud::ActionKind::Queue, .producer = producer, .design = match->id, .count = panel.count},
                   .enabled = leveled && free > 0 && _newest.ore >= cost};
  else
    panel.queue = {.label = "QUEUE",
                   .action = {.kind = Hud::ActionKind::SaveAndQueue, .producer = producer, .count = panel.count},
                   .enabled = leveled && savesNew && free > 0 && stats.has_value() && _newest.ore >= cost};
  return panel;
}

// A floating window's look (ADR-031), after the owner's mockup (Phase 1 design §11): a dark navy body, a hatched title bar
// with the title in the title face, a close box at its right end, and a bracket at each corner.
// Opaque, so that nothing bright behind a window shows through its near-black (task 16.1).
constexpr DirectX::XMFLOAT4 WINDOW_COLOR{0.009f, 0.013f, 0.024f, 1.0f};
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
// Three part cards from CARDS_LEFT, and the mockup's room right of them (ADR-062).
constexpr float DESIGNER_WIDTH = 818.0f;
constexpr float DESIGNER_INSET = 24.0f;
// The hatched header runs on below the title bar, which the window is dragged by, to hold the title and the Shipyard.
constexpr float DESIGNER_HEADER = 72.0f;
constexpr float NAME_ROW_TOP = 84.0f;
constexpr float NAME_ROW_HEIGHT = 38.0f;
constexpr float CHIP_ROW_TOP = 132.0f;
constexpr float CHIP_HEIGHT = 26.0f;
constexpr float CHIPS_LEFT = 70.0f;
// A little narrower than a part card, so that the arrows that scroll the chips stand clear of the third (task 16.1).
constexpr float CHIP_WIDTH = 216.0f;
constexpr float SLOTS_TOP = 168.0f;
constexpr float SLOT_GAP = 10.0f;
constexpr float SLOT_DIVIDER_LEFT = 108.0f;
constexpr float CARDS_LEFT = 118.0f;
constexpr float CARD_WIDTH = 220.0f;
constexpr float CARD_HEIGHT = 80.0f;
constexpr float CARD_GAP = 7.0f;
constexpr std::size_t CARDS_PER_LINE = 3;
// A part card's lines under its name: its numbers, any note, such as a weapon's splash, and at its foot the research that
// unlocks it. A row whose cards have notes is taller, so that a note and that line never meet (ADR-061).
constexpr float CARD_NUMBERS_TOP = 34.0f;
constexpr float CARD_NOTE_TOP = 52.0f;
constexpr float CARD_NOTE_ROOM = 16.0f;
constexpr float CARD_LOCK_FROM_FOOT = 20.0f;
constexpr float SECTION_GAP = 20.0f;
constexpr float SECTION_LABEL_HEIGHT = 26.0f;
constexpr float BAR_ROW_HEIGHT = 25.0f;
constexpr float BAR_LEFT = 104.0f;
// A previewed figure and its change, "1,680 (+1,482)", end at BAR_VALUE_RIGHT clear of the bar (task 15.3).
constexpr float BAR_WIDTH = 90.0f;
constexpr float BAR_VALUE_RIGHT = 316.0f;
constexpr float DAMAGE_LEFT = 364.0f;
constexpr float DAMAGE_CARD_HEIGHT = 96.0f;
constexpr float DAMAGE_CARD_GAP = 8.0f;
constexpr float FOOTER_HEIGHT = 48.0f;
// An arrow's button, and the room between it and what it steps (task 16.1).
constexpr float SMALL_BUTTON_UNITS = 24.0f;
constexpr float ARROW_GAP_UNITS = 8.0f;
constexpr float LINE_UNITS = 1.0f;
// Labels in small spaced capitals.
constexpr float LABEL_TRACKING_UNITS = 2.0f;
// Ore's diamond beside a figure: this share of the figure's size, and this far from it.
constexpr float ORE_MARK_SHARE = 0.6f;
constexpr float ORE_MARK_GAP_UNITS = 5.0f;
// The room a line cut short to fit keeps from what stands beside it, such as a cost (ADR-061).
constexpr float FIT_GAP_UNITS = 6.0f;
// The box a window's header holds the player's Ore in.
constexpr float ORE_BOX_WIDTH = 116.0f;
// How far down a small button its mark's line starts.
constexpr float SMALL_BUTTON_TEXT_TOP = 4.0f;
// A key's cap on a button (task 15.2), and how far down it the letter's line starts.
constexpr float KEY_CAP_UNITS = 20.0f;
constexpr float KEY_CAP_TEXT_TOP = 2.0f;

// The mockup's colors (gate H6, confirmed at the owner's run), sampled from it and made linear, as the render target
// encodes them to sRGB. Those raised so that every text stands at 4.5:1 or more against its panel say so (ADR-062).
constexpr DirectX::XMFLOAT4 LABEL_COLOR{0.22f, 0.29f, 0.42f, 1.0f};
constexpr DirectX::XMFLOAT4 ROW_LABEL_COLOR{0.41f, 0.49f, 0.61f, 1.0f};
// A label on a window's hatched title bar, lighter than other labels to stand at 4.5:1 on its stripes (ADR-062).
constexpr DirectX::XMFLOAT4 HEADER_LABEL_COLOR = ROW_LABEL_COLOR;
constexpr DirectX::XMFLOAT4 NUMBERS_COLOR{0.65f, 0.77f, 0.93f, 1.0f};
constexpr DirectX::XMFLOAT4 SOFT_COLOR{0.29f, 0.37f, 0.5f, 1.0f};
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
// Raised from the mockup's (0.15, 0.19, 0.26) to 4.6:1 on a locked card's hatching, and still under a third of live
// text's luminance (ADR-062).
constexpr DirectX::XMFLOAT4 LOCKED_TEXT_COLOR{0.2f, 0.26f, 0.35f, 1.0f};
constexpr DirectX::XMFLOAT4 AMBER_COLOR{0.62f, 0.34f, 0.09f, 1.0f};
constexpr DirectX::XMFLOAT4 GOOD_COLOR{0.15f, 0.9f, 0.075f, 1.0f};
constexpr DirectX::XMFLOAT4 FAIR_COLOR{0.9f, 0.46f, 0.026f, 1.0f};
// A touch brighter than the mockup's (0.75, 0.08, 0.044), so that a worse figure stands at 4.5:1 on a window (ADR-062).
constexpr DirectX::XMFLOAT4 POOR_COLOR{0.8f, 0.09f, 0.05f, 1.0f};
constexpr DirectX::XMFLOAT4 BAR_TRACK_COLOR{0.004f, 0.005f, 0.009f, 1.0f};
constexpr DirectX::XMFLOAT4 BAR_FILL_COLOR{0.17f, 0.3f, 0.58f, 1.0f};
// The Queue button's face and stripes, at 0.65 of the mockup's, so that the gold of its cost stands at 4.5:1 on them
// (ADR-062).
constexpr DirectX::XMFLOAT4 QUEUE_COLOR{0.024f, 0.081f, 0.031f, 1.0f};
constexpr DirectX::XMFLOAT4 QUEUE_HATCH_COLOR{0.034f, 0.099f, 0.044f, 1.0f};
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

// A queue's label, as every window writes it: its jobs against the queue's limit, "QUEUE · 2 / 5" (task 16.1).
std::string QueueLabel(std::size_t _jobs)
{
  return std::format("QUEUE{}{} / {}", DOT, _jobs, Outpost::QUEUE_LIMIT);
}

// Where the arrows stand that step a subject: in a window's title bar, and on its header's title line.
constexpr float ARROWS_IN_TITLE_TOP = 6.0f;
constexpr float ARROWS_IN_HEADER_TOP = 43.0f;

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
// everything (ADR-031), measuring its text with the fonts it is drawn in (ADR-061).
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
  void RightText(std::string _text, float _right, float _top, const DirectX::XMFLOAT4& _color, Hud::Typeface _face, float _tracking = 0.0f)
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

  // A small square button with a mark on it, such as an arrow, in its middle.
  void SmallButton(std::string _mark, float _left, float _top, bool _enabled, const Hud::Action& _action)
  {
    const Hud::Rect face = Panel(_left, _top, SMALL_BUTTON_UNITS, SMALL_BUTTON_UNITS, _enabled ? CARD_COLOR : DISABLED_BUTTON_COLOR);
    if (_enabled)
      Press(face, _action);
    const float left = _left + ((SMALL_BUTTON_UNITS - Width(_mark, Hud::Typeface::Label)) / 2.0f);
    Text(std::move(_mark), left, _top + SMALL_BUTTON_TEXT_TOP, _enabled ? TEXT_COLOR : DIM_TEXT_COLOR, Hud::Typeface::Label);
  }

  // _subject between the arrows that step it, "< SHIPYARD 05 >" (task 16.1): the left arrow at _left, the subject cut
  // short to _roomUnits, and the right arrow after it. _arrowTop places the arrows and _textTop the subject. It returns
  // where the right arrow ends.
  float Stepped(const std::string& _subject, float _left, float _arrowTop, float _textTop, float _roomUnits, bool _canStep,
                const Hud::Action& _previous, const Hud::Action& _next, const DirectX::XMFLOAT4& _color, Hud::Typeface _face,
                float _tracking = 0.0f)
  {
    SmallButton("<", _left, _arrowTop, _canStep, _previous);
    const float subjectLeft = _left + SMALL_BUTTON_UNITS + ARROW_GAP_UNITS;
    std::string subject = Fit(_subject, _face, _roomUnits - (2.0f * (SMALL_BUTTON_UNITS + ARROW_GAP_UNITS)), _tracking);
    const float nextLeft = subjectLeft + Width(subject, _face, _tracking) + ARROW_GAP_UNITS;
    Text(std::move(subject), subjectLeft, _textTop, _color, _face, _tracking);
    SmallButton(">", nextLeft, _arrowTop, _canStep, _next);
    return nextLeft + SMALL_BUTTON_UNITS;
  }

  // How wide an amount of Ore is set, its diamond and its figure, in _face.
  [[nodiscard]] float DiamondAndFigureWidth(std::int32_t _ore, Hud::Typeface _face) const
  {
    return std::round(Hud::FaceUnits(_face) * ORE_MARK_SHARE) + ORE_MARK_GAP_UNITS + Width(Outpost::WithThousands(_ore), _face);
  }

  // An amount of Ore that ends at _right: Ore's diamond, then the figure, in _face.
  void DiamondAndFigure(std::int32_t _ore, float _right, float _top, Hud::Typeface _face, const DirectX::XMFLOAT4& _color)
  {
    DiamondAndFigureFrom(_ore, _right - DiamondAndFigureWidth(_ore, _face), _top, _face, _color);
  }

  // An amount of Ore that starts at _left: Ore's diamond, sized by the face, then the figure, in _face.
  void DiamondAndFigureFrom(std::int32_t _ore, float _left, float _top, Hud::Typeface _face, const DirectX::XMFLOAT4& _color)
  {
    const float sizeUnits = Hud::FaceUnits(_face);
    const float mark = std::round(sizeUnits * ORE_MARK_SHARE);
    Sprite(Hud::Sprite::OreMark, _left, _top + ((sizeUnits * 1.25f) - mark) / 2.0f, mark, _color);
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
    DiamondAndFigure(_ore, _right - 10.0f, 39.0f, Hud::Typeface::Title, GOLD_COLOR);
  }

private:
  Hud::Layout& m_layout;
  const Hud::TextMetrics& m_metrics;
  float m_originX;
  float m_originY;
  float m_scale;
};

// A button of the HUD, in pixels, in the look of a window's card (ADR-043): its face and edge, its label in the name face,
// any cost after a '|' as Ore's diamond and the figure at its right, or its key as a cap there (task 15.2), and its place
// among the actions when it does something. The label is cut short only where it would meet the cost, the note or the
// cap. Work it started that is under way runs as a bar along its foot, under the label, from left to right.
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
  if (_button.progressPermille.has_value())
  {
    const float barWidth = width - (2.0f * BUTTON_BAR_INSET);
    const float barTop = height - BUTTON_BAR_INSET - BUTTON_BAR_UNITS;
    paint.Panel(BUTTON_BAR_INSET, barTop, barWidth, BUTTON_BAR_UNITS, BAR_TRACK_COLOR);
    paint.Panel(BUTTON_BAR_INSET, barTop,
                barWidth * static_cast<float>(std::clamp(*_button.progressPermille, 0, Outpost::PERMILLE)) /
                  static_cast<float>(Outpost::PERMILLE),
                BUTTON_BAR_UNITS, BAR_FILL_COLOR);
  }
  const size_t split = _button.label.find('|');
  const std::string label = _button.label.substr(0, split);
  const DirectX::XMFLOAT4& labelColor = _button.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR;
  const float labelTop = (height - NAME_LINE_UNITS) / 2.0f;
  const float figureTop = (height - FIGURE_LINE_UNITS) / 2.0f;
  float room = width - (2.0f * BUTTON_INSET);
  if (!_button.key.empty())
  {
    // The key's cap: an outlined square with the letter in the label face, in the middle of it.
    const float capLeft = width - BUTTON_INSET - KEY_CAP_UNITS;
    const float capTop = (height - KEY_CAP_UNITS) / 2.0f;
    paint.Panel(capLeft, capTop, KEY_CAP_UNITS, KEY_CAP_UNITS, FIELD_COLOR);
    paint.Outline(capLeft, capTop, KEY_CAP_UNITS, KEY_CAP_UNITS, ROW_LABEL_COLOR);
    paint.Text(_button.key, capLeft + ((KEY_CAP_UNITS - paint.Width(_button.key, Hud::Typeface::Label)) / 2.0f), capTop + KEY_CAP_TEXT_TOP,
               ROW_LABEL_COLOR, Hud::Typeface::Label);
    room -= KEY_CAP_UNITS + FIT_GAP_UNITS;
  }
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
  const float costWidth = paint.DiamondAndFigureWidth(cost, Hud::Typeface::Figure);
  paint.Text(paint.Fit(label, Hud::Typeface::Name, room - costWidth - FIT_GAP_UNITS), BUTTON_INSET, labelTop, labelColor,
             Hud::Typeface::Name);
  paint.DiamondAndFigure(cost, width - BUTTON_INSET, figureTop, Hud::Typeface::Figure, _button.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
}

void LayDesigner(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, const Hud::DesignerPanel& _panel,
                 Outpost::WindowManager::Point _corner, float _scale)
{
  const DesignerExtent extent = ExtentOf(_panel);
  (void)OpenWindow(_layout, Outpost::WindowKind::Designer, std::string(), _corner, DESIGNER_WIDTH, extent.height - Hud::TITLE_BAR_UNITS,
                   _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  const float tracking = paint.Tracking();

  // The header: the target Shipyard between the arrows that step it, the title, the Shipyard's queue over its slots and the
  // ships it has built beside them, and the Ore.
  paint.HeaderBand(DESIGNER_WIDTH, DESIGNER_HEADER);
  // The queue's slots, ending a little short of the Ore's box.
  constexpr float SLOT_SIZE = 30.0f;
  constexpr float SLOT_SPACING = 3.0f;
  constexpr float SLOTS_LEFT = DESIGNER_WIDTH - DESIGNER_INSET - ORE_BOX_WIDTH - 14.0f -
                               (static_cast<float>(Outpost::QUEUE_LIMIT) * (SLOT_SIZE + SLOT_SPACING)) + SLOT_SPACING;
  const float steppedEnd = paint.Stepped(_panel.shipyard, DESIGNER_INSET, ARROWS_IN_TITLE_TOP, 11.0f, SLOTS_LEFT - DESIGNER_INSET - 120.0f,
                                         _panel.hasShipyard, {.kind = Hud::ActionKind::PreviousShipyard},
                                         {.kind = Hud::ActionKind::NextShipyard}, HEADER_LABEL_COLOR, Hud::Typeface::Label, tracking);
  paint.Text(std::format("{}DESIGNER", DOT.substr(1)), steppedEnd + ARROW_GAP_UNITS, 11.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label,
             tracking);
  paint.Text("SHIP DESIGN", DESIGNER_INSET, 40.0f, TEXT_COLOR, Hud::Typeface::Title);
  paint.Text(QueueLabel(_panel.queued), SLOTS_LEFT, 12.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label, tracking);
  paint.RightText(std::format("BUILT{}{}", DOT, _panel.built), SLOTS_LEFT - 12.0f, 44.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label,
                  tracking);
  for (std::size_t slot = 0; slot < Outpost::QUEUE_LIMIT; ++slot)
  {
    paint.Panel(SLOTS_LEFT + (static_cast<float>(slot) * (SLOT_SIZE + SLOT_SPACING)), 40.0f, SLOT_SIZE, 24.0f,
                slot < _panel.queued ? SLOT_FILLED_COLOR : FIELD_COLOR);
  }
  paint.OreBox(_panel.ore, DESIGNER_WIDTH - DESIGNER_INSET);

  // The name, its count against the limit, and Save at the row's right end.
  constexpr float SAVE_WIDTH = 68.0f;
  constexpr float SAVE_LEFT = DESIGNER_WIDTH - DESIGNER_INSET - SAVE_WIDTH;
  const float fieldWidth = SAVE_LEFT - DESIGNER_INSET - 8.0f;
  const Hud::Rect field = paint.Panel(DESIGNER_INSET, NAME_ROW_TOP, fieldWidth, NAME_ROW_HEIGHT, FIELD_COLOR);
  paint.Press(field, {.kind = Hud::ActionKind::EditName});
  paint.Outline(DESIGNER_INSET, NAME_ROW_TOP, fieldWidth, NAME_ROW_HEIGHT, _panel.editing ? PICKED_EDGE_COLOR : EDGE_COLOR);
  paint.Text("NAME", DESIGNER_INSET + 12.0f, NAME_ROW_TOP + 13.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
  const std::string count = std::format("{}/{}", _panel.name.size(), Outpost::DESIGN_NAME_LIMIT);
  const float nameRoom = fieldWidth - 66.0f - 12.0f - paint.Width(count, Hud::Typeface::Figure) - FIT_GAP_UNITS;
  paint.Text(paint.Fit(_panel.editing ? _panel.name + "_" : _panel.name, Hud::Typeface::Name, nameRoom), DESIGNER_INSET + 66.0f,
             NAME_ROW_TOP + 9.0f, _panel.nameValid ? TEXT_COLOR : WARNING_COLOR, Hud::Typeface::Name);
  paint.RightText(count, DESIGNER_INSET + fieldWidth - 12.0f, NAME_ROW_TOP + 12.0f, LABEL_COLOR, Hud::Typeface::Figure);
  {
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
    const float chipNameRoom = CHIP_WIDTH - 16.0f - paint.Width(chip.code, Hud::Typeface::Detail) - FIT_GAP_UNITS;
    paint.Text(paint.Fit(chip.name, Hud::Typeface::Label, chipNameRoom), left + 8.0f, CHIP_ROW_TOP + 6.0f, TEXT_COLOR,
               Hud::Typeface::Label);
    paint.RightText(chip.code, left + CHIP_WIDTH - 8.0f, CHIP_ROW_TOP + 7.0f, chip.shown ? ACCENT_COLOR : CODE_COLOR,
                    Hud::Typeface::Detail);
  }
  if (_panel.chips.size() > CHIPS_SHOWN)
  {
    const float arrowsLeft = DESIGNER_WIDTH - DESIGNER_INSET - (2.0f * SMALL_BUTTON_UNITS) - 4.0f;
    paint.SmallButton("<", arrowsLeft, CHIP_ROW_TOP + 1.0f, _panel.firstChip > 0, {.kind = Hud::ActionKind::PreviousDesigns});
    paint.SmallButton(">", arrowsLeft + SMALL_BUTTON_UNITS + 4.0f, CHIP_ROW_TOP + 1.0f,
                      _panel.firstChip + CHIPS_SHOWN < _panel.chips.size(), {.kind = Hud::ActionKind::NextDesigns});
  }

  // A row for each slot: its label and pick, then its cards, three to a line.
  for (std::size_t slot = 0; slot < _panel.slots.size(); ++slot)
  {
    const Hud::SlotRow& row = _panel.slots[slot];
    const float top = extent.slotTops[slot];
    const float lines = CardLinesOf(row);
    const float cardHeight = CardHeightOf(row);
    // The label and the pick, 18 units apart so that the label's line clears the pick's at 1280x720's rounding, and centered
    // on a line of cards together (ADR-062).
    paint.Text(row.label, DESIGNER_INSET, top + 21.0f, LABEL_COLOR, Hud::Typeface::Label, tracking);
    paint.Text(paint.Fit(row.picked, Hud::Typeface::Name, SLOT_DIVIDER_LEFT - DESIGNER_INSET - FIT_GAP_UNITS), DESIGNER_INSET, top + 39.0f,
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
      const float costWidth = paint.DiamondAndFigureWidth(card.cost, Hud::Typeface::Figure);
      const DirectX::XMFLOAT4& nameColor = locked ? LOCKED_TEXT_COLOR : TEXT_COLOR;
      paint.Text(paint.Fit(card.name, Hud::Typeface::Name, inner - costWidth - FIT_GAP_UNITS), left + 12.0f, cardTop + 8.0f, nameColor,
                 Hud::Typeface::Name);
      paint.DiamondAndFigure(card.cost, left + CARD_WIDTH - 12.0f, cardTop + 11.0f, Hud::Typeface::Figure,
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
  paint.Text(std::format("DAMAGE / S AFTER ARMOR{}VS A FORMATION", DOT), DAMAGE_LEFT, sections, LABEL_COLOR, Hud::Typeface::Label,
             tracking);
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
    // Clear of the hull's line, which the name face sets about 19 units tall, at every scale's rounding.
    paint.Text(card.perShip, left + 10.0f, cardsTop + 30.0f, ChangeColor(card.change, TEXT_COLOR), Hud::Typeface::LargeFigure);
    // How many ships a splash reaches, at the card's right above the change.
    if (!card.reach.empty())
      paint.RightText(card.reach, left + cardWidth - 8.0f, cardsTop + 28.0f, LABEL_COLOR, Hud::Typeface::Label);
    // A hovered part's change, at the card's right on the large figure's line (task 15.3).
    if (!card.perShipChange.empty())
    {
      paint.RightText(card.perShipChange, left + cardWidth - 8.0f, cardsTop + 44.0f, ChangeColor(card.change, TEXT_COLOR),
                      Hud::Typeface::Figure);
    }
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
  // The design's retreat, between Rename and the stepper; a press steps it (Phase 4 design §10).
  {
    constexpr float RETREAT_LEFT = DESIGNER_INSET + 102.0f;
    constexpr float RETREAT_WIDTH = 206.0f;
    const Hud::Rect face = paint.Panel(RETREAT_LEFT, footer, RETREAT_WIDTH, FOOTER_HEIGHT, CARD_COLOR);
    paint.Outline(RETREAT_LEFT, footer, RETREAT_WIDTH, FOOTER_HEIGHT, EDGE_COLOR);
    paint.Press(face, _panel.retreat.action);
    paint.Text(paint.Fit(_panel.retreat.label, Hud::Typeface::Label, RETREAT_WIDTH - 28.0f), RETREAT_LEFT + 14.0f, footer + 16.0f,
               TEXT_COLOR, Hud::Typeface::Label, tracking);
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
    // The label's line, up to 27 units in the title face, ends above the detail's at either scale's rounding (ADR-061).
    paint.Text(queue.label, QUEUE_LEFT + 18.0f, footer + 2.0f, color, Hud::Typeface::Title, tracking);
    paint.Text(_panel.queueDetail, QUEUE_LEFT + 18.0f, footer + 31.0f, color, Hud::Typeface::Detail);
    paint.DiamondAndFigure(_panel.queueCost, QUEUE_LEFT + queueWidth - 16.0f, footer + 10.0f, Hud::Typeface::Title,
                           queue.enabled ? GOLD_COLOR : DIM_TEXT_COLOR);
  }
}

// The production and research windows (Phase 1 design §12), in the designer's look, in reference units from the window's
// top-left corner: a hatched header with the window's subject and the Ore, the cards that add to the queue, and under them
// the queue, a row for each job. The cards stay where they are as the queue grows, so that one can be clicked again and
// again, and the window grows at its foot (ADR-043). Research is wide enough that a topic's name fits its card, and
// production as wide as leaves both windows clear of the designer, side by side at the reference width (ADR-062).
constexpr float RESEARCH_WINDOW_WIDTH = 560.0f;
constexpr float PRODUCTION_WINDOW_WIDTH = Hud::REFERENCE_WIDTH_UNITS - (4.0f * MARGIN) - DESIGNER_WIDTH - RESEARCH_WINDOW_WIDTH;
constexpr float WINDOW_INSET = 24.0f;
// A section's label, and how far under it what it labels starts.
constexpr float SECTION_TOP = 84.0f;
constexpr float SECTION_BODY_GAP = 20.0f;
constexpr float QUEUE_ROW_HEIGHT = 30.0f;
constexpr float QUEUE_ROW_GAP = 4.0f;
constexpr float QUEUE_BAR_WIDTH = 120.0f;
// A production card: its name; its code and cost; and its build time and its strength against each hull (ADR-068).
constexpr float OPTION_HEIGHT = 74.0f;
constexpr float OPTION_THIRD_LINE_TOP = 50.0f;
// A strength's bar: three segments, as many lit as its rating is good, beside the hull's initial.
constexpr float STRENGTH_SEGMENT_UNITS = 6.0f;
constexpr float STRENGTH_SEGMENT_HEIGHT = 8.0f;
constexpr float STRENGTH_SEGMENT_GAP = 2.0f;
constexpr float STRENGTH_LETTER_GAP = 3.0f;
constexpr float STRENGTH_GROUP_GAP = 10.0f;
constexpr std::size_t STRENGTH_SEGMENTS = 3;
// A topic card: its name, then a line for each line of its effect and for each prerequisite neither researched nor queued,
// or one for its tier and time. Every card is as tall as the most lines any topic needs, and two at least (ADR-061).
constexpr float TOPIC_LINES_TOP = 28.0f;
constexpr float TOPIC_LINE_UNITS = 18.0f;
constexpr float TOPIC_FOOT_UNITS = 4.0f;
constexpr std::size_t TOPIC_LEAST_LINES = 2;
constexpr float CARD_SPACING = 8.0f;
// A card's text stands this far in from its edges.
constexpr float CARD_INSET = 12.0f;
// Where the production and research windows stand at first: under the Ore panel and the status panel's place (ADR-066).
constexpr float WINDOWS_TOP = MARGIN + ORE_PANEL_HEIGHT + PANEL_GAP + STATUS_PANEL_KEPT_UNITS + MARGIN;
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

// A window's header: its subject in the title face, and the Ore. A subject that is stepped through, such as production's
// producer, stands between the arrows that step it (task 16.1).
void Header(Painter& _paint, const std::string& _subject, std::int32_t _ore, float _widthUnits,
            std::optional<std::pair<Hud::Action, Hud::Action>> _steps = std::nullopt, bool _canStep = false)
{
  _paint.HeaderBand(_widthUnits, DESIGNER_HEADER);
  const float room = _widthUnits - (2.0f * WINDOW_INSET) - ORE_BOX_WIDTH - FIT_GAP_UNITS;
  if (_steps.has_value())
  {
    (void)_paint.Stepped(_subject, WINDOW_INSET, ARROWS_IN_HEADER_TOP, 40.0f, room, _canStep, _steps->first, _steps->second, TEXT_COLOR,
                         Hud::Typeface::Title);
  }
  else
    _paint.Text(_paint.Fit(_subject, Hud::Typeface::Title, room), WINDOW_INSET, 40.0f, TEXT_COLOR, Hud::Typeface::Title);
  _paint.OreBox(_ore, _widthUnits - WINDOW_INSET);
}

// A queue under its label at _top: a row for each job, with its name, and the front one's progress or its wait for Ore.
// The label counts the jobs against the queue's limit, which says how many more it takes.
void QueueRows(Painter& _paint, const std::vector<Hud::QueueLine>& _queue, float _widthUnits, float _top)
{
  _paint.Text(QueueLabel(_queue.size()), WINDOW_INSET, _top, LABEL_COLOR, Hud::Typeface::Label, _paint.Tracking());
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
    // The name runs up to the front job's progress or its wait, for Ore or for the fleet cap (Phase 4 design §5).
    const std::string_view waiting = line.capped ? "WAITING FOR FLEET CAP" : "WAITING FOR ORE";
    const float nameLeft = WINDOW_INSET + 32.0f;
    const float nameRight = line.waiting ? right - _paint.Width(waiting, Hud::Typeface::Label) : line.front ? barLeft : right;
    _paint.Text(_paint.Fit(line.name, Hud::Typeface::Name, nameRight - nameLeft - FIT_GAP_UNITS), nameLeft, top + 5.0f, TEXT_COLOR,
                Hud::Typeface::Name);
    if (line.waiting)
      _paint.RightText(std::string(waiting), right, top + 9.0f, AMBER_COLOR, Hud::Typeface::Label);
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
  (void)OpenWindow(_layout, Outpost::WindowKind::Production, std::string(), _corner, PRODUCTION_WINDOW_WIDTH,
                   ProductionHeight(_panel) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.Text("PRODUCTION", WINDOW_INSET, 11.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  Header(paint, _panel.producer, _panel.ore, PRODUCTION_WINDOW_WIDTH,
         std::pair{Hud::Action{.kind = Hud::ActionKind::PreviousProducer}, Hud::Action{.kind = Hud::ActionKind::NextProducer}},
         _panel.canStep);

  paint.Text("BUILD", WINDOW_INSET, SECTION_TOP, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  const float width = (PRODUCTION_WINDOW_WIDTH - (2.0f * WINDOW_INSET) - CARD_SPACING) / 2.0f;
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
    const float costWidth = paint.DiamondAndFigureWidth(option.cost, Hud::Typeface::Figure);
    paint.Text(paint.Fit(option.detail, Hud::Typeface::Detail, inner - costWidth - FIT_GAP_UNITS), left + CARD_INSET, cardTop + 28.0f,
               option.enabled ? CODE_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
    paint.DiamondAndFigure(option.cost, left + width - CARD_INSET, cardTop + 28.0f, Hud::Typeface::Figure,
                           option.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
    // How long one takes, and how it does against each hull: three segments a hull, three lit for Good, two for Fair and
    // one for Poor, so that the bars read without their colors (ADR-068).
    const float thirdTop = cardTop + OPTION_THIRD_LINE_TOP;
    paint.Text(option.time, left + CARD_INSET, thirdTop, option.enabled ? LABEL_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
    const float segmentsWidth =
      (static_cast<float>(STRENGTH_SEGMENTS) * (STRENGTH_SEGMENT_UNITS + STRENGTH_SEGMENT_GAP)) - STRENGTH_SEGMENT_GAP;
    float groupsWidth = 0.0f;
    for (const Hud::HullRating& strength : option.strengths)
      groupsWidth += paint.Width(strength.hull, Hud::Typeface::Label) + STRENGTH_LETTER_GAP + segmentsWidth + STRENGTH_GROUP_GAP;
    float x = left + width - CARD_INSET - std::max(0.0f, groupsWidth - STRENGTH_GROUP_GAP);
    for (const Hud::HullRating& strength : option.strengths)
    {
      paint.Text(strength.hull, x, thirdTop, option.enabled ? ROW_LABEL_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Label);
      x += paint.Width(strength.hull, Hud::Typeface::Label) + STRENGTH_LETTER_GAP;
      const auto lit = static_cast<std::size_t>(strength.rating) + 1;
      for (std::size_t segment = 0; segment < STRENGTH_SEGMENTS; ++segment)
      {
        const DirectX::XMFLOAT4& color = segment >= lit   ? BAR_TRACK_COLOR
                                         : option.enabled ? RatingColor(strength.rating)
                                                          : LOCKED_TEXT_COLOR;
        paint.Panel(x, thirdTop + 5.0f, STRENGTH_SEGMENT_UNITS, STRENGTH_SEGMENT_HEIGHT, color);
        x += STRENGTH_SEGMENT_UNITS + STRENGTH_SEGMENT_GAP;
      }
      x += STRENGTH_GROUP_GAP - STRENGTH_SEGMENT_GAP;
    }
  }
  if (!_panel.hint.empty())
  {
    paint.Text(paint.Fit(_panel.hint, Hud::Typeface::Detail, PRODUCTION_WINDOW_WIDTH - (2.0f * WINDOW_INSET)), WINDOW_INSET,
               CARDS_TOP + 16.0f, LABEL_COLOR, Hud::Typeface::Detail);
  }
  QueueRows(paint, _panel.queue, PRODUCTION_WINDOW_WIDTH, QueueTop(CardsBottom(OptionLines(_panel), OPTION_HEIGHT)));
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
  return (RESEARCH_WINDOW_WIDTH - (2.0f * WINDOW_INSET) - ((columns - 1.0f) * CARD_SPACING)) / columns;
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
  (void)OpenWindow(_layout, Outpost::WindowKind::Research, std::string(), _corner, RESEARCH_WINDOW_WIDTH,
                   QueueWindowHeight(cardsBottom, _panel.queue.size()) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.Text("RESEARCH", WINDOW_INSET, 11.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  Header(paint, _panel.lab, _panel.ore, RESEARCH_WINDOW_WIDTH);

  // Where the page is among the topics, "TOPICS · 1-10 OF 21", with an ASCII hyphen (ADR-030; task 16.1).
  const std::size_t pageEnd = std::min(_panel.firstTopic + Hud::TOPICS_SHOWN, _panel.topics.size());
  paint.Text(_panel.topics.empty() ? std::string("TOPICS")
                                   : std::format("TOPICS{}{}-{} OF {}", DOT, _panel.firstTopic + 1, pageEnd, _panel.topics.size()),
             WINDOW_INSET, SECTION_TOP, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  if (_panel.topics.size() > Hud::TOPICS_SHOWN)
  {
    const float arrowsLeft = RESEARCH_WINDOW_WIDTH - WINDOW_INSET - (2.0f * SMALL_BUTTON_UNITS) - 4.0f;
    const float arrowsTop = SECTION_TOP - 6.0f;
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
    paint.Outline(left, cardTop, width, topicHeight, EDGE_COLOR);
    const DirectX::XMFLOAT4& color = topic.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR;
    const float costWidth = paint.DiamondAndFigureWidth(topic.cost, Hud::Typeface::Figure);
    paint.Text(paint.Fit(topic.name, Hud::Typeface::Name, width - (2.0f * CARD_INSET) - costWidth - FIT_GAP_UNITS), left + CARD_INSET,
               cardTop + 6.0f, color, Hud::Typeface::Name);
    paint.DiamondAndFigure(topic.cost, left + width - CARD_INSET, cardTop + 9.0f, Hud::Typeface::Figure,
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
      paint.Text(paint.Fit(needs, Hud::Typeface::Label, width - 28.0f - CARD_INSET), left + 28.0f, lineTop, AMBER_COLOR,
                 Hud::Typeface::Label);
      lineTop += TOPIC_LINE_UNITS;
    }
    if (!blocked)
      paint.Text(topic.time, left + CARD_INSET, lineTop, topic.enabled ? LABEL_COLOR : LOCKED_TEXT_COLOR, Hud::Typeface::Detail);
  }
  QueueRows(paint, _panel.queue, RESEARCH_WINDOW_WIDTH, QueueTop(cardsBottom));
}

// The Controls window (task 16.4): every key and mouse action the game reads in a match, as KeyBindings lists them, under
// their headings, the keys in a column of their own.
constexpr float CONTROLS_WINDOW_WIDTH = 700.0f;
constexpr float CONTROLS_KEYS_WIDTH = 240.0f;
constexpr float CONTROLS_HEADING_GAP = 10.0f;

float ControlsHeight(const std::vector<Outpost::KeyBinding>& _bindings) noexcept
{
  float height = Hud::TITLE_BAR_UNITS + PADDING;
  for (std::size_t line = 0; line < _bindings.size(); ++line)
    height += NAME_LINE_UNITS + (_bindings[line].does.empty() && line > 0 ? CONTROLS_HEADING_GAP : 0.0f);
  return height + PADDING;
}

void LayControls(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, Outpost::WindowManager::Point _corner, float _scale)
{
  const std::vector<Outpost::KeyBinding> bindings = Outpost::KeyBindings();
  (void)OpenWindow(_layout, Outpost::WindowKind::Controls, std::string(), _corner, CONTROLS_WINDOW_WIDTH,
                   ControlsHeight(bindings) - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.Text("CONTROLS", WINDOW_INSET, 11.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  const float doesLeft = WINDOW_INSET + CONTROLS_KEYS_WIDTH;
  float top = Hud::TITLE_BAR_UNITS + PADDING;
  for (std::size_t line = 0; line < bindings.size(); ++line)
  {
    const Outpost::KeyBinding& binding = bindings[line];
    if (binding.does.empty())
    {
      top += line > 0 ? CONTROLS_HEADING_GAP : 0.0f;
      paint.Text(binding.keys, WINDOW_INSET, top + 3.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
    }
    else
    {
      paint.Text(paint.Fit(binding.keys, Hud::Typeface::Name, CONTROLS_KEYS_WIDTH - FIT_GAP_UNITS), WINDOW_INSET, top, ACCENT_COLOR,
                 Hud::Typeface::Name);
      paint.Text(paint.Fit(binding.does, Hud::Typeface::Name, CONTROLS_WINDOW_WIDTH - WINDOW_INSET - doesLeft), doesLeft, top, TEXT_COLOR,
                 Hud::Typeface::Name);
    }
    top += NAME_LINE_UNITS;
  }
}

// The orders window (Phase 5 design §7, §11): under a label, the seat's scheduled orders, a line each; and under another,
// the form's rows, each its label and its value between the arrows that step it, the line that says when the order fires,
// and the button that gives it. It is as tall as its most orders and rows, so that it does not jump as the form changes.
constexpr float ORDERS_WINDOW_WIDTH = 640.0f;
constexpr float ORDER_ROW_UNITS = 30.0f;
constexpr float ORDER_VALUE_LEFT = 150.0f;
constexpr float ORDER_ARROW_TOP = 3.0f;
constexpr float ORDER_TEXT_TOP = 6.0f;
constexpr float ORDER_LABEL_TOP = 9.0f;

// Where the form's label stands, and the window's height.
constexpr float ORDERS_FORM_TOP =
  Hud::TITLE_BAR_UNITS + PADDING + NAME_LINE_UNITS + (static_cast<float>(Hud::ORDERS_SHOWN + 1) * NAME_LINE_UNITS) + SECTION_GAP;
constexpr float ORDERS_ROWS_TOP = ORDERS_FORM_TOP + NAME_LINE_UNITS;
constexpr float ORDERS_FIRES_TOP = ORDERS_ROWS_TOP + (static_cast<float>(Hud::ORDER_ROWS) * ORDER_ROW_UNITS) + PANEL_GAP;
constexpr float ORDERS_GIVE_TOP = ORDERS_FIRES_TOP + NAME_LINE_UNITS + PANEL_GAP;
constexpr float ORDERS_WINDOW_HEIGHT = ORDERS_GIVE_TOP + BUTTON_HEIGHT + PADDING;

void LayOrders(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, const Hud::OrdersPanel& _panel,
               Outpost::WindowManager::Point _corner, float _scale)
{
  (void)OpenWindow(_layout, Outpost::WindowKind::Orders, std::string(), _corner, ORDERS_WINDOW_WIDTH,
                   ORDERS_WINDOW_HEIGHT - Hud::TITLE_BAR_UNITS, _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.Text("SCHEDULED ORDERS", WINDOW_INSET, 11.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  const float room = ORDERS_WINDOW_WIDTH - (2.0f * WINDOW_INSET);
  float top = Hud::TITLE_BAR_UNITS + PADDING;
  paint.Text("WAITING", WINDOW_INSET, top + 3.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  top += NAME_LINE_UNITS;
  if (_panel.scheduled.empty())
    paint.Text("None", WINDOW_INSET, top, DIM_TEXT_COLOR, Hud::Typeface::Name);
  for (const std::string& line : _panel.scheduled)
  {
    paint.Text(paint.Fit(line, Hud::Typeface::Name, room), WINDOW_INSET, top, TEXT_COLOR, Hud::Typeface::Name);
    top += NAME_LINE_UNITS;
  }
  if (_panel.more > 0)
    paint.Text(std::format("and {} more", _panel.more), WINDOW_INSET, top, DIM_TEXT_COLOR, Hud::Typeface::Name);

  paint.Text("NEW ORDER FOR THE SELECTION", WINDOW_INSET, ORDERS_FORM_TOP + 3.0f, LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  for (std::size_t row = 0; row < _panel.rows.size() && row < Hud::ORDER_ROWS; ++row)
  {
    const Hud::OrderRow& order = _panel.rows[row];
    const float rowTop = ORDERS_ROWS_TOP + (static_cast<float>(row) * ORDER_ROW_UNITS);
    paint.Text(order.label, WINDOW_INSET, rowTop + ORDER_LABEL_TOP, ROW_LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
    (void)paint.Stepped(order.value, ORDER_VALUE_LEFT, rowTop + ORDER_ARROW_TOP, rowTop + ORDER_TEXT_TOP,
                        ORDERS_WINDOW_WIDTH - WINDOW_INSET - ORDER_VALUE_LEFT, true,
                        {.kind = Hud::ActionKind::StepOrder, .field = order.field, .step = -1},
                        {.kind = Hud::ActionKind::StepOrder, .field = order.field, .step = 1}, TEXT_COLOR, Hud::Typeface::Name);
  }
  paint.Text(paint.Fit(_panel.fires, Hud::Typeface::Name, room), WINDOW_INSET, ORDERS_FIRES_TOP, NUMBERS_COLOR, Hud::Typeface::Name);
  AddButton(_layout, _metrics, _scale, paint.Area(WINDOW_INSET, ORDERS_GIVE_TOP, room, BUTTON_HEIGHT, CARD_COLOR), _panel.give);
}

// The away window (Phase 5 design §11): how long the player was away, in the title face, and what happened, a line each.
constexpr float AWAY_WINDOW_WIDTH = 600.0f;

float AwayHeight(std::size_t _lines) noexcept
{
  return Hud::TITLE_BAR_UNITS + PADDING + TITLE_LINE_UNITS + (static_cast<float>(_lines) * NAME_LINE_UNITS) + PADDING;
}

void LayAway(Hud::Layout& _layout, const Hud::TextMetrics& _metrics, const Hud::AwayPanel& _panel, Outpost::WindowManager::Point _corner,
             float _scale)
{
  const std::size_t lines = std::min(_panel.lines.size(), Hud::AWAY_LINES);
  (void)OpenWindow(_layout, Outpost::WindowKind::Away, std::string(), _corner, AWAY_WINDOW_WIDTH, AwayHeight(lines) - Hud::TITLE_BAR_UNITS,
                   _scale);
  Painter paint(_layout, _metrics, _corner, _scale);
  paint.Text("WHILE YOU WERE AWAY", WINDOW_INSET, 11.0f, HEADER_LABEL_COLOR, Hud::Typeface::Label, paint.Tracking());
  const float room = AWAY_WINDOW_WIDTH - (2.0f * WINDOW_INSET);
  float top = Hud::TITLE_BAR_UNITS + PADDING;
  paint.Text(paint.Fit(_panel.since, Hud::Typeface::Title, room), WINDOW_INSET, top, TEXT_COLOR, Hud::Typeface::Title);
  top += TITLE_LINE_UNITS;
  for (std::size_t line = 0; line < lines; ++line)
  {
    paint.Text(paint.Fit(_panel.lines[line], Hud::Typeface::Name, room), WINDOW_INSET, top, NUMBERS_COLOR, Hud::Typeface::Name);
    top += NAME_LINE_UNITS;
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

float Outpost::Hud::TextMetrics::Width(Typeface _face, std::string_view _text, float _trackingUnits) const noexcept
{
  const auto font = static_cast<std::size_t>(_face);
  if (font >= m_fonts.size() || m_scale <= 0.0f)
    return 0.0f;
  return m_fonts[font].Width(_text, std::round(_trackingUnits * m_scale)) / m_scale;
}

std::string Outpost::Hud::TextMetrics::Fit(Typeface _face, std::string_view _text, float _widthUnits, float _trackingUnits) const
{
  if (Width(_face, _text, _trackingUnits) <= _widthUnits)
    return std::string(_text);
  // The longest start of the text that fits with the dots after it, less any space it would end on. ends[n] is where the
  // first n characters end, counting a character of several UTF-8 bytes once.
  constexpr std::string_view DOTS = "...";
  std::vector<std::size_t> ends{0};
  for (std::size_t index = 0; index < _text.size();)
  {
    (void)Neuron::NextCodePoint(_text, index);
    ends.push_back(index);
  }
  for (std::size_t characters = ends.size() - 1; characters-- > 0;)
  {
    std::string_view start = _text.substr(0, ends[characters]);
    while (!start.empty() && start.back() == ' ')
      start.remove_suffix(1);
    std::string cut = std::string(start) + std::string(DOTS);
    if (Width(_face, cut, _trackingUnits) <= _widthUnits)
      return cut;
  }
  return {};
}

std::vector<std::string> Outpost::Hud::TextMetrics::Wrap(Typeface _face, std::string_view _text, float _widthUnits) const
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
    if (Width(_face, joined) <= _widthUnits)
    {
      line = std::move(joined);
      continue;
    }
    if (!line.empty())
      lines.push_back(std::move(line));
    line = Fit(_face, word, _widthUnits);
  }
  if (!line.empty())
    lines.push_back(std::move(line));
  return lines;
}

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

std::string Hud::DescribeDerelict(const Snapshot& _newest, const EntityView& _derelict, std::uint32_t _ticksPerSecond)
{
  std::string text = std::format("Derelict: {} Ore", WithThousands(_derelict.salvageOre));
  if (_derelict.salvageTopic.IsValid())
    text += std::format(", and part of {}'s research time", TopicNameOf(_newest, _derelict.salvageTopic));
  if (_derelict.salvagePermille > 0)
    text += std::format("{}salvaged {}%", DOT, _derelict.salvagePermille / 10);
  if (_derelict.remembered)
    text += std::format(" ({})", LastSeen(_newest, _derelict, _ticksPerSecond));
  return text;
}

std::string Hud::DescribeMemory(const Snapshot& _newest, const EntityView& _structure, std::uint32_t _ticksPerSecond)
{
  const auto type = std::ranges::find(_newest.structureTypes, _structure.structure, &StructureTypeView::structure);
  std::string text = std::format("{}{}{}", type != _newest.structureTypes.end() ? type->nameUtf8 : std::string("Structure"), DOT,
                                 LastSeen(_newest, _structure, _ticksPerSecond));
  if (_structure.builtPermille < PERMILLE)
    text += std::format(", {}% built", _structure.builtPermille / 10);
  return text;
}

std::optional<Hud::Outcome> Hud::DescribeOutcome(const Snapshot& _newest, std::uint32_t _ticksPerSecond)
{
  // In a world no match ends: a player who lost waits for its seat to restart at its start (Phase 5 design §8).
  if (_newest.restartTick.has_value())
  {
    const std::uint64_t restart = *_newest.restartTick;
    const std::uint64_t seconds =
      _ticksPerSecond > 0 && restart > _newest.tick ? (restart - _newest.tick + _ticksPerSecond - 1) / _ticksPerSecond : 0;
    return Outcome{.title = "Empire fallen",
                   .detail = seconds > 0 ? std::format("It restarts at your start in {}", MinutesAndSeconds(seconds))
                                         : std::string("It restarts once your start is clear of the enemy")};
  }
  if (!_newest.matchOver)
    return std::nullopt;
  const std::string_view title = !_newest.winner.IsValid() ? "Draw" : _newest.winner == _newest.player ? "Victory" : "Defeat";
  const std::uint64_t seconds = _ticksPerSecond > 0 ? _newest.matchEndedTick / _ticksPerSecond : 0;
  // A domination says so: the side that ran out of tickets held less of the map (Phase 2 design §8); and so does a battle
  // matchup's end (ADR-083).
  std::string_view how;
  switch (_newest.ending)
  {
  case MatchEnding::Domination:
    how = "By domination. ";
    break;
  case MatchEnding::FleetDestroyed:
    how = "A fleet destroyed. ";
    break;
  case MatchEnding::TimeLimit:
    how = "Out of time. ";
    break;
  case MatchEnding::LostProduction:
    break;
  }
  return Outcome{.title = std::string(title), .detail = std::format("{}Match length {}", how, MinutesAndSeconds(seconds))};
}

Hud::OrdersPanel Hud::DescribeOrders(const Snapshot& _newest, std::span<const EntityView> _entities, std::span<const EntityId> _selected,
                                     const OrderForm& _form, const PlayerClock& _clock, std::chrono::sys_seconds _now)
{
  OrdersPanel panel;
  const auto ships = [](std::size_t _count) { return std::format("{} {}", _count, _count == 1 ? "ship" : "ships"); };
  for (const ScheduledOrderView& order : _newest.scheduled)
  {
    if (panel.scheduled.size() == ORDERS_SHOWN)
    {
      ++panel.more;
      continue;
    }
    std::string text = DescribeScheduledOrder(order, _newest, _clock);
    text.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(text.front())));
    panel.scheduled.push_back(std::format("{}{}{}", text, DOT, ships(order.ships.size())));
  }

  const auto sectorName = [&_newest](std::int32_t _sector, std::string_view _none)
  {
    const auto sector = std::ranges::find(_newest.sectors, _sector, &SectorView::id);
    return sector != _newest.sectors.end() ? sector->nameUtf8 : std::string(_none);
  };
  panel.rows.push_back({.label = "WHEN", .value = std::string(OrderForm::NameOf(_form.Trigger())), .field = OrderField::Trigger});
  const bool timed = _form.Trigger() == ScheduledTriggerKind::TimeOfDay;
  if (timed)
  {
    panel.rows.push_back({.label = "HOUR", .value = std::format("{:02}", _form.Hour()), .field = OrderField::Hour});
    panel.rows.push_back({.label = "MINUTE", .value = std::format("{:02}", _form.Minute()), .field = OrderField::Minute});
  }
  else
    panel.rows.push_back(
      {.label = "WATCHING", .value = sectorName(_form.TriggerSector(), "None held"), .field = OrderField::TriggerSector});
  panel.rows.push_back({.label = "DO", .value = std::string(OrderForm::NameOf(_form.Action())), .field = OrderField::Action});
  if (!_newest.sectors.empty())
    panel.rows.push_back({.label = "WHERE", .value = sectorName(_form.ActionSector(), "None"), .field = OrderField::ActionSector});
  if (_form.TakesTarget())
  {
    const std::vector<const EntityView*> targets = _form.Targets(_newest, _entities);
    const auto target = std::ranges::find(targets, _form.Target(), &EntityView::id);
    panel.rows.push_back({.label = "TARGET",
                          .value = target != targets.end() ? std::format("{} ({} of {})", OrderForm::NameOf(**target, _newest),
                                                                         (target - targets.begin()) + 1, targets.size())
                                                           : std::string("None there"),
                          .field = OrderField::Target});
  }
  const std::optional<std::int32_t> condition = _form.Condition();
  panel.rows.push_back({.label = "UNLESS",
                        .value = condition.has_value() ? std::format("Over {} enemy CP there", *condition) : "Never",
                        .field = OrderField::Condition});

  if (timed)
  {
    const std::chrono::sys_seconds moment = _clock.NextMoment(_now, _form.Hour(), _form.Minute());
    panel.fires = std::format("Fires at {} on your clock, in {}", _clock.Reading(moment),
                              MinutesAndSeconds(static_cast<std::uint64_t>((moment - _now).count())));
  }
  else
    panel.fires = "Fires once, the first time it happens";
  const std::size_t given = _form.ShipsFor(_selected, _entities).size();
  std::string missing = _form.Missing(_selected, _newest, _entities);
  panel.give = {.label = given > 0 ? std::format("Give to {}|", ships(given)) : std::string("Give the order|"),
                .action = {.kind = ActionKind::GiveOrder},
                .enabled = missing.empty(),
                .note = std::move(missing)};
  return panel;
}

Hud::AwayPanel Hud::DescribeAway(const Snapshot& _newest, const AwayReport& _report, std::uint64_t _untilTick,
                                 std::uint32_t _ticksPerSecond)
{
  const std::uint64_t ticks = _untilTick > _report.sinceTick ? _untilTick - _report.sinceTick : 0;
  AwayPanel panel{.since = std::format("Away for {}", MinutesAndSeconds(_ticksPerSecond > 0 ? ticks / _ticksPerSecond : 0)), .lines = {}};
  const auto count = [](std::uint32_t _count, std::string_view _thing)
  { return _count == 0 ? std::format("no {}s", _thing) : std::format("{} {}{}", _count, _thing, _count == 1 ? "" : "s"); };
  const auto sectorName = [&_newest](std::int32_t _sector)
  {
    const auto sector = std::ranges::find(_newest.sectors, _sector, &SectorView::id);
    return sector != _newest.sectors.end() ? sector->nameUtf8 : std::string("the field");
  };
  const auto names = [&](const std::vector<std::int32_t>& _sectors)
  {
    std::string text;
    for (const std::int32_t sector : _sectors)
      text += std::format("{}{}", text.empty() ? "" : ", ", sectorName(sector));
    return text;
  };
  panel.lines.push_back(std::format("Built {} and {}", count(_report.shipsBuilt, "ship"), count(_report.structuresBuilt, "structure")));
  panel.lines.push_back(std::format("Lost {} and {}", count(_report.shipsLost, "ship"), count(_report.structuresLost, "structure")));
  if (!_report.sectorsGained.empty())
    panel.lines.push_back(std::format("Sectors gained: {}", names(_report.sectorsGained)));
  if (!_report.sectorsLost.empty())
    panel.lines.push_back(std::format("Sectors lost: {}", names(_report.sectorsLost)));
  for (const EventView& fired : _report.ordersFired)
    panel.lines.push_back(DescribeFiredOrder(fired, sectorName(fired.sector)));
  return panel;
}

Hud::Layout Hud::LayMenu(const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor,
                         const MenuState& _state)
{
  const float scale = Scale(_widthPixels, _heightPixels, _factor);
  Layout layout{.fontPixels = FONT_UNITS * scale, .panels = {}, .texts = {}, .actions = {}, .minimap = {}, .mapSizeMeters = 0.0f};
  // A skirmish at each difficulty (ADR-065), a world to join when there is one (ADR-078), then Quit.
  std::vector<Button> buttons{{.label = "Easy skirmish", .action = {.kind = ActionKind::StartSkirmish, .difficulty = Difficulty::Easy}},
                              {.label = "Normal skirmish", .action = {.kind = ActionKind::StartSkirmish, .difficulty = Difficulty::Normal}},
                              {.label = "Hard skirmish", .action = {.kind = ActionKind::StartSkirmish, .difficulty = Difficulty::Hard}}};
  if (_state.joinWorld)
    buttons.push_back({.label = "Join world", .action = {.kind = ActionKind::JoinWorld}});
  buttons.push_back({.label = "Quit", .action = {.kind = ActionKind::Quit}});
  // The notice, such as why the last game ended, wrapped to the menu's width under its subtitle.
  const std::vector<std::string> notice =
    _state.notice.empty() ? std::vector<std::string>() : _metrics.Wrap(Typeface::Name, _state.notice, MENU_WIDTH - (2.0f * PADDING));
  const auto noticeUnits = static_cast<float>(notice.size()) * NAME_LINE_UNITS;
  const auto count = static_cast<float>(buttons.size());
  const float heightUnits = (2.0f * PADDING) + TITLE_LINE_UNITS + NAME_LINE_UNITS + noticeUnits + BUTTON_GAP + (count * BUTTON_HEIGHT) +
                            ((count - 1.0f) * BUTTON_GAP);
  const WindowManager::Point corner{.xUnits = (static_cast<float>(_widthPixels) / scale / 2.0f) - (MENU_WIDTH / 2.0f),
                                    .yUnits = (static_cast<float>(_heightPixels) / scale / 2.0f) - (heightUnits / 2.0f)};
  (void)Frame(layout, corner.xUnits * scale, corner.yUnits * scale, MENU_WIDTH * scale, heightUnits * scale, scale);
  Painter paint(layout, _metrics, corner, scale);
  paint.Text("Outpost Commander", PADDING, PADDING, GOLD_COLOR, Typeface::Title);
  paint.Text("A skirmish against the AI", PADDING, PADDING + TITLE_LINE_UNITS, DIM_TEXT_COLOR, Typeface::Name);
  float top = PADDING + TITLE_LINE_UNITS + NAME_LINE_UNITS;
  for (const std::string& line : notice)
  {
    paint.Text(line, PADDING, top, WARNING_COLOR, Typeface::Name);
    top += NAME_LINE_UNITS;
  }
  top += BUTTON_GAP;
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
                                             const Designer* _designer, std::optional<Action> _hovered, std::uint32_t _ticksPerSecond)
{
  Content content{.ore = _newest.ore,
                  .oreIncomeHundredthsPerSecond = _newest.oreIncomeHundredthsPerSecond,
                  .selection = {},
                  .buttons = {},
                  .hint = {},
                  .status = StatusLinesOf(_newest, _entities),
                  .designer = std::nullopt,
                  .mapSizeMeters = _newest.mapSizeMeters,
                  .marks = {}};
  if (_designer != nullptr)
    content.designer = DescribeDesigner(_newest, *_designer, _hovered);

  const auto sideOf = [&_newest](PlayerId _owner) {
    return !_owner.IsValid() ? Side::Neutral : _owner == _newest.player ? Side::Own : _owner == PIRATES ? Side::Pirate : Side::Enemy;
  };
  if (!_newest.sectors.empty())
  {
    Territory territory{.nodes = static_cast<std::int32_t>(_newest.sectors.size()), .cap = _newest.nodeCap};
    content.sectors.reserve(_newest.sectors.size());
    // Which nodes the player could claim now, as the world marks them and the Relay's ghost judges them.
    const TerritoryMarks marked = MarkTerritory(_newest, _entities);
    for (const SectorView& sector : _newest.sectors)
    {
      // A sector the pirates guard is held by no one, and shown as theirs (ADR-073).
      const Side side = sector.guarded && !sector.holder.IsValid() ? Side::Pirate : sideOf(sector.holder);
      territory.ownNodes += side == Side::Own ? 1 : 0;
      territory.enemyNodes += side == Side::Enemy ? 1 : 0;
      content.sectors.push_back({.minXMeters = sector.minXMeters,
                                 .maxXMeters = sector.maxXMeters,
                                 .minZMeters = sector.minZMeters,
                                 .maxZMeters = sector.maxZMeters,
                                 .side = side,
                                 .suppressed = sector.suppressed,
                                 .cutOff = sector.cutOff,
                                 .node = sector.node,
                                 .claimable = content.sectors.size() < marked.nodes.size() &&
                                              marked.nodes[content.sectors.size()].look == TerritoryMarks::Look::Claimable});
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
                             .dry = entity.kind == EntityKind::Asteroid && entity.oreReserveHundredths == 0,
                             .remembered = entity.remembered});
  }

  const auto typeOf = [&_newest](StructureKind _kind) -> const StructureTypeView*
  {
    const auto type = std::ranges::find(_newest.structureTypes, _kind, &StructureTypeView::structure);
    return type != _newest.structureTypes.end() ? &*type : nullptr;
  };
  const bool capReached = AtNodeCap(_newest.sectors, _newest.entities, _newest.player, _newest.nodeCap);
  if (_placing.has_value())
  {
    const StructureTypeView* type = typeOf(*_placing);
    content.hint = std::format("Placing {}: left-click to build, right-click to cancel", type != nullptr ? type->nameUtf8 : "a structure");
    // The reason a Relay's ghost is red everywhere (Phase 3 design §7).
    if (*_placing == StructureKind::Relay && capReached)
      content.hint = "Relay: at the node cap; upgrade the Command Station to claim more";
  }

  const auto nameOf = [&_newest](DesignId _design) { return DesignNameOf(_newest, _design); };

  // A structure is selected on its own (PlayerControls): its kind, its state, and the windows it opens.
  if (_selected.size() == 1)
  {
    const auto structure = std::ranges::find(_entities, _selected.front(), &EntityView::id);
    if (structure != _entities.end() && structure->kind == EntityKind::Structure)
    {
      const StructureTypeView* type = typeOf(structure->structure);
      // A kind that grows names its level, "Shipyard 01 · L2" (Phase 3 design §9).
      const bool grows = type != nullptr && !type->levels.empty();
      std::string name = type != nullptr ? type->nameUtf8 : std::string("Structure");
      if (structure->shipyardNumber > 0)
        name += std::format(" {:02}", structure->shipyardNumber);
      if (grows)
        name += std::format("{}L{}", DOT, structure->level);
      content.selection.push_back(std::move(name));
      // One the player only remembers says how long ago it was seen, and that what follows is as it was then (interface
      // plan 2, task UI1.2).
      const bool memory = structure->remembered;
      const std::string_view then = memory ? " when seen" : "";
      if (memory)
        content.selection.push_back(Capitalized(LastSeen(_newest, *structure, _ticksPerSecond)));
      if (structure->builtPermille < PERMILLE)
      {
        content.selection.push_back(memory ? std::format("{}% built{}", structure->builtPermille / 10, then)
                                           : std::format("Under construction, {}%", structure->builtPermille / 10));
      }
      if (structure->upgradePermille.has_value())
        content.selection.push_back(std::format("Upgrading to L{}, {}%{}", structure->level + 1, *structure->upgradePermille / 10, then));
      content.selection.push_back(std::format("Hit points {} / {}{}", WithThousands(WholePoints(structure->hitPointsHundredths)),
                                              WithThousands(WholePoints(structure->maxHitPointsHundredths)), then));
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
      // Its queue and what it can add to it are its windows' (Phase 1 design §12; owner, 2026-10-03): its panel says what it
      // is doing (ADR-066) and offers to open them, once it is the player's own and finished.
      if (structure->owner == _newest.player && structure->builtPermille >= PERMILLE)
      {
        if (structure->structure == StructureKind::CommandStation || structure->structure == StructureKind::Shipyard)
        {
          content.selection.push_back(WorkLine("Building", structure->queue.empty() ? std::string() : FrontJobNameOf(_newest, *structure),
                                               structure->jobPermille, structure->queue.size(),
                                               WaitsOnFleetCap(_newest, *structure) ? "the fleet cap" : "Ore"));
        }
        else if (structure->structure == StructureKind::ResearchLab)
        {
          content.selection.push_back(
            WorkLine("Researching", structure->research.empty() ? std::string() : TopicNameOf(_newest, structure->research.front()),
                     structure->jobPermille, structure->research.size()));
        }
        // Each window's button shows the key that opens it too (task 15.2).
        const Button production{
          .label = "Production", .action = {.kind = ActionKind::OpenProduction, .producer = structure->id}, .key = KeyCap(KEY_PRODUCTION)};
        if (structure->structure == StructureKind::CommandStation)
          content.buttons.push_back(production);
        else if (structure->structure == StructureKind::Shipyard)
        {
          content.buttons.push_back(production);
          content.buttons.push_back({.label = "Ship designer",
                                     .action = {.kind = ActionKind::OpenDesigner, .producer = structure->id},
                                     .key = KeyCap(KEY_DESIGNER)});
        }
        else if (structure->structure == StructureKind::ResearchLab)
        {
          content.buttons.push_back(
            {.label = "Research", .action = {.kind = ActionKind::OpenResearch, .producer = structure->id}, .key = KeyCap(KEY_RESEARCH)});
        }
        // The next level: what it gives, and a button with its time and cost, dim while one is being built or the Ore is
        // short (Phase 3 design §9).
        const auto next = static_cast<std::size_t>(std::max(0, structure->level - 1));
        if (grows && next < type->levels.size())
        {
          const StructureLevelView& level = type->levels[next];
          const bool upgrading = structure->upgradePermille.has_value();
          // A Shipyard's next level names the hulls it adds, a Research Lab's the tier or the slot it gives, and a Command
          // Station's the nodes it lets the player hold, the Defence guns it adds (Phase 3 design §5–§7) and its fleet cap
          // (Phase 4 design §5).
          std::string gives;
          if (level.nodes > 0)
            gives = std::format("{} nodes, ", level.nodes);
          if (level.guns > 0)
            gives += std::format("{} Defence guns, ", level.guns);
          if (level.commandPoints > 0)
            gives += std::format("fleet {}, ", level.commandPoints);
          if (level.opensTier > 0)
            gives = std::format("tier {}, ", level.opensTier);
          else if (level.researchSlots > 1)
            gives = "a second research slot, ";
          if (structure->structure == StructureKind::Shipyard)
          {
            for (const HullView& hull : _newest.hulls)
            {
              if (hull.shipyardLevel == structure->level + 1)
                gives += std::format("{}{}", gives.empty() ? "" : " and ", hull.nameUtf8);
            }
            if (!gives.empty())
              gives += " hulls, ";
          }
          content.selection.push_back(
            std::format("L{}: {}{} hit points", structure->level + 1, gives, WithThousands(WholePoints(level.maxHitPointsHundredths))));
          // The topics a Research Lab's next level needs that the player has not researched (Phase 3 design §6).
          std::string missing;
          for (const ResearchTopicId required : level.prerequisites)
          {
            const auto topic = std::ranges::find(_newest.research, required, &ResearchTopicView::id);
            if (topic != _newest.research.end() && !topic->researched)
              missing += std::format("{}{}", missing.empty() ? "" : " and ", topic->nameUtf8);
          }
          if (!missing.empty())
            content.selection.push_back(std::format("L{} needs {}", structure->level + 1, missing));
          content.buttons.push_back(
            {.label = std::format("Upgrade to L{}{}{}|{}", structure->level + 1, DOT,
                                  MinutesAndSeconds(static_cast<std::uint64_t>(std::llround(level.buildSeconds))), level.cost),
             .action = {.kind = ActionKind::Upgrade, .producer = structure->id},
             .enabled = !upgrading && missing.empty() && _newest.ore >= level.cost,
             .note = upgrading         ? std::format("UPGRADING{}{}%", DOT, *structure->upgradePermille / 10)
                     : missing.empty() ? std::string()
                                       : std::string("NEEDS RESEARCH"),
             .progressPermille = structure->upgradePermille});
        }
        else if (grows)
          content.selection.emplace_back("Top level");
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
  // The first ship's retreat, whether the others share it, and how many are going back to be repaired (Phase 4 design §10).
  std::optional<RetreatThreshold> retreat;
  bool mixedRetreat = false;
  size_t retreating = 0;
  for (const EntityId id : _selected)
  {
    const auto ship = std::ranges::find(_entities, id, &EntityView::id);
    if (ship == _entities.end() || ship->kind != EntityKind::Ship)
      continue;
    constructors = constructors || ship->role == ShipRole::Constructor;
    holding = holding || ship->standing == StandingOrder::HoldSector;
    patrolling = patrolling || ship->standing == StandingOrder::Patrol;
    mixedRetreat = mixedRetreat || (retreat.has_value() && *retreat != ship->retreat);
    if (!retreat.has_value())
      retreat = ship->retreat;
    retreating += ship->retreating ? 1 : 0;
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
  if (retreating > 0)
    content.selection.push_back(ships == 1 ? std::string("Retreating to be repaired")
                                           : std::format("{} retreating to be repaired", retreating));
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
      const bool secondLab = type.structure == StructureKind::ResearchLab && hasLab;
      // A Relay past the Command Station's cap waits for the station's next level (Phase 3 design §7).
      const bool capped = type.structure == StructureKind::Relay && capReached;
      content.buttons.push_back({.label = std::format("{}|{}", type.nameUtf8, type.cost),
                                 .action = {.kind = ActionKind::Build, .structure = type.structure},
                                 .enabled = !secondLab && !capped && _newest.ore >= type.cost,
                                 .note = secondLab ? std::string("ONE PER PLAYER")
                                         : capped  ? std::string("NODE CAP")
                                                   : std::string()});
    }
  }
  // The selection's retreat, in the corner below the rest, which a press steps for every ship in it, from the first one's
  // (Phase 4 design §10, §13).
  if (retreat.has_value())
  {
    content.buttons.push_back({.label = mixedRetreat ? std::string("Retreat: mixed") : RetreatWords(*retreat),
                               .action = {.kind = ActionKind::SetRetreat, .retreat = NextRetreat(*retreat)},
                               .enabled = true});
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
  if (!panel.queue.empty() && WaitsOnFleetCap(_newest, *_producer))
    panel.queue.front().capped = true;

  // A Constructor at the Command Station; each saved design at a Shipyard, with its abbreviation (design §5).
  const bool room = _producer->queue.size() < QUEUE_LIMIT;
  if (station)
  {
    panel.options.push_back({.name = std::string(CONSTRUCTOR_NAME),
                             .cost = _newest.constructorCost,
                             .action = {.kind = ActionKind::Queue, .producer = _producer->id},
                             .enabled = room && _newest.ore >= _newest.constructorCost,
                             .time = std::format("{} s", Tenths(_newest.constructorBuildSeconds))});
    return panel;
  }
  // A design's card says how long one takes at the Shipyards' speed, and how it does against each hull, as the designer
  // rates it (ADR-068).
  const Best best = BestOfAll(_newest);
  panel.options.reserve(_newest.designs.size());
  for (const DesignView& design : _newest.designs)
  {
    // A design whose hull is above the Shipyard's level is dim, and says the level it needs (Phase 3 design §5).
    const auto hull = std::ranges::find(_newest.hulls, design.hull, &HullView::id);
    const std::int32_t level = hull != _newest.hulls.end() ? hull->shipyardLevel : 1;
    const bool leveled = _producer->level >= level;
    QueueOption& option = panel.options.emplace_back(QueueOption{
      .name = design.nameUtf8,
      .detail = leveled ? DesignCodeOf(_newest, design) : std::format("{}{}NEEDS L{}", DesignCodeOf(_newest, design), DOT, level),
      .cost = design.cost,
      .action = {.kind = ActionKind::Queue, .producer = _producer->id, .design = design.id},
      .enabled = leveled && room && _newest.ore >= design.cost});
    if (const std::optional<DesignStats> stats = StatsOf(_newest, design))
    {
      option.time = std::format("{} s", Tenths(stats->buildSeconds / _newest.shipyardBuildSpeedFactor));
      for (size_t i = 0; i < _newest.hulls.size(); ++i)
      {
        option.strengths.push_back(
          {.hull = Abbreviation(_newest.hulls[i].nameUtf8),
           .rating = RatingOf(ShareOf(
             FormationDamagePerSecond(*stats, _newest.hulls[i].armorHundredths, static_cast<float>(_newest.hulls[i].footprintRadiusMeters)),
             best.damage[i]))});
      }
    }
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
    // A second topic researched beside the first shows how far it has come too (Phase 3 design §6).
    if (found->secondJobPermille > 0 && panel.queue.size() > 1)
      panel.queue[1] = {.name = std::move(panel.queue[1].name), .front = true, .permille = found->secondJobPermille};
  }

  // The level of the Research Lab that opens each tier (Phase 3 design §6).
  const auto labType = std::ranges::find(_newest.structureTypes, StructureKind::ResearchLab, &StructureTypeView::structure);
  const auto levelFor = [&](std::int32_t _tier)
  {
    for (std::size_t level = 0; labType != _newest.structureTypes.end() && level < labType->levels.size(); ++level)
    {
      if (labType->levels[level].opensTier == _tier)
        return static_cast<std::int32_t>(level) + 2;
    }
    return 1;
  };

  // Each topic not researched or queued yet, and what it does; one whose prerequisites are neither, or of a tier the Lab
  // has not opened, is dim, and says what it needs (design §8, Phase 3 design §6).
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
    if (topic.tier > _newest.researchTier)
      needs.insert(needs.begin(), std::format("RESEARCH LAB L{}", levelFor(topic.tier)));
    const bool enabled = canResearch && needs.empty() && _newest.ore >= topic.cost;
    panel.topics.push_back(
      {.name = topic.nameUtf8,
       .effect = topic.effectUtf8,
       .cost = topic.cost,
       .time = std::format("TIER {}{}{} s{}", topic.tier, DOT, Tenths(topic.researchSeconds),
                           topic.recovered ? std::format("{}SALVAGED", DOT) : ""),
       .needs = std::move(needs),
       .action = {.kind = ActionKind::Research, .producer = found != nullptr ? found->id : EntityId{}, .topic = topic.id},
       .enabled = enabled,
       .tier = topic.tier});
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

static_assert(Hud::FACE_UNITS.size() == static_cast<std::size_t>(Hud::Typeface::Detail) + 1, "a size for every typeface");

std::vector<Neuron::FontDesc> Hud::Typefaces()
{
  // Weights as DirectWrite counts them: 600 is semibold, 700 bold; Consolas, which has no semibold, meets 600 with its bold.
  // Sizes are FaceUnits' (ADR-062).
  const std::vector<std::wstring> condensed{L"Bahnschrift"};
  const std::vector<std::wstring> figures{L"Cascadia Mono", L"Consolas"};
  return {
    {.families = {L"Segoe UI"}, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = FaceUnits(Typeface::Body)},
    {.families = condensed, .weight = 700, .stretch = Neuron::FontStretch::SemiCondensed, .emUnits = FaceUnits(Typeface::Title)},
    {.families = condensed, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = FaceUnits(Typeface::Label)},
    {.families = condensed, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = FaceUnits(Typeface::Name)},
    {.families = figures, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = FaceUnits(Typeface::Figure)},
    {.families = figures, .weight = 700, .stretch = Neuron::FontStretch::Normal, .emUnits = FaceUnits(Typeface::LargeFigure)},
    {.families = figures, .weight = 400, .stretch = Neuron::FontStretch::Normal, .emUnits = FaceUnits(Typeface::Detail)},
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

float Hud::Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor) noexcept
{
  return _factor *
         std::min(static_cast<float>(_widthPixels) / REFERENCE_WIDTH_UNITS, static_cast<float>(_heightPixels) / REFERENCE_HEIGHT_UNITS);
}

bool Hud::WindowsFit(const Snapshot* _newest, std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor)
{
  const float scale = Scale(_widthPixels, _heightPixels, _factor);
  if (scale <= 0.0f)
    return false;
  const float screenWidthUnits = static_cast<float>(_widthPixels) / scale;
  const float screenHeightUnits = static_cast<float>(_heightPixels) / scale;
  const auto fits = [&](float _widthUnits, float _heightUnits)
  { return _widthUnits + (2.0f * MARGIN) <= screenWidthUnits && _heightUnits + (2.0f * MARGIN) <= screenHeightUnits; };
  if (!fits(CONTROLS_WINDOW_WIDTH, ControlsHeight(KeyBindings())))
    return false;
  if (_newest == nullptr)
    return true;
  if (!fits(ORDERS_WINDOW_WIDTH, ORDERS_WINDOW_HEIGHT) || !fits(AWAY_WINDOW_WIDTH, AwayHeight(AWAY_LINES)))
    return false;
  // The designer with every component of the match, locked or not, as tall as it grows.
  Designer designer;
  designer.Update(*_newest);
  return fits(DESIGNER_WIDTH, ExtentOf(DescribeDesigner(*_newest, designer, std::nullopt)).height);
}

float Hud::StepInterface(float _factor, int _step, const Snapshot* _newest, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  // The step _factor stands at, or the one below it.
  std::size_t at = 0;
  for (std::size_t step = 0; step < INTERFACE_STEPS.size(); ++step)
  {
    if (INTERFACE_STEPS[step] <= _factor + 0.001f)
      at = step;
  }
  if (_step > 0 && at + 1 < INTERFACE_STEPS.size() && WindowsFit(_newest, _widthPixels, _heightPixels, INTERFACE_STEPS[at + 1]))
    return INTERFACE_STEPS[at + 1];
  if (_step < 0 && at > 0)
    return INTERFACE_STEPS[at - 1];
  // Down to the largest that fits, and never below the first step.
  while (at > 0 && !WindowsFit(_newest, _widthPixels, _heightPixels, INTERFACE_STEPS[at]))
    --at;
  return INTERFACE_STEPS[at];
}

Hud::Layout Hud::Lay(const Content& _content, const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                     std::span<const PlanePosition> _view, const WindowManager* _windows, float _factor)
{
  const float scale = Scale(_widthPixels, _heightPixels, _factor);
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
    paint.DiamondAndFigureFrom(_content.ore, PADDING, (ORE_PANEL_HEIGHT - TITLE_LINE_UNITS) / 2.0f, Typeface::Title, GOLD_COLOR);
    const std::int32_t income = _content.oreIncomeHundredthsPerSecond;
    const std::string incomeText = income % HUNDREDTHS == 0 ? std::format("+{}/s", income / HUNDREDTHS)
                                                            : std::format("+{:.1f}/s", static_cast<double>(income) / HUNDREDTHS);
    paint.RightText(incomeText, ORE_PANEL_WIDTH - PADDING, ((ORE_PANEL_HEIGHT - FIGURE_LINE_UNITS) / 2.0f) + 2.0f,
                    income > 0 ? NUMBERS_COLOR : WARNING_COLOR, Typeface::Figure);
  }

  // Under the Ore: what the Research Lab and the Shipyards are doing (ADR-066).
  const float underOreUnits = MARGIN + ORE_PANEL_HEIGHT + PANEL_GAP;
  const float underStatusUnits = underOreUnits + STATUS_PANEL_KEPT_UNITS + PANEL_GAP;

  // Under the status panel's place: the nodes each side holds, and each side's tickets, the player's in its color and the
  // enemy's in theirs (ADR-056, ADR-057).
  if (_content.territory.has_value())
  {
    const Territory& territory = *_content.territory;
    const bool tickets = territory.ownTickets.has_value() && territory.enemyTickets.has_value();
    const float heightUnits = (2.0f * TERRITORY_INSET_UNITS) + (tickets ? 2.0f : 1.0f) * NAME_LINE_UNITS;
    Painter paint = frame({.xUnits = MARGIN, .yUnits = underStatusUnits}, TERRITORY_PANEL_WIDTH, heightUnits);
    const auto row = [&](const std::string& _label, const std::string& _own, const std::string& _enemy, float _top)
    {
      const float figureTop = _top + ((NAME_LINE_UNITS - FIGURE_LINE_UNITS) / 2.0f) + 2.0f;
      const float right = TERRITORY_PANEL_WIDTH - PADDING;
      std::string own = std::format("{} : ", _own);
      const float ownRight = right - paint.Width(_enemy, Typeface::Figure);
      const float labelRoom = ownRight - paint.Width(own, Typeface::Figure) - PADDING - FIT_GAP_UNITS;
      paint.Text(paint.Fit(_label, Typeface::Name, labelRoom), PADDING, _top, TEXT_COLOR, Typeface::Name);
      paint.RightText(_enemy, right, figureTop, ENEMY_COLOR, Typeface::Figure);
      paint.RightText(std::move(own), ownRight, figureTop, OWN_COLOR, Typeface::Figure);
    };
    // The player's own against its Command Station's cap, "4 / 5" (Phase 3 design §7).
    const std::string own =
      territory.cap > 0 ? std::format("{} / {}", territory.ownNodes, territory.cap) : std::to_string(territory.ownNodes);
    row(std::format("Nodes of {}", territory.nodes), own, std::to_string(territory.enemyNodes), TERRITORY_INSET_UNITS);
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
    const float territoryUnits =
      _content.territory.has_value() ? (2.0f * TERRITORY_INSET_UNITS) + ((tickets ? 2.0f : 1.0f) * NAME_LINE_UNITS) + PANEL_GAP : 0.0f;
    const float topUnits = underStatusUnits + territoryUnits;
    const float heightUnits = (2.0f * TERRITORY_INSET_UNITS) + (static_cast<float>(_content.alerts.size()) * NAME_LINE_UNITS);
    Painter paint = frame({.xUnits = MARGIN, .yUnits = topUnits}, ALERT_PANEL_WIDTH, heightUnits);
    for (size_t line = 0; line < _content.alerts.size(); ++line)
    {
      paint.Text(paint.Fit(_content.alerts[line].first, Typeface::Name, ALERT_PANEL_WIDTH - (2.0f * PADDING)), PADDING,
                 TERRITORY_INSET_UNITS + (static_cast<float>(line) * NAME_LINE_UNITS), line == 0 ? WARNING_COLOR : TEXT_COLOR,
                 Typeface::Name);
    }
  }
  // The status panel: a line for the Research Lab and one for the Shipyards, an idle one in the warning's color, each a place
  // to click that opens its window (ADR-066). It is as wide as its longest line, within its bounds.
  if (!_content.status.empty())
  {
    float textUnits = 0.0f;
    for (const StatusLine& line : _content.status)
      textUnits = std::max(textUnits, _metrics.Width(Typeface::Name, line.text));
    const float widthUnits = std::clamp(textUnits + (2.0f * PADDING), STATUS_PANEL_MIN_WIDTH, STATUS_PANEL_WIDTH);
    const float heightUnits = (2.0f * TERRITORY_INSET_UNITS) + (static_cast<float>(_content.status.size()) * NAME_LINE_UNITS);
    Painter paint = frame({.xUnits = MARGIN, .yUnits = underOreUnits}, widthUnits, heightUnits);
    for (size_t line = 0; line < _content.status.size(); ++line)
    {
      const StatusLine& status = _content.status[line];
      const float top = TERRITORY_INSET_UNITS + (static_cast<float>(line) * NAME_LINE_UNITS);
      // The line's row across the panel takes the click, the first and the last reaching to the panel's edge.
      const float rowTop = line == 0 ? 0.0f : top;
      const float rowBottom = line + 1 == _content.status.size() ? heightUnits : top + NAME_LINE_UNITS;
      paint.Press(paint.Area(0.0f, rowTop, widthUnits, rowBottom - rowTop, WINDOW_COLOR), status.action);
      paint.Text(paint.Fit(status.text, Typeface::Name, widthUnits - (2.0f * PADDING)), PADDING, top,
                 status.idle ? WARNING_COLOR : TEXT_COLOR, Typeface::Name);
    }
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
      DirectX::XMFLOAT4 color = _side == Side::Own ? OWN_COLOR : _side == Side::Pirate ? PIRATE_COLOR : ENEMY_COLOR;
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
        // A memory is drawn over the fog, below.
        if ((mark.side != Side::Neutral) == sided && !mark.remembered)
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
                                       : mark.side == Side::Pirate              ? PIRATE_COLOR
                                       : mark.kind == EntityKind::Derelict      ? DERELICT_COLOR
                                       : mark.kind == EntityKind::AsteroidField ? ASTEROID_FIELD_COLOR
                                       : mark.kind == EntityKind::Asteroid      ? ORE_ASTEROID_COLOR
                                                                                : NEUTRAL_COLOR;
      const float markLeft = at.x - (side / 2.0f);
      const float markTop = at.y - (side / 2.0f);
      if (mark.kind == EntityKind::Asteroid && mark.side == Side::Neutral)
      {
        // An ore asteroid, dry or not, as an outline, its top and bottom across the whole square and its sides between them.
        const float line = ORE_OUTLINE_UNITS * scale;
        layout.panels.push_back({markLeft, markTop, side, line, color});
        layout.panels.push_back({markLeft, markTop + side - line, side, line, color});
        layout.panels.push_back({markLeft, markTop + line, line, side - (2.0f * line), color});
        layout.panels.push_back({markLeft + side - line, markTop + line, line, side - (2.0f * line), color});
      }
      else
        layout.panels.push_back({markLeft, markTop, side, side, color});
    }

    // The fog over the marks (ADR-024), filled by the client from the fog's texture across the whole map (ADR-052).
    if (_content.fog)
    {
      layout.panels.push_back(
        {layout.minimap.left, layout.minimap.top, layout.minimap.width, layout.minimap.height, {0.0f, 0.0f, 0.0f, 1.0f}, Fill::Fog});
    }

    // Over the fog, since both sides know who holds what and where the pirates are: every sector's border, then each held
    // or guarded sector's outline, dashed while it is cut off, and stripes over a suppressed one.
    const float sectorLine = SECTOR_LINE_UNITS * scale;
    const auto outline = [&layout, sectorLine](const Rect& _area)
    {
      layout.panels.push_back({_area.left, _area.top, _area.width, sectorLine, _area.color});
      layout.panels.push_back({_area.left, _area.top + _area.height - sectorLine, _area.width, sectorLine, _area.color});
      layout.panels.push_back({_area.left, _area.top, sectorLine, _area.height, _area.color});
      layout.panels.push_back({_area.left + _area.width - sectorLine, _area.top, sectorLine, _area.height, _area.color});
    };
    // Dashes along each side, each side starting with one.
    const auto dashedOutline = [&layout, sectorLine, scale](const Rect& _area)
    {
      const float dash = CUT_OFF_DASH_UNITS * scale;
      const float period = (CUT_OFF_DASH_UNITS + CUT_OFF_GAP_UNITS) * scale;
      for (int index = 0; static_cast<float>(index) * period < _area.width; ++index)
      {
        const float along = static_cast<float>(index) * period;
        const float length = std::min(dash, _area.width - along);
        layout.panels.push_back({_area.left + along, _area.top, length, sectorLine, _area.color});
        layout.panels.push_back({_area.left + along, _area.top + _area.height - sectorLine, length, sectorLine, _area.color});
      }
      for (int index = 0; static_cast<float>(index) * period < _area.height; ++index)
      {
        const float along = static_cast<float>(index) * period;
        const float length = std::min(dash, _area.height - along);
        layout.panels.push_back({_area.left, _area.top + along, sectorLine, length, _area.color});
        layout.panels.push_back({_area.left + _area.width - sectorLine, _area.top + along, sectorLine, length, _area.color});
      }
    };
    for (const SectorMark& sector : _content.sectors)
      outline(sectorRect(sector, LATTICE_COLOR, Fill::Solid));
    for (const SectorMark& sector : _content.sectors)
    {
      if (sector.side == Side::Neutral)
        continue;
      const Rect area = sectorRect(sector, sideColor(sector.side, SECTOR_OUTLINE_ALPHA), Fill::Solid);
      if (sector.cutOff)
        dashedOutline(area);
      else
        outline(area);
      if (sector.suppressed)
        layout.panels.push_back(sectorRect(sector, sideColor(sector.side, SECTOR_HATCH_ALPHA), Fill::Hatched));
    }

    // Every node, and round one the player could claim now an outline in its own color.
    for (const SectorMark& sector : _content.sectors)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(sector.node);
      const float dot = NODE_MARK_UNITS * scale;
      layout.panels.push_back({at.x - (dot / 2.0f), at.y - (dot / 2.0f), dot, dot,
                               sector.side == Side::Neutral ? FREE_NODE_COLOR : sideColor(sector.side, 1.0f)});
      if (sector.claimable)
      {
        const float side = CLAIMABLE_MARK_UNITS * scale;
        outline({at.x - (side / 2.0f), at.y - (side / 2.0f), side, side, OWN_COLOR});
      }
    }

    // What the player only remembers, as a cross in its side's color, over the fog that would dim it twice.
    for (const Mark& mark : _content.marks)
    {
      if (!mark.remembered)
        continue;
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(mark.position);
      const float across = MEMORY_MARK_UNITS * scale;
      const float arm = MEMORY_ARM_UNITS * scale;
      const DirectX::XMFLOAT4 color = mark.side == Side::Neutral ? DERELICT_COLOR : sideColor(mark.side, 1.0f);
      layout.panels.push_back({at.x - (across / 2.0f), at.y - (arm / 2.0f), across, arm, color});
      layout.panels.push_back({at.x - (arm / 2.0f), at.y - (across / 2.0f), arm, across, color});
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
  std::vector<WindowKind> backToFront{WindowKind::Production, WindowKind::Research, WindowKind::Designer,
                                      WindowKind::Controls,   WindowKind::Orders,   WindowKind::Away};
  if (_windows != nullptr)
    backToFront.assign(_windows->FrontToBack().rbegin(), _windows->FrontToBack().rend());
  const auto place = [&](WindowKind _kind, WindowManager::Point _default, float _widthUnits)
  {
    const std::optional<WindowManager::Point> moved = _windows != nullptr ? _windows->PositionOf(_kind) : std::nullopt;
    return KeepOnScreen(moved.value_or(_default), _widthUnits, screenWidthUnits, screenHeightUnits);
  };
  // Production and research stand at first in the slots under the Ore, each in the one it took as it opened (task 15.4):
  // the first at the margin, the second beside where the other window stands in the first. Without a manager, production
  // takes the first and research the second.
  const auto slotted = [&](WindowKind _kind, std::size_t _slot)
  {
    const std::size_t slot = _windows != nullptr ? _windows->SlotOf(_kind).value_or(_slot) : _slot;
    const float other = _kind == WindowKind::Production ? RESEARCH_WINDOW_WIDTH : PRODUCTION_WINDOW_WIDTH;
    return WindowManager::Point{.xUnits = slot == 0 ? MARGIN : (2.0f * MARGIN) + other, .yUnits = WINDOWS_TOP};
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
    // Production and research, at first side by side under the Ore and the status panel, clear of the designer.
    else if (kind == WindowKind::Production && _content.production.has_value())
      LayProduction(layout, _metrics, *_content.production, place(kind, slotted(kind, 0), PRODUCTION_WINDOW_WIDTH), scale);
    else if (kind == WindowKind::Research && _content.laboratory.has_value())
      LayResearch(layout, _metrics, *_content.laboratory, place(kind, slotted(kind, 1), RESEARCH_WINDOW_WIDTH), scale);
    // The Controls window, at first in the middle of the screen (task 16.4).
    else if (kind == WindowKind::Controls && _content.controls)
    {
      const WindowManager::Point middle{.xUnits = (screenWidthUnits - CONTROLS_WINDOW_WIDTH) / 2.0f,
                                        .yUnits = std::max(MARGIN, (screenHeightUnits - ControlsHeight(KeyBindings())) / 2.0f)};
      LayControls(layout, _metrics, place(kind, middle, CONTROLS_WINDOW_WIDTH), scale);
    }
    // The orders window, at first at the right under the Ore's line, and the away window in the middle (Phase 5 design §11).
    else if (kind == WindowKind::Orders && _content.orders.has_value())
    {
      LayOrders(layout, _metrics, *_content.orders,
                place(kind, {.xUnits = screenWidthUnits - MARGIN - ORDERS_WINDOW_WIDTH, .yUnits = WINDOWS_TOP}, ORDERS_WINDOW_WIDTH),
                scale);
    }
    else if (kind == WindowKind::Away && _content.away.has_value())
    {
      const float heightUnits = AwayHeight(std::min(_content.away->lines.size(), AWAY_LINES));
      const WindowManager::Point middle{.xUnits = (screenWidthUnits - AWAY_WINDOW_WIDTH) / 2.0f,
                                        .yUnits = std::max(MARGIN, (screenHeightUnits - heightUnits) / 2.0f)};
      LayAway(layout, _metrics, *_content.away, place(kind, middle, AWAY_WINDOW_WIDTH), scale);
    }
  }
  return layout;
}
