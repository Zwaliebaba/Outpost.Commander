#include "pch.h"

#include "Panels.h"

#include <algorithm>
#include <array>

namespace Outpost
{

namespace
{
// ===================================================================================================
// THE PALETTE -- `design_handoff_hud/palette.json`, as the bytes the back buffer shows. See
// `Neuron::GlyphQuad` for why these go in encoded rather than converted to linear.
// ===================================================================================================

constexpr Neuron::QuadColor TEAM_OWN = Neuron::HexColor(0x38D1F5);
constexpr Neuron::QuadColor TEAM_B = Neuron::HexColor(0xC4E838);
constexpr Neuron::QuadColor TEAM_C = Neuron::HexColor(0xF75FD0);
constexpr Neuron::QuadColor TEAM_D = Neuron::HexColor(0xA45CFF);

constexpr Neuron::QuadColor SIG_SHORT = Neuron::HexColor(0xFF5A4A);

constexpr Neuron::QuadColor SCRIM = Neuron::HexColor(0x07090B, 0.88f);
constexpr Neuron::QuadColor PLATE = Neuron::HexColor(0x0E1214, 0.92f);
constexpr Neuron::QuadColor PLATE_QUIT = Neuron::HexColor(0x140806, 0.92f);
constexpr Neuron::QuadColor RULE = Neuron::HexColor(0x313A40);
constexpr Neuron::QuadColor RULE_LIT = Neuron::HexColor(0x5A6A72);
constexpr Neuron::QuadColor TICK = Neuron::HexColor(0x8A9AA2);
constexpr Neuron::QuadColor TICK_BAR = Neuron::HexColor(0x3A454B);
constexpr Neuron::QuadColor TRACK = Neuron::HexColor(0x1A2024);
constexpr Neuron::QuadColor ORE = Neuron::HexColor(0xD8A23C);
constexpr Neuron::QuadColor SIG_ARMED = Neuron::HexColor(0xFFB020);
constexpr Neuron::QuadColor PLATE_ARMED = Neuron::HexColor(0x17120A, 0.94f);
constexpr Neuron::QuadColor TEXT_ARMED = Neuron::HexColor(0xFFE1A8);
constexpr Neuron::QuadColor PLATE_DIM = Neuron::HexColor(0x090C0E, 0.92f);
constexpr Neuron::QuadColor RULE_DIM = Neuron::HexColor(0x232C31);
constexpr Neuron::QuadColor TEXT_DIM = Neuron::HexColor(0x6B7A80);
constexpr Neuron::QuadColor HATCH = Neuron::HexColor(0x5A6A72, 0.22f);

constexpr Neuron::QuadColor TEXT = Neuron::HexColor(0xE8ECEC);
constexpr Neuron::QuadColor TEXT_2 = Neuron::HexColor(0x93A0A5);

constexpr Neuron::QuadColor HULL = Neuron::HexColor(0xCFDDE0);
constexpr Neuron::QuadColor HULL_LOST = Neuron::HexColor(0xFF3B2F, 0.85f);

/// The world's clear color, used by the interface only as the overlays' full-frame scrim.
constexpr std::uint32_t SPACE_HEX = 0x04060A;

/// The alphas the handoff draws at that are not tokens of their own.
constexpr float OWN_INDEX_ALPHA = 0.75f;
constexpr float CREDITS_INDEX_ALPHA = 0.6f;
constexpr float FRAME_TICK_ALPHA = 0.45f;
constexpr float PROGRESS_PLATE_ALPHA = 0.6f;
constexpr float RECONNECT_SCRIM_ALPHA = 0.60f;
constexpr float RESULT_SCRIM_ALPHA = 0.72f;
constexpr float RESULT_BLOCK_ALPHA = 0.9f;

/// Letter-spacing, in em, as the handoff's reference sets it on each kind of label.
constexpr float TRACK_LABEL = 0.10f;
constexpr float TRACK_NAME = 0.06f;
constexpr float TRACK_BUTTON = 0.05f;
constexpr float TRACK_OVERLAY = 0.04f;
constexpr float TRACK_RESULT = 0.14f;

/// A color with its alpha replaced.
[[nodiscard]] constexpr Neuron::QuadColor WithAlpha(Neuron::QuadColor _color, float _alpha) noexcept
{
  _color.alpha = _alpha;
  return _color;
}

/// The emitter's three verbs. Everything the interface draws at M1 is one of them: a rectangle, a
/// rectangle's outline, and a run of text.
class Emitter
{
public:
  explicit Emitter(std::vector<HudItem>& _items)
    : m_items(_items)
  {
  }

  void Solid(const HudRect& _rect, const Neuron::QuadColor& _color)
  {
    if ((_rect.w > 0) && (_rect.h > 0))
    {
      m_items.push_back(HudItem{.kind = HudItem::Kind::Solid, .rect = _rect, .color = _color});
    }
  }

  /// A border inset into the rect, as CSS's `box-sizing: border-box` draws it. Four strips that do not
  /// overlap, so a translucent border is not darker at its corners.
  void Outline(const HudRect& _rect, std::int32_t _width, const Neuron::QuadColor& _color)
  {
    Solid({_rect.x, _rect.y, _rect.w, _width}, _color);
    Solid({_rect.x, _rect.y + _rect.h - _width, _rect.w, _width}, _color);
    Solid({_rect.x, _rect.y + _width, _width, _rect.h - (_width * 2)}, _color);
    Solid({_rect.x + _rect.w - _width, _rect.y + _width, _width, _rect.h - (_width * 2)}, _color);
  }

