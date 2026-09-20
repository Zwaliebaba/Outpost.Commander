#include "pch.h"

#include "Operator.h"

#include "Victory.h"

#include "FixedPoint.h"

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

namespace Outpost
{

namespace
{

/// The two buttons of §10's overlays, in authored pixels. Laid out here rather than through
/// UiPanel, because a panel carries hover and press state a menu of two fixed buttons does not
/// need and K4 is the task that grows one.
[[nodiscard]] Neuron::UiRect ResumeButton() noexcept
{
  return {PAUSE_MENU_RECT.x + Neuron::PANEL_INSET_PIXELS, PAUSE_MENU_RECT.y + Neuron::PANEL_CLIENT_TOP_PIXELS + 24,
          PAUSE_MENU_RECT.width - (2 * Neuron::PANEL_INSET_PIXELS), Neuron::BUTTON_HEIGHT_PIXELS};
}

[[nodiscard]] Neuron::UiRect PauseQuitButton() noexcept
{
  const Neuron::UiRect resume = ResumeButton();
  return {resume.x, resume.y + Neuron::BUTTON_HEIGHT_PIXELS + Neuron::PANEL_INSET_PIXELS, resume.width, resume.height};
}

[[nodiscard]] Neuron::UiRect MatchEndQuitButton() noexcept
{
  return {MATCH_END_RECT.x + Neuron::PANEL_INSET_PIXELS,
          MATCH_END_RECT.Bottom() - Neuron::PANEL_INSET_PIXELS - Neuron::BUTTON_HEIGHT_PIXELS,
          MATCH_END_RECT.width - (2 * Neuron::PANEL_INSET_PIXELS), Neuron::BUTTON_HEIGHT_PIXELS};
}

[[nodiscard]] PickBox BoxOf(const Neuron::UiRect& _rect) noexcept
{
  return {_rect.x, _rect.y, _rect.Right(), _rect.Bottom()};
}

/// A key pressed THIS FRAME and not held: the edge, so that a key held down fires once.
///
/// NO BOUND TEST. keyEdges has one entry per virtual-key code (Client/FrameInput.h's KEY_COUNT is
/// 256) and the key is a std::uint8_t, so a comparison against the size is always true - which is
/// a finding rather than a safety net, and clang-tidy says so.
[[nodiscard]] bool Pressed(const Neuron::FrameInput& _input, std::uint8_t _key) noexcept
{
  return _input.keyEdges[_key] > 0;
}

[[nodiscard]] bool ButtonPressed(const Neuron::FrameInput& _input, Neuron::MouseButton _button) noexcept
{
  return _input.buttonEdges[Neuron::IndexOf(_button)] > 0;
}

[[nodiscard]] bool ButtonReleased(const Neuron::FrameInput& _input, Neuron::MouseButton _button) noexcept
{
  return _input.buttonEdges[Neuron::IndexOf(_button)] < 0;
}

/// What a refused order is called, for the one line of §6. The strings are here rather than in the
/// content tables because §7's own note says the binding layer and the text tables are M1's later
/// work; what matters now is that the line says which order and why in words.
[[nodiscard]] std::string_view NameOf(OrderKind _kind) noexcept
{
  switch (_kind)
  {
  case OrderKind::Move:
    return "Move";
  case OrderKind::AttackMove:
    return "Attack-move";
  case OrderKind::Attack:
    return "Attack";
  case OrderKind::Patrol:
    return "Patrol";
  case OrderKind::Guard:
    return "Guard";
  case OrderKind::Stop:
    return "Stop";
  case OrderKind::ReturnToRepair:
    return "Return to repair";
  case OrderKind::SetStance:
    return "Stance";
  case OrderKind::PlaceStructure:
    return "Build";
  case OrderKind::CancelStructure:
    return "Cancel construction";
  case OrderKind::Demolish:
    return "Demolish";
  case OrderKind::BuildModule:
    return "Build module";
  case OrderKind::SetProduction:
    return "Produce";
  case OrderKind::CancelProduction:
    return "Cancel production";
  case OrderKind::SetResearch:
    return "Research";
  case OrderKind::CancelResearch:
    return "Cancel research";
  case OrderKind::SaveDesign:
    return "Design";
  case OrderKind::Group:
    return "Group";
  case OrderKind::Surrender:
    return "Surrender";
  case OrderKind::Chat:
    return "Chat";
  }
  return "Order"; // Every enumerator is named above; this is what a wire byte outside the set gets.
}

[[nodiscard]] std::string_view ReasonOf(RejectReason _reason) noexcept
{
  switch (_reason)
  {
  case RejectReason::NotOwned:
    return "that is not yours";
  case RejectReason::NotVisible:
    return "you cannot see that";
  case RejectReason::CannotAfford:
    return "not enough power";
  case RejectReason::AtCap:
    return "at the cap";
  case RejectReason::InvalidTarget:
    return "not a target for that";
  case RejectReason::InvalidPlacement:
    return "it cannot stand there";
  case RejectReason::NotResearched:
    return "not researched";
  case RejectReason::NoCommandPost:
    return "no command post";
  case RejectReason::Malformed:
    return "malformed";
  case RejectReason::Accepted:
    break; // Never reached: a seat reports no refusal by sequence 0.
  }
  return "refused";
}

[[nodiscard]] std::string_view ArmedName(ArmedOrder _armed) noexcept
{
  switch (_armed)
  {
  case ArmedOrder::Move:
    return "Move";
  case ArmedOrder::Patrol:
    return "Patrol";
  case ArmedOrder::AttackMove:
    return "Attack-move";
  case ArmedOrder::PlaceStructure:
    return "Build";
  case ArmedOrder::None:
    break;
  }
  return "";
}

/// Text centred on _centerX at _y, through BitmapFont's fixed advance. A proportional font would
/// need a measure; this one is a grid, so the width is the count.
void AppendCentered(std::string_view _text, std::int32_t _centerX, std::int32_t _y, std::uint32_t _color,
                    std::vector<Neuron::UiQuad>& _outQuads)
{
  const std::int32_t width = static_cast<std::int32_t>(_text.size()) * Neuron::GLYPH_ADVANCE_PIXELS;
  Neuron::AppendText(_text, _centerX - (width / 2), _y, _color, _outQuads);
}

} // namespace

void Operator::BlockedBy(const Frame& _frame, const Match& _match, std::vector<PickBox>& _outBoxes) const
{
  _outBoxes.clear();
  _outBoxes.insert(_outBoxes.end(), _frame.panels.begin(), _frame.panels.end());
  if (m_menuOpen)
  {
    _outBoxes.push_back(BoxOf(PAUSE_MENU_RECT));
  }
  if (_match.Commander().Own().victory != static_cast<std::uint8_t>(VictoryState::Playing))
  {
    _outBoxes.push_back(BoxOf(MATCH_END_RECT));
  }
}

bool Operator::TakeMenuKeys(const Frame& _frame, Match& _match)
{
  const Neuron::FrameInput& input = *_frame.input;
  // F10 FIRST AND FROM EITHER MODE (§10). It opens the menu in POINT mode whatever the game was in,
  // because a menu with no pointer is a menu nobody can click, and Resume puts the mode back.
  if (Pressed(input, KEY_PAUSE_MENU))
  {
    m_menuOpen = !m_menuOpen;
    if (m_menuOpen)
    {
      m_modeBeforeMenu = m_mode;
      m_mode = Neuron::PointerMode::Point;
    }
    else
    {
      m_mode = m_modeBeforeMenu;
    }
    _match.Pause(m_menuOpen);
    return true;
  }
  // ESCAPE IS PEELED AND NOT SWALLOWED (§7). The decision is taken from the state BEFORE this
  // frame's input, because OrderInput disarms on the same press: asking afterwards would find
  // nothing armed and swap the pointer's mode on the press that was meant to disarm.
  if (Pressed(input, KEY_ESCAPE))
  {
    switch (Neuron::PeelOf(m_orders.Armed() != ArmedOrder::None, m_menuOpen))
    {
    case Neuron::EscapePeel::ArmedOrder:
      return false; // OrderInput's own Advance disarms it, below.
    case Neuron::EscapePeel::ModalPanel:
      m_menuOpen = false;
      m_mode = m_modeBeforeMenu;
      _match.Pause(false);
      return true;
    case Neuron::EscapePeel::SwapPointer:
      m_mode = Neuron::Swapped(m_mode);
      return true;
    }
  }
  if (!m_menuOpen)
  {
    return false;
  }
  // The menu's own two buttons. A click reaches them only in point mode, which opening the menu
  // guaranteed.
  if (_frame.pointerInside && ButtonPressed(input, Neuron::MouseButton::Left))
  {
    if (ResumeButton().Contains(_frame.pointerX, _frame.pointerY))
    {
      m_menuOpen = false;
      m_mode = m_modeBeforeMenu;
      _match.Pause(false);
    }
    else if (PauseQuitButton().Contains(_frame.pointerX, _frame.pointerY))
    {
      m_quit = true;
    }
  }
  return true; // A menu is modal: the world sees none of this frame.
}

void Operator::Advance(const Frame& _frame, const ContentTree& _content, Match& _match)
{
  const Neuron::FrameInput& input = *_frame.input;
  m_warning.Take(_match.Commander().Own(), _frame.nowMilliseconds);
  // A selected device that died stays selected otherwise, and every order given to it afterwards
  // comes back refused for an object that is not there - which reads as the game ignoring him.
  m_selection.Retain(_match.Picks().candidates);

  const bool over = _match.Commander().Own().victory != static_cast<std::uint8_t>(VictoryState::Playing);
  if (TakeMenuKeys(_frame, _match))
  {
    _match.Select(m_selection.Ids());
    return;
  }
  if (over)
  {
    // The world is decided and its overlay has one button. The camera still moves - §10 wants a
    // player able to look at the field - so this returns rather than blocking the whole frame.
    if (_frame.pointerInside && m_mode == Neuron::PointerMode::Point && ButtonPressed(input, Neuron::MouseButton::Left) &&
        MatchEndQuitButton().Contains(_frame.pointerX, _frame.pointerY))
    {
      m_quit = true;
    }
    _match.Select(m_selection.Ids());
    return;
  }

  BlockedBy(_frame, _match, m_blocked);
  // WHERE THE RAY GOES IS THE MODE'S (§4): the screen's CENTRE in aim, the pointer in point. It is
  // one branch here and nothing below knows which mode it is in, which is what §5 and §6 mean by
  // being written about the ray rather than about the mouse.
  const bool aiming = m_mode == Neuron::PointerMode::Aim;
  const std::int32_t rayX = aiming ? static_cast<std::int32_t>(Neuron::AUTHORED_WIDTH_PIXELS / 2) : _frame.pointerX;
  const std::int32_t rayY = aiming ? static_cast<std::int32_t>(Neuron::AUTHORED_HEIGHT_PIXELS / 2) : _frame.pointerY;
  // A pointer in the letterbox bars is over no world (§4); an aimed ray is always in the picture.
  const bool onScreen = aiming || _frame.pointerInside;

  // ONE RAY A FRAME, AGAINST THE HEIGHTFIELD (m1-vertical-slice/K7). Replica's own GroundPoint
  // intersects the plane at y = 0 because Replica may not include Client, and over ground 200
  // world units up at the camera's default pitch that is 400 units - six cells - from where the
  // commander is pointing. The executable holds both libraries, so it casts the ray that is right
  // and hands the answer to the orders AND to the ring, which is how the two cannot disagree.
  m_ground = Neuron::GroundHit{};
  if (_frame.terrain != nullptr && onScreen)
  {
    const PickRay ray = RayThrough(_frame.camera, rayX, rayY);
    m_ground =
      Neuron::RayAgainstGround(*_frame.terrain, {ray.originX, ray.originY, ray.originZ}, {ray.directionX, ray.directionY, ray.directionZ});
  }

  SelectionFrame selection{};
  selection.camera = _frame.camera;
  selection.candidates = _match.Picks().candidates;
  selection.blocked = m_blocked;
  selection.ownSeat = _match.Commander().Seat();
  selection.x = rayX;
  selection.y = rayY;
  selection.pressed = onScreen && ButtonPressed(input, Neuron::MouseButton::Left);
  selection.released = ButtonReleased(input, Neuron::MouseButton::Left);
  selection.additive = input.keysHeld[0x10]; // Shift
  // A LEFT CLICK SELECTS UNLESS AN ORDER IS ARMED (§6), which is the one place the two objects have
  // to agree: an armed Build whose click also selected whatever was under it would hand the
  // commander a new selection every time he laid a structure.
  if (m_orders.Armed() == ArmedOrder::None)
  {
    m_selection.Advance(selection);
  }
  _match.Select(m_selection.Ids());

  AbilitiesOf(_match.Commander().Devices(), _match.Commander().Structures(), _match.Commander().Designs(), _content, m_selection.Ids(),
              m_abilities);
  OrderFrame orders{};
  orders.camera = _frame.camera;
  orders.candidates = _match.Picks().candidates;
  orders.blocked = m_blocked;
  orders.selected = m_abilities;
  orders.ownSeat = _match.Commander().Seat();
  orders.x = rayX;
  orders.y = rayY;
  orders.leftPressed = onScreen && ButtonPressed(input, Neuron::MouseButton::Left);
  orders.rightPressed = onScreen && ButtonPressed(input, Neuron::MouseButton::Right);
  orders.keyEdges = input.keyEdges;
  orders.structureRowCount = static_cast<std::uint32_t>(_content.structures.structures.size());
  orders.groundKnown = m_ground.hit;
  orders.groundX = static_cast<std::int32_t>(m_ground.x * static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT));
  orders.groundZ = static_cast<std::int32_t>(m_ground.z * static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT));
  m_made.clear();
  m_orders.Advance(orders, m_made);
  for (const Order& order : m_made)
  {
    if (!_match.Submit(order))
    {
      // The unacknowledged window is full, which is the host not keeping up rather than a refusal.
      // Dropping the rest is right: they were given in one gesture and half of them is worse.
      break;
    }
  }
}

