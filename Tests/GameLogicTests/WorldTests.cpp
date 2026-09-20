#include "pch.h"

#include "Sim.h"
#include "Snapshot.h"
#include "World.h"

#include <cstdint>
#include <optional>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// What World owes every later system (TechnicalDesign.md §4.3): an id issued in creation order
// that never aliases, resolution that refuses a stale one, iteration in id order rather than slot
// order, and a hash and a snapshot that cover every field of every record.
namespace SimTests
{

namespace
{

Outpost::Device Unit(std::uint8_t _seat, std::int32_t _x)
{
  Outpost::Device device{};
  device.seat = _seat;
  device.design = 2;
  device.x = _x;
  device.y = 64;
  device.z = -_x;
  device.facing = 0x4000;
  device.hitPoints = 250;
  device.experience = 7;
  device.primaryOrder = Outpost::PrimaryOrder::Move;
  device.destinationX = _x + 1024;
  device.destinationZ = 512;
  device.group = 3;
  device.reloadTicks = {3, 0, 0, 0, 0, 0, 0, 0};
  return device;
}

Outpost::Structure Building(std::uint8_t _seat, std::uint32_t _cell)
{
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = 5;
  structure.cellX = _cell;
  structure.cellY = _cell + 1;
  structure.y = 32;
  structure.state = Outpost::StructurePhase::UnderConstruction;
  structure.hitPoints = 400;
  structure.buildEffortHundredths = 4250;
  structure.modules = {1, 2, 0, 0};
  structure.moduleCount = 2;
  structure.workRemainingTicks = 40;
  return structure;
}

} // namespace

TEST_CLASS(WorldTests)
{
public:
  TEST_METHOD(IdsAreIssuedInCreationOrderAcrossEveryKind)
  {
    Outpost::World world;
    const Outpost::ObjectId first = world.Create(Unit(0, 100));
    const Outpost::ObjectId second = world.Create(Building(0, 4));
    const Outpost::ObjectId third = world.Create(Unit(1, 200));
    Assert::AreEqual(std::uint32_t{1}, first.value, L"the first object is 1, because 0 is NO_OBJECT");
    Assert::AreEqual(std::uint32_t{2}, second.value, L"one counter across the kinds, not one per kind");
    Assert::AreEqual(std::uint32_t{3}, third.value);
    Assert::IsTrue(first.kind == Outpost::ObjectKind::Device);
    Assert::IsTrue(second.kind == Outpost::ObjectKind::Structure);
    Assert::AreEqual(std::uint32_t{4}, world.NextId());
  }

  TEST_METHOD(ARemovedIdResolvesToNothingEvenAfterItsSlotIsReused)
  {
    // The whole point of a counter over an index: the new device takes the freed slot, and the old
    // id must still resolve to nothing rather than to it.
    Outpost::World world;
    const Outpost::ObjectId first = world.Create(Unit(0, 100));
    Assert::IsNotNull(world.FindDevice(first));
    Assert::IsTrue(world.Remove(first));
    Assert::IsNull(world.FindDevice(first), L"a removed id resolves to nothing");
    Assert::IsFalse(world.Alive(first));
    Assert::IsFalse(world.Remove(first), L"and removing it twice is refused");

    const Outpost::ObjectId second = world.Create(Unit(0, 900));
    Assert::AreNotEqual(first.value, second.value, L"an id is never issued twice");
    Assert::IsNull(world.FindDevice(first), L"the slot is reused; the id is not");
    Assert::IsNotNull(world.FindDevice(second));
    Assert::AreEqual(std::int32_t{900}, world.FindDevice(second)->x);
  }

  TEST_METHOD(AnIdOfTheWrongKindOrNoneResolvesToNothing)
  {
    Outpost::World world;
    const Outpost::ObjectId device = world.Create(Unit(0, 100));
    Assert::IsNull(world.FindStructure(device), L"the kind tag is part of the identity");
    Assert::IsNull(world.FindDevice(Outpost::NO_OBJECT));
    Assert::IsFalse(world.Alive(Outpost::NO_OBJECT));
    Assert::IsFalse(world.Remove(Outpost::NO_OBJECT));
    Assert::IsNull(world.FindDevice({device.value + 50, Outpost::ObjectKind::Device}), L"and one never issued");
  }

  TEST_METHOD(IterationIsInIdOrderWhateverTheSlotsDid)
  {
    // Erase from the middle and create again: the new record takes the freed slot, so slot order
    // and id order disagree and only id order is stable across the two runs a match may take.
    Outpost::World world;
    const Outpost::ObjectId first = world.Create(Unit(0, 1));
    const Outpost::ObjectId second = world.Create(Unit(0, 2));
    const Outpost::ObjectId third = world.Create(Unit(0, 3));
    Assert::IsTrue(world.Remove(second));
    const Outpost::ObjectId fourth = world.Create(Unit(0, 4));
    std::vector<std::uint32_t> seen;
    world.ForEachDevice([&seen](Outpost::ObjectId _id, const Outpost::Device&) { seen.push_back(_id.value); });
    Assert::AreEqual(std::size_t{3}, seen.size());
    Assert::AreEqual(first.value, seen[0]);
    Assert::AreEqual(third.value, seen[1]);
    Assert::AreEqual(fourth.value, seen[2], L"ascending id, though its slot is the second's");
  }

  TEST_METHOD(EveryKindCountsAndResolvesOnItsOwnMap)
  {
    Outpost::World world;
    const Outpost::ObjectId device = world.Create(Unit(0, 10));
    const Outpost::ObjectId structure = world.Create(Building(1, 6));
    Outpost::Projectile shot{};
    shot.seat = 1;
    shot.shooter = device;
    shot.ticksToImpact = 12;
    const Outpost::ObjectId projectile = world.Create(shot);
    Outpost::Feature rock{};
    rock.design = 3;
    rock.cellX = 20;
    const Outpost::ObjectId feature = world.Create(rock);
    Outpost::Wreck hulk{};
    hulk.origin = device;
    hulk.decayTicks = 200;
    const Outpost::ObjectId wreck = world.Create(hulk);

    Assert::AreEqual(std::size_t{1}, world.Count(Outpost::ObjectKind::Device));
    Assert::AreEqual(std::size_t{1}, world.Count(Outpost::ObjectKind::Structure));
    Assert::AreEqual(std::size_t{1}, world.Count(Outpost::ObjectKind::Projectile));
    Assert::AreEqual(std::size_t{1}, world.Count(Outpost::ObjectKind::Feature));
    Assert::AreEqual(std::size_t{1}, world.Count(Outpost::ObjectKind::Wreck));
    Assert::IsNotNull(world.FindStructure(structure));
    Assert::IsNotNull(world.FindProjectile(projectile));
    Assert::IsNotNull(world.FindFeature(feature));
    Assert::IsNotNull(world.FindWreck(wreck));
    // A projectile refers to its shooter by id, so the shooter dying leaves the reference stale
    // rather than dangling, which is the property the whole scheme exists for.
    Assert::IsTrue(world.Remove(device));
    Assert::IsNull(world.FindDevice(world.FindProjectile(projectile)->shooter));
  }
};

} // namespace SimTests
