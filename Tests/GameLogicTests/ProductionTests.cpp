#include "pch.h"

#include "Construction.h"
#include "Design.h"
#include "Production.h"
#include "Sim.h"
#include "Snapshot.h"

#include "FixedPoint.h"

#include <cstdint>
#include <optional>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Devices and production (GameDesign.md §6; m1-vertical-slice/S5). What the seat holds is a design
// of ROW INDICES and what Content derives from is one of IDS, so the first thing under test is that
// the conversion reaches the design's own two worked answers; after that, the queue, the price, the
// pause at the cap, where a device comes out, and that an upgrade researched later reaches what is
// already in the field.
namespace SimTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;

enum class Row : std::uint32_t
{
  CommandPost = 0,
  Factory = 1
};

enum class Chassis : std::uint32_t
{
  Light = 0,
  Heavy = 1
};

enum class Drive : std::uint32_t
{
  Wheels = 0,
  Tracks = 1
};

enum class Module : std::uint32_t
{
  MachineGun = 0,
  Cannon = 1,
  Builder = 2
};

/// The shipped rows of the two designs GameDesign.md §6 works through, plus a factory and a
/// builder. Written out rather than loaded so that the suite needs no GameData beside it;
/// ContentTests pins the SHIPPED tables against the same two answers, so if these drift from
/// GameData\Components.json that suite fails and this one goes on measuring the conversion.
const Outpost::ContentTree& Tables()
{
  static const Outpost::ContentTree TREE = []
  {
    Outpost::ContentTree tree;
    Outpost::StructureDesc post{};
    post.id = "CommandPost";
    post.role = Outpost::StructureRole::CommandPost;
    post.footprintCellsX = 3;
    post.footprintCellsY = 3;
    post.hitPoints = 1500;
    post.costHundredths = 50000;
    post.buildTimeTicks = 1200;
    Outpost::StructureDesc factory{};
    factory.id = "Factory";
    factory.role = Outpost::StructureRole::Factory;
    factory.footprintCellsX = 3;
    factory.footprintCellsY = 3;
    factory.hitPoints = 800;
    factory.costHundredths = 40000;
    factory.buildTimeTicks = 1200;
    factory.moduleSlots = 2;
    factory.modules = {"FactoryModule"};
    tree.structures.structures = {post, factory};
    Outpost::StructureModuleDesc factoryModule{};
    factoryModule.id = "FactoryModule";
    factoryModule.effect = Outpost::StructureModuleEffect::ShortenBuildTime;
    factoryModule.amount = 25;
    factoryModule.costHundredths = 15000;
    factoryModule.buildTimeTicks = 400;
    tree.structures.modules = {factoryModule};

    Outpost::ChassisDesc light{};
    light.id = "LightI";
    light.chassisClass = Outpost::ChassisClass::Light;
    light.hitPoints = 100;
    light.kineticArmor = 5;
    light.thermalArmor = 5;
    light.baseSpeedSubunitsPerTick = 1024;
    light.sightSubunits = 20 * CELL;
    light.costHundredths = 6000;
    light.mounts = 1;
    Outpost::ChassisDesc heavy{};
    heavy.id = "HeavyI";
    heavy.chassisClass = Outpost::ChassisClass::Heavy;
    heavy.unlockedBy = "HeavyChassis";
    heavy.hitPoints = 500;
    heavy.kineticArmor = 25;
    heavy.thermalArmor = 18;
    heavy.baseSpeedSubunitsPerTick = 512;
    heavy.sightSubunits = 16 * CELL;
    heavy.costHundredths = 32000;
    heavy.mounts = 1;
    tree.components.chassis = {light, heavy};

    Outpost::DriveDesc wheels{};
    wheels.id = "Wheels";
    wheels.driveClass = Outpost::DriveClass::Wheels;
    wheels.speedFactorHundredths = 130;
    wheels.maxSlopePercent = 25;
    wheels.hitPointFactorHundredths = 100;
    wheels.costHundredths = 3000;
    Outpost::DriveDesc tracks{};
    tracks.id = "Tracks";
    tracks.driveClass = Outpost::DriveClass::Tracks;
    tracks.unlockedBy = "TrackedDrives";
    tracks.speedFactorHundredths = 80;
    tracks.maxSlopePercent = 40;
    tracks.hitPointFactorHundredths = 150;
    tracks.costHundredths = 7000;
    tree.components.drives = {wheels, tracks};

    Outpost::ModuleDesc gun{};
    gun.id = "MachineGun";
    gun.systemKind = Outpost::SystemKind::None;
    gun.weaponClass = Outpost::WeaponClass::AntiLight;
    gun.costHundredths = 4000;
    gun.damage = 8;
    Outpost::ModuleDesc cannon{};
    cannon.id = "Cannon";
    cannon.systemKind = Outpost::SystemKind::None;
    cannon.unlockedBy = "Ballistics";
    cannon.weaponClass = Outpost::WeaponClass::AntiTank;
    cannon.weightPenaltyPercent = 10;
    cannon.costHundredths = 10000;
    cannon.damage = 60;
    Outpost::ModuleDesc builder{};
    builder.id = "Builder";
    builder.systemKind = Outpost::SystemKind::Builder;
    builder.costHundredths = 5000;
    builder.systemRangeSubunits = 8 * CELL;
    builder.buildPowerHundredthsPerTick = Outpost::REFERENCE_BUILD_POWER_HUNDREDTHS_PER_TICK;
    tree.components.modules = {gun, cannon, builder};

    // Three items, so that a design's unlocks are a real question rather than always yes.
    Outpost::ResearchItemDesc ballistics{};
    ballistics.id = "Ballistics";
    Outpost::ResearchItemDesc heavyChassis{};
    heavyChassis.id = "HeavyChassis";
    Outpost::ResearchItemDesc trackedDrives{};
    trackedDrives.id = "TrackedDrives";
    tree.research = {ballistics, heavyChassis, trackedDrives};
    return tree;
  }();
  return TREE;
}

