#include "pch.h"
#include "PlayerControls.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::uint8_t KEY_ATTACK_MOVE = 'A';
constexpr std::uint8_t KEY_STOP = 'S';
// The standing orders (ADR-059): hold a sector, and patrol.
constexpr std::uint8_t KEY_HOLD_SECTOR = 'H';
constexpr std::uint8_t KEY_PATROL = 'T';

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

// Whether a Constructor has work to do on _entity: building it, building its next level, or repairing it.
bool NeedsWork(const Outpost::EntityView& _entity) noexcept
{
  return _entity.maxHitPointsHundredths > 0 && (_entity.builtPermille < Outpost::PERMILLE || _entity.upgradePermille.has_value() ||
                                                _entity.hitPointsHundredths < _entity.maxHitPointsHundredths);
}
} // namespace

void Outpost::PlayerControls::Update(const Neuron::InputState& _input, std::span<const EntityView> _entities, PlayerId _player,
                                     Camera& _camera, const Viewport& _viewport)
{
  const Frame frame{.entities = _entities, .player = _player, .camera = _camera, .viewport = _viewport};
  Prune(frame);
  // A placement lasts only while there are Constructors to build it.
  if (m_placing.has_value() && !_entities.empty() && SelectedConstructors(_entities).empty())
    m_placing.reset();

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

void Outpost::PlayerControls::ArmPlacement(StructureKind _structure, std::span<const EntityView> _entities)
{
  m_attackMoveArmed = false;
  m_standingArmed.reset();
  m_placing.reset();
  if (!SelectedConstructors(_entities).empty())
    m_placing = _structure;
}

void Outpost::PlayerControls::Queue(EntityId _producer, DesignId _design)
{
  Give(QueueShipCommand{.producer = _producer, .design = _design});
}

void Outpost::PlayerControls::Research(EntityId _lab, ResearchTopicId _topic)
{
  Give(StartResearchCommand{.lab = _lab, .topic = _topic});
}

void Outpost::PlayerControls::Upgrade(EntityId _structure)
{
  Give(UpgradeStructureCommand{.structure = _structure, .constructors = {}});
}

void Outpost::PlayerControls::SaveDesign(SaveDesignCommand _save)
{
  Give(std::move(_save));
}

void Outpost::PlayerControls::MoveTo(PlanePosition _destination, std::span<const EntityView> _entities)
{
  std::vector<EntityId> ships = SelectedShips(_entities);
  if (!ships.empty())
    Give(MoveCommand{.ships = std::move(ships), .destination = _destination});
}

std::vector<Outpost::EntityId> Outpost::PlayerControls::SelectedShips(std::span<const EntityView> _entities) const
{
  std::vector<EntityId> ships;
  for (const EntityId id : m_selected)
  {
    if (const EntityView* entity = Find(_entities, id); entity != nullptr && entity->kind == EntityKind::Ship)
      ships.push_back(id);
  }
  return ships;
}

std::vector<Outpost::EntityId> Outpost::PlayerControls::SelectedConstructors(std::span<const EntityView> _entities) const
{
  std::vector<EntityId> constructors;
  for (const EntityId id : m_selected)
  {
    if (const EntityView* entity = Find(_entities, id); entity != nullptr && entity->role == ShipRole::Constructor)
      constructors.push_back(id);
  }
  return constructors;
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
    std::vector<EntityId> ships = SelectedShips(_frame.entities);
    if (destination.has_value() && !ships.empty())
      Give(AttackMoveCommand{.ships = std::move(ships), .destination = *destination});
    return;
  }
  // So does a standing order (ADR-059): a hold names the sector the click is in, and the server finds it.
  if (m_standingArmed.has_value())
  {
    const StandingOrder standing = *m_standingArmed;
    m_standingArmed.reset();
    const std::optional<PlanePosition> point =
      _frame.camera.GroundPointAtPixel(static_cast<float>(_event.xPixels), static_cast<float>(_event.yPixels), _frame.viewport);
    std::vector<EntityId> ships = SelectedShips(_frame.entities);
    if (!point.has_value() || ships.empty())
      return;
    if (standing == StandingOrder::HoldSector)
      Give(HoldSectorCommand{.ships = std::move(ships), .position = *point});
    else
      Give(PatrolCommand{.ships = std::move(ships), .destination = *point});
    return;
  }
  // So does a structure's placement, which the server checks and a Mining Rig's snaps (ADR-016). Shift keeps it armed. A
  // Mining Rig is ordered only by an asteroid among the entities the controls are given, which leave out those in space
  // the player has never seen; a click that orders none leaves the placement armed (ADR-046).
  if (m_placing.has_value())
  {
    const std::optional<PlanePosition> site =
      _frame.camera.GroundPointAtPixel(static_cast<float>(_event.xPixels), static_cast<float>(_event.yPixels), _frame.viewport);
    const auto inReach = [&site](const EntityView& _asteroid)
    {
      return _asteroid.kind == EntityKind::Asteroid &&
             std::hypot(_asteroid.position.xMeters - site->xMeters, _asteroid.position.zMeters - site->zMeters) - _asteroid.radiusMeters <=
               RIG_SNAP_METERS;
    };
    if (*m_placing == StructureKind::MiningRig && site.has_value() && std::ranges::none_of(_frame.entities, inReach))
      return;
    std::vector<EntityId> constructors = SelectedConstructors(_frame.entities);
    if (site.has_value() && !constructors.empty())
      Give(BuildStructureCommand{.constructors = std::move(constructors), .structure = *m_placing, .position = *site});
    if (!_event.shift)
      m_placing.reset();
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
    // One of the player's structures is selected on its own, for its queue and its state (task 4.5).
    const std::optional<EntityId> structure =
      PickEntity(_frame.entities, _frame.camera, _frame.viewport, {x, y},
                 [player](const EntityView& _entity) { return _entity.kind == EntityKind::Structure && _entity.owner == player; });
    if (structure.has_value())
      m_selected = {*structure};
    else if (!_event.shift)
      m_selected.clear();
    m_lastClickedShip.reset();
    return;
  }
  // A ship clicked with Shift joins ships, not a structure.
  if (_event.shift)
    m_selected = SelectedShips(_frame.entities);

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
  m_standingArmed.reset();
  // Right-click cancels a placement rather than ordering.
  if (m_placing.has_value())
  {
    m_placing.reset();
    return;
  }
  std::vector<EntityId> ships = SelectedShips(_frame.entities);
  if (ships.empty())
    return;
  const auto x = static_cast<float>(_event.xPixels);
  const auto y = static_cast<float>(_event.yPixels);
  const PlayerId player = _frame.player;

  // Constructors repair, or build, one of the player's own ships or structures that needs it; the rest of the selection
  // goes there.
  std::vector<EntityId> constructors = SelectedConstructors(_frame.entities);
  if (!constructors.empty())
  {
    const std::optional<EntityId> friendly =
      PickEntity(_frame.entities, _frame.camera, _frame.viewport, {x, y},
                 [&](const EntityView& _entity)
                 {
                   return _entity.owner == player && (_entity.kind == EntityKind::Ship || _entity.kind == EntityKind::Structure) &&
                          NeedsWork(_entity) && std::ranges::find(constructors, _entity.id) == constructors.end();
                 });
    if (friendly.has_value())
    {
      std::erase_if(ships, [&constructors](EntityId _id) { return std::ranges::find(constructors, _id) != constructors.end(); });
      Give(RepairCommand{.constructors = std::move(constructors), .target = *friendly});
      if (const EntityView* target = Find(_frame.entities, *friendly); target != nullptr && !ships.empty())
        Give(MoveCommand{.ships = std::move(ships), .destination = target->position});
      return;
    }
  }

  // An enemy ship under the cursor is attacked, or failing that an enemy structure; anywhere else is a destination.
  const auto isEnemy = [player](const EntityView& _entity) { return _entity.owner.IsValid() && _entity.owner != player; };
  std::optional<EntityId> enemy = PickShip(_frame.entities, _frame.camera, _frame.viewport, {x, y}, isEnemy);
  if (!enemy.has_value())
  {
    enemy = PickEntity(_frame.entities, _frame.camera, _frame.viewport, {x, y},
                       [&isEnemy](const EntityView& _entity) { return _entity.kind == EntityKind::Structure && isEnemy(_entity); });
  }
  if (enemy.has_value())
  {
    Give(AttackCommand{.ships = std::move(ships), .target = *enemy});
    return;
  }
  const std::optional<PlanePosition> destination = _frame.camera.GroundPointAtPixel(x, y, _frame.viewport);
  if (destination.has_value())
  {
    m_lastMove = MoveOrder{.ships = ships, .inputRead = _event.read};
    Give(MoveCommand{.ships = std::move(ships), .destination = *destination});
  }
}

