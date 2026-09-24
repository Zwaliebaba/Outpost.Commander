#pragma once

#include "NeuronClient.h"

#include <cstddef>
#include <cstdint>

namespace Outpost
{

/// **EVERY RECTANGLE OF THE INTERFACE, IN INTEGER AUTHORED COORDINATES** (M1.14).
///
/// The numbers are `design_handoff_hud/geometry.json`'s and are **taken, not derived** -- the plan is
/// explicit that this step does not invent geometry. Each constant carries a `// geometry:` tag naming
/// the JSON row it copies, and `Scripts/CheckHudGeometry.py` reads those tags and fails when the two
/// disagree. **Python reads the JSON and C++ reads none**, because R14 closes the dependency list and
/// there is no JSON parser in this tree; the tag is the whole of the contract between them.
///
/// A rect is **x, y, width, height** here rather than the edges `Neuron::AuthoredRect` carries, because
/// that is how the handoff states every one of them and a comparison against it should be a comparison
/// of four numbers rather than of four numbers after arithmetic. `Authored()` converts.
///
/// **A ROW WHOSE JSON WIDTH IS NULL IS A TEXT BOX**, and the width here is the box this client clips it
/// to. The gate compares the fields the JSON states and skips the ones it leaves null.
///
/// R8: a public aggregate.
struct HudRect
{
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t w = 0;
  std::int32_t h = 0;

  [[nodiscard]] constexpr Neuron::AuthoredRect Authored() const noexcept
  {
    return Neuron::AuthoredRect{.left = x, .top = y, .right = x + w, .bottom = y + h};
  }

  /// A cell-relative rect placed inside its cell.
  [[nodiscard]] constexpr HudRect Within(const HudRect& _cell) const noexcept
  {
    return HudRect{.x = _cell.x + x, .y = _cell.y + y, .w = w, .h = h};
  }

  [[nodiscard]] constexpr bool Contains(float _x, float _y) const noexcept
  {
    return (_x >= static_cast<float>(x)) && (_x < static_cast<float>(x + w)) && (_y >= static_cast<float>(y)) &&
           (_y < static_cast<float>(y + h));
  }

