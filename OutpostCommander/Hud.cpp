#include "pch.h"

#include "Hud.h"

#include "Victory.h"

#include "FixedPoint.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace Outpost
{

namespace
{

// ── The widget ids, named rather than numbered (Hud.h says why) ──────────────────────────────
//
// Each panel numbers from its own base, so that a press armed in one frame finds the same control
// in the next however the panel was rebuilt around it.
constexpr std::uint32_t ID_TAB_BASE = 100;            ///< + the tab
constexpr std::uint32_t ID_STRUCTURE_BASE = 200;      ///< + the structure row
constexpr std::uint32_t ID_DESIGN_LIST = 300;         ///< The produce tab's designs
constexpr std::uint32_t ID_RESEARCH_LIST = 310;       ///< The research tab's items
constexpr std::uint32_t ID_DESIGN_CHASSIS_BASE = 400; ///< + the chassis row
constexpr std::uint32_t ID_DESIGN_DRIVE_BASE = 430;   ///< + the drive row
constexpr std::uint32_t ID_DESIGN_MODULE_BASE = 460;  ///< + the module row
constexpr std::uint32_t ID_DESIGN_SAVE = 499;
constexpr std::uint32_t ID_DESIGN_NAME = 498;
constexpr std::uint32_t ID_PORTRAIT_BASE = 600; ///< + the index into the selection
constexpr std::uint32_t ID_ORDER_BASE = 700;    ///< + the OrderKind
constexpr std::uint32_t ID_STANCE_BASE = 800;   ///< + the axis times STANCE_IDS_PER_ROW + the option
/// How far apart the stance rows' ids are: more than the widest row has options, so that two rows
/// can never name the same id, and the division back in OnEvent is the same number.
constexpr std::uint32_t STANCE_IDS_PER_ROW = 8;

/// The minimap's own image is opaque: every pixel of it is already the colour it should be.
constexpr std::uint32_t MINIMAP_ALPHA = 255;
constexpr std::uint32_t ID_DECORATION = 0; ///< A readout: never hit, never pressed

/// §7's own captions, in tab order.
constexpr std::array<const char*, HUD_TAB_COUNT> TAB_NAMES = {"CONSTRUCT", "PRODUCE", "RESEARCH", "DESIGN", "MATCH"};

/// §9.1's five primary orders, in the order the row of buttons lists them, and the sixth that is
/// listed and DISABLED because M1 has neither a repair bay nor a repair module (S10).
constexpr std::array<OrderKind, 6> PRIMARY_ORDERS = {OrderKind::Move,  OrderKind::AttackMove, OrderKind::Patrol,
                                                     OrderKind::Guard, OrderKind::Stop,       OrderKind::ReturnToRepair};
constexpr std::array<const char*, 6> PRIMARY_ORDER_ICONS = {"OrderMove",  "OrderAttackMove", "OrderPatrol",
                                                            "OrderGuard", "OrderStop",       "OrderReturnToRepair"};

/// §9.1's four stance rows. The options of each are the enumerators of its axis, in order, and the
/// icon names are Interface.json's.
struct StanceRow
{
  StanceAxis axis;
  const char* name;
  std::uint8_t options;
  std::array<const char*, 3> icons;
  std::array<bool, 3> enabled; ///< Retreat is Never only in M1, because nothing repairs
};

constexpr std::array<StanceRow, 4> STANCE_ROWS = {{
  {StanceAxis::Fire, "FIRE", 3, {"StanceFireAtWill", "StanceReturnFire", "StanceHoldFire"}, {true, true, true}},
  {StanceAxis::Range, "RANGE", 2, {"StanceRangeOptimal", "StanceRangeLong", ""}, {true, true, false}},
  {StanceAxis::Retreat, "RETREAT", 3, {"StanceRetreat50", "StanceRetreat25", "StanceRetreatNever"}, {false, false, true}},
  {StanceAxis::Movement, "MOVE", 2, {"StancePursue", "StanceHoldPosition", ""}, {true, true, false}},
}};

[[nodiscard]] std::int32_t IconOf(const ContentTree& _content, std::string_view _name) noexcept
{
  const std::uint32_t cell = _content.ui.icons.Find(_name);
  return cell == NO_ICON ? -1 : static_cast<std::int32_t>(cell);
}

/// Power as the readouts print it: hundredths into whole units with a thousands separator, which is
/// §9.3's own "POWER 1,240 / 2,000".
[[nodiscard]] std::string Power(std::int64_t _hundredths)
{
  const std::int64_t whole = _hundredths / 100;
  std::string digits = std::to_string(whole < 0 ? -whole : whole);
  for (std::size_t at = digits.size(); at > 3; at -= 3)
  {
    digits.insert(at - 3, ",");
  }
  return (whole < 0 ? "-" : "") + digits;
}

/// mm:ss from a tick count (§9.4).
[[nodiscard]] std::string Clock(std::uint32_t _ticks)
{
  const std::uint32_t seconds = _ticks / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND);
  char text[16];
  std::snprintf(text, sizeof text, "%02u:%02u", seconds / 60, seconds % 60);
  return text;
}

[[nodiscard]] Neuron::UiWidget Label(const Neuron::UiRect& _rect, std::string _text)
{
  Neuron::UiWidget widget{};
  widget.kind = Neuron::UiWidgetKind::Label;
  widget.rect = _rect;
  widget.id = ID_DECORATION;
  widget.text = std::move(_text);
  return widget;
}

[[nodiscard]] Neuron::UiWidget Button(const Neuron::UiRect& _rect, std::uint32_t _id, std::string _text, bool _enabled = true)
{
  Neuron::UiWidget widget{};
  widget.kind = Neuron::UiWidgetKind::Button;
  widget.rect = _rect;
  widget.id = _id;
  widget.text = std::move(_text);
  widget.enabled = _enabled;
  return widget;
}

[[nodiscard]] Neuron::UiWidget Toggle(const Neuron::UiRect& _rect, std::uint32_t _id, std::string _text, bool _on, bool _enabled = true)
{
  Neuron::UiWidget widget = Button(_rect, _id, std::move(_text), _enabled);
  widget.kind = Neuron::UiWidgetKind::Toggle;
  widget.on = _on;
  return widget;
}

[[nodiscard]] Neuron::UiWidget Icon(const Neuron::UiRect& _rect, std::int32_t _cell)
{
  Neuron::UiWidget widget{};
  widget.kind = Neuron::UiWidgetKind::Icon;
  widget.rect = _rect;
  widget.id = ID_DECORATION;
  widget.value = _cell;
  return widget;
}

[[nodiscard]] Neuron::UiWidget Bar(const Neuron::UiRect& _rect, std::int32_t _value, std::int32_t _maximum, Neuron::UiBarStyle _style)
{
  Neuron::UiWidget widget{};
  widget.kind = Neuron::UiWidgetKind::ProgressBar;
  widget.rect = _rect;
  widget.id = ID_DECORATION;
  widget.value = _value;
  widget.maximum = _maximum;
  widget.barStyle = _style;
  return widget;
}

/// Whether the seat has researched the row a content id names. An empty id is a row available from
/// the first tick (Content/ComponentDesc.h), which is every M1 row until research runs.
[[nodiscard]] bool Unlocked(const ContentTree& _content, std::uint64_t _complete, const std::string& _unlockedBy)
{
  if (_unlockedBy.empty())
  {
    return true;
  }
  for (std::size_t row = 0; row < _content.research.size() && row < RESEARCH_MASK_BITS; ++row)
  {
    if (_content.research[row].id == _unlockedBy)
    {
      return (_complete & (std::uint64_t{1} << row)) != 0;
    }
  }
  return false; // It names a row the tables do not carry; C1's validator refuses that content.
}

// ── What the enumerations are called on the screen ───────────────────────────────────────────
//
// The strings are here rather than in the content tables for Operator.cpp's reason: §7's own note
// hands the text tables to M1's later work, and what matters now is that a panel says which thing
// in words. They are UPPER CASE because every caption in §3's mock-ups is.

[[nodiscard]] const char* NameOfDesignFault(DesignFault _fault) noexcept
{
  switch (_fault)
  {
  case DesignFault::UnknownChassis:
    return "NO CHASSIS";
  case DesignFault::UnknownDrive:
    return "NO DRIVE";
  case DesignFault::UnknownModule:
    return "UNKNOWN MODULE";
  case DesignFault::NoModules:
    return "NO MODULES";
  case DesignFault::TooManyModules:
    return "TOO MANY MODULES";
  case DesignFault::ModuleRefusesChassis:
    return "THIS CHASSIS REFUSES THAT MODULE";
  case DesignFault::None:
    break;
  }
  return "";
}

[[nodiscard]] const char* NameOfSizeClass(SizeClass _size) noexcept
{
  switch (_size)
  {
  case SizeClass::Small:
    return "SMALL";
  case SizeClass::Medium:
    return "MEDIUM";
  case SizeClass::Large:
    return "LARGE";
  case SizeClass::Frontier:
    return "FRONTIER";
  }
  return "";
}

[[nodiscard]] const char* NameOfVictory(VictoryCondition _victory) noexcept
{
  switch (_victory)
  {
  case VictoryCondition::Annihilation:
    return "ANNIHILATION";
  case VictoryCondition::Dominance:
    return "DOMINANCE";
  case VictoryCondition::Survival:
    return "SURVIVAL";
  }
  return "";
}

[[nodiscard]] const char* NameOfPowerLevel(PowerLevel _level) noexcept
{
  switch (_level)
  {
  case PowerLevel::Low:
    return "LOW";
  case PowerLevel::Medium:
    return "MEDIUM";
  case PowerLevel::High:
    return "HIGH";
  }
  return "";
}

[[nodiscard]] const char* NameOfSeatKind(SeatKind _kind) noexcept
{
  switch (_kind)
  {
  case SeatKind::Empty:
    return "EMPTY";
  case SeatKind::Human:
    return "HUMAN";
  case SeatKind::Ai:
    return "AI";
  }
  return "";
}

[[nodiscard]] const char* NameOfPrimaryOrder(PrimaryOrder _order) noexcept
{
  switch (_order)
  {
  case PrimaryOrder::Stop:
    return "HOLDING";
  case PrimaryOrder::Move:
    return "MOVING";
  case PrimaryOrder::AttackMove:
    return "ATTACK-MOVING";
  case PrimaryOrder::Attack:
    return "ATTACKING";
  case PrimaryOrder::Patrol:
    return "PATROLLING";
  case PrimaryOrder::Guard:
    return "GUARDING";
  case PrimaryOrder::ReturnToRepair:
    return "RETURNING TO REPAIR";
  }
  return "";
}

[[nodiscard]] const char* NameOfStructureRole(StructureRole _role) noexcept
{
  switch (_role)
  {
  case StructureRole::CommandPost:
    return "COMMAND POST";
  case StructureRole::Extractor:
    return "EXTRACTOR";
  case StructureRole::Generator:
    return "GENERATOR";
  case StructureRole::Factory:
    return "FACTORY";
  case StructureRole::ResearchLab:
    return "RESEARCH LAB";
  case StructureRole::RepairBay:
    return "REPAIR BAY";
  case StructureRole::SensorTower:
    return "SENSOR TOWER";
  case StructureRole::Wall:
    return "WALL";
  case StructureRole::Hardpoint:
    return "HARDPOINT";
  case StructureRole::Tower:
    return "TOWER";
  case StructureRole::Bunker:
    return "BUNKER";
  case StructureRole::Uplink:
    return "UPLINK";
  }
  return "";
}

/// Which option of an axis a device's stances carry, so that §9.1's four rows can be built from one
/// subscript rather than four switches.
[[nodiscard]] std::uint8_t StanceValueOf(const OrderAndStances& _stances, StanceAxis _axis) noexcept
{
  switch (_axis)
  {
  case StanceAxis::Fire:
    return static_cast<std::uint8_t>(_stances.fire);
  case StanceAxis::Range:
    return static_cast<std::uint8_t>(_stances.range);
  case StanceAxis::Retreat:
    return static_cast<std::uint8_t>(_stances.retreat);
  case StanceAxis::Movement:
    return static_cast<std::uint8_t>(_stances.movement);
  }
  return 0;
}

/// The design a device carries, or null while the record has not arrived. A client is told an
/// enemy's design with the first device of it that it sees (Replica/DesignStore.h), and for a frame
/// or two after a rejoin it holds none - which §8 draws as DESIGN UNKNOWN rather than as a guess.
[[nodiscard]] const DesignState* DesignOf(const Replica& _replica, const DeviceState& _device) noexcept
{
  return _replica.Designs().Find(_device.seat, _device.design);
}

/// The component rows of a design as three lines of text (§8's "the chassis, drive and modules as
/// three lines"). The modules are one line, comma separated, because a design carries up to eight
/// and eight lines would not fit the panel.
void DescribeDesign(const ContentTree& _content, const DesignState& _design, std::string& _outChassis, std::string& _outDrive,
                    std::string& _outModules)
{
  _outChassis = _design.chassis < _content.components.chassis.size() ? _content.components.chassis[_design.chassis].name : "?";
  _outDrive = _design.drive < _content.components.drives.size() ? _content.components.drives[_design.drive].name : "?";
  _outModules.clear();
  for (std::uint8_t mount = 0; mount < _design.moduleCount && mount < MAX_MOUNTS; ++mount)
  {
    const std::uint32_t row = _design.modules[mount];
    if (row >= _content.components.modules.size())
    {
      continue;
    }
    _outModules += _outModules.empty() ? "" : ", ";
    _outModules += _content.components.modules[row].name;
  }
}

/// Which armed order a primary-order button arms, or None for one that is issued outright.
[[nodiscard]] ArmedOrder ArmedFor(OrderKind _kind) noexcept
{
  switch (_kind)
  {
  case OrderKind::Move:
    return ArmedOrder::Move;
  case OrderKind::AttackMove:
    return ArmedOrder::AttackMove;
  case OrderKind::Patrol:
    return ArmedOrder::Patrol;
  default:
    break;
  }
  return ArmedOrder::None;
}

/// Whether an id is in the frame's selection, for the minimap's accent (§9.2's fourth layer).
[[nodiscard]] bool SelectedHere(std::span<const SelectedObject> _selected, std::uint32_t _id) noexcept
{
  for (const SelectedObject& object : _selected)
  {
    if (object.id == _id)
    {
      return true;
    }
  }
  return false;
}

/// Where a readout's one line of 16-pixel text sits in its rectangle (§9.3, §9.4). It is not the
/// client area: a readout is 40 pixels tall and §3's chrome would leave eight of them, so the line
/// is centred in the whole rectangle and the title strip behind it carries no title.
[[nodiscard]] Neuron::UiRect ReadoutLineOf(const Neuron::UiRect& _panel) noexcept
{
  return Neuron::UiRect{_panel.x + Neuron::PANEL_INSET_PIXELS, _panel.y + ((_panel.height - 16) / 2),
                        _panel.width - (2 * Neuron::PANEL_INSET_PIXELS), 16};
}

/// The statistics of a design the replica was sent, for the health bar's maximum. False when the
/// design names a row the content tables do not carry, which is a client and a host disagreeing
/// about their content and is already refused at the join by the content hash.
[[nodiscard]] bool StatsOf(const ContentTree& _content, const DesignState& _design, DesignStats& _outStats)
{
  DeviceDesign design{};
  design.chassis = _design.chassis;
  design.drive = _design.drive;
  design.modules = _design.modules;
  design.moduleCount = _design.moduleCount;
  const ClassUpgrades none{};
  // THE BASE STATISTICS AND NOT THE OWNER'S. A device's hit points are raised by its seat's
  // research (Content/DesignStats.h's ClassUpgrades), and a client is sent neither an enemy seat's
  // research nor the effects of its own as upgrades - only the mask of what IT has completed. A
  // maximum that is too low would draw a full bar as overfull, so the bar's own maximum is taken
  // from the device's current hit points where those are higher, in the caller.
  return DeriveDesignStats(_content, RecipeFor(_content, design), none, _outStats) == DesignFault::None;
}

/// The icon a portrait draws: the design's first module, which is what tells a gun from a builder
/// at 48 pixels. A design with no module the tables carry draws none.
[[nodiscard]] std::string PortraitIconOf(const ContentTree& _content, const DesignState* _design)
{
  if (_design == nullptr)
  {
    return "";
  }
  for (std::uint8_t mount = 0; mount < _design->moduleCount && mount < MAX_MOUNTS; ++mount)
  {
    if (_design->modules[mount] < _content.components.modules.size())
    {
      return "Module" + _content.components.modules[_design->modules[mount]].id;
    }
  }
  return "";
}

/// A caption cut to the pixels it has. THE FONT IS A GRID, so the width is the character count
/// times Neuron::GLYPH_ADVANCE_PIXELS and this is a division rather than a measure - but the cut
/// itself is not optional: Design/Interface.md §7.3 asks a research row for "the name, the cost,
/// the time in seconds, and the effect in one line from the item's description field", and this
/// tree's longest name and description come to eighty characters before the cost and the time are
/// added. At sixteen pixels an advance that is 1,536 across a panel 752 wide, drawn from the
/// panel's own x and straight over the orders panel beside it. AppendText clips nothing, so a
/// caption that does not fit is not one that wraps; it is one that runs off the screen.
///
/// An ellipsis and not a bare cut, because a line that simply stops reads as content that is
/// missing rather than as content that is longer than the panel.
[[nodiscard]] std::string Fit(std::string _text, std::int32_t _pixels)
{
  const std::size_t characters = _pixels <= 0 ? 0u : static_cast<std::size_t>(_pixels / Neuron::GLYPH_ADVANCE_PIXELS);
  if (_text.size() <= characters)
  {
    return _text;
  }
  if (characters <= 3)
  {
    _text.resize(characters);
    return _text;
  }
  _text.resize(characters - 3);
  return _text + "...";
}

/// One row of a list, laid out down a panel's client area.
[[nodiscard]] Neuron::UiRect RowAt(const Neuron::UiRect& _area, std::int32_t _index, std::int32_t _height)
{
  return Neuron::RowIn(_area, _index, _height, 2);
}

} // namespace

