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
// The minimap: the map's square, and its marks in the color of whose they are, an ore asteroid in Ore's gold, darkened, so
// that the map reads without a legend: each player's is its set's, as the world draws it (Hud::SideColor), violet is the
// pirates' (ADR-073), and gold is ore (ADR-043). An ore asteroid's mark is an outlined square, so that it differs from a
// player's filled one in shape as well as in brightness (ADR-043 decision 8). The camera's view is a light outline.
constexpr DirectX::XMFLOAT4 MAP_COLOR{0.02f, 0.04f, 0.07f, 0.95f};
constexpr DirectX::XMFLOAT4 PIRATE_COLOR{0.78f, 0.45f, 1.0f, 1.0f};
// A derelict, light gray, a filled square like a structure's, whoever's it was (ADR-074).
constexpr DirectX::XMFLOAT4 DERELICT_COLOR{0.62f, 0.62f, 0.66f, 1.0f};
constexpr DirectX::XMFLOAT4 NEUTRAL_COLOR{0.45f, 0.42f, 0.4f, 1.0f};
constexpr DirectX::XMFLOAT4 ORE_ASTEROID_COLOR{0.42f, 0.23f, 0.02f, 1.0f};
// An ore asteroid that has run out: darker than one with ore, and warmed toward rust, at 3:1 against the map
// (ADR-043 decision 8).
constexpr DirectX::XMFLOAT4 DRY_COLOR{0.32f, 0.2f, 0.147f, 1.0f};
// An asteroid field is only in the way, so it is drawn darker than an ore asteroid, which is worth going to, though a
// field's square is the larger; dark enough that the fields stand back from the ore (ADR-043 decision 7), and at 2:1
// against the map, so that it is seen (ADR-043 decision 8).
constexpr DirectX::XMFLOAT4 ASTEROID_FIELD_COLOR{0.13f, 0.13f, 0.14f, 1.0f};
constexpr DirectX::XMFLOAT4 VIEW_COLOR{0.85f, 0.9f, 1.0f, 0.8f};
// A sector on the minimap (ADR-056): a faint wash of its holder's color under the marks, an outline of it over the fog,
// and stripes of it while suppressed.
constexpr float SECTOR_WASH_ALPHA = 0.12f;
constexpr float SECTOR_OUTLINE_ALPHA = 0.55f;
constexpr float SECTOR_HATCH_ALPHA = 0.35f;
constexpr float SECTOR_LINE_UNITS = 1.0f;
// The territory over the fog: every sector's border in LATTICE_COLOR, at 2:1 against the map,
// under the held sectors' outlines; a cut-off sector's outline in dashes of CUT_OFF_DASH_UNITS with CUT_OFF_GAP_UNITS between
// them, where a suppressed one keeps its hatching; and, while the player places a Relay, each sector whose node it could
// claim now outlined CLAIMABLE_INSET_UNITS inside its border in the player's color. A node has no mark of its own: the
// map's nodes are at their sectors' centers (Tools/MakeMap.py), so a mark would say only what the sector's border and wash
// say (ADR-081 decision 7).
constexpr DirectX::XMFLOAT4 LATTICE_COLOR{0.11f, 0.13f, 0.17f, 1.0f};
constexpr float CUT_OFF_DASH_UNITS = 6.0f;
constexpr float CUT_OFF_GAP_UNITS = 4.0f;
constexpr float CLAIMABLE_INSET_UNITS = 3.0f;
// A structure or derelict the player only remembers is a cross MEMORY_MARK_UNITS across, its arms MEMORY_ARM_UNITS thick,
// drawn over the fog in its side's color (ADR-081 decision 4).
constexpr float MEMORY_MARK_UNITS = 8.0f;
constexpr float MEMORY_ARM_UNITS = 2.0f;

// Everything below in reference units.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
// A line of text in the title face, and in the name face, as the HUD's panels set them; a figure's line (ADR-062).
constexpr float TITLE_LINE_UNITS = 30.0f;
constexpr float NAME_LINE_UNITS = 22.0f;
constexpr float FIGURE_LINE_UNITS = 17.0f;

// The Ore panel, anchored to the top-left corner: the stockpile as the windows write it, Ore's gem and the figure in the
// title face from the panel's left, so that the gem stays put as the figure changes, and the income at its right
// (ADR-043 decision 3). It is as wide as the column under it.
constexpr float ORE_PANEL_HEIGHT = 44.0f;

// The selection panel, in the middle of the bottom edge, as wide as its longest line within these (ADR-043): at its widest,
// the room a 16:9 screen at 150% leaves it between the minimap and the command card (ADR-088 decision 2).
constexpr float SELECTION_PANEL_MIN_WIDTH = 280.0f;
constexpr float SELECTION_PANEL_WIDTH = 552.0f;
// Lines of designs before the rest are counted together.
constexpr size_t SELECTION_DESIGN_LINES = 5;
// The selection's hit points as a bar under its lines: green above half, then amber, then red, as a bar over a damaged
// ship is (ADR-088 decisions 4 and 5).
constexpr float HEALTH_BAR_UNITS = 6.0f;
constexpr float HEALTH_BAR_GAP_UNITS = 8.0f;
constexpr float HEALTH_HURT_SHARE = 0.5f;
constexpr float HEALTH_LOW_SHARE = 0.25f;
// A ship's own bar, under a selection of up to Hud::SHIP_BARS_MOST ships, in rows of twelve that fit the narrowest
// selection panel (ADR-088 decision 4).
constexpr float SHIP_BAR_WIDTH_UNITS = 17.0f;
constexpr float SHIP_BAR_HEIGHT_UNITS = 10.0f;
constexpr float SHIP_BAR_GAP_UNITS = 4.0f;
constexpr size_t SHIP_BARS_A_ROW = 12;
static_assert((SHIP_BARS_A_ROW * SHIP_BAR_WIDTH_UNITS) + ((SHIP_BARS_A_ROW - 1) * SHIP_BAR_GAP_UNITS) <=
              SELECTION_PANEL_MIN_WIDTH - (2.0f * PADDING));

// The bars over an entity, on the screen, at any zoom and however the camera has turned (ADR-088 decision 5):
// each this size in reference units at the back buffer's scale, not the player's factor, which sizes the HUD and not the
// world's marks (ADR-070); the health bar over the build bar, this far apart, and the pair this far above the top of the
// entity's model on the screen.
constexpr float ENTITY_BAR_WIDTH_UNITS = 32.0f;
constexpr float ENTITY_BAR_HEIGHT_UNITS = 5.0f;
constexpr float ENTITY_BAR_GAP_UNITS = 2.0f;
constexpr float ENTITY_BAR_LIFT_UNITS = 4.0f;

// The command card, anchored to the bottom-right corner, each of its buttons in a cell of its own (ADR-088 decision 2); a
// button's label and cost stand this far in. Build takes the left cell of its last row, and a structure's Upgrade the
// whole of that row.
constexpr float COMMAND_CARD_WIDTH = 380.0f;
constexpr std::uint8_t BUILD_CELL = 4;
constexpr std::uint8_t UPGRADE_CELL = 4;
constexpr float BUTTON_HEIGHT = 34.0f;
constexpr float BUTTON_GAP = 6.0f;
constexpr float BUTTON_INSET = 12.0f;
// The bar along a button's foot that shows the work it started, and its inset from the button's edges.
constexpr float BUTTON_BAR_UNITS = 3.0f;
constexpr float BUTTON_BAR_INSET = 2.0f;

// The minimap, a square anchored to the bottom-left corner, with the map drawn inside its padding: about 36 m a unit on the
// 10 km map (ADR-043 decision 8).
constexpr float MINIMAP_SIZE = 300.0f;
constexpr float MINIMAP_PADDING = 8.0f;
// The smallest a mark is drawn, and how wide the view's outline is, so that both stay visible. An ore asteroid's is the
// largest, since it is what the player looks for on the map; a rig's mark is drawn over its asteroid's (ADR-043 decision 7).
constexpr float SHIP_MARK_UNITS = 3.0f;
constexpr float STRUCTURE_MARK_UNITS = 6.0f;
constexpr float ORE_MARK_UNITS = 8.0f;
constexpr float VIEW_LINE_UNITS = 1.5f;
// The line an ore asteroid's mark is outlined with (ADR-043 decision 8).
constexpr float ORE_OUTLINE_UNITS = 2.0f;

// The hint, anchored to the top edge's middle.
constexpr float HINT_PANEL_WIDTH = 640.0f;

// The column under the Ore (ADR-066 decision 3): the Ore, status, territory and alerts panels are one width, so
// that the column's edge stands still. It is the widest of the alerts' COLUMN_LEAST_UNITS, the longest research topic's
// name set on its line with room for "+99" queued at its end, and the widest of COLUMN_WIDEST_LINES, the status panel's
// lines at their longest, set in the name face.
constexpr float COLUMN_LEAST_UNITS = 380.0f;
constexpr std::array<std::string_view, 3> COLUMN_WIDEST_LINES{"Shipyards \xC2\xB7 BUILDING 99 \xC2\xB7 WAITING 99 \xC2\xB7 IDLE 99",
                                                              "Shipyards at the fleet cap \xC2\xB7 Station upgrading, 100%",
                                                              "Research: every open topic done"};
constexpr std::string_view COLUMN_WIDEST_QUEUED = "+99";
// The status panel under the Ore (ADR-066): the room above and below its lines, which the territory's and the alerts'
// panels keep too. Its place is kept for its three lines, the first with a bar under it, whether or not it has them, so that
// what stands under it does not move as a Lab or a Shipyard is finished.
constexpr float STATUS_LINES_KEPT = 3.0f;
constexpr float PANEL_GAP = 8.0f;
constexpr float TERRITORY_INSET_UNITS = 10.0f;
// A status line's bar (ADR-066 decision 3): under its words, STATUS_UNDER_BAR_UNITS thick in a row of STATUS_BAR_ROW_UNITS;
// beside them, STATUS_BESIDE_BAR_UNITS thick and STATUS_BAR_GAP_UNITS clear of the words and the figure.
constexpr float STATUS_BAR_ROW_UNITS = 8.0f;
constexpr float STATUS_UNDER_BAR_UNITS = 4.0f;
constexpr float STATUS_BESIDE_BAR_UNITS = 6.0f;
constexpr float STATUS_BAR_GAP_UNITS = 8.0f;
constexpr float STATUS_PANEL_KEPT_UNITS = (2.0f * TERRITORY_INSET_UNITS) + (STATUS_LINES_KEPT * NAME_LINE_UNITS) + STATUS_BAR_ROW_UNITS;
// How large an alert's mark is on the minimap (ADR-059).
constexpr float ALERT_MARK_UNITS = 14.0f;

// The match's end, anchored to the top edge's middle under the hint: the outcome, its length, and the way back.
constexpr float BANNER_WIDTH = 520.0f;
// Where the match runs, anchored to the top-right corner (ADR-086): as wide as its words, a server's address cut short
// beyond CONNECTION_MOST_UNITS, and as wide as CONNECTION_WIDEST_SILENCE while it warns, so that its edge stands still as
// the seconds count.
constexpr float CONNECTION_MOST_UNITS = 360.0f;
constexpr std::string_view CONNECTION_WIDEST_SILENCE = "No word from the server \xC2\xB7 999 s";
// The round trip after a server's address at its widest, which the tag is as wide as whatever it reads (ADR-086 decision 1).
constexpr std::string_view CONNECTION_WIDEST_ROUND_TRIP = " \xC2\xB7 999 ms";
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

// Whether _producer's front job waits for its player's fleet cap rather than for Ore (ADR-071 decision 3): a warship not yet
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
// _waitingFor what a job not yet started waits for: Ore, or the fleet cap (ADR-071 decision 3).
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

// Whether a topic is open to the player's Research Lab now: not researched, of a tier its Lab has opened, and with every
// prerequisite researched (ADR-066 decision 1).
bool IsOpen(const Outpost::Snapshot& _newest, const Outpost::ResearchTopicView& _topic)
{
  const auto researched = [&_newest](Outpost::ResearchTopicId _id)
  {
    const auto found = std::ranges::find(_newest.research, _id, &Outpost::ResearchTopicView::id);
    return found != _newest.research.end() && found->researched;
  };
  return !_topic.researched && _topic.tier <= _newest.researchTier && std::ranges::all_of(_topic.prerequisites, researched);
}

// The status panel's lines (ADR-066): what the player's finished Research Lab is doing, what its finished Shipyards are
// doing or what holds them back, and the fleet against its cap.
// - The Lab's line names the topic it researches over a bar of how far it has come, with how many more are queued at the
//   bar's end, "+3", or says it waits for Ore. Idle, it says IDLE in a chip (ADR-085 decision 1) while a topic
//   is open to it, and that every open topic is done otherwise. A click opens research.
// - The Shipyards' line says first whether the fleet cap holds them back: a Shipyard waits for it, or the fleet has room for
//   none of the player's saved designs. Then it reads "Shipyards at the fleet cap · Station L4: +10", with what the
//   station's next level adds to the cap, or how far its upgrade has come, plainly, since its remedy is a level and not a
//   queue. A click selects the Command Station, whose panel offers the upgrade.
// - Otherwise it counts them, a label and a figure each, "Shipyards · BUILDING 2 · WAITING 1 · IDLE 3". The idle count is a
//   chip only when an idle Shipyard could start a ship now: a saved design of a hull it builds fits under the cap, and the
//   player has its Ore. A click opens production at the first idle Shipyard by number, or at the first.
// - Under it, with a cap, the fleet as a bar against it, "29 / 30", whose click does what the Shipyards' does.
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
    {
      if (std::ranges::any_of(_newest.research, [&_newest](const Outpost::ResearchTopicView& _topic) { return IsOpen(_newest, _topic); }))
        lines.push_back({.runs = {{.text = "Research Lab "}, {.text = "IDLE", .chip = true}}, .action = open});
      else
        lines.push_back({.runs = {{.text = "Research: every open topic done"}}, .action = open});
    }
    else
    {
      Hud::StatusLine line{
        .runs = {{.text = TopicNameOf(_newest, lab->research.front())}},
        .action = open,
        .bar = Hud::StatusBar{.share = static_cast<float>(lab->jobPermille) / static_cast<float>(Outpost::PERMILLE), .under = true}};
      if (lab->jobPermille == 0)
        line.runs.push_back({.text = std::format("{}waiting for Ore", DOT)});
      if (lab->research.size() > 1)
        line.bar->figure = std::format("+{}", lab->research.size() - 1);
      lines.push_back(std::move(line));
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

  // A saved design's hull, and whether it fits under the fleet cap now.
  const auto hullOf = [&_newest](const Outpost::DesignView& _design)
  {
    const auto hull = std::ranges::find(_newest.hulls, _design.hull, &Outpost::HullView::id);
    return hull != _newest.hulls.end() ? &*hull : nullptr;
  };
  const auto fits = [&_newest, &hullOf](const Outpost::DesignView& _design)
  {
    const Outpost::HullView* hull = hullOf(_design);
    return hull != nullptr && (_newest.fleetCap <= 0 || _newest.commandPoints + hull->commandPoints <= _newest.fleetCap);
  };
  // Whether _shipyard, idle, could start a ship now: a saved design of a hull it builds that fits, with its Ore.
  const auto couldStart = [&_newest, &hullOf, &fits](const Outpost::EntityView& _shipyard)
  {
    return std::ranges::any_of(_newest.designs,
                               [&](const Outpost::DesignView& _design)
                               {
                                 const Outpost::HullView* hull = hullOf(_design);
                                 return hull != nullptr && hull->shipyardLevel <= _shipyard.level && fits(_design) &&
                                        _newest.ore >= _design.cost;
                               });
  };

  std::size_t building = 0;
  std::size_t waiting = 0;
  std::size_t capped = 0;
  const Outpost::EntityView* firstIdle = nullptr;
  std::size_t idle = 0;
  bool idleCouldStart = false;
  for (const Outpost::EntityView* shipyard : shipyards)
  {
    if (shipyard->queue.empty())
    {
      firstIdle = firstIdle != nullptr ? firstIdle : shipyard;
      ++idle;
      idleCouldStart = idleCouldStart || couldStart(*shipyard);
    }
    else if (shipyard->jobPermille > 0)
      ++building;
    else if (WaitsOnFleetCap(_newest, *shipyard))
      ++capped;
    else
      ++waiting;
  }

  // At the cap: the station's next level is the remedy, and a click goes to the station.
  const bool atCap = _newest.fleetCap > 0 && (capped > 0 || (!_newest.designs.empty() && std::ranges::none_of(_newest.designs, fits)));
  // The fleet against its cap, under the Shipyards' line, its click theirs.
  const auto fleet = [&_newest](const Hud::Action& _action)
  {
    const float share = static_cast<float>(_newest.commandPoints) / static_cast<float>(_newest.fleetCap);
    return Hud::StatusLine{.runs = {{.text = "Fleet"}},
                           .action = _action,
                           .bar = Hud::StatusBar{.share = std::clamp(share, 0.0f, 1.0f),
                                                 .figure = std::format("{} / {}", _newest.commandPoints, _newest.fleetCap)}};
  };
  if (atCap)
  {
    std::string text = "Shipyards at the fleet cap";
    Hud::Action action{.kind = Hud::ActionKind::OpenProduction, .producer = shipyards.front()->id};
    const auto station = std::ranges::find_if(_entities, [&own](const Outpost::EntityView& _entity)
                                              { return own(_entity, Outpost::StructureKind::CommandStation); });
    if (station != _entities.end())
    {
      action = {.kind = Hud::ActionKind::Select, .entity = station->id};
      const auto type =
        std::ranges::find(_newest.structureTypes, Outpost::StructureKind::CommandStation, &Outpost::StructureTypeView::structure);
      const auto next = static_cast<std::size_t>(std::max(0, station->level - 1));
      if (station->upgradePermille.has_value())
        text += std::format("{}Station upgrading, {}%", DOT, *station->upgradePermille / 10);
      else if (type != _newest.structureTypes.end() && next < type->levels.size() && type->levels[next].commandPoints > _newest.fleetCap)
        text += std::format("{}Station L{}: +{}", DOT, station->level + 1, type->levels[next].commandPoints - _newest.fleetCap);
    }
    lines.push_back({.runs = {{.text = std::move(text)}}, .action = action});
    lines.push_back(fleet(action));
    return lines;
  }

  std::vector<Hud::StatusRun> runs{{.text = std::format("Shipyards{}BUILDING {}{}WAITING {}{}", DOT, building, DOT, waiting, DOT)}};
  const std::string idleText = std::format("IDLE {}", idle);
  if (idle > 0 && idleCouldStart)
    runs.push_back({.text = idleText, .chip = true});
  else
    runs.back().text += idleText;
  const Hud::Action open{.kind = Hud::ActionKind::OpenProduction, .producer = (firstIdle != nullptr ? firstIdle : shipyards.front())->id};
  lines.push_back({.runs = std::move(runs), .action = open});
  if (_newest.fleetCap > 0)
    lines.push_back(fleet(open));
  return lines;
}

// A damage card's rating: Good from two thirds of the best any design does to the hull, Fair from a third.
constexpr float GOOD_SHARE = 2.0f / 3.0f;
constexpr float FAIR_SHARE = 1.0f / 3.0f;

// A design's code, its components' initials joined by middle dots, "S·I·MD", with its module's after them when it has
// one, "S·I·MD·SA" (ADR-058 decision 6). The designer's chips and the production window both show it.
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
// the tick the server says it was last seen in (ADR-081 decision 4); "last seen earlier" when that is not known.
std::string LastSeen(const Outpost::Snapshot& _newest, const Outpost::EntityView& _entity, std::uint32_t _ticksPerSecond)
{
  if (_ticksPerSecond == 0 || _entity.lastSeenTick == 0 || _entity.lastSeenTick > _newest.tick)
    return "last seen earlier";
  return std::format("last seen {} ago", Outpost::MinutesAndSeconds((_newest.tick - _entity.lastSeenTick) / _ticksPerSecond));
}

// The retreats a row offers, left to right, and what each cell says (ADR-088 decision 3).
constexpr std::array<Outpost::RetreatThreshold, 3> RETREAT_CHOICES{Outpost::RetreatThreshold::Quarter, Outpost::RetreatThreshold::Half,
                                                                   Outpost::RetreatThreshold::Never};
std::string_view RetreatCell(Outpost::RetreatThreshold _retreat) noexcept
{
  switch (_retreat)
  {
  case Outpost::RetreatThreshold::Quarter:
    return "25%";
  case Outpost::RetreatThreshold::Half:
    return "50%";
  case Outpost::RetreatThreshold::Never:
    break;
  }
  return "Never";
}

// The best each of a design's numbers reaches over every design the components make, locked ones included, so that the
// designer's bars keep their scale as research unlocks parts (ADR-017 decision 7).
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
  // A module adds its cost and its sight to any design (ADR-058 decision 4).
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

// A design's damage against a hull rated by its share of the best any design does to it (ADR-017 decision 7).
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
// the change is the difference of the two figures shown.
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

// The name of the topic that unlocks _component, or none when no topic does.
template <typename IdType>
std::string UnlockedBy(const Outpost::Snapshot& _newest, IdType _component, IdType Outpost::ResearchTopicView::*_unlocks)
{
  const auto topic = std::ranges::find(_newest.research, _component, _unlocks);
  return topic != _newest.research.end() ? topic->nameUtf8 : std::string();
}

