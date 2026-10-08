#pragma once

namespace Outpost
{
class ByteWriter;
class ByteReader;

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
  // The last waypoint of the ship's lane through its group's band (ADR-047), while the path still holds it: the waypoints
  // up to it are the ship's place in the band, which it keeps until it is close to each.
  std::optional<PlanePosition> laneEnd;
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
  // A warship's standing order, kept until it is given another (Phase 2 design §9, ADR-059): the group it was given to,
  // which acts as one; for a hold, the sector's identifier and its node; for a patrol, its two ends and whether the ship
  // heads for the second.
  StandingOrder standing = StandingOrder::None;
  std::uint32_t standingGroup = 0;
  std::int32_t holdSector = 0;
  PlanePosition standingFrom;
  PlanePosition standingTo;
  bool standingOutward = true;
  // A ship's retreat (Phase 4 design §10, ADR-075), and while it goes back to be repaired, the structure it goes to. Its
  // standing order waits until it is whole.
  RetreatThreshold retreat = RetreatThreshold::Never;
  bool retreating = false;
  EntityId repairer;

  // A structure's construction, in thousandths of a tick of one Constructor's work: built once the two are equal. Both
  // are zero for a structure placed whole (ADR-016).
  std::int32_t buildWorkDone = 0;
  std::int32_t buildWorkNeeded = 0;
  // A Command Station's further Defence guns, which its level gives, each with its reload as the first's is
  // (Phase 3 design §7).
  std::vector<std::int32_t> extraGunReloadMilliticks;
  // A structure's level, from 1, and the next level's construction while it is upgraded, in thousandths of a tick of
  // the level's time: both are zero while no upgrade is under way (Phase 3 design §4, ADR-064).
  std::int32_t level = 1;
  std::int32_t upgradeWorkDone = 0;
  std::int32_t upgradeWorkNeeded = 0;
  // The Defence gun a built structure carries, if any (design §6).
  StructureWeaponId structureWeapon;
  // A Shipyard's or the Command Station's jobs, front first, and the front job's progress in thousandths of a tick. The
  // work needed is zero until the job starts, which is when it is paid for (design §5).
  std::vector<JobView> queue;
  // A Research Lab's topics, front first, likewise; its front topic's progress is the job's (design §8).
  std::vector<ResearchTopicId> researchQueue;
  std::int32_t jobWorkDone = 0;
  std::int32_t jobWorkNeeded = 0;
  // A Research Lab's second topic, researched beside the first from the level that gives it a second slot (Phase 3 design
  // §6), counted as the first is: zero until it starts.
  std::int32_t secondJobWorkDone = 0;
  std::int32_t secondJobWorkNeeded = 0;
  // A Shipyard's number among its owner's, given when it is first finished, and the ships it has built (Phase 1 design
  // §11).
  std::uint32_t shipyardNumber = 0;
  std::uint32_t shipsBuilt = 0;
  // A derelict's: the Ore it pays, the topic whose time it recovers if any, and its salvage, in thousandths of a tick of one
  // Constructor's work (ADR-074).
  std::int32_t salvageOre = 0;
  ResearchTopicId salvageTopic;
  std::int32_t salvageWorkDone = 0;
  std::int32_t salvageWorkNeeded = 0;

  [[nodiscard]] bool IsBuilt() const noexcept
  {
    return buildWorkDone >= buildWorkNeeded;
  }

