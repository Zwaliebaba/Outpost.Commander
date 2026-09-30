#pragma once

namespace Outpost
{
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
  // Where a ship's last move order sends it, if anywhere. Movement toward it is task 2.4.
  std::optional<PlanePosition> destination;

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
  explicit Simulation(std::uint64_t _seed) noexcept;

  // Match setup, before the first tick: places the map's ore asteroids and then its fields, in the file's order, as
  // entities with no owner (task 2.3).
  void PlaceMap(const Map& _map);

  // Match setup, before the first tick: places a ship. Production (task 4.3) will use it; until then only tests do.
  EntityId SpawnShip(PlayerId _owner, DesignId _design, PlanePosition _position);

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

  friend bool operator==(const Simulation&, const Simulation&) = default;

private:
  Entity* FindMutableEntity(EntityId _id) noexcept;
  CommandResult ValidateShips(PlayerId _player, const std::vector<EntityId>& _ships) const noexcept;
  CommandResult Apply(PlayerId _player, const MoveCommand& _move);
  CommandResult Apply(PlayerId _player, const StopCommand& _stop);

  std::uint64_t m_tick = 0;
  // In identifier order, which is creation order, so iterating it is deterministic.
  std::vector<Entity> m_entities;
  std::uint32_t m_lastEntityId = 0;
  Neuron::Random m_random;
};
} // namespace Outpost