Hud::Hud()
  : m_command(COMMAND_PANEL_RECT, "COMMAND"),
    m_selection(SELECTION_PANEL_RECT, "SELECTION"),
    m_orders(ORDERS_PANEL_RECT, "ORDERS"),
    m_minimapPanel(MINIMAP_PANEL_RECT, "MAP"),
    m_power(POWER_READOUT_RECT, ""),
    m_matchState(MATCH_READOUT_RECT, "")
{
  m_minimap.assign(Neuron::MINIMAP_BYTES, 0);
}

void Hud::Register(Neuron::UiInputSink& _sink)
{
  // The order they are added is the order they are OFFERED events in, last first
  // (Client/UiInputSink.h), and none of these is modal, so the order between them is only about
  // which one a click on an overlap reaches. They do not overlap: §2's rectangles tile the strip.
  _sink.AddPanel(&m_power);
  _sink.AddPanel(&m_matchState);
  _sink.AddPanel(&m_minimapPanel);
  _sink.AddPanel(&m_selection);
  _sink.AddPanel(&m_command);
  _sink.AddPanel(&m_orders);
}

void Hud::SetVisible(bool _visible) noexcept
{
  for (Neuron::UiPanel* panel : {&m_power, &m_matchState, &m_minimapPanel, &m_selection, &m_command, &m_orders})
  {
    panel->SetVisible(_visible);
  }
}