// The designer's window (ADR-017 decision 7): its target Shipyard, the name, the saved designs, a
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
  panel.retreat = _designer.Retreat(_newest);

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
  // A locked part previews what its topic buys, and the help line names the topic (ADR-017 decision 7).
  bool previewingLocked = false;
  std::string previewTopic;
  Hud::SlotRow& hulls = panel.slots[0];
  hulls.label = "HULL";
  hulls.cards.reserve(_newest.hulls.size());
  for (const Outpost::HullView& hull : _newest.hulls)
  {
    if (hull.id == picked.hull)
      hulls.picked = hull.nameUtf8;
    if (hull.id == preview.hull && preview.hull != picked.hull)
    {
      previewing = hull.nameUtf8;
      previewingLocked = !hull.available;
      previewTopic = UnlockedBy(_newest, hull.id, &Outpost::ResearchTopicView::unlocksHull);
    }
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
    {
      previewing = drive.nameUtf8;
      previewingLocked = !drive.available;
      previewTopic = UnlockedBy(_newest, drive.id, &Outpost::ResearchTopicView::unlocksDrive);
    }
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
    {
      previewing = weapon.nameUtf8;
      previewingLocked = !weapon.available;
      previewTopic = UnlockedBy(_newest, weapon.id, &Outpost::ResearchTopicView::unlocksWeapon);
    }
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
  // The module row: no module first, which every design starts with, then each module (ADR-058 decision 7).
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
    {
      previewing = module.nameUtf8;
      previewingLocked = !module.available;
    }
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
  if (!previewStats.has_value())
    panel.hint = "Hover any part to preview its effect.";
  else if (!previewingLocked)
    panel.hint = std::format("Preview: with {} instead", previewing);
  else
    panel.hint = std::format("Preview: with {}, once {} is researched", previewing, previewTopic.empty() ? "it" : previewTopic);

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
    // How far a module lets the ship see; none without one, when its weapon sets its sight (ADR-058 decision 5).
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
      // The change per ship to a tenth, as the figure is written: "+4.5".
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
  // A Shipyard builds the hulls of its level (ADR-064 decision 9): above it, Queue is dim and says the level the hull needs.
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

// A floating window's look (ADR-031 decision 3), after the owner's mockup: a dark navy body, a hatched title bar
// with the title in the title face, a close box at its right end, and a bracket at each corner.
// Opaque, so that nothing bright behind a window shows through its near-black (ADR-068 decision 1).
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

// The designer's window (ADR-017 decision 7), laid out as the owner's mockup, Docs/Mockups/ShipDesigner.png, which is
// drawn at the reference scale: every place below is in reference units from the window's top-left corner.
// Three part cards from CARDS_LEFT, a little wider than the mockup's, so 14 units of room right of them (ADR-062 decision 4).
constexpr float DESIGNER_WIDTH = 818.0f;
constexpr float DESIGNER_INSET = 24.0f;
// The hatched header runs on below the title bar, which the window is dragged by, to hold the title and the Shipyard.
constexpr float DESIGNER_HEADER = 72.0f;
constexpr float NAME_ROW_TOP = 84.0f;
constexpr float NAME_ROW_HEIGHT = 38.0f;
constexpr float CHIP_ROW_TOP = 132.0f;
constexpr float CHIP_HEIGHT = 26.0f;
constexpr float CHIPS_LEFT = 70.0f;
// A little narrower than a part card, so that the arrows that scroll the chips stand clear of the third (ADR-068 decision 1).
constexpr float CHIP_WIDTH = 216.0f;
constexpr float SLOTS_TOP = 168.0f;
constexpr float SLOT_GAP = 10.0f;
constexpr float SLOT_DIVIDER_LEFT = 108.0f;
constexpr float CARDS_LEFT = 118.0f;
// Wide enough, inside the designer's 818 units, for the Large hull's numbers in Cascadia Mono at 1080p (ADR-062 decision 4).
constexpr float CARD_WIDTH = 224.0f;
// A part card's text stands this far in from either side of the card.
constexpr float CARD_TEXT_INSET = 10.0f;
constexpr float CARD_HEIGHT = 80.0f;
constexpr float CARD_GAP = 7.0f;
constexpr std::size_t CARDS_PER_LINE = 3;
// A part card's lines under its name: its numbers, any note, such as a weapon's splash, and at its foot the research that
// unlocks it. A row whose cards have notes is taller, so that a note and that line never meet (ADR-061).
constexpr float CARD_NUMBERS_TOP = 34.0f;
constexpr float CARD_NOTE_TOP = 52.0f;
constexpr float CARD_NOTE_ROOM = 16.0f;
constexpr float CARD_LOCK_FROM_FOOT = 20.0f;
// The research's words at the card's foot, after its lock.
constexpr float CARD_LOCK_TEXT_LEFT = CARD_TEXT_INSET + 16.0f;
constexpr float SECTION_GAP = 20.0f;
constexpr float SECTION_LABEL_HEIGHT = 26.0f;
constexpr float BAR_ROW_HEIGHT = 25.0f;
// Hit points, armor, speed, range, cost and build time.
constexpr std::size_t BAR_ROWS_OF_EVERY_DESIGN = 6;
constexpr float BAR_LEFT = 104.0f;
// A previewed figure and its change, "1,680 (+1,482)", end at BAR_VALUE_RIGHT clear of the bar.
constexpr float BAR_WIDTH = 90.0f;
constexpr float BAR_VALUE_RIGHT = 316.0f;
constexpr float DAMAGE_LEFT = 364.0f;
constexpr float DAMAGE_CARD_HEIGHT = 96.0f;
constexpr float DAMAGE_CARD_GAP = 8.0f;
constexpr float FOOTER_HEIGHT = 48.0f;
// An arrow's button, and the room between it and what it steps (ADR-068 decision 1).
constexpr float SMALL_BUTTON_UNITS = 24.0f;
constexpr float ARROW_GAP_UNITS = 8.0f;
constexpr float LINE_UNITS = 1.0f;
// Labels in small spaced capitals.
constexpr float LABEL_TRACKING_UNITS = 2.0f;
// Ore's gem beside a figure: this share of the figure's size, and this far from it.
constexpr float ORE_MARK_SHARE = 0.6f;
constexpr float ORE_MARK_GAP_UNITS = 5.0f;
// The room a line cut short to fit keeps from what stands beside it, such as a cost (ADR-061).
constexpr float FIT_GAP_UNITS = 6.0f;
// A warning chip (ADR-085 decision 1): its words in the windows' navy on a tag of the warning's color, which reaches this far
// beyond them on either side and covers their whole line, descenders and all.
constexpr float CHIP_PAD_UNITS = 4.0f;
// A row of retreats (ADR-088 decision 3): the room between its cells, a line's height as a share of its face's
// size, which centers the words in a cell, and in the HUD the room its RETREAT label takes over it.
constexpr float RETREAT_CELL_GAP_UNITS = 4.0f;
constexpr float RETREAT_LINE_SHARE = 1.25f;
constexpr float RETREAT_LABEL_UNITS = 20.0f;
// The box a window's header holds the player's Ore in.
constexpr float ORE_BOX_WIDTH = 116.0f;
// How far down a small button its mark's line starts.
constexpr float SMALL_BUTTON_TEXT_TOP = 4.0f;
// A key's cap on a button (ADR-068 decision 4), and how far down it the letter's line starts.
constexpr float KEY_CAP_UNITS = 20.0f;
constexpr float KEY_CAP_TEXT_TOP = 2.0f;

// The header's queue: its slots, ending a little short of the Ore's box, and its label over them.
constexpr float DESIGNER_SLOT_UNITS = 30.0f;
constexpr float DESIGNER_SLOT_HEIGHT = 24.0f;
constexpr float DESIGNER_SLOT_SPACING = 3.0f;
constexpr float DESIGNER_SLOTS_LEFT = DESIGNER_WIDTH - DESIGNER_INSET - ORE_BOX_WIDTH - 14.0f -
                                      (static_cast<float>(Outpost::QUEUE_LIMIT) * (DESIGNER_SLOT_UNITS + DESIGNER_SLOT_SPACING)) +
                                      DESIGNER_SLOT_SPACING;
constexpr float DESIGNER_QUEUE_TOP = 12.0f;
// The ships built, ending this far short of the slots, on the title's line.
constexpr float DESIGNER_BUILT_GAP = 12.0f;
constexpr float DESIGNER_BUILT_TOP = 44.0f;
// The target Shipyard's room between its arrows, short of the queue by what "· DESIGNER" takes.
constexpr float DESIGNER_SHIPYARD_ROOM = DESIGNER_SLOTS_LEFT - DESIGNER_INSET - 120.0f;
// The name field: its label, the name after it, a unit under the field's middle, and its count at its right; and Save.
constexpr float NAME_LABEL_LEFT = 12.0f;
constexpr float NAME_LABEL_TOP = 13.0f;
constexpr float NAME_TEXT_LEFT = 66.0f;
constexpr float NAME_TEXT_TOP = 9.0f;
constexpr float NAME_COUNT_TOP = 12.0f;
constexpr float SAVE_WIDTH = 68.0f;
constexpr float SAVE_GAP = 8.0f;
constexpr float SAVE_TEXT_LEFT = 12.0f;
// A saved design's chip: its name and its code, inset from its sides, the code a unit lower; the label before them, and
// the arrows that scroll them at the row's right.
constexpr float CHIP_GAP = 6.0f;
constexpr float CHIP_INSET = 8.0f;
constexpr float CHIP_TEXT_TOP = 6.0f;
constexpr float CHIP_LABEL_TOP = 7.0f;
constexpr float CHIP_ARROW_GAP = 4.0f;
// Where the third chip ends, at its width from its place; the chips start a gap after the label where it needs more room
// than their place gives it, and share what is left to that end.
constexpr float CHIPS_END = CHIPS_LEFT - DESIGNER_INSET + (static_cast<float>(CHIPS_SHOWN) * (CHIP_WIDTH + CHIP_GAP)) - CHIP_GAP;
// A slot's label and pick down its row, and its divider's room above and below.
constexpr float SLOT_LABEL_TOP = 21.0f;
constexpr float SLOT_PICK_TOP = 39.0f;
constexpr float SLOT_DIVIDER_INSET = 6.0f;
// The three part cards and their gaps, which run 10 units into the window's inset, 14 from its edge (ADR-062 decision 4).
constexpr float CARDS_WIDTH = (static_cast<float>(CARDS_PER_LINE) * CARD_WIDTH) + (static_cast<float>(CARDS_PER_LINE - 1) * CARD_GAP);
// A part card's name, its cost a little lower, and its lock's box down the lock's line.
constexpr float CARD_NAME_TOP = 8.0f;
constexpr float CARD_COST_TOP = 11.0f;
constexpr float CARD_LOCK_BOX_UNITS = 10.0f;
constexpr float CARD_LOCK_BOX_TOP = 2.0f;
// A bar's row: its label a little down, its track, and its value's unit after the value.
constexpr float BAR_LABEL_TOP = 2.0f;
constexpr float BAR_TOP = 6.0f;
constexpr float BAR_HEIGHT = 7.0f;
constexpr float BAR_UNIT_GAP = 6.0f;
// A damage card: its hull and armor, its figure for a ship, how many a splash reaches and the change above the figure's
// line, its rating's bar, and its figure per 100 Ore, each from the card's top-left; and the hint under the cards.
constexpr float DAMAGE_WIDTH = DESIGNER_WIDTH - DESIGNER_INSET - DAMAGE_LEFT;
constexpr float DAMAGE_INSET = 10.0f;
constexpr float DAMAGE_RIGHT_INSET = 8.0f;
constexpr float DAMAGE_HULL_TOP = 8.0f;
constexpr float DAMAGE_ARMOR_TOP = 11.0f;
constexpr float DAMAGE_FIGURE_TOP = 30.0f;
constexpr float DAMAGE_REACH_TOP = 28.0f;
constexpr float DAMAGE_CHANGE_TOP = 44.0f;
constexpr float DAMAGE_BAR_TOP = 66.0f;
constexpr float DAMAGE_BAR_HEIGHT = 4.0f;
constexpr float DAMAGE_PER_ORE_TOP = 76.0f;
constexpr float DAMAGE_HINT_GAP = 10.0f;
// The footer: Rename, the retreat's label over its row of three, the stepper and its count, and Queue, which takes the
// rest; each one's text from its top-left.
constexpr float RENAME_WIDTH = 90.0f;
constexpr float RENAME_TEXT_LEFT = 14.0f;
constexpr float RENAME_TEXT_TOP = 16.0f;
constexpr float RETREAT_LEFT = DESIGNER_INSET + 102.0f;
constexpr float RETREAT_WIDTH = 206.0f;
constexpr float RETREAT_CELLS_TOP = 18.0f;
constexpr float STEPPER_LEFT = 346.0f;
constexpr float STEP_WIDTH = 36.0f;
constexpr float COUNT_WIDTH = 42.0f;
constexpr float STEP_TEXT_LEFT = 13.0f;
constexpr float COUNT_TEXT_LEFT = 10.0f;
constexpr float FOOTER_TEXT_TOP = 13.0f;
constexpr float QUEUE_LEFT = 468.0f;
constexpr float QUEUE_TEXT_LEFT = 18.0f;
constexpr float QUEUE_RIGHT_INSET = 16.0f;
// Queue's label, up to 27 units in the title face, ends above the detail's line at either scale's rounding (ADR-061).
constexpr float QUEUE_LABEL_TOP = 2.0f;
constexpr float QUEUE_DETAIL_TOP = 31.0f;
constexpr float QUEUE_COST_TOP = 10.0f;

// The mockup's colors (owner, 2026-10-03), sampled from it and made linear, as the render target
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
// A button under the pointer has its edge lit in the windows' edge color, short of a pick's; a status line under it lies on
// the cards' color (ADR-088 decision 1).
constexpr DirectX::XMFLOAT4 HOVER_EDGE_COLOR = WINDOW_EDGE_COLOR;
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

// The build bar over an entity, in a light gray that is no side's (ADR-085 decision 2).
constexpr DirectX::XMFLOAT4 BUILD_BAR_COLOR{0.5f, 0.52f, 0.55f, 1.0f};

// A health bar's fill, over an entity and in the selection panel alike: green above half, then amber above a quarter, then
// red. Brighter than the figures' good, fair and poor, which are tuned to a window's
// navy (ADR-062): on a side's back at 0.3 this red stands at 2.36:1 and this amber at 4.47:1 or more, where the figures'
// would stand at 1.67:1 and 3.32:1 (ADR-088 decision 5).
constexpr DirectX::XMFLOAT4 HEALTH_GOOD_COLOR{0.2f, 0.85f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_FAIR_COLOR{1.0f, 0.7f, 0.1f, 1.0f};
constexpr DirectX::XMFLOAT4 HEALTH_POOR_COLOR{0.95f, 0.2f, 0.15f, 1.0f};

const DirectX::XMFLOAT4& HealthColor(float _share) noexcept
{
  return _share > HEALTH_HURT_SHARE ? HEALTH_GOOD_COLOR : _share > HEALTH_LOW_SHARE ? HEALTH_FAIR_COLOR : HEALTH_POOR_COLOR;
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

// The color of _player's, at _alpha: the pirates' violet on _side Pirate, else the player's color in _content, or the neutral
// marks' gray for a player with none.
DirectX::XMFLOAT4 SideColorOf(const Hud::Content& _content, Hud::Side _side, Outpost::PlayerId _player, float _alpha = 1.0f)
{
  DirectX::XMFLOAT4 color = NEUTRAL_COLOR;
  if (_side == Hud::Side::Pirate)
    color = PIRATE_COLOR;
  else if (const auto found = std::ranges::find(_content.sideColors, _player, &Hud::SideColor::player); found != _content.sideColors.end())
    color = found->color;
  color.w = _alpha;
  return color;
}

// A queue's label, as every window writes it: its jobs against the queue's limit, "QUEUE · 2 / 5" (ADR-043 decision 6).
std::string QueueLabel(std::size_t _jobs)
{
  return std::format("QUEUE{}{} / {}", DOT, _jobs, Outpost::QUEUE_LIMIT);
}

// Where the arrows stand that step a subject: in a window's title bar, and on its header's title line.
constexpr float ARROWS_IN_TITLE_TOP = 6.0f;
constexpr float ARROWS_IN_HEADER_TOP = 43.0f;

// How tall a slot's cards are: taller when any has a note, so that every card of the row stays the same.
float CardHeightOf(const Hud::SlotRow& _slot) noexcept
{
  const bool noted = std::ranges::any_of(_slot.cards, [](const Hud::PartCard& _card) { return !_card.note.empty(); });
  return noted ? CARD_HEIGHT + CARD_NOTE_ROOM : CARD_HEIGHT;
}

// The production and research windows, in the designer's look, in reference units from the window's
// top-left corner: a hatched header with the window's subject and the Ore, the cards that add to the queue, and under them
// the queue, a row for each job. The cards stay where they are as the queue grows, so that one can be clicked again and
// again, and the window grows at its foot (ADR-043). Research is wide enough that a topic's name fits its card, and
// production as wide as the reference width holds beside research and the designer, each a margin apart (ADR-062).
constexpr float RESEARCH_WINDOW_WIDTH = 560.0f;
constexpr float PRODUCTION_WINDOW_WIDTH = Hud::REFERENCE_WIDTH_UNITS - (4.0f * MARGIN) - DESIGNER_WIDTH - RESEARCH_WINDOW_WIDTH;
constexpr float WINDOW_INSET = 24.0f;
// Where a window's header, its label in its title bar, stands from the title bar's top.
constexpr float WINDOW_HEADER_TOP = 11.0f;
// How far under a section's label what it labels starts.
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

// How wide a topic card is: the research window's width shared among its columns.
float TopicWidth() noexcept
{
  const auto columns = static_cast<float>(Hud::TOPIC_COLUMNS);
  return (RESEARCH_WINDOW_WIDTH - (2.0f * WINDOW_INSET) - ((columns - 1.0f) * CARD_SPACING)) / columns;
}

// A table on the canvas (ADR-100; the Style Guide's table): its headings' row, as tall as a figure's line with room above and
// below it; a row, its figure's line 5 down, 3 over its detail line, which ends 4 above its foot; the gap between two rows;
// and how wide the mark at a row's left is.
constexpr float TABLE_HEADING_UNITS = 28.0f;
constexpr float TABLE_FIGURE_TOP_UNITS = 5.0f;
constexpr float TABLE_DETAIL_TOP_UNITS = TABLE_FIGURE_TOP_UNITS + FIGURE_LINE_UNITS + 3.0f;
constexpr float TABLE_ROW_UNITS = TABLE_DETAIL_TOP_UNITS + FIGURE_LINE_UNITS + 4.0f;
constexpr float TABLE_ROW_GAP_UNITS = 4.0f;
constexpr float TABLE_MARK_UNITS = 4.0f;

// The windows on the canvas (ADR-100). The game's look as the canvas's theme: the windows' colors, typefaces and sizes
// above, and a window's header in the label face at the body's inset, as each window has drawn its own.
const Neuron::Theme& GameTheme()
{
  static const Neuron::Theme THEME = []
  {
    Neuron::Theme made;
    made.windowColor = WINDOW_COLOR;
    made.titleBarColor = TITLE_HATCH_COLOR;
    made.titleTextColor = HEADER_LABEL_COLOR;
    made.closeBoxColor = CLOSE_BOX_COLOR;
    made.cornerColor = WINDOW_EDGE_COLOR;
    made.cardColor = CARD_COLOR;
    made.pickedColor = PICKED_COLOR;
    made.fieldColor = FIELD_COLOR;
    made.disabledColor = DISABLED_BUTTON_COLOR;
    made.edgeColor = EDGE_COLOR;
    made.hoverEdgeColor = HOVER_EDGE_COLOR;
    made.pickedEdgeColor = PICKED_EDGE_COLOR;
    made.textColor = TEXT_COLOR;
    made.dimTextColor = DIM_TEXT_COLOR;
    made.labelColor = LABEL_COLOR;
    made.keyColor = ROW_LABEL_COLOR;
    made.lockedTextColor = LOCKED_TEXT_COLOR;
    made.figureColor = GOLD_COLOR;
    made.warningColor = WARNING_COLOR;
    made.barTrackColor = BAR_TRACK_COLOR;
    made.barFillColor = BAR_FILL_COLOR;
    made.detailColor = ROW_LABEL_COLOR;
    made.titleFont = Hud::FontOf(Hud::Typeface::Label);
    made.closeMarkFont = Hud::FontOf(Hud::Typeface::Title);
    made.nameFont = Hud::FontOf(Hud::Typeface::Name);
    made.labelFont = Hud::FontOf(Hud::Typeface::Label);
    made.figureFont = Hud::FontOf(Hud::Typeface::Figure);
    made.detailFont = Hud::FontOf(Hud::Typeface::Detail);
    made.cornerSprite = Hud::SpriteOf(Hud::Sprite::Corner);
    made.checkSprite = Hud::SpriteOf(Hud::Sprite::Checkbox);
    made.titleBarUnits = Hud::TITLE_BAR_UNITS;
    made.titleLeftUnits = WINDOW_INSET;
    made.titleTopUnits = WINDOW_HEADER_TOP;
    made.titleTrackingUnits = LABEL_TRACKING_UNITS;
    made.closeMark = std::string(CLOSE_MARK);
    made.closeMarkLeftUnits = CLOSE_TEXT_LEFT;
    made.closeMarkTopUnits = TITLE_TEXT_INSET;
    made.cornerUnits = CORNER_UNITS;
    made.edgeUnits = LINE_UNITS;
    made.buttonInsetUnits = BUTTON_INSET;
    made.fitGapUnits = FIT_GAP_UNITS;
    made.nameLineUnits = NAME_LINE_UNITS;
    made.figureLineUnits = FIGURE_LINE_UNITS;
    const float figureUnits = Hud::FaceUnits(Hud::Typeface::Figure);
    made.figureMarkUnits = std::round(figureUnits * ORE_MARK_SHARE);
    made.figureMarkTopUnits = ((figureUnits * 1.25f) - made.figureMarkUnits) / 2.0f;
    made.figureMarkGapUnits = ORE_MARK_GAP_UNITS;
    made.keyCapUnits = KEY_CAP_UNITS;
    made.keyCapTextTopUnits = KEY_CAP_TEXT_TOP;
    made.barUnits = BUTTON_BAR_UNITS;
    made.barInsetUnits = BUTTON_BAR_INSET;
    made.smallButtonUnits = SMALL_BUTTON_UNITS;
    made.smallButtonTextTopUnits = SMALL_BUTTON_TEXT_TOP;
    made.arrowGapUnits = ARROW_GAP_UNITS;
    made.chipPadUnits = CHIP_PAD_UNITS;
    // No window has a checkbox yet: a small button's side, its tick as large as a locked card's, and an arrow's gap.
    made.checkUnits = SMALL_BUTTON_UNITS;
    made.checkInsetUnits = (SMALL_BUTTON_UNITS - 10.0f) / 2.0f;
    made.checkGapUnits = ARROW_GAP_UNITS;
    // A scroll view's lane is the room the designer keeps right of its part cards (ADR-062 decision 4), its track and thumb a
    // small button's width less its mark's, and its thumb at least a small button long.
    made.scrollLaneUnits = DESIGNER_WIDTH - CARDS_LEFT - CARDS_WIDTH;
    made.scrollThumbUnits = 6.0f;
    made.scrollLeastThumbUnits = SMALL_BUTTON_UNITS;
    made.tableHeadingUnits = TABLE_HEADING_UNITS;
    made.tableRowUnits = TABLE_ROW_UNITS;
    made.tableFigureTopUnits = TABLE_FIGURE_TOP_UNITS;
    made.tableDetailTopUnits = TABLE_DETAIL_TOP_UNITS;
    made.tableRowGapUnits = TABLE_ROW_GAP_UNITS;
    made.tableMarkUnits = TABLE_MARK_UNITS;
    return made;
  }();
  return THEME;
}

// A window's body on the canvas: its content inset from its sides and a padding from its title bar and its foot, stretched
// across it, so that a row of buttons shares its width.
Neuron::LayoutStyle BodyStyle() noexcept
{
  return {.padding = {.left = WINDOW_INSET, .top = PADDING, .right = WINDOW_INSET, .bottom = PADDING},
          .alignX = Neuron::LayoutAlign::Stretch};
}

// A text the canvas sets in _face on that face's line, cut short where it does not fit.
Neuron::LayoutText FittedText(std::string _text, Hud::Typeface _face, float _lineUnits)
{
  return {.text = std::move(_text), .font = Hud::FontOf(_face), .lineUnits = _lineUnits, .flow = Neuron::TextFlow::Fit};
}

// A press on a window's button: _action at its place among the layout's actions, which the button's region names by its
// payload, its rectangle given once the canvas is finished.
std::uint32_t PressOf(Hud::Layout& _layout, const Hud::Action& _action)
{
  _layout.actions.emplace_back(Hud::Rect{}, _action);
  return static_cast<std::uint32_t>(_layout.actions.size() - 1);
}

// A locked part's card's answer to the pointer: _action at its place among the layout's previews, which the card's region
// names by its payload (ADR-017 decision 7).
std::uint32_t PreviewOf(Hud::Layout& _layout, const Hud::Action& _action)
{
  _layout.previews.emplace_back(Hud::Rect{}, _action);
  return static_cast<std::uint32_t>(_layout.previews.size() - 1);
}

// _button as the canvas draws it (ADR-043 decision 4), naming its action's place among the layout's actions while it is
// enabled: its label up to a '|', and after one its cost, as Ore's gem and the figure. A '|' gives the button a cost's
// place, which its reason takes while it is disabled, even with no cost after it; a button without one shows no reason.
Neuron::ButtonContent ContentOf(Hud::Layout& _layout, const Hud::Button& _button)
{
  const std::size_t split = _button.label.find('|');
  Neuron::ButtonContent content{.label = _button.label.substr(0, split),
                                .note = split != std::string::npos ? _button.note : std::string(),
                                .key = _button.key,
                                .payload = _button.enabled ? PressOf(_layout, _button.action) : Neuron::NO_PAYLOAD,
                                .enabled = _button.enabled,
                                .picked = _button.selected};
  std::int32_t cost = 0;
  const std::string figure = split == std::string::npos ? std::string() : _button.label.substr(split + 1);
  if (split != std::string::npos && std::from_chars(figure.data(), figure.data() + figure.size(), cost).ec == std::errc{})
  {
    content.figure = Outpost::WithThousands(cost);
    content.figureMark = Hud::SpriteOf(Hud::Sprite::OreMark);
  }
  if (_button.progressPermille.has_value())
    content.progress =
      static_cast<float>(std::clamp(*_button.progressPermille, 0, Outpost::PERMILLE)) / static_cast<float>(Outpost::PERMILLE);
  return content;
}

// Where a window stands on the canvas: where the player left it, kept on the screen (ADR-031 decision 2), or else at its
// first place, an anchor of the screen less a margin (ADR-100 decision 8).
struct WindowPlace
{
  std::optional<Outpost::WindowManager::Point> corner;
  Neuron::LayoutAnchor anchor = Neuron::LayoutAnchor::Middle;
  Neuron::LayoutEdges margin;
};

// How tall a window at _place may stand on a screen _screenHeightUnits tall: down to a margin above the screen's foot from
// its top, where it stands at its corner or at a top anchor; between its anchor's margins, where it stands in the middle;
// or up to a margin below the screen's top from its foot, at a bottom anchor (ADR-100 decision 7).
float MostHeightOf(const WindowPlace& _place, float _screenHeightUnits) noexcept
{
  if (_place.corner.has_value())
    return _screenHeightUnits - _place.corner->yUnits - MARGIN;
  switch (_place.anchor)
  {
  case Neuron::LayoutAnchor::TopLeft:
  case Neuron::LayoutAnchor::Top:
  case Neuron::LayoutAnchor::TopRight:
    return _screenHeightUnits - _place.margin.top - MARGIN;
  case Neuron::LayoutAnchor::BottomLeft:
  case Neuron::LayoutAnchor::Bottom:
  case Neuron::LayoutAnchor::BottomRight:
    return _screenHeightUnits - _place.margin.bottom - MARGIN;
  case Neuron::LayoutAnchor::Left:
  case Neuron::LayoutAnchor::Middle:
  case Neuron::LayoutAnchor::Right:
    break;
  }
  return _screenHeightUnits - _place.margin.top - _place.margin.bottom;
}

// Opens _kind's window on _canvas where _place puts it, named and titled by its row (ADR-100), its body of _body's style, no
// taller than the screen of _screenHeightUnits leaves it while a scroll view in it can give up room, and returns its
// frame's node.
std::size_t BeginWindowAt(Neuron::Canvas& _canvas, Neuron::Interaction& _interaction, Outpost::WindowKind _kind, const WindowPlace& _place,
                          float _widthUnits, const Neuron::LayoutStyle& _body, float _screenHeightUnits)
{
  const Outpost::WindowRow& row = Outpost::RowOf(_kind);
  const Neuron::WidgetId window = _interaction.IdOf(row.name);
  const float most = MostHeightOf(_place, _screenHeightUnits);
  if (_place.corner.has_value())
    return _canvas.BeginWindow(window, std::string(row.title), *_place.corner, _widthUnits, _body, most);
  return _canvas.BeginWindow(window, std::string(row.title), _place.anchor, _place.margin, _widthUnits, _body, most);
}

// The rectangle of each press among the layout's actions, and of each card's answer to the pointer among its previews,
// from the regions from _firstRegion that name one, once the canvas is finished.
void PlacePresses(Hud::Layout& _layout, std::size_t _firstRegion)
{
  for (std::size_t region = _firstRegion; region < _layout.regions.size(); ++region)
  {
    const Neuron::CanvasRegion& pressed = _layout.regions[region];
    if (pressed.payload != Neuron::NO_PAYLOAD)
      (pressed.hoverOnly ? _layout.previews : _layout.actions)[pressed.payload].first = pressed.area;
  }
}

// Draws the window built on _canvas into the layout, and keeps beside it the rectangle of each button among the layout's
// actions from _firstAction, and of each card among its previews, and the window's frame, title bar, close box and corner.
void FinishWindow(Hud::Layout& _layout, Neuron::Canvas& _canvas, std::size_t _frame, Outpost::WindowKind _kind, std::size_t _firstAction,
                  Neuron::Canvas* _front)
{
  const std::size_t firstRegion = _layout.regions.size();
  // The window's own layer, which the front canvas's layers follow, if this is the front window, and then an open
  // drop-down's overlay.
  const std::size_t layer = _layout.layers.size();
  _canvas.Finish(_layout, _front);
  PlacePresses(_layout, firstRegion);
  const std::size_t panel = _layout.layers[layer].firstPanel;
  const Neuron::LayoutNode& frame = _canvas.Tree().Node(_frame);
  _layout.windows.push_back({.kind = _kind,
                             .frame = _layout.panels[panel],
                             .titleBar = _layout.panels[panel + 1],
                             .closeBox = _layout.panels[panel + 2],
                             .corner = {.xUnits = frame.leftUnits, .yUnits = frame.topUnits},
                             .firstAction = _firstAction});
}

// Where the windows are built: the layout they are drawn into, its metrics, what their widgets share, and the screen's size
// in reference units; and for the front window, the canvas of what stands over the windows, which it finishes with itself.
struct WindowBench
{
  Hud::Layout* layout = nullptr;
  const Hud::TextMetrics* metrics = nullptr;
  Neuron::Interaction* interaction = nullptr;
  float screenWidthUnits = 0.0f;
  float screenHeightUnits = 0.0f;
  Neuron::Canvas* front = nullptr;
};

// Builds _kind's window _widthUnits wide on a canvas of its own, where _place puts it, its body of _body's style holding what
// _build adds to the canvas, and draws it into the bench's layout.
template <class Build>
void LayWindow(const WindowBench& _bench, Outpost::WindowKind _kind, const WindowPlace& _place, float _widthUnits,
               const Neuron::LayoutStyle& _body, Build&& _build)
{
  Neuron::Canvas canvas(GameTheme(), *_bench.metrics, *_bench.interaction, _bench.screenWidthUnits, _bench.screenHeightUnits);
  const std::size_t firstAction = _bench.layout->actions.size();
  const std::size_t frame = BeginWindowAt(canvas, *_bench.interaction, _kind, _place, _widthUnits, _body, _bench.screenHeightUnits);
  std::forward<Build>(_build)(canvas);
  canvas.EndWindow();
  FinishWindow(*_bench.layout, canvas, frame, _kind, firstAction, _bench.front);
}

// A label over what follows it in a window, in the label face and tracked, its line a name's, the label a little down it.
constexpr float HEADING_TOP = 3.0f;

std::size_t Heading(Neuron::Canvas& _canvas, std::string _text, Neuron::LayoutStyle _style = {})
{
  _style.padding.top = HEADING_TOP;
  return _canvas.Label({.text = std::move(_text),
                        .font = Hud::FontOf(Hud::Typeface::Label),
                        .lineUnits = NAME_LINE_UNITS - HEADING_TOP,
                        .trackingUnits = LABEL_TRACKING_UNITS},
                       LABEL_COLOR, _style);
}

// The Controls window (ADR-068 decision 4): every key and mouse action the game reads in a match, as KeyBindings lists
// them, under their headings, the keys in a column of their own.
constexpr float CONTROLS_WINDOW_WIDTH = 700.0f;
constexpr float CONTROLS_KEYS_WIDTH = 240.0f;
constexpr float CONTROLS_HEADING_GAP = 10.0f;
// Its lines scroll, in a view no shorter than leaves the window 500 units tall at its least, so that on the menu, where it
// is the one window counted, a 16:9 screen, 540 units tall at 200%, holds it between its margins (ADR-070 decision 3); and
// its view's lane stands in the window's inset at its right.
constexpr float CONTROLS_LEAST_UNITS = 440.0f;

// The orders window (ADR-080 decision 9): under a label, the seat's scheduled orders, a line each; and under another,
// the form's rows, each its label and its value between the arrows that step it, the line that says when the order fires,
// and the button that gives it. It is as tall as its most orders and rows, so that it does not jump as the form changes.
constexpr float ORDERS_WINDOW_WIDTH = 640.0f;
constexpr float ORDER_ROW_UNITS = 30.0f;
constexpr float ORDER_VALUE_LEFT = 150.0f;
constexpr float ORDER_ARROW_TOP = 3.0f;
constexpr float ORDER_TEXT_TOP = 6.0f;
constexpr float ORDER_LABEL_TOP = 9.0f;

// What the line beside WAITING says.
constexpr std::string_view ORDERS_LEAVE_NOTE = "Any order to its ships takes them out";

// The away window (ADR-080 decision 9): how long the player was away, in the title face, and what happened, a line each.
constexpr float AWAY_WINDOW_WIDTH = 600.0f;

// The leave window (ADR-078 decision 11): in a universe, whether to go back to the menu, the deputy keeping the empire going
// meanwhile. Leave universe goes, and Stay closes the window, side by side under the question.
constexpr float LEAVE_WINDOW_WIDTH = 560.0f;
constexpr std::string_view LEAVE_QUESTION = "Leave for the menu?";
constexpr std::string_view LEAVE_DEPUTY = "Your deputy keeps your empire going while you are away.";

// The away window: how long the player was away, and as many of what happened as it shows.
void LayAway(const WindowBench& _bench, const Hud::AwayPanel& _panel, const WindowPlace& _place)
{
  LayWindow(_bench, Outpost::WindowKind::Away, _place, AWAY_WINDOW_WIDTH, BodyStyle(),
            [&](Neuron::Canvas& _canvas)
            {
              (void)_canvas.Label(FittedText(_panel.since, Hud::Typeface::Title, TITLE_LINE_UNITS), TEXT_COLOR);
              for (std::size_t line = 0; line < _panel.lines.size() && line < Hud::AWAY_LINES; ++line)
                (void)_canvas.Label(FittedText(_panel.lines[line], Hud::Typeface::Name, NAME_LINE_UNITS), NUMBERS_COLOR);
            });
}

// The leave window: the question, what the deputy does meanwhile, and Leave universe and Stay side by side.
void LayLeave(const WindowBench& _bench, const WindowPlace& _place)
{
  LayWindow(_bench, Outpost::WindowKind::Leave, _place, LEAVE_WINDOW_WIDTH, BodyStyle(),
            [&](Neuron::Canvas& _canvas)
            {
              (void)_canvas.Label(FittedText(std::string(LEAVE_QUESTION), Hud::Typeface::Title, TITLE_LINE_UNITS), TEXT_COLOR);
              (void)_canvas.Label(FittedText(std::string(LEAVE_DEPUTY), Hud::Typeface::Name, NAME_LINE_UNITS), NUMBERS_COLOR);
              (void)_canvas.Tree().Add({.heightUnits = BUTTON_GAP});
              (void)_canvas.Tree().Begin(Neuron::LayoutKind::Row, {.gapUnits = BUTTON_GAP});
              const Hud::Button leave{.label = "Leave universe", .action = {.kind = Hud::ActionKind::BackToMenu}};
              const Hud::Button stay{.label = "Stay", .action = {.kind = Hud::ActionKind::Stay}};
              (void)_canvas.Button("leave", ContentOf(*_bench.layout, leave), {.heightUnits = BUTTON_HEIGHT, .grow = 1.0f});
              (void)_canvas.Button("stay", ContentOf(*_bench.layout, stay), {.heightUnits = BUTTON_HEIGHT, .grow = 1.0f});
              _canvas.Tree().End();
            });
}

// The Controls window: each heading over its keys, a gap before each but the first, and each key in its column beside what
// it does.
void LayControls(const WindowBench& _bench, const WindowPlace& _place)
{
  LayWindow(_bench, Outpost::WindowKind::Controls, _place, CONTROLS_WINDOW_WIDTH, BodyStyle(),
            [&](Neuron::Canvas& _canvas)
            {
              (void)_canvas.BeginScroll("lines", {.widthUnits = CONTROLS_WINDOW_WIDTH - (2.0f * WINDOW_INSET) + GameTheme().scrollLaneUnits,
                                                  .leastHeightUnits = CONTROLS_LEAST_UNITS,
                                                  .grow = 1.0f});
              const std::vector<Outpost::KeyBinding> bindings = Outpost::KeyBindings();
              for (std::size_t line = 0; line < bindings.size(); ++line)
              {
                const Outpost::KeyBinding& binding = bindings[line];
                if (binding.does.empty())
                {
                  if (line > 0)
                    (void)_canvas.Tree().Add({.heightUnits = CONTROLS_HEADING_GAP});
                  (void)Heading(_canvas, binding.keys);
                  continue;
                }
                (void)_canvas.Tree().Begin(Neuron::LayoutKind::Row, {});
                (void)_canvas.Label(FittedText(binding.keys, Hud::Typeface::Name, NAME_LINE_UNITS), ACCENT_COLOR,
                                    {.widthUnits = CONTROLS_KEYS_WIDTH, .padding = {.right = FIT_GAP_UNITS}});
                (void)_canvas.Label(FittedText(binding.does, Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR, {.grow = 1.0f});
                _canvas.Tree().End();
              }
              _canvas.EndScroll();
            });
}

// The orders window: WAITING, and the note beside it at its right; the scheduled orders, each with its
// cancel button at its line's right end, in room for as many as it shows and the line that counts the rest; NEW ORDER FOR
// THE SELECTION over the form's rows, each its label and its value as a stepper, in room for every row; the line that says
// when the order fires; and Give.
// A field of the orders window's form picked from a drop-down, between the arrows that step it as a stepper's do (ADR-080
// decision 9): as wide as the stepper leaves its value room, and each choice naming the action that picks it.
void OrderDropDown(Neuron::Canvas& _canvas, Neuron::Interaction& _interaction, Hud::Layout& _layout, std::size_t _row,
                   const Hud::OrderRow& _order, std::uint32_t _previous, std::uint32_t _next)
{
  constexpr float ROOM = ORDERS_WINDOW_WIDTH - WINDOW_INSET - ORDER_VALUE_LEFT;
  constexpr float ARROWS = 2.0f * (SMALL_BUTTON_UNITS + ARROW_GAP_UNITS);
  std::vector<std::string> names;
  names.reserve(_order.choices.size());
  for (const Outpost::OrderChoice& choice : _order.choices)
    names.push_back(choice.name);
  _interaction.PushId(_interaction.IdOf(std::format("row {}", _row)));
  (void)_canvas.Tree().Begin(Neuron::LayoutKind::Row, {.gapUnits = ARROW_GAP_UNITS});
  (void)_canvas.SmallButton("<", "<", true, _previous);
  (void)_canvas.DropDown(
    "value", _order.value, std::move(names), _order.picked,
    [&_layout, &_order](std::size_t _choice)
    {
      const Outpost::OrderChoice& choice = _order.choices[_choice];
      return PressOf(_layout, {.kind = Hud::ActionKind::PickOrder, .entity = choice.target, .field = _order.field, .value = choice.value});
    },
    {.widthUnits = ROOM - ARROWS, .heightUnits = SMALL_BUTTON_UNITS});
  (void)_canvas.SmallButton(">", ">", true, _next);
  _canvas.Tree().End();
  _interaction.PopId();
}

void LayOrders(const WindowBench& _bench, const Hud::OrdersPanel& _panel, const WindowPlace& _place)
{
  LayWindow(
    _bench, Outpost::WindowKind::Orders, _place, ORDERS_WINDOW_WIDTH, BodyStyle(),
    [&](Neuron::Canvas& _canvas)
    {
      Hud::Layout& layout = *_bench.layout;
      Neuron::LayoutTree& tree = _canvas.Tree();
      constexpr float ROOM = ORDERS_WINDOW_WIDTH - (2.0f * WINDOW_INSET);
      (void)tree.Begin(Neuron::LayoutKind::Row, {});
      (void)Heading(_canvas, "WAITING", {.grow = 1.0f});
      // That a ship given another order leaves the one it waits on, as the cancel button takes them all out.
      (void)_canvas.Label(FittedText(std::string(ORDERS_LEAVE_NOTE), Hud::Typeface::Name, NAME_LINE_UNITS), DIM_TEXT_COLOR,
                          {.mostWidthUnits = ROOM / 2.0f});
      tree.End();
      (void)tree.Begin(Neuron::LayoutKind::Column, {.heightUnits = static_cast<float>(Hud::ORDERS_SHOWN + 1) * NAME_LINE_UNITS,
                                                    .alignX = Neuron::LayoutAlign::Stretch});
      if (_panel.scheduled.empty())
        (void)_canvas.Label(FittedText("None", Hud::Typeface::Name, NAME_LINE_UNITS), DIM_TEXT_COLOR);
      // The cancel button stands in the middle of its line, which it is a little taller than.
      for (const Hud::ScheduledLine& line : _panel.scheduled)
      {
        (void)tree.Begin(Neuron::LayoutKind::Row,
                         {.heightUnits = NAME_LINE_UNITS, .gapUnits = ARROW_GAP_UNITS, .alignY = Neuron::LayoutAlign::Center});
        (void)_canvas.Label(FittedText(line.text, Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR, {.grow = 1.0f});
        (void)_canvas.SmallButton(std::format("cancel {}", line.order), "X", true,
                                  PressOf(layout, {.kind = Hud::ActionKind::CancelOrder, .order = line.order}));
        tree.End();
      }
      if (_panel.more > 0)
        (void)_canvas.Label(FittedText(std::format("and {} more", _panel.more), Hud::Typeface::Name, NAME_LINE_UNITS), DIM_TEXT_COLOR);
      tree.End();
      (void)tree.Add({.heightUnits = SECTION_GAP});
      (void)Heading(_canvas, "NEW ORDER FOR THE SELECTION");
      (void)tree.Begin(Neuron::LayoutKind::Column, {.heightUnits = static_cast<float>(Hud::ORDER_ROWS) * ORDER_ROW_UNITS});
      for (std::size_t row = 0; row < _panel.rows.size() && row < Hud::ORDER_ROWS; ++row)
      {
        const Hud::OrderRow& order = _panel.rows[row];
        (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = ORDER_ROW_UNITS});
        (void)_canvas.Label({.text = order.label,
                             .font = Hud::FontOf(Hud::Typeface::Label),
                             .lineUnits = ORDER_ROW_UNITS - ORDER_LABEL_TOP,
                             .trackingUnits = LABEL_TRACKING_UNITS},
                            ROW_LABEL_COLOR, {.widthUnits = ORDER_VALUE_LEFT - WINDOW_INSET, .padding = {.top = ORDER_LABEL_TOP}});
        // The arrows' actions in the order they are drawn, each pressed apart, since a call's arguments are made in any order.
        const std::uint32_t previous = PressOf(layout, {.kind = Hud::ActionKind::StepOrder, .field = order.field, .step = -1});
        const std::uint32_t next = PressOf(layout, {.kind = Hud::ActionKind::StepOrder, .field = order.field, .step = 1});
        (void)tree.Begin(Neuron::LayoutKind::Column, {.padding = {.top = ORDER_ARROW_TOP}});
        if (Outpost::OrderForm::TakesDropDown(order.field))
          OrderDropDown(_canvas, *_bench.interaction, layout, row, order, previous, next);
        else
          _canvas.Stepper(std::format("row {}", row),
                          {.text = order.value, .font = Hud::FontOf(Hud::Typeface::Name), .lineUnits = NAME_LINE_UNITS}, TEXT_COLOR,
                          ORDERS_WINDOW_WIDTH - WINDOW_INSET - ORDER_VALUE_LEFT, ORDER_TEXT_TOP - ORDER_ARROW_TOP, true, previous, next);
        tree.End();
        tree.End();
      }
      tree.End();
      (void)tree.Add({.heightUnits = PANEL_GAP});
      (void)_canvas.Label(FittedText(_panel.fires, Hud::Typeface::Name, NAME_LINE_UNITS), NUMBERS_COLOR);
      (void)tree.Add({.heightUnits = PANEL_GAP});
      (void)_canvas.Button("give", ContentOf(layout, _panel.give), {.heightUnits = BUTTON_HEIGHT});
    });
}

// A window's size, in reference units.
struct WindowSize
{
  float widthUnits = 0.0f;
  float heightUnits = 0.0f;
};

// How large a window stands at its least once _build has built it on the canvas, which WindowsFit compares with the screen
// (ADR-070): it is built where nothing else is, on a screen of no size, so that its scroll views give up all the room they
// can, and with no font, since every line it holds is one line however wide.
template <class Build> WindowSize BuiltSize(Build&& _build)
{
  Hud::Layout scratch;
  Neuron::Interaction interaction;
  const Hud::TextMetrics metrics({}, 1.0f);
  std::forward<Build>(_build)(WindowBench{.layout = &scratch, .metrics = &metrics, .interaction = &interaction});
  if (scratch.windows.empty())
    return {};
  return {.widthUnits = scratch.windows.back().frame.width, .heightUnits = scratch.windows.back().frame.height};
}

// Whether Constructors are offered _type on the map of _newest: a structure the tuning data lets them build, and a Relay only
// where there are sectors for it to hold (ADR-056).
bool IsOffered(const Outpost::Snapshot& _newest, const Outpost::StructureTypeView& _type) noexcept
{
  return _type.buildable && (_type.structure != Outpost::StructureKind::Relay || !_newest.sectors.empty());
}

// Where a window first stands, as its row's first place has it (ADR-100 decision 8): an anchor of the screen less a margin.
// The top-right corner; the right edge level with the places under the Ore; or the middle. The places under the Ore are
// the window manager's slots, the first of which is the margin.
WindowPlace FirstPlaceOf(Outpost::FirstPlace _first) noexcept
{
  switch (_first)
  {
  case Outpost::FirstPlace::TopRight:
    return {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = MARGIN, .right = MARGIN}};
  case Outpost::FirstPlace::UnderTheOre:
    return {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.left = MARGIN, .top = WINDOWS_TOP}};
  case Outpost::FirstPlace::RightUnderTheOre:
    return {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = WINDOWS_TOP, .right = MARGIN}};
  case Outpost::FirstPlace::Middle:
    break;
  }
  return {.anchor = Neuron::LayoutAnchor::Middle, .margin = {.left = MARGIN, .top = MARGIN, .right = MARGIN, .bottom = MARGIN}};
}

void LayDesigner(const WindowBench& _bench, const Hud::DesignerPanel& _panel, const WindowPlace& _place);

// How large _window stands at its least holding its most, in the match _newest is of, or the Controls window outside one,
// for WindowsFit (ADR-070): the away window with as many lines as it shows, and the designer with every component of the
// match, locked or not, its part rows showing a line of cards. The Controls window, its lines at their least, and the
// orders window are as large whatever they hold. A window its row does not count is not measured.
WindowSize LeastOf(Outpost::WindowKind _window, const Outpost::Snapshot* _newest)
{
  const WindowPlace corner{.corner = Outpost::WindowManager::Point{}};
  switch (_window)
  {
  case Outpost::WindowKind::Controls:
    return BuiltSize([&](const WindowBench& _bench) { LayControls(_bench, corner); });
  case Outpost::WindowKind::Orders:
    return BuiltSize([&](const WindowBench& _bench) { LayOrders(_bench, {}, corner); });
  case Outpost::WindowKind::Away:
    return BuiltSize([&](const WindowBench& _bench) { LayAway(_bench, {.lines = std::vector<std::string>(Hud::AWAY_LINES)}, corner); });
  case Outpost::WindowKind::Designer:
  {
    Outpost::Designer designer;
    designer.Update(*_newest);
    const Hud::DesignerPanel panel = DescribeDesigner(*_newest, designer, std::nullopt);
    return BuiltSize([&](const WindowBench& _bench) { LayDesigner(_bench, panel, corner); });
  }
  case Outpost::WindowKind::Research:
  case Outpost::WindowKind::Production:
  case Outpost::WindowKind::Leave:
    break;
  }
  return {};
}

// Production and research on the canvas (ADR-100), from the top-left of the window as their layout functions place them:
// the header under the title bar, the cards under their section's label, and the queue under them.

// Where a header's subject stands, and the Ore's box in it.
constexpr float HEADER_SUBJECT_TOP = 40.0f;
constexpr float ORE_BOX_TOP = 38.0f;
constexpr float ORE_BOX_HEIGHT = 30.0f;
// A production option's lines from its card's top: its name, its code and cost, and its time and strengths.
constexpr float OPTION_NAME_TOP = 6.0f;
constexpr float OPTION_DETAIL_TOP = 28.0f;
// A topic card's name and cost on the first line, the cost's figure a little lower; a prerequisite's box in its line.
constexpr float TOPIC_COST_TOP = 3.0f;
constexpr float NEED_BOX_UNITS = 10.0f;
constexpr float NEED_BOX_TOP = 2.0f;
constexpr float NEED_TEXT_LEFT = 16.0f;
// How tall a topic card is at least, and the height group every topic's card is measured in (ADR-061 decision 4).
constexpr float TOPIC_LEAST_UNITS = TOPIC_LINES_TOP + (static_cast<float>(TOPIC_LEAST_LINES) * TOPIC_LINE_UNITS) + TOPIC_FOOT_UNITS;
constexpr std::uint32_t TOPIC_CARDS = 1;
// A queue's row: its inset, where its name starts, where its texts and bar stand down it, and the room for the front job's
// percentage after its bar.
constexpr float QUEUE_ROW_INSET = 10.0f;
constexpr float QUEUE_NAME_LEFT = 32.0f;
constexpr float QUEUE_NUMBER_TOP = 7.0f;
constexpr float QUEUE_NAME_TOP = 5.0f;
constexpr float QUEUE_WAIT_TOP = 9.0f;
constexpr float QUEUE_BAR_TOP = 12.0f;
constexpr float QUEUE_BAR_HEIGHT = 6.0f;
constexpr float QUEUE_PERCENT_UNITS = 44.0f;

// A queue window's body: its header across the whole of it, and what is under the header inset by the windows' inset.
Neuron::LayoutStyle QueueWindowBody() noexcept
{
  return {.alignX = Neuron::LayoutAlign::Stretch};
}

Neuron::LayoutStyle UnderHeader() noexcept
{
  return {.padding = {.left = WINDOW_INSET, .top = PADDING, .right = WINDOW_INSET, .bottom = SECTION_GAP},
          .alignX = Neuron::LayoutAlign::Stretch};
}

// A section's label, in the label face and tracked, on a line as tall as the gap under it to what it labels.
Neuron::LayoutText SectionText(std::string _text)
{
  return {.text = std::move(_text),
          .font = Hud::FontOf(Hud::Typeface::Label),
          .lineUnits = SECTION_BODY_GAP,
          .trackingUnits = LABEL_TRACKING_UNITS};
}

// A text set as it is in _face, on that face's line.
Neuron::LayoutText LineText(std::string _text, Hud::Typeface _face, float _lineUnits)
{
  return {.text = std::move(_text), .font = Hud::FontOf(_face), .lineUnits = _lineUnits};
}

// An amount of Ore on the canvas: Ore's gem, sized by _face, in the middle of the figure's line, then the figure.
void OreAmount(Neuron::Canvas& _canvas, std::int32_t _ore, Hud::Typeface _face, const DirectX::XMFLOAT4& _color,
               Neuron::LayoutStyle _style = {})
{
  const float sizeUnits = Hud::FaceUnits(_face);
  const float mark = std::round(sizeUnits * ORE_MARK_SHARE);
  _style.gapUnits = ORE_MARK_GAP_UNITS;
  (void)_canvas.Tree().Begin(Neuron::LayoutKind::Row, _style);
  (void)_canvas.Icon(Hud::SpriteOf(Hud::Sprite::OreMark), mark, _color, {.padding = {.top = ((sizeUnits * 1.25f) - mark) / 2.0f}});
  (void)_canvas.Label(LineText(Outpost::WithThousands(_ore), _face, sizeUnits * 1.25f), _color);
  _canvas.Tree().End();
}

// A window's header band (ADR-068 decision 1): the hatched band under the title bar and its edge at its foot, a stack that
// takes what is built at its places in it until the tree's End.
void BeginHeader(Neuron::Canvas& _canvas, float _widthUnits)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  const std::size_t band = tree.Begin(Neuron::LayoutKind::Stack, {.heightUnits = DESIGNER_HEADER - Hud::TITLE_BAR_UNITS});
  _canvas.Fill(band, TITLE_HATCH_COLOR, Neuron::CanvasFill::Hatched);
  _canvas.Fill(tree.Add({.widthUnits = _widthUnits, .heightUnits = LINE_UNITS, .anchor = Neuron::LayoutAnchor::BottomLeft}), EDGE_COLOR);
}

