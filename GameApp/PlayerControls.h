#pragma once

namespace Outpost
{
// The player's controls, turned from input into selection and orders (ADR-013 decision 5). Selection and control groups are
// client state; orders leave as Commands for the transport (ADR-002).
//
//   Left-click selects one of the player's ships, or one of its structures; failing both, another player's structure, seen
//   or remembered, alone, for a panel that takes no orders (ADR-081 decision 4). A left-drag selects the player's ships in
//   the box, Shift adds, and a double-click selects every visible ship of that design. Right-click moves the selected ships, or
//   attacks an enemy ship or structure under the cursor; selected Constructors right-clicked on one of the player's own
//   ships or structures that is damaged or under construction repair or build it (ADR-016 decision 5). A, then a left-click,
//   attack-moves; Escape cancels it. H, then a left-click, holds the sector clicked in, and T, then a left-click, patrols
//   to the point clicked (ADR-059 decision 7). S stops. Ctrl+0-9 assigns a control group, 0-9 recalls it, and a second tap
//   centers the camera on it. B turns the command card's build mode on and off while the selection holds a Constructor, as
//   its Build button does, and Escape turns it off once nothing is armed (ADR-088 decision 2). The HUD arms a structure's
//   placement, which the next left-click on the ground orders the selected Constructors to build, and which right-click or
//   Escape cancels; Shift keeps it armed for another, and build mode stays on.
class PlayerControls
{
public:
  // Two clicks or taps closer together than this are a double click or a double tap.
  static constexpr std::uint32_t DOUBLE_CLICK_MILLISECONDS = 400;
  // A left press that moves further than this before it is released is a box, not a click.
  static constexpr float DRAG_PIXELS = 6.0f;
  static constexpr size_t GROUP_COUNT = 10;

  // Reads this frame's input against what the player sees. _entities is the interpolated view; _player is the player
  // this client plays. Orders go to TakeCommands, and a double tap moves _camera.
  void Update(const Neuron::InputState& _input, std::span<const EntityView> _entities, PlayerId _player, Camera& _camera,
              const Viewport& _viewport);

  // The orders given since the last call, in the order they were given.
  [[nodiscard]] std::vector<Command> TakeCommands();

  // From the HUD (ADR-013 decision 5): arms placing a structure, when the selection holds a Constructor.
  void ArmPlacement(StructureKind _structure, std::span<const EntityView> _entities);

  [[nodiscard]] std::optional<StructureKind> Placing() const noexcept
  {
    return m_placing;
  }

  // From the command card's Build button and from B alike (ADR-088 decision 2): turns build mode on while the selection
  // holds a Constructor, or off. It lasts while the selection holds one.
  void ToggleBuilding(std::span<const EntityView> _entities);

  // Whether the command card shows the structures the selected Constructors build, in place of its buttons.
  [[nodiscard]] bool IsBuilding() const noexcept
  {
    return m_building;
  }

  // From the HUD: a job for a Shipyard's or the Command Station's queue.
  void Queue(EntityId _producer, DesignId _design);
  // From the HUD: a topic for the Research Lab's queue (ADR-017 decision 1).
  void Research(EntityId _lab, ResearchTopicId _topic);
  // From the HUD: the structure's next level, which then builds itself (ADR-064 decisions 2 and 3).
  void Upgrade(EntityId _structure);
  // From the HUD: _entity selected on its own, as a click on it selects it, and any order armed for the selection before
  // dropped with it (ADR-066 decision 1).
  void SelectAlone(EntityId _entity);
  // From a fleet's plate: its _ships selected, or added to the selection with _add, and any order armed before dropped
  // (ADR-102 decision 2).
  void SelectFleet(std::vector<EntityId> _ships, bool _add);
  // From the HUD's order buttons and from their keys alike (ADR-088 decision 2): arm attack-move, or holding a
  // sector or a patrol, for the next left-click, while the selection holds a ship, dropping whatever was armed before; or
  // stop the selected ships at once. _entities is the interpolated view.
  void ArmAttackMove(std::span<const EntityView> _entities);
  void ArmStanding(StandingOrder _standing, std::span<const EntityView> _entities);
  void Stop(std::span<const EntityView> _entities);
  // From the designer: a new design, or a new name for a saved one (ADR-017 decision 6).
  void SaveDesign(SaveDesignCommand _save);
  // From the selection panel: the selected ships' retreat (ADR-075 decision 7).
  void SetRetreat(RetreatThreshold _retreat, std::span<const EntityView> _entities);
  // From the minimap: the selected ships move to a point.
  void MoveTo(PlanePosition _destination, std::span<const EntityView> _entities);
  // From the orders window: _form's scheduled order for the selection, when it can give one, its time of day the next
  // moment after _now that _clock reads it (ADR-080 decision 6).
  void Schedule(const OrderForm& _form, std::chrono::sys_seconds _now, const PlayerClock& _clock, const Snapshot& _newest,
                std::span<const EntityView> _entities);
  // From the orders window: cancels the seat's scheduled order _order (ADR-080 decision 9).
  void CancelScheduled(std::uint32_t _order);