void Hud::BlockedBy(std::vector<PickBox>& _outBoxes) const
{
  _outBoxes.clear();
  for (const Neuron::UiPanel* panel : {&m_power, &m_matchState, &m_minimapPanel, &m_selection, &m_command, &m_orders})
  {
    if (!panel->Visible())
    {
      continue;
    }
    const Neuron::UiRect& rect = panel->Rect();
    _outBoxes.push_back(PickBox{rect.x, rect.y, rect.Right(), rect.Bottom()});
  }
}

Neuron::MinimapPoint Hud::MinimapPointAt(std::int32_t _x, std::int32_t _y) const noexcept
{
  if (!MINIMAP_IMAGE_RECT.Contains(_x, _y))
  {
    return {}; // The panel's border is not the map; §9.2's click is on the 256 square alone.
  }
  return Neuron::WorldOfMinimap(_x - MINIMAP_IMAGE_RECT.x, _y - MINIMAP_IMAGE_RECT.y, m_minimapCells,
                                static_cast<float>(Neuron::WORLD_UNITS_PER_CELL));
}

bool Hud::TakeTabKey(std::uint8_t _key) noexcept
{
  if (_key < '1' || _key > '5')
  {
    return false;
  }
  m_tab = static_cast<HudTab>(_key - '1');
  return true;
}

void Hud::Refresh(const Frame& _frame)
{
  RefreshCommand(_frame);
  RefreshSelection(_frame);
  RefreshOrders(_frame);
  RefreshReadouts(_frame);
  RefreshMinimap(_frame);
}

void Hud::Build(const ChromePalette& _palette, const Neuron::IconAtlas& _icons, std::vector<Neuron::UiQuad>& _outQuads) const
{
  // BACK TO FRONT, and the minimap's image inside its panel's chrome: the panel is drawn first so
  // that the map sits on it rather than under it.
  Neuron::BuildPanelQuads(m_minimapPanel, _palette, _icons, _outQuads);
  if (m_minimapPanel.Visible())
  {
    // BuildPanelQuads appends nothing for a panel that is off, and the map is not one of its
    // widgets - it is a quad of its own - so the visibility it already respects is asked here too.
    Neuron::AppendMinimap(MINIMAP_IMAGE_RECT, MINIMAP_ALPHA, _outQuads);
  }
  for (const Neuron::UiPanel* panel : {&m_power, &m_matchState, &m_selection, &m_command, &m_orders})
  {
    Neuron::BuildPanelQuads(*panel, _palette, _icons, _outQuads);
  }
}

void Hud::RefreshCommand(const Frame& _frame)
{
  m_command.Reset();
  // THE FIVE TABS ALONG THE TITLE STRIP (§7), the active one a toggle that is down. A toggle and
  // not a button, because §7 draws the active tab filled and a button has no state to draw it from.
  const Neuron::UiRect strip = Neuron::TitleStripOf(COMMAND_PANEL_RECT);
  for (std::uint8_t index = 0; index < HUD_TAB_COUNT; ++index)
  {
    const Neuron::UiRect cell = Neuron::ColumnIn(strip, index, HUD_TAB_COUNT, 0);
    m_command.Add(Toggle(cell, ID_TAB_BASE + index, TAB_NAMES[index], static_cast<std::uint8_t>(m_tab) == index));
  }
  switch (m_tab)
  {
  case HudTab::Construct:
    RefreshConstruct(_frame);
    return;
  case HudTab::Produce:
    RefreshProduce(_frame);
    return;
  case HudTab::Research:
    RefreshResearch(_frame);
    return;
  case HudTab::Design:
    RefreshDesign(_frame);
    return;
  case HudTab::Match:
    RefreshMatch(_frame);
    return;
  }
}

void Hud::RefreshConstruct(const Frame& _frame)
{
  // §7.1: "a row of structure buttons, one per structure the seat has researched, each 128x64
  // holding a 32x32 icon, the name, and the cost in power".
  const Neuron::UiRect area = Neuron::ClientAreaOf(COMMAND_PANEL_RECT);
  const SeatState& seat = _frame.match->Commander().Own();
  std::int32_t column = 0;
  for (std::uint32_t row = 0; row < _frame.content->structures.structures.size(); ++row)
  {
    const StructureDesc& structure = _frame.content->structures.structures[row];
    if (!Unlocked(*_frame.content, seat.researchComplete, structure.unlockedBy))
    {
      continue; // Not researched: §7.1 lists it not at all rather than listing it disabled.
    }
    const Neuron::UiRect cell{area.x + (column * 132), area.y, 128, 64};
    if (cell.Right() > area.Right())
    {
      break; // M1's six fit; a seventh would run off the panel rather than wrap into the stats.
    }
    const bool affordable = seat.powerHundredths >= structure.costHundredths;
    const bool placing = _frame.armed == ArmedOrder::PlaceStructure && _frame.armedStructure == row;
    m_command.Add(Toggle(cell, ID_STRUCTURE_BASE + row, structure.name, placing, affordable));
    m_command.Add(Icon(Neuron::UiRect{cell.x + 4, cell.y + 4, 32, 32}, IconOf(*_frame.content, "Structure" + structure.id)));
    m_command.Add(Label(Neuron::UiRect{cell.x + 40, cell.y + 44, 84, 16}, Power(structure.costHundredths)));
    ++column;
  }
  // "The panel also shows the plan count, n of 64, in dimText, and turns it warning at the cap."
  std::uint32_t plans = 0;
  for (const auto& standing : _frame.match->Commander().Structures())
  {
    plans += standing.second.state.phase == StructurePhase::Plan ? 1u : 0u;
  }
  m_command.Add(Label(Neuron::UiRect{area.x, area.Bottom() - 20, area.width, 16},
                      std::to_string(plans) + " OF " + std::to_string(MAX_PLANS_PER_SEAT) + " PLANS"));
}

void Hud::RefreshProduce(const Frame& _frame)
{
  // §7.2, as far as the wire carries it. THE DESIGN LIST IS HERE AND THE QUEUE IS NOT, and that is
  // recorded rather than faked: Design/Interface.md §11 row 16 says a factory's queue is a per-seat
  // list of up to sixty-four entries that nothing replicates, and a panel that drew an empty queue
  // would be telling the commander his factory was idle when it was not.
  const Neuron::UiRect area = Neuron::ClientAreaOf(COMMAND_PANEL_RECT);
  const Neuron::UiRect left{area.x, area.y, area.width / 2, area.height};
  const Neuron::UiRect right{area.x + (area.width / 2), area.y, area.width / 2, area.height};

  bool factorySelected = false;
  for (const SelectedObject& object : _frame.selected)
  {
    const auto found = _frame.match->Commander().Structures().find(object.id);
    if (found == _frame.match->Commander().Structures().end())
    {
      continue;
    }
    const std::uint32_t row = found->second.state.design;
    factorySelected = factorySelected || (row < _frame.content->structures.structures.size() &&
                                          _frame.content->structures.structures[row].role == StructureRole::Factory);
  }
  if (!factorySelected)
  {
    m_command.Add(Label(left, "SELECT A FACTORY"));
    return;
  }

  Neuron::UiWidget designs{};
  designs.kind = Neuron::UiWidgetKind::List;
  designs.rect = left;
  designs.id = ID_DESIGN_LIST;
  designs.rowHeightPixels = 32;
  designs.value = -1;
  for (const DesignState& design : _frame.match->Commander().Designs().All())
  {
    if (design.seat != _frame.match->Commander().Seat())
    {
      continue;
    }
    designs.rows.push_back("DESIGN " + std::to_string(design.index));
  }
  designs.maximum = static_cast<std::int32_t>(designs.rows.size());
  m_command.Add(designs);

  const SeatState& seat = _frame.match->Commander().Own();
  if (seat.deviceCount >= seat.deviceCap)
  {
    // "A factory whose commander is at the device cap shows AT THE DEVICE CAP in warning where the
    // progress bar would be, and the queue does not advance" (§7.2; GameDesign.md §4).
    m_command.Add(Label(Neuron::UiRect{right.x, right.y, right.width, 16}, "AT THE DEVICE CAP"));
  }
  m_command.Add(Label(Neuron::UiRect{right.x, right.y + 24, right.width, 16}, "QUEUE NOT REPLICATED"));
  m_command.Add(Label(Neuron::UiRect{right.x, right.y + 44, right.width, 16}, "SEE INTERFACE.MD 11 ROW 16"));
}