  [[nodiscard]] friend constexpr bool operator==(const HudRect&, const HudRect&) noexcept = default;
};

/// `Interface.md` §1's three tiers. **A control smaller than its tier is a defect rather than a style
/// choice**, and the suite asserts every target against the one it declares.
enum class TouchTier : std::uint8_t
{
  /// 48 x 48: anything interactive, and nothing is ever smaller.
  Floor,
  /// 64 x 64: the selection groups, clear, and the build item's cancel.
  Combat,
  /// 96 x 96: the build buttons, which are hit while something is exploding.
  UnderFire
};

/// The side of each tier's square, in authored pixels -- `Interface.md` §1's table, which this echoes
/// rather than decides. The gate checks `geometry.json`'s copy against that table; this is the third.
[[nodiscard]] constexpr std::int32_t TierPixels(TouchTier _tier) noexcept
{
  switch (_tier)
  {
  case TouchTier::Floor:
    return 48;
  case TouchTier::Combat:
    return 64;
  case TouchTier::UnderFire:
    return 96;
  }
  return 48;
}

/// **Sixteen pixels of clear space** between adjacent interactive targets (`Interface.md` §1). Twelve
/// pairs in the handoff sit exactly on it, so there is no slack anywhere.
inline constexpr std::int32_t CLEAR_SPACE_PIXELS = 16;

/// **THE ONE REFLECTION, AND IT IS THE WHOLE OF Q33's COST.** `x' = 1440 - x - w`, applied to the
/// selection and build panels and everything in them; credits, system and the overlays do not move.
[[nodiscard]] constexpr HudRect Mirror(const HudRect& _rect) noexcept
{
  return HudRect{.x = Neuron::INTERFACE_AUTHORED_WIDTH - _rect.x - _rect.w, .y = _rect.y, .w = _rect.w, .h = _rect.h};
}

/// A bottom-panel rect for the handedness in force. **Right-handed is the primary layout** -- build under
/// the reaching hand bottom right, selection bottom left where it is read while that hand is on the
/// glass -- and left-handed is its mirror (`OpenQuestions.md` Q33's recommendation).
[[nodiscard]] constexpr HudRect ForHand(const HudRect& _primary, bool _leftHanded) noexcept
{
  return _leftHanded ? Mirror(_primary) : _primary;
}

/// A bar that sits against one side of its cell -- an index bar -- reflected **within** the cell. The
/// cell itself moves with `ForHand`; its text and its hull bar are symmetric about its middle and keep
/// their offsets, which is what the handoff's left-handed frame draws.
[[nodiscard]] constexpr HudRect ReflectWithin(const HudRect& _offset, std::int32_t _cellWidth, bool _leftHanded) noexcept
{
  return _leftHanded ? HudRect{.x = _cellWidth - _offset.x - _offset.w, .y = _offset.y, .w = _offset.w, .h = _offset.h} : _offset;
}

// ===================================================================================================
// Credits -- top left, non-interactive.
// ===================================================================================================

inline constexpr HudRect CREDITS_PANEL{0, 0, 272, 88};       // geometry: credits.panel
inline constexpr HudRect CREDITS_INDEX{0, 0, 3, 88};         // geometry: credits.index
inline constexpr HudRect CREDITS_RULE_BOTTOM{0, 87, 272, 1}; // geometry: credits.rule.bottom
inline constexpr HudRect CREDITS_RULE_RIGHT{271, 0, 1, 88};  // geometry: credits.rule.right
inline constexpr HudRect CREDITS_TICK_LEFT{0, 86, 16, 2};    // geometry: credits.tick.left
inline constexpr HudRect CREDITS_TICK_RIGHT{256, 86, 16, 2}; // geometry: credits.tick.right
inline constexpr HudRect CREDITS_LABEL{16, 14, 240, 20};     // geometry: credits.label
inline constexpr HudRect CREDITS_VALUE{16, 40, 240, 32};     // geometry: credits.value

/// **THE CHANGE FLASH** (M2.7): cyan on a gain, amber on a spend, decaying exponentially (the handoff's
/// motion table, `GameClient/CreditFlash.h`). It is the whole of how income is shown (`OpenQuestions.md` Q36).
inline constexpr HudRect CREDITS_FLASH{16, 76, 120, 3}; // geometry: credits.flash

// ===================================================================================================
// System -- top center, small; and the quit confirm it expands into.
// ===================================================================================================

inline constexpr HudRect SYSTEM_PANEL{624, 0, 192, 64};       // geometry: system.panel
inline constexpr HudRect SYSTEM_INDEX{624, 0, 3, 64};         // geometry: system.index
inline constexpr HudRect SYSTEM_RULE_BOTTOM{624, 63, 192, 1}; // geometry: system.rule.bottom
inline constexpr HudRect SYSTEM_TICK_LEFT{624, 62, 16, 2};    // geometry: system.tick.left
inline constexpr HudRect SYSTEM_TICK_RIGHT{800, 62, 16, 2};   // geometry: system.tick.right
inline constexpr HudRect SYSTEM_LINK_DOT{640, 26, 12, 12};    // geometry: system.link.dot
inline constexpr HudRect SYSTEM_LINK_LABEL{660, 22, 72, 20};  // geometry: system.link.label
inline constexpr HudRect SYSTEM_QUIT{736, 8, 64, 48};         // geometry: system.quit

inline constexpr HudRect CONFIRM_PANEL{592, 0, 256, 152}; // geometry: system.confirm.panel
inline constexpr HudRect CONFIRM_WARN{608, 48, 224, 20};  // geometry: system.confirm.warn
inline constexpr HudRect CONFIRM_STAY{608, 80, 96, 64};   // geometry: system.confirm.stay
inline constexpr HudRect CONFIRM_QUIT{720, 80, 96, 64};   // geometry: system.confirm.quit

/// Inside the confirm panel, from the handoff's own drawing of it: the link state keeps its place
/// relative to the panel's left edge, and the panel's index bar turns `SIG.SHORT`.
inline constexpr HudRect CONFIRM_INDEX{592, 0, 3, 152};
inline constexpr HudRect CONFIRM_RULE_BOTTOM{592, 151, 256, 1};
inline constexpr HudRect CONFIRM_LINK_DOT{608, 26, 12, 12};
inline constexpr HudRect CONFIRM_LINK_LABEL{628, 22, 200, 20};

/// **FOUR THOUSAND MILLISECONDS**, after which an armed quit disarms itself (`Interface.md` §6), so it
/// cannot be left lying on the glass for a stray contact to finish.
inline constexpr std::uint64_t QUIT_ARMED_MILLISECONDS = 4000;

// ===================================================================================================
// The frame's registration ticks, at its quarter points.
// ===================================================================================================

inline constexpr HudRect FRAME_TICK_LEFT_UPPER{0, 192, 14, 2};     // geometry: frame.reg.tick.left.upper
inline constexpr HudRect FRAME_TICK_LEFT_MID{0, 480, 14, 2};       // geometry: frame.reg.tick.left.mid
inline constexpr HudRect FRAME_TICK_RIGHT_UPPER{1426, 192, 14, 2}; // geometry: frame.reg.tick.right.upper
inline constexpr HudRect FRAME_TICK_RIGHT_MID{1426, 480, 14, 2};   // geometry: frame.reg.tick.right.mid

// ===================================================================================================
// Selection -- bottom, away from the reaching hand. Its width is a function of the group count.
// ===================================================================================================

/// One to four groups. Four is the worst case the handoff draws and the most the panel has room for.
inline constexpr std::size_t MAXIMUM_SELECTION_GROUPS = 4;

inline constexpr std::int32_t SELECTION_PANEL_TOP = 832;
inline constexpr std::int32_t SELECTION_PANEL_HEIGHT = 128;
inline constexpr std::int32_t SELECTION_GROUP_WIDTH = 160;
inline constexpr std::int32_t SELECTION_GROUP_HEIGHT = 96;
inline constexpr std::int32_t SELECTION_GROUP_TOP = 848;

/// The handoff's three formulas, as functions of `n`, **in the primary layout**.
[[nodiscard]] constexpr std::int32_t SelectionClearX(std::size_t _groups) noexcept
{
  const std::int32_t n = static_cast<std::int32_t>(_groups);
  return 16 + (160 * n) + (16 * (n - 1)) + 16;
}

[[nodiscard]] constexpr HudRect SelectionPanelRect(std::size_t _groups) noexcept
{
  return HudRect{.x = 0, .y = SELECTION_PANEL_TOP, .w = SelectionClearX(_groups) + 96 + 16, .h = SELECTION_PANEL_HEIGHT};
}

[[nodiscard]] constexpr HudRect SelectionGroupRect(std::size_t _index) noexcept
{
  return HudRect{
    .x = 16 + (176 * static_cast<std::int32_t>(_index)), .y = SELECTION_GROUP_TOP, .w = SELECTION_GROUP_WIDTH, .h = SELECTION_GROUP_HEIGHT};
}

[[nodiscard]] constexpr HudRect SelectionClearRect(std::size_t _groups) noexcept
{
  return HudRect{.x = SelectionClearX(_groups), .y = 864, .w = 96, .h = 64};
}

/// The panel and its targets **at four groups**, which is the row `geometry.json` states. Written out
/// as literals so the gate can read them, and tied to the formulas above by the compiler below -- two
/// statements of one figure with something between them.
inline constexpr HudRect SELECTION_PANEL_AT_FOUR{0, 832, 832, 128};      // geometry: sel.panel
inline constexpr HudRect SELECTION_RULE_TOP_AT_FOUR{0, 832, 832, 1};     // geometry: sel.rule.top
inline constexpr HudRect SELECTION_RULE_INNER_AT_FOUR{831, 832, 1, 128}; // geometry: sel.rule.inner
inline constexpr HudRect SELECTION_GROUP_0{16, 848, 160, 96};            // geometry: sel.group[0]
inline constexpr HudRect SELECTION_GROUP_1{192, 848, 160, 96};           // geometry: sel.group[1]
inline constexpr HudRect SELECTION_GROUP_2{368, 848, 160, 96};           // geometry: sel.group[2]
inline constexpr HudRect SELECTION_GROUP_3{544, 848, 160, 96};           // geometry: sel.group[3]
inline constexpr HudRect SELECTION_CLEAR_AT_FOUR{720, 864, 96, 64};      // geometry: sel.clear

static_assert(SelectionPanelRect(4) == SELECTION_PANEL_AT_FOUR, "the panel-width formula and geometry.json disagree");
static_assert(SelectionClearRect(4) == SELECTION_CLEAR_AT_FOUR, "the clear-position formula and geometry.json disagree");
static_assert(SelectionGroupRect(0) == SELECTION_GROUP_0 && SelectionGroupRect(1) == SELECTION_GROUP_1 &&
                SelectionGroupRect(2) == SELECTION_GROUP_2 && SelectionGroupRect(3) == SELECTION_GROUP_3,
              "the group-position formula and geometry.json disagree");
static_assert(SelectionPanelRect(1).w == 304 && SelectionPanelRect(2).w == 480 && SelectionPanelRect(3).w == 656,
              "the handoff states the panel at one, two and three groups as 304, 480 and 656");

/// Inside a group cell, as offsets from the cell.
inline constexpr HudRect GROUP_INDEX{0, 0, 4, 96};           // geometry: sel.group.index
inline constexpr HudRect GROUP_COUNT{16, 10, 128, 32};       // geometry: sel.group.count
inline constexpr HudRect GROUP_NAME{16, 18, 128, 20};        // geometry: sel.group.name
inline constexpr HudRect GROUP_HULL_TROUGH{16, 52, 128, 10}; // geometry: sel.group.hull.trough
inline constexpr HudRect GROUP_HULL_INNER{17, 53, 126, 8};   // geometry: sel.group.hull.inner
inline constexpr HudRect GROUP_HULL_TICK_0{32, 53, 1, 8};    // geometry: sel.group.hull.tick[0]
inline constexpr HudRect GROUP_HULL_TICK_1{64, 53, 1, 8};    // geometry: sel.group.hull.tick[1]
inline constexpr HudRect GROUP_HULL_TICK_2{95, 53, 1, 8};    // geometry: sel.group.hull.tick[2]

/// **THE FOUR CARGO CHIPS** (M2.7): the first, and each after it `GROUP_CARGO_CHIP_STEP` further right. Drawn
/// only for a design that carries ore -- a design that does not draws no row at all, and the hull row does not
/// move (`design_handoff_hud` section 2). How many light is the record's flags (`OpenQuestions.md` Q53).
inline constexpr HudRect GROUP_CARGO_CHIP{16, 72, 29, 10}; // geometry: sel.group.cargo.chip[i]
inline constexpr std::int32_t GROUP_CARGO_CHIP_STEP = 33;

/// The clear target's label, centered in the button as the handoff draws it.
inline constexpr HudRect CLEAR_LABEL{0, 22, 96, 20};

// ===================================================================================================
// Build -- bottom, on the reaching hand's side.
// ===================================================================================================

// **112 TALLER SINCE M4.4b** (the owner, 2026-09-24, Q85): a third row on top, for research. It grows upward, so every
// row below it stays where the handoff put it.
inline constexpr HudRect BUILD_PANEL{880, 528, 560, 432};    // geometry: build.panel
inline constexpr HudRect BUILD_RULE_TOP{880, 528, 560, 1};   // geometry: build.rule.top
inline constexpr HudRect BUILD_RULE_INNER{880, 528, 1, 432}; // geometry: build.rule.inner
inline constexpr HudRect BUILD_TICK_TOP_INNER{880, 528, 16, 2};
inline constexpr HudRect BUILD_TICK_TOP_OUTER{1424, 528, 16, 2};
inline constexpr HudRect BUILD_TICK_BOTTOM_INNER{880, 944, 2, 16};

/// **THE SHIP ROW, BY PLACE AND NOT BY NAME.** The handoff labels them with the two MVP designs; the
/// panel puts whatever the design table marks buildable into them in table order (ADR-006), so the
/// constants are named for where they are rather than for what happens to be in them today.
inline constexpr HudRect BUILD_BUTTON_SHIP_0{896, 656, 120, 96};  // geometry: build.btn.miner
inline constexpr HudRect BUILD_BUTTON_SHIP_1{1032, 656, 120, 96}; // geometry: build.btn.fighter

/// The module row's four places, drawn since M2.11: each arms a placement, or for an L2 an upgrade
/// (`OpenQuestions.md` Q54), rather than queueing anything itself.
inline constexpr HudRect BUILD_BUTTON_YARD_L1{896, 768, 120, 96};  // geometry: build.btn.yard.l1
inline constexpr HudRect BUILD_BUTTON_YARD_L2{1032, 768, 120, 96}; // geometry: build.btn.yard.l2
inline constexpr HudRect BUILD_BUTTON_ORE_L1{1168, 768, 120, 96};  // geometry: build.btn.ore.l1
inline constexpr HudRect BUILD_BUTTON_ORE_L2{1304, 768, 120, 96};  // geometry: build.btn.ore.l2

/// **THE DEPOT'S BUTTON** (M3.9, `OpenQuestions.md` Q69): the ship row's third place, which the handoff leaves empty.
/// **Not in `geometry.json`** -- the handoff predates the depot -- so it is not tagged and the geometry check does not
/// claim it; it takes the ship buttons' size and their 16 of clear space.
inline constexpr HudRect BUILD_BUTTON_DEPOT{1168, 656, 120, 96};

/// **THE SHIP ROW'S THIRD SHIP** (M4.4): the Cruiser, the third buildable design in table order, in the row's last free
/// place. Not in `geometry.json` for the depot's reason -- the handoff predates it -- and at the ship buttons' size.
inline constexpr HudRect BUILD_BUTTON_SHIP_2{1304, 656, 120, 96};

/// **THE RESEARCH ROW** (M4.4b, the owner's layout, Q85): the research station, placed as a module is, and the button
/// that starts research. Untagged for the depot's reason, at the ship buttons' size and 16 clear of the row below.
inline constexpr HudRect BUILD_BUTTON_RESEARCH_STATION{896, 544, 120, 96};
inline constexpr HudRect BUILD_BUTTON_RESEARCH{1032, 544, 120, 96};

/// Inside a build button, as offsets from the button.
inline constexpr HudRect BUTTON_INDEX{0, 0, 4, 96};          // geometry: build.btn.index
inline constexpr HudRect BUTTON_NAME_LINE1{12, 10, 104, 20}; // geometry: build.btn.name.line1
inline constexpr HudRect BUTTON_NAME_LINE2{12, 32, 104, 20}; // geometry: build.btn.name.line2
inline constexpr HudRect BUTTON_COST{12, 48, 96, 32};        // geometry: build.btn.cost
inline constexpr HudRect BUTTON_SHORT_RULE{0, 93, 120, 3};   // geometry: build.btn.short.rule

inline constexpr HudRect BUILD_PROGRESS{896, 880, 528, 64};       // geometry: build.progress
inline constexpr HudRect BUILD_PROGRESS_INDEX{896, 880, 4, 64};   // geometry: build.progress.index
inline constexpr HudRect BUILD_PROGRESS_NAME{912, 890, 300, 20};  // geometry: build.progress.name
inline constexpr HudRect BUILD_PROGRESS_TRACK{912, 920, 396, 12}; // geometry: build.progress.track
inline constexpr HudRect BUILD_CANCEL{1328, 880, 96, 64};         // geometry: build.cancel

/// The cancel's label, centered in the button.
inline constexpr HudRect CANCEL_LABEL{0, 22, 96, 20};

// ===================================================================================================
// The two overlays.
// ===================================================================================================

inline constexpr HudRect OVERLAY_SCRIM{0, 0, 1440, 960};             // geometry: overlay.scrim
inline constexpr HudRect OVERLAY_RESULT_BLOCK{440, 368, 560, 224};   // geometry: overlay.result.block
inline constexpr HudRect OVERLAY_RECONNECT_BLOCK{540, 436, 360, 88}; // geometry: overlay.reconnect.block

} // namespace Outpost
