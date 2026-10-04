#include "pch.h"
#include "TerritoryMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr Outpost::PlayerId RED = TerritoryMatch::RED;

// The repository map's sectors (ADR-036): Blue's home in the southwest, its flanks south and west, the center, and Red's
// home in the northeast.
constexpr std::int32_t SOUTHWEST = 1;
constexpr std::int32_t SOUTH = 2;
constexpr std::int32_t WEST = 4;
constexpr std::int32_t CENTER = 5;
constexpr std::int32_t NORTHWEST = 7;
constexpr std::int32_t NORTHEAST = 9;
// A near asteroid in the south, and a rich one in the northwest.
constexpr Outpost::PlanePosition SOUTH_ASTEROID{.xMeters = -420.0f, .zMeters = -1350.0f};
constexpr Outpost::PlanePosition NORTHWEST_ASTEROID{.xMeters = -1900.0f, .zMeters = 1500.0f};
// Income in hundredths of an Ore a second: a near rig's 6, a rich rig's 10.
constexpr std::int32_t NEAR_INCOME = 600;
constexpr std::int32_t RICH_INCOME = 1000;

std::wstring Widen(std::string_view _text)
{
  return {_text.begin(), _text.end()};
}

} // namespace

TEST_CLASS(TerritoryTests)
{
public:
  // Phase 2 design §4: each player's Command Station holds its home sector from the first tick, and every other node is
  // free. Both players see every sector and who holds it.
  TEST_METHOD(EachStationHoldsItsHomeSector)
  {
    TerritoryMatch match;
    Assert::IsTrue(match.World().HasTerritory());
    match.Run(1);
    const Outpost::Snapshot blue = match.World().BuildSnapshot(BLUE);
    Assert::AreEqual(size_t{9}, blue.sectors.size());
    for (const Outpost::SectorView& sector : blue.sectors)
    {
      const Outpost::PlayerId expected = sector.id == SOUTHWEST ? BLUE : sector.id == NORTHEAST ? RED : Outpost::PlayerId{};
      Assert::IsTrue(sector.holder == expected, Widen(sector.nameUtf8).c_str());
      Assert::IsFalse(sector.suppressed || sector.cutOff);
    }
    Assert::IsTrue(blue.sectors == match.World().BuildSnapshot(RED).sectors, L"the territory is the same for both");
    const Outpost::SectorView south = match.Sector(BLUE, SOUTH);
    Assert::AreEqual(std::string("South"), south.nameUtf8);
    Assert::IsTrue(south.adjacent == std::vector<std::int32_t>{1, 3, 5});
  }

  // Phase 2 design §5: a Relay snaps to the node of the sector it is ordered in, and holds the sector once it is built; its
  // site holds nothing.
  TEST_METHOD(ARelayHoldsItsSectorOnceBuilt)
  {
    TerritoryMatch match;
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, {.xMeters = 300.0f, .zMeters = -1200.0f}) ==
                   Outpost::CommandResult::Applied);
    const auto relay = std::ranges::find(match.World().Entities(), Outpost::StructureKind::Relay, &Outpost::Entity::structure);
    Assert::IsTrue(relay != match.World().Entities().end());
    Assert::IsTrue(relay->position == match.Placement(SOUTH).node, L"the Relay stands on the node");
    const Outpost::EntityId site = relay->id;
    match.Run(1);
    Assert::IsFalse(match.Sector(BLUE, SOUTH).holder.IsValid(), L"a site holds nothing");

    const std::uint32_t ticksPerSecond = 20;
    for (std::uint32_t second = 0; second < 240 && !match.World().FindEntity(site)->IsBuilt(); ++second)
      match.Run(ticksPerSecond);
    Assert::IsTrue(match.World().FindEntity(site)->IsBuilt());
    match.Run(1);
    Assert::IsTrue(match.Sector(RED, SOUTH).holder == BLUE);
  }

  // Phase 2 design §6: a Relay is built only on a free node next to the player's territory.
  TEST_METHOD(ARelayGrowsTheTerritoryOutward)
  {
    TerritoryMatch match;
    match.Run(1);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(CENTER).node) == Outpost::CommandResult::NotAdjacent);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(NORTHEAST).node) ==
                     Outpost::CommandResult::InvalidPlacement,
                   L"Red's station stands on its home node");
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(SOUTHWEST).node) ==
                     Outpost::CommandResult::InvalidPlacement,
                   L"Blue's own station stands on its home node");

    (void)match.Relay(BLUE, SOUTH);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(SOUTH).node) ==
                     Outpost::CommandResult::InvalidPlacement,
                   L"the node is taken");
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(CENTER).node) == Outpost::CommandResult::Applied);
    Assert::IsTrue(match.Build(RED, Outpost::StructureKind::Relay, match.Placement(WEST).node) == Outpost::CommandResult::NotAdjacent,
                   L"Blue's territory is no step for Red");
  }

  // Phase 2 design §4: a rig may stand only in a sector its player holds.
  TEST_METHOD(ARigNeedsAHeldSector)
  {
    TerritoryMatch match;
    match.Run(1);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::MiningRig, SOUTH_ASTEROID) == Outpost::CommandResult::SectorNotHeld);
    (void)match.Relay(BLUE, SOUTH);
    Assert::IsTrue(match.Build(RED, Outpost::StructureKind::MiningRig, SOUTH_ASTEROID) == Outpost::CommandResult::SectorNotHeld);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::MiningRig, SOUTH_ASTEROID) == Outpost::CommandResult::Applied);
  }

  // Phase 2 design §4, §6: a rig earns while its player holds its sector, and half while that sector is cut off from the
  // home sector.
  TEST_METHOD(ARigEarnsWhileItsSectorIsHeldAndHalfWhenCutOff)
  {
    TerritoryMatch match;
    (void)match.Structure(BLUE, Outpost::StructureKind::MiningRig, SOUTH_ASTEROID);
    (void)match.Structure(BLUE, Outpost::StructureKind::MiningRig, NORTHWEST_ASTEROID);
    match.Run(1);
    Assert::AreEqual(0, match.Income(BLUE), L"neither rig's sector is held");

    (void)match.Relay(BLUE, SOUTH);
    match.Run(1);
    Assert::AreEqual(NEAR_INCOME, match.Income(BLUE));

    // The northwest is held, but not linked to the home: west is free.
    (void)match.Relay(BLUE, NORTHWEST);
    match.Run(1);
    Assert::IsTrue(match.Sector(BLUE, NORTHWEST).cutOff);
    Assert::AreEqual(NEAR_INCOME + (RICH_INCOME / 2), match.Income(BLUE));

    (void)match.Relay(BLUE, WEST);
    match.Run(1);
    Assert::IsFalse(match.Sector(BLUE, NORTHWEST).cutOff);
    Assert::AreEqual(NEAR_INCOME + RICH_INCOME, match.Income(BLUE));
  }

  // A rig in a sector held by the other player earns nothing, and draws nothing from its asteroid (Phase 2 design §4).
  TEST_METHOD(ARigInTheEnemysSectorEarnsNothing)
  {
    TerritoryMatch match;
    const Outpost::EntityId rig = match.Structure(BLUE, Outpost::StructureKind::MiningRig, SOUTH_ASTEROID);
    (void)match.Relay(RED, SOUTH);
    const auto reserve = [&match, rig]
    { return match.World().FindEntity(match.World().FindEntity(rig)->site)->oreReserveHundredths.value_or(-1); };
    const std::int64_t before = reserve();
    match.Run(100);
    Assert::AreEqual(0, match.Income(BLUE));
    Assert::AreEqual(before, reserve());
  }

  // Phase 2 design §5: an enemy warship within 400 m of a Relay suppresses it while none of its owner's is; the sector
  // earns nothing, but stays held, and the lattice still counts it.
  TEST_METHOD(AnEnemyWarshipSuppressesARelay)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, SOUTH);
    (void)match.Structure(BLUE, Outpost::StructureKind::MiningRig, SOUTH_ASTEROID);
    const Outpost::PlanePosition node = match.Placement(SOUTH).node;
    match.Run(1);
    Assert::AreEqual(NEAR_INCOME, match.Income(BLUE));

    // A Constructor is no warship.
    (void)match.World().SpawnConstructor(RED, {.xMeters = node.xMeters + 300.0f, .zMeters = node.zMeters});
    match.Run(1);
    Assert::IsFalse(match.Sector(BLUE, SOUTH).suppressed);

    (void)match.Warship(RED, {.xMeters = node.xMeters + 390.0f, .zMeters = node.zMeters});
    match.Run(1);
    const Outpost::SectorView suppressed = match.Sector(BLUE, SOUTH);
    Assert::IsTrue(suppressed.suppressed);
    Assert::IsTrue(suppressed.holder == BLUE, L"a suppressed Relay is still held");
    Assert::AreEqual(0, match.Income(BLUE));
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(CENTER).node) == Outpost::CommandResult::Applied,
                   L"a suppressed Relay still counts for the lattice");

    (void)match.Warship(BLUE, {.xMeters = node.xMeters - 390.0f, .zMeters = node.zMeters});
    match.Run(1);
    Assert::IsFalse(match.Sector(BLUE, SOUTH).suppressed, L"a defender lifts it");
    Assert::AreEqual(NEAR_INCOME, match.Income(BLUE));
  }

  // Phase 2 design §4: the home sector is never suppressed.
  TEST_METHOD(AHomeSectorIsNeverSuppressed)
  {
    TerritoryMatch match;
    const Outpost::PlanePosition home = match.Placement(SOUTHWEST).node;
    (void)match.Warship(RED, {.xMeters = home.xMeters + 200.0f, .zMeters = home.zMeters + 200.0f});
    match.Run(1);
    Assert::IsFalse(match.Sector(BLUE, SOUTHWEST).suppressed);
  }

  // ADR-056: a held sector that is not suppressed is in its holder's sight, the whole of it; a suppressed one is not, and
  // its Relay sees only as an unarmed structure does.
  TEST_METHOD(AHeldSectorIsSeenWhole)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, SOUTH);
    // Inside the south, beyond the Relay's own 200 m and out of its 400 m of suppression.
    const Outpost::EntityId distant = match.Warship(RED, {.xMeters = 700.0f, .zMeters = -1000.0f});
    // Inside Blue's home, beyond its station's sight.
    const Outpost::EntityId home = match.Warship(RED, {.xMeters = -1000.0f, .zMeters = -2400.0f});
    // Outside every sector Blue holds.
    const Outpost::EntityId away = match.Warship(RED, {.xMeters = 0.0f, .zMeters = 0.0f});
    match.Run(1);
    Assert::IsTrue(match.Sees(BLUE, distant));
    Assert::IsTrue(match.Sees(BLUE, home));
    Assert::IsFalse(match.Sees(BLUE, away));

    const Outpost::PlanePosition node = match.Placement(SOUTH).node;
    (void)match.Warship(RED, {.xMeters = node.xMeters + 150.0f, .zMeters = node.zMeters});
    match.Run(1);
    Assert::IsTrue(match.Sector(BLUE, SOUTH).suppressed);
    Assert::IsFalse(match.Sees(BLUE, distant), L"a suppressed sector is not seen whole");
  }

  // A map without sectors plays Phase 1's rules: no Relay, and ore anywhere (Phase 2 plan, rule 5).
  TEST_METHOD(AMapWithoutSectorsHasNoTerritory)
  {
    Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    map.sectors.clear();
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), map, {.seed = 3});
    server.World().PlaceStartingBases(map);
    Assert::IsFalse(server.World().HasTerritory());
    std::vector<Outpost::EntityId> constructors;
    for (const Outpost::Entity& entity : server.World().Entities())
    {
      if (entity.owner == BLUE && entity.role == Outpost::ShipRole::Constructor && entity.kind == Outpost::EntityKind::Ship)
        constructors.push_back(entity.id);
    }
    const auto build = [&](Outpost::StructureKind _kind, Outpost::PlanePosition _position)
    {
      return server.World()
        .Tick({{.player = BLUE,
                .order = Outpost::BuildStructureCommand{.constructors = constructors, .structure = _kind, .position = _position}}})
        .front();
    };
    Assert::IsTrue(build(Outpost::StructureKind::Relay, {.xMeters = 0.0f, .zMeters = -1667.0f}) == Outpost::CommandResult::NotBuildable);
    Assert::IsTrue(build(Outpost::StructureKind::MiningRig, SOUTH_ASTEROID) == Outpost::CommandResult::Applied);
    Assert::IsTrue(server.World().BuildSnapshot(BLUE).sectors.empty());
  }

  // Phase 3 design §7, gate K4: a Command Station at level 1 lets its player hold 3 nodes, home included, and each level
  // one more; a Relay under construction counts, as it takes its node; and the cap refuses claims but takes no node away,
  // even once the station is gone and its player is back to level 1's cap.
  TEST_METHOD(TheStationCapsTheNodesHeld)
  {
    TerritoryMatch match;
    match.Run(1);
    Assert::AreEqual(3, match.World().BuildSnapshot(BLUE).nodeCap);
    (void)match.Relay(BLUE, SOUTH);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(WEST).node) == Outpost::CommandResult::Applied);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(CENTER).node) == Outpost::CommandResult::CapReached,
                   L"the site in the west takes the third node");

    // Level 2: four nodes.
    const Outpost::EntityId station = std::ranges::find_if(match.World().Entities(),
                                                           [](const Outpost::Entity& _entity)
                                                           {
                                                             return _entity.owner == BLUE &&
                                                                    _entity.structure == Outpost::StructureKind::CommandStation &&
                                                                    _entity.kind == Outpost::EntityKind::Structure;
                                                           })
                                        ->id;
    // The level builds itself, while the Constructor builds the Relay in the west.
    const Outpost::UpgradeStructureCommand upgrade{.structure = station};
    Assert::IsTrue(match.World().Tick({{.player = BLUE, .order = upgrade}}).front() == Outpost::CommandResult::Applied);
    for (int tick = 0; tick < 120 * 20 && match.World().FindEntity(station)->level < 2; ++tick)
      match.Run(1);
    Assert::AreEqual(4, match.World().BuildSnapshot(BLUE).nodeCap);
    for (int tick = 0; tick < 180 * 20 && match.Sector(BLUE, WEST).holder != BLUE; ++tick)
      match.Run(1);
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(CENTER).node) == Outpost::CommandResult::Applied);
    for (int tick = 0; tick < 120 * 20 && match.Sector(BLUE, CENTER).holder != BLUE; ++tick)
      match.Run(1);
    Assert::IsTrue(match.Sector(BLUE, WEST).holder == BLUE && match.Sector(BLUE, CENTER).holder == BLUE);

    // Without its station, Blue has level 1's cap, 3, and keeps the three sectors its Relays hold.
    const Outpost::PlanePosition home = match.Placement(SOUTHWEST).node;
    const Outpost::DesignId lance = match.World().FindDesign(RED, {Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{2}})->id;
    std::vector<Outpost::EntityId> lances;
    lances.reserve(30);
    for (int i = 0; i < 30; ++i)
    {
      const int column = i % 6;
      const int row = i / 6;
      lances.push_back(match.World().SpawnShip(RED, lance,
                                               {.xMeters = home.xMeters + 150.0f + (15.0f * static_cast<float>(column)),
                                                .zMeters = home.zMeters + 150.0f + (15.0f * static_cast<float>(row))}));
    }
    match.Run(1);
    Assert::IsTrue(match.World().Tick({{.player = RED, .order = Outpost::AttackCommand{.ships = lances, .target = station}}}).front() ==
                   Outpost::CommandResult::Applied);
    for (int tick = 0; tick < 120 * 20 && match.World().FindEntity(station) != nullptr; ++tick)
      match.Run(1);
    Assert::IsNull(match.World().FindEntity(station), L"the lances destroyed the station");
    match.Run(1);
    Assert::AreEqual(3, match.World().BuildSnapshot(BLUE).nodeCap);
    for (const std::int32_t sector : {SOUTH, WEST, CENTER})
      Assert::IsTrue(match.Sector(BLUE, sector).holder == BLUE, L"no node is taken away");
    Assert::IsTrue(match.Build(BLUE, Outpost::StructureKind::Relay, match.Placement(NORTHWEST).node) == Outpost::CommandResult::CapReached);
  }
};
} // namespace GameLogicTests
