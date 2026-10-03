#pragma once

namespace Outpost
{
// What a ship is doing because it was told to (design §7, §9). Firing is not an order: an armed ship fires at whatever its
// targeting picks, whatever its order.
enum class ShipOrder : std::uint8_t
{
  // Holding where it is.
  None,
  // Heading for a point, firing on the move.
  Move,
  // Heading for a point, and standing to fire while an enemy is in range.
  AttackMove,
  // Closing on one target and firing at it until it dies.
  Attack,
  // A Constructor's: heading for a structure or ship of its own side, and building or repairing it once in reach.
  Work
};

// How a ship picks what to fire at. Nearest is the game's rule (design §7). The other two are the balance check's two
// extremes, forced by its headless battles (task 3.4) and by nothing in a match.
enum class TargetRule : std::uint8_t
{
  // The nearest enemy ship in range, or with none in range the nearest enemy structure, kept until it dies or leaves range.
  Nearest,
  // A random enemy in range, chosen again for every shot (spread fire). The ship stands and moves as under Nearest.
  Random,
  // The enemy in range with the fewest hit points, chosen again for every shot (focus fire), likewise.
  Weakest
};

// One entity as the server holds it. What a player may see of it is the EntityView a snapshot carries.
struct Entity
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  PlayerId owner;
  DesignId design;
  // Meaningful for a ship only.
  HullId hull;
  ShipRole role = ShipRole::Warship;
  StructureKind structure = StructureKind::CommandStation;
  PlanePosition position;
  float headingRadians = 0.0f;
  float radiusMeters = 0.0f;
  // Meaningful for an ore asteroid, and for the Mining Rig on one.
  OreYield oreYield = OreYield::Home;
  // An ore asteroid's Ore left, in hundredths, which its rig draws down (Phase 1 design §8); none for one that never runs
  // out.
  std::optional<std::int64_t> oreReserveHundredths;
  // A Mining Rig's draw on its asteroid's reserve not yet whole hundredths, in hundredths times ticks a second, as a
  // player's Ore is paid (ADR-017).
  std::int64_t reserveRemainder = 0;
  // A Mining Rig's asteroid.
  EntityId site;
  // Meaningful for a ship only; its radius is radiusMeters above.
  float speedMetersPerSecond = 0.0f;
  float turnRateRadiansPerSecond = 0.0f;
  // Where a ship's move order sends it, until it arrives: the group's destination, not the ship's own slot in it.
  std::optional<PlanePosition> destination;
  // The rest of the ship's way to its slot, and the speed that brings it there with the rest of its group.
  std::vector<PlanePosition> path;
  float cruiseSpeedMetersPerSecond = 0.0f;
  // How close the ship has come to its next waypoint, and for how many ticks it has come no closer.
  float closestMeters = 0.0f;
  std::uint32_t stalledTicks = 0;

  // Hit points left and at full strength, in hundredths (ADR-014). An entity with none at full strength is out of
  // combat: nothing targets or damages it. Asteroids and fields are, and so are ships and structures placed without
  // hit points, which movement tests and task 2.7's load do.
  std::int32_t hitPointsHundredths = 0;
  std::int32_t maxHitPointsHundredths = 0;
  std::int32_t armorHundredths = 0;
  ShipOrder order = ShipOrder::None;
  // An attack order's target, while the order lasts.
  EntityId attackTarget;
  // What the ship fires at this tick: its attack order's target when that is in range, otherwise what its targeting
  // picked. No identifier when nothing is in range.
  EntityId target;
  // Thousandths of a tick until the weapon may fire again. It fires on the tick this reaches zero or less, and the fire
  // interval is added back, so a fractional interval keeps its average. Idle for a whole interval past ready, the weapon
  // is cold, and its next first shot comes at a random moment (ADR-014).
  std::int32_t reloadMilliticks = 0;
  // The attack order's target's position when the ship last pathed to it, and the tick it may path again.
  PlanePosition chasedPosition;
  std::uint64_t chaseTick = 0;
  // A Constructor's work order's target, while the order lasts.
  EntityId workTarget;

