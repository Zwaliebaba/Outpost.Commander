#include "pch.h"
#include "PlayerControls.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::uint8_t KEY_ATTACK_MOVE = 'A';
constexpr std::uint8_t KEY_STOP = 'S';

// Whether _later follows _earlier closely enough to be a double click or a double tap. The millisecond clock wraps,
// and unsigned subtraction wraps with it.
bool IsDouble(std::uint32_t _earlier, std::uint32_t _later) noexcept
{
  return _later - _earlier <= Outpost::PlayerControls::DOUBLE_CLICK_MILLISECONDS;
}

std::optional<size_t> GroupKey(std::uint8_t _key) noexcept
{
  if (_key >= '0' && _key <= '9')
    return static_cast<size_t>(_key - '0');
  return std::nullopt;
}

const Outpost::EntityView* Find(std::span<const Outpost::EntityView> _entities, Outpost::EntityId _id) noexcept
{
  const auto found = std::ranges::find(_entities, _id, &Outpost::EntityView::id);
  return found == _entities.end() ? nullptr : &*found;
}
} // namespace

void Outpost::PlayerControls::Update(const Neuron::InputState& _input, std::span<const EntityView> _entities, PlayerId _player,
                                     Camera& _camera, const Viewport& _viewport)
{
  const Frame frame{.entities = _entities, .player = _player, .camera = _camera, .viewport = _viewport};
  Prune(frame);

  if (!_input.active)
  {
    // A drag cannot finish without the release, which the game will not see.
    m_pressPixels.reset();
    m_dragging = false;
    return;
  }

  for (const Neuron::InputEvent& event : _input.events)
  {
    if (event.kind == Neuron::InputEventKind::ButtonDown && event.key == VK_LBUTTON)
      OnLeftDown(event, frame);
    else if (event.kind == Neuron::InputEventKind::ButtonUp && event.key == VK_LBUTTON)
      OnLeftUp(event, frame);
    else if (event.kind == Neuron::InputEventKind::ButtonDown && event.key == VK_RBUTTON)
      OnRightDown(event, frame);
    else if (event.kind == Neuron::InputEventKind::KeyDown)
      OnKey(event, frame);
  }

  m_cursorPixels = {static_cast<float>(_input.cursorXPixels), static_cast<float>(_input.cursorYPixels)};
  if (m_pressPixels.has_value() && std::hypot(m_cursorPixels.x - m_pressPixels->x, m_cursorPixels.y - m_pressPixels->y) > DRAG_PIXELS)
    m_dragging = true;
}

std::vector<Outpost::Command> Outpost::PlayerControls::TakeCommands()
{
  return std::exchange(m_commands, {});
}

std::optional<Outpost::ScreenRect> Outpost::PlayerControls::DragBox() const noexcept
{
  if (!m_dragging || !m_pressPixels.has_value())
    return std::nullopt;
  return ScreenRect::Between(m_pressPixels->x, m_pressPixels->y, m_cursorPixels.x, m_cursorPixels.y);
}

void Outpost::PlayerControls::OnLeftDown(const Neuron::InputEvent& _event, const Frame& _frame)
{
  // Attack-move takes the next left click as its destination rather than as a selection.
  if (m_attackMoveArmed)
  {
    m_attackMoveArmed = false;
    const std::optional<PlanePosition> destination =
      _frame.camera.GroundPointAtPixel(static_cast<float>(_event.xPixels), static_cast<float>(_event.yPixels), _frame.viewport);
    if (destination.has_value() && !m_selected.empty())
      Give(AttackMoveCommand{.ships = m_selected, .destination = *destination});
    return;
  }
  m_pressPixels = DirectX::XMFLOAT2{static_cast<float>(_event.xPixels), static_cast<float>(_event.yPixels)};
  m_dragging = false;
}

void Outpost::PlayerControls::OnLeftUp(const Neuron::InputEvent& _event, const Frame& _frame)
{
  if (!m_pressPixels.has_value())
    return;
  const DirectX::XMFLOAT2 press = *m_pressPixels;
  const auto x = static_cast<float>(_event.xPixels);
  const auto y = static_cast<float>(_event.yPixels);
  m_pressPixels.reset();

  if (m_dragging || std::hypot(x - press.x, y - press.y) > DRAG_PIXELS)
  {
    m_dragging = false;
    Select(ShipsInBox(_frame.entities, _frame.camera, _frame.viewport, ScreenRect::Between(press.x, press.y, x, y), _frame.player),
           _event.shift);
    m_lastClickedShip.reset();
    return;
  }

  const PlayerId player = _frame.player;
  const std::optional<EntityId> picked =
    PickShip(_frame.entities, _frame.camera, _frame.viewport, {x, y}, [player](const EntityView& _ship) { return _ship.owner == player; });
  if (!picked.has_value())
  {
    if (!_event.shift)
      m_selected.clear();
    m_lastClickedShip.reset();
    return;
  }

  const bool doubleClick = m_lastClickedShip == picked && IsDouble(m_lastClickMilliseconds, _event.timeMilliseconds);
  const EntityView* ship = Find(_frame.entities, *picked);
  if (doubleClick && ship != nullptr)
  {
    Select(VisibleShipsLike(_frame.entities, _frame.camera, _frame.viewport, *ship, player), _event.shift);
    // A third click starts again rather than counting as another double.
    m_lastClickedShip.reset();
    return;
  }

  if (_event.shift && std::ranges::binary_search(m_selected, *picked))
    std::erase(m_selected, *picked);
  else
    Select({*picked}, _event.shift);
  m_lastClickedShip = picked;
  m_lastClickMilliseconds = _event.timeMilliseconds;
}

