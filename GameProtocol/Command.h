#pragma once

namespace Outpost
{
// The orders of design §9. A client never changes the simulation; it sends one of these, and the server validates it
// against ownership, cost and legality and applies it at the start of its next tick, or rejects it (ADR-002).

// Ships head for a point, pathing around obstacles, in formation at the slowest ship's pace (design §9).
struct MoveCommand
{
  std::vector<EntityId> ships;
  PlanePosition destination;
};

// Ships close on one enemy ship or structure and fire at it until it dies.
struct AttackCommand
{
  std::vector<EntityId> ships;
  EntityId target;
};

// Ships head for a point, and a ship that meets an enemy stops at its own weapon range and fires (design §7).
struct AttackMoveCommand
{
  std::vector<EntityId> ships;
  PlanePosition destination;
};

// Ships drop their orders and hold where they are.
struct StopCommand
{
  std::vector<EntityId> ships;
};

// Constructors build a structure at a point. A Mining Rig snaps to the ore asteroid there (design §6).
struct BuildStructureCommand
{
  std::vector<EntityId> constructors;
  StructureKind structure = StructureKind::CommandStation;
  PlanePosition position;
};

// Constructors work on one of their player's own ships or structures: they finish it while it is being built, and repair
// it once it is damaged (design §6, §9: a right-click on a damaged friendly). Repair costs nothing.
struct RepairCommand
{
  std::vector<EntityId> constructors;
  EntityId target;
};

// A Shipyard queues a ship of a saved design, or the Command Station queues a Constructor, which ignores the design
// (design §6). Each holds up to five jobs.
struct QueueShipCommand
{
  EntityId producer;
  DesignId design;
};

// The player's Research Lab queues a topic (design §8). It researches one topic at a time and holds up to QUEUE_LIMIT,
// each paid for when it starts. A topic may follow its prerequisite in the queue.
struct StartResearchCommand
{
  EntityId lab;
  ResearchTopicId topic;
};

// The longest a design's name may be, in characters: room for the longest name design §7 writes, "Medium+Fusion+Missile Rack".
inline constexpr size_t DESIGN_NAME_LIMIT = 32;

// Whether a design may have this name: one to DESIGN_NAME_LIMIT printable ASCII characters, the ones the HUD's font holds
// (ADR-015), and not only spaces. The designer and the server both check it.
[[nodiscard]] constexpr bool IsValidDesignName(std::string_view _name) noexcept
{
  if (_name.empty() || _name.size() > DESIGN_NAME_LIMIT)
    return false;
  bool visible = false;
  for (const char character : _name)
  {
    if (character < ' ' || character > '~')
      return false;
    visible = visible || character != ' ';
  }
  return visible;
}

// Saves a design from the Shipyard panel's designer (design §7, §9). An invalid design identifier saves a new design of
// components the player has, and the server assigns its identifier. A valid one renames that design and sets its retreat:
// a saved design's components never change, so that the ships already built of it stay what they are (ADR-017).
struct SaveDesignCommand
{
  DesignId design;
  std::string nameUtf8;
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  // None for a design without a module (Phase 2 design §10).
  ModuleId module;
  // What every ship built to it from now on starts with (Phase 4 design §10, ADR-075).
  RetreatThreshold retreat = DEFAULT_RETREAT;
};

// A standing order (Phase 2 design §9, ADR-059): the warships hold the sector that holds the point, answering any enemy
// ship they see in it and going back to its node once none is left, until given another order.
struct HoldSectorCommand
{
  std::vector<EntityId> ships;
  PlanePosition position;
};

// A standing order (Phase 2 design §9, ADR-059): the warships attack-move to the point, then back to where they were
// ordered from, and so on, until given another order.
struct PatrolCommand
{
  std::vector<EntityId> ships;
  PlanePosition destination;
};

// Upgrades one of the player's own finished structures by one level (Phase 3 design §4, ADR-064). Its Ore is paid when
// the order is given, and the level then builds itself over its time, with no Constructor, while the structure keeps
// working (owner, 2026-10-04).
struct UpgradeStructureCommand
{
  EntityId structure;
};

// Constructors salvage a derelict they reach, as they build a site (Phase 4 design §9, ADR-074); its Ore, and the topic's
// time it recovers, go to the player whose Constructors finish it.
struct SalvageCommand
{
  std::vector<EntityId> constructors;
  EntityId derelict;
};

// Sets the ships' retreat (Phase 4 design §10, ADR-075). It gives them no order: a ship retreating goes on, unless it is
// set never to retreat.
struct SetRetreatCommand
{
  std::vector<EntityId> ships;
  RetreatThreshold retreat = DEFAULT_RETREAT;
};

// What fires a scheduled order (Phase 5 design §7, ADR-080): a time of day, or one of the player's events in the sector the
// trigger watches.
enum class ScheduledTriggerKind : std::uint8_t
{
  TimeOfDay,
  // Enemy or pirate warships seen entering the sector, which the player holds.
  EnemyInSector,
  // A Relay of the player's in the sector suppressed, or under attack.
  RelayThreatened,
  // A Mining Rig of the player's in the sector lost.
  RigLost
};

struct ScheduledTrigger
{
  ScheduledTriggerKind kind = ScheduledTriggerKind::TimeOfDay;
  // The sector an event trigger watches.
  std::int32_t sector = 0;
  // A time of day's moment, in seconds since 1970 UTC, as the player's clock picked it (owner, 2026-10-08); and the tick the
  // server's host makes of it as the command arrives, which is what the simulation fires on and the log keeps (ADR-009).
  std::int64_t utcSeconds = 0;
  std::uint64_t tick = 0;

  friend bool operator==(const ScheduledTrigger&, const ScheduledTrigger&) = default;
};

// What a scheduled order does when it fires (Phase 5 design §7): the orders of the same names, its ships given them as the
// player gives them; and a Mining Rig built on the asteroid at its point by its Constructors.
enum class ScheduledActionKind : std::uint8_t
{
  Move,
  AttackMove,
  Attack,
  HoldSector,
  Patrol,
  BuildRig
};

struct ScheduledAction
{
  ScheduledActionKind kind = ScheduledActionKind::Move;
  // Where it goes, the sector it holds, the far end of its patrol, or the asteroid its rig stands on.
  PlanePosition position;
  // An attack's target.
  EntityId target;

  friend bool operator==(const ScheduledAction&, const ScheduledAction&) = default;
};

// A scheduled order (Phase 5 design §7, ADR-080): its ships do its action once, when its trigger fires, unless its condition
// says the enemy is too strong, and then they hold the sector they are in. Warships take part, or for a rig Constructors. The
// server keeps it as it keeps a standing order, and any other order to one of its ships takes that ship out of it.
struct ScheduleOrderCommand
{
  std::vector<EntityId> ships;
  ScheduledTrigger trigger;
  ScheduledAction action;
  // The condition, if any: the most command points the enemy warships the player sees in the action's sector may take for
  // the action to go ahead.
  std::optional<std::int32_t> unlessCommandPoints;
};

using Order = std::variant<MoveCommand, AttackCommand, AttackMoveCommand, StopCommand, BuildStructureCommand, RepairCommand,
                           QueueShipCommand, StartResearchCommand, SaveDesignCommand, HoldSectorCommand, PatrolCommand,
                           UpgradeStructureCommand, SalvageCommand, SetRetreatCommand, ScheduleOrderCommand>;

// One order from one player. The player is set by the server's end of the transport, from the connection the command
// arrived on; what a client puts there is never trusted (ADR-002).
struct Command
{
  PlayerId player;
  Order order;
};
} // namespace Outpost