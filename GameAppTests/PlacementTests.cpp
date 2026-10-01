#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
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
} // namespace GameAppTests