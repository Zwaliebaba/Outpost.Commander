#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
constexpr float MAP_SIZE_METERS = 2000.0f;
constexpr Outpost::PlanePosition ASTEROID{.xMeters = 300.0f, .zMeters = 0.0f};

Outpost::StructureTypeView Shipyard()
{
  return {.structure = Outpost::StructureKind::Shipyard, .nameUtf8 = "Shipyard", .radiusMeters = 40.0f};
}

Outpost::StructureTypeView Rig()
{
  return {.structure = Outpost::StructureKind::MiningRig, .nameUtf8 = "Mining Rig", .radiusMeters = 25.0f};
}

Outpost::StructureTypeView Relay()
{
  return {.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay", .radiusMeters = 30.0f};
}

// Three sectors in a row across the map, west to east, the player holding the west one, whose node its Command Station
// stands on (ADR-056); the asteroid is in the east one.
std::vector<Outpost::SectorView> Sectors()
{
  constexpr float HALF = MAP_SIZE_METERS / 2.0f;
  return {{.id = 1,
           .nameUtf8 = "West",
           .minXMeters = -HALF,
           .maxXMeters = -200.0f,
           .minZMeters = -HALF,
           .maxZMeters = HALF,
           .node = {.xMeters = -300.0f, .zMeters = 0.0f},
           .adjacent = {2},
           .holder = Outpost::PlayerId{1}},
          {.id = 2,
           .nameUtf8 = "Middle",
           .minXMeters = -200.0f,
           .maxXMeters = 200.0f,
           .minZMeters = -HALF,
           .maxZMeters = HALF,
           .node = {.xMeters = 0.0f, .zMeters = 500.0f},
           .adjacent = {1, 3}},
          {.id = 3,
           .nameUtf8 = "East",
           .minXMeters = 200.0f,
           .maxXMeters = HALF,
           .minZMeters = -HALF,
           .maxZMeters = HALF,
           .node = {.xMeters = 600.0f, .zMeters = 0.0f},
           .adjacent = {2}}};
}

std::vector<Outpost::EntityView> World()
{
  return {{.id = Outpost::EntityId{1}, .kind = Outpost::EntityKind::Asteroid, .position = ASTEROID, .radiusMeters = 45.0f},
          {.id = Outpost::EntityId{2},
           .kind = Outpost::EntityKind::Structure,
           .owner = Outpost::PlayerId{1},
           .structure = Outpost::StructureKind::CommandStation,
           .position = {.xMeters = -300.0f, .zMeters = 0.0f},
           .radiusMeters = 45.0f}};
}
} // namespace