// The player's Ore in its box at a header's right.
void OreBox(Neuron::Canvas& _canvas, std::int32_t _ore)
{
  (void)_canvas.BeginBox({.face = FIELD_COLOR, .edge = EDGE_COLOR},
                         {.widthUnits = ORE_BOX_WIDTH,
                          .heightUnits = ORE_BOX_HEIGHT,
                          .padding = {.top = LINE_UNITS, .right = 10.0f},
                          .alignX = Neuron::LayoutAlign::End,
                          .anchor = Neuron::LayoutAnchor::TopRight,
                          .margin = {.top = ORE_BOX_TOP - Hud::TITLE_BAR_UNITS, .right = WINDOW_INSET}});
  OreAmount(_canvas, _ore, Hud::Typeface::Title, GOLD_COLOR);
  _canvas.EndBox();
}

// A window's header: its band; its subject in the title face, between the arrows that step it when it is stepped through;
// and the player's Ore in its box.
void Header(Neuron::Canvas& _canvas, Hud::Layout& _layout, const std::string& _subject, std::int32_t _ore, float _widthUnits,
            std::optional<std::pair<Hud::Action, Hud::Action>> _steps = std::nullopt, bool _canStep = false)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  BeginHeader(_canvas, _widthUnits);
  const float room = _widthUnits - (2.0f * WINDOW_INSET) - ORE_BOX_WIDTH - FIT_GAP_UNITS;
  if (_steps.has_value())
  {
    // The arrows' actions in the order they are drawn, each only while the arrows step.
    const std::uint32_t previous = _canStep ? PressOf(_layout, _steps->first) : Neuron::NO_PAYLOAD;
    const std::uint32_t next = _canStep ? PressOf(_layout, _steps->second) : Neuron::NO_PAYLOAD;
    (void)tree.Begin(Neuron::LayoutKind::Column, {.anchor = Neuron::LayoutAnchor::TopLeft,
                                                  .margin = {.left = WINDOW_INSET, .top = ARROWS_IN_HEADER_TOP - Hud::TITLE_BAR_UNITS}});
    _canvas.Stepper("subject", LineText(_subject, Hud::Typeface::Title, TITLE_LINE_UNITS), TEXT_COLOR, room,
                    HEADER_SUBJECT_TOP - ARROWS_IN_HEADER_TOP, _canStep, previous, next);
    tree.End();
  }
  else
  {
    (void)_canvas.Label(FittedText(_subject, Hud::Typeface::Title, TITLE_LINE_UNITS), TEXT_COLOR,
                        {.mostWidthUnits = room,
                         .anchor = Neuron::LayoutAnchor::TopLeft,
                         .margin = {.left = WINDOW_INSET, .top = HEADER_SUBJECT_TOP - Hud::TITLE_BAR_UNITS}});
  }
  OreBox(_canvas, _ore);
  tree.End();
}

// A queue: its label, then a row for each job, a box in the cards' look with the job's number, its name, and
// the front one's progress or its wait, for Ore or for the fleet cap (ADR-071 decision 4), at its right.
void QueueRows(Neuron::Canvas& _canvas, const std::vector<Hud::QueueLine>& _queue)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)_canvas.Label(SectionText(QueueLabel(_queue.size())), LABEL_COLOR);
  for (std::size_t slot = 0; slot < _queue.size(); ++slot)
  {
    const Hud::QueueLine& line = _queue[slot];
    (void)_canvas.BeginBox({.face = CARD_COLOR, .edge = EDGE_COLOR}, {.heightUnits = QUEUE_ROW_HEIGHT,
                                                                      .padding = {.left = QUEUE_ROW_INSET, .right = QUEUE_ROW_INSET},
                                                                      .alignX = Neuron::LayoutAlign::Stretch});
    (void)tree.Begin(Neuron::LayoutKind::Row, {});
    (void)_canvas.Label(LineText(std::to_string(slot + 1), Hud::Typeface::Figure, FIGURE_LINE_UNITS), LABEL_COLOR,
                        {.widthUnits = QUEUE_NAME_LEFT - QUEUE_ROW_INSET, .padding = {.top = QUEUE_NUMBER_TOP}});
    (void)_canvas.Label(FittedText(line.name, Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR,
                        {.padding = {.top = QUEUE_NAME_TOP, .right = FIT_GAP_UNITS}, .grow = 1.0f});
    if (line.waiting)
    {
      const std::string_view waiting = line.capped ? "WAITING FOR FLEET CAP" : "WAITING FOR ORE";
      (void)_canvas.Label(LineText(std::string(waiting), Hud::Typeface::Label, NAME_LINE_UNITS), AMBER_COLOR,
                          {.padding = {.top = QUEUE_WAIT_TOP}});
    }
    else if (line.front)
    {
      (void)tree.Begin(Neuron::LayoutKind::Stack, {.widthUnits = QUEUE_BAR_WIDTH + QUEUE_PERCENT_UNITS});
      (void)_canvas.ProgressBar(static_cast<float>(std::clamp(line.permille, 0, Outpost::PERMILLE)) / static_cast<float>(Outpost::PERMILLE),
                                {.widthUnits = QUEUE_BAR_WIDTH,
                                 .heightUnits = QUEUE_BAR_HEIGHT,
                                 .anchor = Neuron::LayoutAnchor::TopLeft,
                                 .margin = {.top = QUEUE_BAR_TOP}});
      (void)_canvas.Label(LineText(std::format("{}%", line.permille / 10), Hud::Typeface::Figure, FIGURE_LINE_UNITS), TEXT_COLOR,
                          {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = QUEUE_NUMBER_TOP}});
      tree.End();
    }
    tree.End();
    _canvas.EndBox();
    (void)tree.Add({.heightUnits = QUEUE_ROW_GAP});
  }
}

// A production option's card (ADR-068 decision 3), a composite of the game's: its name; its code, cut short where it would
// meet its cost, and its cost; and its build time and its strength against each hull at its right, three segments a hull, three
// lit for Good, two for Fair and one for Poor, so that the bars read without their colors.
void OptionCard(Neuron::Canvas& _canvas, Hud::Layout& _layout, const Hud::QueueOption& _option, std::size_t _index)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  const bool enabled = _option.enabled;
  (void)_canvas.BeginCard(std::to_string(_index), {.face = enabled ? CARD_COLOR : FIELD_COLOR, .edge = EDGE_COLOR},
                          enabled ? PressOf(_layout, _option.action) : Neuron::NO_PAYLOAD,
                          {.heightUnits = OPTION_HEIGHT,
                           .padding = {.left = CARD_INSET, .top = OPTION_NAME_TOP, .right = CARD_INSET},
                           .alignX = Neuron::LayoutAlign::Stretch});
  (void)_canvas.Label(FittedText(_option.name, Hud::Typeface::Name, NAME_LINE_UNITS), enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR,
                      {.heightUnits = OPTION_DETAIL_TOP - OPTION_NAME_TOP});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = OPTION_THIRD_LINE_TOP - OPTION_DETAIL_TOP});
  (void)_canvas.Label(FittedText(_option.detail, Hud::Typeface::Detail, NAME_LINE_UNITS), enabled ? CODE_COLOR : LOCKED_TEXT_COLOR,
                      {.padding = {.right = FIT_GAP_UNITS}, .grow = 1.0f});
  OreAmount(_canvas, _option.cost, Hud::Typeface::Figure, enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR);
  tree.End();
  (void)tree.Begin(Neuron::LayoutKind::Row, {});
  (void)_canvas.Label(LineText(_option.time, Hud::Typeface::Detail, NAME_LINE_UNITS), enabled ? LABEL_COLOR : LOCKED_TEXT_COLOR);
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = STRENGTH_GROUP_GAP, .anchor = Neuron::LayoutAnchor::TopRight});
  for (const Hud::HullRating& strength : _option.strengths)
  {
    (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = STRENGTH_LETTER_GAP});
    (void)_canvas.Label(LineText(strength.hull, Hud::Typeface::Label, NAME_LINE_UNITS), enabled ? ROW_LABEL_COLOR : LOCKED_TEXT_COLOR);
    (void)tree.Begin(Neuron::LayoutKind::Row, {.padding = {.top = 5.0f}, .gapUnits = STRENGTH_SEGMENT_GAP});
    const auto lit = static_cast<std::size_t>(strength.rating) + 1;
    for (std::size_t segment = 0; segment < STRENGTH_SEGMENTS; ++segment)
    {
      const DirectX::XMFLOAT4& color = segment >= lit ? BAR_TRACK_COLOR : enabled ? RatingColor(strength.rating) : LOCKED_TEXT_COLOR;
      _canvas.Fill(tree.Add({.widthUnits = STRENGTH_SEGMENT_UNITS, .heightUnits = STRENGTH_SEGMENT_HEIGHT}), color);
    }
    tree.End();
    tree.End();
  }
  tree.End();
  tree.End();
  _canvas.EndBox();
}

