#pragma once

namespace Outpost
{
// The producer the production window shows (Phase 1 design §12; owner, 2026-10-03): one of the player's finished
// Command Stations or Shipyards, the one selected when the window opens, another selected while it is open, or the next
// or the previous stepped to with its arrows. It is client state, as the designer's target is.
class ProductionTarget
{
public:
  // Keeps the target on one of the player's finished producers: one that is gone gives way to the first.
  void Update(const Snapshot& _newest);

  // The producer shown, or nullptr while the player has none.
  [[nodiscard]] const EntityView* Target(const Snapshot& _newest) const;
  // Shows _producer, when it is one of the player's finished producers.
  void Set(EntityId _producer, const Snapshot& _newest);
  // Steps to the next producer, or the previous with a negative _step, round the end.
  void Step(int _step, const Snapshot& _newest);

  // The player's finished producers in _newest: its Command Stations, and then its Shipyards by number.
  [[nodiscard]] static std::vector<const EntityView*> Producers(const Snapshot& _newest);

private:
  EntityId m_target;
};
} // namespace Outpost
