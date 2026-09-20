#include "pch.h"

#include "ClusterGraph.h"
#include "Construction.h"
#include "Damage.h"
#include "Design.h"
#include "Experience.h"
#include "Movement.h"
#include "Production.h"
#include "Sim.h"
#include "Targeting.h"
#include "Weapons.h"

#include "FixedPoint.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Combat as GameDesign.md §8 writes it (m1-vertical-slice/S10): what a weapon may shoot at, what
// the stances change, what a shell does between the muzzle and the ground, what a kill is worth,
// and who leaves the fight. The formula itself is DamageTests'; what is measured here is the
// simulation applying it - the rules that need a world, a fog of war and a tick to be true or false.
namespace SimTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;
constexpr std::uint32_t SAMPLES = Outpost::SAMPLES_PER_CELL_EDGE;

enum class Row : std::uint32_t
{
  CommandPost = 0,
  Tower = 1,
  RepairBay = 2
};

enum class Chassis : std::uint32_t
{
  Light = 0
};

enum class Drive : std::uint32_t
{
  Wheels = 0,
  Tracks = 1
};

enum class Module : std::uint32_t
{
  MachineGun = 0,
  Mortar = 1,
  Repair = 2
};

/// The shipped rows the design works through, plus the two M1's content does not ship and this
/// suite needs: a tower, which carries a weapon and no stances, and a repair bay, which is the
/// stub the acceptance asks the retreat rule to be tested against.
const Outpost::ContentTree& Tables()
{
  static const Outpost::ContentTree TREE = []
  {
    Outpost::ContentTree tree;

    Outpost::StructureDesc post{};
    post.id = "CommandPost";
    post.role = Outpost::StructureRole::CommandPost;
    post.strength = Outpost::StrengthClass::Hard;
    post.footprintCellsX = 3;
    post.footprintCellsY = 3;
    post.hitPoints = 1500;
    post.kineticArmor = 20;
    post.thermalArmor = 18;
    post.costHundredths = 50000;
    post.buildTimeTicks = 1200;
    post.sightSubunits = 12 * CELL;
    Outpost::StructureDesc tower{};
    tower.id = "Tower";
    tower.role = Outpost::StructureRole::Tower;
    tower.strength = Outpost::StrengthClass::Medium;
    tower.footprintCellsX = 1;
    tower.footprintCellsY = 1;
    tower.hitPoints = 400;
    tower.kineticArmor = 10;
    tower.thermalArmor = 8;
    tower.costHundredths = 20000;
    tower.buildTimeTicks = 600;
    tower.sightSubunits = 20 * CELL;
    tower.weapon = "MachineGun";
    Outpost::StructureDesc bay{};
    bay.id = "RepairBay";
    bay.role = Outpost::StructureRole::RepairBay;
    bay.strength = Outpost::StrengthClass::Medium;
    bay.footprintCellsX = 2;
    bay.footprintCellsY = 2;
    bay.hitPoints = 500;
    bay.costHundredths = 25000;
    bay.buildTimeTicks = 600;
    bay.sightSubunits = 12 * CELL;
    bay.repairHitPointsHundredthsPerTick = 75;
    tree.structures.structures = {post, tower, bay};

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
    tree.components.chassis = {light};

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
    gun.shotsPerSalvo = 1;
    gun.reloadTicks = 5;
    gun.fireKind = Outpost::FireKind::Direct;
    gun.shortRangeSubunits = 8 * CELL;
    gun.longRangeSubunits = 12 * CELL;
    gun.shortHitPercent = 80;
    gun.longHitPercent = 50;
    Outpost::ModuleDesc mortar{};
    mortar.id = "Mortar";
    mortar.systemKind = Outpost::SystemKind::None;
    mortar.weaponClass = Outpost::WeaponClass::Artillery;
    mortar.costHundredths = 11000;
    mortar.damage = 80;
    mortar.shotsPerSalvo = 1;
    mortar.reloadTicks = 80;
    mortar.fireKind = Outpost::FireKind::Indirect;
    mortar.minimumRangeSubunits = 6 * CELL;
    mortar.shortRangeSubunits = 6 * CELL;
    mortar.longRangeSubunits = 28 * CELL;
    mortar.shortHitPercent = 100;
    mortar.longHitPercent = 100;
    mortar.splashSubunits = 2 * CELL;
    Outpost::ModuleDesc repair{};
    repair.id = "Repair";
    repair.systemKind = Outpost::SystemKind::Repair;
    repair.costHundredths = 5000;
    repair.systemRangeSubunits = 4 * CELL;
    repair.repairHitPointsHundredthsPerTick = 75;
    tree.components.modules = {gun, mortar, repair};

    // The version-1 matrix of GameDesign.md §8; DamageTests pins the arithmetic against it.
    tree.damage.modifierPercent[static_cast<std::size_t>(Outpost::WeaponClass::AntiLight)] = {120, 100, 50, 110, 130, 100, 120, 60, 30, 20};
    tree.damage.modifierPercent[static_cast<std::size_t>(Outpost::WeaponClass::AntiTank)] = {90, 100, 120, 90, 70, 60, 80, 100, 110, 60};
    tree.damage.modifierPercent[static_cast<std::size_t>(Outpost::WeaponClass::Flame)] = {130, 110, 70, 120, 140, 40, 150, 80, 40, 10};
    tree.damage.modifierPercent[static_cast<std::size_t>(Outpost::WeaponClass::Artillery)] = {100, 100, 100, 100, 100,
                                                                                              20,  130, 120, 100, 60};
    tree.damage.modifierPercent[static_cast<std::size_t>(Outpost::WeaponClass::Energy)] = {100, 100, 100, 100, 100, 100, 100, 100, 100, 80};
    tree.damage.armorFactorPercent = {100, 100, 100, 0, 50};
    tree.damage.armorKind = {Outpost::ArmorKind::Kinetic, Outpost::ArmorKind::Kinetic, Outpost::ArmorKind::Thermal,
                             Outpost::ArmorKind::Kinetic, Outpost::ArmorKind::Kinetic};
    return tree;
  }();
  return TREE;
}