  // In identifier order.
  [[nodiscard]] const std::vector<EntityId>& Selected() const noexcept
  {
    return m_selected;
  }

  [[nodiscard]] bool IsAttackMoveArmed() const noexcept
  {
    return m_attackMoveArmed;
  }

  // The standing order the next left click gives, if H or T armed one (ADR-059).
  [[nodiscard]] std::optional<StandingOrder> ArmedStanding() const noexcept
  {
    return m_standingArmed;
  }

  // Whether anything waits for the next click, an attack-move, a standing order or a placement, for Esc to cancel.
  [[nodiscard]] bool IsArmed() const noexcept
  {
    return m_attackMoveArmed || m_standingArmed.has_value() || m_placing.has_value();
  }

  // The box being dragged, while the left button is held past DRAG_PIXELS.
  [[nodiscard]] std::optional<ScreenRect> DragBox() const noexcept;

  // The last move order given since the last call, and when the input that gave it was read.
  struct MoveOrder
  {
    std::vector<EntityId> ships;
    std::chrono::steady_clock::time_point inputRead;
  };

  [[nodiscard]] std::optional<MoveOrder> TakeLastMove()
  {
    return std::exchange(m_lastMove, std::nullopt);
  }

  // An order's mark on the ground (ADR-097): the point a move, an attack-move, a hold of a sector or a patrol goes to, or
  // what an attack is at, from as the order is given until MARK_SECONDS have passed; and whether the server refused it.
  struct OrderMark
  {
    std::uint32_t command = 0;
    std::optional<PlanePosition> point;
    std::optional<EntityId> target;
    float ageSeconds = 0.0f;
    bool refused = false;
  };
  // How long a mark shows, shrinking to nothing.
  static constexpr float MARK_SECONDS = 0.4f;

  // Ages the marks by a frame's wall time, and lets go of those MARK_SECONDS old.
  void AgeMarks(float _elapsedSeconds);
  // Turns the mark of the order numbered _command, while it shows: the server refused the order (ADR-094).
  void Refuse(std::uint32_t _command) noexcept;
  // The marks showing, oldest first.
  [[nodiscard]] const std::vector<OrderMark>& Marks() const noexcept
  {
    return m_marks;
  }

private:
  struct Frame
  {
    std::span<const EntityView> entities;
    PlayerId player;
    Camera& camera;
    const Viewport& viewport;
  };

  void OnLeftDown(const Neuron::InputEvent& _event, const Frame& _frame);
  void OnLeftUp(const Neuron::InputEvent& _event, const Frame& _frame);
  void OnRightDown(const Neuron::InputEvent& _event, const Frame& _frame);
  void OnKey(const Neuron::InputEvent& _event, const Frame& _frame);
  void Select(std::vector<EntityId> _ships, bool _add);
  void Give(Order _order);
  // Gives an order to the selected ships.
  void GiveToShips(Order _order);
  // The selection's ships, and the Constructors among them.
  [[nodiscard]] std::vector<EntityId> SelectedShips(std::span<const EntityView> _entities) const;
  [[nodiscard]] std::vector<EntityId> SelectedConstructors(std::span<const EntityView> _entities) const;
  // Drops ships and structures that are gone or no longer the player's, except another player's structure selected alone.
  void Prune(const Frame& _frame);

  std::vector<EntityId> m_selected;
  std::array<std::vector<EntityId>, GROUP_COUNT> m_groups;
  std::vector<Command> m_commands;
  // The number of the last order given, which numbers the orders from 1 (ADR-094 decision 1).
  std::uint32_t m_lastCommandNumber = 0;
  std::optional<MoveOrder> m_lastMove;
  std::vector<OrderMark> m_marks;
  bool m_attackMoveArmed = false;
  std::optional<StandingOrder> m_standingArmed;
  std::optional<StructureKind> m_placing;
  bool m_building = false;

  // The left press being held, where it went down and whether it has become a drag.
  std::optional<DirectX::XMFLOAT2> m_pressPixels;
  DirectX::XMFLOAT2 m_cursorPixels{};
  bool m_dragging = false;

  // The last click on a ship and the last group recalled, for double clicks and double taps.
  std::optional<EntityId> m_lastClickedShip;
  std::uint32_t m_lastClickMilliseconds = 0;
  std::optional<size_t> m_lastRecalledGroup;
  std::uint32_t m_lastRecallMilliseconds = 0;
};
} // namespace Outpost