void Outpost::PlayerControls::OnKey(const Neuron::InputEvent& _event, const Frame& _frame)
{
  if (_event.key == VK_ESCAPE)
  {
    m_attackMoveArmed = false;
    m_standingArmed.reset();
    m_placing.reset();
    return;
  }
  if (_event.key == KEY_ATTACK_MOVE && !_event.control)
  {
    m_placing.reset();
    m_standingArmed.reset();
    m_attackMoveArmed = !SelectedShips(_frame.entities).empty();
    return;
  }
  if ((_event.key == KEY_HOLD_SECTOR || _event.key == KEY_PATROL) && !_event.control)
  {
    m_placing.reset();
    m_attackMoveArmed = false;
    m_standingArmed.reset();
    if (!SelectedShips(_frame.entities).empty())
      m_standingArmed = _event.key == KEY_HOLD_SECTOR ? StandingOrder::HoldSector : StandingOrder::Patrol;
    return;
  }
  if (_event.key == KEY_STOP && !_event.control)
  {
    if (std::vector<EntityId> ships = SelectedShips(_frame.entities); !ships.empty())
      Give(StopCommand{.ships = std::move(ships)});
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
    const EntityView* entity = Find(_frame.entities, _id);
    return entity == nullptr || (entity->kind != EntityKind::Ship && entity->kind != EntityKind::Structure) ||
           entity->owner != _frame.player;
  };
  // Before the first snapshot there is nothing to check against.
  if (_frame.entities.empty())
    return;
  std::erase_if(m_selected, gone);
  for (std::vector<EntityId>& group : m_groups)
    std::erase_if(group, gone);
}