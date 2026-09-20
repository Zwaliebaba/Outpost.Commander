#include "pch.h"

#include "Construction.h"
#include "Production.h"
#include "Research.h"
#include "Sim.h"

#include "FixedPoint.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Research (GameDesign.md §7; m1-vertical-slice/S6): labs in parallel, prerequisites, and upgrades
// that reach what is already standing. The one that is easy to get wrong and hard to notice is the
// last: a device at half health whose maximum grows must still be at half health, and a device
// built before the upgrade must answer the new number at all.
namespace SimTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;

enum class Row : std::uint32_t
{
  CommandPost = 0,
  ResearchLab = 1,
  Extractor = 2,
  Generator = 3
};

enum class Item : std::uint32_t
{
  Ballistics = 0,    ///< 50 power, no prerequisite
  LightArmor = 1,    ///< 30 power, no prerequisite: the cheapest, so auto-research takes it first
  LightHull = 2,     ///< 40 power, needs LightArmor
  Extraction = 3,    ///< 60 power, no prerequisite
  Fortification = 4, ///< 70 power, needs Ballistics
  Laboratories = 5   ///< 30 power, no prerequisite: ties LightArmor, and loses on the row index
};

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
    Outpost::StructureDesc lab{};
    lab.id = "ResearchLab";
    lab.role = Outpost::StructureRole::ResearchLab;
    lab.footprintCellsX = 2;
    lab.footprintCellsY = 2;
    lab.hitPoints = 600;
    lab.costHundredths = 30000;
    lab.moduleSlots = 1;
    lab.modules = {"LabModule"};
    Outpost::StructureDesc extractor{};
    extractor.id = "Extractor";
    extractor.role = Outpost::StructureRole::Extractor;
    extractor.footprintCellsX = 1;
    extractor.footprintCellsY = 1;
    extractor.hitPoints = 200;
    extractor.powerHundredthsPerTick = 25; // 5 power a second at 20 ticks
    Outpost::StructureDesc generator{};
    generator.id = "Generator";
    generator.role = Outpost::StructureRole::Generator;
    generator.footprintCellsX = 2;
    generator.footprintCellsY = 2;
    generator.hitPoints = 600;
    generator.servesExtractors = 4;
    generator.serviceRangeSubunits = 48 * CELL;
    tree.structures.structures = {post, lab, extractor, generator};

    Outpost::StructureModuleDesc labModule{};
    labModule.id = "LabModule";
    labModule.effect = Outpost::StructureModuleEffect::ShortenResearchTime;
    labModule.amount = 30; // GameDesign.md §5: a lab module takes 30 per cent off
    labModule.costHundredths = 15000;
    labModule.buildTimeTicks = 400;
    tree.structures.modules = {labModule};

    Outpost::ChassisDesc light{};
    light.id = "LightI";
    light.chassisClass = Outpost::ChassisClass::Light;
    light.hitPoints = 100;
    light.kineticArmor = 10;
    light.thermalArmor = 10;
    light.baseSpeedSubunitsPerTick = 1024;
    light.sightSubunits = 20 * CELL;
    light.costHundredths = 6000;
    light.mounts = 1;
    tree.components.chassis = {light};
    Outpost::DriveDesc wheels{};
    wheels.id = "Wheels";
    wheels.driveClass = Outpost::DriveClass::Wheels;
    wheels.speedFactorHundredths = 100;
    wheels.hitPointFactorHundredths = 100;
    wheels.costHundredths = 3000;
    tree.components.drives = {wheels};
    Outpost::ModuleDesc gun{};
    gun.id = "MachineGun";
    gun.systemKind = Outpost::SystemKind::None;
    gun.weaponClass = Outpost::WeaponClass::AntiLight;
    gun.costHundredths = 4000;
    gun.damage = 8;
    tree.components.modules = {gun};

    const auto item = [](const char* _id, std::int32_t _cost, std::uint32_t _ticks, Outpost::ResearchEffect _effect, std::uint8_t _target,
                         std::int32_t _percent, std::vector<std::string> _prerequisites)
    {
      Outpost::ResearchItemDesc row{};
      row.id = _id;
      row.costHundredths = _cost;
      row.timeTicks = _ticks;
      row.effect = _effect;
      row.targetClass = _target;
      row.upgradePercent = _percent;
      row.prerequisites = std::move(_prerequisites);
      return row;
    };
    tree.research = {
      item("Ballistics", 5000, 100, Outpost::ResearchEffect::Unlock, 0, 0, {}),
      item("LightArmor", 3000, 100, Outpost::ResearchEffect::ChassisArmor, static_cast<std::uint8_t>(Outpost::ChassisClass::Light), 20, {}),
      item("LightHull", 4000, 100, Outpost::ResearchEffect::ChassisHitPoints, static_cast<std::uint8_t>(Outpost::ChassisClass::Light), 50,
           {"LightArmor"}),
      item("Extraction", 6000, 100, Outpost::ResearchEffect::ExtractorRate, 0, 40, {}),
      item("Fortification", 7000, 100, Outpost::ResearchEffect::StructureHitPoints, 0, 100, {"Ballistics"}),
      item("Laboratories", 3000, 100, Outpost::ResearchEffect::Unlock, 0, 0, {}),
    };
    return tree;
  }();
  return TREE;
}