  // A structure's construction, in thousandths of a tick of one Constructor's work: built once the two are equal. Both
  // are zero for a structure placed whole (ADR-016).
  std::int32_t buildWorkDone = 0;
  std::int32_t buildWorkNeeded = 0;
  // The Defence gun a built structure carries, if any (design §6).
  StructureWeaponId structureWeapon;
  // A Shipyard's or the Command Station's jobs, front first, and the front job's progress in thousandths of a tick. The
  // work needed is zero until the job starts, which is when it is paid for (design §5).
  std::vector<JobView> queue;
  // A Research Lab's topics, front first, likewise; its front topic's progress is the job's (design §8).
  std::vector<ResearchTopicId> researchQueue;
  std::int32_t jobWorkDone = 0;
  std::int32_t jobWorkNeeded = 0;
  // A Shipyard's number among its owner's, given when it is first finished, and the ships it has built (Phase 1 design
  // §11).
  std::uint32_t shipyardNumber = 0;
  std::uint32_t shipsBuilt = 0;

  [[nodiscard]] bool IsBuilt() const noexcept
  {
    return buildWorkDone >= buildWorkNeeded;
  }

  friend bool operator==(const Entity&, const Entity&) = default;
};

// What became of one command. Only the server sees it: the protocol has no way to tell a client yet.
enum class CommandResult : std::uint8_t
{
  Applied,
  // The order names no ships.
  NoShips,
  // An entity it names does not exist.
  UnknownEntity,
  // An entity it names is not a ship.
  NotAShip,
  // A ship it names belongs to another player.
  NotOwned,
  // A position in it is not a finite number.
  InvalidPosition,
  // An attack's target does not exist.
  UnknownTarget,
  // An attack's target is not an enemy ship or structure that combat can touch.
  NotAnEnemy,
  // A ship it names to build or repair is not a Constructor.
  NotAConstructor,
  // The structure is one no Constructor builds: the Command Station.
  NotBuildable,
  // The structure would overlap something, cross the map's edge, or, for a Mining Rig, is not on a free ore asteroid.
  InvalidPlacement,
  // The player has too little Ore to start the job (design §5).
  NotEnoughOre,
  // The player already has the one Research Lab it may have (design §6).
  LimitReached,
  // A repair's target is not one of the player's own ships or structures under construction or damaged.
  NotRepairable,
  // A queue's producer is not one of the player's built Shipyards or its Command Station.
  NotAProducer,
  // A Shipyard's queue names no design of the player's.
  UnknownDesign,
  // The queue already holds five jobs (design §6).
  QueueFull,
  // A research order's lab is not the player's built Research Lab.
  NotALab,
  // A research order's topic, or a design's hull, drive or weapon, is not in the tuning data.
  UnknownTopic,
  UnknownComponent,
  // The topic is researched already, or already in the lab's queue.
  AlreadyResearched,
  // A topic the research needs first is neither researched nor ahead of it in the queue (design §8).
  PrerequisiteMissing,
  // A design's name is empty, too long, or holds a character the HUD cannot show.
  InvalidName,
  // A new design uses a component the player has not unlocked (design §8).
  ComponentLocked,
  // The player already has a design of these components.
  DuplicateDesign,
  // A saved design's components never change; a design of other components is saved as a new one (ADR-017).
  ComponentsFixed,
  // An attack's target is an enemy the player neither sees nor, for a structure, remembers (ADR-024).
  NotVisible,
  // The order is valid protocol, but the task that gives it meaning has not been built yet.
  NotYetSupported
};

// The authoritative game state and the rules that change it (ADR-002). It runs one tick at a time and knows nothing of
// wall time, transports or clients: the same seed and the same commands at the same ticks give the same state, which is
// what a replay relies on (ADR-009).
class Simulation
{
public:
  // _ticksPerSecond is the rate the simulation is run at, which sets how far a ship moves in one tick and how many ticks
  // a weapon takes to reload.
  Simulation(std::uint64_t _seed, std::uint32_t _ticksPerSecond);

  // Match setup, before the first tick: places the map's ore asteroids and then its fields, in the file's order, as
  // entities with no owner (task 2.3).
  void PlaceMap(const Map& _map);

  // Match setup: builds the pathfinding graph for ships of this footprint radius ahead of their first order.
  void PreparePathfinding(float _radiusMeters) const
  {
    m_pathfinder.Prepare(_radiusMeters);
  }