// The production window: the producer between the arrows that step through the others, a card for each thing
// it builds, two to a row and at least a row's room, kept still as the queue under them grows (ADR-043 decision 6), with
// the hint over them when there is one, and the queue.
void LayProduction(const WindowBench& _bench, const Hud::ProductionPanel& _panel, const WindowPlace& _place)
{
  LayWindow(_bench, Outpost::WindowKind::Production, _place, PRODUCTION_WINDOW_WIDTH, QueueWindowBody(),
            [&](Neuron::Canvas& _canvas)
            {
              Neuron::LayoutTree& tree = _canvas.Tree();
              Header(_canvas, *_bench.layout, _panel.producer, _panel.ore, PRODUCTION_WINDOW_WIDTH,
                     std::pair{Hud::Action{.kind = Hud::ActionKind::PreviousProducer}, Hud::Action{.kind = Hud::ActionKind::NextProducer}},
                     _panel.canStep);
              (void)tree.Begin(Neuron::LayoutKind::Column, UnderHeader());
              (void)_canvas.Label(SectionText("BUILD"), LABEL_COLOR);
              (void)tree.Begin(Neuron::LayoutKind::Stack, {.leastHeightUnits = OPTION_HEIGHT, .alignX = Neuron::LayoutAlign::Stretch});
              (void)tree.Begin(Neuron::LayoutKind::Grid, {.gapUnits = CARD_SPACING, .alignX = Neuron::LayoutAlign::Stretch, .columns = 2});
              for (std::size_t option = 0; option < _panel.options.size(); ++option)
                OptionCard(_canvas, *_bench.layout, _panel.options[option], option);
              tree.End();
              if (!_panel.hint.empty())
              {
                (void)_canvas.Label(FittedText(_panel.hint, Hud::Typeface::Detail, TOPIC_LINE_UNITS), LABEL_COLOR,
                                    {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.top = 16.0f}});
              }
              tree.End();
              (void)tree.Add({.heightUnits = SECTION_GAP});
              QueueRows(_canvas, _panel.queue);
              tree.End();
            });
}

// A topic's card (ADR-043 decision 6), a composite of the game's: its name and its cost; its effect, wrapped to the card;
// each prerequisite neither researched nor queued on a line of its own with its box, "NEEDS · RESEARCH LAB L2", then
// "+ HULL PLATING", and hatched and dimmed while it has one (ADR-061 decision 4); or else its tier and time. Every topic's
// card is as tall as the most lines any topic needs, in the height group TopicMeasure measures the others in.
void TopicCardOf(Neuron::Canvas& _canvas, Hud::Layout& _layout, const Hud::TopicCard& _topic, std::size_t _index)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  const bool blocked = !_topic.needs.empty();
  (void)_canvas.BeginCard(std::to_string(_index),
                          {.face = blocked          ? LOCKED_COLOR
                                   : _topic.enabled ? CARD_COLOR
                                                    : FIELD_COLOR,
                           .hatch = blocked ? std::optional(LOCKED_HATCH_COLOR) : std::nullopt,
                           .edge = EDGE_COLOR},
                          _topic.enabled ? PressOf(_layout, _topic.action) : Neuron::NO_PAYLOAD,
                          {.leastHeightUnits = TOPIC_LEAST_UNITS,
                           .padding = {.left = CARD_INSET, .top = OPTION_NAME_TOP, .right = CARD_INSET, .bottom = TOPIC_FOOT_UNITS},
                           .alignX = Neuron::LayoutAlign::Stretch,
                           .heightGroup = TOPIC_CARDS});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = TOPIC_LINES_TOP - OPTION_NAME_TOP});
  (void)_canvas.Label(FittedText(_topic.name, Hud::Typeface::Name, NAME_LINE_UNITS), _topic.enabled ? TEXT_COLOR : LOCKED_TEXT_COLOR,
                      {.padding = {.right = FIT_GAP_UNITS}, .grow = 1.0f});
  OreAmount(_canvas, _topic.cost, Hud::Typeface::Figure, _topic.enabled ? GOLD_COLOR : LOCKED_TEXT_COLOR,
            {.padding = {.top = TOPIC_COST_TOP}});
  tree.End();
  if (!_topic.effect.empty())
  {
    (void)_canvas.Label(
      {.text = _topic.effect, .font = Hud::FontOf(Hud::Typeface::Detail), .lineUnits = TOPIC_LINE_UNITS, .flow = Neuron::TextFlow::Wrap},
      _topic.enabled ? NUMBERS_COLOR : LOCKED_TEXT_COLOR);
  }
  for (std::size_t need = 0; need < _topic.needs.size(); ++need)
  {
    (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = TOPIC_LINE_UNITS});
    (void)_canvas.Icon(Hud::SpriteOf(Hud::Sprite::Checkbox), NEED_BOX_UNITS, AMBER_COLOR,
                       {.widthUnits = NEED_TEXT_LEFT, .padding = {.top = NEED_BOX_TOP}});
    const std::string needs = need == 0 ? std::format("NEEDS{}{}", DOT, _topic.needs[need]) : std::format("+ {}", _topic.needs[need]);
    (void)_canvas.Label(FittedText(needs, Hud::Typeface::Label, TOPIC_LINE_UNITS), AMBER_COLOR, {.grow = 1.0f});
    tree.End();
  }
  if (!blocked)
  {
    (void)_canvas.Label(LineText(_topic.time, Hud::Typeface::Detail, TOPIC_LINE_UNITS), _topic.enabled ? LABEL_COLOR : LOCKED_TEXT_COLOR);
  }
  _canvas.EndBox();
}

// A topic not on the page, measured as its card is and not drawn, standing where it takes no room, so that every card is as
// tall as the most lines any topic needs and the cards keep their height from page to page.
void TopicMeasure(Neuron::LayoutTree& _tree, const Hud::TopicCard& _topic)
{
  (void)_tree.Begin(Neuron::LayoutKind::Column,
                    {.widthUnits = TopicWidth(),
                     .leastHeightUnits = TOPIC_LEAST_UNITS,
                     .padding = {.left = CARD_INSET, .top = TOPIC_LINES_TOP, .right = CARD_INSET, .bottom = TOPIC_FOOT_UNITS},
                     .alignX = Neuron::LayoutAlign::Stretch,
                     .anchor = Neuron::LayoutAnchor::TopLeft,
                     .heightGroup = TOPIC_CARDS});
  if (!_topic.effect.empty())
  {
    (void)_tree.AddText(
      {.text = _topic.effect, .font = Hud::FontOf(Hud::Typeface::Detail), .lineUnits = TOPIC_LINE_UNITS, .flow = Neuron::TextFlow::Wrap});
  }
  (void)_tree.Add({.heightUnits = TOPIC_LINE_UNITS * static_cast<float>(std::max<std::size_t>(1, _topic.needs.size()))});
  _tree.End();
}

// The research window: the Research Lab, where the page stands among the topics with the arrows that scroll a
// row at a time when they do not all fit, a page of topic cards and the others measured with them, and the queue.
void LayResearch(const WindowBench& _bench, const Hud::ResearchPanel& _panel, const WindowPlace& _place)
{
  LayWindow(
    _bench, Outpost::WindowKind::Research, _place, RESEARCH_WINDOW_WIDTH, QueueWindowBody(),
    [&](Neuron::Canvas& _canvas)
    {
      Neuron::LayoutTree& tree = _canvas.Tree();
      Hud::Layout& layout = *_bench.layout;
      Header(_canvas, layout, _panel.lab, _panel.ore, RESEARCH_WINDOW_WIDTH);
      (void)tree.Begin(Neuron::LayoutKind::Column, UnderHeader());
      // Where the page is among the topics, "TOPICS · 1-10 OF 21", with an ASCII hyphen (ADR-030; ADR-068 decision 1).
      const std::size_t pageEnd = std::min(_panel.firstTopic + Hud::TOPICS_SHOWN, _panel.topics.size());
      (void)tree.Begin(Neuron::LayoutKind::Row, {});
      (void)_canvas.Label(SectionText(_panel.topics.empty()
                                        ? std::string("TOPICS")
                                        : std::format("TOPICS{}{}-{} OF {}", DOT, _panel.firstTopic + 1, pageEnd, _panel.topics.size())),
                          LABEL_COLOR);
      if (_panel.topics.size() > Hud::TOPICS_SHOWN)
      {
        const bool back = _panel.firstTopic > 0;
        const bool on = _panel.firstTopic + Hud::TOPICS_SHOWN < _panel.topics.size();
        const std::uint32_t previous = back ? PressOf(layout, {.kind = Hud::ActionKind::PreviousTopics}) : Neuron::NO_PAYLOAD;
        const std::uint32_t next = on ? PressOf(layout, {.kind = Hud::ActionKind::NextTopics}) : Neuron::NO_PAYLOAD;
        (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = 4.0f, .anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = -6.0f}});
        (void)_canvas.SmallButton("previous", "<", back, previous);
        (void)_canvas.SmallButton("next", ">", on, next);
        tree.End();
      }
      tree.End();
      (void)tree.Begin(Neuron::LayoutKind::Grid,
                       {.gapUnits = CARD_SPACING, .alignX = Neuron::LayoutAlign::Stretch, .columns = Hud::TOPIC_COLUMNS});
      for (std::size_t topic = _panel.firstTopic; topic < pageEnd; ++topic)
        TopicCardOf(_canvas, layout, _panel.topics[topic], topic);
      // With no card on the page, a card's room still.
      if (_panel.firstTopic >= pageEnd)
        (void)tree.Add({.leastHeightUnits = TOPIC_LEAST_UNITS, .heightGroup = TOPIC_CARDS});
      for (std::size_t topic = 0; topic < _panel.topics.size(); ++topic)
      {
        if (topic < _panel.firstTopic || topic >= pageEnd)
          TopicMeasure(tree, _panel.topics[topic]);
      }
      tree.End();
      (void)tree.Add({.heightUnits = SECTION_GAP});
      QueueRows(_canvas, _panel.queue);
      tree.End();
    });
}

// The designer on the canvas (ADR-100), from the top-left of the window as the owner's mockup places it: the header under
// the title bar, then the name, the saved designs, a row for each slot, the bars and the damage cards beside them, and the
// footer, each under the one before.

// A label in the label face, tracked, on a line of _lineUnits.
Neuron::LayoutText TrackedText(std::string _text, float _lineUnits)
{
  return {
    .text = std::move(_text), .font = Hud::FontOf(Hud::Typeface::Label), .lineUnits = _lineUnits, .trackingUnits = LABEL_TRACKING_UNITS};
}

// The designer's header: the target Shipyard between the arrows that step it, in the title bar, and the window's name
// after it; the title; the Shipyard's queue over its slots and the ships it has built beside them; and the Ore.
void DesignerHeader(Neuron::Canvas& _canvas, Hud::Layout& _layout, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  BeginHeader(_canvas, DESIGNER_WIDTH);
  const std::uint32_t previous = _panel.hasShipyard ? PressOf(_layout, {.kind = Hud::ActionKind::PreviousShipyard}) : Neuron::NO_PAYLOAD;
  const std::uint32_t next = _panel.hasShipyard ? PressOf(_layout, {.kind = Hud::ActionKind::NextShipyard}) : Neuron::NO_PAYLOAD;
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = ARROW_GAP_UNITS,
                                             .anchor = Neuron::LayoutAnchor::TopLeft,
                                             .margin = {.left = DESIGNER_INSET, .top = ARROWS_IN_TITLE_TOP - Hud::TITLE_BAR_UNITS}});
  _canvas.Stepper("shipyard", TrackedText(_panel.shipyard, NAME_LINE_UNITS), HEADER_LABEL_COLOR, DESIGNER_SHIPYARD_ROOM,
                  WINDOW_HEADER_TOP - ARROWS_IN_TITLE_TOP, _panel.hasShipyard, previous, next);
  (void)_canvas.Label(TrackedText(std::format("{}DESIGNER", DOT.substr(1)), NAME_LINE_UNITS), HEADER_LABEL_COLOR,
                      {.padding = {.top = WINDOW_HEADER_TOP - ARROWS_IN_TITLE_TOP}});
  tree.End();
  (void)_canvas.Label(
    LineText("SHIP DESIGN", Hud::Typeface::Title, TITLE_LINE_UNITS), TEXT_COLOR,
    {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.left = DESIGNER_INSET, .top = HEADER_SUBJECT_TOP - Hud::TITLE_BAR_UNITS}});
  (void)_canvas.Label(
    TrackedText(QueueLabel(_panel.queued), NAME_LINE_UNITS), HEADER_LABEL_COLOR,
    {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.left = DESIGNER_SLOTS_LEFT, .top = DESIGNER_QUEUE_TOP - Hud::TITLE_BAR_UNITS}});
  (void)_canvas.Label(
    TrackedText(std::format("BUILT{}{}", DOT, _panel.built), NAME_LINE_UNITS), HEADER_LABEL_COLOR,
    {.anchor = Neuron::LayoutAnchor::TopRight,
     .margin = {.top = DESIGNER_BUILT_TOP - Hud::TITLE_BAR_UNITS, .right = DESIGNER_WIDTH - DESIGNER_SLOTS_LEFT + DESIGNER_BUILT_GAP}});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = DESIGNER_SLOT_SPACING,
                                             .anchor = Neuron::LayoutAnchor::TopLeft,
                                             .margin = {.left = DESIGNER_SLOTS_LEFT, .top = HEADER_SUBJECT_TOP - Hud::TITLE_BAR_UNITS}});
  for (std::size_t slot = 0; slot < Outpost::QUEUE_LIMIT; ++slot)
  {
    _canvas.Fill(tree.Add({.widthUnits = DESIGNER_SLOT_UNITS, .heightUnits = DESIGNER_SLOT_HEIGHT}),
                 slot < _panel.queued ? SLOT_FILLED_COLOR : FIELD_COLOR);
  }
  tree.End();
  OreBox(_canvas, _panel.ore);
  tree.End();
}

// The name, as the canvas's text field with its label before it and its count against the limit at its right, and Save at
// the row's right end. What the field did is the layout's, for GameClient to give the designer (ADR-017 decision 8).
void NameRow(Neuron::Canvas& _canvas, const WindowBench& _bench, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  Hud::Layout& layout = *_bench.layout;
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = NAME_ROW_HEIGHT, .gapUnits = SAVE_GAP});
  (void)tree.Begin(Neuron::LayoutKind::Stack, {.alignX = Neuron::LayoutAlign::Stretch, .grow = 1.0f});
  (void)_canvas.Label(TrackedText("NAME", NAME_LINE_UNITS), LABEL_COLOR,
                      {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.left = NAME_LABEL_LEFT, .top = NAME_LABEL_TOP}});
  const std::string count = std::format("{}/{}", _panel.name.size(), Outpost::DESIGN_NAME_LIMIT);
  std::string name = _panel.name;
  const Neuron::FieldEnd end =
    _canvas.TextField("name", name, Outpost::DESIGN_NAME_LIMIT, _panel.nameValid,
                      {.heightUnits = NAME_ROW_HEIGHT,
                       .padding = {.left = NAME_TEXT_LEFT - BUTTON_INSET,
                                   .top = (2.0f * NAME_TEXT_TOP) - (NAME_ROW_HEIGHT - NAME_LINE_UNITS),
                                   .right = _bench.metrics->Width(Hud::Typeface::Figure, count) + FIT_GAP_UNITS}});
  if (end != Neuron::FieldEnd::Typing || _bench.interaction->Focused() == _bench.interaction->IdOf("name"))
    layout.typedName = Hud::TypedName{.name = std::move(name), .end = end};
  (void)_canvas.Label(LineText(count, Hud::Typeface::Figure, FIGURE_LINE_UNITS), LABEL_COLOR,
                      {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = NAME_COUNT_TOP, .right = NAME_LABEL_LEFT}});
  tree.End();
  const Hud::Button& save = _panel.save;
  (void)_canvas.BeginCard(
    "save", {.face = save.enabled ? CARD_COLOR : FIELD_COLOR, .edge = save.selected ? QUEUE_EDGE_COLOR : EDGE_COLOR},
    save.enabled ? PressOf(layout, save.action) : Neuron::NO_PAYLOAD,
    {.widthUnits = SAVE_WIDTH, .heightUnits = NAME_ROW_HEIGHT, .padding = {.left = SAVE_TEXT_LEFT, .top = NAME_LABEL_TOP}});
  (void)_canvas.Label(TrackedText(save.label, NAME_LINE_UNITS), save.selected  ? QUEUE_EDGE_COLOR
                                                                : save.enabled ? TEXT_COLOR
                                                                               : DIM_TEXT_COLOR);
  _canvas.EndBox();
  tree.End();
}

// The saved designs, as many as fit from the first shown, each a chip with its name and its code, and the arrows that
// scroll them when they do not all fit. The label before them has the room it is set in at every step: the chips start a
// gap after it where their place is too near, and are as much narrower as keeps the third's end.
void SavedDesigns(Neuron::Canvas& _canvas, const WindowBench& _bench, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  Hud::Layout& layout = *_bench.layout;
  const float chipsLeft =
    std::max(CHIPS_LEFT - DESIGNER_INSET, _bench.metrics->Width(Hud::Typeface::Label, "SAVED", LABEL_TRACKING_UNITS) + CHIP_GAP);
  const float chipWidth =
    std::min(CHIP_WIDTH, (CHIPS_END - chipsLeft - (static_cast<float>(CHIPS_SHOWN - 1) * CHIP_GAP)) / static_cast<float>(CHIPS_SHOWN));
  (void)tree.Begin(Neuron::LayoutKind::Stack, {.heightUnits = CHIP_HEIGHT, .alignX = Neuron::LayoutAlign::Stretch});
  (void)_canvas.Label(TrackedText("SAVED", NAME_LINE_UNITS), LABEL_COLOR,
                      {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.top = CHIP_LABEL_TOP}});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = CHIP_GAP, .anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.left = chipsLeft}});
  for (std::size_t i = _panel.firstChip, column = 0; i < _panel.chips.size() && column < CHIPS_SHOWN; ++i, ++column)
  {
    const Hud::DesignChip& chip = _panel.chips[i];
    (void)_canvas.BeginCard(std::format("chip {}", i),
                            {.face = chip.shown ? PICKED_COLOR : CARD_COLOR, .edge = chip.shown ? PICKED_EDGE_COLOR : EDGE_COLOR},
                            PressOf(layout, chip.action),
                            {.widthUnits = chipWidth,
                             .heightUnits = CHIP_HEIGHT,
                             .padding = {.left = CHIP_INSET, .top = CHIP_TEXT_TOP, .right = CHIP_INSET},
                             .alignX = Neuron::LayoutAlign::Stretch});
    (void)tree.Begin(Neuron::LayoutKind::Row, {});
    (void)_canvas.Label(FittedText(chip.name, Hud::Typeface::Label, NAME_LINE_UNITS), TEXT_COLOR,
                        {.padding = {.right = FIT_GAP_UNITS}, .grow = 1.0f});
    (void)_canvas.Label(LineText(chip.code, Hud::Typeface::Detail, NAME_LINE_UNITS), chip.shown ? ACCENT_COLOR : CODE_COLOR,
                        {.padding = {.top = CHIP_LABEL_TOP - CHIP_TEXT_TOP}});
    tree.End();
    _canvas.EndBox();
  }
  tree.End();
  if (_panel.chips.size() > CHIPS_SHOWN)
  {
    const bool back = _panel.firstChip > 0;
    const bool on = _panel.firstChip + CHIPS_SHOWN < _panel.chips.size();
    const std::uint32_t previous = back ? PressOf(layout, {.kind = Hud::ActionKind::PreviousDesigns}) : Neuron::NO_PAYLOAD;
    const std::uint32_t next = on ? PressOf(layout, {.kind = Hud::ActionKind::NextDesigns}) : Neuron::NO_PAYLOAD;
    (void)tree.Begin(Neuron::LayoutKind::Row,
                     {.gapUnits = CHIP_ARROW_GAP, .anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = LINE_UNITS}});
    (void)_canvas.SmallButton("previous designs", "<", back, previous);
    (void)_canvas.SmallButton("next designs", ">", on, next);
    tree.End();
  }
  tree.End();
}

// A part's card (ADR-017 decision 7), a composite of the game's, _heightUnits tall: its name and cost, its numbers, any
// note, and while research has yet to unlock it, the topic that does at its foot after a lock's box. A locked card is
// hatched and dimmed, takes no press, and previews what its topic buys under the pointer.
void PartCardOf(Neuron::Canvas& _canvas, Hud::Layout& _layout, const Hud::PartCard& _card, std::string_view _id, float _heightUnits)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  const bool locked = _card.IsLocked();
  (void)_canvas.BeginCard(_id,
                          {.face = _card.picked ? PICKED_COLOR
                                   : locked     ? LOCKED_COLOR
                                                : CARD_COLOR,
                           .hatch = locked ? std::optional(LOCKED_HATCH_COLOR) : std::nullopt,
                           .edge = _card.picked ? PICKED_EDGE_COLOR : EDGE_COLOR},
                          locked ? PreviewOf(_layout, _card.action) : PressOf(_layout, _card.action),
                          {.heightUnits = _heightUnits,
                           .padding = {.left = CARD_TEXT_INSET, .top = CARD_NAME_TOP, .right = CARD_TEXT_INSET},
                           .alignX = Neuron::LayoutAlign::Stretch},
                          locked ? Neuron::CardPress::Previewed : Neuron::CardPress::Pressed);
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = CARD_NUMBERS_TOP - CARD_NAME_TOP});
  (void)_canvas.Label(FittedText(_card.name, Hud::Typeface::Name, NAME_LINE_UNITS), locked ? LOCKED_TEXT_COLOR : TEXT_COLOR,
                      {.padding = {.right = FIT_GAP_UNITS}, .grow = 1.0f});
  OreAmount(_canvas, _card.cost, Hud::Typeface::Figure, locked ? LOCKED_TEXT_COLOR : GOLD_COLOR,
            {.padding = {.top = CARD_COST_TOP - CARD_NAME_TOP}});
  tree.End();
  const DirectX::XMFLOAT4& numbersColor = locked ? LOCKED_TEXT_COLOR : NUMBERS_COLOR;
  (void)_canvas.Label(FittedText(_card.numbers, Hud::Typeface::Detail, CARD_NOTE_TOP - CARD_NUMBERS_TOP), numbersColor);
  if (!_card.note.empty())
    (void)_canvas.Label(FittedText(_card.note, Hud::Typeface::Detail, CARD_NOTE_ROOM), numbersColor);
  if (locked)
  {
    (void)tree.Begin(Neuron::LayoutKind::Row, {.widthUnits = CARD_WIDTH - (2.0f * CARD_TEXT_INSET),
                                               .anchor = Neuron::LayoutAnchor::TopLeft,
                                               .margin = {.top = _heightUnits - CARD_LOCK_FROM_FOOT - CARD_NAME_TOP}});
    (void)_canvas.Icon(Hud::SpriteOf(Hud::Sprite::Checkbox), CARD_LOCK_BOX_UNITS, AMBER_COLOR,
                       {.widthUnits = CARD_LOCK_TEXT_LEFT - CARD_TEXT_INSET, .padding = {.top = CARD_LOCK_BOX_TOP}});
    (void)_canvas.Label(FittedText(_card.lockedBy, Hud::Typeface::Label, CARD_LOCK_FROM_FOOT), AMBER_COLOR, {.grow = 1.0f});
    tree.End();
  }
  _canvas.EndBox();
}

// A row for each slot: its label and pick, 18 units apart so that the label's line clears the pick's at 1280x720's
// rounding, and centered on a line of cards together (ADR-062); a divider; and its cards, three to a line, every one as
// tall as the tallest, so that a row whose cards have notes is taller (ADR-061). The rows scroll, in a view that shows a
// line of the tallest cards at least, with its lane in the room right of the cards; nothing above or below them does
// (ADR-100 decision 7).
void SlotRows(Neuron::Canvas& _canvas, Hud::Layout& _layout, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  float tallest = CARD_HEIGHT;
  for (const Hud::SlotRow& row : _panel.slots)
    tallest = std::max(tallest, CardHeightOf(row));
  (void)_canvas.BeginScroll("parts", {.widthUnits = DESIGNER_WIDTH - DESIGNER_INSET, .leastHeightUnits = tallest, .grow = 1.0f});
  (void)tree.Begin(Neuron::LayoutKind::Column, {.gapUnits = SLOT_GAP, .alignX = Neuron::LayoutAlign::Stretch});
  for (std::size_t slot = 0; slot < _panel.slots.size(); ++slot)
  {
    const Hud::SlotRow& row = _panel.slots[slot];
    (void)tree.Begin(Neuron::LayoutKind::Row, {.alignY = Neuron::LayoutAlign::Stretch});
    (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = SLOT_DIVIDER_LEFT - DESIGNER_INSET,
                                                  .padding = {.top = SLOT_LABEL_TOP, .right = FIT_GAP_UNITS},
                                                  .alignX = Neuron::LayoutAlign::Stretch});
    (void)_canvas.Label(TrackedText(row.label, SLOT_PICK_TOP - SLOT_LABEL_TOP), LABEL_COLOR);
    (void)_canvas.Label(FittedText(row.picked, Hud::Typeface::Name, NAME_LINE_UNITS), ACCENT_COLOR);
    tree.End();
    (void)tree.Begin(Neuron::LayoutKind::Column,
                     {.widthUnits = CARDS_LEFT - SLOT_DIVIDER_LEFT, .padding = {.top = SLOT_DIVIDER_INSET, .bottom = SLOT_DIVIDER_INSET}});
    _canvas.Fill(tree.Add({.widthUnits = LINE_UNITS, .grow = 1.0f}), EDGE_COLOR);
    tree.End();
    (void)tree.Begin(Neuron::LayoutKind::Grid,
                     {.widthUnits = CARDS_WIDTH, .gapUnits = CARD_GAP, .alignX = Neuron::LayoutAlign::Stretch, .columns = CARDS_PER_LINE});
    for (std::size_t card = 0; card < row.cards.size(); ++card)
      PartCardOf(_canvas, _layout, row.cards[card], std::format("part {} {}", slot, card), CardHeightOf(row));
    tree.End();
    tree.End();
  }
  tree.End();
  _canvas.EndScroll();
}

// Performance: each number's bar against the best in the game, in room for the six every design has; while a part is
// hovered, the change it would make, the part of the bar it adds or takes away in the change's color, under what both share.
void Performance(Neuron::Canvas& _canvas, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  constexpr float WIDTH = DAMAGE_LEFT - DESIGNER_INSET;
  constexpr float BAR_FROM = BAR_LEFT - DESIGNER_INSET;
  (void)tree.Begin(Neuron::LayoutKind::Column,
                   {.widthUnits = WIDTH,
                    .leastHeightUnits = SECTION_LABEL_HEIGHT + (static_cast<float>(BAR_ROWS_OF_EVERY_DESIGN) * BAR_ROW_HEIGHT),
                    .alignX = Neuron::LayoutAlign::Stretch});
  (void)_canvas.Label(TrackedText("PERFORMANCE", SECTION_LABEL_HEIGHT), LABEL_COLOR);
  for (const Hud::StatBar& bar : _panel.bars)
  {
    (void)tree.Begin(Neuron::LayoutKind::Stack, {.heightUnits = BAR_ROW_HEIGHT});
    (void)_canvas.Label(LineText(bar.label, Hud::Typeface::Label, BAR_ROW_HEIGHT), ROW_LABEL_COLOR,
                        {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.top = BAR_LABEL_TOP}});
    const auto barAt = [&tree](float _share)
    {
      return tree.Add({.widthUnits = BAR_WIDTH * _share,
                       .heightUnits = BAR_HEIGHT,
                       .anchor = Neuron::LayoutAnchor::TopLeft,
                       .margin = {.left = BAR_FROM, .top = BAR_TOP}});
    };
    _canvas.Fill(barAt(1.0f), BAR_TRACK_COLOR);
    if (bar.previewShare.has_value() && bar.change != Hud::Change::Same)
    {
      _canvas.Fill(barAt(std::max(bar.share, *bar.previewShare)), ChangeColor(bar.change, BAR_FILL_COLOR));
      _canvas.Fill(barAt(std::min(bar.share, *bar.previewShare)), BAR_FILL_COLOR);
    }
    else
      _canvas.Fill(barAt(bar.share), BAR_FILL_COLOR);
    (void)_canvas.Label(LineText(bar.previewShare.has_value() ? bar.previewValue : bar.value, Hud::Typeface::Figure, FIGURE_LINE_UNITS),
                        ChangeColor(bar.change, TEXT_COLOR),
                        {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.right = DAMAGE_LEFT - BAR_VALUE_RIGHT}});
    (void)_canvas.Label(
      LineText(bar.unit, Hud::Typeface::Label, BAR_ROW_HEIGHT), LABEL_COLOR,
      {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.left = BAR_VALUE_RIGHT + BAR_UNIT_GAP - DESIGNER_INSET, .top = BAR_LABEL_TOP}});
    tree.End();
  }
  tree.End();
}

