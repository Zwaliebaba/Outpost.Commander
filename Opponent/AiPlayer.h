#pragma once

namespace Outpost
{
// The design the AI builds against _enemyWarships, one entry for each enemy warship it has seen (design §10): the first
// counter, from _settings, to the most common of them by hull, drive and weapon whose every component the AI has
// unlocked in _snapshot; the default design when there is none, and when it has seen no warship. A tie goes to the design
// of the lowest hull, then drive, then weapon.
[[nodiscard]] DesignComponents ChooseAnswer(const AiSettings& _settings, std::span<const DesignComponents> _enemyWarships,
                                            const Snapshot& _snapshot);

// The same against the enemy warships _snapshot shows.
[[nodiscard]] DesignComponents ChooseAnswer(const AiSettings& _settings, const Snapshot& _snapshot);

// The computer opponent of design §10. It is a client like the human's (ADR-002): it reads its own player's snapshot every
// tick and answers with orders, which the server validates as it does the human's. Nothing it does is hidden from the
// server or given to it by the server, so it plays by the same rules.
//
// Once a second it builds its base in a fixed order, keeps its Constructors, researches in its order and keeps its
// Shipyards busy with the design it last chose. Every review it chooses that design again from the enemy warships it has
// seen since the last one, and keeps it when it has seen none. Its warships gather in reserve near its Command Station, go
// to the defence of any structure of its that comes under fire, and once enough have gathered they attack the enemy's
// production first, then the next structure (ADR-037). Under fog of war it may know none (ADR-024): the attack then goes
// across the map's center from its own base, where the point-symmetric map puts the enemy's. An attack that has lost too
// many ships falls back and regroups, and the AI fortifies its base as its Shipyards grow (ADR-041).
class AiPlayer
{
public:
  AiPlayer(AiSettings _settings, std::uint32_t _ticksPerSecond);

  // Reads one snapshot of the AI's player, which must be handed every snapshot in order, and returns the orders to send.
  // The snapshot lists its entities in identifier order, as the server sends them.
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
  // Adds the next Shipyard to the plan once the income calls for it.
  void PlanShipyards(const Snapshot& _snapshot);
  // Adds a rig on the nearest asteroid with ore left once one of its rigs' asteroids has run dry (Phase 1 design §13).
  void FollowOre(const Snapshot& _snapshot);
  // The plan's rig on _asteroid, and a Defence Platform beside a rig of the plan, on the side of its base.
  void AddRigSlot(const Snapshot& _snapshot, const EntityView& _asteroid);
  void AddPlatformBesideRig(const Snapshot& _snapshot, size_t _rig);
  // A Defence Platform round the base, toward the map's center, built once the income reaches _minimumIncome (task 12.2).
  void AddHomePlatform(const Snapshot& _snapshot, std::int32_t _minimumIncomeHundredthsPerSecond);
  // Where the enemy's Command Stations are, or under fog of war, where the enemy's base must be.
  [[nodiscard]] std::vector<PlanePosition> EnemyStations(const Snapshot& _snapshot) const;
  // Whether an asteroid is nearer an enemy base than this one: that is the enemy's home.
  [[nodiscard]] bool IsEnemyHome(PlanePosition _asteroid, const std::vector<PlanePosition>& _enemyStations) const;
  [[nodiscard]] bool IsDone(const Slot& _slot, const Snapshot& _snapshot) const;
  [[nodiscard]] bool IsBlocked(const Slot& _slot, const Snapshot& _snapshot) const;
  // What a structure the AI plans must keep clear of: what blocks in _snapshot, and the planned structures not yet placed,
  // apart from slot _skippedSlot. Called during a decision only, and good until the next call.
  [[nodiscard]] std::span<const EntityView> Blockers(const Snapshot& _snapshot, std::optional<size_t> _skippedSlot);
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
  // Its Command Station's place, across the map's center from the enemy's, and its footprint: the base's center, which
  // the Shipyards stand around, still once the station is lost.
  PlanePosition m_home;
  float m_homeRadiusMeters = 0.0f;

  DesignComponents m_productionDesign;
  // The enemy warships it has seen since its last review, and what each is.
  std::map<EntityId, DesignComponents> m_seenWarships;
  std::optional<std::uint64_t> m_designSaveTick;

  // Warships committed to the attack, and the enemy structure they are sent at; or, knowing none, whether they were sent
  // across the map to look for one.
  std::vector<EntityId> m_attackGroup;
  EntityId m_attackTarget;
  bool m_searching = false;
  // The ships of the attack group already ordered to attack its target from close by, once each.
  std::vector<EntityId> m_closingIn;
  // The ships the attack group had when it last grew, which its losses are counted against, and the tick before which a
  // reserve that fell back does not attack again (task 12.2).
  size_t m_launchShips = 0;
  std::uint64_t m_regroupUntilTick = 0;
  // The Defence Platforms planned round the base so far, which places the next (task 12.2).
  size_t m_homePlatforms = 0;
  // Blockers' list: what blocks in the snapshot of the decision under way, the first m_snapshotBlockers, gathered once at
  // the decision's first call, and after them the planned structures of the last call.
  std::vector<EntityView> m_blockers;
  size_t m_snapshotBlockers = 0;
  bool m_blockersGathered = false;
  // Where each reserve warship was last sent, so that it is sent again only when that changes.
  std::map<EntityId, PlanePosition> m_reserveDestinations;
  // Its structures in the last snapshot, and where they stand.
  std::vector<std::pair<EntityId, PlanePosition>> m_structures;
  // The last of them that came under fire, where it stands, and when.
  std::optional<std::uint64_t> m_lastDefenseShotTick;
  PlanePosition m_defensePosition;
};
} // namespace Outpost