  // Match setup: the tuning data the base's rules come from: structures, the Constructor, the Defence gun and Ore income
  // (milestone 4). Without it the simulation still moves and fights, and rejects orders to build, repair or queue as not
  // yet supported, which is what the movement and combat tests run on.
  void UseTuning(const Tuning& _tuning);

  // Match setup, after UseTuning: fog of war (ADR-024). Each player then sees what its own ships and structures see, the
  // tuning data's sight beyond their weapons' range, and an enemy shooter for a while after each hit it lands; it
  // remembers the enemy structures it has seen. Without it every player sees everything. Throws Neuron::Exception before
  // UseTuning.
  void UseFog();

  // Whether _player sees _entity now: its own and the map's always; an enemy's only under the rules of UseFog, as of the
  // end of the last tick.
  [[nodiscard]] bool Sees(PlayerId _player, const Entity& _entity) const noexcept;

  // How far _entity sees under fog of war: its weapon's range and the tuning data's margin, or the unarmed sight. Zero
  // before UseTuning.
  [[nodiscard]] float SightMetersOf(const Entity& _entity) const noexcept;

  // Match setup: a player and its Ore. A snapshot for a player not added here shows no Ore.
  void AddPlayer(PlayerId _player, std::int32_t _ore);

  // The player's Ore in hundredths, or zero for a player not added.
  [[nodiscard]] std::int64_t OreHundredths(PlayerId _player) const noexcept;

  // The topics the player has researched, in the order they finished; empty for a player not added (design §8).
  [[nodiscard]] std::span<const ResearchTopicId> Researched(PlayerId _player) const noexcept;

  // What the player's research has done to its rates; none before UseTuning.
  [[nodiscard]] Upgrades UpgradesOf(PlayerId _player) const;
  // A structure's full hit points, in hundredths, and a Constructor's speed, with _owner's research (Phase 1 design §6).
  [[nodiscard]] std::int32_t StructureHitPoints(PlayerId _owner, const StructureTuning& _tuning) const;
  [[nodiscard]] float ConstructorSpeed(PlayerId _owner) const;

  // Saves a design for _owner and returns its identifier. Match setup saves the starting designs; the designer sends a
  // command (task 5.2).
  DesignId SaveDesign(PlayerId _owner, std::string _name, const DesignComponents& _components, const DesignStats& _stats);

  // Match setup: saves every starting design for _owner (design §7). Throws Neuron::Exception when the tuning data has
  // none.
  void SaveStartingDesigns(PlayerId _owner, const Tuning& _tuning);

  // The design with this identifier, or nullptr.
  [[nodiscard]] const ShipDesign* FindDesign(DesignId _id) const noexcept;

  // _owner's design of these components, or nullptr.
  [[nodiscard]] const ShipDesign* FindDesign(PlayerId _owner, const DesignComponents& _components) const noexcept;

  // Places a ship of one of _owner's saved designs, with its movement and full hit points. Throws Neuron::Exception when
  // the design does not exist or is another player's.
  EntityId SpawnShip(PlayerId _owner, DesignId _design, PlanePosition _position, float _headingRadians = 0.0f);

  // Places a ship that only moves: it has no weapon and no hit points, so combat does not touch it. Movement tests and
  // task 2.7's load use it.
  EntityId SpawnShip(PlayerId _owner, DesignId _design, const ShipMovement& _movement, PlanePosition _position, HullId _hull = {},
                     float _headingRadians = 0.0f);

  // Places a Constructor of the tuning data's, with full hit points. Throws Neuron::Exception before UseTuning.
  EntityId SpawnConstructor(PlayerId _owner, PlanePosition _position, float _headingRadians = 0.0f);

  // Places a built structure, which blocks movement unless it is a Mining Rig, which stands on its asteroid. Once the
  // tuning data is known, a kind that carries a Defence gun is armed. Without hit points it is out of combat, as task
  // 2.7's load places them.
  EntityId SpawnStructure(PlayerId _owner, StructureKind _kind, PlanePosition _position, float _radiusMeters,
                          std::int32_t _hitPointsHundredths = 0, std::int32_t _armorHundredths = 0);