// Damage per second after armor against each hull, a card for each, and the hint under them.
void Damage(Neuron::Canvas& _canvas, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = DAMAGE_WIDTH, .alignX = Neuron::LayoutAlign::Stretch});
  (void)_canvas.Label(TrackedText(std::format("DAMAGE / S AFTER ARMOR{}VS A FORMATION", DOT), SECTION_LABEL_HEIGHT), LABEL_COLOR);
  const std::size_t columns = std::max<std::size_t>(1, _panel.damage.size());
  const auto cards = static_cast<float>(columns);
  const float cardWidth = (DAMAGE_WIDTH - ((cards - 1.0f) * DAMAGE_CARD_GAP)) / cards;
  const float inner = cardWidth - (2.0f * DAMAGE_INSET);
  (void)tree.Begin(
    Neuron::LayoutKind::Grid,
    {.leastHeightUnits = DAMAGE_CARD_HEIGHT, .gapUnits = DAMAGE_CARD_GAP, .alignX = Neuron::LayoutAlign::Stretch, .columns = columns});
  for (const Hud::DamageCard& card : _panel.damage)
  {
    (void)_canvas.BeginBox({.face = FIELD_COLOR, .edge = EDGE_COLOR},
                           {.heightUnits = DAMAGE_CARD_HEIGHT,
                            .padding = {.left = DAMAGE_INSET, .top = DAMAGE_HULL_TOP, .right = DAMAGE_RIGHT_INSET},
                            .alignX = Neuron::LayoutAlign::Stretch});
    const DirectX::XMFLOAT4 changed = ChangeColor(card.change, TEXT_COLOR);
    (void)tree.Begin(Neuron::LayoutKind::Row, {});
    (void)_canvas.Label(FittedText(card.hull, Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR,
                        {.padding = {.right = FIT_GAP_UNITS}, .grow = 1.0f});
    (void)_canvas.Label(LineText(card.armor, Hud::Typeface::Label, NAME_LINE_UNITS), LABEL_COLOR,
                        {.padding = {.top = DAMAGE_ARMOR_TOP - DAMAGE_HULL_TOP}});
    tree.End();
    // Clear of the hull's line, which the name face sets about 19 units tall, at every scale's rounding.
    (void)_canvas.Label(LineText(card.perShip, Hud::Typeface::LargeFigure, TITLE_LINE_UNITS), changed,
                        {.anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.top = DAMAGE_FIGURE_TOP - DAMAGE_HULL_TOP}});
    // How many ships a splash reaches, at the card's right above the change.
    if (!card.reach.empty())
    {
      (void)_canvas.Label(LineText(card.reach, Hud::Typeface::Label, NAME_LINE_UNITS), LABEL_COLOR,
                          {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = DAMAGE_REACH_TOP - DAMAGE_HULL_TOP}});
    }
    // A hovered part's change, at the card's right on the large figure's line.
    if (!card.perShipChange.empty())
    {
      (void)_canvas.Label(LineText(card.perShipChange, Hud::Typeface::Figure, FIGURE_LINE_UNITS), changed,
                          {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = DAMAGE_CHANGE_TOP - DAMAGE_HULL_TOP}});
    }
    const auto barAt = [&tree, inner](float _share)
    {
      return tree.Add({.widthUnits = inner * _share,
                       .heightUnits = DAMAGE_BAR_HEIGHT,
                       .anchor = Neuron::LayoutAnchor::TopLeft,
                       .margin = {.top = DAMAGE_BAR_TOP - DAMAGE_HULL_TOP}});
    };
    _canvas.Fill(barAt(1.0f), BAR_TRACK_COLOR);
    _canvas.Fill(barAt(card.share), RatingColor(card.rating));
    (void)_canvas.Label(
      FittedText(card.perOre, Hud::Typeface::Detail, FIGURE_LINE_UNITS), SOFT_COLOR,
      {.mostWidthUnits = inner, .anchor = Neuron::LayoutAnchor::TopLeft, .margin = {.top = DAMAGE_PER_ORE_TOP - DAMAGE_HULL_TOP}});
    _canvas.EndBox();
  }
  tree.End();
  (void)_canvas.Label(FittedText(_panel.hint, Hud::Typeface::Detail, FIGURE_LINE_UNITS), LABEL_COLOR,
                      {.padding = {.top = DAMAGE_HINT_GAP}});
  tree.End();
}

// A card of the footer, _widthUnits wide, with _text at _left and _top in it: its face a card's while it can be pressed and
// a field's while not, and its edge the windows'.
void FooterCard(Neuron::Canvas& _canvas, Hud::Layout& _layout, std::string_view _id, Neuron::LayoutText _text, bool _enabled,
                const Hud::Action& _action, float _widthUnits, float _left, float _top)
{
  (void)_canvas.BeginCard(_id, {.face = _enabled ? CARD_COLOR : FIELD_COLOR, .edge = EDGE_COLOR},
                          _enabled ? PressOf(_layout, _action) : Neuron::NO_PAYLOAD,
                          {.widthUnits = _widthUnits, .heightUnits = FOOTER_HEIGHT, .padding = {.left = _left, .top = _top}});
  (void)_canvas.Label(std::move(_text), _enabled ? TEXT_COLOR : DIM_TEXT_COLOR);
  _canvas.EndBox();
}

// The bottom row: Rename; the design's retreat, RETREAT over a row of three, its setting lit, each a press that sets it
// (ADR-088 decision 3); the stepper of how many ships Queue asks for; and Queue, hatched, with the build time of one ship
// and the cost of all.
void Footer(Neuron::Canvas& _canvas, Hud::Layout& _layout, const Hud::DesignerPanel& _panel)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = FOOTER_HEIGHT});
  FooterCard(_canvas, _layout, "rename", TrackedText(_panel.rename.label, NAME_LINE_UNITS), _panel.rename.enabled, _panel.rename.action,
             RENAME_WIDTH, RENAME_TEXT_LEFT, RENAME_TEXT_TOP);
  (void)tree.Add({.widthUnits = RETREAT_LEFT - DESIGNER_INSET - RENAME_WIDTH});
  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = RETREAT_WIDTH});
  (void)_canvas.Label(TrackedText("RETREAT", RETREAT_CELLS_TOP), ROW_LABEL_COLOR);
  std::array<Neuron::LayoutText, RETREAT_CHOICES.size()> words;
  std::array<std::uint32_t, RETREAT_CHOICES.size()> payloads{};
  for (std::size_t i = 0; i < RETREAT_CHOICES.size(); ++i)
  {
    words[i] =
      TrackedText(Capitals(RetreatCell(RETREAT_CHOICES[i])), std::round(Hud::FaceUnits(Hud::Typeface::Label) * RETREAT_LINE_SHARE));
    payloads[i] = PressOf(_layout, {.kind = Hud::ActionKind::DesignRetreat, .retreat = RETREAT_CHOICES[i]});
  }
  const auto picked = std::ranges::find(RETREAT_CHOICES, _panel.retreat);
  _canvas.Cells("retreat", words, payloads,
                picked == RETREAT_CHOICES.end() ? std::nullopt : std::optional(static_cast<std::size_t>(picked - RETREAT_CHOICES.begin())),
                FOOTER_HEIGHT - RETREAT_CELLS_TOP, {.widthUnits = RETREAT_WIDTH, .gapUnits = RETREAT_CELL_GAP_UNITS});
  tree.End();
  (void)tree.Add({.widthUnits = STEPPER_LEFT - RETREAT_LEFT - RETREAT_WIDTH});
  FooterCard(_canvas, _layout, "fewer", LineText("-", Hud::Typeface::Name, NAME_LINE_UNITS), _panel.canFewer,
             {.kind = Hud::ActionKind::FewerShips}, STEP_WIDTH, STEP_TEXT_LEFT, FOOTER_TEXT_TOP);
  (void)_canvas.BeginBox(
    {.face = FIELD_COLOR, .edge = EDGE_COLOR},
    {.widthUnits = COUNT_WIDTH, .heightUnits = FOOTER_HEIGHT, .padding = {.left = COUNT_TEXT_LEFT, .top = FOOTER_TEXT_TOP}});
  (void)_canvas.Label(LineText(std::format("{}{}", TIMES, _panel.count), Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR);
  _canvas.EndBox();
  FooterCard(_canvas, _layout, "more", LineText("+", Hud::Typeface::Name, NAME_LINE_UNITS), _panel.canMore,
             {.kind = Hud::ActionKind::MoreShips}, STEP_WIDTH, STEP_TEXT_LEFT, FOOTER_TEXT_TOP);
  (void)tree.Add({.widthUnits = QUEUE_LEFT - STEPPER_LEFT - (2.0f * STEP_WIDTH) - COUNT_WIDTH});
  const Hud::Button& queue = _panel.queue;
  const DirectX::XMFLOAT4& color = queue.enabled ? QUEUE_TEXT_COLOR : DIM_TEXT_COLOR;
  (void)_canvas.BeginCard("queue",
                          {.face = queue.enabled ? QUEUE_COLOR : FIELD_COLOR,
                           .hatch = queue.enabled ? std::optional(QUEUE_HATCH_COLOR) : std::nullopt,
                           .edge = queue.enabled ? QUEUE_EDGE_COLOR : EDGE_COLOR},
                          queue.enabled ? PressOf(_layout, queue.action) : Neuron::NO_PAYLOAD,
                          {.widthUnits = DESIGNER_WIDTH - DESIGNER_INSET - QUEUE_LEFT,
                           .heightUnits = FOOTER_HEIGHT,
                           .padding = {.left = QUEUE_TEXT_LEFT, .top = QUEUE_LABEL_TOP, .right = QUEUE_RIGHT_INSET},
                           .alignX = Neuron::LayoutAlign::Stretch});
  (void)tree.Begin(Neuron::LayoutKind::Row, {});
  (void)tree.Begin(Neuron::LayoutKind::Column, {.grow = 1.0f});
  (void)_canvas.Label({.text = queue.label,
                       .font = Hud::FontOf(Hud::Typeface::Title),
                       .lineUnits = QUEUE_DETAIL_TOP - QUEUE_LABEL_TOP,
                       .trackingUnits = LABEL_TRACKING_UNITS},
                      color);
  (void)_canvas.Label(LineText(_panel.queueDetail, Hud::Typeface::Detail, FIGURE_LINE_UNITS), color);
  tree.End();
  OreAmount(_canvas, _panel.queueCost, Hud::Typeface::Title, queue.enabled ? GOLD_COLOR : DIM_TEXT_COLOR,
            {.padding = {.top = QUEUE_COST_TOP - QUEUE_LABEL_TOP}});
  tree.End();
  _canvas.EndBox();
  tree.End();
}

// The designer's window (ADR-017 decision 7), laid out as the owner's mockup, Docs/Mockups/ShipDesigner.png: the header
// across the window, and under it, inset from its sides, the name, the saved designs, the slots' rows, a line, the bars
// beside the damage cards, and the footer.
void LayDesigner(const WindowBench& _bench, const Hud::DesignerPanel& _panel, const WindowPlace& _place)
{
  LayWindow(_bench, Outpost::WindowKind::Designer, _place, DESIGNER_WIDTH, QueueWindowBody(),
            [&](Neuron::Canvas& _canvas)
            {
              Neuron::LayoutTree& tree = _canvas.Tree();
              Hud::Layout& layout = *_bench.layout;
              DesignerHeader(_canvas, layout, _panel);
              (void)tree.Begin(
                Neuron::LayoutKind::Column,
                {.padding = {.left = DESIGNER_INSET, .top = NAME_ROW_TOP - DESIGNER_HEADER, .right = DESIGNER_INSET, .bottom = SECTION_GAP},
                 .alignX = Neuron::LayoutAlign::Stretch,
                 .grow = 1.0f});
              NameRow(_canvas, _bench, _panel);
              (void)tree.Add({.heightUnits = CHIP_ROW_TOP - NAME_ROW_TOP - NAME_ROW_HEIGHT});
              SavedDesigns(_canvas, _bench, _panel);
              (void)tree.Add({.heightUnits = SLOTS_TOP - CHIP_ROW_TOP - CHIP_HEIGHT});
              SlotRows(_canvas, layout, _panel);
              (void)tree.Begin(Neuron::LayoutKind::Stack, {.heightUnits = SECTION_GAP});
              _canvas.Fill(tree.Add({.widthUnits = DESIGNER_WIDTH - (2.0f * DESIGNER_INSET),
                                     .heightUnits = LINE_UNITS,
                                     .anchor = Neuron::LayoutAnchor::TopLeft,
                                     .margin = {.top = SECTION_GAP / 2.0f}}),
                           EDGE_COLOR);
              tree.End();
              (void)tree.Begin(Neuron::LayoutKind::Row, {});
              Performance(_canvas, _panel);
              Damage(_canvas, _panel);
              tree.End();
              (void)tree.Add({.heightUnits = PADDING});
              Footer(_canvas, layout, _panel);
              tree.End();
            });
}

// The minimap's marks, fog and view, which the game draws into its square, _layout's minimap, once the canvas has placed
// it (ADR-100 decision 7): the sectors' washes, the marks, the fog, the sectors' borders and outlines, what the player only
// remembers, the alerts' places and the camera's view.
void DrawMap(Hud::Layout& _layout, const Hud::Content& _content, std::span<const Outpost::PlanePosition> _view, float _scale)
{
  _layout.mapSizeMeters = _content.mapSizeMeters;
  _layout.panels.push_back(_layout.minimap);

  const float pixelsPerMeter = _layout.minimap.width / _content.mapSizeMeters;
  // A sector's square on the minimap, in pixels.
  const auto sectorRect = [&_layout](const Hud::SectorMark& _sector, DirectX::XMFLOAT4 _color, Hud::Fill _fill)
  {
    const DirectX::XMFLOAT2 low = _layout.MinimapPixelOf({.xMeters = _sector.minXMeters, .zMeters = _sector.maxZMeters});
    const DirectX::XMFLOAT2 high = _layout.MinimapPixelOf({.xMeters = _sector.maxXMeters, .zMeters = _sector.minZMeters});
    return Hud::Rect{low.x, low.y, high.x - low.x, high.y - low.y, _color, _fill};
  };
  for (const Hud::SectorMark& sector : _content.sectors)
  {
    if (sector.side != Hud::Side::Neutral)
      _layout.panels.push_back(sectorRect(sector, SideColorOf(_content, sector.side, sector.holder, SECTOR_WASH_ALPHA), Hud::Fill::Solid));
  }
  // The neutral marks first, so that a rig's shows over its asteroid's.
  std::vector<const Hud::Mark*> marks;
  marks.reserve(_content.marks.size());
  for (const bool sided : {false, true})
  {
    for (const Hud::Mark& mark : _content.marks)
    {
      // A memory is drawn over the fog, below.
      if ((mark.side != Hud::Side::Neutral) == sided && !mark.remembered)
        marks.push_back(&mark);
    }
  }
  for (const Hud::Mark* marked : marks)
  {
    const Hud::Mark& mark = *marked;
    const float smallest = (mark.kind == Outpost::EntityKind::Ship       ? SHIP_MARK_UNITS
                            : mark.kind == Outpost::EntityKind::Asteroid ? ORE_MARK_UNITS
                                                                         : STRUCTURE_MARK_UNITS) *
                           _scale;
    const float side = std::max(smallest, 2.0f * mark.radiusMeters * pixelsPerMeter);
    const DirectX::XMFLOAT2 at = _layout.MinimapPixelOf(mark.position);
    const DirectX::XMFLOAT4 color = mark.dry                                          ? DRY_COLOR
                                    : mark.side != Hud::Side::Neutral                 ? SideColorOf(_content, mark.side, mark.owner)
                                    : mark.kind == Outpost::EntityKind::Derelict      ? DERELICT_COLOR
                                    : mark.kind == Outpost::EntityKind::AsteroidField ? ASTEROID_FIELD_COLOR
                                    : mark.kind == Outpost::EntityKind::Asteroid      ? ORE_ASTEROID_COLOR
                                                                                      : NEUTRAL_COLOR;
    const float markLeft = at.x - (side / 2.0f);
    const float markTop = at.y - (side / 2.0f);
    if (mark.kind == Outpost::EntityKind::Asteroid && mark.side == Hud::Side::Neutral)
    {
      // An ore asteroid, dry or not, as an outline, its top and bottom across the whole square and its sides between them.
      const float line = ORE_OUTLINE_UNITS * _scale;
      _layout.panels.push_back({markLeft, markTop, side, line, color});
      _layout.panels.push_back({markLeft, markTop + side - line, side, line, color});
      _layout.panels.push_back({markLeft, markTop + line, line, side - (2.0f * line), color});
      _layout.panels.push_back({markLeft + side - line, markTop + line, line, side - (2.0f * line), color});
    }
    else
      _layout.panels.push_back({markLeft, markTop, side, side, color});
  }

  // The fog over the marks (ADR-024), filled by the client from the fog's texture across the whole map (ADR-052).
  if (_content.fog)
  {
    _layout.panels.push_back({_layout.minimap.left,
                              _layout.minimap.top,
                              _layout.minimap.width,
                              _layout.minimap.height,
                              {0.0f, 0.0f, 0.0f, 1.0f},
                              Hud::Fill::Image});
  }

  // Over the fog, since both sides know who holds what and where the pirates are: every sector's border, then each held
  // or guarded sector's outline, dashed while it is cut off, and stripes over a suppressed one.
  const float sectorLine = SECTOR_LINE_UNITS * _scale;
  const auto outline = [&_layout, sectorLine](const Hud::Rect& _area)
  {
    _layout.panels.push_back({_area.left, _area.top, _area.width, sectorLine, _area.color});
    _layout.panels.push_back({_area.left, _area.top + _area.height - sectorLine, _area.width, sectorLine, _area.color});
    _layout.panels.push_back({_area.left, _area.top, sectorLine, _area.height, _area.color});
    _layout.panels.push_back({_area.left + _area.width - sectorLine, _area.top, sectorLine, _area.height, _area.color});
  };
  // Dashes along each side, each side starting with one.
  const auto dashedOutline = [&_layout, sectorLine, _scale](const Hud::Rect& _area)
  {
    const float dash = CUT_OFF_DASH_UNITS * _scale;
    const float period = (CUT_OFF_DASH_UNITS + CUT_OFF_GAP_UNITS) * _scale;
    for (int index = 0; static_cast<float>(index) * period < _area.width; ++index)
    {
      const float along = static_cast<float>(index) * period;
      const float length = std::min(dash, _area.width - along);
      _layout.panels.push_back({_area.left + along, _area.top, length, sectorLine, _area.color});
      _layout.panels.push_back({_area.left + along, _area.top + _area.height - sectorLine, length, sectorLine, _area.color});
    }
    for (int index = 0; static_cast<float>(index) * period < _area.height; ++index)
    {
      const float along = static_cast<float>(index) * period;
      const float length = std::min(dash, _area.height - along);
      _layout.panels.push_back({_area.left, _area.top + along, sectorLine, length, _area.color});
      _layout.panels.push_back({_area.left + _area.width - sectorLine, _area.top + along, sectorLine, length, _area.color});
    }
  };
  for (const Hud::SectorMark& sector : _content.sectors)
    outline(sectorRect(sector, LATTICE_COLOR, Hud::Fill::Solid));
  for (const Hud::SectorMark& sector : _content.sectors)
  {
    if (sector.side == Hud::Side::Neutral)
      continue;
    const Hud::Rect area = sectorRect(sector, SideColorOf(_content, sector.side, sector.holder, SECTOR_OUTLINE_ALPHA), Hud::Fill::Solid);
    if (sector.cutOff)
      dashedOutline(area);
    else
      outline(area);
    if (sector.suppressed)
      _layout.panels.push_back(
        sectorRect(sector, SideColorOf(_content, sector.side, sector.holder, SECTOR_HATCH_ALPHA), Hud::Fill::Hatched));
  }

  // While the player places a Relay, each sector it could claim now, outlined in its own color inside the border, clear
  // of the outline of its held sector beside it.
  const float claimableInset = CLAIMABLE_INSET_UNITS * _scale;
  const DirectX::XMFLOAT4 ownColor = SideColorOf(_content, Hud::Side::Own, _content.player);
  for (const Hud::SectorMark& sector : _content.sectors)
  {
    if (!sector.claimable)
      continue;
    const Hud::Rect area = sectorRect(sector, ownColor, Hud::Fill::Solid);
    outline({area.left + claimableInset, area.top + claimableInset, area.width - (2.0f * claimableInset),
             area.height - (2.0f * claimableInset), ownColor});
  }

  // What the player only remembers, as a cross in its side's color, over the fog that would dim it twice.
  for (const Hud::Mark& mark : _content.marks)
  {
    if (!mark.remembered)
      continue;
    const DirectX::XMFLOAT2 at = _layout.MinimapPixelOf(mark.position);
    const float across = MEMORY_MARK_UNITS * _scale;
    const float arm = MEMORY_ARM_UNITS * _scale;
    const DirectX::XMFLOAT4 color = mark.side == Hud::Side::Neutral ? DERELICT_COLOR : SideColorOf(_content, mark.side, mark.owner);
    _layout.panels.push_back({at.x - (across / 2.0f), at.y - (arm / 2.0f), across, arm, color});
    _layout.panels.push_back({at.x - (arm / 2.0f), at.y - (across / 2.0f), arm, across, color});
  }

  // Each alert's place, as an outlined square over everything else (ADR-059); a refusal has none (ADR-094).
  for (const auto& [text, position] : _content.alerts)
  {
    if (!position.has_value())
      continue;
    const DirectX::XMFLOAT2 at = _layout.MinimapPixelOf(*position);
    const float half = ALERT_MARK_UNITS * _scale / 2.0f;
    const float side = 2.0f * half;
    _layout.panels.push_back({at.x - half, at.y - half, side, sectorLine, WARNING_COLOR});
    _layout.panels.push_back({at.x - half, at.y + half - sectorLine, side, sectorLine, WARNING_COLOR});
    _layout.panels.push_back({at.x - half, at.y - half, sectorLine, side, WARNING_COLOR});
    _layout.panels.push_back({at.x + half - sectorLine, at.y - half, sectorLine, side, WARNING_COLOR});
  }

  // The view's outline: the ground the screen's four corners show, joined in turn, so that it shows the near side and the
  // far and turns with the camera (ADR-015 decision 4). What runs past the map is cut at the minimap's edge.
  for (std::size_t corner = 0; corner < _view.size(); ++corner)
  {
    if (const std::optional<std::pair<DirectX::XMFLOAT2, DirectX::XMFLOAT2>> inside = Neuron::ClipToRect(
          _layout.MinimapPixelAt(_view[corner]), _layout.MinimapPixelAt(_view[(corner + 1) % _view.size()]), _layout.minimap))
    {
      _layout.lines.push_back(
        {.segment = {.from = inside->first, .to = inside->second, .widthPixels = VIEW_LINE_UNITS * _scale}, .color = VIEW_COLOR});
    }
  }
}

// The HUD on the canvas (Game §interface.hud; ADR-100 decision 7): each panel anchored where it always stands (ADR-043
// decision 1), what it holds at its places in it, in reference units from the panel's top-left corner, and the panels in
// the order they are drawn: the column under the Ore, the hint, the tag, the banner, the selection and its buttons, and
// the minimap.

// What the HUD is built from: the canvas, the layout whose actions its presses name, the measure, the action under the
// pointer, which lights the status line and the ship's bar that take it (ADR-088 decision 1), and the screen's size.
struct HudBench
{
  Neuron::Canvas* canvas = nullptr;
  // The canvas of what stands over the windows, the hint and the banner (ADR-015 decision 4).
  Neuron::Canvas* front = nullptr;
  Hud::Layout* layout = nullptr;
  const Hud::TextMetrics* metrics = nullptr;
  std::optional<Hud::Action> hovered;
  float screenWidthUnits = 0.0f;
  float screenHeightUnits = 0.0f;

  [[nodiscard]] bool IsHovered(const Hud::Action& _action) const noexcept
  {
    return hovered.has_value() && *hovered == _action;
  }
};

// A button of the HUD as the canvas draws it, lit as under the pointer while it is enabled and its action is the one under
// the pointer, with every other that takes it (ADR-088 decision 1).
Neuron::ButtonContent ContentOfHud(const HudBench& _bench, const Hud::Button& _button)
{
  Neuron::ButtonContent content = ContentOf(*_bench.layout, _button);
  content.pointed = _button.enabled && _bench.IsHovered(_button.action);
  return content;
}

// A place in a panel: _style anchored at the panel's top-left corner, _leftUnits and _topUnits in.
Neuron::LayoutStyle At(float _leftUnits, float _topUnits, Neuron::LayoutStyle _style = {})
{
  _style.anchor = Neuron::LayoutAnchor::TopLeft;
  _style.margin = {.left = _leftUnits, .top = _topUnits};
  return _style;
}

// A place in a panel whose right edge stands _rightUnits in from the panel's right, _topUnits down.
Neuron::LayoutStyle EndingAt(float _rightUnits, float _topUnits)
{
  return {.anchor = Neuron::LayoutAnchor::TopRight, .margin = {.top = _topUnits, .right = _rightUnits}};
}

// A panel of the HUD (ADR-043 decision 1), _widthUnits by _heightUnits with its top-left corner at (_leftUnits, _topUnits),
// which takes what is built until the canvas's EndPanel.
void BeginHudPanel(Neuron::Canvas& _canvas, float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits)
{
  (void)_canvas.BeginPanel(At(_leftUnits, _topUnits, {.widthUnits = _widthUnits, .heightUnits = _heightUnits}));
}

// A panel of the HUD that stands over the windows, a layer of its own that takes a press over them (ADR-031 decision 4).
void BeginFrontPanel(Neuron::Canvas& _canvas, float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits)
{
  (void)_canvas.BeginLayerPanel(At(_leftUnits, _topUnits, {.widthUnits = _widthUnits, .heightUnits = _heightUnits}));
}

// A rectangle of _color at a place in a panel, which the game composes of its own, such as a bar; its node.
std::size_t FillAt(Neuron::Canvas& _canvas, float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits,
                   const DirectX::XMFLOAT4& _color)
{
  const std::size_t node = _canvas.Tree().Add(At(_leftUnits, _topUnits, {.widthUnits = _widthUnits, .heightUnits = _heightUnits}));
  _canvas.Fill(node, _color);
  return node;
}

// The edge round a lit rectangle at a place in a panel: its top, its foot, its left and its right.
void EdgeAt(Neuron::Canvas& _canvas, float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits,
            const DirectX::XMFLOAT4& _color)
{
  (void)FillAt(_canvas, _leftUnits, _topUnits, _widthUnits, LINE_UNITS, _color);
  (void)FillAt(_canvas, _leftUnits, _topUnits + _heightUnits - LINE_UNITS, _widthUnits, LINE_UNITS, _color);
  (void)FillAt(_canvas, _leftUnits, _topUnits, LINE_UNITS, _heightUnits, _color);
  (void)FillAt(_canvas, _leftUnits + _widthUnits - LINE_UNITS, _topUnits, LINE_UNITS, _heightUnits, _color);
}

// A warning chip of _text at a place in a panel (ADR-085 decision 1).
void ChipAt(Neuron::Canvas& _canvas, Neuron::LayoutText _text, const Neuron::LayoutStyle& _place)
{
  (void)_canvas.Tree().Begin(Neuron::LayoutKind::Row, _place);
  (void)_canvas.Chip(std::move(_text));
  _canvas.Tree().End();
}

