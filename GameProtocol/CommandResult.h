#pragma once

namespace Outpost
{
// What became of one command. The server tells the player who gave it of any but Applied, as a CommandRefused event
// in its snapshot (ADR-094).
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
  // The player has too little Ore to start the job (ADR-016 decision 4).
  NotEnoughOre,
  // The player already has the one Research Lab it may have (ADR-016 decision 4).
  LimitReached,
  // A repair's target is not one of the player's own ships or structures under construction or damaged.
  NotRepairable,
  // A queue's producer is not one of the player's built Shipyards or its Command Station.
  NotAProducer,
  // A Shipyard's queue names no design of the player's.
  UnknownDesign,
  // The queue already holds five jobs (ADR-016 decision 6).
  QueueFull,
  // A research order's lab is not the player's built Research Lab.
  NotALab,
  // A research order's topic, or a design's hull, drive or weapon, is not in the tuning data.
  UnknownTopic,
  UnknownComponent,
  // The topic is researched already, or already in the lab's queue.
  AlreadyResearched,
  // A topic the research needs first is neither researched nor ahead of it in the queue (ADR-017 decision 1), or a topic
  // a Research Lab's next level needs is not researched (ADR-064 decision 10).
  PrerequisiteMissing,
  // A design's name is empty, too long, or holds a character the HUD cannot show.
  InvalidName,
  // A new design uses a component the player has not unlocked (ADR-017 decision 4).
  ComponentLocked,
  // The player already has a design of these components.
  DuplicateDesign,
  // A saved design's components never change; a design of other components is saved as a new one (ADR-017).
  ComponentsFixed,
  // An attack's target is an enemy the player neither sees nor, for a structure, remembers (ADR-024).
  NotVisible,
  // A Relay ordered in a sector that is not adjacent to one the player holds (ADR-056 decision 4).
  NotAdjacent,
  // A Mining Rig's asteroid, or a structure NeedsHeldSector names, in a sector the player does not hold (ADR-056 decision 5).
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
  // A Shipyard's job names a hull above its level, or a Research Lab's a topic of a tier its level has not opened
  // (ADR-064 decisions 9 and 10).
  LevelTooLow,
  // A Relay would take the player past the nodes its Command Station's level lets it hold (ADR-064 decision 11).
  CapReached,
  // A Relay ordered in a sector whose pirate outpost still has a structure standing (ADR-056 decision 4).
  Guarded,
  // A salvage order's target is not a derelict (ADR-074 decision 4).
  NotSalvageable,
  // A scheduled order's condition sets fewer than no command points (ADR-080).
  InvalidCondition,
  // A cancel names no scheduled order of the player's: it fired, its ships left it, or it was never there (ADR-080).
  UnknownOrder,
  // The order is valid protocol, but the task that gives it meaning has not been built yet.
  NotYetSupported
};
} // namespace Outpost