TEST_CLASS(PlacementTests)
{
public:
  // ADR-056: a Relay's ghost stands on the node of the sector under the cursor, and is green only on a free node next to
  // a sector the player holds; without sectors it is never green.
  TEST_METHOD(ARelaySnapsToItsSectorsNode)
  {
    constexpr Outpost::PlayerId PLAYER{1};
    const std::vector<Outpost::EntityView> world = World();
    std::vector<Outpost::SectorView> sectors = Sectors();
    const Outpost::GhostPlacement middle = Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS, sectors, PLAYER);
    Assert::IsTrue(middle.position == sectors[1].node);
    Assert::AreEqual(30.0f, middle.radiusMeters);
    Assert::IsTrue(middle.valid);
    Assert::IsFalse(Outpost::PlaceGhost(Relay(), {700.0f, 0.0f}, world, MAP_SIZE_METERS, sectors, PLAYER).valid,
                    L"not next to its sectors");
    Assert::IsFalse(Outpost::PlaceGhost(Relay(), {-500.0f, 0.0f}, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"its own node");
    Assert::IsFalse(Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS, sectors, Outpost::PlayerId{2}).valid,
                    L"another player's territory is no step");
    sectors[1].holder = Outpost::PlayerId{2};
    Assert::IsFalse(Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"a held node");
    Assert::IsFalse(Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS).valid, L"no sectors, no Relay");
  }

  // Phase 4 design §8: a Relay's ghost is red in a sector the pirates guard, which every player sees (ADR-073).
  TEST_METHOD(ARelayWaitsForThePiratesToFall)
  {
    constexpr Outpost::PlayerId PLAYER{1};
    const std::vector<Outpost::EntityView> world = World();
    std::vector<Outpost::SectorView> sectors = Sectors();
    sectors[1].guarded = true;
    Assert::IsFalse(Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS, sectors, PLAYER).valid);
    sectors[1].guarded = false;
    Assert::IsTrue(Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS, sectors, PLAYER).valid);
  }

  // Phase 3 design §7: a Relay's ghost is red while the player has taken its Command Station's cap of nodes, and a Relay
  // still under construction has taken its node.
  TEST_METHOD(ARelayWaitsAtTheNodeCap)
  {
    constexpr Outpost::PlayerId PLAYER{1};
    std::vector<Outpost::EntityView> world = World();
    const std::vector<Outpost::SectorView> sectors = Sectors();
    const auto valid = [&](std::int32_t _cap)
    { return Outpost::PlaceGhost(Relay(), {100.0f, -700.0f}, world, MAP_SIZE_METERS, sectors, PLAYER, _cap).valid; };
    Assert::IsTrue(valid(0), L"no cap");
    Assert::IsTrue(valid(2), L"one node of two");
    Assert::IsFalse(valid(1), L"its home is its one node");
    Assert::AreEqual(1, Outpost::NodesTaken(sectors, world, PLAYER));

    world.push_back({.id = Outpost::EntityId{3},
                     .kind = Outpost::EntityKind::Structure,
                     .owner = PLAYER,
                     .structure = Outpost::StructureKind::Relay,
                     .position = {.xMeters = 600.0f, .zMeters = 0.0f},
                     .radiusMeters = 30.0f,
                     .builtPermille = 400});
    Assert::AreEqual(2, Outpost::NodesTaken(sectors, world, PLAYER), L"a site takes its node");
    Assert::IsFalse(valid(2));
    Assert::IsTrue(Outpost::AtNodeCap(sectors, world, PLAYER, 2));
    Assert::IsFalse(Outpost::AtNodeCap(sectors, world, Outpost::PlayerId{2}, 2), L"another player's");
  }

  // Interface plan 2, task UI1.1: a node is marked claimable by the rule that makes the Relay's ghost green there, for
  // every sector, player and cap, with the node clear or covered.
  TEST_METHOD(ClaimsANodeByTheGhostsRule)
  {
    std::vector<Outpost::EntityView> world = World();
    std::vector<Outpost::SectorView> sectors = Sectors();
    const auto agree = [&]()
    {
      for (const Outpost::SectorView& sector : sectors)
      {
        for (const Outpost::PlayerId player : {Outpost::PlayerId{1}, Outpost::PlayerId{2}})
        {
          for (const std::int32_t cap : {0, 1, 2})
          {
            const bool ghost = Outpost::PlaceGhost(Relay(), sector.node, world, MAP_SIZE_METERS, sectors, player, cap).valid;
            Assert::AreEqual(ghost, Outpost::CanClaim(sector, Relay().radiusMeters, world, MAP_SIZE_METERS, sectors, player, cap));
          }
        }
      }
    };
    agree();
    Assert::IsTrue(Outpost::CanClaim(sectors[1], 30.0f, world, MAP_SIZE_METERS, sectors, Outpost::PlayerId{1}, 0), L"next to home");
    Assert::IsFalse(Outpost::CanClaim(sectors[2], 30.0f, world, MAP_SIZE_METERS, sectors, Outpost::PlayerId{1}, 0), L"two away");
    // A derelict over the middle node, as an outpost's wreck covers its camp's (ADR-074).
    world.push_back(
      {.id = Outpost::EntityId{3}, .kind = Outpost::EntityKind::Derelict, .position = sectors[1].node, .radiusMeters = 20.0f});
    agree();
    Assert::IsFalse(Outpost::CanClaim(sectors[1], 30.0f, world, MAP_SIZE_METERS, sectors, Outpost::PlayerId{1}, 0), L"covered");
    sectors[1].guarded = true;
    world.pop_back();
    agree();
    Assert::IsFalse(Outpost::CanClaim(sectors[1], 30.0f, world, MAP_SIZE_METERS, sectors, Outpost::PlayerId{1}, 0), L"guarded");
  }

  // Phase 2 design §4: on a map with sectors a rig's ghost is green only in a sector the player holds.
  TEST_METHOD(ARigNeedsAHeldSector)
  {
    constexpr Outpost::PlayerId PLAYER{1};
    const std::vector<Outpost::EntityView> world = World();
    std::vector<Outpost::SectorView> sectors = Sectors();
    Assert::IsFalse(Outpost::PlaceGhost(Rig(), ASTEROID, world, MAP_SIZE_METERS, sectors, PLAYER).valid);
    sectors[2].holder = PLAYER;
    Assert::IsTrue(Outpost::PlaceGhost(Rig(), ASTEROID, world, MAP_SIZE_METERS, sectors, PLAYER).valid);
    Assert::IsTrue(Outpost::PlaceGhost(Rig(), ASTEROID, world, MAP_SIZE_METERS).valid, L"without sectors, ore anywhere");
  }

  // Phase 4 design §6, gate L5: on a map with sectors a Shipyard's ghost is green only in a sector the player holds.
  TEST_METHOD(AShipyardNeedsAHeldSector)
  {
    constexpr Outpost::PlayerId PLAYER{1};
    const std::vector<Outpost::EntityView> world = World();
    std::vector<Outpost::SectorView> sectors = Sectors();
    constexpr Outpost::PlanePosition WEST{.xMeters = -700.0f, .zMeters = 400.0f};
    constexpr Outpost::PlanePosition EAST{.xMeters = 600.0f, .zMeters = 400.0f};
    Assert::IsTrue(Outpost::PlaceGhost(Shipyard(), WEST, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"its own sector");
    Assert::IsFalse(Outpost::PlaceGhost(Shipyard(), EAST, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"a free sector");
    sectors[2].holder = Outpost::PlayerId{2};
    Assert::IsFalse(Outpost::PlaceGhost(Shipyard(), EAST, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"the enemy's sector");
    sectors[2].holder = PLAYER;
    Assert::IsTrue(Outpost::PlaceGhost(Shipyard(), EAST, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"a sector it took");
    Assert::IsTrue(Outpost::PlaceGhost(Shipyard(), EAST, world, MAP_SIZE_METERS).valid, L"without sectors, anywhere");

    // Phase 4 design §10: so is a Repair Bay's.
    const Outpost::StructureTypeView bay{.structure = Outpost::StructureKind::RepairBay, .nameUtf8 = "Repair Bay", .radiusMeters = 30.0f};
    sectors[2].holder = {};
    Assert::IsTrue(Outpost::PlaceGhost(bay, WEST, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"a bay in its own sector");
    Assert::IsFalse(Outpost::PlaceGhost(bay, EAST, world, MAP_SIZE_METERS, sectors, PLAYER).valid, L"a bay in a free sector");
  }

  // ADR-016's rules as the ghost shows them: inside the map, and overlapping no asteroid, field or structure.
  TEST_METHOD(AStructureStandsClearOfEverything)
  {
    const std::vector<Outpost::EntityView> world = World();
    Assert::IsTrue(Outpost::PlaceGhost(Shipyard(), {0.0f, 0.0f}, world, MAP_SIZE_METERS).valid);
    Assert::IsFalse(Outpost::PlaceGhost(Shipyard(), {ASTEROID.xMeters - 80.0f, 0.0f}, world, MAP_SIZE_METERS).valid);
    Assert::IsFalse(Outpost::PlaceGhost(Shipyard(), {-220.0f, 0.0f}, world, MAP_SIZE_METERS).valid);
    Assert::IsFalse(Outpost::PlaceGhost(Shipyard(), {980.0f, 0.0f}, world, MAP_SIZE_METERS).valid);
    const Outpost::GhostPlacement ghost = Outpost::PlaceGhost(Shipyard(), {0.0f, 100.0f}, world, MAP_SIZE_METERS);
    Assert::AreEqual(100.0f, ghost.position.zMeters);
    Assert::AreEqual(40.0f, ghost.radiusMeters);
  }

  // ADR-074: no structure stands on a derelict, though it blocks no path.
  TEST_METHOD(AStructureStandsClearOfADerelict)
  {
    std::vector<Outpost::EntityView> world = World();
    world.push_back({.id = Outpost::EntityId{9},
                     .kind = Outpost::EntityKind::Derelict,
                     .position = {.xMeters = 0.0f, .zMeters = 300.0f},
                     .radiusMeters = 25.0f});
    Assert::IsFalse(Outpost::PlaceGhost(Shipyard(), {0.0f, 330.0f}, world, MAP_SIZE_METERS).valid);
    Assert::IsTrue(Outpost::PlaceGhost(Shipyard(), {0.0f, 400.0f}, world, MAP_SIZE_METERS).valid);
  }

  // Design §6: a Mining Rig snaps to a free ore asteroid within reach of the cursor, and covers it.
  TEST_METHOD(ARigSnapsToAFreeAsteroid)
  {
    std::vector<Outpost::EntityView> world = World();
    const Outpost::PlanePosition beside{ASTEROID.xMeters, 45.0f + Outpost::RIG_SNAP_METERS - 1.0f};
    const Outpost::GhostPlacement ghost = Outpost::PlaceGhost(Rig(), beside, world, MAP_SIZE_METERS);
    Assert::IsTrue(ghost.valid);
    Assert::IsTrue(ghost.position == ASTEROID);
    Assert::AreEqual(45.0f, ghost.radiusMeters);
    Assert::IsFalse(Outpost::PlaceGhost(Rig(), {0.0f, 0.0f}, world, MAP_SIZE_METERS).valid, L"no asteroid within reach");

    world.push_back({.id = Outpost::EntityId{3},
                     .kind = Outpost::EntityKind::Structure,
                     .owner = Outpost::PlayerId{2},
                     .structure = Outpost::StructureKind::MiningRig,
                     .position = ASTEROID,
                     .radiusMeters = 45.0f});
    Assert::IsFalse(Outpost::PlaceGhost(Rig(), beside, world, MAP_SIZE_METERS).valid, L"taken");
  }
};
} // namespace GameProtocolTests