// How wide the column under the Ore is: as wide as its widest fixed line and the longest topic's name with a queue's
// count after it (ADR-066 decision 3).
float ColumnUnitsOf(const Hud::Content& _content, const Hud::TextMetrics& _metrics)
{
  float columnUnits = COLUMN_LEAST_UNITS;
  const float queuedUnits = _metrics.Width(Hud::Typeface::Figure, COLUMN_WIDEST_QUEUED) + FIT_GAP_UNITS;
  for (const std::string& name : _content.researchNames)
    columnUnits = std::max(columnUnits, _metrics.Width(Hud::Typeface::Name, name) + queuedUnits + (2.0f * PADDING));
  for (const std::string_view line : COLUMN_WIDEST_LINES)
    columnUnits = std::max(columnUnits, _metrics.Width(Hud::Typeface::Name, line) + (2.0f * CHIP_PAD_UNITS) + (2.0f * PADDING));
  return columnUnits;
}

// The Ore, at the top left, as the windows write it, and what the rigs earn each second, a warning chip when they earn
// nothing, since then nothing the player spends comes back.
void OrePanel(const HudBench& _bench, const Hud::Content& _content, float _columnUnits)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  BeginHudPanel(canvas, MARGIN, MARGIN, _columnUnits, ORE_PANEL_HEIGHT);
  OreAmount(canvas, _content.ore, Hud::Typeface::Title, GOLD_COLOR, At(PADDING, (ORE_PANEL_HEIGHT - TITLE_LINE_UNITS) / 2.0f));
  const std::int32_t income = _content.oreIncomeHundredthsPerSecond;
  std::string incomeText = income % Outpost::HUNDREDTHS == 0 ? std::format("+{}/s", income / Outpost::HUNDREDTHS)
                                                             : std::format("+{:.1f}/s", static_cast<double>(income) / Outpost::HUNDREDTHS);
  const float incomeTop = ((ORE_PANEL_HEIGHT - FIGURE_LINE_UNITS) / 2.0f) + 2.0f;
  if (income > 0)
    (void)canvas.Label(LineText(std::move(incomeText), Hud::Typeface::Figure, FIGURE_LINE_UNITS), NUMBERS_COLOR,
                       EndingAt(PADDING, incomeTop));
  else
    ChipAt(canvas, LineText(std::move(incomeText), Hud::Typeface::Figure, FIGURE_LINE_UNITS), EndingAt(PADDING, incomeTop));
  canvas.EndPanel();
}

// How many lines the territory panel holds: the nodes, the tickets while both sides have them, and the drain's.
float TerritoryRows(const Hud::Territory& _territory) noexcept
{
  const bool tickets = _territory.ownTickets.has_value() && _territory.enemyTickets.has_value();
  return 1.0f + (tickets ? 1.0f : 0.0f) + (_territory.drain.empty() ? 0.0f : 1.0f);
}

// Under the status panel's place: the nodes each side holds and each side's tickets, the player's against the enemy's,
// "3 : 2", the player's in its color and the enemy's in theirs (ADR-056, ADR-057). Beside "Nodes", in the labels' color,
// the Command Station's cap of the map's nodes, "cap 10 of 25" (ADR-064 decision 11). Under the tickets, who the drain
// takes from and when they run out, a warning chip when it is the player (ADR-057 decision 8).
void TerritoryPanel(const HudBench& _bench, const Hud::Content& _content, const Hud::Territory& _territory, float _columnUnits,
                    float _topUnits)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  const bool tickets = _territory.ownTickets.has_value() && _territory.enemyTickets.has_value();
  BeginHudPanel(canvas, MARGIN, _topUnits, _columnUnits, (2.0f * TERRITORY_INSET_UNITS) + (TerritoryRows(_territory) * NAME_LINE_UNITS));
  const float right = _columnUnits - PADDING;
  const auto row = [&](const std::string& _label, const std::string& _note, const std::string& _own, const std::string& _enemy, float _top)
  {
    const float figureTop = _top + ((NAME_LINE_UNITS - FIGURE_LINE_UNITS) / 2.0f) + 2.0f;
    std::string own = std::format("{} : ", _own);
    const float ownRight = right - metrics.Width(Hud::Typeface::Figure, _enemy);
    const float labelRoom = ownRight - metrics.Width(Hud::Typeface::Figure, own) - PADDING - FIT_GAP_UNITS;
    std::string label = metrics.Fit(Hud::Typeface::Name, _label, labelRoom);
    const float noteLeft = PADDING + metrics.Width(Hud::Typeface::Name, label) + FIT_GAP_UNITS;
    (void)canvas.Label(LineText(std::move(label), Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR, At(PADDING, _top));
    if (!_note.empty())
    {
      (void)canvas.Label(
        LineText(metrics.Fit(Hud::Typeface::Name, _note, PADDING + labelRoom - noteLeft), Hud::Typeface::Name, NAME_LINE_UNITS),
        LABEL_COLOR, At(noteLeft, _top));
    }
    (void)canvas.Label(LineText(_enemy, Hud::Typeface::Figure, FIGURE_LINE_UNITS),
                       SideColorOf(_content, Hud::Side::Enemy, _territory.enemy), EndingAt(PADDING, figureTop));
    (void)canvas.Label(LineText(std::move(own), Hud::Typeface::Figure, FIGURE_LINE_UNITS),
                       SideColorOf(_content, Hud::Side::Own, _content.player), EndingAt(_columnUnits - ownRight, figureTop));
  };
  const std::string note =
    _territory.cap > 0 ? std::format("cap {} of {}", _territory.cap, _territory.nodes) : std::format("of {}", _territory.nodes);
  row("Nodes", note, std::to_string(_territory.ownNodes), std::to_string(_territory.enemyNodes), TERRITORY_INSET_UNITS);
  float top = TERRITORY_INSET_UNITS + NAME_LINE_UNITS;
  if (tickets)
  {
    row("Tickets", {}, Outpost::WithThousands(_territory.ownTickets.value_or(0)),
        Outpost::WithThousands(_territory.enemyTickets.value_or(0)), top);
    top += NAME_LINE_UNITS;
  }
  if (!_territory.drain.empty())
  {
    if (_territory.drainWarns)
    {
      ChipAt(canvas,
             LineText(metrics.Fit(Hud::Typeface::Name, _territory.drain, _columnUnits - (2.0f * PADDING) - (2.0f * CHIP_PAD_UNITS)),
                      Hud::Typeface::Name, NAME_LINE_UNITS),
             At(PADDING - CHIP_PAD_UNITS, top));
    }
    else
    {
      (void)canvas.Label(
        LineText(metrics.Fit(Hud::Typeface::Name, _territory.drain, _columnUnits - (2.0f * PADDING)), Hud::Typeface::Name, NAME_LINE_UNITS),
        TEXT_COLOR, At(PADDING, top));
    }
  }
  canvas.EndPanel();
}

// Under the territory: the alerts, newest first, the newest a warning chip whose words line up with the others' (ADR-059,
// ADR-085 decision 1).
void AlertsPanel(const HudBench& _bench, const Hud::Content& _content, float _columnUnits, float _topUnits)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  BeginHudPanel(canvas, MARGIN, _topUnits, _columnUnits,
                (2.0f * TERRITORY_INSET_UNITS) + (static_cast<float>(_content.alerts.size()) * NAME_LINE_UNITS));
  for (std::size_t line = 0; line < _content.alerts.size(); ++line)
  {
    const float top = TERRITORY_INSET_UNITS + (static_cast<float>(line) * NAME_LINE_UNITS);
    const std::string& text = _content.alerts[line].first;
    if (line == 0)
    {
      ChipAt(canvas,
             LineText(metrics.Fit(Hud::Typeface::Name, text, _columnUnits - (2.0f * PADDING) - (2.0f * CHIP_PAD_UNITS)),
                      Hud::Typeface::Name, NAME_LINE_UNITS),
             At(PADDING - CHIP_PAD_UNITS, top));
    }
    else
    {
      (void)canvas.Label(
        LineText(metrics.Fit(Hud::Typeface::Name, text, _columnUnits - (2.0f * PADDING)), Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR,
        At(PADDING, top));
    }
  }
  canvas.EndPanel();
}

// The status panel, under the Ore: the Research Lab's line, the Shipyards' and the fleet's, an idle one's IDLE a chip,
// research's progress and the fleet against its cap as bars, each line a place to click (ADR-066). Under the pointer a
// line lies on the cards' color, as does any line whose click does the same.
void StatusPanel(const HudBench& _bench, const Hud::Content& _content, float _columnUnits, float _topUnits)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  const auto runUnits = [&metrics](const Hud::StatusRun& _run)
  { return metrics.Width(Hud::Typeface::Name, _run.text) + (_run.chip ? 2.0f * CHIP_PAD_UNITS : 0.0f); };
  const auto rowUnits = [](const Hud::StatusLine& _line)
  { return NAME_LINE_UNITS + (_line.bar.has_value() && _line.bar->under ? STATUS_BAR_ROW_UNITS : 0.0f); };
  float heightUnits = 2.0f * TERRITORY_INSET_UNITS;
  for (const Hud::StatusLine& line : _content.status)
    heightUnits += rowUnits(line);
  BeginHudPanel(canvas, MARGIN, _topUnits, _columnUnits, heightUnits);
  const float right = _columnUnits - PADDING;
  float top = TERRITORY_INSET_UNITS;
  for (std::size_t line = 0; line < _content.status.size(); ++line)
  {
    const Hud::StatusLine& status = _content.status[line];
    const float lineUnits = rowUnits(status);
    // The line's row across the panel takes the click, the first and the last reaching to the panel's edge.
    const float rowTop = line == 0 ? 0.0f : top;
    const float rowBottom = line + 1 == _content.status.size() ? heightUnits : top + lineUnits;
    const std::size_t row = canvas.Tree().Add(At(0.0f, rowTop, {.widthUnits = _columnUnits, .heightUnits = rowBottom - rowTop}));
    if (_bench.IsHovered(status.action))
      canvas.Fill(row, CARD_COLOR);
    canvas.Press(row, PressOf(*_bench.layout, status.action));
    // A bar's figure at the line's end, which the words stop short of, with room for a bar beside them.
    float wordsRight = right;
    if (status.bar.has_value() && !status.bar->figure.empty())
    {
      (void)canvas.Label(LineText(status.bar->figure, Hud::Typeface::Figure, FIGURE_LINE_UNITS), NUMBERS_COLOR,
                         EndingAt(PADDING, top + ((NAME_LINE_UNITS - FIGURE_LINE_UNITS) / 2.0f) + 2.0f));
      wordsRight -= metrics.Width(Hud::Typeface::Figure, status.bar->figure) + (status.bar->under ? FIT_GAP_UNITS : STATUS_BAR_GAP_UNITS);
    }
    // Each run after the one before; a plain run is cut short to leave room for the runs after it, a chip never is.
    float left = PADDING;
    for (std::size_t run = 0; run < status.runs.size(); ++run)
    {
      const Hud::StatusRun& words = status.runs[run];
      if (words.chip)
      {
        ChipAt(canvas, LineText(words.text, Hud::Typeface::Name, NAME_LINE_UNITS), At(left, top));
        left += runUnits(words);
        continue;
      }
      float afterUnits = 0.0f;
      for (std::size_t later = run + 1; later < status.runs.size(); ++later)
        afterUnits += runUnits(status.runs[later]);
      std::string text = metrics.Fit(Hud::Typeface::Name, words.text, wordsRight - left - afterUnits);
      const float textUnitsSet = metrics.Width(Hud::Typeface::Name, text);
      (void)canvas.Label(LineText(std::move(text), Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR, At(left, top));
      left += textUnitsSet;
    }
    // The bar: under the words, across the panel, or beside them, from their end to its figure; filled for its share.
    if (status.bar.has_value())
    {
      const Hud::StatusBar& bar = *status.bar;
      const float barLeft = bar.under ? PADDING : left + STATUS_BAR_GAP_UNITS;
      const float barWidth = (bar.under ? right : wordsRight) - barLeft;
      const float thickness = bar.under ? STATUS_UNDER_BAR_UNITS : STATUS_BESIDE_BAR_UNITS;
      const float barTop =
        bar.under ? top + NAME_LINE_UNITS + ((STATUS_BAR_ROW_UNITS - thickness) / 2.0f) : top + ((NAME_LINE_UNITS - thickness) / 2.0f);
      if (barWidth > 0.0f)
      {
        (void)FillAt(canvas, barLeft, barTop, barWidth, thickness, BAR_TRACK_COLOR);
        if (bar.share > 0.0f)
          (void)FillAt(canvas, barLeft, barTop, barWidth * std::min(bar.share, 1.0f), thickness, BAR_FILL_COLOR);
      }
    }
    top += lineUnits;
  }
  canvas.EndPanel();
}

// The hint at the top middle, over the windows: what a click on the ground will do, or what the pointer is over.
void HintPanel(const HudBench& _bench, const std::string& _hint, Outpost::WindowManager::Point _at)
{
  Neuron::Canvas& canvas = *_bench.front;
  BeginFrontPanel(canvas, _at.xUnits, _at.yUnits, HINT_PANEL_WIDTH, ORE_PANEL_HEIGHT);
  (void)canvas.Label(
    LineText(_bench.metrics->Fit(Hud::Typeface::Name, _hint, HINT_PANEL_WIDTH - (2.0f * PADDING)), Hud::Typeface::Name, NAME_LINE_UNITS),
    TEXT_COLOR, At(PADDING, (ORE_PANEL_HEIGHT - NAME_LINE_UNITS) / 2.0f));
  canvas.EndPanel();
}

// The tag at the top right: where the match runs, "Local skirmish" or "Server 203.0.113.5:4433 · 42 ms", and under it,
// once no snapshot has come for SILENT_SECONDS, a warning chip, "No word from the server · 5 s" (ADR-086). The round trip
// is as wide as 999 ms whatever it reads, so that the tag's edge stands still as it changes. It takes no click.
void TagPanel(const HudBench& _bench, const Hud::Connection& _connection)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  const std::string where = _connection.server.empty() ? std::string("Local skirmish") : std::format("Server {}", _connection.server);
  std::string roundTrip;
  float roundTripUnits = 0.0f;
  if (_connection.roundTripMilliseconds.has_value())
  {
    roundTrip = std::format("{}{} ms", DOT, std::clamp(*_connection.roundTripMilliseconds, 0, 999));
    roundTripUnits = metrics.Width(Hud::Typeface::Name, CONNECTION_WIDEST_ROUND_TRIP);
  }
  // Joining a lost connection again says so in the silence's place (ADR-078 decision 10).
  const bool silent = _connection.silentSeconds >= Hud::SILENT_SECONDS || _connection.reconnectingSeconds.has_value();
  // An address too long is cut short, so that with its round trip the tag is no wider than CONNECTION_MOST_UNITS.
  const float whereUnits = std::min(metrics.Width(Hud::Typeface::Name, where), CONNECTION_MOST_UNITS - roundTripUnits);
  float textUnits = whereUnits + roundTripUnits;
  if (silent)
    textUnits = std::max(textUnits, metrics.Width(Hud::Typeface::Name, CONNECTION_WIDEST_SILENCE) + (2.0f * CHIP_PAD_UNITS));
  const float widthUnits = textUnits + (2.0f * PADDING);
  const float heightUnits = (2.0f * TERRITORY_INSET_UNITS) + ((silent ? 2.0f : 1.0f) * NAME_LINE_UNITS);
  BeginHudPanel(canvas, _bench.screenWidthUnits - MARGIN - widthUnits, MARGIN, widthUnits, heightUnits);
  (void)canvas.Label(LineText(metrics.Fit(Hud::Typeface::Name, where, whereUnits) + roundTrip, Hud::Typeface::Name, NAME_LINE_UNITS),
                     TEXT_COLOR, At(PADDING, TERRITORY_INSET_UNITS));
  if (silent)
  {
    std::string warning = _connection.reconnectingSeconds.has_value()
                            ? std::format("Reconnecting{}{} s", DOT, std::clamp(*_connection.reconnectingSeconds, 0, 999))
                            : std::format("No word from the server{}{} s", DOT, std::min(_connection.silentSeconds, 999));
    ChipAt(canvas, LineText(std::move(warning), Hud::Typeface::Name, NAME_LINE_UNITS),
           At(PADDING - CHIP_PAD_UNITS, TERRITORY_INSET_UNITS + NAME_LINE_UNITS));
  }
  canvas.EndPanel();
}

// The banner under the hint, over the windows: how the match ended, and the way back to the menu.
void BannerPanel(const HudBench& _bench, const Hud::Outcome& _outcome, Outpost::WindowManager::Point _at)
{
  Neuron::Canvas& canvas = *_bench.front;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  const float heightUnits = (2.0f * PADDING) + TITLE_LINE_UNITS + NAME_LINE_UNITS + BUTTON_GAP + BUTTON_HEIGHT;
  BeginFrontPanel(canvas, _at.xUnits, _at.yUnits, BANNER_WIDTH, heightUnits);
  const float room = BANNER_WIDTH - (2.0f * PADDING);
  (void)canvas.Label(LineText(metrics.Fit(Hud::Typeface::Title, _outcome.title, room), Hud::Typeface::Title, TITLE_LINE_UNITS), GOLD_COLOR,
                     At(PADDING, PADDING));
  (void)canvas.Label(LineText(metrics.Fit(Hud::Typeface::Name, _outcome.detail, room), Hud::Typeface::Name, NAME_LINE_UNITS), TEXT_COLOR,
                     At(PADDING, PADDING + TITLE_LINE_UNITS));
  (void)canvas.Button(
    "back to menu", ContentOfHud(_bench, {.label = "Back to menu", .action = {.kind = Hud::ActionKind::BackToMenu}}),
    At(PADDING, PADDING + TITLE_LINE_UNITS + NAME_LINE_UNITS + BUTTON_GAP, {.widthUnits = room, .heightUnits = BUTTON_HEIGHT}));
  canvas.EndPanel();
}

// The selection at the bottom middle (ADR-088 decision 2): its first line in the title face and the rest in the name face,
// cut short to the panel, and under them its ships' bars in rows, or else its one bar. A ship's bar fills from the left as
// its health does, edged in the text's color while the ship goes back to be repaired, or in the hover's under the pointer
// otherwise; a click on it selects that ship alone (ADR-088 decision 4).
void SelectionPanel(const HudBench& _bench, const Hud::Content& _content, float _leftUnits, float _widthUnits)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  const float linesUnits = TITLE_LINE_UNITS + (NAME_LINE_UNITS * static_cast<float>(_content.selection.size() - 1));
  const std::size_t barRows = (_content.shipBars.size() + SHIP_BARS_A_ROW - 1) / SHIP_BARS_A_ROW;
  float barUnits = 0.0f;
  if (barRows > 0)
  {
    const auto rows = static_cast<float>(barRows);
    barUnits = HEALTH_BAR_GAP_UNITS + (rows * SHIP_BAR_HEIGHT_UNITS) + ((rows - 1.0f) * SHIP_BAR_GAP_UNITS);
  }
  else if (_content.selectionHealth.has_value())
    barUnits = HEALTH_BAR_GAP_UNITS + HEALTH_BAR_UNITS;
  const float heightUnits = (2.0f * PADDING) + linesUnits + barUnits;
  BeginHudPanel(canvas, _leftUnits, _bench.screenHeightUnits - MARGIN - heightUnits, _widthUnits, heightUnits);
  const float room = _widthUnits - (2.0f * PADDING);
  (void)canvas.Label(LineText(metrics.Fit(Hud::Typeface::Title, _content.selection.front(), room), Hud::Typeface::Title, TITLE_LINE_UNITS),
                     TEXT_COLOR, At(PADDING, PADDING));
  for (std::size_t line = 1; line < _content.selection.size(); ++line)
  {
    (void)canvas.Label(LineText(metrics.Fit(Hud::Typeface::Name, _content.selection[line], room), Hud::Typeface::Name, NAME_LINE_UNITS),
                       NUMBERS_COLOR, At(PADDING, PADDING + TITLE_LINE_UNITS + (NAME_LINE_UNITS * static_cast<float>(line - 1))));
  }
  const float barTop = PADDING + linesUnits + HEALTH_BAR_GAP_UNITS;
  for (std::size_t i = 0; i < _content.shipBars.size(); ++i)
  {
    const Hud::ShipBar& bar = _content.shipBars[i];
    const std::size_t row = i / SHIP_BARS_A_ROW;
    const std::size_t column = i % SHIP_BARS_A_ROW;
    const float left = PADDING + (static_cast<float>(column) * (SHIP_BAR_WIDTH_UNITS + SHIP_BAR_GAP_UNITS));
    const float top = barTop + (static_cast<float>(row) * (SHIP_BAR_HEIGHT_UNITS + SHIP_BAR_GAP_UNITS));
    const Hud::Action select{.kind = Hud::ActionKind::Select, .entity = bar.ship};
    const std::size_t track = FillAt(canvas, left, top, SHIP_BAR_WIDTH_UNITS, SHIP_BAR_HEIGHT_UNITS, BAR_TRACK_COLOR);
    const float share = std::clamp(bar.share, 0.0f, 1.0f);
    if (share > 0.0f)
      (void)FillAt(canvas, left, top, SHIP_BAR_WIDTH_UNITS * share, SHIP_BAR_HEIGHT_UNITS, HealthColor(share));
    if (bar.retreating || _bench.IsHovered(select))
    {
      EdgeAt(canvas, left - LINE_UNITS, top - LINE_UNITS, SHIP_BAR_WIDTH_UNITS + (2.0f * LINE_UNITS),
             SHIP_BAR_HEIGHT_UNITS + (2.0f * LINE_UNITS), bar.retreating ? TEXT_COLOR : HOVER_EDGE_COLOR);
    }
    canvas.Press(track, PressOf(*_bench.layout, select));
  }
  if (barRows == 0 && _content.selectionHealth.has_value())
  {
    const float share = std::clamp(*_content.selectionHealth, 0.0f, 1.0f);
    (void)FillAt(canvas, PADDING, barTop, room, HEALTH_BAR_UNITS, BAR_TRACK_COLOR);
    if (share > 0.0f)
      (void)FillAt(canvas, PADDING, barTop, room * share, HEALTH_BAR_UNITS, HealthColor(share));
  }
  canvas.EndPanel();
}

// The command card's rows of cells (ADR-088 decision 2), each cell as wide as its row's share of the card less the gaps; a
// structure's cell in build mode, its name over its cost or why it cannot be built, this far in from its top and its foot;
// and the card as tall in every selection and mode: its padding, the BUILD label's line and three rows of structures,
// within which three rows of buttons and the retreat under them fit too.
constexpr std::size_t CARD_ROWS = Hud::CARD_CELLS / Hud::CARD_COLUMNS;
constexpr float CARD_ROOM_UNITS = COMMAND_CARD_WIDTH - (2.0f * PADDING);
constexpr float CARD_CELL_WIDTH =
  (CARD_ROOM_UNITS - (static_cast<float>(Hud::CARD_COLUMNS - 1) * BUTTON_GAP)) / static_cast<float>(Hud::CARD_COLUMNS);
constexpr float STRUCTURE_CELL_INSET = 6.0f;
constexpr float STRUCTURE_CELL_HEIGHT = (2.0f * STRUCTURE_CELL_INSET) + NAME_LINE_UNITS + FIGURE_LINE_UNITS;
constexpr float COMMAND_CARD_HEIGHT = (2.0f * PADDING) + RETREAT_LABEL_UNITS + (static_cast<float>(CARD_ROWS) * STRUCTURE_CELL_HEIGHT) +
                                      (static_cast<float>(CARD_ROWS - 1) * BUTTON_GAP);
static_assert((2.0f * PADDING) + (static_cast<float>(CARD_ROWS) * BUTTON_HEIGHT) + (static_cast<float>(CARD_ROWS) * BUTTON_GAP) +
                RETREAT_LABEL_UNITS + BUTTON_HEIGHT <=
              COMMAND_CARD_HEIGHT);

// The place of the command card's cell _cell, _rowHeightUnits to a row, from the card's top-left corner, as wide as its row
// when it is _wide.
Neuron::LayoutStyle CellAt(std::uint8_t _cell, bool _wide, float _topUnits, float _rowHeightUnits)
{
  const int row = _cell / Hud::CARD_COLUMNS;
  const int column = _cell % Hud::CARD_COLUMNS;
  return At(_wide ? PADDING : PADDING + (static_cast<float>(column) * (CARD_CELL_WIDTH + BUTTON_GAP)),
            _topUnits + (static_cast<float>(row) * (_rowHeightUnits + BUTTON_GAP)),
            {.widthUnits = _wide ? CARD_ROOM_UNITS : CARD_CELL_WIDTH, .heightUnits = _rowHeightUnits});
}

// The command card in the bottom-right corner (ADR-088 decision 2), as tall whatever it holds: each button in its cell, a
// row of cells under another from the card's top, and at its foot the selection's retreat: RETREAT, with MIXED at the
// right while its ships differ, over a row of three, its setting lit, each a press that sets it for every ship (ADR-088
// decision 3). In build mode, BUILD with B's cap at its right, the key that turns it off as Esc does, over the structures
// the selected Constructors build, each in its cell on two lines, and neither the buttons nor the retreat.
void CommandCard(const HudBench& _bench, const Hud::Content& _content)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  Hud::Layout& layout = *_bench.layout;
  BeginHudPanel(canvas, _bench.screenWidthUnits - MARGIN - COMMAND_CARD_WIDTH, _bench.screenHeightUnits - MARGIN - COMMAND_CARD_HEIGHT,
                COMMAND_CARD_WIDTH, COMMAND_CARD_HEIGHT);
  if (_content.build.has_value())
  {
    (void)canvas.Label(TrackedText("BUILD", RETREAT_LABEL_UNITS), ROW_LABEL_COLOR, At(PADDING, PADDING));
    (void)canvas.Tree().Begin(Neuron::LayoutKind::Row, EndingAt(PADDING, PADDING));
    (void)canvas.KeyCap(Outpost::KeyCap(Outpost::KEY_BUILD));
    canvas.Tree().End();
    for (std::size_t i = 0; i < _content.build->structures.size(); ++i)
    {
      const Hud::Button& structure = _content.build->structures[i];
      Neuron::ButtonContent content = ContentOfHud(_bench, structure);
      content.stacked = true;
      (void)canvas.Button(std::format("structure {}", i), content,
                          CellAt(structure.cell, false, PADDING + RETREAT_LABEL_UNITS, STRUCTURE_CELL_HEIGHT));
    }
    canvas.EndPanel();
    return;
  }
  for (std::size_t i = 0; i < _content.buttons.size(); ++i)
  {
    const Hud::Button& button = _content.buttons[i];
    (void)canvas.Button(std::format("button {}", i), ContentOfHud(_bench, button),
                        CellAt(button.cell, button.wide, PADDING, BUTTON_HEIGHT));
  }
  if (_content.retreat.has_value())
  {
    const float retreatTop = COMMAND_CARD_HEIGHT - PADDING - BUTTON_HEIGHT - RETREAT_LABEL_UNITS;
    const Hud::RetreatChoice& retreat = *_content.retreat;
    (void)canvas.Label(TrackedText("RETREAT", RETREAT_LABEL_UNITS), ROW_LABEL_COLOR, At(PADDING, retreatTop));
    if (retreat.mixed)
      (void)canvas.Label(TrackedText("MIXED", RETREAT_LABEL_UNITS), NUMBERS_COLOR, EndingAt(PADDING, retreatTop));
    std::array<Neuron::LayoutText, RETREAT_CHOICES.size()> words;
    std::array<std::uint32_t, RETREAT_CHOICES.size()> payloads{};
    for (std::size_t i = 0; i < RETREAT_CHOICES.size(); ++i)
    {
      words[i] = LineText(std::string(RetreatCell(RETREAT_CHOICES[i])), Hud::Typeface::Name,
                          std::round(Hud::FaceUnits(Hud::Typeface::Name) * RETREAT_LINE_SHARE));
      payloads[i] = PressOf(layout, {.kind = Hud::ActionKind::SetRetreat, .retreat = RETREAT_CHOICES[i]});
    }
    const auto picked = std::ranges::find(RETREAT_CHOICES, retreat.setting);
    canvas.Cells(
      "retreat", words, payloads,
      retreat.mixed || picked == RETREAT_CHOICES.end() ? std::nullopt
                                                       : std::optional(static_cast<std::size_t>(picked - RETREAT_CHOICES.begin())),
      BUTTON_HEIGHT, At(PADDING, retreatTop + RETREAT_LABEL_UNITS, {.widthUnits = CARD_ROOM_UNITS, .gapUnits = RETREAT_CELL_GAP_UNITS}));
  }
  canvas.EndPanel();
}

