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
// components the player has, and the server assigns its identifier. A valid one renames that design: a saved design's
// components never change, so that the ships already built of it stay what they are (ADR-017).
struct SaveDesignCommand
{
  DesignId design;
  std::string nameUtf8;
  HullId hull;
  DriveId drive;
  WeaponId weapon;
  // None for a design without a module (Phase 2 design §10).
  ModuleId module;
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

using Order = std::variant<MoveCommand, AttackCommand, AttackMoveCommand, StopCommand, BuildStructureCommand, RepairCommand,
                           QueueShipCommand, StartResearchCommand, SaveDesignCommand, HoldSectorCommand, PatrolCommand>;

// One order from one player. The player is set by the server's end of the transport, from the connection the command
// arrived on; what a client puts there is never trusted (ADR-002).
struct Command
{
  PlayerId player;
  Order order;
};
} // namespace Outpost