void Operator::MoveTo(float _worldX, float _worldZ, Match& _match)
{
  // NO PICKING AND NO RAY: the point is already a place on the landscape, which is what makes the
  // minimap's right click the one order in §6's table that needs neither. A structure in the
  // selection takes no primary order and is skipped, exactly as OrderInput's own default does.
  for (const std::uint32_t id : m_selection.Ids())
  {
    const auto device = _match.Commander().Devices().find(id);
    if (device == _match.Commander().Devices().end() || device->second.state.seat != _match.Commander().Seat())
    {
      continue;
    }
    Order order{};
    order.seat = _match.Commander().Seat();
    order.kind = OrderKind::Move;
    order.operands[0] = static_cast<std::int32_t>(id);
    order.operands[1] = static_cast<std::int32_t>(std::lround(_worldX * static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT)));
    order.operands[2] = static_cast<std::int32_t>(std::lround(_worldZ * static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT)));
    if (!_match.Submit(order))
    {
      break;
    }
  }
}

void Operator::BuildOverlay(const ChromePalette& _palette, std::span<const StructureDesc> _structures, const Match& _match,
                            std::int64_t _nowMilliseconds, std::vector<Neuron::UiQuad>& _outQuads) const
{
  // THE DRAG RECTANGLE IS POINT MODE'S ALONE (§4). It never forms in aim mode anyway - the ray is
  // the screen's centre and never travels - and the branch says so rather than leaving a reader to
  // work that out from the threshold.
  if (m_mode == Neuron::PointerMode::Point && m_selection.Dragging())
  {
    const PickBox band = m_selection.Band();
    const Neuron::UiRect rect{band.left, band.top, band.right - band.left, band.bottom - band.top};
    Neuron::AppendBorder(rect, Neuron::PANEL_BORDER_PIXELS, Neuron::PackedColor(_palette.accent), _outQuads);
  }
  // WHAT IS ARMED, IN WORDS. §7 gives Build one key that cycles the structure rows, so the
  // commander has to be told which one he is about to lay; the cursor of §4 shows a Build and not
  // WHICH build, and K4's panel is what replaces this line with a row of buttons.
  if (m_orders.Armed() != ArmedOrder::None)
  {
    std::string armed(ArmedName(m_orders.Armed()));
    if (m_orders.Armed() == ArmedOrder::PlaceStructure && m_orders.ArmedStructure() < _structures.size())
    {
      armed += ": ";
      armed += _structures[m_orders.ArmedStructure()].name;
    }
    if (m_orders.AwaitingSecondPoint())
    {
      armed += " (the far point)";
    }
    AppendCentered(armed, static_cast<std::int32_t>(Neuron::AUTHORED_WIDTH_PIXELS / 2), ARMED_LINE_Y_PIXELS,
                   Neuron::PackedColor(_palette.accent), _outQuads);
  }
  if (m_warning.Showing(_nowMilliseconds))
  {
    std::string line(NameOf(m_warning.Kind()));
    line += ": ";
    line += ReasonOf(m_warning.Reason());
    AppendCentered(line, static_cast<std::int32_t>(Neuron::AUTHORED_WIDTH_PIXELS / 2), WARNING_LINE_Y_PIXELS,
                   Neuron::PackedColor(_palette.warning), _outQuads);
  }

  const std::uint8_t victory = _match.Commander().Own().victory;
  if (victory != static_cast<std::uint8_t>(VictoryState::Playing))
  {
    const bool won = victory == static_cast<std::uint8_t>(VictoryState::Won);
    Neuron::AppendRect(MATCH_END_RECT, Neuron::PackedColor(_palette.panelFill), _outQuads);
    Neuron::AppendBorder(MATCH_END_RECT, Neuron::PANEL_BORDER_PIXELS, Neuron::PackedColor(_palette.panelBorder), _outQuads);
    const std::int32_t middle = MATCH_END_RECT.x + (MATCH_END_RECT.width / 2);
    AppendCentered(won ? "VICTORY" : "DEFEAT", middle, MATCH_END_RECT.y + Neuron::PANEL_CLIENT_TOP_PIXELS + 40,
                   Neuron::PackedColor(won ? _palette.accent : _palette.warning), _outQuads);
    // The match's length in whole seconds, from the tick the host stopped on: the one figure §10
    // asks the panel to carry beside the word.
    const std::uint32_t seconds = _match.HostSide().Tick() / static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND);
    AppendCentered(std::to_string(seconds / 60) + " min " + std::to_string(seconds % 60) + " s", middle,
                   MATCH_END_RECT.y + Neuron::PANEL_CLIENT_TOP_PIXELS + 100, Neuron::PackedColor(_palette.bodyText), _outQuads);
    const Neuron::UiRect quit = MatchEndQuitButton();
    Neuron::AppendRect(quit, Neuron::PackedColor(_palette.buttonFill), _outQuads);
    Neuron::AppendText("Quit", quit.x + Neuron::BUTTON_TEXT_INSET_PIXELS, quit.y + 4, Neuron::PackedColor(_palette.bodyText), _outQuads);
  }

  if (m_menuOpen)
  {
    Neuron::AppendRect(PAUSE_MENU_RECT, Neuron::PackedColor(_palette.panelFill), _outQuads);
    Neuron::AppendBorder(PAUSE_MENU_RECT, Neuron::PANEL_BORDER_PIXELS, Neuron::PackedColor(_palette.panelBorder), _outQuads);
    Neuron::AppendGradient(Neuron::TitleStripOf(PAUSE_MENU_RECT), Neuron::PackedColor(_palette.panelTitleFrom),
                           Neuron::PackedColor(_palette.panelTitleTo), _outQuads);
    Neuron::AppendText("Paused", PAUSE_MENU_RECT.x + Neuron::PANEL_TITLE_TEXT_X_PIXELS, PAUSE_MENU_RECT.y + 2,
                       Neuron::PackedColor(_palette.titleText), _outQuads);
    const std::array<std::pair<Neuron::UiRect, const char*>, 2> buttons{
      std::pair{ResumeButton(), "Resume"},
      std::pair{PauseQuitButton(), "Quit"},
    };
    for (const auto& button : buttons)
    {
      Neuron::AppendRect(button.first, Neuron::PackedColor(_palette.buttonFill), _outQuads);
      Neuron::AppendText(button.second, button.first.x + Neuron::BUTTON_TEXT_INSET_PIXELS, button.first.y + 4,
                         Neuron::PackedColor(_palette.bodyText), _outQuads);
    }
  }
}

} // namespace Outpost
