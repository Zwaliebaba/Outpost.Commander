#pragma once

// The fields of every type that crosses the wire, which ByteWriter and ByteReader find by argument-dependent lookup, and
// each of their enumerations' last value. WireFormat.cpp writes messages from them, and a world's save writes the
// protocol's types it holds from them too (ADR-060, ADR-077). Included where it is needed, not by GameProtocol.h.

#include <concepts>
#include <tuple>
#include <type_traits>

namespace Outpost
{
// Each type's fields, in the order it declares them, which is the order they cross the wire in, and the order they are
// saved in where a world's state holds them (ADR-077). A structured binding has to name every field, so a field added to
// one of these types and not here does not compile (ADR-060).
template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, PlanePosition>
auto Fields(Self& _value)
{
  auto& [xMeters, zMeters] = _value;
  return std::tie(xMeters, zMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, MoveCommand>
auto Fields(Self& _value)
{
  auto& [ships, destination] = _value;
  return std::tie(ships, destination);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, AttackCommand>
auto Fields(Self& _value)
{
  auto& [ships, target] = _value;
  return std::tie(ships, target);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, AttackMoveCommand>
auto Fields(Self& _value)
{
  auto& [ships, destination] = _value;
  return std::tie(ships, destination);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StopCommand>
auto Fields(Self& _value)
{
  auto& [ships] = _value;
  return std::tie(ships);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, BuildStructureCommand>
auto Fields(Self& _value)
{
  auto& [constructors, structure, position] = _value;
  return std::tie(constructors, structure, position);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, RepairCommand>
auto Fields(Self& _value)
{
  auto& [constructors, target] = _value;
  return std::tie(constructors, target);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, QueueShipCommand>
auto Fields(Self& _value)
{
  auto& [producer, design] = _value;
  return std::tie(producer, design);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StartResearchCommand>
auto Fields(Self& _value)
{
  auto& [lab, topic] = _value;
  return std::tie(lab, topic);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SaveDesignCommand>
auto Fields(Self& _value)
{
  auto& [design, nameUtf8, hull, drive, weapon, module, retreat] = _value;
  return std::tie(design, nameUtf8, hull, drive, weapon, module, retreat);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HoldSectorCommand>
auto Fields(Self& _value)
{
  auto& [ships, position] = _value;
  return std::tie(ships, position);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, PatrolCommand>
auto Fields(Self& _value)
{
  auto& [ships, destination] = _value;
  return std::tie(ships, destination);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, UpgradeStructureCommand>
auto Fields(Self& _value)
{
  auto& [structure] = _value;
  return std::tie(structure);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SalvageCommand>
auto Fields(Self& _value)
{
  auto& [constructors, derelict] = _value;
  return std::tie(constructors, derelict);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SetRetreatCommand>
auto Fields(Self& _value)
{
  auto& [ships, retreat] = _value;
  return std::tie(ships, retreat);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, Command>
auto Fields(Self& _value)
{
  auto& [player, order] = _value;
  return std::tie(player, order);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, JobView>
auto Fields(Self& _value)
{
  auto& [role, design] = _value;
  return std::tie(role, design);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, EntityView>
auto Fields(Self& _value)
{
  auto& [id, kind, owner, design, hull, drive, weapon, module, role, structure, position, headingRadians, radiusMeters, hitPointsHundredths,
         maxHitPointsHundredths, builtPermille, level, upgradePermille, shipyardNumber, shipsBuilt, queue, research, jobPermille,
         secondJobPermille, remembered, lastSeenTick, sightMeters, order, standing, retreat, retreating, oreReserveHundredths, salvageOre,
         salvageTopic, salvagePermille] = _value;
  return std::tie(id, kind, owner, design, hull, drive, weapon, module, role, structure, position, headingRadians, radiusMeters,
                  hitPointsHundredths, maxHitPointsHundredths, builtPermille, level, upgradePermille, shipyardNumber, shipsBuilt, queue,
                  research, jobPermille, secondJobPermille, remembered, lastSeenTick, sightMeters, order, standing, retreat, retreating,
                  oreReserveHundredths, salvageOre, salvageTopic, salvagePermille);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HullView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, hitPointsHundredths, armorHundredths, speedMetersPerSecond, turnRateDegreesPerSecond, footprintRadiusMeters, cost,
         buildSeconds, available, shipyardLevel, commandPoints] = _value;
  return std::tie(id, nameUtf8, hitPointsHundredths, armorHundredths, speedMetersPerSecond, turnRateDegreesPerSecond, footprintRadiusMeters,
                  cost, buildSeconds, available, shipyardLevel, commandPoints);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DriveView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, speedFactor, hitPointsFactor, turnRateFactor, cost, available] = _value;
  return std::tie(id, nameUtf8, speedFactor, hitPointsFactor, turnRateFactor, cost, available);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, WeaponView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, damageHundredths, fireIntervalSeconds, rangeMeters, splashRadiusMeters, cost, available] = _value;
  return std::tie(id, nameUtf8, damageHundredths, fireIntervalSeconds, rangeMeters, splashRadiusMeters, cost, available);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ModuleView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, sightMeters, speedFactor, cost, available] = _value;
  return std::tie(id, nameUtf8, sightMeters, speedFactor, cost, available);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ResearchTopicView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, effectUtf8, cost, researchSeconds, prerequisites, researched, unlocksHull, unlocksDrive, unlocksWeapon, tier,
         recovered] = _value;
  return std::tie(id, nameUtf8, effectUtf8, cost, researchSeconds, prerequisites, researched, unlocksHull, unlocksDrive, unlocksWeapon,
                  tier, recovered);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DesignView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, hull, drive, weapon, module, cost, retreat] = _value;
  return std::tie(id, nameUtf8, hull, drive, weapon, module, cost, retreat);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StructureLevelView>
auto Fields(Self& _value)
{
  auto& [cost, buildSeconds, maxHitPointsHundredths, opensTier, researchSlots, prerequisites, nodes, guns, commandPoints] = _value;
  return std::tie(cost, buildSeconds, maxHitPointsHundredths, opensTier, researchSlots, prerequisites, nodes, guns, commandPoints);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StructureTypeView>
auto Fields(Self& _value)
{
  auto& [structure, nameUtf8, radiusMeters, buildable, cost, levels] = _value;
  return std::tie(structure, nameUtf8, radiusMeters, buildable, cost, levels);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SectorView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, minXMeters, maxXMeters, minZMeters, maxZMeters, node, adjacent, holder, suppressed, cutOff, guarded] = _value;
  return std::tie(id, nameUtf8, minXMeters, maxXMeters, minZMeters, maxZMeters, node, adjacent, holder, suppressed, cutOff, guarded);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, TicketsView>
auto Fields(Self& _value)
{
  auto& [player, tickets] = _value;
  return std::tie(player, tickets);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ShotView>
auto Fields(Self& _value)
{
  auto& [shooter, target, weapon, from, to, splashRadiusMeters, gun] = _value;
  return std::tie(shooter, target, weapon, from, to, splashRadiusMeters, gun);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DestroyedView>
auto Fields(Self& _value)
{
  auto& [id, kind, structure, owner, hull, position, headingRadians, radiusMeters] = _value;
  return std::tie(id, kind, structure, owner, hull, position, headingRadians, radiusMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, Snapshot>
auto Fields(Self& _value)
{
  auto& [tick, player, entities, shots, destroyed, ore, oreIncomeHundredthsPerSecond, designs, mapSizeMeters, structureTypes,
         constructorCost, constructorBuildSeconds, hulls, drives, weapons, modules, research, shipyardBuildSpeedFactor, researchTier,
         nodeCap, commandPoints, fleetCap, matchOver, winner, matchEndedTick, ending, fogOfWar, sectors, tickets, startingTickets] = _value;
  return std::tie(tick, player, entities, shots, destroyed, ore, oreIncomeHundredthsPerSecond, designs, mapSizeMeters, structureTypes,
                  constructorCost, constructorBuildSeconds, hulls, drives, weapons, modules, research, shipyardBuildSpeedFactor,
                  researchTier, nodeCap, commandPoints, fleetCap, matchOver, winner, matchEndedTick, ending, fogOfWar, sectors, tickets,
                  startingTickets);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HelloMessage>
auto Fields(Self& _value)
{
  auto& [protocolVersion, player, token] = _value;
  return std::tie(protocolVersion, player, token);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, WelcomeMessage>
auto Fields(Self& _value)
{
  auto& [player] = _value;
  return std::tie(player);
}

// Each enumeration's last enumerator: a value past it is not one the enumeration has. An enumerator added after it has to
// be named here too, or a message or a save that carries it is refused.
constexpr EntityKind LastOf(EntityKind) noexcept
{
  return EntityKind::Derelict;
}

constexpr ShipRole LastOf(ShipRole) noexcept
{
  return ShipRole::Constructor;
}

constexpr StructureKind LastOf(StructureKind) noexcept
{
  return StructureKind::RepairBay;
}

constexpr RetreatThreshold LastOf(RetreatThreshold) noexcept
{
  return RetreatThreshold::Quarter;
}

constexpr ShipOrder LastOf(ShipOrder) noexcept
{
  return ShipOrder::Work;
}

constexpr StandingOrder LastOf(StandingOrder) noexcept
{
  return StandingOrder::Patrol;
}

constexpr MatchEnding LastOf(MatchEnding) noexcept
{
  return MatchEnding::Domination;
}
} // namespace Outpost
