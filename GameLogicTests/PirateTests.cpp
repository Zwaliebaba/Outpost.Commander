#include "pch.h"
#include "TerritoryMatch.h"

#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr std::uint32_t TICKS_PER_SECOND = TerritoryMatch::TICKS_PER_SECOND;
// Blue's home, the repository map's start in the southwest (ADR-036).
constexpr Outpost::PlanePosition BLUE_HOME{.xMeters = -4000.0f, .zMeters = -4000.0f};
// Tuning.json's guard and chase (Phase 4 design §8).
constexpr float GUARD_METERS = 600.0f;
constexpr float CHASE_METERS = 900.0f;
// A group back at its node stands about this close to it (Simulation's HOLD_RETURN_METERS).
constexpr float HOME_METERS = 200.0f;

// The first outpost of this size the match's seed placed.
const Outpost::OutpostPlacement& OutpostOf(const TerritoryMatch& _match, std::string_view _size)
{
  const auto found = std::ranges::find(_match.Outposts(), _size, &Outpost::OutpostPlacement::outpost);
  Assert::IsTrue(found != _match.Outposts().end());
  return *found;
}

// The pirates' ships and structures in the sector.
std::vector<const Outpost::Entity*> PiratesIn(TerritoryMatch& _match, std::int32_t _sector)
{
  std::vector<const Outpost::Entity*> pirates;
  for (const Outpost::Entity& entity : _match.World().Entities())
  {
    if (entity.owner == Outpost::PIRATES && _match.Placement(_sector).Contains(entity.position))
      pirates.push_back(&entity);
  }
  return pirates;
}

// How far the pirates' ships in the sector stand from its node, at most.
float FarthestShip(TerritoryMatch& _match, std::int32_t _sector)
{
  float farthest = 0.0f;
  for (const Outpost::Entity* pirate : PiratesIn(_match, _sector))
  {
    if (pirate->kind == Outpost::EntityKind::Ship)
      farthest = std::max(farthest, Outpost::Distance(pirate->position, _match.Placement(_sector).node));
  }
  return farthest;
}

// A point _meters from the sector's node, on the way to Blue's home.
Outpost::PlanePosition TowardBlue(TerritoryMatch& _match, std::int32_t _sector, float _meters)
{
  const Outpost::PlanePosition node = _match.Placement(_sector).node;
  return node + Outpost::Normalized(BLUE_HOME - node, {1.0f, 0.0f}) * _meters;
}

// Blue's ships of this design in a row across the way to the node, 30 m apart, _meters from it, sent at it; whether they
// cleared the outpost, and how many of them were left, after at most _seconds.
std::pair<bool, std::size_t> Assault(TerritoryMatch& _match, std::int32_t _sector, Outpost::WeaponId _weapon, std::size_t _count,
                                     std::uint32_t _seconds)
{
  const Outpost::PlanePosition node = _match.Placement(_sector).node;
  const Outpost::PlanePosition from = TowardBlue(_match, _sector, 850.0f);
  const Outpost::PlaneVector across = Outpost::Perpendicular(Outpost::Normalized(node - from, {1.0f, 0.0f}));
  const Outpost::DesignId design = _match.World().FindDesign(BLUE, {Outpost::HullId{1}, Outpost::DriveId{1}, _weapon})->id;
  std::vector<Outpost::EntityId> ships;
  ships.reserve(_count);
  for (std::size_t i = 0; i < _count; ++i)
    ships.push_back(_match.World().SpawnShip(BLUE, design, from + across * (30.0f * static_cast<float>(i))));
  for (std::uint32_t tick = 0; tick < _seconds * TICKS_PER_SECOND && !PiratesIn(_match, _sector).empty(); ++tick)
  {
    std::erase_if(ships, [&_match](Outpost::EntityId _ship) { return _match.World().FindEntity(_ship) == nullptr; });
    if (ships.empty())
      break;
    // Sent again every five seconds, as a player would, since an attack-move ends where it was aimed. They fight to the
    // end, as the camp's balance was measured (ADR-073), rather than going back to be repaired (ADR-075).
    std::vector<Outpost::Command> commands;
    if (tick == 0)
      commands.push_back(
        {.player = BLUE, .order = Outpost::SetRetreatCommand{.ships = ships, .retreat = Outpost::RetreatThreshold::Never}});
    if (tick % (5 * TICKS_PER_SECOND) == 0)
      commands.push_back({.player = BLUE, .order = Outpost::AttackMoveCommand{.ships = ships, .destination = node}});
    (void)_match.World().Tick(commands);
  }
  std::erase_if(ships, [&_match](Outpost::EntityId _ship) { return _match.World().FindEntity(_ship) == nullptr; });
  return {PiratesIn(_match, _sector).empty(), ships.size()};
}
} // namespace