void Hud::RefreshResearch(const Frame& _frame)
{
  // §7.3: the items available to the seat, one row each; "a row whose prerequisites are unmet is
  // not listed at all", and the lab that is researching shows above the list with its bar.
  const Neuron::UiRect area = Neuron::ClientAreaOf(COMMAND_PANEL_RECT);
  const SeatState& seat = _frame.match->Commander().Own();
  std::int32_t top = area.y;
  if (seat.researchItem != NO_RESEARCH_ITEM && seat.researchItem < _frame.content->research.size())
  {
    const ResearchItemDesc& item = _frame.content->research[seat.researchItem];
    m_command.Add(Label(Neuron::UiRect{area.x, top, area.width / 2, 16}, item.name));
    m_command.Add(Bar(Neuron::UiRect{area.x + (area.width / 2), top, (area.width / 2) - 80, Neuron::BAR_HEIGHT_PIXELS},
                      static_cast<std::int32_t>(item.timeTicks - std::min(item.timeTicks, seat.researchRemainingTicks)),
                      static_cast<std::int32_t>(std::max(item.timeTicks, 1u)), Neuron::UiBarStyle::Build));
    m_command.Add(Label(Neuron::UiRect{area.Right() - 76, top, 76, 16},
                        std::to_string(seat.researchRemainingTicks / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND)) + " S"));
    top += 24;
  }

  Neuron::UiWidget items{};
  items.kind = Neuron::UiWidgetKind::List;
  items.rect = Neuron::UiRect{area.x, top, area.width, area.Bottom() - top};
  items.id = ID_RESEARCH_LIST;
  items.rowHeightPixels = 32;
  items.value = -1;
  for (std::size_t row = 0; row < _frame.content->research.size() && row < RESEARCH_MASK_BITS; ++row)
  {
    const ResearchItemDesc& item = _frame.content->research[row];
    if ((seat.researchComplete & (std::uint64_t{1} << row)) != 0)
    {
      continue; // Already done; §7.3 lists what is AVAILABLE.
    }
    bool ready = true;
    for (const std::string& prerequisite : item.prerequisites)
    {
      ready = ready && Unlocked(*_frame.content, seat.researchComplete, prerequisite);
    }
    if (!ready)
    {
      continue; // "A row whose prerequisites are unmet is not listed at all."
    }
    items.rows.push_back(Fit(item.name + "   " + Power(item.costHundredths) + "   " +
                               std::to_string(item.timeTicks / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND)) + "S   " +
                               item.description,
                             items.rect.width));
  }
  items.maximum = static_cast<std::int32_t>(items.rows.size());
  m_command.Add(items);
}

void Hud::RefreshDesign(const Frame& _frame)
{
  // §7.4: three columns of buttons, the derived statistics under them, a name field and Save.
  const Neuron::UiRect area = Neuron::ClientAreaOf(COMMAND_PANEL_RECT);
  const SeatState& seat = _frame.match->Commander().Own();
  bool hasCommandPost = false;
  for (const auto& standing : _frame.match->Commander().Structures())
  {
    const std::uint32_t row = standing.second.state.design;
    hasCommandPost =
      hasCommandPost || (standing.second.state.seat == _frame.match->Commander().Seat() && !standing.second.ghost &&
                         standing.second.state.phase == StructurePhase::Standing && row < _frame.content->structures.structures.size() &&
                         _frame.content->structures.structures[row].role == StructureRole::CommandPost);
  }
  if (!hasCommandPost)
  {
    m_command.Add(Label(area, "A COMMAND POST IS NEEDED"));
    return;
  }

  const std::int32_t columnWidth = area.width / 3;
  const auto column = [&](std::int32_t _index) { return Neuron::UiRect{area.x + (_index * columnWidth), area.y, columnWidth - 4, 160}; };
  std::int32_t row = 0;
  for (std::uint32_t index = 0; index < _frame.content->components.chassis.size(); ++index)
  {
    const ChassisDesc& chassis = _frame.content->components.chassis[index];
    if (!Unlocked(*_frame.content, seat.researchComplete, chassis.unlockedBy))
    {
      continue;
    }
    m_command.Add(
      Toggle(RowAt(column(0), row, Neuron::BUTTON_HEIGHT_PIXELS), ID_DESIGN_CHASSIS_BASE + index, chassis.name, m_designChassis == index));
    ++row;
  }
  row = 0;
  for (std::uint32_t index = 0; index < _frame.content->components.drives.size(); ++index)
  {
    const DriveDesc& drive = _frame.content->components.drives[index];
    if (!Unlocked(*_frame.content, seat.researchComplete, drive.unlockedBy))
    {
      continue;
    }
    m_command.Add(
      Toggle(RowAt(column(1), row, Neuron::BUTTON_HEIGHT_PIXELS), ID_DESIGN_DRIVE_BASE + index, drive.name, m_designDrive == index));
    ++row;
  }
  row = 0;
  for (std::uint32_t index = 0; index < _frame.content->components.modules.size(); ++index)
  {
    const ModuleDesc& module = _frame.content->components.modules[index];
    if (!Unlocked(*_frame.content, seat.researchComplete, module.unlockedBy))
    {
      continue;
    }
    const bool mounted = std::find(m_designModules.begin(), m_designModules.end(), index) != m_designModules.end();
    m_command.Add(Toggle(RowAt(column(2), row, Neuron::BUTTON_HEIGHT_PIXELS), ID_DESIGN_MODULE_BASE + index, module.name, mounted));
    ++row;
  }

  // THE DERIVED STATISTICS, RECOMPUTED ON EVERY CHANGE AND SHOWN BEFORE THE COMMANDER COMMITS
  // (§7.4). The fault, when there is one, is the reason Save is disabled.
  DesignRecipe recipe{};
  recipe.name = m_designName;
  if (m_designChassis < _frame.content->components.chassis.size())
  {
    recipe.chassis = _frame.content->components.chassis[m_designChassis].id;
  }
  if (m_designDrive < _frame.content->components.drives.size())
  {
    recipe.drive = _frame.content->components.drives[m_designDrive].id;
  }
  for (const std::uint32_t module : m_designModules)
  {
    if (module < _frame.content->components.modules.size())
    {
      recipe.modules.push_back(_frame.content->components.modules[module].id);
    }
  }
  DesignStats stats{};
  const ClassUpgrades none{};
  const DesignFault fault = DeriveDesignStats(*_frame.content, recipe, none, stats);
  const std::int32_t statsTop = area.y + 168;
  if (fault == DesignFault::None)
  {
    m_command.Add(Label(Neuron::UiRect{area.x, statsTop, columnWidth, 16},
                        "SPEED " + std::to_string(WorldUnitsPerSecond(stats.speedSubunitsPerTick)) + " WU/S"));
    m_command.Add(Label(Neuron::UiRect{area.x + columnWidth, statsTop, columnWidth, 16}, "HIT POINTS " + std::to_string(stats.hitPoints)));
    m_command.Add(Label(Neuron::UiRect{area.x, statsTop + 20, columnWidth, 16},
                        "ARMOUR " + std::to_string(stats.kineticArmor) + " K / " + std::to_string(stats.thermalArmor) + " T"));
    m_command.Add(Label(Neuron::UiRect{area.x + columnWidth, statsTop + 20, columnWidth, 16},
                        "SIGHT " + std::to_string(stats.sightSubunits / Neuron::SUBUNITS_PER_CELL) + " CELLS"));
    m_command.Add(Label(Neuron::UiRect{area.x, statsTop + 40, columnWidth, 16}, "COST " + Power(stats.costHundredths) + " POWER"));
    m_command.Add(
      Label(Neuron::UiRect{area.x + columnWidth, statsTop + 40, columnWidth, 16},
            "BUILD TIME " + std::to_string(stats.buildTimeTicks / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND)) + " S"));
  }
  else
  {
    m_command.Add(Label(Neuron::UiRect{area.x, statsTop, area.width, 16}, NameOfDesignFault(fault)));
  }

  Neuron::UiWidget name{};
  name.kind = Neuron::UiWidgetKind::TextField;
  name.rect = Neuron::UiRect{area.x + (2 * columnWidth), statsTop, columnWidth - 4, Neuron::BUTTON_HEIGHT_PIXELS};
  name.id = ID_DESIGN_NAME;
  name.text = m_designName;
  name.textLimit = 24; // §7.4's own limit
  m_command.Add(name);
  m_command.Add(Button(Neuron::UiRect{area.x + (2 * columnWidth), statsTop + 32, columnWidth - 4, Neuron::BUTTON_HEIGHT_PIXELS},
                       ID_DESIGN_SAVE, "SAVE", fault == DesignFault::None));
}

void Hud::RefreshMatch(const Frame& _frame)
{
  // §7.5: what the lobby fixed, read-only, "because M1 has no lobby and the defaults are otherwise
  // invisible".
  const Neuron::UiRect area = Neuron::ClientAreaOf(COMMAND_PANEL_RECT);
  const MatchSettings& settings = _frame.match->Settings();
  std::int32_t row = 0;
  const auto line = [&](std::string _text) { m_command.Add(Label(RowAt(area, row++, 18), Fit(std::move(_text), area.width))); };
  line(std::string("LANDSCAPE ") + SLICE_LANDSCAPE + "   " + NameOfSizeClass(settings.sizeClass));
  line(std::string("VICTORY ") + NameOfVictory(settings.victory));
  line("DEVICE CAP " + std::to_string(DEVICE_CAPS[static_cast<std::size_t>(settings.deviceCapLevel)]));
  line(std::string("STARTING POWER ") + NameOfPowerLevel(settings.powerLevel));
  line("TECHNOLOGY TIERS " + std::to_string(settings.technologyTiers));
  for (std::uint8_t seat = 0; seat < settings.seatCount; ++seat)
  {
    line("SEAT " + std::to_string(seat) + "   " + NameOfSeatKind(settings.seats[seat].kind) + "   ALLIANCE " +
         (settings.seats[seat].alliance == NO_ALLIANCE ? std::string("NONE") : std::to_string(settings.seats[seat].alliance)));
  }
}

