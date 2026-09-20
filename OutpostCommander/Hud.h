#pragma once

#include "Match.h"

#include "OrderInput.h"
#include "Picking.h"

#include "Design.h"
#include "Plan.h"

#include "DesignStats.h"
#include "InterfaceDesc.h"
#include "StructureDesc.h"

#include "Minimap.h"
#include "UiDraw.h"
#include "UiInputSink.h"
#include "UiPanel.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The panels (Design/Interface.md §7 to §9; m1-vertical-slice/K4): the command panel's five tabs,
// the selection panel, the orders panel, the minimap and the two readouts. It reads the replica and
// emits Orders, and nothing else - which is the acceptance line in as many words, and is what makes
// the panels a different way of saying what §6's hotkeys already say.
//
// IT IS IN THE EXECUTABLE FOR THE REASON Operator.h GIVES: it needs Client's toolkit and Replica's
// records, and ADR-001 makes those siblings over Core, so only an executable may hold both. What
// could be a library already is - UiPanel, UiWidgets and UiDraw in Client, PlacementPreview and
// OrderInput in Replica, the minimap's picture in Client/Minimap.h - and what is left is the
// reading of one and the filling of the other.
//
// EVERY PANEL IS REBUILT FROM THE REPLICA EVERY FRAME, and the widgets therefore carry NAMED ids
// rather than the numbers UiPanel::Add hands out. A button acts on the release and only inside the
// widget it was pressed in, so an id that moved to another control between the press and the
// release would fire the wrong one; a named id cannot move (Client/UiPanel.h's Reset says so too).

namespace Outpost
{

/// §2's rectangles, in authored pixels, under the document's own names.
inline constexpr Neuron::UiRect MINIMAP_PANEL_RECT{0, 792, 288, 288};
inline constexpr Neuron::UiRect SELECTION_PANEL_RECT{288, 792, 512, 288};
inline constexpr Neuron::UiRect COMMAND_PANEL_RECT{800, 792, 768, 288};
inline constexpr Neuron::UiRect ORDERS_PANEL_RECT{1568, 792, 352, 288};
inline constexpr Neuron::UiRect POWER_READOUT_RECT{0, 0, 400, 40};
inline constexpr Neuron::UiRect MATCH_READOUT_RECT{1520, 0, 400, 40};

/// Where the minimap's own 256x256 image sits inside its panel (§9.2: "the client area is exactly
/// 256x256"), centred in the 288 the panel is.
inline constexpr Neuron::UiRect MINIMAP_IMAGE_RECT{16, 808, 256, 256};

/// The command panel's five tabs (§7), in the order the keys 1 to 5 select them.
enum class HudTab : std::uint8_t
{
  Construct,
  Produce,
  Research,
  Design,
  Match
};

inline constexpr std::uint8_t HUD_TAB_COUNT = 5;

/// §8's portrait grid: "up to 32 portraits, 48x48 each"; "over 32 selected, the grid shows the
/// first 32 and a count". Eight to a row fills the 512-pixel panel's client area exactly.
inline constexpr std::uint32_t PORTRAITS_SHOWN = 32;
inline constexpr std::int32_t PORTRAITS_PER_ROW = 8;

/// How many modules a SaveDesign order can carry: its fourth operand packs them one to a byte
/// (Sim/Order.h), which bounds a saved design to four mounts against MAX_MOUNTS' eight. The design
/// tab stops at four rather than letting the encoder drop the fifth silently.
inline constexpr std::size_t DESIGN_MODULES_ON_THE_WIRE = 4;

class Hud
{
public:
  /// What a frame gives the panels. The abilities are the Operator's own, so that the selection is
  /// read once a frame rather than once by each of them.
  struct Frame
  {
    const Match* match = nullptr;
    const ContentTree* content = nullptr;
    std::span<const SelectedObject> selected;
    /// THE REPLICA'S NEWEST TICK and not the host's (Design/Interface.md §9.4: "the elapsed match
    /// time as mm:ss FROM THE REPLICA'S TICK"). A clock read off LocalHost would be the host's own
    /// and would run ahead of everything else on the screen by the interpolation delay, which is a
    /// readout disagreeing with the world beside it for no reason a player could ever discover.
    std::uint32_t tick = 0;
    /// The camera, for the frustum quadrilateral §9.2 draws on the map. Four rays and a plane.
    PickCamera camera;
    /// What the commander is placing, for the construct tab's own button to show as armed.
    ArmedOrder armed = ArmedOrder::None;
    std::uint32_t armedStructure = 0;
  };

  Hud();

  /// Rebuilds every panel from the replica. Once a frame, AFTER the frame's game logic and before
  /// the frame is drawn - which puts it before the NEXT frame's events are dispatched, so that a
  /// click lands on the panel the commander was actually looking at when he pressed the button.
  /// Refreshing after the dispatch instead would test a click against a panel built from a replica
  /// a frame newer than the picture he clicked on.
  void Refresh(const Frame& _frame);

  /// Turns a panel event into orders and into whatever the panel itself changes - the tab, the
  /// armed structure. Appends nothing for an event that changed no game state.
  void OnEvent(const Neuron::UiEventResult& _result, const Frame& _frame, std::vector<Order>& _outOrders);

  /// The key 1 to 5, or any other key, for §7's tab hotkeys. True when the key was a tab's.
  bool TakeTabKey(std::uint8_t _key) noexcept;

  void Register(Neuron::UiInputSink& _sink);

  /// Shows or hides every panel at once, for §7's F1: "toggle the panel overlay off, for a clean
  /// look at the world". An invisible panel draws nothing, takes no click and blocks no ray, which
  /// is all three halves of being off.
  void SetVisible(bool _visible) noexcept;