Outpost::MatchSettings TwoSeats(bool _autoResearch = false)
{
  Outpost::MatchSettings settings{};
  settings.seed = 17;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::High;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = Outpost::VictoryCondition::Annihilation;
  settings.seats[0] = {Outpost::SeatKind::Human, 0, _autoResearch};
  settings.seats[1] = {Outpost::SeatKind::Ai, 1, false};
  return settings;
}

Outpost::LandscapeDefinition Ground(std::vector<Outpost::CellPosition> _deposits = {})
{
  Outpost::LandscapeDefinition definition{};
  definition.version = Outpost::LANDSCAPE_DEFINITION_VERSION;
  definition.sizeClass = Outpost::SizeClass::Small;
  definition.cellsPerSide = Outpost::SIZE_CLASS_CELLS[0];
  definition.seed = 1;
  definition.palette = "Default";
  definition.tiles = {{0, 0, 512, 170, 90, 100, 48, 70, 1, 32, ""}};
  definition.starts = {{16, 16}, {100, 100}};
  definition.deposits = std::move(_deposits);
  return definition;
}

Outpost::ObjectId Standing(Outpost::Sim& _sim, std::uint8_t _seat, Row _row, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = static_cast<std::uint32_t>(_row);
  structure.cellX = _cellX;
  structure.cellY = _cellY;
  structure.state = Outpost::StructurePhase::Standing;
  structure.hitPoints = Tables().structures.structures[static_cast<std::uint32_t>(_row)].hitPoints;
  structure.buildEffortHundredths = Outpost::RequiredEffortHundredths(100);
  structure.working = Outpost::NO_OBJECT;
  return _sim.Objects().Create(structure);
}

Outpost::ObjectId Fielded(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::Seat& seat = _sim.SeatAt(_seat);
  if (seat.designs.empty())
  {
    Outpost::DeviceDesign design{};
    design.chassis = 0;
    design.drive = 0;
    design.modules = {};
    design.modules[0] = 0;
    design.moduleCount = 1;
    seat.designs.push_back(design);
  }
  Outpost::Device device{};
  device.seat = _seat;
  device.design = 0;
  device.x = static_cast<std::int32_t>(_cellX) * CELL + CELL / 2;
  device.z = static_cast<std::int32_t>(_cellY) * CELL + CELL / 2;
  device.hitPoints = 100;
  device.target = Outpost::NO_OBJECT;
  return _sim.Objects().Create(device);
}

} // namespace