  [[nodiscard]] bool IsUpgrading() const noexcept
  {
    return upgradeWorkNeeded > 0;
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
  // A topic the research needs first is neither researched nor ahead of it in the queue (design §8), or a topic a Research
  // Lab's next level needs is not researched (Phase 3 design §6).
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
  // A Relay ordered in a sector that is not adjacent to one the player holds (Phase 2 design §6).
  NotAdjacent,
  // A Mining Rig ordered onto an ore asteroid in a sector the player does not hold (Phase 2 design §4).
  SectorNotHeld,
  // A hold names a point in no sector, or the map has no territory (ADR-059).
  NoSector,
  // An upgrade names something that is not one of the player's own structures (ADR-064).
  NotUpgradable,
  // An upgrade names a structure still being built, or one already being upgraded.
  UnderConstruction,
  AlreadyUpgrading,
  // An upgrade names a structure at its kind's highest level, which for a kind that does not grow is its first.
  TopLevel,
  // A Shipyard's job names a hull above its level, or a Research Lab's a topic of a tier its level has not opened (Phase 3
  // design §5, §6).
  LevelTooLow,
  // A Relay would take the player past the nodes its Command Station's level lets it hold (Phase 3 design §7).
  CapReached,
  // A Relay ordered in a sector whose pirate outpost still has a structure standing (Phase 4 design §8).
  Guarded,
  // A salvage order's target is not a derelict (Phase 4 design §9).
  NotSalvageable,
  // A scheduled order's condition sets fewer than no command points (ADR-080).
  InvalidCondition,
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
  // entities with no owner (task 2.3), and keeps its sectors (ADR-036).
  void PlaceMap(const Map& _map);

  // Whether the match is played for territory: the map has sectors and the simulation has the tuning data's rules for
  // them (Phase 2 design §4, ADR-056). Without, a Relay cannot be built and ore belongs to no sector, as in Phase 1.
  [[nodiscard]] bool HasTerritory() const noexcept
  {
    return m_tuning != nullptr && !m_sectors.empty();
  }

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
  // A structure's full hit points at _level, in hundredths, and a Constructor's speed, with _owner's research (Phase 1
  // design §6). A level's percent adds to research's (Phase 3 design §4).
  [[nodiscard]] std::int32_t StructureHitPoints(PlayerId _owner, const StructureTuning& _tuning, std::int32_t _level = 1) const;
  [[nodiscard]] float ConstructorSpeed(PlayerId _owner) const;

  // Saves a design for _owner and returns its identifier. Match setup saves the starting designs; the designer sends a
  // command (task 5.2).
  DesignId SaveDesign(PlayerId _owner, std::string _name, const DesignComponents& _components, const DesignStats& _stats,
                      RetreatThreshold _retreat = DEFAULT_RETREAT);

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
  // 2.7's load places them. It stands at _level, which tests give it as they give its hit points (Phase 3 design §4).
  EntityId SpawnStructure(PlayerId _owner, StructureKind _kind, PlanePosition _position, float _radiusMeters,
                          std::int32_t _hitPointsHundredths = 0, std::int32_t _armorHundredths = 0, std::int32_t _level = 1);

  // Match setup, after PlaceMap and UseTuning: gives every player its Command Station on its start and the starting
  // Constructors in front of it, facing the map's center (design §6). Throws Neuron::Exception when the base would
  // overlap an obstacle or cross the map's edge. From then on a player left with neither a Command Station nor a
  // finished Shipyard loses the match (Phase 1 design §4).
  void PlaceStartingBases(const Map& _map);

  // A world's rules (Phase 5 design §8), set before PlaceStartingBases and saved with the world: domination is off and no
  // match ends. A player who loses is told so (EventKind::EmpireLost), and _restartSeconds of ticks later, an hour in a
  // world and less in a test, its seat restarts at its start once the start is free: its Command Station and the starting
  // Constructors again, and its Ore raised to the starting Ore when it has less (owner, 2026-10-08). A start is free while
  // no other player holds its home sector or has a structure in it.
  void UseWorldRules(std::uint32_t _restartSeconds = RESTART_SECONDS) noexcept
  {
    m_worldRules = true;
    m_restartSeconds = _restartSeconds;
  }
  [[nodiscard]] bool HasWorldRules() const noexcept
  {
    return m_worldRules;
  }
  static constexpr std::uint32_t RESTART_SECONDS = 3600;
  // Under a world's rules, the tick a lost player's seat restarts at, or has waited from for its start since; none while
  // it stands.
  [[nodiscard]] std::optional<std::uint64_t> RestartTick(PlayerId _player) const noexcept;

  // Match setup, after PlaceMap and UseTuning: the pirates' outposts the map's seed placed (Phase 4 design §8, ADR-073),
  // each on its sector's node, with the tuning data's Defence Platforms and ships of the outpost's size, all owned by
  // PIRATES at the base level. Throws Neuron::Exception when an outpost's size is not in the tuning data, its chase would
  // leave its sector, or a structure of it would overlap an obstacle.
  void PlacePirates(const Map& _map);

  // Match setup, after UseTuning: the derelicts the map's seed placed (Phase 4 design §9, ADR-074), each with the tuning
  // data's salvage work. None without salvage in the tuning data.
  void PlaceDerelicts(const Map& _map);

  // Whether a player has lost its Command Station and its last finished Shipyard, or on a map with territory has run out
  // of tickets, which ends the match (Phase 1 design §4, Phase 2 design §8); the player who still stands won, or nobody
  // when both fell in the same tick. Only a match whose bases were placed can end.
  [[nodiscard]] bool MatchOver() const noexcept
  {
    return m_matchOver;
  }
  [[nodiscard]] PlayerId Winner() const noexcept
  {
    return m_winner;
  }
  [[nodiscard]] MatchEnding Ending() const noexcept
  {
    return m_ending;
  }

  // A player's tickets (ADR-057); zero for a player not added or without territory.
  [[nodiscard]] std::int32_t Tickets(PlayerId _player) const noexcept;

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

  // Equal when the state is: the pathfinder's cache of graphs is not state, and nor is what the last tick reported, nor
  // what each player's research makes of the tuning data, which follows from the topics researched.
  friend bool operator==(const Simulation& _a, const Simulation& _b) noexcept
  {
    return _a.m_tick == _b.m_tick && _a.m_entities == _b.m_entities && _a.m_lastEntityId == _b.m_lastEntityId &&
           _a.m_designs == _b.m_designs && _a.m_lastDesignId == _b.m_lastDesignId && _a.m_players == _b.m_players &&
           _a.m_targetRule == _b.m_targetRule && _a.m_random == _b.m_random && _a.m_pathfinder.Obstacles() == _b.m_pathfinder.Obstacles() &&
           _a.m_basePlayers == _b.m_basePlayers && _a.m_matchOver == _b.m_matchOver && _a.m_winner == _b.m_winner &&
           _a.m_matchEndedTick == _b.m_matchEndedTick && _a.m_ending == _b.m_ending && _a.m_fog == _b.m_fog &&
           _a.m_worldRules == _b.m_worldRules && _a.m_restartSeconds == _b.m_restartSeconds && _a.m_starts == _b.m_starts &&
           _a.m_plannedOrders == _b.m_plannedOrders && _a.m_lastStandingGroup == _b.m_lastStandingGroup && _a.m_sectors == _b.m_sectors &&
           _a.m_outposts == _b.m_outposts && _a.m_scheduledOrders == _b.m_scheduledOrders &&
           _a.m_lastScheduledOrder == _b.m_lastScheduledOrder;
  }

  // A group order for more ships than this plans its paths over two ticks rather than one, and the group sets off in the
  // second (ADR-032), so that no tick plans more than about half of a large order's searches.
  static constexpr std::size_t SPLIT_ORDER_SHIPS = 32;

  // Writes the state a world's save holds, between two ticks (ADR-077): everything that compares equal above, and nothing
  // that follows from it.
  void SaveState(ByteWriter& _writer) const;

  // Replaces the state with what SaveState wrote, and works out again what follows from it: the path graphs, which are
  // built when next needed, and each player's research effects. The simulation is made with the save's seed and tick rate,
  // and given the tuning data the save was made with (UseTuning). Throws Neuron::Exception when the bytes are not a state
  // SaveState could have written.
  void LoadState(ByteReader& _reader);

  // How SaveState lays the state out, as ByteLayout describes it, which a test pins to WORLD_STATE_VERSION (AGENTS.md
  // R18).
  [[nodiscard]] static std::string StateLayout();

private:
  // What a player's research makes of the tuning data: its upgrades, and the components and research topics its snapshots
  // show. Every tick asks for it, so it is worked out again only when the tuning data or the player's research changes
  // (UseTuning, AddPlayer, CompleteResearch). It follows from the topics researched, which are state, so it is not state
  // itself and compares equal whatever it holds.
  struct ResearchEffects
  {
    Upgrades upgrades;
    std::vector<HullView> hulls;
    std::vector<DriveView> drives;
    std::vector<WeaponView> weapons;
    std::vector<ModuleView> modules;
    std::vector<ResearchTopicView> topics;

    friend bool operator==(const ResearchEffects&, const ResearchEffects&) noexcept
    {
      return true;
    }
  };

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
    // The topics a salvaged derelict recovered time of before they started, which take that much less once they do
    // (ADR-074).
    std::vector<ResearchTopicId> recovered;
    // Its tickets on a map with territory (ADR-057).
    std::int32_t tickets = 0;
    // The sectors it holds in which it saw an enemy or pirate warship at the end of the last tick, which tells the next
    // tick's sightings from new ones (ADR-080).
    std::vector<std::int32_t> enemySectors;
    // Under a world's rules, the tick it lost on, while it waits to restart (Phase 5 design §8).
    std::optional<std::uint64_t> lostTick;
    ResearchEffects researchEffects;

    friend bool operator==(const PlayerState&, const PlayerState&) = default;

    // Its fields as a save holds them (ADR-077), which are all but its research effects: they follow from what it has
    // researched.
    template <typename Self>
      requires std::same_as<std::remove_const_t<Self>, PlayerState>
    friend auto Fields(Self& _value)
    {
      [[maybe_unused]] auto& [id, oreHundredths, oreRemainder, researched, shipyardsFinished, seen, remembered, revealedUntil,
                              knownReserves, recovered, tickets, enemySectors, lostTick, researchEffects] = _value;
      return std::tie(id, oreHundredths, oreRemainder, researched, shipyardsFinished, seen, remembered, revealedUntil, knownReserves,
                      recovered, tickets, enemySectors, lostTick);
    }
  };