Outpost::MatchSettings TwoSeats()
{
  Outpost::MatchSettings settings{};
  settings.seed = 13;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::High;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = Outpost::VictoryCondition::Annihilation;
  settings.seats[0] = {Outpost::SeatKind::Human, 0};
  settings.seats[1] = {Outpost::SeatKind::Ai, 1};
  return settings;
}

Outpost::LandscapeDefinition Ground()
{
  Outpost::LandscapeDefinition definition{};
  definition.version = Outpost::LANDSCAPE_DEFINITION_VERSION;
  definition.sizeClass = Outpost::SizeClass::Small;
  definition.cellsPerSide = Outpost::SIZE_CLASS_CELLS[0];
  definition.seed = 1;
  definition.palette = "Default";
  definition.tiles = {{0, 0, 512, 170, 90, 100, 48, 70, 1, 32, ""}};
  definition.starts = {{16, 16}, {100, 100}};
  return definition;
}

[[nodiscard]] Outpost::DeviceDesign Designed(Chassis _chassis, Drive _drive, Module _module)
{
  Outpost::DeviceDesign design{};
  design.chassis = static_cast<std::uint32_t>(_chassis);
  design.drive = static_cast<std::uint32_t>(_drive);
  design.modules = {};
  design.modules[0] = static_cast<std::uint32_t>(_module);
  design.moduleCount = 1;
  return design;
}

/// The Sim a snapshot of _sim reads back as. The failure is Assert::Fail rather than a check the
/// caller makes, because that is what the other suites do and because clang-tidy can see it is
/// noreturn where it cannot see that an assertion on has_value() guards the dereference.
Outpost::Sim Reload(const Outpost::Sim& _sim)
{
  std::optional<Outpost::Sim> reloaded = Outpost::Snapshot::Read(Outpost::Snapshot::Write(_sim), Tables());
  if (!reloaded.has_value())
  {
    Assert::Fail(L"the snapshot did not read back"); // noreturn, which is what the access below relies on
  }
  return *reloaded;
}

Outpost::ObjectId Standing(Outpost::Sim& _sim, std::uint8_t _seat, Row _row, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = static_cast<std::uint32_t>(_row);
  structure.cellX = _cellX;
  structure.cellY = _cellY;
  structure.state = Outpost::StructurePhase::Standing;
  structure.hitPoints = 800;
  structure.buildEffortHundredths = Outpost::RequiredEffortHundredths(1200);
  structure.working = Outpost::NO_OBJECT;
  return _sim.Objects().Create(structure);
}