  // Match setup, after PlaceMap and UseTuning: gives every player its Command Station on its start and the starting
  // Constructors in front of it, facing the map's center (design §6). Throws Neuron::Exception when the base would
  // overlap an obstacle or cross the map's edge. From then on a player left with neither a Command Station nor a
  // finished Shipyard loses the match (Phase 1 design §4).
  void PlaceStartingBases(const Map& _map);

  // Whether a player has lost its Command Station and its last finished Shipyard, which ends the match (Phase 1 design
  // §4); the player who still has one of them won, or nobody when both lost theirs in the same tick. Only a match whose
  // bases were placed can end.
  [[nodiscard]] bool MatchOver() const noexcept
  {
    return m_matchOver;
  }
  [[nodiscard]] PlayerId Winner() const noexcept
  {
    return m_winner;
  }

  // How every ship picks its target. Only the balance check's headless battles change it (task 3.4).
  void SetTargetRule(TargetRule _rule) noexcept
  {
    m_targetRule = _rule;
  }

  // Runs one tick: applies _commands in order at its start, then advances the world. Returns one result per command.
  // _observer, when there is one, is told where each part of the tick begins and ends, for measurement (task 8.1). The
  // simulation reads no clock itself (ADR-009), and nothing the observer learns reaches the state.
  [[nodiscard]] std::vector<CommandResult> Tick(const std::vector<Command>& _commands, TickObserver* _observer = nullptr);

  // The ticks run so far. A snapshot built now is stamped with it.
  [[nodiscard]] std::uint64_t CurrentTick() const noexcept
  {
    return m_tick;
  }

  // What _player may see (ADR-002 decision 4): every entity, shot and destruction of the last tick, or under fog of war
  // what the player sees of them and the enemy structures it remembers (ADR-024).
  [[nodiscard]] Snapshot BuildSnapshot(PlayerId _player) const;

  // The entity with this identifier, or nullptr.
  [[nodiscard]] const Entity* FindEntity(EntityId _id) const noexcept;

  // Every entity, in identifier order.
  [[nodiscard]] std::span<const Entity> Entities() const noexcept
  {
    return m_entities;
  }

  // Equal when the state is: the pathfinder's cache of graphs is not state, and nor is what the last tick reported.
  friend bool operator==(const Simulation& _a, const Simulation& _b) noexcept
  {
    return _a.m_tick == _b.m_tick && _a.m_entities == _b.m_entities && _a.m_lastEntityId == _b.m_lastEntityId &&
           _a.m_designs == _b.m_designs && _a.m_lastDesignId == _b.m_lastDesignId && _a.m_players == _b.m_players &&
           _a.m_targetRule == _b.m_targetRule && _a.m_random == _b.m_random && _a.m_pathfinder.Obstacles() == _b.m_pathfinder.Obstacles() &&
           _a.m_basePlayers == _b.m_basePlayers && _a.m_matchOver == _b.m_matchOver && _a.m_winner == _b.m_winner &&
           _a.m_matchEndedTick == _b.m_matchEndedTick && _a.m_fog == _b.m_fog && _a.m_plannedOrders == _b.m_plannedOrders;
  }

  // A group order for more ships than this plans its paths over two ticks rather than one, and the group sets off in the
  // second (ADR-032), so that no tick plans more than about half of a large order's searches.
  static constexpr std::size_t SPLIT_ORDER_SHIPS = 32;

private:
  struct PlayerState
  {
    PlayerId id;
    // In hundredths, so that a Mining Rig's income per tick is whole (ADR-016).
    std::int64_t oreHundredths = 0;
    // Income earned but not yet whole hundredths, in hundredths times ticks a second, so that an upgraded income that is
    // not a whole number of hundredths a tick is still paid in full (ADR-017).
    std::int64_t oreRemainder = 0;
    std::vector<ResearchTopicId> researched;
    // The Shipyards it has finished, which numbers the next.
    std::uint32_t shipyardsFinished = 0;
    // Under fog of war (ADR-024): the enemy entities the player sees, in identifier order; the enemy structures it has
    // seen, as it last saw them; and the enemy shooters it sees because they hit it, until the tick each fades on.
    std::vector<EntityId> seen;
    std::vector<EntityView> remembered;
    std::vector<std::pair<EntityId, std::uint64_t>> revealedUntil;
    // The Ore left in each ore asteroid the player has seen, as it last saw it (Phase 1 design §8).
    std::vector<std::pair<EntityId, std::int64_t>> knownReserves;