TEST_CLASS(PirateTests)
{
public:
  // Phase 4 design §8: a camp is a Defence Platform and three Small Mass Driver ships, a stronghold two Defence Platforms
  // and four Medium ships, one with a Lance, on their sectors' nodes, at the base level, holding their nodes for no one.
  TEST_METHOD(OutpostsStandOnTheirNodesForNoOne)
  {
    TerritoryMatch match(true);
    Assert::AreEqual(size_t{7}, match.Outposts().size());
    for (const Outpost::OutpostPlacement& outpost : match.Outposts())
    {
      const bool camp = outpost.outpost == "camp";
      std::int32_t platforms = 0;
      std::vector<Outpost::DesignComponents> ships;
      for (const Outpost::Entity* pirate : PiratesIn(match, outpost.sector))
      {
        Assert::IsTrue(Outpost::Distance(pirate->position, match.Placement(outpost.sector).node) < 150.0f, L"it stands at the node");
        Assert::IsTrue(pirate->hitPointsHundredths > 0 && pirate->hitPointsHundredths == pirate->maxHitPointsHundredths);
        if (pirate->kind == Outpost::EntityKind::Structure)
        {
          Assert::IsTrue(pirate->structure == Outpost::StructureKind::DefensePlatform);
          ++platforms;
          continue;
        }
        const Outpost::ShipDesign& design = *match.World().FindDesign(pirate->design);
        Assert::IsTrue(design.owner == Outpost::PIRATES);
        Assert::IsTrue(design.stats == Outpost::DesignStatsFor(match.TuningData(), design.components), L"at the base level");
        ships.push_back(design.components);
      }
      const Outpost::HullId hull{camp ? 1u : 2u};
      Assert::AreEqual(camp ? 1 : 2, platforms);
      Assert::AreEqual(size_t{camp ? 3u : 4u}, ships.size());
      Assert::AreEqual(std::ptrdiff_t{camp ? 0 : 1}, std::ranges::count(ships, Outpost::WeaponId{2}, &Outpost::DesignComponents::weapon));
      Assert::IsTrue(std::ranges::all_of(ships, [hull](const Outpost::DesignComponents& _ship) { return _ship.hull == hull; }));
      Assert::IsFalse(match.Sector(BLUE, outpost.sector).holder.IsValid(), L"the node is held for no one");
    }
    // Each outpost and its mirror's stand the same, turned half a turn about the center (ADR-073).
    for (const Outpost::Entity& pirate : match.World().Entities())
    {
      if (pirate.owner != Outpost::PIRATES || pirate.kind != Outpost::EntityKind::Structure)
        continue;
      const Outpost::PlanePosition across{.xMeters = -pirate.position.xMeters, .zMeters = -pirate.position.zMeters};
      Assert::IsTrue(
        std::ranges::any_of(match.World().Entities(), [&](const Outpost::Entity& _other)
                            { return _other.owner == Outpost::PIRATES && Outpost::Distance(_other.position, across) < 0.01f; }));
    }
  }

  // They attack a player's ship within the guard of their node, and once it is gone go back to the node.
  TEST_METHOD(AttackAnIntruderAndGoBack)
  {
    TerritoryMatch match(true);
    const std::int32_t sector = OutpostOf(match, "camp").sector;
    const Outpost::EntityId intruder = match.Warship(BLUE, TowardBlue(match, sector, GUARD_METERS - 100.0f));
    match.FightToTheEnd(BLUE, {intruder});
    match.Run(40 * TICKS_PER_SECOND);
    Assert::IsNull(match.World().FindEntity(intruder), L"the camp destroyed it");
    match.Run(30 * TICKS_PER_SECOND);
    Assert::IsTrue(FarthestShip(match, sector) < HOME_METERS + 50.0f, L"and went back to its node");
  }

  // A ship beyond the guard is left alone.
  TEST_METHOD(LeaveAShipBeyondTheGuardAlone)
  {
    TerritoryMatch match(true);
    const std::int32_t sector = OutpostOf(match, "camp").sector;
    const Outpost::EntityId passer = match.Warship(BLUE, TowardBlue(match, sector, GUARD_METERS + 100.0f));
    match.Run(30 * TICKS_PER_SECOND);
    Assert::IsNotNull(match.World().FindEntity(passer));
    Assert::IsTrue(FarthestShip(match, sector) < HOME_METERS);
  }

  // They chase a ship that runs no further than the chase from their node, never leave their sector, and go back.
  TEST_METHOD(ChaseNoFurtherThanTheChase)
  {
    TerritoryMatch match(true);
    const std::int32_t sector = OutpostOf(match, "camp").sector;
    const Outpost::EntityId runner = match.Warship(BLUE, TowardBlue(match, sector, GUARD_METERS - 50.0f));
    match.Run(3 * TICKS_PER_SECOND);
    Assert::IsTrue(FarthestShip(match, sector) > 250.0f, L"they set out after it");
    (void)match.World().Tick(
      {{.player = BLUE, .order = Outpost::MoveCommand{.ships = {runner}, .destination = TowardBlue(match, sector, 2500.0f)}}});
    float farthest = 0.0f;
    std::size_t pirates = PiratesIn(match, sector).size();
    for (std::uint32_t tick = 0; tick < 60 * TICKS_PER_SECOND; ++tick)
    {
      match.Run(1);
      farthest = std::max(farthest, FarthestShip(match, sector));
      // None of them left the sector: they are all still found in it.
      Assert::AreEqual(pirates, PiratesIn(match, sector).size());
    }
    Assert::IsTrue(farthest < CHASE_METERS + 50.0f, L"they chased no further than the chase");
    Assert::IsTrue(FarthestShip(match, sector) < HOME_METERS + 50.0f, L"and went back to the node");
    Assert::IsNotNull(match.World().FindEntity(runner));
  }

  // The sector cannot be claimed while a structure of its outpost stands, and can once they have all fallen.
  TEST_METHOD(ARelayWaitsForTheOutpostToFall)
  {
    TerritoryMatch match(true);
    const std::int32_t sector = OutpostOf(match, "camp").sector;
    (void)match.Relay(BLUE, match.Placement(sector).adjacent.front());
    match.Run(1);
    const Outpost::PlanePosition node = match.Placement(sector).node;
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, node) == Outpost::CommandResult::Guarded);
    Assert::IsTrue(Assault(match, sector, Outpost::WeaponId{2}, 8, 120).first, L"eight Pickets cleared it");
    // The camp's wreck lies on the node, where its platform stood, until it is salvaged (ADR-074).
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, node) == Outpost::CommandResult::InvalidPlacement, L"the wreck blocks");
    const auto wreck =
      std::ranges::find_if(match.World().Entities(), [node](const Outpost::Entity& _entity)
                           { return _entity.kind == Outpost::EntityKind::Derelict && Outpost::Distance(_entity.position, node) < 1.0f; });
    Assert::IsTrue(wreck != match.World().Entities().end(), L"a wreck on the node");
    Assert::AreEqual(600, wreck->salvageOre, L"a camp's");
    const Outpost::EntityId constructor = match.World().SpawnConstructor(BLUE, TowardBlue(match, sector, 80.0f));
    const Outpost::EntityId derelict = wreck->id;
    Assert::IsTrue(match.World()
                       .Tick({{.player = BLUE, .order = Outpost::SalvageCommand{.constructors = {constructor}, .derelict = derelict}}})
                       .front() == Outpost::CommandResult::Applied,
                   L"salvage ordered");
    match.Run(40 * TICKS_PER_SECOND);
    Assert::IsNull(match.World().FindEntity(derelict), L"salvaged");
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, node) == Outpost::CommandResult::Applied, L"then a Relay");
  }

  // Phase 4 design §8: a first fleet of Small ships clears a camp with losses. Measured: four Pickets clear it and lose
  // one; Swarms need eight, since a Mass Driver does a Defence Platform's armor little harm.
  TEST_METHOD(AFirstFleetClearsACampWithLosses)
  {
    {
      TerritoryMatch match(true);
      const auto [cleared, left] = Assault(match, OutpostOf(match, "camp").sector, Outpost::WeaponId{2}, 4, 120);
      Assert::IsTrue(cleared && left > 0 && left < 4, L"four Pickets clear a camp with losses");
    }
    {
      TerritoryMatch match(true);
      Assert::IsFalse(Assault(match, OutpostOf(match, "camp").sector, Outpost::WeaponId{2}, 3, 120).first, L"three Pickets do not");
    }
  }

  // Fog of war hides pirates as it hides an enemy (ADR-024).
  TEST_METHOD(PiratesHideUnderFog)
  {
    TerritoryMatch match(true);
    const std::int32_t sector = OutpostOf(match, "camp").sector;
    const std::vector<const Outpost::Entity*> pirates = PiratesIn(match, sector);
    const Outpost::EntityId id =
      (*std::ranges::find_if(pirates, [](const Outpost::Entity* _pirate) { return _pirate->kind == Outpost::EntityKind::Structure; }))->id;
    match.Run(1);
    Assert::IsFalse(match.Sees(BLUE, id));
    (void)match.Warship(BLUE, TowardBlue(match, sector, 150.0f));
    match.Run(1);
    Assert::IsTrue(match.Sees(BLUE, id));
  }
};
} // namespace GameLogicTests