void Hud::RefreshSelection(const Frame& _frame)
{
  m_selection.Reset();
  m_selection.SetTitle("SELECTION");
  const Neuron::UiRect area = Neuron::ClientAreaOf(SELECTION_PANEL_RECT);
  const Replica& replica = _frame.match->Commander();
  const ContentTree& content = *_frame.content;

  // What of the selection the replica still holds. A selected device that died is left out here
  // although Selection keeps it (Replica/Selection.h says why): a panel that described a device
  // nobody can see any more is a panel describing a memory.
  std::vector<const ReplicaDevice*> devices;
  const ReplicaStructure* structure = nullptr;
  for (const SelectedObject& object : _frame.selected)
  {
    if (object.kind == ObjectKind::Device)
    {
      const auto found = replica.Devices().find(object.id);
      if (found != replica.Devices().end())
      {
        devices.push_back(&found->second);
      }
      continue;
    }
    const auto found = replica.Structures().find(object.id);
    if (found != replica.Structures().end())
    {
      structure = &found->second;
    }
  }

  if (devices.empty() && structure == nullptr)
  {
    // "Nothing selected: the panel body is empty but for a dimText line naming the two things a
    // player who has just started needs."
    m_selection.Add(Label(RowAt(area, 0, 18), "LEFT CLICK TO SELECT"));
    m_selection.Add(Label(RowAt(area, 1, 18), "RIGHT CLICK TO ORDER"));
    m_detail = 0;
    return;
  }

  if (!devices.empty())
  {
    RefreshDevices(_frame, devices);
    return;
  }

  // ── One structure (§8's third, fourth and sixth cases) ──────────────────────────────────────
  const StructureState& state = structure->state;
  const StructureDesc* row = state.design < content.structures.structures.size() ? &content.structures.structures[state.design] : nullptr;
  if (structure->ghost)
  {
    // "A ghost structure shows its kind and the health it had when last seen, with the whole
    // panel's body text in dimText and the word REMEMBERED in the title strip." The health it had
    // is exactly what the encoder does NOT send (Replica/ReplicaObject.h's GHOST_HIT_POINTS), so
    // the honest reading of "the health it had when last seen" is that the wire carries none and
    // the panel says so rather than drawing a full bar.
    m_selection.SetTitle("REMEMBERED");
  }
  std::int32_t line = 0;
  m_selection.Add(Label(RowAt(area, line, 18), row != nullptr ? row->name : "STRUCTURE"));
  m_selection.Add(Icon(Neuron::UiRect{area.Right() - 36, area.y, 32, 32}, row != nullptr ? IconOf(content, "Structure" + row->id) : -1));
  ++line;
  if (row != nullptr)
  {
    m_selection.Add(Label(RowAt(area, line, 18), NameOfStructureRole(row->role)));
    ++line;
  }
  if (state.phase == StructurePhase::Plan)
  {
    // "A structure plan shows the structure's name, PLAN, its cost, and NOT STARTED until a builder
    // begins it." A plan that had been begun would be UnderConstruction, so the line is not a
    // condition: a plan is by definition not started.
    m_selection.Add(Label(RowAt(area, line++, 18), "PLAN"));
    m_selection.Add(Label(RowAt(area, line++, 18), row != nullptr ? Power(row->costHundredths) + " POWER" : ""));
    m_selection.Add(Label(RowAt(area, line, 18), "NOT STARTED"));
    return;
  }
  if (state.phase == StructurePhase::UnderConstruction)
  {
    // "A structure under construction shows a barBuild bar and the seconds remaining instead of
    // health, because health follows progress." The seconds are the row's build time against the
    // percent done: the wire carries the percent and not the ticks left (Net/Records.h), and the
    // host owns the arithmetic that got it there - §7.2's own rule, "the panel divides and prints".
    m_selection.Add(Bar(Neuron::UiRect{area.x, area.y + (line * 20), area.width - 80, Neuron::BAR_HEIGHT_PIXELS}, state.buildPercent, 100,
                        Neuron::UiBarStyle::Build));
    const std::uint32_t remaining =
      row == nullptr ? 0u : row->buildTimeTicks * (100u - std::min<std::uint32_t>(state.buildPercent, 100u)) / 100u;
    m_selection.Add(Label(Neuron::UiRect{area.Right() - 76, area.y + (line * 20), 76, 16},
                          std::to_string(remaining / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND)) + " S"));
    return;
  }
  if (structure->ghost)
  {
    m_selection.Add(Label(RowAt(area, line, 18), "HEALTH NOT REMEMBERED"));
    return;
  }
  const std::int32_t maximum = row != nullptr ? row->hitPoints : state.hitPoints;
  m_selection.Add(Bar(Neuron::UiRect{area.x, area.y + (line * 20), area.width - 96, Neuron::BAR_HEIGHT_PIXELS}, state.hitPoints, maximum,
                      Neuron::UiBarStyle::Health));
  m_selection.Add(Label(Neuron::UiRect{area.Right() - 92, area.y + (line * 20), 92, 16},
                        std::to_string(state.hitPoints) + " / " + std::to_string(maximum)));
  ++line;
  if (row != nullptr)
  {
    m_selection.Add(Label(RowAt(area, line, 18), Fit(RoleLineOf(_frame, *structure, *row), area.width)));
  }
}

std::string Hud::RoleLineOf(const Frame& _frame, const ReplicaStructure& _structure, const StructureDesc& _row) const
{
  // §8's "its role's own line". THREE OF THE SIX ARE THE SEAT'S OWN NUMBERS and two of them are the
  // host's: a factory's current build and a lab's current item are in SeatState only while the lab
  // is this seat's, and a factory's queue is not on the wire at all (§11 row 16). What is left is
  // what the replica can answer without guessing, and each line says which it is.
  const Replica& replica = _frame.match->Commander();
  const SeatState& seat = replica.Own();
  switch (_row.role)
  {
  case StructureRole::Factory:
    return "QUEUE NOT REPLICATED (INTERFACE.MD 11 ROW 16)";
  case StructureRole::ResearchLab:
  {
    if (_structure.state.seat != replica.Seat() || seat.researchItem == NO_RESEARCH_ITEM ||
        seat.researchItem >= _frame.content->research.size())
    {
      return "IDLE";
    }
    // THE SEAT'S ITEM AND NOT THIS LAB'S. SeatState carries one research row for the whole
    // commander, so a second lab would be described by the same line; M1 gives a seat one lab and
    // the line is right, and a milestone that gives it two needs the row per lab on the wire.
    return _frame.content->research[seat.researchItem].name + "   " +
           std::to_string(seat.researchRemainingTicks / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND)) + " S";
  }
  case StructureRole::Generator:
  {
    // "A generator's served extractor count." The assignment is the host's (Sim/Economy.h serves
    // the four nearest unserved extractors in range, recomputed every tick) and nothing replicates
    // it, so what is shown is what the row says it CAN serve rather than what it does.
    return "SERVES UP TO " + std::to_string(_row.servesExtractors);
  }
  case StructureRole::Extractor:
    // "An extractor's yield per second or UNSERVED in warning." Which of the two it is depends on
    // the same assignment, so the yield is the row's and the word UNSERVED cannot be said.
    return Power(static_cast<std::int64_t>(_row.powerHundredthsPerTick) * Neuron::TICKS_PER_SECOND) + " POWER/S WHEN SERVED";
  case StructureRole::CommandPost:
    return Power(static_cast<std::int64_t>(_row.powerHundredthsPerTick) * Neuron::TICKS_PER_SECOND) + " POWER/S TRICKLE";
  case StructureRole::RepairBay:
  case StructureRole::SensorTower:
  case StructureRole::Wall:
  case StructureRole::Hardpoint:
  case StructureRole::Tower:
  case StructureRole::Bunker:
  case StructureRole::Uplink:
    break;
  }
  return "";
}

