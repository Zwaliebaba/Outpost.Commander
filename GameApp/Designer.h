#pragma once

namespace Outpost
{
// The ship designer in the Shipyard panel (task 5.2, design §9): a pick for each slot, a name the player may type, and
// the live stats of the picked design, derived from the snapshot's components with the function the server uses
// (ADR-017). It is client state and pauses nothing. The HUD draws it; this holds what it shows and what saving sends.
//
// The picked components either match one of the player's saved designs or they do not. A match shows that design's name
// and can be renamed or queued; anything else shows a name made of its components, and can be saved as a new design once
// research has unlocked every component in it.
class Designer
{
public:
  // Keeps every pick on an available component of the newest snapshot: a slot with no pick, or with one the snapshot no
  // longer lists as available, takes the first available component.
  void Update(const Snapshot& _newest);

  void PickHull(HullId _hull) noexcept
  {
    m_hull = _hull;
  }
  void PickDrive(DriveId _drive) noexcept
  {
    m_drive = _drive;
  }
  void PickWeapon(WeaponId _weapon) noexcept
  {
    m_weapon = _weapon;
  }
  [[nodiscard]] DesignComponents Picked() const noexcept
  {
    return {m_hull, m_drive, m_weapon};
  }

  // The player's saved design of the picked components, if there is one.
  [[nodiscard]] const DesignView* Match(const Snapshot& _newest) const noexcept;

  // The name as typed, or else the matching design's, or else the components' names as design §7 writes them, such as
  // "Small+Ion+Mass Driver".
  [[nodiscard]] std::string Name(const Snapshot& _newest) const;

  // The picked design's stats, or none while a pick names nothing in the snapshot.
  [[nodiscard]] std::optional<DesignStats> Stats(const Snapshot& _newest) const;

  // What saving sends now: a new design of available components that is not saved yet, or a new name for the matching
  // design; nothing when the name is not one the server takes (IsValidDesignName) or nothing would change.
  [[nodiscard]] std::optional<SaveDesignCommand> SaveCommand(const Snapshot& _newest) const;

  // The name field. Editing starts from the name shown, takes printable characters up to DESIGN_NAME_LIMIT and Backspace;
  // Enter returns what saving sends and stops, Escape drops what was typed and stops. A name typed and left stays until
  // the design is saved.
  void BeginEditing(const Snapshot& _newest);
  void EndEditing() noexcept
  {
    m_editing = false;
  }
  [[nodiscard]] bool IsEditing() const noexcept
  {
    return m_editing;
  }
  std::optional<SaveDesignCommand> Edit(const Neuron::InputEvent& _event, const Snapshot& _newest);

  // After a save is sent, the name follows the snapshot again.
  void ForgetTypedName() noexcept
  {
    m_typed.reset();
    m_editing = false;
  }

  // How long a save may take to come back in a snapshot before it counts as refused: two seconds at the server's 20 ticks
  // a second.
  static constexpr std::uint64_t SAVE_WAIT_TICKS = 40;

  // Queue on picks that are no saved design yet (ADR-023): a queue at _producer waits for the design, and this returns
  // the save to send, unless one for the same picks is already on its way. Nothing when SaveCommand has no new design.
  [[nodiscard]] std::optional<SaveDesignCommand> SaveAndQueue(EntityId _producer, const Snapshot& _newest);

  // The waiting queues whose design _newest holds, now that the server has saved it. A queue whose design is not in a
  // snapshot within SAVE_WAIT_TICKS of its save was refused, and is dropped.
  [[nodiscard]] std::vector<QueueShipCommand> TakeQueueCommands(const Snapshot& _newest);

private:
  struct WaitingQueue
  {
    EntityId producer;
    DesignComponents components;
    std::uint64_t savedTick = 0;
  };

  HullId m_hull;
  DriveId m_drive;
  WeaponId m_weapon;
  std::optional<std::string> m_typed;
  bool m_editing = false;
  std::vector<WaitingQueue> m_waiting;
};
} // namespace Outpost