TEST_CLASS(ResearchTests)
{
public:
  TEST_METHOD(AnItemWaitsForItsPrerequisitesAndForSomethingToPayForIt)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    const Outpost::ObjectId lab = Standing(sim, 0, Row::ResearchLab, 30, 30);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;

    Assert::IsFalse(Outpost::Available(seat, Tables(), static_cast<std::uint32_t>(Item::LightHull)), L"LightArmor comes first");
    Assert::IsFalse(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::LightHull)));
    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::LightArmor)));
    Assert::AreEqual(97000, seat.powerHundredths, L"the cost is drawn at the start");
    // One item a lab, whatever else is available.
    Assert::IsFalse(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::Ballistics)));

    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(std::size_t{1}, seat.researchComplete.size());
    Assert::IsTrue(Outpost::Available(seat, Tables(), static_cast<std::uint32_t>(Item::LightHull)), L"and now the hull is reachable");

    // Not a penny in the stockpile buys nothing, however available the item is.
    seat.powerHundredths = 3999;
    Assert::IsFalse(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::LightHull)));
    seat.powerHundredths = 4000;
    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::LightHull)));
  }

  TEST_METHOD(ThreeLabsResearchThreeItemsAtOnceAndAModuleShortensOne)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    const Outpost::ObjectId first = Standing(sim, 0, Row::ResearchLab, 30, 30);
    const Outpost::ObjectId second = Standing(sim, 0, Row::ResearchLab, 40, 40);
    const Outpost::ObjectId third = Standing(sim, 0, Row::ResearchLab, 50, 50);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;
    // The third lab carries a module: 30 per cent off a hundred ticks is seventy.
    sim.Objects().FindStructure(third)->modules[0] = 0;
    sim.Objects().FindStructure(third)->moduleCount = 1;

    Assert::IsTrue(Outpost::SetResearch(sim, 0, first, static_cast<std::uint32_t>(Item::Ballistics)));
    Assert::IsTrue(Outpost::SetResearch(sim, 0, second, static_cast<std::uint32_t>(Item::LightArmor)));
    Assert::IsTrue(Outpost::SetResearch(sim, 0, third, static_cast<std::uint32_t>(Item::Extraction)));
    Assert::AreEqual(std::size_t{3}, seat.researchActive.size(), L"three at once, which is what three labs are for");

    for (std::uint32_t tick = 0; tick < 70; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(std::size_t{1}, seat.researchComplete.size(), L"the one with the module, at seventy ticks");
    Assert::AreEqual(static_cast<std::uint32_t>(Item::Extraction), seat.researchComplete.front());
    for (std::uint32_t tick = 0; tick < 30; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(std::size_t{3}, seat.researchComplete.size(), L"and the other two at a hundred");
    Assert::AreEqual(std::size_t{0}, seat.researchActive.size());
  }

  TEST_METHOD(AnArmourUpgradeReachesADeviceAlreadyInTheFieldAndItKeepsItsFraction)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    const Outpost::ObjectId lab = Standing(sim, 0, Row::ResearchLab, 30, 30);
    const Outpost::ObjectId device = Fielded(sim, 0, 60, 60);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;

    Outpost::DesignStats before{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 0, before) == Outpost::DesignFault::None);
    Assert::AreEqual(100, before.hitPoints);
    Assert::AreEqual(10, before.kineticArmor);
    // Half health, which is the state the retroactive rule is actually about.
    sim.Objects().FindDevice(device)->hitPoints = 50;

    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::LightArmor)));
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      sim.Advance();
    }
    Outpost::DesignStats armored{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 0, armored) == Outpost::DesignFault::None);
    Assert::AreEqual(12, armored.kineticArmor, L"a fifth more armour, on a device built before the research");
    Assert::AreEqual(50, sim.Objects().FindDevice(device)->hitPoints, L"an armour upgrade is not a hit-point one");

    // And the hit-point upgrade, which is the one that has to rescale.
    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::LightHull)));
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      sim.Advance();
    }
    Outpost::DesignStats hulled{};
    Assert::IsTrue(Outpost::DeriveSeatDesign(seat, Tables(), 0, hulled) == Outpost::DesignFault::None);
    Assert::AreEqual(150, hulled.hitPoints, L"half again");
    Assert::AreEqual(75, sim.Objects().FindDevice(device)->hitPoints, L"and still half of it, which is the fraction rule");
  }

  TEST_METHOD(AStructureUpgradeAndAnExtractorUpgradeReachTheirOwnKind)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground({{20, 20}})));
    const Outpost::ObjectId lab = Standing(sim, 0, Row::ResearchLab, 30, 30);
    const Outpost::ObjectId post = Standing(sim, 0, Row::CommandPost, 40, 40);
    Standing(sim, 0, Row::Extractor, 20, 20);
    Standing(sim, 0, Row::Generator, 22, 20);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;
    sim.Objects().FindStructure(post)->hitPoints = 750; // half of 1,500

    seat.researchComplete = {static_cast<std::uint32_t>(Item::Ballistics)};
    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::Fortification)));
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(100, seat.upgrades.structureHitPointPercent);
    Assert::AreEqual(1500, sim.Objects().FindStructure(post)->hitPoints, L"twice the maximum, and still half of it");

    // The extractor's yield is 5 power a second; 40 per cent more is 7.
    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::Extraction)));
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(40, seat.upgrades.extractorRatePercent);
    seat.powerHundredths = 0;
    for (std::uint32_t tick = 0; tick < Neuron::TICKS_PER_SECOND; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(700, seat.powerHundredths, L"seven power a second where it was five");
  }

  TEST_METHOD(ADestroyedLabLosesItsProgressAndCancelingRefundsNothing)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    const Outpost::ObjectId lab = Standing(sim, 0, Row::ResearchLab, 30, 30);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;
    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::Ballistics)));
    Assert::AreEqual(95000, seat.powerHundredths);
    for (std::uint32_t tick = 0; tick < 50; ++tick)
    {
      sim.Advance();
    }
    Assert::AreEqual(50u, seat.researchActive.front().remainingTicks, L"half way");

    Outpost::DestroyStructure(sim, lab);
    sim.Advance();
    Assert::AreEqual(std::size_t{0}, seat.researchActive.size(), L"the lab took the work with it");
    Assert::AreEqual(95000, seat.powerHundredths, L"and the power with that: a destroyed lab refunds nothing");
    Assert::AreEqual(std::size_t{0}, seat.researchComplete.size());

    // Cancelling is the same on the money and different on the lab: it survives.
    const Outpost::ObjectId second = Standing(sim, 0, Row::ResearchLab, 40, 40);
    Assert::IsTrue(Outpost::SetResearch(sim, 0, second, static_cast<std::uint32_t>(Item::Ballistics)));
    Assert::AreEqual(90000, seat.powerHundredths);
    Assert::IsTrue(Outpost::CancelResearch(sim, 0, second));
    Assert::AreEqual(90000, seat.powerHundredths, L"nothing back: what was bought was the work");
    Assert::IsTrue(Outpost::SetResearch(sim, 0, second, static_cast<std::uint32_t>(Item::Ballistics)), L"and the lab is free again");
  }

  TEST_METHOD(AutoResearchFillsEveryIdleLabWithTheCheapestItLeftAlone)
  {
    Outpost::Sim sim(TwoSeats(true), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Standing(sim, 0, Row::ResearchLab, 30, 30);
    Standing(sim, 0, Row::ResearchLab, 40, 40);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;

    sim.Advance();
    Assert::AreEqual(std::size_t{2}, seat.researchActive.size(), L"both labs, on the first tick");
    // LightArmor and Laboratories both cost 30 power; the tie goes to the lower row index, so the
    // first lab takes LightArmor and the second takes Laboratories. Two hosts pick the same pair.
    Assert::AreEqual(static_cast<std::uint32_t>(Item::LightArmor), seat.researchActive[0].item);
    Assert::AreEqual(static_cast<std::uint32_t>(Item::Laboratories), seat.researchActive[1].item);
    Assert::AreEqual(94000, seat.powerHundredths, L"and both were paid for");

    // Seat 1 has the option off and a lab, and researches nothing at all.
    Standing(sim, 1, Row::ResearchLab, 90, 90);
    sim.SeatAt(1).powerHundredths = 100000;
    sim.Advance();
    Assert::AreEqual(std::size_t{0}, sim.Seats()[1].researchActive.size(), L"a commander who did not ask for it");
  }

  TEST_METHOD(AnUnlockIsNothingButTheItemBeingComplete)
  {
    // A row says what unlocks it and an item says what it unlocks; the simulation reads the first,
    // so there is no second mechanism to keep in step (Sim/Research.h).
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    const Outpost::ObjectId lab = Standing(sim, 0, Row::ResearchLab, 30, 30);
    Outpost::Seat& seat = sim.SeatAt(0);
    seat.powerHundredths = 100000;
    const Outpost::ClassUpgrades before = seat.upgrades;

    Assert::IsTrue(Outpost::SetResearch(sim, 0, lab, static_cast<std::uint32_t>(Item::Ballistics)));
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      sim.Advance();
    }
    Assert::IsTrue(before == seat.upgrades, L"an unlock changes no percentage");
    Assert::AreEqual(std::size_t{1}, seat.researchComplete.size());
    Assert::IsTrue(Outpost::UnlockedFor(seat, Tables(), "Ballistics"));
    Assert::IsFalse(Outpost::UnlockedFor(seat, Tables(), "Fortification"), L"and unlocks nothing else");
    Assert::IsTrue(Outpost::UnlockedFor(seat, Tables(), ""), L"an empty prerequisite is a row available from the start");
    Assert::IsFalse(Outpost::UnlockedFor(seat, Tables(), "NoSuchItem"), L"and one nothing defines is unreachable, not free");
  }
};

} // namespace SimTests