  // One of a player's ships or structures, as far as it sees.
  struct Observer
  {
    PlanePosition position;
    float sightMeters = 0.0f;
  };

  // One of the map's sectors, and who holds it as of the end of the last tick (ADR-056). Who holds it, whether it is
  // suppressed and whether it is cut off follow from where the structures and warships stand.
  struct Sector
  {
    SectorPlacement placement;
    PlayerId holder;
    // Its holder's Command Station stands on its node.
    bool home = false;
    bool suppressed = false;
    bool cutOff = false;
    // A pirate structure stands in it (ADR-073).
    bool guarded = false;

    friend bool operator==(const Sector&, const Sector&) = default;

    // Its fields as a save holds them (ADR-077).
    template <typename Self>
      requires std::same_as<std::remove_const_t<Self>, Sector>
    friend auto Fields(Self& _value)
    {
      auto& [placement, holder, home, suppressed, cutOff, guarded] = _value;
      return std::tie(placement, holder, home, suppressed, cutOff, guarded);
    }
  };

  // A pirate outpost (ADR-073): its sector and node, the ships that guard it, and the player's ship or structure they are
  // after, if any.
  struct PirateOutpost
  {
    std::int32_t sector = 0;
    PlanePosition node;
    std::vector<EntityId> ships;
    EntityId quarry;
    // The derelict it leaves where its last structure falls, and whether it has (ADR-074).
    DerelictPlacement wreck;
    bool wrecked = false;

