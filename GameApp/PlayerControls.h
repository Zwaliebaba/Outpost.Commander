#pragma once

namespace Outpost
{
// Design §9's player controls, turned from input into selection and orders (task 2.6). Selection and control groups are
// client state; orders leave as Commands for the transport (ADR-002).
//
//   Left-click selects a ship, or one of the player's structures, a left-drag selects the player's ships in the box,
//   Shift adds, and a double-click selects every visible ship of that design. Right-click moves the selected ships, or
//   attacks an enemy ship or structure under the cursor; selected Constructors right-clicked on one of the player's own
//   ships or structures that is damaged or under construction repair or build it (task 4.2). A, then a left-click,
//   attack-moves; Escape cancels it. H, then a left-click, holds the sector clicked in, and T, then a left-click, patrols
//   to the point clicked (Phase 2 design §9, ADR-059). S stops. Ctrl+0-9 assigns a control group, 0-9 recalls it, and a second tap
//   centers the camera on it. The HUD arms a structure's placement, which the next left-click on the ground orders the
//   selected Constructors to build, and which right-click or Escape cancels; Shift keeps it armed for another.
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

  // From the HUD (task 4.5): arms placing a structure, when the selection holds a Constructor.
  void ArmPlacement(StructureKind _structure, std::span<const EntityView> _entities);

  [[nodiscard]] std::optional<StructureKind> Placing() const noexcept
  {
    return m_placing;
  }

  // From the HUD: a job for a Shipyard's or the Command Station's queue.
  void Queue(EntityId _producer, DesignId _design);
  // From the HUD: a topic for the Research Lab's queue (task 5.1).
  void Research(EntityId _lab, ResearchTopicId _topic);
  // From the designer: a new design, or a new name for a saved one (task 5.2).
  void SaveDesign(SaveDesignCommand _save);
  // From the minimap: the selected ships move to a point.
  void MoveTo(PlanePosition _destination, std::span<const EntityView> _entities);

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

  // The box being dragged, while the left button is held past DRAG_PIXELS.
  [[nodiscard]] std::optional<ScreenRect> DragBox() const noexcept;

  // The last move order given since the last call, and when the input that gave it was read (task 2.7).
  struct MoveOrder
  {
    std::vector<EntityId> ships;
    std::chrono::steady_clock::time_point inputRead;
  };

  [[nodiscard]] std::optional<MoveOrder> TakeLastMove()
  {
    return std::exchange(m_lastMove, std::nullopt);
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
  // The selection's ships, and the Constructors among them.
  [[nodiscard]] std::vector<EntityId> SelectedShips(std::span<const EntityView> _entities) const;
  [[nodiscard]] std::vector<EntityId> SelectedConstructors(std::span<const EntityView> _entities) const;
  // Drops ships that are gone or no longer the player's.
  void Prune(const Frame& _frame);

  std::vector<EntityId> m_selected;
  std::array<std::vector<EntityId>, GROUP_COUNT> m_groups;
  std::vector<Command> m_commands;
  std::optional<MoveOrder> m_lastMove;
  bool m_attackMoveArmed = false;
  std::optional<StandingOrder> m_standingArmed;
  std::optional<StructureKind> m_placing;

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