  void Text(const HudRect& _box, std::wstring _text, Neuron::TextSize _size, Neuron::TextAlign _align, float _trackingEm,
            const Neuron::QuadColor& _color)
  {
    m_items.push_back(HudItem{.kind = HudItem::Kind::Text,
                              .rect = _box,
                              .color = _color,
                              .text = std::move(_text),
                              .size = _size,
                              .align = _align,
                              .trackingEm = _trackingEm});
  }

private:
  std::vector<HudItem>& m_items;
};

/// The display string for a design. **The one place a design has a name**, and it is a display string:
/// nothing in the simulation knows what a fighter is (M1.2), and the panel does not either beyond this.
[[nodiscard]] std::wstring DesignDisplayName(DesignId _design)
{
  switch (_design)
  {
  case DesignId::Miner:
    return L"MINER";
  case DesignId::Fighter:
    return L"FIGHTER";
  case DesignId::Station:
    return L"STATION";
  // `design_handoff_hud`'s build panel names, which M2.11's two-line buttons split at the space.
  case DesignId::ModuleShipyardL1:
    return L"SHIPYARD L1";
  case DesignId::ModuleShipyardL2:
    return L"SHIPYARD L2";
  case DesignId::ModuleOreProcessorL1:
    return L"ORE PROC L1";
  case DesignId::ModuleOreProcessorL2:
    return L"ORE PROC L2";
  }
  return L"";
}

/// A team's color **relative to this client**: `TEAM.OWN` is always the local player, whichever slot
/// that is, and the hostiles take the other three in slot order (`palette.json`'s hue rule).
[[nodiscard]] Neuron::QuadColor TeamColor(PlayerId _team, PlayerId _local) noexcept
{
  if (_team == _local)
  {
    return TEAM_OWN;
  }
  const std::array<Neuron::QuadColor, 3> hostiles{TEAM_B, TEAM_C, TEAM_D};
  const std::size_t rank = static_cast<std::size_t>(_team) - 1 - (((_local != NO_PLAYER) && (_local < _team)) ? 1 : 0);
  return hostiles[std::min(rank, hostiles.size() - 1)];
}

/// A continuous bar's filled width in pixels: `round(inner * percent / 100)`, which is the handoff's
/// formula for the hull bar and the one its drawings were made with.
[[nodiscard]] std::int32_t FilledPixels(std::int32_t _innerPixels, std::uint32_t _percent) noexcept
{
  const std::uint32_t clamped = std::min<std::uint32_t>(_percent, 100);
  return static_cast<std::int32_t>(((static_cast<std::uint32_t>(_innerPixels) * clamped) + 50) / 100);
}

void EmitFrameTicks(Emitter& _emit)
{
  const Neuron::QuadColor faint = WithAlpha(TICK, FRAME_TICK_ALPHA);
  _emit.Solid(FRAME_TICK_LEFT_UPPER, faint);
  _emit.Solid(FRAME_TICK_LEFT_MID, faint);
  _emit.Solid(FRAME_TICK_RIGHT_UPPER, faint);
  _emit.Solid(FRAME_TICK_RIGHT_MID, faint);
}

void EmitCredits(const HudState& _state, Emitter& _emit, HudHitTable& _hits)
{
  _emit.Solid(CREDITS_PANEL, SCRIM);
  _emit.Solid(CREDITS_INDEX, WithAlpha(TEAM_OWN, CREDITS_INDEX_ALPHA));
  _emit.Solid(CREDITS_RULE_BOTTOM, RULE);
  _emit.Solid(CREDITS_RULE_RIGHT, RULE);
  _emit.Solid(CREDITS_TICK_LEFT, TICK);
  _emit.Solid(CREDITS_TICK_RIGHT, TICK);
  _emit.Text(CREDITS_LABEL, L"CREDITS", Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_LABEL, TEXT_2);
  _emit.Text(CREDITS_VALUE, FormatCredits(_state.credits), Neuron::TextSize::Display, Neuron::TextAlign::Left, 0.0f, TEXT);

  // **NO INCOME RATE, AND THE FLASH IS WHY NONE IS MISSED** (`OpenQuestions.md` Q36, ruled 2026-09-23). Cyan
  // on a gain and amber on a spend, as `CreditFlash` fades it.
  if ((_state.creditFlash != CreditChange::None) && (_state.creditFlashAlpha > 0.0f))
  {
    _emit.Solid(CREDITS_FLASH, WithAlpha((_state.creditFlash == CreditChange::Gain) ? TEAM_OWN : SIG_ARMED, _state.creditFlashAlpha));
  }

  _hits.AddBlocker(CREDITS_PANEL);
}

[[nodiscard]] Neuron::QuadColor LinkDotColor(LinkState _link) noexcept
{
  switch (_link)
  {
  case LinkState::Linked:
    return TEAM_OWN;
  case LinkState::Refused:
    return SIG_SHORT;
  case LinkState::Joining:
  case LinkState::Reconnecting:
    break;
  }
  return TICK;
}

/// A plate with a one-pixel outline and a four-pixel index bar, which is what every button in the
/// interface is. The bar sits on the leading edge; _leftHanded reflects it to the trailing one.
void EmitButton(Emitter& _emit, const HudRect& _button, const Neuron::QuadColor& _plate, const Neuron::QuadColor& _border,
                const Neuron::QuadColor& _index, bool _leftHanded)
{
  _emit.Solid(_button, _plate);
  _emit.Outline(_button, 1, _border);
  _emit.Solid(ReflectWithin({0, 0, 4, _button.h}, _button.w, _leftHanded).Within(_button), _index);
}

void EmitSystem(const HudState& _state, Emitter& _emit, HudHitTable& _hits)
{
  if (!_state.quitArmed)
  {
    _emit.Solid(SYSTEM_PANEL, SCRIM);
    _emit.Solid(SYSTEM_INDEX, RULE_LIT);
    _emit.Solid(SYSTEM_RULE_BOTTOM, RULE);
    _emit.Solid(SYSTEM_TICK_LEFT, TICK);
    _emit.Solid(SYSTEM_TICK_RIGHT, TICK);
    _emit.Solid(SYSTEM_LINK_DOT, LinkDotColor(_state.link));
    _emit.Text(SYSTEM_LINK_LABEL, L"LINK", Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_LABEL, TEXT_2);

    // **THE SMALLEST TARGET ON SCREEN, DELIBERATELY.** It stays at the 48-pixel floor rather than a
    // comfort tier: it is the one control that should be slightly hard to hit.
    _emit.Solid(SYSTEM_QUIT, PLATE);
    _emit.Outline(SYSTEM_QUIT, 1, RULE_LIT);
    _emit.Text({SYSTEM_QUIT.x, SYSTEM_QUIT.y + 14, SYSTEM_QUIT.w, 20}, L"QUIT", Neuron::TextSize::Body, Neuron::TextAlign::Center, 0.0f,
               TEXT_2);

    _hits.AddBlocker(SYSTEM_PANEL);
    _hits.AddTarget({.hit = SYSTEM_QUIT, .tier = TouchTier::Floor, .action = HudAction::ArmQuit, .argument = 0, .surface = 's'});
    return;
  }

  // **ARMED: THE PANEL EXPANDS IN PLACE**, and the second tap is on a confirm rather than on the same
  // spot as the first, so a double contact on one place cannot end the match.
  _emit.Solid(CONFIRM_PANEL, SCRIM);
  _emit.Solid(CONFIRM_INDEX, SIG_SHORT);
  _emit.Solid(CONFIRM_RULE_BOTTOM, RULE);
  _emit.Solid(CONFIRM_LINK_DOT, LinkDotColor(_state.link));
  _emit.Text(CONFIRM_LINK_LABEL, L"LINK", Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_LABEL, TEXT_2);
  _emit.Text(CONFIRM_WARN, L"NO SAVE. NO REJOIN.", Neuron::TextSize::Body, Neuron::TextAlign::Center, TRACK_OVERLAY, SIG_SHORT);

  EmitButton(_emit, CONFIRM_STAY, PLATE, RULE_LIT, RULE_LIT, false);
  _emit.Text({CONFIRM_STAY.x, CONFIRM_STAY.y + 22, CONFIRM_STAY.w, 20}, L"STAY", Neuron::TextSize::Body, Neuron::TextAlign::Center, 0.0f,
             TEXT);

  EmitButton(_emit, CONFIRM_QUIT, PLATE_QUIT, SIG_SHORT, SIG_SHORT, false);
  _emit.Text({CONFIRM_QUIT.x, CONFIRM_QUIT.y + 22, CONFIRM_QUIT.w, 20}, L"QUIT", Neuron::TextSize::Body, Neuron::TextAlign::Center, 0.0f,
             SIG_SHORT);

  _hits.AddBlocker(CONFIRM_PANEL);
  _hits.AddTarget({.hit = CONFIRM_STAY, .tier = TouchTier::Combat, .action = HudAction::StayInMatch, .argument = 0, .surface = 's'});
  _hits.AddTarget({.hit = CONFIRM_QUIT, .tier = TouchTier::Combat, .action = HudAction::ConfirmQuit, .argument = 0, .surface = 's'});
}

void EmitSelection(const HudState& _state, Emitter& _emit, HudHitTable& _hits)
{
  if (_state.groups.empty())
  {
    // **ONLY DRAWN WHEN SOMETHING IS SELECTED.** An empty panel would be a permanent strip of battlefield
    // nobody can see, for a readout with nothing to say.
    return;
  }

  const bool left = _state.leftHanded;
  const std::size_t groups = std::min(_state.groups.size(), MAXIMUM_SELECTION_GROUPS);
  const HudRect primary = SelectionPanelRect(groups);

  const HudRect panel = ForHand(primary, left);
  _emit.Solid(panel, SCRIM);
  _emit.Solid(ForHand({0, SELECTION_PANEL_TOP, primary.w, 1}, left), RULE);
  _emit.Solid(ForHand({primary.w - 1, SELECTION_PANEL_TOP, 1, SELECTION_PANEL_HEIGHT}, left), RULE);
  _emit.Solid(ForHand({0, SELECTION_PANEL_TOP, 16, 2}, left), TICK);
  _emit.Solid(ForHand({primary.w - 16, SELECTION_PANEL_TOP, 16, 2}, left), TICK);
  _hits.AddBlocker(panel);

  for (std::size_t index = 0; index < groups; ++index)
  {
    const SelectionGroupSummary& group = _state.groups[index];
    const HudRect cell = ForHand(SelectionGroupRect(index), left);

    _emit.Outline(cell, 1, RULE);
    _emit.Solid(ReflectWithin(GROUP_INDEX, SELECTION_GROUP_WIDTH, left).Within(cell), WithAlpha(TEAM_OWN, OWN_INDEX_ALPHA));

    // **THE COUNT IS NOT ANIMATED.** It is the readout the player watches while their hand covers the
    // double tap's circle, and it must not lag the gesture by a single frame.
    _emit.Text(GROUP_COUNT.Within(cell), std::to_wstring(group.count), Neuron::TextSize::Display, Neuron::TextAlign::Left, 0.0f, TEXT);
    _emit.Text(GROUP_NAME.Within(cell), DesignDisplayName(group.design), Neuron::TextSize::Body, Neuron::TextAlign::Right, TRACK_NAME,
               TEXT_2);

    // The hull: continuous, a keyline over a trough, the remaining portion and the lost one, and three
    // index ticks drawn over the fill.
    const HudRect trough = GROUP_HULL_TROUGH.Within(cell);
    _emit.Solid(trough, TRACK);
    _emit.Outline(trough, 1, RULE);

    const HudRect inner = GROUP_HULL_INNER.Within(cell);
    const std::int32_t remaining = FilledPixels(inner.w, group.hullPercent);
    _emit.Solid({inner.x, inner.y, remaining, inner.h}, HULL);
    _emit.Solid({inner.x + remaining, inner.y, inner.w - remaining, inner.h}, HULL_LOST);
    _emit.Solid(GROUP_HULL_TICK_0.Within(cell), TICK_BAR);
    _emit.Solid(GROUP_HULL_TICK_1.Within(cell), TICK_BAR);
    _emit.Solid(GROUP_HULL_TICK_2.Within(cell), TICK_BAR);

    // **THE CARGO ROW, ONLY FOR A DESIGN THAT CARRIES ORE** (M2.7). Four discrete chips and no trough behind
    // them: filled is `ORE`, empty is `TRACK` under a one-pixel keyline -- continuity, keyline, hue and a
    // fixed row are what keep it apart from the hull bar above.
    if (group.carriesOre)
    {
      for (std::int32_t chip = 0; chip < static_cast<std::int32_t>(CARGO_CHIP_COUNT); ++chip)
      {
        const HudRect rect = HudRect{.x = GROUP_CARGO_CHIP.x + (chip * GROUP_CARGO_CHIP_STEP),
                                     .y = GROUP_CARGO_CHIP.y,
                                     .w = GROUP_CARGO_CHIP.w,
                                     .h = GROUP_CARGO_CHIP.h}
                               .Within(cell);
        if (chip < static_cast<std::int32_t>(group.cargoChips))
        {
          _emit.Solid(rect, ORE);
        }
        else
        {
          _emit.Solid(rect, TRACK);
          _emit.Outline(rect, 1, RULE);
        }
      }
    }

    _hits.AddTarget({.hit = cell,
                     .tier = TouchTier::Combat,
                     .action = HudAction::SelectGroup,
                     .argument = static_cast<std::uint8_t>(group.design),
                     .surface = 'l'});
  }

  const HudRect clear = ForHand(SelectionClearRect(groups), left);
  EmitButton(_emit, clear, PLATE, RULE_LIT, RULE_LIT, left);
  _emit.Text(CLEAR_LABEL.Within(clear), L"CLEAR", Neuron::TextSize::Body, Neuron::TextAlign::Center, TRACK_NAME, TEXT_2);
  _hits.AddTarget({.hit = clear, .tier = TouchTier::Combat, .action = HudAction::ClearSelection, .argument = 0, .surface = 'l'});
}

/// The module row's four places, and the design in each (`design_handoff_hud`: yard L1, yard L2, ore L1, ore L2).
struct ModuleButton
{
  HudRect rect;
  DesignId design;
};

constexpr std::array<ModuleButton, 4> MODULE_ROW{ModuleButton{.rect = BUILD_BUTTON_YARD_L1, .design = DesignId::ModuleShipyardL1},
                                                 ModuleButton{.rect = BUILD_BUTTON_YARD_L2, .design = DesignId::ModuleShipyardL2},
                                                 ModuleButton{.rect = BUILD_BUTTON_ORE_L1, .design = DesignId::ModuleOreProcessorL1},
                                                 ModuleButton{.rect = BUILD_BUTTON_ORE_L2, .design = DesignId::ModuleOreProcessorL2}};

/// **WHAT A MODULE BUTTON CHARGES**: a placed level its design's cost, and an upgrade the difference from the
/// level it upgrades (Q54) -- `GameCore`'s figure, so the panel and the host cannot disagree about it.
[[nodiscard]] std::uint32_t ModuleCostCredits(DesignId _design) noexcept
{
  if (IsPlacedLevel(_design))
  {
    return Derive(_design).cost;
  }
  for (const DesignEntry& from : Designs())
  {
    if (UpgradesTo(from.id, _design))
    {
      return UpgradeCostCredits(from.id, _design);
    }
  }
  return Derive(_design).cost;
}

/// **THE UNAVAILABLE HATCH** (`design_handoff_hud`): 45-degree lines 12 pixels apart, inset a pixel. The interface
/// draws rectangles and nothing else, so each line is dotted a pixel every three -- the geometric cue that keeps
/// the state readable in peripheral vision and without its hue.
void EmitHatch(Emitter& _emit, const HudRect& _button)
{
  constexpr std::int32_t SPACING = 12;
  constexpr std::int32_t DOT_STEP = 3;
  const HudRect inner{_button.x + 1, _button.y + 1, _button.w - 2, _button.h - 2};
  // Each line is x - y = offset, taken from the bottom-left corner to the top-right.
  for (std::int32_t offset = -inner.h + (SPACING / 2); offset < inner.w; offset += SPACING)
  {
    for (std::int32_t y = 0; y < inner.h; y += DOT_STEP)
    {
      const std::int32_t x = offset + y;
      if ((x >= 0) && (x < inner.w))
      {
        _emit.Solid({inner.x + x, inner.y + (inner.h - 1 - y), 1, 1}, HATCH);
      }
    }
  }
}

/// **THE PLACEMENT RADIUS** (M2.11), in `SIG.ARMED` at the handoff's 0.70 -- under the panels, so a panel over
/// the ring still reads as a panel, and never in the hit table.
void EmitPlacementRing(const HudState& _state, Emitter& _emit)
{
  for (const HudRect& square : _state.placementRing)
  {
    _emit.Solid(square, WithAlpha(SIG_ARMED, 0.70f));
  }
}

void EmitBuild(const HudState& _state, Emitter& _emit, HudHitTable& _hits)
{
  if (!_state.buildPanelOpen)
  {
    return;
  }

  const bool left = _state.leftHanded;
  const HudRect panel = ForHand(BUILD_PANEL, left);
  _emit.Solid(panel, SCRIM);
  _emit.Solid(ForHand(BUILD_RULE_TOP, left), RULE);
  _emit.Solid(ForHand(BUILD_RULE_INNER, left), RULE);
  _emit.Solid(ForHand(BUILD_TICK_TOP_INNER, left), TICK);
  _emit.Solid(ForHand(BUILD_TICK_TOP_OUTER, left), TICK);
  _emit.Solid(ForHand(BUILD_TICK_BOTTOM_INNER, left), TICK);
  _hits.AddBlocker(panel);

  // === THE SHIP ROW ================================================================================
  //
  // **LIVE OR UNAFFORDABLE, AND THE TWO LOOK DIFFERENT.** Unaffordable means *save up*: the plate stays
  // lit and only the cost, the index bar and a rule along the bottom redden. The button stays a target,
  // because the host is what refuses an unaffordable order (M1.6) and a client that also refused would be
  // a second copy of the rule.
  const std::array<HudRect, 2> places{BUILD_BUTTON_SHIP_0, BUILD_BUTTON_SHIP_1};
  const std::span<const DesignId> designs = BuildableDesigns();
  for (std::size_t index = 0; (index < designs.size()) && (index < places.size()); ++index)
  {
    const DesignId design = designs[index];
    const std::uint32_t cost = Derive(design).cost;
    const bool affordable = _state.credits >= cost;
    const HudRect button = ForHand(places[index], left);

    EmitButton(_emit, button, PLATE, RULE_LIT, affordable ? RULE_LIT : SIG_SHORT, left);
    _emit.Text(BUTTON_NAME_LINE1.Within(button), DesignDisplayName(design), Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_BUTTON,
               TEXT);
    _emit.Text(BUTTON_COST.Within(button), std::to_wstring(cost), Neuron::TextSize::Display, Neuron::TextAlign::Right, 0.0f,
               affordable ? TEXT : SIG_SHORT);
    if (!affordable)
    {
      _emit.Solid(BUTTON_SHORT_RULE.Within(button), SIG_SHORT);
    }

    _hits.AddTarget({.hit = button,
                     .tier = TouchTier::UnderFire,
                     .action = HudAction::Build,
                     .argument = static_cast<std::uint8_t>(design),
                     .surface = 'b'});
  }

  // === THE MODULE ROW (M2.11) ======================================================================
  //
  // **ARMED OR NOT, AND ARMED IS ITS OWN LOOK** (`design_handoff_hud`'s four states): the button that arms a
  // placement takes the armed plate, a 2-pixel `SIG.ARMED` outline and a 4-pixel rule along its top edge. The
  // handoff's pulse on that outline is not drawn -- the interface has no clock of its own, and the geometric cue
  // carries the state without it. **An L2 shows what it will charge**, which is the difference (Q54).
  //
  // **AND UNAVAILABLE, WHICH WINS** (M2.11b): an L2 with nothing to upgrade, or a placement past the cap, dims
  // and hatches the whole button and leaves the hit table -- the panel still swallows the tap, and nothing arms.
  for (const ModuleButton& place : MODULE_ROW)
  {
    const DesignId design = place.design;
    const std::uint32_t cost = ModuleCostCredits(design);
    const BuildButtonState look = ModuleButtonState(design, _state);
    const bool armed = look == BuildButtonState::Armed;
    const bool affordable = look != BuildButtonState::Unaffordable;
    const HudRect button = ForHand(place.rect, left);
    const std::wstring name = DesignDisplayName(design);
    const std::size_t space = name.rfind(L' ');

    if (look == BuildButtonState::Unavailable)
    {
      EmitButton(_emit, button, PLATE_DIM, RULE_DIM, RULE, left);
      EmitHatch(_emit, button);
      _emit.Text(BUTTON_NAME_LINE1.Within(button), name.substr(0, space), Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_BUTTON,
                 TEXT_DIM);
      _emit.Text(BUTTON_NAME_LINE2.Within(button), (space == std::wstring::npos) ? std::wstring{} : name.substr(space + 1),
                 Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_BUTTON, TEXT_DIM);
      _emit.Text(BUTTON_COST.Within(button), std::to_wstring(cost), Neuron::TextSize::Display, Neuron::TextAlign::Right, 0.0f, TEXT_DIM);
      continue;
    }

    if (armed)
    {
      EmitButton(_emit, button, PLATE_ARMED, SIG_ARMED, SIG_ARMED, left);
      _emit.Outline(button, 2, SIG_ARMED);
      _emit.Solid({button.x, button.y, button.w, 4}, SIG_ARMED);
    }
    else
    {
      EmitButton(_emit, button, PLATE, RULE_LIT, affordable ? RULE_LIT : SIG_SHORT, left);
    }
    const Neuron::QuadColor nameColor = armed ? TEXT_ARMED : TEXT;
    _emit.Text(BUTTON_NAME_LINE1.Within(button), name.substr(0, space), Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_BUTTON,
               nameColor);
    _emit.Text(BUTTON_NAME_LINE2.Within(button), (space == std::wstring::npos) ? std::wstring{} : name.substr(space + 1),
               Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_BUTTON, nameColor);
    _emit.Text(BUTTON_COST.Within(button), std::to_wstring(cost), Neuron::TextSize::Display, Neuron::TextAlign::Right, 0.0f,
               armed ? TEXT_ARMED : (affordable ? TEXT : SIG_SHORT));
    if (!armed && !affordable)
    {
      _emit.Solid(BUTTON_SHORT_RULE.Within(button), SIG_SHORT);
    }

    _hits.AddTarget({.hit = button,
                     .tier = TouchTier::UnderFire,
                     .action = HudAction::ArmModule,
                     .argument = static_cast<std::uint8_t>(design),
                     .surface = 'b'});
  }

  // === THE PROGRESS STRIP ==========================================================================
  //
  // **THE NAME AND THE PERCENT HANG FROM THE TRACK**, left and right edges, which is what keeps them in
  // place when the strip mirrors: the track reflects with the panel and the text follows it.
  const HudRect strip = ForHand(BUILD_PROGRESS, left);
  _emit.Solid(strip, WithAlpha(PLATE, PROGRESS_PLATE_ALPHA));
  _emit.Outline(strip, 1, RULE);
  _emit.Solid(ForHand(BUILD_PROGRESS_INDEX, left), WithAlpha(TEAM_OWN, OWN_INDEX_ALPHA));

  const HudRect track = ForHand(BUILD_PROGRESS_TRACK, left);
  const HudRect nameRow{track.x, BUILD_PROGRESS_NAME.y, track.w, BUILD_PROGRESS_NAME.h};
  _emit.Solid(track, TRACK);
  _emit.Outline(track, 1, RULE);

  DesignId building = DesignId::Miner;
  if (BuildingDesign(_state.buildingWire, building))
  {
    // **"+N" FOR WHAT WAITS BEHIND IT** (Q80), on the name, so the row's geometry does not move.
    const std::uint32_t queued = QueuedOf(_state.buildingWire);
    const std::wstring name = (queued == 0) ? std::wstring{DesignDisplayName(building)}
                                            : (std::wstring{DesignDisplayName(building)} + L"  +" + std::to_wstring(queued));
    _emit.Text(nameRow, name, Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_NAME, TEXT);
    _emit.Text(nameRow, std::to_wstring(_state.buildProgressPercent) + L"%", Neuron::TextSize::Body, Neuron::TextAlign::Right, 0.0f,
               TEXT_2);

    // **LEFT TO RIGHT IN BOTH LAYOUTS.** Progress reads in the direction text does; the track reflects
    // with the panel, the fill inside it does not.
    const HudRect inner{track.x + 1, track.y + 1, track.w - 2, track.h - 2};
    _emit.Solid({inner.x, inner.y, FilledPixels(inner.w, _state.buildProgressPercent), inner.h}, TEAM_OWN);
    for (std::int32_t quarter = 1; quarter <= 3; ++quarter)
    {
      _emit.Solid({inner.x + ((inner.w * quarter) / 4), inner.y, 1, inner.h}, TICK_BAR);
    }

    const HudRect cancel = ForHand(BUILD_CANCEL, left);
    EmitButton(_emit, cancel, PLATE, RULE_LIT, SIG_SHORT, left);
    _emit.Text(CANCEL_LABEL.Within(cancel), L"CANCEL", Neuron::TextSize::Body, Neuron::TextAlign::Center, TRACK_NAME, SIG_SHORT);
    _hits.AddTarget({.hit = cancel, .tier = TouchTier::Combat, .action = HudAction::CancelBuild, .argument = 0, .surface = 'b'});
  }
  else
  {
    // **NOTHING BUILDING: NO CANCEL.** Not drawn and not in the hit table, because a target that does
    // nothing is a target a player learns not to trust.
    _emit.Text(nameRow, L"SLOT EMPTY", Neuron::TextSize::Body, Neuron::TextAlign::Right, TRACK_NAME, TEXT_2);
  }
}

/// The reconnect overlay's block, which the refusal borrows: both are one headline and one line under it.
void EmitNoticeOverlay(Emitter& _emit, std::wstring _headline, std::wstring _detail)
{
  const HudRect block = OVERLAY_RECONNECT_BLOCK;
  _emit.Solid(OVERLAY_SCRIM, Neuron::HexColor(SPACE_HEX, RECONNECT_SCRIM_ALPHA));
  _emit.Solid(block, SCRIM);
  _emit.Solid({block.x, block.y, block.w, 1}, RULE);
  _emit.Solid({block.x, block.y + block.h - 1, block.w, 1}, RULE);
  _emit.Solid({block.x, block.y, 16, 2}, TICK);
  _emit.Solid({block.x + block.w - 16, block.y, 16, 2}, TICK);
  _emit.Text({block.x, block.y + 20, block.w, 32}, std::move(_headline), Neuron::TextSize::Display, Neuron::TextAlign::Center,
             TRACK_OVERLAY, TEXT);
  _emit.Text({block.x, block.y + 60, block.w, 20}, std::move(_detail), Neuron::TextSize::Body, Neuron::TextAlign::Center, TRACK_OVERLAY,
             TEXT_2);
}

void EmitResultOverlay(const HudState& _state, Emitter& _emit)
{
  const HudRect block = OVERLAY_RESULT_BLOCK;
  _emit.Solid(OVERLAY_SCRIM, Neuron::HexColor(SPACE_HEX, RESULT_SCRIM_ALPHA));
  _emit.Solid(block, WithAlpha(SCRIM, RESULT_BLOCK_ALPHA));
  _emit.Solid({block.x, block.y, block.w, 1}, RULE);
  _emit.Solid({block.x, block.y + block.h - 1, block.w, 1}, RULE);
  _emit.Solid({block.x, block.y, 16, 2}, TICK);
  _emit.Solid({block.x + block.w - 16, block.y, 16, 2}, TICK);
  _emit.Solid({block.x, block.y + block.h - 2, 16, 2}, TICK);
  _emit.Solid({block.x + block.w - 16, block.y + block.h - 2, 16, 2}, TICK);

  _emit.Text({block.x + 32, block.y + 40, 496, 20}, _state.endedOnClock ? L"ENGAGEMENT ENDED ON THE CLOCK" : L"ENGAGEMENT ENDED",
             Neuron::TextSize::Body, Neuron::TextAlign::Left, TRACK_RESULT, TEXT_2);

  // **IT NAMES THE WINNER** (`Interface.md` §6): from M4 there are three and four players, and "you lost"
  // does not say to whom. A draw names nobody (Q65).
  if (_state.winner == NO_PLAYER)
  {
    _emit.Text({block.x + 32, block.y + 88, 496, 32}, L"NO ONE HOLDS THE FIELD", Neuron::TextSize::Display, Neuron::TextAlign::Left, 0.0f,
               TEXT);
  }
  else
  {
    _emit.Solid({block.x + 32, block.y + 88, 32, 32}, TeamColor(_state.winner, _state.player));
    _emit.Text({block.x + 80, block.y + 88, 448, 32}, L"TEAM " + std::to_wstring(_state.winner) + L" HOLDS THE FIELD",
               Neuron::TextSize::Display, Neuron::TextAlign::Left, 0.0f, TEXT);
  }

  // **TWO LINES, BECAUSE THE RENDERER CANNOT WRAP** -- never one long string.
  _emit.Text({block.x + 32, block.y + 152, 496, 20}, L"NEXT MATCH IS SEEDING", Neuron::TextSize::Body, Neuron::TextAlign::Left,
             TRACK_OVERLAY, TEXT_2);
  _emit.Text({block.x + 32, block.y + 180, 496, 20}, L"YOU WILL BE DROPPED IN", Neuron::TextSize::Body, Neuron::TextAlign::Left,
             TRACK_OVERLAY, TEXT_2);
}
} // namespace

bool ModuleAvailable(DesignId _design, std::span<const DesignId> _ownModules) noexcept
{
  if (IsPlacedLevel(_design))
  {
    return _ownModules.size() < MAXIMUM_MODULES_PER_STATION;
  }
  for (const DesignId owned : _ownModules)
  {
    if (UpgradesTo(owned, _design))
    {
      return true;
    }
  }
  return false;
}

BuildButtonState ModuleButtonState(DesignId _design, const HudState& _state) noexcept
{
  if (!ModuleAvailable(_design, _state.ownModules))
  {
    return BuildButtonState::Unavailable;
  }
  if (_state.moduleArmed && (_state.armedModule == _design))
  {
    return BuildButtonState::Armed;
  }
  return (_state.credits >= ModuleCostCredits(_design)) ? BuildButtonState::Live : BuildButtonState::Unaffordable;
}

bool BuildingDesign(std::uint8_t _wire, DesignId& _outDesign) noexcept
{
  // The low four bits are the design plus one; the high four the queue behind it (Q80, `GameCore/Update.h`).
  const std::uint8_t designPlusOne = BuildingDesignOf(_wire);
  if (designPlusOne == 0)
  {
    return false;
  }
  _outDesign = static_cast<DesignId>(designPlusOne - 1);
  return true;
}

std::span<const DesignId> BuildableDesigns() noexcept
{
  // Built once from the design table, in its order. A design is a button because the table says it is
  // buildable, and for no other reason.
  static const std::vector<DesignId> buildable = []
  {
    std::vector<DesignId> designs;
    for (const DesignEntry& entry : Designs())
    {
      if (entry.buildable)
      {
        designs.push_back(entry.id);
      }
    }
    return designs;
  }();
  return buildable;
}

HudFrame BuildHud(const HudState& _state)
{
  HudFrame frame;
  Emitter emit{frame.items};

  // **THE ORDER IS THE LAYERING.** The frame's ticks, then the four panels, then the overlays over all
  // of it: an overlay's scrim dims the panels without hiding them, and suppresses nothing (the handoff's
  // *Overlays*) -- the hit table is untouched by it, so QUIT still answers under the result.
  EmitFrameTicks(emit);
  EmitPlacementRing(_state, emit);
  EmitCredits(_state, emit, frame.hits);
  EmitSystem(_state, emit, frame.hits);
  EmitSelection(_state, emit, frame.hits);
  EmitBuild(_state, emit, frame.hits);

  if (_state.link == LinkState::Reconnecting)
  {
    EmitNoticeOverlay(emit, L"RECONNECTING", L"THE MATCH DID NOT WAIT");
  }
  else if (_state.link == LinkState::Refused)
  {
    // **M1.4's REFUSAL, SHOWN.** ADR-013 answers a client that finds no free slot with a reason, and this
    // is where the player reads it.
    if (_state.fieldMismatch)
    {
      // Q76: the host's map and this client's disagree, and playing on would be playing on a map nobody can see.
      EmitNoticeOverlay(emit, L"MAP MISMATCH", L"HOST AND CLIENT DISAGREE");
    }
    else
    {
      EmitNoticeOverlay(emit, L"MATCH FULL", L"NO SLOT IS FREE");
    }
  }

  if (_state.matchEnded)
  {
    EmitResultOverlay(_state, emit);
  }

  return frame;
}

std::vector<SelectionGroupSummary> SummarizeSelection(std::span<const WireIdentity> _selection, std::span<const EntityRecord> _entities)
{
  struct Tally
  {
    std::uint32_t count = 0;
    std::uint32_t hullSum = 0;
    std::uint32_t chipSum = 0;
  };
  std::array<Tally, 256> tallies{};

  for (const WireIdentity identity : _selection)
  {
    for (const EntityRecord& record : _entities)
    {
      if (record.identity == identity)
      {
        Tally& tally = tallies[record.designIdentity];
        ++tally.count;
        tally.hullSum += record.hullPercentRemaining;
        tally.chipSum += CargoChipsOf(record.flags);
        break;
      }
    }
  }

  std::vector<SelectionGroupSummary> groups;
  for (std::size_t design = 0; (design < tallies.size()) && (groups.size() < MAXIMUM_SELECTION_GROUPS); ++design)
  {
    const Tally& tally = tallies[design];
    if (tally.count == 0)
    {
      continue;
    }
    // A design the table does not have carries nothing; one it has carries ore when its derived capacity
    // says so -- the catalog read the build panel already makes, and no rule the host evaluates.
    const bool known = design < Designs().size();
    groups.push_back(SelectionGroupSummary{.design = static_cast<DesignId>(design),
                                           .count = tally.count,
                                           .hullPercent = static_cast<std::uint8_t>((tally.hullSum + (tally.count / 2)) / tally.count),
                                           .carriesOre = known && (Derive(static_cast<DesignId>(design)).oreCapacity > 0),
                                           .cargoChips = static_cast<std::uint8_t>((tally.chipSum + (tally.count / 2)) / tally.count)});
  }
  return groups;
}

std::size_t NarrowToDesign(Selection& _selection, std::span<const EntityRecord> _entities, DesignId _design)
{
  std::vector<WireIdentity> kept;
  for (const WireIdentity identity : _selection.Identities())
  {
    for (const EntityRecord& record : _entities)
    {
      if ((record.identity == identity) && (static_cast<DesignId>(record.designIdentity) == _design))
      {
        kept.push_back(identity);
        break;
      }
    }
  }

  _selection.Clear();
  for (const WireIdentity identity : kept)
  {
    _selection.Add(identity);
  }
  return _selection.Count();
}

std::wstring FormatCredits(std::uint32_t _credits)
{
  const std::wstring digits = std::to_wstring(_credits);
  std::wstring grouped;
  grouped.reserve(digits.size() + (digits.size() / 3));
  for (std::size_t index = 0; index < digits.size(); ++index)
  {
    if ((index > 0) && (((digits.size() - index) % 3) == 0))
    {
      grouped.push_back(L',');
    }
    grouped.push_back(digits[index]);
  }
  return grouped;
}

Command BuildStationCommand(std::uint16_t _sequence, CommandType _type, DesignId _design) noexcept
{
  Command command;
  command.sequence = _sequence;
  command.type = _type;
  // `Command::TargetDesign` reads the design out of the low byte of `targetX`; a cancel carries none.
  command.targetX = (_type == CommandType::Build) ? static_cast<std::int16_t>(_design) : 0;
  command.targetY = 0;
  return command;
}

HudTypeface TypefaceOf(const Neuron::GlyphAtlas& _atlas) noexcept
{
  HudTypeface typeface;
  typeface.tables = {&_atlas.Table(Neuron::TextSize::Body), &_atlas.Table(Neuron::TextSize::Display)};
  typeface.faces = {_atlas.Face(Neuron::TextSize::Body), _atlas.Face(Neuron::TextSize::Display)};
  typeface.solid = _atlas.SolidSlot();
  typeface.atlasWidthPixels = _atlas.WidthPixels();
  typeface.atlasHeightPixels = _atlas.HeightPixels();
  return typeface;
}

void EmitQuads(const HudFrame& _frame, const HudTypeface& _typeface, const Neuron::FitTransform& _interfaceFit,
               std::vector<Neuron::GlyphQuad>& _out)
{
  for (const HudItem& item : _frame.items)
  {
    const Neuron::PhysicalRect placed = Neuron::MapAuthoredRect(_interfaceFit, item.rect.Authored());
    if (item.kind == HudItem::Kind::Solid)
    {
      static_cast<void>(
        Neuron::AppendSolidQuad(_typeface.solid, _typeface.atlasWidthPixels, _typeface.atlasHeightPixels, placed, item.color, _out));
      continue;
    }

    const std::size_t size = static_cast<std::size_t>(item.size);
    if ((size >= Neuron::TEXT_SIZE_COUNT) || (_typeface.tables[size] == nullptr))
    {
      continue;
    }

    static_cast<void>(Neuron::LayoutText(
      *_typeface.tables[size], _typeface.faces[size], _typeface.atlasWidthPixels, _typeface.atlasHeightPixels,
      Neuron::TextRun{.box = placed, .text = item.text, .align = item.align, .trackingEm = item.trackingEm, .color = item.color}, _out));
  }
}

} // namespace Outpost