void Hud::RefreshDevices(const Frame& _frame, std::span<const ReplicaDevice* const> _devices)
{
  const Neuron::UiRect area = Neuron::ClientAreaOf(SELECTION_PANEL_RECT);
  const Replica& replica = _frame.match->Commander();
  const ContentTree& content = *_frame.content;
  m_detail = m_detail < _devices.size() ? m_detail : static_cast<std::uint32_t>(_devices.size() - 1);

  std::int32_t top = area.y;
  if (_devices.size() > 1)
  {
    // "A grid of up to 32 portraits, 48x48 each, each with a two-pixel health strip along its
    // bottom edge and a one-pixel accent border on the one the panel's detail lines describe.
    // Over 32 selected, the grid shows the first 32 and a count."
    const std::size_t shown = std::min<std::size_t>(_devices.size(), PORTRAITS_SHOWN);
    for (std::size_t index = 0; index < shown; ++index)
    {
      const std::int32_t column = static_cast<std::int32_t>(index % PORTRAITS_PER_ROW);
      const std::int32_t row = static_cast<std::int32_t>(index / PORTRAITS_PER_ROW);
      const Neuron::UiRect cell{area.x + (column * 52), top + (row * 52), 48, 48};
      const DeviceState& state = _devices[index]->state;
      m_selection.Add(Toggle(cell, ID_PORTRAIT_BASE + static_cast<std::uint32_t>(index), "", index == m_detail));
      const DesignState* design = DesignOf(replica, state);
      DesignStats stats{};
      const bool known = design != nullptr && StatsOf(content, *design, stats);
      m_selection.Add(Icon(Neuron::UiRect{cell.x + 8, cell.y + 4, 32, 32}, IconOf(content, PortraitIconOf(content, design))));
      m_selection.Add(Bar(Neuron::UiRect{cell.x + 2, cell.Bottom() - 4, cell.width - 4, 2}, state.hitPoints,
                          known ? stats.hitPoints : state.hitPoints, Neuron::UiBarStyle::Health));
    }
    top += ((static_cast<std::int32_t>(shown - 1) / PORTRAITS_PER_ROW) + 1) * 52;
    if (_devices.size() > PORTRAITS_SHOWN)
    {
      m_selection.Add(Label(Neuron::UiRect{area.x, top, area.width, 16}, std::to_string(_devices.size()) + " SELECTED"));
      top += 20;
    }
  }

  // ── The detail lines, for one device or for the one the grid points at ──────────────────────
  const DeviceState& state = _devices[m_detail]->state;
  const bool own = state.seat == replica.Seat();
  const DesignState* design = DesignOf(replica, state);
  DesignStats stats{};
  const bool known = design != nullptr && StatsOf(content, *design, stats);
  const auto line = [&](std::int32_t _index) { return Neuron::UiRect{area.x, top + (_index * 18), area.width, 16}; };
  std::int32_t index = 0;
  // "Its design name" - and nothing in the tree carries one. A design is a chassis, a drive and a
  // list of module rows, in the simulation (Sim/Device.h), on the wire (Net/Records.h) and in the
  // order that saves it (Sim/Order.h's SaveDesign); the name §7.4 types is not carried by any of
  // the three. Design/Interface.md §11 row 19 owns it; until then a design is its index, which is
  // what DeviceState::design names it by.
  m_selection.Add(Label(line(index++), (own ? "DESIGN " : "ENEMY DESIGN ") + std::to_string(state.design)));
  m_selection.Add(Bar(Neuron::UiRect{area.x, top + (index * 18), area.width - 96, Neuron::BAR_HEIGHT_PIXELS}, state.hitPoints,
                      known ? stats.hitPoints : state.hitPoints, Neuron::UiBarStyle::Health));
  m_selection.Add(Label(Neuron::UiRect{area.Right() - 92, top + (index * 18), 92, 16},
                        std::to_string(state.hitPoints) + " / " + std::to_string(known ? stats.hitPoints : state.hitPoints)));
  ++index;
  // "The rank badge and rank name." Neither exists: GameDesign.md §8 gives eight ranks, their
  // thresholds and their percentages and no names, and Icons.dds has no badge cell - which is
  // Design/Interface.md §11 row 6, open and owned by the design. The number is what can be said.
  m_selection.Add(Label(line(index++), "RANK " + std::to_string(state.rank)));
  if (design == nullptr)
  {
    // "Until that record arrives the panel shows the health bar and DESIGN UNKNOWN in dimText, and
    // fills in when it arrives" (§8, of an enemy; it is as true of a rejoining client's own).
    m_selection.Add(Label(line(index), "DESIGN UNKNOWN"));
    return;
  }
  std::string chassis;
  std::string drive;
  std::string modules;
  DescribeDesign(content, *design, chassis, drive, modules);
  m_selection.Add(Label(line(index++), Fit(chassis, area.width)));
  m_selection.Add(Label(line(index++), Fit(drive, area.width)));
  m_selection.Add(Label(line(index++), Fit(modules, area.width)));
  if (own)
  {
    // "And the current order kind with its target."
    std::string order(NameOfPrimaryOrder(state.stances.order));
    if (state.target != 0)
    {
      order += " " + std::to_string(state.target);
    }
    m_selection.Add(Label(line(index), order));
  }
}

void Hud::RefreshOrders(const Frame& _frame)
{
  m_orders.Reset();
  const Neuron::UiRect area = Neuron::ClientAreaOf(ORDERS_PANEL_RECT);
  const Replica& replica = _frame.match->Commander();

  // "Shown for a selection of own devices; empty for a structure or an enemy." And the stances of
  // the row are the selection's where it agrees and none where it does not: "a mixed selection
  // shows no option filled in a row where its devices disagree, and clicking sets them all."
  std::uint32_t own = 0;
  OrderAndStances common{};
  std::array<bool, STANCE_AXIS_COUNT> agreed{};
  agreed.fill(true);
  for (const SelectedObject& object : _frame.selected)
  {
    if (object.kind != ObjectKind::Device)
    {
      continue;
    }
    const auto found = replica.Devices().find(object.id);
    if (found == replica.Devices().end() || found->second.state.seat != replica.Seat())
    {
      continue;
    }
    const OrderAndStances& stances = found->second.state.stances;
    if (own == 0)
    {
      common = stances;
    }
    else
    {
      for (std::uint8_t axis = 0; axis < STANCE_AXIS_COUNT; ++axis)
      {
        agreed[axis] =
          agreed[axis] && StanceValueOf(stances, static_cast<StanceAxis>(axis)) == StanceValueOf(common, static_cast<StanceAxis>(axis));
      }
    }
    ++own;
  }
  if (own == 0)
  {
    return;
  }

  // "Primary orders, as a row of 48x48 icon buttons." An order that ARMS is a toggle that is down
  // while it is armed (§9.1: "an order that arms (§6) fills accent while armed"); Guard and Stop
  // are issued on the release and light nothing.
  for (std::uint32_t index = 0; index < PRIMARY_ORDERS.size(); ++index)
  {
    const Neuron::UiRect cell{area.x + static_cast<std::int32_t>(index * 56), area.y, 48, 48};
    const bool enabled = PRIMARY_ORDERS[index] != OrderKind::ReturnToRepair;
    const bool lit = _frame.armed != ArmedOrder::None && ArmedFor(PRIMARY_ORDERS[index]) == _frame.armed;
    m_orders.Add(Toggle(cell, ID_ORDER_BASE + static_cast<std::uint32_t>(PRIMARY_ORDERS[index]), "", lit, enabled));
    m_orders.Add(Icon(Neuron::UiRect{cell.x + 8, cell.y + 8, 32, 32}, IconOf(*_frame.content, PRIMARY_ORDER_ICONS[index])));
  }

  // "Stances, as four rows of small buttons, each row a mutually exclusive set, the active one
  // filled accent."
  std::int32_t top = area.y + 56;
  for (std::size_t row = 0; row < STANCE_ROWS.size(); ++row)
  {
    const StanceRow& stance = STANCE_ROWS[row];
    m_orders.Add(Label(Neuron::UiRect{area.x, top + 4, 64, 16}, stance.name));
    for (std::uint8_t option = 0; option < stance.options; ++option)
    {
      const Neuron::UiRect cell{area.x + 68 + (option * 40), top, 36, Neuron::BUTTON_HEIGHT_PIXELS};
      const bool on = agreed[row] && StanceValueOf(common, stance.axis) == option;
      m_orders.Add(
        Toggle(cell, ID_STANCE_BASE + static_cast<std::uint32_t>((row * STANCE_IDS_PER_ROW) + option), "", on, stance.enabled[option]));
      m_orders.Add(Icon(Neuron::UiRect{cell.x + 2, cell.y - 4, 32, 32}, IconOf(*_frame.content, stance.icons[option])));
    }
    top += Neuron::BUTTON_HEIGHT_PIXELS + 4;
  }
}

void Hud::RefreshReadouts(const Frame& _frame)
{
  m_power.Reset();
  m_matchState.Reset();
  const Replica& replica = _frame.match->Commander();
  const SeatState& seat = replica.Own();

  // ── §9.3, and the income measured rather than re-derived (Hud.h's members say why) ──────────
  if (_frame.tick < m_sampleTick || m_sampleTick == 0)
  {
    // A rejoin restarts the replica's clock, and a total that went backwards against a tick that
    // went backwards is not a rate; the sample is simply retaken.
    m_sampleTick = _frame.tick;
    m_sampleExtracted = seat.extractedHundredths;
    m_extractionHundredthsPerTick = 0;
  }
  else if (_frame.tick >= m_sampleTick + static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND))
  {
    const std::int64_t rise = seat.extractedHundredths - m_sampleExtracted;
    m_extractionHundredthsPerTick = static_cast<std::int32_t>(rise / static_cast<std::int64_t>(_frame.tick - m_sampleTick));
    m_sampleTick = _frame.tick;
    m_sampleExtracted = seat.extractedHundredths;
  }
  std::int32_t trickleHundredthsPerTick = 0;
  for (const auto& standing : replica.Structures())
  {
    const StructureState& state = standing.second.state;
    if (state.seat != replica.Seat() || standing.second.ghost || state.phase != StructurePhase::Standing ||
        state.design >= _frame.content->structures.structures.size())
    {
      continue;
    }
    const StructureDesc& row = _frame.content->structures.structures[state.design];
    trickleHundredthsPerTick += row.role == StructureRole::CommandPost ? row.powerHundredthsPerTick : 0;
  }
  const std::int64_t perSecond =
    static_cast<std::int64_t>(m_extractionHundredthsPerTick + trickleHundredthsPerTick) * Neuron::TICKS_PER_SECOND;
  m_power.Add(Label(ReadoutLineOf(POWER_READOUT_RECT), "POWER " + Power(seat.powerHundredths) + " / " + Power(seat.stockpileCapHundredths) +
                                                         "   +" + Power(perSecond) + "/S"));

  // ── §9.4 ────────────────────────────────────────────────────────────────────────────────────
  std::string clock = Clock(_frame.tick);
  if (_frame.match->Settings().victory == VictoryCondition::Survival)
  {
    const std::uint32_t survival = _frame.match->Settings().survivalTicks;
    clock += "   SURVIVE " + Clock(survival - std::min(survival, _frame.tick));
  }
  m_matchState.Add(Label(ReadoutLineOf(MATCH_READOUT_RECT), clock));
}

