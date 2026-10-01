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
  Attack
};

// How a ship picks what to fire at. Nearest is the game's rule (design §7). The other two are the Q2 check's two
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
  StructureKind structure = StructureKind::CommandStation;
  PlanePosition position;
  float headingRadians = 0.0f;
  float radiusMeters = 0.0f;
  // Meaningful for an ore asteroid only.
  OreYield oreYield = OreYield::Home;
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

  // Match setup: a player and its Ore. A snapshot for a player not added here shows no Ore.
  void AddPlayer(PlayerId _player, std::int32_t _ore);

  // Saves a design for _owner and returns its identifier. Match setup saves the starting designs; the designer will
  // send a command (task 5.2).
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

  // Match setup: places a structure. Task 4.2 builds structures and makes them block movement. Without hit points it is
  // out of combat, as task 2.7's load places them.
  EntityId SpawnStructure(PlayerId _owner, StructureKind _kind, PlanePosition _position, float _radiusMeters,
                          std::int32_t _hitPointsHundredths = 0, std::int32_t _armorHundredths = 0);

  // Match setup, after PlaceMap: gives every player the map's starting fleet, in a grid centered on its start and facing
  // the map's center (task 2.5). Each ship is of the player's saved design of its components, which is saved first if
  // the player has none. Throws Neuron::Exception when a component names nothing in _tuning, or when a ship would
  // overlap an obstacle or cross the map's edge.
  void PlaceStartingFleets(const Map& _map, const Tuning& _tuning);

  // How every ship picks its target. Only the Q2 check's headless battles change it (task 3.4).
  void SetTargetRule(TargetRule _rule) noexcept
  {
    m_targetRule = _rule;
  }

  // Runs one tick: applies _commands in order at its start, then advances the world. Returns one result per command.
  [[nodiscard]] std::vector<CommandResult> Tick(const std::vector<Command>& _commands);

  // The ticks run so far. A snapshot built now is stamped with it.
  [[nodiscard]] std::uint64_t CurrentTick() const noexcept
  {
    return m_tick;
  }

  // What _player may see. In the MVP that is every entity (ADR-002 decision 4), and every shot and destruction of the
  // last tick.
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
           _a.m_targetRule == _b.m_targetRule && _a.m_random == _b.m_random && _a.m_pathfinder.Obstacles() == _b.m_pathfinder.Obstacles();
  }

private:
  struct PlayerState
  {
    PlayerId id;
    std::int32_t ore = 0;

    friend bool operator==(const PlayerState&, const PlayerState&) = default;
  };

  Entity* FindMutableEntity(EntityId _id) noexcept;
  CommandResult ValidateShips(PlayerId _player, const std::vector<EntityId>& _ships) const noexcept;
  CommandResult Apply(PlayerId _player, const MoveCommand& _move);
  CommandResult Apply(PlayerId _player, const AttackMoveCommand& _attackMove);
  CommandResult Apply(PlayerId _player, const AttackCommand& _attack);
  CommandResult Apply(PlayerId _player, const StopCommand& _stop);
  CommandResult OrderMove(PlayerId _player, const std::vector<EntityId>& _ships, PlanePosition _destination, ShipOrder _order);
  [[nodiscard]] EntityId ChooseTarget(const Entity& _ship, float _rangeMeters, TargetRule _rule);
  void Fight();
  void ChaseTargets();
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
  // The map's obstacles and edge.
  Pathfinder m_pathfinder;
  // What the last tick did, for the snapshots built after it.
  std::vector<ShotView> m_shots;
  std::vector<DestroyedView> m_destroyed;
};
} // namespace Outpost