Outpost::MatchSettings TwoSeats()
{
  Outpost::MatchSettings settings{};
  settings.seed = 11;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::Medium;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = Outpost::VictoryCondition::Annihilation;
  settings.seats[0] = {Outpost::SeatKind::Human, 0};
  settings.seats[1] = {Outpost::SeatKind::Human, 1};
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

/// Level ground where the fight happens, so that a ridge between two devices is never the reason
/// one of them did not fire: what is under test is the rules and not the heightfield.
[[nodiscard]] bool Level(Outpost::Sim& _sim, std::uint32_t _cellX, std::uint32_t _cellY, std::uint32_t _cells, std::int16_t _height)
{
  Outpost::HeightDelta delta{};
  delta.x = _cellX * SAMPLES;
  delta.y = _cellY * SAMPLES;
  delta.width = _cells * SAMPLES + 1;
  delta.height = _cells * SAMPLES + 1;
  delta.heights.assign(static_cast<std::size_t>(delta.width) * delta.height, _height);
  return _sim.FlattenTerrain(delta);
}

[[nodiscard]] std::int32_t Middle(std::uint32_t _cell)
{
  return static_cast<std::int32_t>(_cell) * CELL + CELL / 2;
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

Outpost::ObjectId Spawn(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _design, std::int32_t _x, std::int32_t _z)
{
  Outpost::DesignStats stats{};
  Assert::IsTrue(Outpost::DeriveSeatDesign(_sim.Seats()[_seat], Tables(), _design, stats) == Outpost::DesignFault::None);
  Outpost::Device device{};
  device.seat = _seat;
  device.design = _design;
  device.x = _x;
  device.z = _z;
  device.y = Outpost::GroundHeightSubunits(_sim.Terrain(), _x, _z);
  device.hitPoints = stats.hitPoints;
  device.primaryOrder = Outpost::PrimaryOrder::Stop;
  device.target = Outpost::NO_OBJECT;
  device.destinationX = _x;
  device.destinationZ = _z;
  device.anchorX = _x;
  device.anchorZ = _z;
  device.fire = Outpost::FireStance::FireAtWill;
  device.range = Outpost::RangeStance::Optimal;
  device.retreat = Outpost::RetreatStance::Never;
  device.movement = Outpost::MovementStance::HoldPosition;
  return _sim.Objects().Create(device);
}

Outpost::ObjectId Standing(Outpost::Sim& _sim, std::uint8_t _seat, Row _row, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = static_cast<std::uint32_t>(_row);
  structure.cellX = _cellX;
  structure.cellY = _cellY;
  structure.state = Outpost::StructurePhase::Standing;
  structure.hitPoints = Tables().structures.structures[static_cast<std::size_t>(_row)].hitPoints;
  structure.buildEffortHundredths = Outpost::RequiredEffortHundredths(600);
  structure.moduleUnderConstruction = Outpost::NO_STRUCTURE_MODULE;
  structure.working = Outpost::NO_OBJECT;
  return _sim.Objects().Create(structure);
}

const Outpost::Device* DeviceAt(const Outpost::Sim& _sim, Outpost::ObjectId _id)
{
  return _sim.Objects().FindDevice(_id);
}

/// A match on level ground with the three designs saved for both commanders.
struct Field
{
  Outpost::Sim sim{TwoSeats(), Tables()};

  Field()
  {
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Assert::IsTrue(Level(sim, 50, 50, 40, 40));
    for (std::uint8_t seat = 0; seat < 2; ++seat)
    {
      Assert::IsTrue(Outpost::SaveDesign(sim, seat, 0, Designed(Chassis::Light, Drive::Wheels, Module::MachineGun)));
      Assert::IsTrue(Outpost::SaveDesign(sim, seat, 1, Designed(Chassis::Light, Drive::Wheels, Module::Mortar)));
      Assert::IsTrue(Outpost::SaveDesign(sim, seat, 2, Designed(Chassis::Light, Drive::Wheels, Module::Repair)));
    }
  }
};

} // namespace

TEST_CLASS(CombatTests)
{
public:
  TEST_METHOD(AMachineGunAcquiresWhatItCanSeeFiresAtItAndKillsIt)
  {
    Field field;
    const Outpost::ObjectId mine = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    const Outpost::ObjectId theirs = Spawn(field.sim, 1, 0, Middle(64), Middle(60));

    std::uint32_t died = 0;
    for (std::uint32_t tick = 0; tick < 600 && died == 0; ++tick)
    {
      field.sim.Advance();
      if (DeviceAt(field.sim, theirs) == nullptr || DeviceAt(field.sim, mine) == nullptr)
      {
        died = field.sim.Tick();
      }
    }
    Logger::WriteMessage((L"measured: two scouts four cells apart settled it on tick " + std::to_wstring(died)).c_str());
    Assert::IsTrue(died > 0, L"one of them died");
    // A scout is 100 hit points and a machine gun deals 4 through 5 armour at 80 percent every
    // five ticks, so a one-sided kill is about 160 ticks and a mutual one rather less.
    Assert::IsTrue(died > 100 && died < 400, L"in about the time the tables allow");
    Assert::IsTrue(field.sim.Objects().Count(Outpost::ObjectKind::Wreck) > 0, L"and left a wreck");
  }

  TEST_METHOD(EveryTriggerPullIsRecordedForTheTickThatPulledItAndForNoOther)
  {
    // m1-vertical-slice/C9's producer. A commander learns that a shot happened from an Event and
    // from nothing else (TechnicalDesign.md §5.3), and DIRECT fire leaves no other trace in the
    // simulation at all: the hit is decided at the trigger and no Projectile is kept, deliberately.
    // So a shot that is not on this list is invisible to everybody, the commander who fired it
    // included, and there is nothing downstream that could notice.
    Field field;
    const Outpost::ObjectId gunner = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    const Outpost::ObjectId victim = Spawn(field.sim, 1, 2, Middle(64), Middle(60)); // Repair: it never fires back

    std::uint32_t firedOn = 0;
    Outpost::SimShot first{};
    for (std::uint32_t tick = 0; tick < 200 && firedOn == 0; ++tick)
    {
      field.sim.Advance();
      if (!field.sim.Shots().empty())
      {
        firedOn = field.sim.Tick();
        first = field.sim.Shots().front();
        Assert::AreEqual(static_cast<std::size_t>(1), field.sim.Shots().size(), L"one trigger pull is one record");
      }
    }
    Assert::IsTrue(firedOn > 0, L"the gun fired inside two hundred ticks");
    Assert::IsTrue(first.shooter == gunner, L"the record names who fired");
    Assert::IsTrue(first.target == victim, L"and what it was aimed at");
    Assert::AreEqual(static_cast<std::uint32_t>(Module::MachineGun), first.module, L"and which weapon row it came from");
    Assert::AreEqual(firedOn, first.tick, L"and the tick the trigger was pulled on");
    Assert::AreEqual(Middle(64), first.x, L"and the point aimed at, in the simulation's own subunits");
    Assert::AreEqual(Middle(60), first.z);
    Assert::AreEqual(std::uint8_t{0}, first.seat, L"and whose shot it was, which is what the interest filter above Sim reads");

    // SCRATCH AND NOT STATE, which is the half a producer usually gets wrong. The gun reloads for
    // five ticks, so the tick straight after a shot pulls no trigger and the list has to be empty
    // again; a list that accumulated would have the host republishing the opening shot of the
    // match for the rest of it.
    field.sim.Advance();
    Assert::IsTrue(field.sim.Shots().empty(), L"the tick that fired nothing left nothing behind");
  }

  TEST_METHOD(AShellIsRecordedTheTickItLeavesTheBarrelAndNotTheTickItLands)
  {
    // The other fire kind, because the record is written BEFORE Fire branches on it and a mutation
    // that moved it inside the direct branch would pass the case above. Indirect fire does leave a
    // Projectile, so it is the one shot a client could in principle infer - but it would infer it
    // from a position the wire never carries, and it would see the shell appear rather than the
    // barrel fire.
    Field field;
    Assert::IsTrue(Level(field.sim, 20, 55, 60, 40));
    const Outpost::ObjectId mortar = Spawn(field.sim, 0, 1, Middle(40), Middle(60));
    const Outpost::ObjectId distant = Spawn(field.sim, 1, 2, Middle(64), Middle(60));
    (void)Spawn(field.sim, 0, 2, Middle(56), Middle(60)); // The spotter twenty-four cells out needs

    std::uint32_t recordedOn = 0;
    std::uint32_t shellsOn = 0;
    Outpost::SimShot shell{};
    for (std::uint32_t tick = 0; tick < 300 && shellsOn == 0; ++tick)
    {
      field.sim.Advance();
      if (recordedOn == 0 && !field.sim.Shots().empty())
      {
        recordedOn = field.sim.Tick();
        shell = field.sim.Shots().front();
      }
      if (field.sim.Objects().Count(Outpost::ObjectKind::Projectile) > 0)
      {
        shellsOn = field.sim.Tick();
      }
    }
    Assert::IsTrue(recordedOn > 0, L"the mortar fired and said so");
    Assert::AreEqual(recordedOn, shellsOn, L"on the same tick the shell came into being, not when it lands");
    Assert::IsTrue(shell.shooter == mortar);
    Assert::IsTrue(shell.target == distant, L"aimed at what the spotter found");
    Assert::AreEqual(static_cast<std::uint32_t>(Module::Mortar), shell.module);
    Assert::AreEqual(recordedOn, shell.tick);
  }

  TEST_METHOD(ARankIsEarnedByKillsWeightedByWhatTheyCost)
  {
    // The weighting of GameLogic/Experience.h, against the reference device it is defined by.
    Assert::AreEqual(1u, Outpost::ExperienceForKill(13000), L"the scout GameDesign.md §6 sizes by");
    Assert::AreEqual(1u, Outpost::ExperienceForKill(6000), L"and anything cheaper still counts");
    Assert::AreEqual(3u, Outpost::ExperienceForKill(50000), L"a command post is worth three");
    Assert::AreEqual(0u, static_cast<std::uint32_t>(Outpost::RankOf(1)));
    Assert::AreEqual(1u, static_cast<std::uint32_t>(Outpost::RankOf(2)));

    // And the killer is the one that gets it: a lone gunner against a target that cannot answer.
    Field field;
    const Outpost::ObjectId gunner = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    (void)Spawn(field.sim, 1, 2, Middle(64), Middle(60)); // A repair device carries no weapon
    for (std::uint32_t tick = 0; tick < 600; ++tick)
    {
      field.sim.Advance();
      if (field.sim.Objects().Count(Outpost::ObjectKind::Device) == 1)
      {
        break;
      }
    }
    Assert::IsTrue(DeviceAt(field.sim, gunner) != nullptr, L"the unarmed one was the one that died");
    Assert::AreEqual(1u, DeviceAt(field.sim, gunner)->experience, L"and the kill was worth one");
  }

  TEST_METHOD(HoldFireNeverShootsAndReturnFireAnswersWhatShotAtIt)
  {
    Field field;
    const Outpost::ObjectId quiet = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    const Outpost::ObjectId other = Spawn(field.sim, 1, 0, Middle(64), Middle(60));
    field.sim.Objects().FindDevice(quiet)->fire = Outpost::FireStance::HoldFire;
    field.sim.Objects().FindDevice(other)->fire = Outpost::FireStance::HoldFire;
    for (std::uint32_t tick = 0; tick < 200; ++tick)
    {
      field.sim.Advance();
    }
    Assert::IsTrue(DeviceAt(field.sim, quiet) != nullptr && DeviceAt(field.sim, other) != nullptr, L"neither fired a shot");
    Assert::IsTrue(DeviceAt(field.sim, quiet)->target == Outpost::NO_OBJECT, L"and hold fire acquires nothing");

    // Return fire takes no target of its own, and takes one the moment it is shot at.
    field.sim.Objects().FindDevice(quiet)->fire = Outpost::FireStance::ReturnFire;
    field.sim.Advance();
    Assert::IsTrue(DeviceAt(field.sim, quiet)->target == Outpost::NO_OBJECT, L"return fire goes looking for nobody");
    field.sim.Objects().FindDevice(other)->fire = Outpost::FireStance::FireAtWill;
    for (std::uint32_t tick = 0; tick < 60; ++tick)
    {
      field.sim.Advance();
    }
    Assert::IsTrue(DeviceAt(field.sim, quiet) == nullptr || DeviceAt(field.sim, quiet)->target == other,
                   L"and answers the one that shot at it");
  }

  TEST_METHOD(ADeviceWithNoOrderClosesOnWhatItCanSeeAndCannotShoot)
  {
    // THE OWNER'S RULING OF 2026-09-20 on m1-vertical-slice/S14. "Optimal range" means fight at
    // your best range: a machine gun acquires at twelve cells and opens at eight, and a device
    // that stood in the gap watching an enemy it was not allowed to shoot at was doing what the
    // code said and not what the stance means. Measured before the ruling: two scripted armies
    // stared at each other from ten cells for twenty thousand ticks.
    //
    // The fixture's gun is eight cells short and twelve long, so ten cells is inside acquisition
    // and outside fire - the gap this rule exists for.
    Field field;
    const Outpost::ObjectId gunner = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    const Outpost::ObjectId quarry = Spawn(field.sim, 1, 2, Middle(70), Middle(60)); // Unarmed, so it never shoots back
    Assert::IsTrue(DeviceAt(field.sim, gunner)->primaryOrder == Outpost::PrimaryOrder::Stop, L"no order of its own");
    Assert::IsTrue(DeviceAt(field.sim, gunner)->movement == Outpost::MovementStance::HoldPosition,
                   L"and Spawn holds position, which is what the second half of this case needs");

    // HOLD POSITION MEANS HOLD, and that is the safety on the rule: a device a commander put
    // somewhere deliberately does not wander off to start a fight.
    const std::int32_t startedAt = DeviceAt(field.sim, gunner)->x;
    for (std::uint32_t tick = 0; tick < 120; ++tick)
    {
      field.sim.Advance();
    }
    Assert::IsTrue(DeviceAt(field.sim, gunner)->target != Outpost::NO_OBJECT, L"it can see it at ten cells");
    Assert::AreEqual(startedAt, DeviceAt(field.sim, gunner)->x, L"and holds its ground, because it was told to");
    Assert::IsTrue(DeviceAt(field.sim, gunner)->primaryOrder == Outpost::PrimaryOrder::Stop,
                   L"and is standing still rather than merely stationary: the ORDER is what a commander reads, and what a "
                   L"scripted seat counts an idle fighter by");

    // Pursue, and it closes until its gun opens and then fires.
    field.sim.Objects().FindDevice(gunner)->movement = Outpost::MovementStance::Pursue;
    bool fired = false;
    for (std::uint32_t tick = 0; tick < 400 && !fired; ++tick)
    {
      field.sim.Advance();
      fired = !field.sim.Shots().empty();
    }
    Assert::IsTrue(fired, L"it closed into its own short band and opened fire");
    const auto closedTo = static_cast<std::int32_t>(DeviceAt(field.sim, gunner)->x);
    Assert::IsTrue(closedTo > startedAt, L"having walked toward it");
    // AND IT CHASES A DEVICE TO WHERE IT STANDS, which is what separates a device from a building:
    // a structure is held off at the edge of the gun's band because its own cells are impassable
    // (the case below), and a device is run down because it will not be there in a moment. A
    // chaser that held off from something that RUNS would never catch anything.
    Assert::AreEqual(DeviceAt(field.sim, quarry)->x, DeviceAt(field.sim, gunner)->destinationX,
                     L"its destination is the quarry itself and not a stand-off point short of it");
    Assert::IsTrue(DeviceAt(field.sim, gunner)->primaryOrder == Outpost::PrimaryOrder::AttackMove, L"on the order it gave itself");
    Logger::WriteMessage(
      ("    measured: a gunner ten cells out closed to " + std::to_string((Middle(70) - closedTo) / CELL) + " cells and fired\n").c_str());

    // AND IT STANDS AGAIN WHEN THERE IS NOTHING LEFT TO CLOSE ON, which is why the order it gives
    // itself is an AttackMove and not an Attack: Arrive turns an AttackMove back into Stop when it
    // gets there, so a device that closed and then killed what it came for is idle again - and a
    // scripted commander counts its idle fighters by exactly that (GameLogic/AiBlackboard.cpp).
    for (std::uint32_t tick = 0;
         tick < 900 && DeviceAt(field.sim, gunner) != nullptr && field.sim.Objects().Count(Outpost::ObjectKind::Device) > 1; ++tick)
    {
      field.sim.Advance();
    }
    Assert::AreEqual(std::size_t{1}, field.sim.Objects().Count(Outpost::ObjectKind::Device), L"it killed what it closed on");
    Assert::IsTrue(DeviceAt(field.sim, gunner)->primaryOrder == Outpost::PrimaryOrder::Stop,
                   L"and is standing still again, on no order at all");
  }

  TEST_METHOD(ADeviceAlreadyInRangeStandsAndShootsRatherThanWalkingIn)
  {
    // The other half of the rule: closing is for a target that is TOO FAR, not for one it cannot
    // hit for any reason. A device that can already fire has no business walking into a knife
    // fight, and a mortar whose target is inside its MINIMUM range would make things worse by
    // closing - which is why the flag is "too far for every weapon" and not "fired nothing".
    Field field;
    const Outpost::ObjectId gunner = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    (void)Spawn(field.sim, 1, 2, Middle(64), Middle(60)); // Four cells: inside the gun's short band
    field.sim.Objects().FindDevice(gunner)->movement = Outpost::MovementStance::Pursue;
    const std::int32_t startedAt = DeviceAt(field.sim, gunner)->x;
    bool fired = false;
    for (std::uint32_t tick = 0; tick < 60; ++tick)
    {
      field.sim.Advance();
      fired = fired || !field.sim.Shots().empty();
    }
    Assert::IsTrue(fired, L"it fired from where it stood");
    Assert::AreEqual(startedAt, DeviceAt(field.sim, gunner)->x, L"and did not close on something it could already hit");
    Assert::IsTrue(DeviceAt(field.sim, gunner)->primaryOrder == Outpost::PrimaryOrder::Stop,
                   L"nor gave itself an order to: a device already in range has nothing to advance on");
  }

  TEST_METHOD(ACommandersOrderOutranksWhatTheWeaponWouldRatherDo)
  {
    // The other safety on the closing rule: only a device with NO order gives itself one. A Move,
    // a Patrol or a Guard is the commander's, and a unit that abandoned the route it was sent on
    // because it glimpsed something would be a unit nobody could position.
    Field field;
    const Outpost::ObjectId gunner = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    (void)Spawn(field.sim, 1, 2, Middle(70), Middle(60)); // Ten cells: seen, and outside the gun's band
    field.sim.Objects().FindDevice(gunner)->movement = Outpost::MovementStance::Pursue;

    // Sent the OTHER WAY, away from what it can see.
    Outpost::Order away{};
    away.seat = 0;
    away.kind = Outpost::OrderKind::Move;
    away.operands[0] = static_cast<std::int32_t>(gunner.value);
    away.operands[1] = Middle(50);
    away.operands[2] = Middle(60);
    field.sim.Submit(away);
    // EVERY TICK OF THE JOURNEY, and not a reading at the end of it. The device holds a target
    // only while the enemy is inside its weapon's reach - twelve cells - and it is walking out of
    // that, correctly; what has to be true is that on every tick it HAD one, it was still carrying
    // the commander's order rather than an AttackMove it wrote for itself.
    std::uint32_t ticksHoldingATarget = 0;
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      field.sim.Advance();
      const Outpost::Device* walking = DeviceAt(field.sim, gunner);
      Assert::IsTrue(walking != nullptr, L"it walked away from a fight rather than into one");
      if (walking->target != Outpost::NO_OBJECT)
      {
        ++ticksHoldingATarget;
        Assert::IsTrue(walking->primaryOrder == Outpost::PrimaryOrder::Move,
                       L"a device with a commander's order keeps it, whatever it can see");
      }
    }
    Assert::IsTrue(ticksHoldingATarget > 0, L"it did hold a target it could not shoot, or this asserts nothing");
    Assert::IsTrue(DeviceAt(field.sim, gunner)->x < Middle(60), L"and walked away from it, as it was told to");
  }

  TEST_METHOD(AMortarNeedsASpotterAndMissesWhatWalksOutFromUnderIt)
  {
    {
      // Twenty-four cells away: inside the mortar's twenty-eight and well outside the twenty its
      // chassis sees, so nothing of its commander's is looking and it must hold (GameDesign.md §8).
      Field field;
      Assert::IsTrue(Level(field.sim, 20, 55, 60, 40));
      const Outpost::ObjectId mortar = Spawn(field.sim, 0, 1, Middle(40), Middle(60));
      const Outpost::ObjectId distant = Spawn(field.sim, 1, 2, Middle(64), Middle(60));
      for (std::uint32_t tick = 0; tick < 200; ++tick)
      {
        field.sim.Advance();
      }
      Assert::IsTrue(DeviceAt(field.sim, mortar)->target == Outpost::NO_OBJECT, L"no spotter, no target");
      Assert::AreEqual(std::size_t{0}, field.sim.Objects().Count(Outpost::ObjectKind::Projectile), L"and no shell");
      Assert::IsTrue(DeviceAt(field.sim, distant) != nullptr);

      // Give it a spotter - anything of its commander's with the target in its own sight - and the
      // same shot becomes legal.
      (void)Spawn(field.sim, 0, 2, Middle(56), Middle(60));
      bool fired = false;
      for (std::uint32_t tick = 0; tick < 200 && !fired; ++tick)
      {
        field.sim.Advance();
        fired = field.sim.Objects().Count(Outpost::ObjectKind::Projectile) > 0 || DeviceAt(field.sim, distant) == nullptr;
      }
      Assert::IsTrue(fired, L"with a spotter it fires");
    }
    {
      // A shell is aimed at the ground the target stood on, and the arithmetic of the miss is the
      // flight against the splash: at twenty-four cells a shell is thirty-two ticks in the air, a
      // scout covers two and a half cells in that time, and the splash is two. So it misses - but
      // only at range. A mortar firing at ten cells leads its target by less than the splash and
      // connects, which is correct and is why this fires from near its maximum.
      Field field;
      Assert::IsTrue(Level(field.sim, 36, 40, 70, 40));
      const Outpost::ObjectId mortar = Spawn(field.sim, 0, 1, Middle(40), Middle(60));
      // The runner starts on the first COLUMN of a cluster, which is where the graph puts the
      // doorways between one cluster and the one above it (GameLogic/ClusterGraph.cpp), so its route
      // north is a straight line. Half a cluster to the side it would detour ten cells west to
      // reach a doorway - toward the mortar - and a shell fired over fourteen cells leads its
      // target by less than the splash and connects. That is correct behaviour and it is not what
      // this test is about, so the run is straight and the assertion below says so.
      const std::uint32_t column = 4 * Outpost::CLUSTER_CELLS;
      const Outpost::ObjectId runner = Spawn(field.sim, 1, 0, Middle(column), Middle(60));
      field.sim.Objects().FindDevice(runner)->fire = Outpost::FireStance::HoldFire;

      const std::int32_t started = DeviceAt(field.sim, runner)->hitPoints;
      // Across the line of fire rather than along it, so that it stays about twenty-four cells out
      // and goes on being shot at instead of walking out of range after one shell.
      Outpost::Order order{};
      order.seat = 1;
      order.kind = Outpost::OrderKind::Move;
      order.operands[0] = static_cast<std::int32_t>(runner.value);
      order.operands[1] = Middle(column);
      order.operands[2] = Middle(100);
      field.sim.Submit(order);
      // Up to speed before anything can see it: a scout still turning is a stationary target, and
      // what is under test is the shell that leads a moving one.
      for (std::uint32_t tick = 0; tick < 30; ++tick)
      {
        field.sim.Advance();
      }
      Assert::AreEqual(started, DeviceAt(field.sim, runner)->hitPoints, L"nothing has seen it yet");
      (void)Spawn(field.sim, 0, 2, Middle(column - 4), Middle(72)); // The spotter, beside its path

      bool fired = false;
      for (std::uint32_t tick = 0; tick < 300; ++tick)
      {
        field.sim.Advance();
        fired = fired || field.sim.Objects().Count(Outpost::ObjectKind::Projectile) > 0;
      }
      Assert::IsTrue(fired, L"shells were in the air");
      Assert::IsTrue(DeviceAt(field.sim, runner) != nullptr, L"it is still alive");
      Assert::AreEqual(started, DeviceAt(field.sim, runner)->hitPoints, L"and untouched: it walked out from under every shell");
      Assert::IsTrue(std::abs(DeviceAt(field.sim, runner)->x - Middle(column)) < CELL,
                     L"and it really did run straight, which is what the arithmetic above assumes");
      Assert::IsTrue(DeviceAt(field.sim, runner)->z > Middle(70), L"and covered the ground");
      Assert::IsTrue(DeviceAt(field.sim, mortar) != nullptr);
    }
  }

  TEST_METHOD(ATowerFiresWithNoStancesAndTakesTheNearestDeviceFirst)
  {
    Field field;
    const Outpost::ObjectId tower = Standing(field.sim, 0, Row::Tower, 60, 60);
    // A structure of theirs nearer than a device of theirs: devices come first all the same,
    // because a thing that shoots back is the thing that has to stop (GameLogic/Targeting.h).
    (void)Standing(field.sim, 1, Row::CommandPost, 62, 60);
    const Outpost::ObjectId device = Spawn(field.sim, 1, 2, Middle(66), Middle(60));
    field.sim.Advance(); // Stage 7 stamps the tower's sight before stage 8 reads it
    field.sim.Advance();

    const std::int32_t started = DeviceAt(field.sim, device)->hitPoints;
    for (std::uint32_t tick = 0; tick < 120; ++tick)
    {
      field.sim.Advance();
    }
    Assert::IsTrue(field.sim.Objects().FindStructure(tower) != nullptr, L"the tower stands");
    Assert::IsTrue(DeviceAt(field.sim, device) == nullptr || DeviceAt(field.sim, device)->hitPoints < started,
                   L"and shot the device rather than the nearer building");
  }

  TEST_METHOD(RetreatBreaksOffOnlyWhenARepairPointIsWithinSixtyCells)
  {
    Field field;
    const Outpost::ObjectId hurt = Spawn(field.sim, 0, 0, Middle(60), Middle(60));
    field.sim.Objects().FindDevice(hurt)->retreat = Outpost::RetreatStance::AtHalf;
    field.sim.Objects().FindDevice(hurt)->hitPoints = 40; // Under half of a scout's hundred

    // Nothing to go to: it holds and fights, which GameDesign.md §8 asks for by name.
    field.sim.Advance();
    Assert::IsTrue(DeviceAt(field.sim, hurt)->primaryOrder != Outpost::PrimaryOrder::ReturnToRepair, L"nowhere to go, so it stays");

    // A bay beyond sixty cells is still nowhere to go.
    (void)Standing(field.sim, 0, Row::RepairBay, 125, 125);
    field.sim.Objects().FindDevice(hurt)->hitPoints = 40;
    field.sim.Advance();
    Assert::IsTrue(DeviceAt(field.sim, hurt)->primaryOrder != Outpost::PrimaryOrder::ReturnToRepair, L"sixty cells is the rule");

    // One within sixty is, and the device breaks off toward it.
    const Outpost::ObjectId bay = Standing(field.sim, 0, Row::RepairBay, 70, 60);
    field.sim.Objects().FindDevice(hurt)->hitPoints = 40;
    field.sim.Advance();
    Assert::IsTrue(DeviceAt(field.sim, hurt)->primaryOrder == Outpost::PrimaryOrder::ReturnToRepair, L"and now it breaks off");
    Assert::IsTrue(DeviceAt(field.sim, hurt)->destinationX > Middle(60), L"toward the bay");
    Assert::IsTrue(field.sim.Objects().FindStructure(bay) != nullptr);

    // A device whose stance says never does not, however badly hurt.
    const Outpost::ObjectId stubborn = Spawn(field.sim, 0, 0, Middle(62), Middle(62));
    field.sim.Objects().FindDevice(stubborn)->hitPoints = 5;
    field.sim.Advance();
    Assert::IsTrue(DeviceAt(field.sim, stubborn) == nullptr ||
                     DeviceAt(field.sim, stubborn)->primaryOrder != Outpost::PrimaryOrder::ReturnToRepair,
                   L"never is never");
  }

  TEST_METHOD(TwoSimsGivenTheSameFightResolveItIdentically)
  {
    Field one;
    Field other;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
      const Outpost::ObjectId mine = Spawn(one.sim, 0, 0, Middle(58 + index), Middle(60));
      Assert::IsTrue(Spawn(other.sim, 0, 0, Middle(58 + index), Middle(60)) == mine);
      const Outpost::ObjectId theirs = Spawn(one.sim, 1, 0, Middle(66 + index), Middle(61));
      Assert::IsTrue(Spawn(other.sim, 1, 0, Middle(66 + index), Middle(61)) == theirs);
    }
    for (std::uint32_t tick = 0; tick < 400; ++tick)
    {
      one.sim.Advance();
      other.sim.Advance();
      Assert::AreEqual(one.sim.Hash(), other.sim.Hash());
    }
    Logger::WriteMessage((L"measured: of eight scouts, " + std::to_wstring(one.sim.Objects().Count(Outpost::ObjectKind::Device)) +
                          L" were left after 400 ticks")
                           .c_str());
    Assert::IsTrue(one.sim.Objects().Count(Outpost::ObjectKind::Device) < 8, L"and they actually fought");
  }
};

} // namespace SimTests