/// A match with a factory at (40,40) and one saved design, ready to build.
struct Line
{
  Outpost::Sim sim{TwoSeats(), Tables()};
  Outpost::ObjectId factory;

  Line()
  {
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    factory = Standing(sim, 0, Row::Factory, 40, 40);
    Assert::IsTrue(Outpost::SaveDesign(sim, 0, 0, Designed(Chassis::Light, Drive::Wheels, Module::MachineGun)));
  }
};

} // namespace

TEST_CLASS(ProductionTests)
{
public:
  TEST_METHOD(ASeatsIndexedDesignReachesTheSameTwoAnswersTheDesignWorksThrough)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.designs.push_back(Designed(Chassis::Light, Drive::Wheels, Module::MachineGun));

    Outpost::DesignStats scout{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 0, scout) == Outpost::DesignFault::None);
    // GameDesign.md §6: 104 world units a second for 130 power in 13 seconds.
    Assert::AreEqual(104, Outpost::WorldUnitsPerSecond(scout.speedSubunitsPerTick));
    Assert::AreEqual(13000, scout.costHundredths);
    Assert::AreEqual(std::uint32_t{13} * Neuron::TICKS_PER_SECOND, Outpost::BuildTimeTicksFor(scout.costHundredths));
    Assert::AreEqual(100, scout.hitPoints);

    // The second needs research, which is the other half of "modules the seat has researched".
    seat.designs.push_back(Designed(Chassis::Heavy, Drive::Tracks, Module::Cannon));
    Assert::IsTrue(Outpost::CheckDesign(seat, Tables(), seat.designs[1]) == Outpost::DesignFault::UnknownChassis,
                   L"a heavy chassis nobody researched is not a chassis this commander has");
    seat.researchComplete = {0, 1, 2};
    Outpost::DesignStats line{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 1, line) == Outpost::DesignFault::None);
    // GameDesign.md §6: 29 world units a second for 490 power in 49 seconds.
    Assert::AreEqual(29, Outpost::WorldUnitsPerSecond(line.speedSubunitsPerTick));
    Assert::AreEqual(49000, line.costHundredths);
    Assert::AreEqual(std::uint32_t{49} * Neuron::TICKS_PER_SECOND, Outpost::BuildTimeTicksFor(line.costHundredths));
    Assert::AreEqual(750, line.hitPoints, L"500 hit points at the tracks' factor of 1.5");
  }

  TEST_METHOD(ADesignIsRefusedUntilItsPartsAreResearchedAndAnOrderPacksItsModules)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsFalse(Outpost::SaveDesign(sim, 0, 0, Designed(Chassis::Light, Drive::Wheels, Module::Cannon)), L"the cannon is research");
    sim.SeatAt(0).researchComplete = {0};
    Assert::IsTrue(Outpost::SaveDesign(sim, 0, 0, Designed(Chassis::Light, Drive::Wheels, Module::Cannon)));
    Assert::AreEqual(std::size_t{1}, sim.SeatAt(0).designs.size());
    // A slot past the end would leave a hole nothing fills, and a design index is what an order
    // names.
    Assert::IsFalse(Outpost::SaveDesign(sim, 0, 5, Designed(Chassis::Light, Drive::Wheels, Module::MachineGun)));
    Assert::IsTrue(Outpost::SaveDesign(sim, 0, 1, Designed(Chassis::Light, Drive::Wheels, Module::MachineGun)));

    // The wire form: the modules one per byte, the empty slots 0xFF (Sim/Order.h's table).
    const Outpost::DeviceDesign packed = Outpost::DesignFromOrder(0, 0, static_cast<std::int32_t>(0xFFFFFF00u));
    Assert::AreEqual(std::uint8_t{1}, packed.moduleCount);
    Assert::AreEqual(0u, packed.modules[0]);
    const Outpost::DeviceDesign two = Outpost::DesignFromOrder(1, 1, static_cast<std::int32_t>(0xFFFF0100u));
    Assert::AreEqual(std::uint8_t{2}, two.moduleCount);
    Assert::AreEqual(1u, two.modules[1]);
    Assert::AreEqual(std::uint8_t{0}, Outpost::DesignFromOrder(0, 0, -1).moduleCount, L"every slot empty is no modules at all");
  }

  TEST_METHOD(AFactoryBuildsTheQueueAndTheDeviceComesOutBesideIt)
  {
    Line line;
    Outpost::Seat& seat = line.sim.SeatAt(0);
    seat.powerHundredths = 13000;
    Assert::IsTrue(Outpost::SetProduction(line.sim, 0, line.factory, 0, 2));

    const std::uint32_t ticks = Outpost::BuildTimeTicksFor(13000);
    for (std::uint32_t tick = 0; tick < ticks; ++tick)
    {
      line.sim.Advance();
    }
    Assert::AreEqual(std::size_t{1}, line.sim.Objects().Count(Outpost::ObjectKind::Device), L"the first one, on the tick its time says");
    Assert::AreEqual(0, seat.powerHundredths, L"and 130 power drawn when the factory started it");
    Assert::AreEqual(1u, seat.production.front().remaining, L"one of the two still to build");

    // It stands on a cell of the ring around the factory's three by three, not inside it.
    std::uint32_t found = 0;
    line.sim.Objects().ForEachDevice(
      [&found](Outpost::ObjectId, const Outpost::Device& _device)
      {
        const std::uint32_t cellX = static_cast<std::uint32_t>(_device.x >> Neuron::SUBUNITS_PER_CELL_SHIFT);
        const std::uint32_t cellY = static_cast<std::uint32_t>(_device.z >> Neuron::SUBUNITS_PER_CELL_SHIFT);
        const bool inside = cellX >= 40 && cellX < 43 && cellY >= 40 && cellY < 43;
        Assert::IsFalse(inside, L"a device does not spawn under its own factory");
        Assert::IsTrue(cellX >= 39 && cellX <= 43 && cellY >= 39 && cellY <= 43, L"and it is next to it");
        ++found;
      });
    Assert::AreEqual(1u, found);
  }

  TEST_METHOD(AFactoryAtTheDeviceCapPausesAndKeepsWhatItPaidFor)
  {
    Line line;
    Outpost::Seat& seat = line.sim.SeatAt(0);
    seat.powerHundredths = 13000;
    Assert::IsTrue(Outpost::SetProduction(line.sim, 0, line.factory, 0, 1));
    line.sim.Advance();
    const std::uint32_t remaining = line.sim.Objects().FindStructure(line.factory)->workRemainingTicks;
    Assert::IsTrue(remaining > 0, L"started, and paid for");

    seat.deviceCap = 0;
    for (std::uint32_t tick = 0; tick < 10; ++tick)
    {
      line.sim.Advance();
    }
    Assert::AreEqual(remaining, line.sim.Objects().FindStructure(line.factory)->workRemainingTicks, L"paused where it was");
    Assert::AreEqual(std::size_t{0}, line.sim.Objects().Count(Outpost::ObjectKind::Device));

    // Room again: it goes on from exactly where it stopped.
    seat.deviceCap = 10;
    for (std::uint32_t tick = 0; tick < remaining; ++tick)
    {
      line.sim.Advance();
    }
    Assert::AreEqual(std::size_t{1}, line.sim.Objects().Count(Outpost::ObjectKind::Device));
  }

  TEST_METHOD(CancelingGivesBackTheShareNotYetBuiltAndAFactoryModuleShortensTheTime)
  {
    Line line;
    Outpost::Seat& seat = line.sim.SeatAt(0);
    seat.powerHundredths = 13000;
    Assert::IsTrue(Outpost::SetProduction(line.sim, 0, line.factory, 0, 1));
    const std::uint32_t ticks = Outpost::BuildTimeTicksFor(13000);
    for (std::uint32_t tick = 0; tick < ticks / 4; ++tick)
    {
      line.sim.Advance();
    }
    seat.powerHundredths = 0;
    Assert::IsTrue(Outpost::CancelProduction(line.sim, 0, line.factory, 0));
    Assert::AreEqual(9750, seat.powerHundredths, L"three quarters of 130 power back");
    Assert::AreEqual(std::size_t{0}, seat.production.size());
    Assert::AreEqual(0u, line.sim.Objects().FindStructure(line.factory)->workRemainingTicks);

    // Two factory modules take half off, which is GameDesign.md §5's number read through §6's time.
    Outpost::Structure* factory = line.sim.Objects().FindStructure(line.factory);
    factory->modules[0] = 0;
    factory->modules[1] = 0;
    factory->moduleCount = 2;
    seat.powerHundredths = 13000;
    Assert::IsTrue(Outpost::SetProduction(line.sim, 0, line.factory, 0, 1));
    for (std::uint32_t tick = 0; tick < ticks / 2; ++tick)
    {
      line.sim.Advance();
    }
    Assert::AreEqual(std::size_t{1}, line.sim.Objects().Count(Outpost::ObjectKind::Device), L"half the time with two modules");
  }

  TEST_METHOD(AnUpgradeResearchedLaterReachesWhatIsAlreadyInTheField)
  {
    // The whole reason nothing derived is stored on a device (Sim/Design.h): the seat's upgrades
    // are read wherever a statistic is wanted, so a device built an hour ago answers the new
    // number without anything walking the world to tell it.
    Line line;
    Outpost::Seat& seat = line.sim.SeatAt(0);
    Outpost::DesignStats before{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 0, before) == Outpost::DesignFault::None);
    Assert::AreEqual(100, before.hitPoints);
    Assert::AreEqual(5, before.kineticArmor);

    seat.upgrades.chassisHitPointPercent[static_cast<std::size_t>(Outpost::ChassisClass::Light)] = 25;
    seat.upgrades.chassisArmorPercent[static_cast<std::size_t>(Outpost::ChassisClass::Light)] = 20;
    Outpost::DesignStats after{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 0, after) == Outpost::DesignFault::None);
    Assert::AreEqual(125, after.hitPoints, L"a quarter more");
    Assert::AreEqual(6, after.kineticArmor, L"and a fifth more armour");
  }

  TEST_METHOD(EightRanksWithThresholdsThatDouble)
  {
    Assert::AreEqual(std::uint8_t{8}, Outpost::RANK_COUNT);
    Assert::AreEqual(std::uint8_t{0}, Outpost::RankOf(0));
    Assert::AreEqual(std::uint8_t{0}, Outpost::RankOf(1));
    Assert::AreEqual(std::uint8_t{1}, Outpost::RankOf(2));
    Assert::AreEqual(std::uint8_t{4}, Outpost::RankOf(39));
    Assert::AreEqual(std::uint8_t{5}, Outpost::RankOf(40));
    Assert::AreEqual(std::uint8_t{7}, Outpost::RankOf(160));
    Assert::AreEqual(std::uint8_t{7}, Outpost::RankOf(1000000), L"the eighth is the last");
    Assert::AreEqual(0, Outpost::RANK_ACCURACY_PERCENT[0], L"a recruit gets nothing");
    Assert::AreEqual(24, Outpost::RANK_DAMAGE_PERCENT[Outpost::RANK_COUNT - 1]);
  }

  TEST_METHOD(AQueueSurvivesASnapshotBecauseItIsStateAndNotSomethingRebuilt)
  {
    Line line;
    line.sim.SeatAt(0).powerHundredths = 100000;
    line.sim.SeatAt(0).upgrades.chassisHitPointPercent[0] = 25;
    Assert::IsTrue(Outpost::SetProduction(line.sim, 0, line.factory, 0, 3));
    line.sim.Advance();
    const std::uint64_t hash = line.sim.ComputeHash();

    const Outpost::Sim reloaded = Reload(line.sim);
    Assert::AreEqual(hash, reloaded.ComputeHash(), L"the queue and the upgrades are in the stream and in the hash");
    Assert::AreEqual(std::size_t{1}, reloaded.Seats()[0].production.size());
    Assert::AreEqual(3u, reloaded.Seats()[0].production.front().remaining);
    Assert::AreEqual(25, reloaded.Seats()[0].upgrades.chassisHitPointPercent[0]);
  }
};

} // namespace SimTests