void Outpost::PlayerControls::OnRightDown(const Neuron::InputEvent& _event, const Frame& _frame)
{
  m_attackMoveArmed = false;
  if (m_selected.empty())
    return;
  const auto x = static_cast<float>(_event.xPixels);
  const auto y = static_cast<float>(_event.yPixels);

  // An enemy ship under the cursor is attacked; anywhere else is a destination.
  const PlayerId player = _frame.player;
  const std::optional<EntityId> enemy =
    PickShip(_frame.entities, _frame.camera, _frame.viewport, {x, y}, [player](const EntityView& _ship) { return _ship.owner != player; });
  if (enemy.has_value())
  {
    Give(AttackCommand{.ships = m_selected, .target = *enemy});
    return;
  }
  const std::optional<PlanePosition> destination = _frame.camera.GroundPointAtPixel(x, y, _frame.viewport);
  if (destination.has_value())
  {
    Give(MoveCommand{.ships = m_selected, .destination = *destination});
    m_lastMove = MoveOrder{.ships = m_selected, .inputRead = _event.read};
  }
}

void Outpost::PlayerControls::OnKey(const Neuron::InputEvent& _event, const Frame& _frame)
{
  if (_event.key == VK_ESCAPE)
  {
    m_attackMoveArmed = false;
    return;
  }
  if (_event.key == KEY_ATTACK_MOVE && !_event.control)
  {
    m_attackMoveArmed = !m_selected.empty();
    return;
  }
  if (_event.key == KEY_STOP && !_event.control)
  {
    if (!m_selected.empty())
      Give(StopCommand{.ships = m_selected});
    return;
  }

  const std::optional<size_t> group = GroupKey(_event.key);
  if (!group.has_value())
    return;
  if (_event.control)
  {
    m_groups[*group] = m_selected;
    m_lastRecalledGroup.reset();
    return;
  }

  // Recall; a second tap of the same group centers the camera on it.
  Select(m_groups[*group], false);
  const bool doubleTap = m_lastRecalledGroup == group && IsDouble(m_lastRecallMilliseconds, _event.timeMilliseconds);
  m_lastRecalledGroup = group;
  m_lastRecallMilliseconds = _event.timeMilliseconds;
  if (!doubleTap || m_selected.empty())
    return;
  float sumX = 0.0f;
  float sumZ = 0.0f;
  for (const EntityId id : m_selected)
  {
    if (const EntityView* ship = Find(_frame.entities, id))
    {
      sumX += ship->position.xMeters;
      sumZ += ship->position.zMeters;
    }
  }
  const auto count = static_cast<float>(m_selected.size());
  _frame.camera.SetFocus(sumX / count, sumZ / count);
}

void Outpost::PlayerControls::Select(std::vector<EntityId> _ships, bool _add)
{
  if (_add)
    _ships.insert(_ships.end(), m_selected.begin(), m_selected.end());
  std::ranges::sort(_ships);
  const auto duplicates = std::ranges::unique(_ships);
  _ships.erase(duplicates.begin(), duplicates.end());
  m_selected = std::move(_ships);
}

void Outpost::PlayerControls::Give(Order _order)
{
  // The server fills in the player from the connection (ADR-002).
  m_commands.push_back({.player = {}, .order = std::move(_order)});
}

void Outpost::PlayerControls::Prune(const Frame& _frame)
{
  const auto gone = [&_frame](EntityId _id)
  {
    const EntityView* ship = Find(_frame.entities, _id);
    return ship == nullptr || ship->kind != EntityKind::Ship || ship->owner != _frame.player;
  };
  // Before the first snapshot there is nothing to check against.
  if (_frame.entities.empty())
    return;
  std::erase_if(m_selected, gone);
  for (std::vector<EntityId>& group : m_groups)
    std::erase_if(group, gone);
}