    friend bool operator==(const PirateOutpost&, const PirateOutpost&) = default;

    // Its fields as a save holds them (ADR-077).
    template <typename Self>
      requires std::same_as<std::remove_const_t<Self>, PirateOutpost>
    friend auto Fields(Self& _value)
    {
      auto& [sector, node, ships, quarry, wreck, wrecked] = _value;
      return std::tie(sector, node, ships, quarry, wreck, wrecked);
    }
  };

  // A scheduled order the server keeps until it fires (ADR-080): whose it is, as given, and the ships still waiting on it.
  struct ScheduledOrder
  {
    std::uint32_t id = 0;
    PlayerId player;
    std::vector<EntityId> ships;
    ScheduledTrigger trigger;
    ScheduledAction action;
    std::optional<std::int32_t> unlessCommandPoints;

    friend bool operator==(const ScheduledOrder&, const ScheduledOrder&) = default;

    // Its fields as a save holds them (ADR-077).
    template <typename Self>
      requires std::same_as<std::remove_const_t<Self>, ScheduledOrder>
    friend auto Fields(Self& _value)
    {
      auto& [id, player, ships, trigger, action, unlessCommandPoints] = _value;
      return std::tie(id, player, ships, trigger, action, unlessCommandPoints);
    }
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
  // What a player who has researched _researched has of _tuning.
  [[nodiscard]] static ResearchEffects EffectsFrom(const Tuning& _tuning, std::span<const ResearchTopicId> _researched);
  // _player's research effects as last worked out; for a player not added, those of no research.
  [[nodiscard]] const ResearchEffects& EffectsOf(PlayerId _player) const noexcept;
  [[nodiscard]] std::optional<Armament> ArmamentOf(const Entity& _entity) const noexcept;
  [[nodiscard]] std::vector<Observer> ObserversOf(PlayerId _player) const;
  // Whether a circle at _position is within sight of any of _observers.
  [[nodiscard]] static bool InSight(std::span<const Observer> _observers, PlanePosition _position, float _radiusMeters) noexcept;
  // What a snapshot shows of _entity; _detailed adds its queues, which only its owner sees under fog of war.
  [[nodiscard]] EntityView EntityViewOf(const Entity& _entity, bool _detailed) const;
  // At the end of each tick under fog of war: what each player sees and remembers, and attack orders on ships that went
  // out of sight end.
  void UpdateVision();
  // Raises an event for _player, which its snapshot of this tick carries (ADR-080); none for the pirates or a player not
  // added. _position's sector is the event's unless it names one.
  void Raise(PlayerId _player, EventView _event);
  // At the end of each tick: each player's enemy warships seen in a sector it holds that held none it saw the tick before.
  void RaiseSightings();
  // The players whose base was placed and whose Command Station has fallen.
  [[nodiscard]] std::vector<PlayerId> PlayersWithoutStation() const;
  // Under fog of war, the side a shot hits sees its shooter for the tuning data's time.
  void RevealShooter(PlayerId _hitPlayer, EntityId _shooter);
  [[nodiscard]] const StructureTuning* StructureTuningFor(StructureKind _kind) const noexcept;
  CommandResult ValidateShips(PlayerId _player, const std::vector<EntityId>& _ships) const noexcept;
  CommandResult ValidateConstructors(PlayerId _player, const std::vector<EntityId>& _constructors) const noexcept;
  // Moves a Mining Rig's _position onto its asteroid, and a Relay's onto its sector's node.
  [[nodiscard]] CommandResult CheckPlacement(PlayerId _player, StructureKind _kind, float _radiusMeters, PlanePosition& _position) const;
  // The first sector that holds _position, or the one with this identifier; nullptr when there is none.
  [[nodiscard]] const Sector* SectorAt(PlanePosition _position) const noexcept;
  [[nodiscard]] const Sector* SectorById(std::int32_t _id) const noexcept;
  // Works out again who holds each sector, which Relays are suppressed, which sectors are cut off (ADR-056) and which the
  // pirates guard (ADR-073).
  void UpdateTerritory();
  // The nodes _player may hold (Phase 3 design §7): its Command Station's level's, or level 1's without one; zero for no
  // cap. And the nodes it holds or has taken with a Relay under construction.
  [[nodiscard]] std::int32_t NodeCapOf(PlayerId _player) const noexcept;
  [[nodiscard]] std::int32_t NodesTaken(PlayerId _player) const noexcept;
  // The command points _player's warships may take (Phase 4 design §5): its Command Station's level's, or level 1's without
  // one; zero for no cap. And what they take: every warship's, and every warship job a producer of its has started.
  [[nodiscard]] std::int32_t FleetCapOf(PlayerId _player) const noexcept;
  [[nodiscard]] std::int32_t CommandPointsOf(PlayerId _player) const noexcept;
  // What one warship of _hull takes; zero for an unknown hull.
  [[nodiscard]] std::int32_t CommandPointsOfHull(HullId _hull) const noexcept;
  // The share of its income a rig earns where it stands: none outside a sector its owner holds or in a suppressed one,
  // the tuning data's share in one cut off, and all of it otherwise or without territory (Phase 2 design §4–§6).
  [[nodiscard]] double TerritoryShare(const Entity& _rig) const noexcept;
  // Whether _position is in a sector _player holds and that is not suppressed, all of which it sees (ADR-056).
  [[nodiscard]] bool InSectorSight(PlayerId _player, PlanePosition _position) const noexcept;
  CommandResult Apply(PlayerId _player, const MoveCommand& _move);
  CommandResult Apply(PlayerId _player, const AttackMoveCommand& _attackMove);
  CommandResult Apply(PlayerId _player, const AttackCommand& _attack);
  CommandResult Apply(PlayerId _player, const StopCommand& _stop);
  CommandResult Apply(PlayerId _player, const BuildStructureCommand& _build);
  CommandResult Apply(PlayerId _player, const RepairCommand& _repair);
  CommandResult Apply(PlayerId _player, const QueueShipCommand& _queue);
  CommandResult Apply(PlayerId _player, const StartResearchCommand& _research);
  CommandResult Apply(PlayerId _player, const SaveDesignCommand& _save);
  CommandResult Apply(PlayerId _player, const HoldSectorCommand& _hold);
  CommandResult Apply(PlayerId _player, const PatrolCommand& _patrol);
  CommandResult Apply(PlayerId _player, const UpgradeStructureCommand& _upgrade);
  CommandResult Apply(PlayerId _player, const SalvageCommand& _salvage);
  CommandResult Apply(PlayerId _player, const SetRetreatCommand& _retreat);
  CommandResult Apply(PlayerId _player, const ScheduleOrderCommand& _schedule);
  // Applies one order as a player gives it, with what any order of a player's does besides: its ships leave the group they
  // were planning with, end their retreat, and, for an order that gives them something to do, leave their standing and
  // scheduled orders (ADR-032, ADR-059, ADR-075, ADR-080).
  CommandResult ApplyOrder(PlayerId _player, const Order& _order);
  // _ships wait on no scheduled order any more; an order none waits on is dropped.
  void LeaveScheduledOrders(const std::vector<EntityId>& _ships);
  // At the end of each tick, once its events are raised: each scheduled order whose trigger this tick fired gives its ships
  // its action, or holds them, and is dropped; and an order whose ships are all gone is dropped (ADR-080).
  void FireScheduledOrders();
  // Whether _entity is a structure of its player's that repairs its ships near it (ADR-075).
  [[nodiscard]] bool IsRepairer(const Entity& _entity) const noexcept;
  // Sends each of _ships back to the repairer the design's order picks for it, as a group with the others going to the same
  // one, and ends the retreat of a ship with none to go to (ADR-075).
  void Retreat(const std::vector<EntityId>& _ships);
  // Once a second, a retreating ship whose repairer fell goes to another, and one that stopped short sets out again.
  void KeepRetreats();
  // Every repairer repairs the ships near it, and a retreating ship once whole stops retreating.
  void RepairShips();
  // Places a derelict, out of combat and blocking no path (ADR-074).
  void SpawnDerelict(const DerelictPlacement& _derelict);
  // _player salvaged a derelict: it is paid its Ore, and the topic it names recovers the tuning data's share of its time.
  void Salvage(PlayerState& _player, const Entity& _derelict);
  // After a fight, each outpost whose last structure fell leaves its wreck where that structure stood (ADR-074).
  void LeaveWrecks();
  // The warships among _ships, each once; validated by the caller.
  [[nodiscard]] std::vector<EntityId> WarshipsOf(const std::vector<EntityId>& _ships) const;
  // Once a second, every group on a standing order moves as its order says (ADR-059).
  void KeepStandingOrders();
  // Whether _ship is in a group order whose paths are being planned (ADR-032).
  [[nodiscard]] bool IsPlanning(EntityId _ship) const noexcept;
  // Once a second, each outpost's ships go after a player's ship or structure near its node, and back to it (ADR-073).
  void GuardOutposts();
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
      // How far to the left of the band's middle the ship keeps along its route (ADR-047), and where its lane ends once
      // its path is planned.
      float laneMeters = 0.0f;
      std::optional<std::vector<PlanePosition>> path;
      std::optional<PlanePosition> laneEnd;