    friend bool operator==(const PlayerState&, const PlayerState&) = default;
  };

  // One of a player's ships or structures, as far as it sees.
  struct Observer
  {
    PlanePosition position;
    float sightMeters = 0.0f;
  };

  // What an entity fires: a warship's design's weapon, or a built structure's Defence gun.
  struct Armament
  {
    std::int32_t damageHundredths = 0;
    double fireIntervalSeconds = 0.0;
    float rangeMeters = 0.0f;
    // Zero for a weapon without splash, and for a Defence gun.
    float splashRadiusMeters = 0.0f;
    // For the shot's presentation; no weapon for a Defence gun.
    WeaponId weapon;
  };

  Entity* FindMutableEntity(EntityId _id) noexcept;
  PlayerState* FindPlayer(PlayerId _player) noexcept;
  [[nodiscard]] const PlayerState* FindPlayer(PlayerId _player) const noexcept;
  [[nodiscard]] std::optional<Armament> ArmamentOf(const Entity& _entity) const noexcept;
  [[nodiscard]] std::vector<Observer> ObserversOf(PlayerId _player) const;
  // Whether a circle at _position is within sight of any of _observers.
  [[nodiscard]] static bool InSight(std::span<const Observer> _observers, PlanePosition _position, float _radiusMeters) noexcept;
  // What a snapshot shows of _entity; _detailed adds its queues, which only its owner sees under fog of war.
  [[nodiscard]] EntityView EntityViewOf(const Entity& _entity, bool _detailed) const;
  // At the end of each tick under fog of war: what each player sees and remembers, and attack orders on ships that went
  // out of sight end.
  void UpdateVision();
  // The players whose base was placed and whose Command Station has fallen.
  [[nodiscard]] std::vector<PlayerId> PlayersWithoutStation() const;
  // Under fog of war, the side a shot hits sees its shooter for the tuning data's time.
  void RevealShooter(PlayerId _hitPlayer, EntityId _shooter);
  [[nodiscard]] const StructureTuning* StructureTuningFor(StructureKind _kind) const noexcept;
  CommandResult ValidateShips(PlayerId _player, const std::vector<EntityId>& _ships) const noexcept;
  CommandResult ValidateConstructors(PlayerId _player, const std::vector<EntityId>& _constructors) const noexcept;
  // Moves a Mining Rig's _position onto its asteroid.
  [[nodiscard]] CommandResult CheckPlacement(StructureKind _kind, float _radiusMeters, PlanePosition& _position) const;
  CommandResult Apply(PlayerId _player, const MoveCommand& _move);
  CommandResult Apply(PlayerId _player, const AttackMoveCommand& _attackMove);
  CommandResult Apply(PlayerId _player, const AttackCommand& _attack);
  CommandResult Apply(PlayerId _player, const StopCommand& _stop);
  CommandResult Apply(PlayerId _player, const BuildStructureCommand& _build);
  CommandResult Apply(PlayerId _player, const RepairCommand& _repair);
  CommandResult Apply(PlayerId _player, const QueueShipCommand& _queue);
  CommandResult Apply(PlayerId _player, const StartResearchCommand& _research);
  CommandResult Apply(PlayerId _player, const SaveDesignCommand& _save);
  void OrderWork(const std::vector<EntityId>& _constructors, EntityId _target);
  // The map's obstacles and every structure but the Mining Rigs, which stand on asteroids; and ships whose way a new
  // structure blocks look for another.
  void UpdateObstacles();
  CommandResult OrderMove(PlayerId _player, const std::vector<EntityId>& _ships, PlanePosition _destination, ShipOrder _order);

  // A group order whose ships' paths are being planned (ADR-032): what it orders, where to or whom, the routes its ships
  // may join, and each ship's goal, its slot or the target, and its path once planned. Its ships hold, with no order, until
  // every path is planned and the group sets off.
  struct PlannedOrder
  {
    struct Member
    {
      EntityId ship;
      PlanePosition goal;
      std::optional<std::vector<PlanePosition>> path;