  /// The interface's quads, back to front, and the minimap's own image with them.
  void Build(const ChromePalette& _palette, const Neuron::IconAtlas& _icons, std::vector<Neuron::UiQuad>& _outQuads) const;

  /// The minimap's pixels for this frame, or empty before there is a landscape. The pass uploads
  /// them; they are built by Refresh, so that one walk of the replica serves the map and the panels.
  [[nodiscard]] std::span<const std::uint8_t> MinimapPixels() const noexcept
  {
    return m_minimap;
  }

  /// The rectangles a world click may not be cast through (§5). Every visible panel.
  void BlockedBy(std::vector<PickBox>& _outBoxes) const;

  /// Where a click on the minimap lands on the landscape, for §9.2's left and right click. Inside
  /// is false for a click anywhere else, including the panel's border.
  [[nodiscard]] Neuron::MinimapPoint MinimapPointAt(std::int32_t _x, std::int32_t _y) const noexcept;

  [[nodiscard]] HudTab Tab() const noexcept
  {
    return m_tab;
  }

  /// What a button last asked to arm, and whether it asked since the last call. THE OPERATOR ARMS
  /// IT, because arming is OrderInput's state and a panel holds none of the game's: §6 gives the
  /// armed order one owner and §9.1's buttons are a second way of reaching it, not a second copy.
  /// _outRow is the structure row and is meaningless unless _outArmed is PlaceStructure.
  [[nodiscard]] bool TakeArmRequest(ArmedOrder& _outArmed, std::uint32_t& _outRow) noexcept;

  /// The device a click on a portrait asked to narrow the selection to (§8). The Operator holds the
  /// selection, for the reason above: one owner, and the panel asks.
  [[nodiscard]] bool TakePortraitPick(std::uint32_t& _outId) noexcept;

private:
  void RefreshCommand(const Frame& _frame);
  void RefreshConstruct(const Frame& _frame);
  void RefreshProduce(const Frame& _frame);
  void RefreshResearch(const Frame& _frame);
  void RefreshDesign(const Frame& _frame);
  void RefreshMatch(const Frame& _frame);
  void RefreshSelection(const Frame& _frame);
  void RefreshDevices(const Frame& _frame, std::span<const ReplicaDevice* const> _devices);
  void RefreshOrders(const Frame& _frame);
  void RefreshReadouts(const Frame& _frame);
  void RefreshMinimap(const Frame& _frame);
  [[nodiscard]] std::string RoleLineOf(const Frame& _frame, const ReplicaStructure& _structure, const StructureDesc& _row) const;
  [[nodiscard]] static std::array<float, 8> FrustumGround(const Frame& _frame, std::uint32_t _cellsPerSide);
  void OnPrimaryOrder(OrderKind _kind, const Frame& _frame, std::vector<Order>& _outOrders);
  void OnProduce(std::int32_t _row, const Frame& _frame, std::vector<Order>& _outOrders);
  void OnResearch(std::int32_t _row, const Frame& _frame, std::vector<Order>& _outOrders);
  void OnDesignEvent(std::uint32_t _widget, const Frame& _frame, std::vector<Order>& _outOrders);

  HudTab m_tab = HudTab::Construct;
  Neuron::UiPanel m_command;
  Neuron::UiPanel m_selection;
  Neuron::UiPanel m_orders;
  Neuron::UiPanel m_minimapPanel;
  Neuron::UiPanel m_power;
  Neuron::UiPanel m_matchState;
  std::vector<std::uint8_t> m_minimap;
  /// The landscape's size, for turning a minimap pixel back into a world point. Zero until the
  /// join has answered, and a click then lands nowhere rather than on a landscape of no cells.
  std::uint32_t m_minimapCells = 0;
  std::vector<Neuron::MinimapBlip> m_blips;
  std::vector<std::uint32_t> m_commanderColors;
  /// What the design tab has chosen, as row indices into the content tables.
  std::uint32_t m_designChassis = 0;
  std::uint32_t m_designDrive = 0;
  std::vector<std::uint32_t> m_designModules;
  std::string m_designName = "DESIGN";
  /// Which of the selected devices the detail lines describe, as an index into the selection (§8's
  /// "a one-pixel accent border on the one the panel's detail lines describe"). Clamped by every
  /// Refresh, so a selection that shrank under it describes the last device rather than nothing.
  std::uint32_t m_detail = 0;
  /// Set by a click on a button that arms, taken by the Operator.
  ArmedOrder m_armRequest = ArmedOrder::None;
  std::uint32_t m_armRequestRow = 0;
  bool m_armRequested = false;
  std::uint32_t m_portraitPick = 0;
  bool m_portraitPicked = false;
  /// THE INCOME, MEASURED RATHER THAN RE-DERIVED (§9.3). SeatState::extractedHundredths is the
  /// running total of what this seat's served extractors have yielded, so the rate is the rise over
  /// the ticks between two readings of it - which needs neither the generator service assignment
  /// nor the seat's research upgrades, and cannot disagree with the host about either. The command
  /// post's trickle is added on top from the replica's own structures, because it is a flat row
  /// value that no assignment touches.
  ///
  /// SAMPLED OVER A SECOND AND NOT OVER A FRAME. A frame lands every publish - two ticks - and the
  /// extraction of two ticks divided by two is a figure that jumps whenever a generator picks up an
  /// extractor mid-interval; over twenty ticks it is the number §9.3 prints.
  std::int64_t m_sampleExtracted = 0;
  std::uint32_t m_sampleTick = 0;
  std::int32_t m_extractionHundredthsPerTick = 0;
};

} // namespace Outpost