// The HUD's panels on _bench's canvas, each where it always stands (Game §interface.hud), finished into its layout, and
// the minimap's marks, fog and view drawn into the minimap's square once the canvas has placed it.
void LayHud(const HudBench& _bench, const Hud::Content& _content, std::span<const Outpost::PlanePosition> _view, float _scale)
{
  Neuron::Canvas& canvas = *_bench.canvas;
  Hud::Layout& layout = *_bench.layout;
  const Hud::TextMetrics& metrics = *_bench.metrics;
  const float screenWidthUnits = _bench.screenWidthUnits;
  const float screenHeightUnits = _bench.screenHeightUnits;

  // The column under the Ore, its panels one width whatever they hold (ADR-066 decision 3): the Ore, then the territory
  // and the alerts under the status panel's place, then the status panel, drawn in that order.
  const float columnUnits = ColumnUnitsOf(_content, metrics);
  OrePanel(_bench, _content, columnUnits);
  const float underOreUnits = MARGIN + ORE_PANEL_HEIGHT + PANEL_GAP;
  const float underStatusUnits = underOreUnits + STATUS_PANEL_KEPT_UNITS + PANEL_GAP;
  if (_content.territory.has_value())
    TerritoryPanel(_bench, _content, *_content.territory, columnUnits, underStatusUnits);
  if (!_content.alerts.empty())
  {
    const float territoryUnits = _content.territory.has_value()
                                   ? (2.0f * TERRITORY_INSET_UNITS) + (TerritoryRows(*_content.territory) * NAME_LINE_UNITS) + PANEL_GAP
                                   : 0.0f;
    AlertsPanel(_bench, _content, columnUnits, underStatusUnits + territoryUnits);
  }
  if (!_content.status.empty())
    StatusPanel(_bench, _content, columnUnits, underOreUnits);

  // The top row's middle (Game §interface.hud; ADR-015 decision 4): the hint, and the banner under it, stand in the middle
  // of the screen while they clear the column at its left and the tag, at its widest and tallest, at its right. Where they
  // do not, as the interface's scale grows, they stand aside: under the top row, in the middle of the room right of the
  // column, the banner under the hint.
  const bool tagged = _content.connection.has_value();
  const float tagUnits =
    tagged ? std::max(CONNECTION_MOST_UNITS, metrics.Width(Hud::Typeface::Name, CONNECTION_WIDEST_SILENCE) + (2.0f * CHIP_PAD_UNITS)) +
               (2.0f * PADDING)
           : 0.0f;
  const float tagHeightUnits = (2.0f * TERRITORY_INSET_UNITS) + (2.0f * NAME_LINE_UNITS);
  const float roomLeftUnits = MARGIN + columnUnits + PANEL_GAP;
  const float roomRightUnits = screenWidthUnits - MARGIN - (tagged ? tagUnits + PANEL_GAP : 0.0f);
  const float underTopRowUnits = MARGIN + std::max(ORE_PANEL_HEIGHT, tagged ? tagHeightUnits : 0.0f) + PANEL_GAP;
  const auto clears = [&](float _widthUnits)
  {
    const float left = (screenWidthUnits - _widthUnits) / 2.0f;
    return left >= roomLeftUnits && left + _widthUnits <= roomRightUnits;
  };
  const auto asideLeft = [&](float _widthUnits) { return (roomLeftUnits + screenWidthUnits - MARGIN - _widthUnits) / 2.0f; };
  std::optional<Outpost::WindowManager::Point> underHint;
  if (!_content.hint.empty())
  {
    const Outpost::WindowManager::Point at =
      clears(HINT_PANEL_WIDTH) ? Outpost::WindowManager::Point{.xUnits = (screenWidthUnits - HINT_PANEL_WIDTH) / 2.0f, .yUnits = MARGIN}
                               : Outpost::WindowManager::Point{.xUnits = asideLeft(HINT_PANEL_WIDTH), .yUnits = underTopRowUnits};
    if (at.yUnits > MARGIN)
      underHint = Outpost::WindowManager::Point{.xUnits = asideLeft(BANNER_WIDTH), .yUnits = at.yUnits + ORE_PANEL_HEIGHT + PANEL_GAP};
    HintPanel(_bench, _content.hint, at);
  }
  if (_content.connection.has_value())
    TagPanel(_bench, *_content.connection);
  if (_content.outcome.has_value())
  {
    const Outpost::WindowManager::Point at = underHint.value_or(
      clears(BANNER_WIDTH) ? Outpost::WindowManager::Point{.xUnits = (screenWidthUnits - BANNER_WIDTH) / 2.0f, .yUnits = underOreUnits}
                           : Outpost::WindowManager::Point{.xUnits = asideLeft(BANNER_WIDTH), .yUnits = underTopRowUnits});
    BannerPanel(_bench, *_content.outcome, at);
  }

  // The bottom row's console (ADR-088 decision 2): the selection in the middle, moved only as far as it must to stand a gap
  // clear of the command card's place and of the minimap, as wide as its longest line within its bounds; and the command
  // card in the bottom-right corner, while the selection offers a button, its retreat or build mode.
  if (!_content.selection.empty())
  {
    float textUnits = 0.0f;
    for (std::size_t line = 0; line < _content.selection.size(); ++line)
      textUnits = std::max(textUnits, metrics.Width(line == 0 ? Hud::Typeface::Title : Hud::Typeface::Name, _content.selection[line]));
    const float selectionUnits = std::clamp(textUnits + (2.0f * PADDING), SELECTION_PANEL_MIN_WIDTH, SELECTION_PANEL_WIDTH);
    float selectionLeft =
      std::min((screenWidthUnits - selectionUnits) / 2.0f, screenWidthUnits - MARGIN - COMMAND_CARD_WIDTH - PANEL_GAP - selectionUnits);
    if (_content.mapSizeMeters > 0.0f)
      selectionLeft = std::max(selectionLeft, MARGIN + MINIMAP_SIZE + PANEL_GAP);
    SelectionPanel(_bench, _content, selectionLeft, selectionUnits);
  }
  if (!_content.buttons.empty() || _content.retreat.has_value() || _content.build.has_value())
    CommandCard(_bench, _content);

  // The bottom left: the minimap's frame, its square the leaf the game draws the map into.
  std::size_t map = Neuron::LayoutNode::NO_NODE;
  if (_content.mapSizeMeters > 0.0f)
  {
    BeginHudPanel(canvas, MARGIN, screenHeightUnits - MARGIN - MINIMAP_SIZE, MINIMAP_SIZE, MINIMAP_SIZE);
    const float inner = MINIMAP_SIZE - (2.0f * MINIMAP_PADDING);
    map = canvas.Tree().Add(At(MINIMAP_PADDING, MINIMAP_PADDING, {.widthUnits = inner, .heightUnits = inner}));
    canvas.EndPanel();
  }

  const std::size_t firstRegion = layout.regions.size();
  canvas.Finish(layout);
  PlacePresses(layout, firstRegion);
  if (map != Neuron::LayoutNode::NO_NODE)
  {
    const Neuron::LayoutNode& square = canvas.Tree().Node(map);
    layout.minimap = {square.leftUnits * _scale, square.topUnits * _scale, square.widthUnits * _scale, square.heightUnits * _scale,
                      MAP_COLOR};
    DrawMap(layout, _content, _view, _scale);
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

// A fleet's plate (ADR-102 decision 2), in reference units at the interface's scale: its disc, the ring round it a unit
// out, as a line this wide, and the insignia on the disc. The ring's arc, its count and its insignia are white, which none
// of health's colors is, and the count stands on a dark halo. A whole fleet's ring is one sprite; a hurt fleet's is drawn
// in this many pieces round the circle, white as far as its hit points reach and in the halo's color for the rest, so
// that the arc reads as a share. At 13 units, a piece strays from the circle by a quarter of a pixel at 1080p.
constexpr float PLATE_DISC_UNITS = 22.0f;
constexpr float PLATE_RING_STROKE_UNITS = 2.0f;
constexpr float PLATE_RING_RADIUS_UNITS = (PLATE_DISC_UNITS / 2.0f) + 1.0f + (PLATE_RING_STROKE_UNITS / 2.0f);
static_assert(2.0f * (PLATE_RING_RADIUS_UNITS + (PLATE_RING_STROKE_UNITS / 2.0f)) == Hud::PLATE_WIDTH_UNITS);
constexpr int PLATE_RING_PIECES = 16;
constexpr float PLATE_INSIGNIA_UNITS = 14.0f;
constexpr float PLATE_HALO_EMS = 0.15f;
constexpr DirectX::XMFLOAT4 PLATE_MARK_COLOR{1.0f, 1.0f, 1.0f, 1.0f};
constexpr DirectX::XMFLOAT4 PLATE_HALO_COLOR{0.01f, 0.015f, 0.03f, 0.9f};
// Plates of two sides that meet are moved apart, up and down, at most this many times over.
constexpr int PLATE_PARTINGS = 4;

Hud::Sprite SpriteOf(Hud::Insignia _insignia) noexcept
{
  switch (_insignia)
  {
  case Hud::Insignia::TwoChevrons:
    return Hud::Sprite::TwoChevrons;
  case Hud::Insignia::ThreeChevrons:
    return Hud::Sprite::ThreeChevrons;
  case Hud::Insignia::Constructor:
    return Hud::Sprite::Plus;
  case Hud::Insignia::OneChevron:
  default:
    return Hud::Sprite::OneChevron;
  }
}

// The plates over the fleets, each standing a bar's lift above its foot and centered over it (ADR-102 decision 2): a
// count over a disc in its side's color, ringed by the arc of its hit points, with its insignia on the disc. A plate of one
// ship has no count. Plates of two sides that meet are moved apart, the higher up and the lower down, and a plate wholly
// off the screen is left out. Whole pixels, so that the sprites and the count stay sharp as the plates move.
void LayPlates(Hud::Layout& _layout, const Hud::Content& _content, const Hud::TextMetrics& _metrics, float _scale, float _width,
               float _height)
{
  struct Placed
  {
    const Hud::Plate* plate = nullptr;
    float left = 0.0f;
    float top = 0.0f;
    float height = 0.0f;
  };
  const float plateWidth = Hud::PLATE_WIDTH_UNITS * _scale;
  std::vector<Placed> placed;
  placed.reserve(_content.plates.size());
  for (const Hud::Plate& plate : _content.plates)
  {
    const float height = (plate.count > 1 ? Hud::PLATE_HEIGHT_UNITS : Hud::PLATE_WIDTH_UNITS) * _scale;
    const float bottom = plate.footPixels.y - (ENTITY_BAR_LIFT_UNITS * _scale);
    placed.push_back({.plate = &plate, .left = plate.footPixels.x - (plateWidth / 2.0f), .top = bottom - height, .height = height});
  }
  for (int parting = 0; parting < PLATE_PARTINGS; ++parting)
  {
    bool moved = false;
    for (std::size_t first = 0; first < placed.size(); ++first)
    {
      for (std::size_t second = first + 1; second < placed.size(); ++second)
      {
        Placed& a = placed[first];
        Placed& b = placed[second];
        const float overlap = std::min(a.top + a.height, b.top + b.height) - std::max(a.top, b.top);
        if (a.plate->owner == b.plate->owner || std::abs(a.left - b.left) >= plateWidth || overlap <= 0.0f)
          continue;
        Placed& upper = a.top < b.top || (a.top == b.top && a.plate->fleet < b.plate->fleet) ? a : b;
        Placed& lower = &upper == &a ? b : a;
        upper.top -= overlap / 2.0f;
        lower.top += overlap / 2.0f;
        moved = true;
      }
    }
    if (!moved)
      break;
  }

  const std::span<const Neuron::GlyphAtlas::Font> fonts = _metrics.Fonts();
  const std::size_t countFont = Hud::FontOf(Hud::Typeface::Plate);
  const float lineHeight =
    countFont < fonts.size() ? fonts[countFont].lineHeight : std::round(Hud::FaceUnits(Hud::Typeface::Plate) * 1.2f * _scale);
  const float stroke = std::max(1.0f, std::round(PLATE_RING_STROKE_UNITS * _scale));
  for (const Placed& at : placed)
  {
    const Hud::Plate& plate = *at.plate;
    const float left = std::round(at.left);
    const float top = std::round(at.top);
    if (left + plateWidth <= 0.0f || left >= _width || top + at.height <= 0.0f || top >= _height)
      continue;
    const float centerX = left + (plateWidth / 2.0f);
    const float centerY = top + at.height - (plateWidth / 2.0f);
    // The ring from the top, clockwise, white as far round as the fleet's hit points reach.
    const float radius = PLATE_RING_RADIUS_UNITS * _scale;
    const float share = std::clamp(plate.health, 0.0f, 1.0f);
    const auto pointAt = [centerX, centerY, radius](float _turn)
    {
      const float angle = (2.0f * std::numbers::pi_v<float> * _turn) - (std::numbers::pi_v<float> / 2.0f);
      return DirectX::XMFLOAT2{centerX + (radius * std::cos(angle)), centerY + (radius * std::sin(angle))};
    };
    if (share >= 1.0f)
    {
      const float ring = std::round(Hud::PLATE_WIDTH_UNITS * _scale);
      _layout.plates.sprites.push_back({.sprite = Hud::SpriteOf(Hud::Sprite::PlateRing),
                                        .area = {.left = std::round(centerX - (ring / 2.0f)),
                                                 .top = std::round(centerY - (ring / 2.0f)),
                                                 .width = ring,
                                                 .height = ring,
                                                 .color = PLATE_MARK_COLOR}});
    }
    for (int piece = 0; piece < PLATE_RING_PIECES && share < 1.0f; ++piece)
    {
      const float from = static_cast<float>(piece) / static_cast<float>(PLATE_RING_PIECES);
      const float to = static_cast<float>(piece + 1) / static_cast<float>(PLATE_RING_PIECES);
      if (share > from)
        _layout.plates.lines.push_back(
          {.segment = {.from = pointAt(from), .to = pointAt(std::min(to, share)), .widthPixels = stroke}, .color = PLATE_MARK_COLOR});
      if (share < to)
        _layout.plates.lines.push_back(
          {.segment = {.from = pointAt(std::max(from, share)), .to = pointAt(to), .widthPixels = stroke}, .color = PLATE_HALO_COLOR});
    }
    const float disc = std::round(PLATE_DISC_UNITS * _scale);
    DirectX::XMFLOAT4 discColor = plate.color;
    discColor.w = 1.0f;
    _layout.plates.sprites.push_back({.sprite = Hud::SpriteOf(Hud::Sprite::PlateDisc),
                                      .area = {.left = std::round(centerX - (disc / 2.0f)),
                                               .top = std::round(centerY - (disc / 2.0f)),
                                               .width = disc,
                                               .height = disc,
                                               .color = discColor}});
    const float insignia = std::round(PLATE_INSIGNIA_UNITS * _scale);
    _layout.plates.sprites.push_back({.sprite = Hud::SpriteOf(SpriteOf(plate.insignia)),
                                      .area = {.left = std::round(centerX - (insignia / 2.0f)),
                                               .top = std::round(centerY - (insignia / 2.0f)),
                                               .width = insignia,
                                               .height = insignia,
                                               .color = PLATE_MARK_COLOR}});
    if (plate.count > 1)
    {
      const std::string count = std::to_string(plate.count);
      const float countWidth = _metrics.Width(Hud::Typeface::Plate, count) * _scale;
      _layout.plates.texts.push_back({.text = count,
                                      .left = std::round(centerX - (countWidth / 2.0f)),
                                      .top = std::round(top + (((Hud::PLATE_COUNT_UNITS * _scale) - lineHeight) / 2.0f)),
                                      .color = PLATE_MARK_COLOR,
                                      .font = countFont,
                                      .trackingPixels = 0.0f,
                                      .halo = PLATE_HALO_COLOR});
    }
    if (plate.own)
      _layout.plateHits.emplace_back(Hud::Rect{.left = left, .top = top, .width = plateWidth, .height = at.height}, plate.fleet);
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

std::string Hud::StatusLine::Text() const
{
  std::string text;
  for (const StatusRun& run : runs)
    text += run.text;
  return text;
}

bool Hud::StatusLine::Warns() const noexcept
{
  return std::ranges::any_of(runs, &StatusRun::chip);
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

std::optional<Hud::EntityBar> Hud::BarOver(const EntityView& _entity, DirectX::XMFLOAT2 _footPixels, const DirectX::XMFLOAT4& _back,
                                           BarRule _rule) noexcept
{
  if (_entity.remembered)
    return std::nullopt;
  EntityBar bar{.footPixels = _footPixels, .health = std::nullopt, .built = std::nullopt, .back = _back};
  // A ship far out has its fleet's plate in place of its bar, unless Alt is held or the pointer is over it (ADR-102
  // decision 3); a structure keeps its bar.
  const bool ship = _entity.kind == EntityKind::Ship;
  const bool hurt = _entity.hitPointsHundredths < _entity.maxHitPointsHundredths;
  const bool shows = _rule.every || (ship ? (_rule.pointed || (_rule.closeIn && hurt)) : hurt);
  if (_entity.maxHitPointsHundredths > 0 && shows)
    bar.health = HealthShare(_entity.hitPointsHundredths, _entity.maxHitPointsHundredths);
  if (_entity.builtPermille < PERMILLE)
    bar.built = static_cast<float>(std::max(_entity.builtPermille, 0)) / static_cast<float>(PERMILLE);
  if (!bar.health.has_value() && !bar.built.has_value())
    return std::nullopt;
  return bar;
}

Hud::Plate Hud::PlateOver(const ScreenFleets::Fleet& _fleet, const Snapshot& _newest, std::span<const EntityView> _entities,
                          std::span<const SideColor> _sideColors)
{
  Plate plate{.fleet = _fleet.id,
              .owner = _fleet.owner,
              .footPixels = _fleet.footPixels,
              .count = static_cast<std::uint32_t>(_fleet.ships.size()),
              .insignia = Insignia::OneChevron,
              .health = 1.0f,
              .color = NEUTRAL_COLOR,
              .own = _fleet.owner == _newest.player};
  if (_fleet.owner == PIRATES)
    plate.color = PIRATE_COLOR;
  else if (const auto found = std::ranges::find(_sideColors, _fleet.owner, &SideColor::player); found != _sideColors.end())
    plate.color = found->color;

  // The match's hulls, smallest first, give a warship's chevrons: one for the smallest, three for the third and any larger.
  std::vector<const HullView*> hulls;
  hulls.reserve(_newest.hulls.size());
  for (const HullView& hull : _newest.hulls)
    hulls.push_back(&hull);
  std::ranges::stable_sort(hulls, {}, &HullView::footprintRadiusMeters);
  const auto insigniaOf = [&hulls](const EntityView& _ship)
  {
    if (_ship.role == ShipRole::Constructor)
      return Insignia::Constructor;
    const auto rank = std::ranges::find(hulls, _ship.hull, &HullView::id);
    if (rank == hulls.end())
      return Insignia::OneChevron;
    const auto place = std::min<std::ptrdiff_t>(rank - hulls.begin(), 2);
    return place <= 0 ? Insignia::OneChevron : place == 1 ? Insignia::TwoChevrons : Insignia::ThreeChevrons;
  };
  std::array<std::uint32_t, 4> counts{};
  std::int64_t hundredths = 0;
  std::int64_t maxHundredths = 0;
  for (const EntityView& entity : _entities)
  {
    if (!std::ranges::binary_search(_fleet.ships, entity.id))
      continue;
    ++counts[static_cast<std::size_t>(insigniaOf(entity))];
    hundredths += entity.hitPointsHundredths;
    maxHundredths += entity.maxHitPointsHundredths;
  }
  // The commonest; a tie goes to the larger hull, and a warship before a Constructor.
  const std::array<Insignia, 4> preferred{Insignia::ThreeChevrons, Insignia::TwoChevrons, Insignia::OneChevron, Insignia::Constructor};
  std::uint32_t most = 0;
  for (const Insignia insignia : preferred)
  {
    if (counts[static_cast<std::size_t>(insignia)] > most)
    {
      most = counts[static_cast<std::size_t>(insignia)];
      plate.insignia = insignia;
    }
  }
  plate.health = maxHundredths > 0 ? HealthShare(hundredths, maxHundredths) : 1.0f;
  return plate;
}

std::optional<Hud::Outcome> Hud::DescribeOutcome(const Snapshot& _newest, std::uint32_t _ticksPerSecond)
{
  // In a universe no match ends: a player who lost waits for its seat to restart at its start (ADR-084 decisions 4 and 7).
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
  // A domination says so: the side that ran out of tickets held less of the map (ADR-057 decision 8); and so does a battle
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

std::string Hud::ServerName(std::string_view _host, std::uint16_t _port)
{
  return _host.contains(':') ? std::format("[{}]:{}", _host, _port) : std::format("{}:{}", _host, _port);
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
    panel.scheduled.push_back({.text = std::format("{}{}{}", text, DOT, ships(order.ships.size())), .order = order.id});
  }

  // A field picked from a drop-down: its choices, and the one it holds, which is its value, or _none.
  const auto picked = [&](std::string _label, OrderField _field, std::string_view _none)
  {
    OrderRow row{
      .label = std::move(_label), .value = std::string(_none), .field = _field, .choices = _form.Choices(_field, _newest, _entities)};
    const auto held = std::ranges::find_if(row.choices, [&](const OrderChoice& _choice) { return _form.Holds(_field, _choice); });
    if (held != row.choices.end())
    {
      row.picked = static_cast<std::size_t>(held - row.choices.begin());
      row.value = held->name;
    }
    return row;
  };
  panel.rows.push_back({.label = "WHEN", .value = std::string(OrderForm::NameOf(_form.Trigger())), .field = OrderField::Trigger});
  const bool timed = _form.Trigger() == ScheduledTriggerKind::TimeOfDay;
  if (timed)
  {
    panel.rows.push_back(picked("HOUR", OrderField::Hour, ""));
    panel.rows.push_back(picked("MINUTE", OrderField::Minute, ""));
  }
  else
    panel.rows.push_back(picked("WATCHING", OrderField::TriggerSector, "None held"));
  panel.rows.push_back({.label = "DO", .value = std::string(OrderForm::NameOf(_form.Action())), .field = OrderField::Action});
  if (!_newest.sectors.empty())
    panel.rows.push_back(picked("WHERE", OrderField::ActionSector, "None"));
  if (_form.TakesTarget())
    panel.rows.push_back(picked("TARGET", OrderField::Target, "None there"));
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
  Layout layout;
  layout.fontPixels = FONT_UNITS * scale;
  // A skirmish at each difficulty (ADR-065), a universe to join when there is one (ADR-078), then Quit.
  std::vector<Button> buttons{{.label = "Easy skirmish", .action = {.kind = ActionKind::StartSkirmish, .difficulty = Difficulty::Easy}},
                              {.label = "Normal skirmish", .action = {.kind = ActionKind::StartSkirmish, .difficulty = Difficulty::Normal}},
                              {.label = "Hard skirmish", .action = {.kind = ActionKind::StartSkirmish, .difficulty = Difficulty::Hard}}};
  if (_state.joinUniverse)
    buttons.push_back({.label = "Join universe", .action = {.kind = ActionKind::JoinUniverse}});
  // While a join connects, nothing else starts.
  const bool connecting = !_state.connecting.empty();
  for (Button& button : buttons)
    button.enabled = !connecting;
  buttons.push_back({.label = "Quit", .action = {.kind = ActionKind::Quit}});
  // The notice, such as why the last game ended, wrapped to the menu's width under its subtitle.
  const std::vector<std::string> notice =
    _state.notice.empty() ? std::vector<std::string>() : _metrics.Wrap(Typeface::Name, _state.notice, MENU_WIDTH - (2.0f * PADDING));
  const auto noticeUnits = (static_cast<float>(notice.size()) + (connecting ? 1.0f : 0.0f)) * NAME_LINE_UNITS;
  const auto count = static_cast<float>(buttons.size());
  const float heightUnits = (2.0f * PADDING) + TITLE_LINE_UNITS + NAME_LINE_UNITS + noticeUnits + BUTTON_GAP + (count * BUTTON_HEIGHT) +
                            ((count - 1.0f) * BUTTON_GAP);
  const WindowManager::Point corner{.xUnits = (static_cast<float>(_widthPixels) / scale / 2.0f) - (MENU_WIDTH / 2.0f),
                                    .yUnits = (static_cast<float>(_heightPixels) / scale / 2.0f) - (heightUnits / 2.0f)};
  // The menu is a panel of the canvas in the middle of the screen: its title, its subtitle, what it says, and its buttons,
  // which nothing lights under the pointer.
  Neuron::Interaction unpointed;
  Neuron::Canvas canvas(GameTheme(), _metrics, unpointed, static_cast<float>(_widthPixels) / scale,
                        static_cast<float>(_heightPixels) / scale);
  BeginHudPanel(canvas, corner.xUnits, corner.yUnits, MENU_WIDTH, heightUnits);
  (void)canvas.Label(LineText("Outpost Commander", Typeface::Title, TITLE_LINE_UNITS), GOLD_COLOR, At(PADDING, PADDING));
  (void)canvas.Label(LineText("A skirmish against the AI", Typeface::Name, NAME_LINE_UNITS), DIM_TEXT_COLOR,
                     At(PADDING, PADDING + TITLE_LINE_UNITS));
  float top = PADDING + TITLE_LINE_UNITS + NAME_LINE_UNITS;
  if (connecting)
  {
    (void)canvas.Label(
      LineText(_metrics.Fit(Typeface::Name, std::format("Connecting to {}...", _state.connecting), MENU_WIDTH - (2.0f * PADDING)),
               Typeface::Name, NAME_LINE_UNITS),
      TEXT_COLOR, At(PADDING, top));
    top += NAME_LINE_UNITS;
  }
  for (const std::string& line : notice)
  {
    (void)canvas.Label(LineText(line, Typeface::Name, NAME_LINE_UNITS), WARNING_COLOR, At(PADDING, top));
    top += NAME_LINE_UNITS;
  }
  top += BUTTON_GAP;
  for (std::size_t button = 0; button < buttons.size(); ++button)
  {
    (void)canvas.Button(std::format("menu {}", button), ContentOf(layout, buttons[button]),
                        At(PADDING, top, {.widthUnits = MENU_WIDTH - (2.0f * PADDING), .heightUnits = BUTTON_HEIGHT}));
    top += BUTTON_HEIGHT + BUTTON_GAP;
  }
  canvas.EndPanel();
  canvas.Finish(layout);
  PlacePresses(layout, 0);
  return layout;
}

std::optional<Hud::Action> Hud::Layout::ActionAt(float _xPixels, float _yPixels) const noexcept
{
  if (const Neuron::CanvasRegion* region = HitAt(_xPixels, _yPixels); region != nullptr && region->payload != Neuron::NO_PAYLOAD)
    return actions[region->payload].second;
  return std::nullopt;
}

std::optional<std::uint32_t> Hud::Layout::PlateAt(float _xPixels, float _yPixels) const noexcept
{
  if (Covers(_xPixels, _yPixels))
    return std::nullopt;
  const auto hit = std::ranges::find_if(plateHits, [_xPixels, _yPixels](const std::pair<Rect, std::uint32_t>& _plate)
                                        { return _plate.first.Contains(_xPixels, _yPixels); });
  return hit != plateHits.end() ? std::optional(hit->second) : std::nullopt;
}

std::optional<Hud::Action> Hud::Layout::HoverAt(float _xPixels, float _yPixels) const noexcept
{
  if (const Neuron::CanvasRegion* region = PointedAt(_xPixels, _yPixels); region != nullptr && region->payload != Neuron::NO_PAYLOAD)
    return (region->hoverOnly ? previews : actions)[region->payload].second;
  return std::nullopt;
}

const Hud::Window* Hud::Layout::WindowAt(float _xPixels, float _yPixels) const noexcept
{
  // An open drop-down's overlay, the front layer, is no window's.
  const std::size_t layer = LayerAt(_xPixels, _yPixels);
  return layer == 0 || layer > windows.size() ? nullptr : &windows[layer - 1];
}

Hud::Span Hud::Layout::ActionsOf(std::size_t _layer) const noexcept
{
  return LayerSpan(*this, _layer, &Window::firstAction, actions.size());
}

Outpost::WindowManager::Point Hud::KeepOnScreen(WindowManager::Point _corner, float _widthUnits, float _screenWidthUnits,
                                                float _screenHeightUnits) noexcept
{
  return Neuron::KeepOnScreen(_corner, _widthUnits, _screenWidthUnits, _screenHeightUnits, WINDOW_KEPT_ON_SCREEN_UNITS, TITLE_BAR_UNITS);
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

DirectX::XMFLOAT2 Hud::Layout::MinimapPixelAt(PlanePosition _point) const noexcept
{
  const float half = mapSizeMeters / 2.0f;
  return {minimap.left + ((_point.xMeters + half) / mapSizeMeters * minimap.width),
          minimap.top + ((half - _point.zMeters) / mapSizeMeters * minimap.height)};
}

std::vector<Outpost::Hud::SideColor> Outpost::Hud::SideColorsOf(const ModelCatalog& _catalog)
{
  std::vector<SideColor> colors;
  for (const PlayerModels& player : _catalog.players)
  {
    if (const ModelSet* set = _catalog.SetForPlayer(player.player); set != nullptr)
      colors.push_back({.player = player.player, .color = set->color});
  }
  return colors;
}

void Outpost::Hud::DrawMinimap(Layout& _layout, const Rect& _square, const Content& _content, float _scale)
{
  _layout.minimap = {_square.left, _square.top, _square.width, _square.height, MAP_COLOR};
  DrawMap(_layout, _content, {}, _scale);
}

Outpost::Hud::Content Outpost::Hud::Describe(const Snapshot& _newest, std::span<const EntityView> _entities,
                                             std::span<const EntityId> _selected, std::optional<StructureKind> _placing,
                                             const Designer* _designer, std::optional<Action> _hovered, std::uint32_t _ticksPerSecond,
                                             std::optional<ActionKind> _armed)
{
  Content content{.ore = _newest.ore,
                  .oreIncomeHundredthsPerSecond = _newest.oreIncomeHundredthsPerSecond,
                  .selection = {},
                  .buttons = {},
                  .hint = {},
                  .status = StatusLinesOf(_newest, _entities),
                  .designer = std::nullopt,
                  .mapSizeMeters = _newest.mapSizeMeters,
                  .marks = {},
                  .player = _newest.player};
  if (_designer != nullptr)
    content.designer = DescribeDesigner(_newest, *_designer, _hovered);
  content.researchNames.reserve(_newest.research.size());
  for (const ResearchTopicView& topic : _newest.research)
    content.researchNames.push_back(topic.nameUtf8);

  const auto sideOf = [&_newest](PlayerId _owner) {
    return !_owner.IsValid() ? Side::Neutral : _owner == _newest.player ? Side::Own : _owner == PIRATES ? Side::Pirate : Side::Enemy;
  };
  if (!_newest.sectors.empty())
  {
    Territory territory{.nodes = static_cast<std::int32_t>(_newest.sectors.size()), .cap = _newest.nodeCap};
    content.sectors.reserve(_newest.sectors.size());
    // While the player places a Relay, which nodes it could claim now, as the world marks them and the Relay's ghost judges
    // them.
    const TerritoryMarks marked = _placing == StructureKind::Relay ? MarkTerritory(_newest, _entities) : TerritoryMarks{};
    for (const SectorView& sector : _newest.sectors)
    {
      // A sector the pirates guard is held by no one, and shown as theirs (ADR-073).
      const Side side = sector.guarded && !sector.holder.IsValid() ? Side::Pirate : sideOf(sector.holder);
      territory.ownNodes += side == Side::Own ? 1 : 0;
      territory.enemyNodes += side == Side::Enemy ? 1 : 0;
      if (side == Side::Enemy)
        territory.enemy = sector.holder;
      content.sectors.push_back({.minXMeters = sector.minXMeters,
                                 .maxXMeters = sector.maxXMeters,
                                 .minZMeters = sector.minZMeters,
                                 .maxZMeters = sector.maxZMeters,
                                 .side = side,
                                 .holder = sector.holder,
                                 .suppressed = sector.suppressed,
                                 .cutOff = sector.cutOff,
                                 .claimable = std::ranges::find(marked.claimable, sector.node) != marked.claimable.end()});
    }
    // Domination's tickets, which both players see (ADR-057).
    for (const TicketsView& tickets : _newest.tickets)
    {
      if (tickets.player == _newest.player)
        territory.ownTickets = tickets.tickets;
      else
      {
        territory.enemyTickets = tickets.tickets;
        territory.enemy = tickets.player;
      }
    }
    // Who the drain takes tickets from, at what a minute, and when they run out: the drains their tickets last, rounded up,
    // times the interval (ADR-057 decision 8). The next drain may be up to an interval away, which the row does not try to show.
    if (territory.ownTickets.has_value() && territory.enemyTickets.has_value() && _newest.drainIntervalSeconds > 0.0 &&
        _newest.drainTicketsPerNodeDifference > 0)
    {
      const std::int32_t behind = std::abs(territory.ownNodes - territory.enemyNodes);
      if (behind == 0)
        territory.drain = "No drain";
      else
      {
        const bool own = territory.ownNodes < territory.enemyNodes;
        const std::int64_t perDrain = std::int64_t{_newest.drainTicketsPerNodeDifference} * behind;
        const std::int64_t left = std::max(0, own ? *territory.ownTickets : *territory.enemyTickets);
        const std::int64_t drains = (left + perDrain - 1) / perDrain;
        const auto seconds = static_cast<std::int64_t>(std::ceil(static_cast<double>(drains) * _newest.drainIntervalSeconds));
        const std::int64_t perMinute = std::llround(static_cast<double>(perDrain) * 60.0 / _newest.drainIntervalSeconds);
        territory.drain =
          std::format("{} -{} a minute{}out in {}:{:02}", own ? "You" : "Enemy", WithThousands(perMinute), DOT, seconds / 60, seconds % 60);
        territory.drainWarns = own;
      }
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
                             .owner = entity.owner,
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
    // The reason a Relay's ghost is red everywhere (ADR-056 decision 11).
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
      // A kind that grows names its level, "Shipyard 01 · L2" (ADR-064 decision 7).
      const bool grows = type != nullptr && !type->levels.empty();
      std::string name = type != nullptr ? type->nameUtf8 : std::string("Structure");
      if (structure->shipyardNumber > 0)
        name += std::format(" {:02}", structure->shipyardNumber);
      if (grows)
        name += std::format("{}L{}", DOT, structure->level);
      content.selection.push_back(std::move(name));
      // One the player only remembers says how long ago it was seen, and that what follows is as it was then
      // (ADR-081 decision 4).
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
      // A Mining Rig's asteroid's Ore left, as far as the player knows it (ADR-035 decision 5).
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
      // Its queue and what it can add to it are its windows' (ADR-031 decision 6; owner, 2026-10-03): its panel says what it
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
        // Each window's button shows the key that opens it too (ADR-068 decision 4), in the command card's first row, the
        // first window in its left cell (ADR-088 decision 2).
        const Button production{
          .label = "Production", .action = {.kind = ActionKind::OpenProduction, .producer = structure->id}, .key = KeyCap(KEY_PRODUCTION)};
        if (structure->structure == StructureKind::CommandStation)
          content.buttons.push_back(production);
        else if (structure->structure == StructureKind::Shipyard)
        {
          content.buttons.push_back(production);
          content.buttons.push_back({.label = "Ship designer",
                                     .action = {.kind = ActionKind::OpenDesigner, .producer = structure->id},
                                     .key = KeyCap(KEY_DESIGNER),
                                     .cell = 1});
        }
        else if (structure->structure == StructureKind::ResearchLab)
        {
          content.buttons.push_back(
            {.label = "Research", .action = {.kind = ActionKind::OpenResearch, .producer = structure->id}, .key = KeyCap(KEY_RESEARCH)});
        }
        // The next level: what it gives, and a button with its time and cost, dim while one is being built or the Ore is
        // short (ADR-064 decision 7).
        const auto next = static_cast<std::size_t>(std::max(0, structure->level - 1));
        if (grows && next < type->levels.size())
        {
          const StructureLevelView& level = type->levels[next];
          const bool upgrading = structure->upgradePermille.has_value();
          // A Shipyard's next level names the hulls it adds, a Research Lab's the tier or the slot it gives, and a Command
          // Station's the nodes it lets the player hold, the Defence guns it adds and its fleet cap
          // (ADR-064 decisions 9 to 11).
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
          // The topics a Research Lab's next level needs that the player has not researched (ADR-064 decision 10).
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
             .progressPermille = structure->upgradePermille,
             .cell = UPGRADE_CELL,
             .wide = true});
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
  bool warships = false;
  bool holding = false;
  bool patrolling = false;
  // The first ship's retreat, whether the others share it, and how many are going back to be repaired (ADR-075 decision 10).
  std::optional<RetreatThreshold> retreat;
  bool mixedRetreat = false;
  size_t retreating = 0;
  // Each ship's own bar, beside its design, to stand in the order of the lines (ADR-088 decision 4).
  std::vector<std::pair<DesignId, ShipBar>> bars;
  bars.reserve(_selected.size());
  for (const EntityId id : _selected)
  {
    const auto ship = std::ranges::find(_entities, id, &EntityView::id);
    if (ship == _entities.end() || ship->kind != EntityKind::Ship)
      continue;
    constructors = constructors || ship->role == ShipRole::Constructor;
    warships = warships || ship->role == ShipRole::Warship;
    holding = holding || ship->standing == StandingOrder::HoldSector;
    patrolling = patrolling || ship->standing == StandingOrder::Patrol;
    mixedRetreat = mixedRetreat || (retreat.has_value() && *retreat != ship->retreat);
    if (!retreat.has_value())
      retreat = ship->retreat;
    retreating += ship->retreating ? 1 : 0;
    hitPoints += ship->hitPointsHundredths;
    maxHitPoints += ship->maxHitPointsHundredths;
    bars.emplace_back(ship->design, ShipBar{.ship = ship->id,
                                            .share = HealthShare(ship->hitPointsHundredths, ship->maxHitPointsHundredths),
                                            .retreating = ship->retreating});
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
  // One ship is titled with its design's name, and ships of one design with their count and its name, once
  // (ADR-088 decision 4). Ships of several designs are counted over a line for each.
  if (ships == 1)
    content.selection.push_back(nameOf(byDesign.front().first));
  else if (byDesign.size() == 1)
    content.selection.push_back(std::format("{} {} {}", ships, TIMES, nameOf(byDesign.front().first)));
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
  // Two to SHIP_BARS_MOST ships have a bar each, design by design in the lines' order; one ship, or more than that, the
  // selection's one bar (ADR-088 decision 4).
  if (ships > 1 && ships <= SHIP_BARS_MOST)
  {
    const auto rank = [&byDesign](const std::pair<DesignId, ShipBar>& _bar)
    { return std::ranges::find(byDesign, _bar.first, &std::pair<DesignId, size_t>::first) - byDesign.begin(); };
    std::ranges::stable_sort(bars, {}, rank);
    content.shipBars.reserve(bars.size());
    for (const auto& [design, bar] : bars)
      content.shipBars.push_back(bar);
  }
  else
    content.selectionHealth = HealthShare(hitPoints, maxHitPoints);
  // A standing order the selection keeps (ADR-059).
  if (holding)
    content.selection.emplace_back("Holding a sector");
  if (patrolling)
    content.selection.emplace_back("On patrol");
  if (retreating > 0)
    content.selection.push_back(ships == 1 ? std::string("Retreating to be repaired")
                                           : std::format("{} retreating to be repaired", retreating));
  // A warship's orders, each with its key's cap in the command card's cell it always takes, the one armed for the next
  // left-click lit until it is given or canceled (ADR-088 decision 2). Each does what its key does, to every selected ship. A
  // map without sectors has none to hold (ADR-059), and its cell stands empty.
  if (warships)
  {
    const auto order = [&_armed](std::string _label, ActionKind _kind, std::uint8_t _key, std::uint8_t _cell) {
      return Button{.label = std::move(_label), .action = {.kind = _kind}, .selected = _armed == _kind, .key = KeyCap(_key), .cell = _cell};
    };
    content.buttons.push_back(order("Attack-move", ActionKind::AttackMove, KEY_ATTACK_MOVE, 0));
    if (!_newest.sectors.empty())
      content.buttons.push_back(order("Hold sector", ActionKind::HoldSector, KEY_HOLD_SECTOR, 1));
    content.buttons.push_back(order("Patrol", ActionKind::Patrol, KEY_PATROL, 2));
    content.buttons.push_back(order("Stop", ActionKind::Stop, KEY_STOP, 3));
  }
  // Constructors offer one Build button, which turns the command card's build mode on, in the place of the buttons, of the
  // structures they build (ADR-088 decision 2).
  if (constructors)
    content.buttons.push_back({.label = "Build", .action = {.kind = ActionKind::OpenBuild}, .key = KeyCap(KEY_BUILD), .cell = BUILD_CELL});
  // The selection's retreat, under the rest, the first ship's and whether the others differ (ADR-088 decision 3).
  if (retreat.has_value())
    content.retreat = RetreatChoice{.setting = *retreat, .mixed = mixedRetreat};
  return content;
}

Hud::BuildPanel Hud::DescribeBuild(const Snapshot& _newest, std::span<const EntityView> _entities, std::span<const EntityId> _selected)
{
  const bool constructors =
    std::ranges::any_of(_selected,
                        [&_entities](EntityId _id)
                        {
                          const auto ship = std::ranges::find(_entities, _id, &EntityView::id);
                          return ship != _entities.end() && ship->kind == EntityKind::Ship && ship->role == ShipRole::Constructor;
                        });
  const bool hasLab = std::ranges::any_of(_entities,
                                          [&_newest](const EntityView& _entity) {
                                            return _entity.kind == EntityKind::Structure &&
                                                   _entity.structure == StructureKind::ResearchLab && _entity.owner == _newest.player;
                                          });
  const bool capReached = AtNodeCap(_newest.sectors, _newest.entities, _newest.player, _newest.nodeCap);
  // Every structure Constructors build (Game §structures.building), one Research Lab a player, each in the command card's
  // cell its place among them gives it, so that a map without sectors leaves the Relay's empty (ADR-088 decision 2). The card
  // holds as many as it has cells.
  BuildPanel panel;
  panel.structures.reserve(CARD_CELLS);
  std::uint8_t cell = 0;
  for (const StructureTypeView& type : _newest.structureTypes)
  {
    if (!type.buildable || cell == CARD_CELLS)
      continue;
    const std::uint8_t at = cell++;
    if (!IsOffered(_newest, type))
      continue;
    const bool secondLab = type.structure == StructureKind::ResearchLab && hasLab;
    // A Relay past the Command Station's cap waits for the station's next level (ADR-056 decision 11).
    const bool capped = type.structure == StructureKind::Relay && capReached;
    panel.structures.push_back({.label = std::format("{}|{}", type.nameUtf8, type.cost),
                                .action = {.kind = ActionKind::Build, .structure = type.structure},
                                .enabled = constructors && !secondLab && !capped && _newest.ore >= type.cost,
                                .note = secondLab ? std::string("ONE PER PLAYER")
                                        : capped  ? std::string("NODE CAP")
                                                  : std::string(),
                                .cell = at});
  }
  return panel;
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

  // A Constructor at the Command Station; each saved design at a Shipyard, with its abbreviation (Game §ships.designs.names).
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
    // A design whose hull is above the Shipyard's level is dim, and says the level it needs (ADR-064 decision 9).
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
  // The player's Research Lab, of which it has one at most (ADR-016 decision 4).
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
    // A second topic researched beside the first shows how far it has come too (ADR-064 decision 10).
    if (found->secondJobPermille > 0 && panel.queue.size() > 1)
      panel.queue[1] = {.name = std::move(panel.queue[1].name), .front = true, .permille = found->secondJobPermille};
  }

  // The level of the Research Lab that opens each tier (ADR-064 decision 10).
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
  // has not opened, is dim, and says what it needs (ADR-017 decision 9; ADR-033 decision 5).
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
  // Tier by tier, each in the tuning data's order (ADR-033 decision 5).
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

static_assert(Hud::FACE_UNITS.size() == static_cast<std::size_t>(Hud::Typeface::Plate) + 1, "a size for every typeface");

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
    {.families = condensed,
     .weight = 700,
     .stretch = Neuron::FontStretch::SemiCondensed,
     .emUnits = FaceUnits(Typeface::Plate),
     .haloEms = PLATE_HALO_EMS},
  };
}

const Neuron::Theme& Hud::Theme()
{
  return GameTheme();
}

std::vector<Neuron::SpriteDesc> Hud::Sprites()
{
  return {
    {.shape = Neuron::SpriteShape::Gem, .sizeUnits = 12.0f},
    {.shape = Neuron::SpriteShape::Checkbox, .sizeUnits = 10.0f},
    {.shape = Neuron::SpriteShape::CornerBracket, .sizeUnits = 12.0f},
    {.shape = Neuron::SpriteShape::Ring, .sizeUnits = PLATE_WIDTH_UNITS},
    {.shape = Neuron::SpriteShape::Disc, .sizeUnits = PLATE_DISC_UNITS},
    {.shape = Neuron::SpriteShape::Chevron, .sizeUnits = PLATE_INSIGNIA_UNITS},
    {.shape = Neuron::SpriteShape::TwoChevrons, .sizeUnits = PLATE_INSIGNIA_UNITS},
    {.shape = Neuron::SpriteShape::ThreeChevrons, .sizeUnits = PLATE_INSIGNIA_UNITS},
    {.shape = Neuron::SpriteShape::Plus, .sizeUnits = PLATE_INSIGNIA_UNITS},
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
  // In a match, the HUD's bottom row at its widest: the minimap with its margin and the gap after it, the selection panel
  // at its widest, the gap before the command card, the card, and the margin at the right (ADR-088 decision 2). It is
  // as wide as a 16:9 screen at 150%, so that a 16:9, 16:10 or 3:2 screen holds it at 150% and no further.
  constexpr float BOTTOM_ROW_UNITS = MARGIN + MINIMAP_SIZE + PANEL_GAP + SELECTION_PANEL_WIDTH + PANEL_GAP + COMMAND_CARD_WIDTH + MARGIN;
  static_assert(BOTTOM_ROW_UNITS * 1.5f == REFERENCE_WIDTH_UNITS);
  if (_newest != nullptr && BOTTOM_ROW_UNITS > screenWidthUnits)
    return false;
  // Each window its row counts, at its least, as short as its scroll views let it be (ADR-100), the Controls window on the
  // menu too and the others only in a match.
  for (const WindowRow& row : WINDOWS)
  {
    if (!row.fits || (_newest == nullptr && row.kind != WindowKind::Controls))
      continue;
    const WindowSize least = LeastOf(row.kind, _newest);
    if (least.widthUnits + (2.0f * MARGIN) > screenWidthUnits || least.heightUnits + (2.0f * MARGIN) > screenHeightUnits)
      return false;
  }
  return true;
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
                     std::span<const PlanePosition> _view, const WindowManager* _windows, float _factor, std::optional<Action> _hovered,
                     Neuron::Interaction* _interaction)
{
  // The windows on the canvas name their widgets afresh each frame (ADR-100).
  Neuron::Interaction unpointed;
  Neuron::Interaction& interaction = _interaction != nullptr ? *_interaction : unpointed;
  interaction.BeginBuild();
  const float scale = Scale(_widthPixels, _heightPixels, _factor);
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  Layout layout;
  layout.fontPixels = FONT_UNITS * scale;

  // Every panel of the HUD is in the windows' look (ADR-043), and is laid out in reference units from its own top-left
  // corner, as a window's content is.
  const float screenWidthUnits = width / scale;
  const float screenHeightUnits = height / scale;
  // First, under every panel and window, the bars over the entities: each a back of its side's color with its fill from
  // the left, the health bar over the build bar, the pair centered over its foot, a little above it, and set to whole
  // pixels, so that it keeps its size as it moves (ADR-088 decision 5). A pair wholly off the screen is left out.
  {
    const float barScale = Scale(_widthPixels, _heightPixels);
    const float barWidth = std::round(ENTITY_BAR_WIDTH_UNITS * barScale);
    const float barHeight = std::max(std::round(ENTITY_BAR_HEIGHT_UNITS * barScale), 1.0f);
    const float barGap = std::round(ENTITY_BAR_GAP_UNITS * barScale);
    const float barLift = std::round(ENTITY_BAR_LIFT_UNITS * barScale);
    for (const EntityBar& bar : _content.entityBars)
    {
      const float left = std::round(bar.footPixels.x - (barWidth / 2.0f));
      float bottom = std::round(bar.footPixels.y) - barLift;
      const float count = (bar.health.has_value() ? 1.0f : 0.0f) + (bar.built.has_value() ? 1.0f : 0.0f);
      const float stackHeight = (count * barHeight) + (std::max(count - 1.0f, 0.0f) * barGap);
      if (count == 0.0f || left + barWidth <= 0.0f || left >= width || bottom <= 0.0f || bottom - stackHeight >= height)
        continue;
      const auto addBar = [&layout, &bar, &bottom, left, barWidth, barHeight, barGap](float _share, const DirectX::XMFLOAT4& _fill)
      {
        const float top = bottom - barHeight;
        layout.bars.push_back({.left = left, .top = top, .width = barWidth, .height = barHeight, .color = bar.back});
        const float share = std::clamp(_share, 0.0f, 1.0f);
        if (share > 0.0f)
          layout.bars.push_back({.left = left, .top = top, .width = barWidth * share, .height = barHeight, .color = _fill});
        bottom = top - barGap;
      };
      if (bar.built.has_value())
        addBar(bar.built.value_or(0.0f), BUILD_BAR_COLOR);
      if (bar.health.has_value())
      {
        const float share = bar.health.value_or(0.0f);
        addBar(share, HealthColor(share));
      }
    }
  }
  // Then the plates over the fleets, at the interface's scale, which their count is drawn at (ADR-102 decision 2).
  LayPlates(layout, _content, _metrics, scale, width, height);

  // The HUD's panels on the canvas (ADR-100 decision 7), and the hint and the banner on a canvas of their own, which stands
  // over the windows (ADR-015 decision 4; ADR-031 decision 4): the front window finishes it after itself and before an
  // open drop-down's list, or, with no window open, it is finished after the HUD.
  Neuron::Canvas front(GameTheme(), _metrics, interaction, screenWidthUnits, screenHeightUnits);
  {
    Neuron::Canvas hud(GameTheme(), _metrics, interaction, screenWidthUnits, screenHeightUnits);
    LayHud({.canvas = &hud,
            .front = &front,
            .layout = &layout,
            .metrics = &_metrics,
            .hovered = _hovered,
            .screenWidthUnits = screenWidthUnits,
            .screenHeightUnits = screenHeightUnits},
           _content, _view, scale);
  }

  // The floating windows (ADR-031), over everything else, back to front: those the manager has open, where it left them,
  // or, without a manager, every window the content has at its default place.
  std::vector<WindowKind> backToFront{WindowKind::Production, WindowKind::Research, WindowKind::Designer, WindowKind::Controls,
                                      WindowKind::Orders,     WindowKind::Away,     WindowKind::Leave};
  if (_windows != nullptr)
    backToFront.assign(_windows->FrontToBack().rbegin(), _windows->FrontToBack().rend());
  // An open drop-down's list stands over every window (ADR-100 decision 6), so it stays open only while the orders window,
  // the one that holds drop-downs (ADR-080 decision 9), stands in front of every other.
  if (backToFront.empty() || backToFront.back() != WindowKind::Orders)
    interaction.CloseDropDown();
  const auto place = [&](WindowKind _kind, WindowManager::Point _default, float _widthUnits)
  {
    const std::optional<WindowManager::Point> moved = _windows != nullptr ? _windows->PositionOf(_kind) : std::nullopt;
    return KeepOnScreen(moved.value_or(_default), _widthUnits, screenWidthUnits, screenHeightUnits);
  };
  // A window on the canvas stands where the player left it, kept on the screen, or else at its row's first place.
  const auto placed = [&](WindowKind _kind, float _widthUnits)
  {
    if (const std::optional<WindowManager::Point> moved = _windows != nullptr ? _windows->PositionOf(_kind) : std::nullopt)
      return WindowPlace{.corner = KeepOnScreen(*moved, _widthUnits, screenWidthUnits, screenHeightUnits)};
    return FirstPlaceOf(RowOf(_kind).first);
  };
  WindowBench bench{.layout = &layout,
                    .metrics = &_metrics,
                    .interaction = &interaction,
                    .screenWidthUnits = screenWidthUnits,
                    .screenHeightUnits = screenHeightUnits};
  // The front window: the last of those whose content there is to lay out.
  const auto laid = [&_content](WindowKind _kind)
  {
    switch (_kind)
    {
    case WindowKind::Designer:
      return _content.designer.has_value();
    case WindowKind::Research:
      return _content.laboratory.has_value();
    case WindowKind::Production:
      return _content.production.has_value();
    case WindowKind::Controls:
      return _content.controls;
    case WindowKind::Orders:
      return _content.orders.has_value();
    case WindowKind::Away:
      return _content.away.has_value();
    case WindowKind::Leave:
      return _content.leave;
    }
    return false;
  };
  const auto frontWindow = std::find_if(backToFront.rbegin(), backToFront.rend(), laid);
  // Production and research stand at first in the slots beside the column under the Ore, each in the one it took as it
  // opened (ADR-031 decision 2; ADR-066 decision 3): the first a gap clear of the column, the second beside where the other
  // window stands in the first, each wholly on the screen, so that the second stands over the first where the screen is
  // too narrow for both. Without a manager, production takes the first and research the second.
  const float besideColumnUnits = MARGIN + ColumnUnitsOf(_content, _metrics) + PANEL_GAP;
  const auto slotted = [&](WindowKind _kind, std::size_t _slot)
  {
    const std::size_t slot = _windows != nullptr ? _windows->SlotOf(_kind).value_or(_slot) : _slot;
    const bool production = _kind == WindowKind::Production;
    const float other = production ? RESEARCH_WINDOW_WIDTH : PRODUCTION_WINDOW_WIDTH;
    const float widthUnits = production ? PRODUCTION_WINDOW_WIDTH : RESEARCH_WINDOW_WIDTH;
    const float left = slot == 0 ? besideColumnUnits : besideColumnUnits + other + MARGIN;
    return WindowManager::Point{.xUnits = std::max(MARGIN, std::min(left, screenWidthUnits - MARGIN - widthUnits)), .yUnits = WINDOWS_TOP};
  };
  for (const WindowKind kind : backToFront)
  {
    bench.front = frontWindow != backToFront.rend() && kind == *frontWindow ? &front : nullptr;
    // The designer, at first in the top-right corner.
    if (kind == WindowKind::Designer && _content.designer.has_value())
      LayDesigner(bench, *_content.designer, placed(kind, DESIGNER_WIDTH));
    // Production and research, at first side by side beside the column under the Ore.
    else if (kind == WindowKind::Production && _content.production.has_value())
      LayProduction(bench, *_content.production, {.corner = place(kind, slotted(kind, 0), PRODUCTION_WINDOW_WIDTH)});
    else if (kind == WindowKind::Research && _content.laboratory.has_value())
      LayResearch(bench, *_content.laboratory, {.corner = place(kind, slotted(kind, 1), RESEARCH_WINDOW_WIDTH)});
    // The windows on the canvas, each at first where its row puts it (ADR-100 decision 8): the Controls, away and leave
    // windows in the middle, and the orders window at the right under the Ore's line.
    else if (kind == WindowKind::Controls && _content.controls)
      LayControls(bench, placed(kind, CONTROLS_WINDOW_WIDTH));
    else if (kind == WindowKind::Orders && _content.orders.has_value())
      LayOrders(bench, *_content.orders, placed(kind, ORDERS_WINDOW_WIDTH));
    else if (kind == WindowKind::Away && _content.away.has_value())
      LayAway(bench, *_content.away, placed(kind, AWAY_WINDOW_WIDTH));
    else if (kind == WindowKind::Leave && _content.leave)
      LayLeave(bench, placed(kind, LEAVE_WINDOW_WIDTH));
  }
  if (frontWindow == backToFront.rend())
  {
    const std::size_t firstRegion = layout.regions.size();
    front.Finish(layout);
    PlacePresses(layout, firstRegion);
  }
  return layout;
}
