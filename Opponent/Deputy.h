#pragma once

namespace Outpost
{
// A seat's deputy (Phase 5 design §6, gate H3, ADR-079): a keeper, which keeps its player's empire running while the player
// is away and plays for nothing more. It watches the seat while its player plays it, and plays it while the server says
// the player is away. Once a second it:
//
// - keeps the player's Research Lab researching: the AI's research order first, then any topic the Lab can take;
// - has each Shipyard whose queue has emptied build again what it last built, a job the Ore and the fleet cap start in
//   their time, and upgrades the Command Station, as the AI does, when the cap holds production back;
// - keeps as many Constructors as the player had when the deputy took the seat (owner, 2026-10-08);
// - rebuilds a lost Mining Rig on an asteroid the player mined, in a sector the player holds, and repairs and finishes the
//   player's structures;
// - sends the player's idle warships at enemy warships in a sector the player holds, or at whatever fires on the player's
//   structures, and moves them back to where they stood once the attack is over (owner, 2026-10-08).
//
// It never attacks, raids, claims a sector or clears pirates, and it leaves a ship that is retreating, or that has an order,
// a standing order or a scheduled order of the player's, alone (ADR-080).
class Deputy final : public HostedPlayer
{
public:
  Deputy(AiSettings _settings, std::uint32_t _ticksPerSecond);

  [[nodiscard]] std::vector<Command> Play(const Snapshot& _snapshot) override;
  void Watch(const Snapshot& _snapshot) override;

  // The warships it has sent to defend and not yet brought back.
  [[nodiscard]] size_t Defenders() const noexcept
  {
    return m_defenders.size();
  }

private:
  // What it learns from every snapshot, whoever plays the seat: what each Shipyard last built, and where the player's rigs
  // stand.
  void Learn(const Snapshot& _snapshot);
  void Decide(const Snapshot& _snapshot, std::vector<Command>& _orders);
  void Research(const Snapshot& _snapshot, std::vector<Command>& _orders) const;
  // Queues its Shipyards' next ships. Says whether the fleet cap holds one back.
  [[nodiscard]] bool Produce(const Snapshot& _snapshot, std::vector<Command>& _orders) const;
  void UpgradeStation(const Snapshot& _snapshot, std::int32_t& _ore, std::vector<Command>& _orders) const;
  void KeepConstructors(const Snapshot& _snapshot, std::vector<Command>& _orders) const;
  void RebuildRigs(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::int32_t& _ore, std::vector<Command>& _orders);
  void Repair(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders);
  void Defend(const Snapshot& _snapshot, std::vector<Command>& _orders);
  // The design a Shipyard builds next: what it last built, or the design most of the player's warships are of; none when it
  // knows neither, or the Shipyard's level cannot build it.
  [[nodiscard]] const DesignView* NextDesign(const Snapshot& _snapshot, const EntityView& _yard) const;

  AiSettings m_settings;
  std::uint32_t m_ticksPerSecond = 0;
  PlayerId m_player;
  std::uint64_t m_nextDecisionTick = 0;
  // Whether the seat was its player's at the last snapshot, so that the next it plays begins its turn.
  bool m_watching = true;

  // What each Shipyard built last, from its queue; and where the player has had a Mining Rig.
  std::map<EntityId, DesignId> m_lastBuilt;
  std::vector<PlanePosition> m_rigSites;
  // A rig it ordered, by its site, and when, so that it is not ordered again before its site appears.
  std::vector<std::pair<PlanePosition, std::uint64_t>> m_rigOrders;

  // Its turn's: the Constructors it keeps, set when it takes the seat; the structure each Constructor it sent works on;
  // and each warship it sent to defend, with where it stood, which it goes back to.
  std::int32_t m_constructors = 0;
  std::map<EntityId, EntityId> m_repairs;
  std::map<EntityId, PlanePosition> m_defenders;
  // Where the attack it answers is, and the last tick it saw the attack there.
  PlanePosition m_threat;
  std::optional<std::uint64_t> m_threatTick;
  // Its structures in the last snapshot, and where they stand, since the shot that destroys one names a structure that has
  // left the snapshot.
  std::vector<std::pair<EntityId, PlanePosition>> m_structures;
};
} // namespace Outpost