void Hud::RefreshMinimap(const Frame& _frame)
{
  const Replica& replica = _frame.match->Commander();
  const Neuron::FogView& fog = _frame.match->View().fog;
  m_minimapCells = fog.cellsPerSide;
  if (fog.cellsPerSide == 0)
  {
    return; // Before the join there is no landscape; the map keeps whatever it last held.
  }
  m_blips.clear();
  // STRUCTURES FIRST AND DEVICES OVER THEM, which is §9.2's own back-to-front order; BuildMinimap
  // draws the blips in the order they are given, so the order is here rather than in a sort.
  for (const auto& standing : replica.Structures())
  {
    const StructureState& state = standing.second.state;
    const std::uint32_t cellsX = state.design < _frame.content->structures.structures.size()
                                   ? _frame.content->structures.structures[state.design].footprintCellsX
                                   : 1u;
    const std::uint32_t cellsY = state.design < _frame.content->structures.structures.size()
                                   ? _frame.content->structures.structures[state.design].footprintCellsY
                                   : 1u;
    Neuron::MinimapBlip blip{};
    blip.x = static_cast<float>(state.cellX * Neuron::WORLD_UNITS_PER_CELL);
    blip.z = static_cast<float>(state.cellY * Neuron::WORLD_UNITS_PER_CELL);
    blip.cellsX = cellsX;
    blip.cellsY = cellsY;
    blip.colorIndex = state.seat;
    blip.selected = SelectedHere(_frame.selected, state.id);
    blip.ghost = standing.second.ghost;
    m_blips.push_back(blip);
  }
  for (const auto& device : replica.Devices())
  {
    const DeviceState& state = device.second.state;
    Neuron::MinimapBlip blip{};
    blip.x = Neuron::WorldUnitsOfSubunits(state.x * SUBUNITS_PER_WIRE_UNIT);
    blip.z = Neuron::WorldUnitsOfSubunits(state.z * SUBUNITS_PER_WIRE_UNIT);
    blip.colorIndex = state.seat;
    blip.selected = SelectedHere(_frame.selected, state.id);
    m_blips.push_back(blip);
  }

  m_commanderColors.clear();
  for (const Rgba8& color : _frame.content->ui.commanders)
  {
    m_commanderColors.push_back(Neuron::PackedRgba8(color.red, color.green, color.blue, color.alpha));
  }

  Neuron::MinimapView view{};
  view.fog = &fog;
  view.blips = m_blips;
  view.commanderColors = m_commanderColors;
  view.terrainColor = Neuron::PackedRgba8(96, 104, 88, 255);
  view.accentColor = Neuron::PackedRgba8(_frame.content->ui.chrome.accent.red, _frame.content->ui.chrome.accent.green,
                                         _frame.content->ui.chrome.accent.blue, _frame.content->ui.chrome.accent.alpha);
  view.borderColor = Neuron::PackedRgba8(_frame.content->ui.chrome.panelBorder.red, _frame.content->ui.chrome.panelBorder.green,
                                         _frame.content->ui.chrome.panelBorder.blue, _frame.content->ui.chrome.panelBorder.alpha);
  view.worldUnitsPerCell = static_cast<float>(Neuron::WORLD_UNITS_PER_CELL);
  view.frustumGround = FrustumGround(_frame, fog.cellsPerSide);
  Neuron::BuildMinimap(view, m_minimap);
}

std::array<float, 8> Hud::FrustumGround(const Frame& _frame, std::uint32_t _cellsPerSide)
{
  // "The camera frustum, a one-pixel panelBorder quadrilateral where the frustum's four far corners
  // meet the ground plane, clipped to the map" (§9.2). The plane at y = 0 and not the heightfield:
  // the quadrilateral is a hint about where the camera is looking, a cell of slop in it is not
  // visible at two pixels a cell, and a march of the terrain for each of four corners every frame
  // would be paying for precision the map cannot draw.
  //
  // A CORNER THAT MEETS NO GROUND IS PUSHED OUT RATHER THAN DROPPED. The top two corners of a
  // camera looking at the horizon point above the plane and meet it nowhere; the ground they see
  // ends at the edge of the landscape, so the ray is followed that far and the clip §9.2 asks for
  // does the rest. Dropping the quadrilateral instead would make it vanish exactly when a player
  // tilts the camera up, which reads as a bug in the map.
  const float reach = static_cast<float>(_cellsPerSide * Neuron::WORLD_UNITS_PER_CELL);
  const std::array<std::array<std::int32_t, 2>, 4> corners = {
    {{0, 0},
     {static_cast<std::int32_t>(Neuron::AUTHORED_WIDTH_PIXELS) - 1, 0},
     {static_cast<std::int32_t>(Neuron::AUTHORED_WIDTH_PIXELS) - 1, static_cast<std::int32_t>(Neuron::AUTHORED_HEIGHT_PIXELS) - 1},
     {0, static_cast<std::int32_t>(Neuron::AUTHORED_HEIGHT_PIXELS) - 1}}};
  std::array<float, 8> ground{};
  for (std::size_t index = 0; index < corners.size(); ++index)
  {
    const PickRay ray = RayThrough(_frame.camera, corners[index][0], corners[index][1]);
    std::int32_t x = 0;
    std::int32_t z = 0;
    if (GroundPoint(ray, x, z))
    {
      ground[index * 2] = Neuron::WorldUnitsOfSubunits(x);
      ground[(index * 2) + 1] = Neuron::WorldUnitsOfSubunits(z);
      continue;
    }
    const float horizontal = std::sqrt((ray.directionX * ray.directionX) + (ray.directionZ * ray.directionZ));
    const float scale = horizontal > 0.0f ? reach / horizontal : 0.0f;
    ground[index * 2] = ray.originX + (ray.directionX * scale);
    ground[(index * 2) + 1] = ray.originZ + (ray.directionZ * scale);
  }
  return ground;
}

void Hud::OnEvent(const Neuron::UiEventResult& _result, const Frame& _frame, std::vector<Order>& _outOrders)
{
  if (!_result.consumed || _result.widget == ID_DECORATION)
  {
    return;
  }
  const std::uint32_t widget = _result.widget;
  const Replica& replica = _frame.match->Commander();
  const std::uint8_t seat = replica.Seat();

  if (widget >= ID_TAB_BASE && widget < ID_TAB_BASE + HUD_TAB_COUNT)
  {
    m_tab = static_cast<HudTab>(widget - ID_TAB_BASE);
    return;
  }
  if (widget >= ID_STRUCTURE_BASE && widget < ID_STRUCTURE_BASE + _frame.content->structures.structures.size())
  {
    // §7.1: "clicking one enters placement". No order is made here - the order is the left click
    // on the world that follows, which is OrderInput's (§6).
    m_armRequest = ArmedOrder::PlaceStructure;
    m_armRequestRow = widget - ID_STRUCTURE_BASE;
    m_armRequested = true;
    return;
  }
  if (widget >= ID_PORTRAIT_BASE && widget < ID_PORTRAIT_BASE + PORTRAITS_SHOWN)
  {
    // §8: "clicking a portrait narrows the selection to that device". Ctrl-clicking removes it, and
    // that cannot be done: Client/InputEvent.h carries no modifier at all, so the sink that
    // consumed this click could not tell the panel which it was (Interface.md §11 row 18).
    const std::uint32_t index = widget - ID_PORTRAIT_BASE;
    std::uint32_t at = 0;
    for (const SelectedObject& object : _frame.selected)
    {
      if (object.kind != ObjectKind::Device || replica.Devices().find(object.id) == replica.Devices().end())
      {
        continue;
      }
      if (at == index)
      {
        m_detail = index;
        m_portraitPick = object.id;
        m_portraitPicked = true;
        return;
      }
      ++at;
    }
    return;
  }
  if (widget >= ID_ORDER_BASE && widget < ID_ORDER_BASE + ORDER_KIND_COUNT)
  {
    OnPrimaryOrder(static_cast<OrderKind>(widget - ID_ORDER_BASE), _frame, _outOrders);
    return;
  }
  if (widget >= ID_STANCE_BASE && widget < ID_STANCE_BASE + (STANCE_AXIS_COUNT * STANCE_IDS_PER_ROW))
  {
    const std::uint32_t axis = (widget - ID_STANCE_BASE) / STANCE_IDS_PER_ROW;
    const std::uint32_t option = (widget - ID_STANCE_BASE) % STANCE_IDS_PER_ROW;
    for (const SelectedObject& object : _frame.selected)
    {
      const auto found = replica.Devices().find(object.id);
      if (object.kind != ObjectKind::Device || found == replica.Devices().end() || found->second.state.seat != seat)
      {
        continue;
      }
      Order order{};
      order.seat = seat;
      order.kind = OrderKind::SetStance;
      order.operands[0] = static_cast<std::int32_t>(object.id);
      order.operands[1] = static_cast<std::int32_t>(axis);
      order.operands[2] = static_cast<std::int32_t>(option);
      _outOrders.push_back(order);
    }
    return;
  }
  if (widget == ID_DESIGN_LIST && _result.action == Neuron::UiAction::Selected)
  {
    OnProduce(_result.value, _frame, _outOrders);
    return;
  }
  if (widget == ID_RESEARCH_LIST && _result.action == Neuron::UiAction::Selected)
  {
    OnResearch(_result.value, _frame, _outOrders);
    return;
  }
  if (widget == ID_DESIGN_NAME)
  {
    // The field edits and nothing else: SaveDesign carries a chassis, a drive and four module rows
    // and no name at all (Sim/Order.h), so what is typed here reaches the simulation nowhere.
    // Interface.md §11 row 19.
    const Neuron::UiWidget* field = m_command.Find(ID_DESIGN_NAME);
    m_designName = field != nullptr ? field->text : m_designName;
    return;
  }
  OnDesignEvent(widget, _frame, _outOrders);
}