      friend bool operator==(const Member&, const Member&) = default;
    };

    ShipOrder order = ShipOrder::Move;
    // The order's destination, or the target's position when it was given.
    PlanePosition destination;
    EntityId attackTarget;
    float widestRadiusMeters = 0.0f;
    std::vector<GroupRoutes::Route> routes;
    std::vector<Member> members;

    friend bool operator==(const PlannedOrder&, const PlannedOrder&) = default;
  };

  // Plans the paths of _order's ships not planned yet, every _stride-th in its members' order, and keeps the routes they
  // found.
  void PlanPaths(PlannedOrder& _order, std::size_t _stride);
  // Gives every ship of a fully planned _order its order and its path, and the group's pace.
  void SetOff(const PlannedOrder& _order);
  // Plans what is left of the orders planned in the last tick, and sets them off.
  void FinishPlannedOrders();
  // Plans _order now, or its first half now and the rest in the next tick, and sets it off once planned.
  void Plan(PlannedOrder _order);
  [[nodiscard]] EntityId ChooseTarget(const Entity& _ship, float _rangeMeters, TargetRule _rule);
  void Fight();
  void ChaseTargets();
  void ApproachWork();
  void Work();
  void Produce();
  void Research();
  // The player gains the topic's upgrade or unlock at once: its designs take their new stats, and its ships keep the
  // share of their hit points they had (design §8).
  void CompleteResearch(PlayerState& _player, ResearchTopicId _topic);
  // What the player's built Mining Rigs earn each second, in hundredths of an Ore, with its research applied.
  [[nodiscard]] std::int64_t IncomeHundredthsPerSecond(PlayerId _player) const;
  // What one built rig earns each second, in hundredths, with its owner's income factor: its asteroid's rate, or the
  // trickle once the asteroid has run dry (Phase 1 design §8).
  [[nodiscard]] std::int64_t RigIncomeHundredthsPerSecond(const Entity& _rig, double _incomeFactor) const;
  // The Ore left in an ore asteroid as _player knows it: what it holds without fog of war, and what the player last saw
  // under it. None for one never seen, or one that never runs out.
  [[nodiscard]] std::optional<std::int64_t> ReserveKnownTo(PlayerId _player, EntityId _asteroid) const;
  void Mine();
  // Ends the match once a player whose base was placed has no Command Station left.
  void DecideMatch();
  void MoveShips();
  void SeparateShips();
  void KeepShipsClear();

  std::uint32_t m_ticksPerSecond = 0;
  float m_secondsPerTick = 0.0f;
  std::uint32_t m_stallLimitTicks = 0;
  std::uint64_t m_tick = 0;
  // In identifier order, which is creation order, so iterating it is deterministic.
  std::vector<Entity> m_entities;
  std::uint32_t m_lastEntityId = 0;
  // In identifier order, like the entities.
  std::vector<ShipDesign> m_designs;
  std::uint32_t m_lastDesignId = 0;
  std::vector<PlayerState> m_players;
  TargetRule m_targetRule = TargetRule::Nearest;
  Neuron::Random m_random;
  // The map's obstacles and edge, and the structures that block.
  Pathfinder m_pathfinder;
  // Who measures the tick under way, if anyone (Tick); null between ticks, so a copy never holds one, and not state.
  TickObserver* m_observer = nullptr;
  // The group orders whose paths the next tick finishes planning (ADR-032), and whether this tick planned any paths, which
  // leaves graph rebuilding to a quieter tick.
  std::vector<PlannedOrder> m_plannedOrders;
  bool m_plannedThisTick = false;
  std::vector<Obstacle> m_mapObstacles;
  float m_mapHalfSizeMeters = 0.0f;
  // Set by UseTuning; configuration, not state, and shared by copies of the simulation.
  std::shared_ptr<const Tuning> m_tuning;
  // The players whose bases PlaceStartingBases placed, and how the match stands.
  std::vector<PlayerId> m_basePlayers;
  bool m_matchOver = false;
  PlayerId m_winner;
  std::uint64_t m_matchEndedTick = 0;
  bool m_fog = false;
  // What the last tick did, for the snapshots built after it.
  std::vector<ShotView> m_shots;
  std::vector<DestroyedView> m_destroyed;
};
} // namespace Outpost