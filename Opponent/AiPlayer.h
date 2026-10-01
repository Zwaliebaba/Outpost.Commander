#pragma once

namespace Outpost
{
// The design the AI builds against the fleet in _snapshot (design §10): the counter, from _settings, to the enemy's most
// common warship by hull, drive and weapon, when the AI has unlocked every component of it; the default design otherwise,
// and while the enemy has no warship. A tie goes to the design of the lowest hull, then drive, then weapon.
[[nodiscard]] DesignComponents ChooseAnswer(const AiSettings& _settings, const Snapshot& _snapshot);

// The computer opponent of design §10. It is a client like the human's (ADR-002): it reads its own player's snapshot every
// tick and answers with orders, which the server validates as it does the human's. Nothing it does is hidden from the
// server or given to it by the server, so it plays by the same rules.
//
// Once a second it builds its base in a fixed order, keeps its Constructors, researches in its order and keeps its
// Shipyards busy with the design it last chose. Every review it chooses that design again from the enemy's fleet. Its
// warships gather in reserve near its Command Station, go to the defence of a Mining Rig or Defence Platform of its that
// comes under fire, and once enough have gathered they attack the nearest enemy structure, then the next, until none of
// them is left.
class AiPlayer
{
public:
  AiPlayer(AiSettings _settings, std::uint32_t _ticksPerSecond);

  // Reads one snapshot of the AI's player, which must be handed every snapshot in order, and returns the orders to send.
  [[nodiscard]] std::vector<Command> Update(const Snapshot& _snapshot);

  // The design the AI chose at its last review.
  [[nodiscard]] const DesignComponents& ProductionDesign() const noexcept
  {
    return m_productionDesign;
  }

  // The warships it has committed to an attack and that still live.
  [[nodiscard]] size_t AttackGroupShips() const noexcept
  {
    return m_attackGroup.size();
  }

private:
  // A structure the AI's base plan holds, in the order it builds them (owner, 2026-10-01).
  struct Slot
  {
    StructureKind structure = StructureKind::MiningRig;
    PlanePosition position;
    float radiusMeters = 0.0f;
    // A Mining Rig's asteroid.
    EntityId asteroid;
    // A Defence Platform beside a contested rig is skipped while that rig's asteroid is someone else's.
    std::optional<size_t> besideRig;
    // In hundredths of an Ore a second: the slot waits until the AI's income has reached it. Zero for none.
    std::int32_t minimumIncomeHundredthsPerSecond = 0;
    // No place for it could be found; the AI builds the rest of its plan without it.
    bool abandoned = false;
  };

  // Constructors the AI has sent to one structure: a site it has ordered and is waiting to see, or one it can.
  struct Work
  {
    std::vector<EntityId> constructors;
    EntityId target;
    // The slot a build order was for; none for a repair or a site it did not order.
    std::optional<size_t> slot;
    std::uint64_t orderedTick = 0;
  };

  void Watch(const Snapshot& _snapshot);
  void Decide(const Snapshot& _snapshot, std::vector<Command>& _orders);
  void Plan(const Snapshot& _snapshot, const EntityView& _station);
  [[nodiscard]] bool IsDone(const Slot& _slot, const Snapshot& _snapshot) const;
  [[nodiscard]] bool IsBlocked(const Slot& _slot, const Snapshot& _snapshot) const;
  // What a structure the AI plans must keep clear of: what blocks in _snapshot, and the planned structures not yet placed,
  // apart from slot _skippedSlot.
  [[nodiscard]] std::vector<EntityView> Blockers(const Snapshot& _snapshot, std::optional<size_t> _skippedSlot) const;
  void TendWork(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders);
  // Orders the next structures of the plan, and says whether the next one waits for Ore.
  [[nodiscard]] bool Build(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders);
  void Produce(const Snapshot& _snapshot, bool _structureWaiting, std::vector<Command>& _orders);
  void CommandFleet(const Snapshot& _snapshot, std::vector<Command>& _orders);

  AiSettings m_settings;
  std::uint32_t m_ticksPerSecond = 0;
  PlayerId m_player;
  std::uint64_t m_nextDecisionTick = 0;
  std::uint64_t m_nextReviewTick = 0;

  bool m_planned = false;
  std::vector<Slot> m_slots;
  std::vector<Work> m_work;
  PlanePosition m_rally;

  DesignComponents m_productionDesign;
  std::optional<std::uint64_t> m_designSaveTick;

  // Warships committed to the attack, and the enemy structure they are sent at.
  std::vector<EntityId> m_attackGroup;
  EntityId m_attackTarget;
  // Where each reserve warship was last sent, so that it is sent again only when that changes.
  std::map<EntityId, PlanePosition> m_reserveDestinations;
  // Its Mining Rigs and Defence Platforms in the last snapshot, and where they stand.
  std::vector<std::pair<EntityId, PlanePosition>> m_outposts;
  // The last of them that came under fire, where it stands, and when.
  std::optional<std::uint64_t> m_lastDefenseShotTick;
  PlanePosition m_defensePosition;
};
} // namespace Outpost