void Hud::OnPrimaryOrder(OrderKind _kind, const Frame& _frame, std::vector<Order>& _outOrders)
{
  // Move, Attack-move and Patrol ARM (§6) and the click on the world that follows is what makes the
  // order; Stop and Guard are issued here, because neither takes a point the commander has still to
  // give. Guard is given the device's own ground, which is what "guard where you stand" is: its
  // operand 3 is 0, so it guards a position and not another device (Sim/Order.h's table).
  if (_kind == OrderKind::Move || _kind == OrderKind::AttackMove || _kind == OrderKind::Patrol)
  {
    m_armRequest = ArmedFor(_kind);
    m_armRequestRow = 0;
    m_armRequested = true;
    return;
  }
  if (_kind == OrderKind::ReturnToRepair)
  {
    return; // Listed and disabled in M1: nothing repairs (S10).
  }
  const Replica& replica = _frame.match->Commander();
  for (const SelectedObject& object : _frame.selected)
  {
    const auto found = replica.Devices().find(object.id);
    if (object.kind != ObjectKind::Device || found == replica.Devices().end() || found->second.state.seat != replica.Seat())
    {
      continue;
    }
    Order order{};
    order.seat = replica.Seat();
    order.kind = _kind;
    order.operands[0] = static_cast<std::int32_t>(object.id);
    if (_kind == OrderKind::Guard)
    {
      order.operands[1] = found->second.state.x * SUBUNITS_PER_WIRE_UNIT;
      order.operands[2] = found->second.state.z * SUBUNITS_PER_WIRE_UNIT;
    }
    _outOrders.push_back(order);
  }
}

void Hud::OnProduce(std::int32_t _row, const Frame& _frame, std::vector<Order>& _outOrders)
{
  // §7.2: "clicking one appends it to that factory's queue (SetProduction)". The row is an index
  // into the list as RefreshProduce built it, which is this seat's designs in the store's order.
  const Replica& replica = _frame.match->Commander();
  std::int32_t at = 0;
  std::uint32_t design = 0;
  bool found = false;
  for (const DesignState& held : replica.Designs().All())
  {
    if (held.seat != replica.Seat())
    {
      continue;
    }
    if (at == _row)
    {
      design = held.index;
      found = true;
      break;
    }
    ++at;
  }
  if (!found)
  {
    return;
  }
  for (const SelectedObject& object : _frame.selected)
  {
    const auto structure = replica.Structures().find(object.id);
    if (object.kind != ObjectKind::Structure || structure == replica.Structures().end() || structure->second.state.seat != replica.Seat() ||
        structure->second.state.design >= _frame.content->structures.structures.size() ||
        _frame.content->structures.structures[structure->second.state.design].role != StructureRole::Factory)
    {
      continue;
    }
    Order order{};
    order.seat = replica.Seat();
    order.kind = OrderKind::SetProduction;
    order.operands[0] = static_cast<std::int32_t>(object.id);
    order.operands[1] = static_cast<std::int32_t>(design);
    order.operands[2] = 1; // One, and not a repeat count: §7.2's click appends one entry.
    _outOrders.push_back(order);
  }
}

void Hud::OnResearch(std::int32_t _row, const Frame& _frame, std::vector<Order>& _outOrders)
{
  // §7.3: "clicking a row starts it in the first idle lab (SetResearch)". The list holds only the
  // rows that were offered, so the click's index is walked back to the content row the same way it
  // was built - one loop, one rule, and no second table to fall out of step.
  const Replica& replica = _frame.match->Commander();
  const SeatState& seat = replica.Own();
  std::int32_t at = 0;
  std::uint32_t item = 0;
  bool found = false;
  for (std::size_t row = 0; row < _frame.content->research.size() && row < RESEARCH_MASK_BITS; ++row)
  {
    if ((seat.researchComplete & (std::uint64_t{1} << row)) != 0)
    {
      continue;
    }
    bool ready = true;
    for (const std::string& prerequisite : _frame.content->research[row].prerequisites)
    {
      ready = ready && Unlocked(*_frame.content, seat.researchComplete, prerequisite);
    }
    if (!ready)
    {
      continue;
    }
    if (at == _row)
    {
      item = static_cast<std::uint32_t>(row);
      found = true;
      break;
    }
    ++at;
  }
  if (!found)
  {
    return;
  }
  // THE FIRST IDLE LAB IS THE FIRST LAB THIS COMMANDER HOLDS. SeatState carries one research row
  // for the whole seat (Net/Records.h), so "idle" is a property of the commander and not of a lab,
  // and M1 gives a seat one lab; a milestone with two needs the lab's own row on the wire.
  for (const auto& standing : replica.Structures())
  {
    const StructureState& state = standing.second.state;
    if (state.seat != replica.Seat() || standing.second.ghost || state.phase != StructurePhase::Standing ||
        state.design >= _frame.content->structures.structures.size() ||
        _frame.content->structures.structures[state.design].role != StructureRole::ResearchLab)
    {
      continue;
    }
    Order order{};
    order.seat = replica.Seat();
    order.kind = OrderKind::SetResearch;
    order.operands[0] = static_cast<std::int32_t>(state.id);
    order.operands[1] = static_cast<std::int32_t>(item);
    _outOrders.push_back(order);
    return;
  }
}

void Hud::OnDesignEvent(std::uint32_t _widget, const Frame& _frame, std::vector<Order>& _outOrders)
{
  // §7.4's three columns and its Save. The rows are indices into the content tables, which is what
  // SaveDesign's operands are (Sim/Order.h): a chassis row, a drive row, and up to four module rows
  // packed one to a byte.
  if (_widget >= ID_DESIGN_CHASSIS_BASE && _widget < ID_DESIGN_CHASSIS_BASE + _frame.content->components.chassis.size())
  {
    m_designChassis = _widget - ID_DESIGN_CHASSIS_BASE;
    return;
  }
  if (_widget >= ID_DESIGN_DRIVE_BASE && _widget < ID_DESIGN_DRIVE_BASE + _frame.content->components.drives.size())
  {
    m_designDrive = _widget - ID_DESIGN_DRIVE_BASE;
    return;
  }
  if (_widget >= ID_DESIGN_MODULE_BASE && _widget < ID_DESIGN_MODULE_BASE + _frame.content->components.modules.size())
  {
    const std::uint32_t module = _widget - ID_DESIGN_MODULE_BASE;
    const auto mounted = std::find(m_designModules.begin(), m_designModules.end(), module);
    if (mounted != m_designModules.end())
    {
      m_designModules.erase(mounted);
    }
    else if (m_designModules.size() < DESIGN_MODULES_ON_THE_WIRE)
    {
      // FOUR AND NOT MAX_MOUNTS' EIGHT. SaveDesign packs the module rows one to a byte of a single
      // operand (Sim/Order.h), so four is what an order can carry however many mounts a chassis
      // has; a fifth would be silently dropped by the encoder, which is worse than a button that
      // does nothing.
      m_designModules.push_back(module);
    }
    return;
  }
  if (_widget != ID_DESIGN_SAVE)
  {
    return;
  }
  const Replica& replica = _frame.match->Commander();
  Order order{};
  order.seat = replica.Seat();
  order.kind = OrderKind::SaveDesign;
  // THE INDEX IS THE NEXT FREE ONE THIS COMMANDER HOLDS. There is no delete and no rename in M1
  // (§7.4), so a save is always a new row until the seat is full, at which point the host refuses
  // it - which is the correction §6 draws.
  std::uint32_t next = 0;
  for (const DesignState& held : replica.Designs().All())
  {
    next = held.seat == replica.Seat() && held.index >= next ? held.index + 1 : next;
  }
  order.operands[0] = static_cast<std::int32_t>(next);
  order.operands[1] = static_cast<std::int32_t>(m_designChassis);
  order.operands[2] = static_cast<std::int32_t>(m_designDrive);
  std::int32_t packed = 0;
  for (std::size_t mount = 0; mount < DESIGN_MODULES_ON_THE_WIRE; ++mount)
  {
    const std::int32_t row =
      mount < m_designModules.size() ? static_cast<std::int32_t>(m_designModules[mount]) : static_cast<std::int32_t>(NO_PACKED_MODULE);
    packed |= row << (mount * 8);
  }
  order.operands[3] = packed;
  _outOrders.push_back(order);
}

bool Hud::TakeArmRequest(ArmedOrder& _outArmed, std::uint32_t& _outRow) noexcept
{
  if (!m_armRequested)
  {
    return false;
  }
  m_armRequested = false;
  _outArmed = m_armRequest;
  _outRow = m_armRequestRow;
  return true;
}

bool Hud::TakePortraitPick(std::uint32_t& _outId) noexcept
{
  if (!m_portraitPicked)
  {
    return false;
  }
  m_portraitPicked = false;
  _outId = m_portraitPick;
  return true;
}

} // namespace Outpost
