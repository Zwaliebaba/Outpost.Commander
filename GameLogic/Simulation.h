#pragma once

namespace Outpost
{
// How a ship moves: from its hull and drive (design §7, §12), set when it is spawned.
struct ShipMovement
{
  float speedMetersPerSecond = 0.0f;
  float turnRateRadiansPerSecond = 0.0f;
  // Its footprint: the circle it keeps clear of obstacles and of other ships.
  float radiusMeters = 0.0f;

  friend constexpr bool operator==(const ShipMovement&, const ShipMovement&) = default;
};

// A ship of this hull and drive: the hull's speed and turn rate times the drive's factors, and the hull's footprint.
// Throws Neuron::Exception when either identifier names nothing in _tuning.
[[nodiscard]] ShipMovement MovementFor(const Tuning& _tuning, HullId _hull, DriveId _drive);

// One entity as the server holds it. What a player may see of it is the EntityView a snapshot carries.
struct Entity
{
  EntityId id;
  EntityKind kind = EntityKind::Ship;
  PlayerId owner;
  DesignId design;
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
  // The order is valid protocol, but the task that gives it meaning has not been built yet.
  NotYetSupported
};

// The authoritative game state and the rules that change it (ADR-002). It runs one tick at a time and knows nothing of
// wall time, transports or clients: the same seed and the same commands at the same ticks give the same state, which is
// what a replay relies on (ADR-009).
class Simulation
{
public:
  // _ticksPerSecond is the rate the simulation is run at, which sets how far a ship moves in one tick.
  Simulation(std::uint64_t _seed, std::uint32_t _ticksPerSecond);

  // Match setup, before the first tick: places the map's ore asteroids and then its fields, in the file's order, as
  // entities with no owner (task 2.3).
  void PlaceMap(const Map& _map);

  // Match setup: builds the pathfinding graph for ships of this footprint radius ahead of their first order.
  void PreparePathfinding(float _radiusMeters) const
  {
    m_pathfinder.Prepare(_radiusMeters);
  }

  // Match setup, before the first tick: places a ship. Production (task 4.3) will use it; until then only tests do.
  EntityId SpawnShip(PlayerId _owner, DesignId _design, const ShipMovement& _movement, PlanePosition _position);

  // Runs one tick: applies _commands in order at its start, then advances the world. Returns one result per command.
  [[nodiscard]] std::vector<CommandResult> Tick(const std::vector<Command>& _commands);

  // The ticks run so far. A snapshot built now is stamped with it.
  [[nodiscard]] std::uint64_t CurrentTick() const noexcept
  {
    return m_tick;
  }

  // What _player may see. In the MVP that is every entity (ADR-002 decision 4).
  [[nodiscard]] Snapshot BuildSnapshot(PlayerId _player) const;

  // The entity with this identifier, or nullptr.
  [[nodiscard]] const Entity* FindEntity(EntityId _id) const noexcept;

  // Equal when the state is: the pathfinder's cache of graphs is not state.
  friend bool operator==(const Simulation& _a, const Simulation& _b) noexcept
  {
    return _a.m_tick == _b.m_tick && _a.m_entities == _b.m_entities && _a.m_lastEntityId == _b.m_lastEntityId &&
           _a.m_random == _b.m_random && _a.m_pathfinder.Obstacles() == _b.m_pathfinder.Obstacles();
  }

private:
  Entity* FindMutableEntity(EntityId _id) noexcept;
  CommandResult ValidateShips(PlayerId _player, const std::vector<EntityId>& _ships) const noexcept;
  CommandResult Apply(PlayerId _player, const MoveCommand& _move);
  CommandResult Apply(PlayerId _player, const StopCommand& _stop);
  void MoveShips();
  void SeparateShips();
  void KeepShipsClear();

  float m_secondsPerTick = 0.0f;
  std::uint32_t m_stallLimitTicks = 0;
  std::uint64_t m_tick = 0;
  // In identifier order, which is creation order, so iterating it is deterministic.
  std::vector<Entity> m_entities;
  std::uint32_t m_lastEntityId = 0;
  Neuron::Random m_random;
  // The map's obstacles and edge.
  Pathfinder m_pathfinder;
};
} // namespace Outpost