      friend bool operator==(const Member&, const Member&) = default;

      // Its fields as a save holds them (ADR-077).
      template <typename Self>
        requires std::same_as<std::remove_const_t<Self>, Member>
      friend auto Fields(Self& _value)
      {
        auto& [ship, goal, laneMeters, path, laneEnd] = _value;
        return std::tie(ship, goal, laneMeters, path, laneEnd);
      }
    };

    ShipOrder order = ShipOrder::Move;
    // The order's destination, or the target's position when it was given.
    PlanePosition destination;
    EntityId attackTarget;
    float widestRadiusMeters = 0.0f;
    // Half the width of the band of lanes the group keeps along its routes (ADR-047), or 0 for none.
    float bandHalfWidthMeters = 0.0f;
    std::vector<GroupRoutes::Route> routes;
    std::vector<Member> members;

    friend bool operator==(const PlannedOrder&, const PlannedOrder&) = default;

    // Its fields as a save holds them (ADR-077).
    template <typename Self>
      requires std::same_as<std::remove_const_t<Self>, PlannedOrder>
    friend auto Fields(Self& _value)
    {
      auto& [order, destination, attackTarget, widestRadiusMeters, bandHalfWidthMeters, routes, members] = _value;
      return std::tie(order, destination, attackTarget, widestRadiusMeters, bandHalfWidthMeters, routes, members);
    }
  };

  // What a save holds that lives inside a member which is not a record (ADR-077): the PRNG's state, and the pathfinder's
  // obstacles, from which it builds its graphs again.
  struct SavedParts
  {
    std::array<std::uint64_t, 4> random{};
    std::vector<Obstacle> obstacles;
  };

  // The state a save holds, as one tuple of references into _simulation and _parts, which SaveState writes and LoadState
  // reads. It names every member of the simulation in one structured binding, so a member added and not considered here
  // does not compile (ADR-077).
  template <typename Self, typename Parts> static auto SavedFields(Self& _simulation, Parts& _parts);

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
  void BuildLevels();
  void Produce();
  void Research();
  // The player gains the topic's upgrade or unlock at once: its designs take their new stats, and its ships keep the
  // share of their hit points they had (design §8).
  void CompleteResearch(PlayerState& _player, ResearchTopicId _topic);
  // A structure's full hit points at _level with this factor from research, as StructureHitPoints gives them.
  [[nodiscard]] std::int32_t StructureHitPointsAt(const StructureTuning& _tuning, double _researchFactor, std::int32_t _level) const;
  // _entity's full hit points become _after, and it keeps the share of them it had (owner, 2026-10-01).
  static void RescaleHitPoints(Entity& _entity, std::int64_t _after) noexcept;
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
  // Under a world's rules: tells a player that it lost, and restarts its seat on the hour once its start is free.
  void RestartLostPlayers(const std::vector<PlayerId>& _standing);
  [[nodiscard]] bool IsStartFree(PlayerId _player, PlanePosition _start) const noexcept;
  // A player's Command Station on _start, and the starting Constructors in front of it, facing the map's center.
  void PlaceStartingBase(PlayerId _owner, PlanePosition _start);
  // On a map with territory, every drain interval: each player that holds fewer nodes than another loses tickets, and a
  // player out of them ends the match (Phase 2 design §8, ADR-057).
  void Dominate();
  // The match ends now: _standing are the players still in it, of whom the winner is the one, if one.
  void EndMatch(const std::vector<PlayerId>& _standing, MatchEnding _ending);
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
  // The last group a standing order was given to (ADR-059).
  std::uint32_t m_lastStandingGroup = 0;
  // The scheduled orders not yet fired, in the order they were given, and the last one's identifier (ADR-080).
  std::vector<ScheduledOrder> m_scheduledOrders;
  std::uint32_t m_lastScheduledOrder = 0;
  std::vector<Obstacle> m_mapObstacles;
  float m_mapHalfSizeMeters = 0.0f;
  // The map's sectors, in its order; none on a map without them.
  std::vector<Sector> m_sectors;
  // The pirates' outposts, in the map's order (ADR-073).
  std::vector<PirateOutpost> m_outposts;
  // Set by UseTuning; configuration, not state, and shared by copies of the simulation.
  std::shared_ptr<const Tuning> m_tuning;
  // What the tuning data gives a player who has researched nothing, as a player not added has; none before UseTuning.
  ResearchEffects m_unresearched;
  // The players whose bases PlaceStartingBases placed, and how the match stands.
  std::vector<PlayerId> m_basePlayers;
  bool m_matchOver = false;
  PlayerId m_winner;
  std::uint64_t m_matchEndedTick = 0;
  MatchEnding m_ending = MatchEnding::LostProduction;
  bool m_fog = false;
  // A world's rules, and each base player's start, where its seat restarts (Phase 5 design §8).
  bool m_worldRules = false;
  std::uint32_t m_restartSeconds = RESTART_SECONDS;
  std::vector<std::pair<PlayerId, PlanePosition>> m_starts;
  // What the last tick did, for the snapshots built after it: its shots, its destructions, and each player's events.
  std::vector<ShotView> m_shots;
  std::vector<DestroyedView> m_destroyed;
  std::vector<std::pair<PlayerId, EventView>> m_events;
};
} // namespace Outpost