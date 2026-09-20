#include "pch.h"

#include "Sim.h"
#include "Victory.h"

#include "FixedPoint.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// How a match ends (GameDesign.md §2; m1-vertical-slice/S11). The three conditions the lobby
// offers are one stage: annihilation takes a commander out when he holds neither a structure nor a
// builder, the last alliance standing wins whatever the condition, and the survival clock is
// settled on what each side EXTRACTED rather than on what it still has in the bank.
namespace SimTests
{

namespace
{

constexpr std::uint32_t CELL = static_cast<std::uint32_t>(Neuron::SUBUNITS_PER_CELL);

enum class Row : std::uint32_t
{
  CommandPost = 0,
  Extractor = 1,
  Generator = 2
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
    post.costHundredths = 50000;
    post.buildTimeTicks = 1200;
    Outpost::StructureDesc extractor{};
    extractor.id = "Extractor";
    extractor.role = Outpost::StructureRole::Extractor;
    extractor.footprintCellsX = 1;
    extractor.footprintCellsY = 1;
    extractor.costHundredths = 5000;
    extractor.buildTimeTicks = 300;
    extractor.powerHundredthsPerTick = 25; // 5 power a second at 20 ticks
    Outpost::StructureDesc generator{};
    generator.id = "Generator";
    generator.role = Outpost::StructureRole::Generator;
    generator.footprintCellsX = 2;
    generator.footprintCellsY = 2;
    generator.costHundredths = 25000;
    generator.buildTimeTicks = 800;
    generator.servesExtractors = 4;
    generator.serviceRangeSubunits = 48 * Neuron::SUBUNITS_PER_CELL;
    tree.structures.structures = {post, extractor, generator};

    // The least a builder needs to exist, and one design with no builder in it, so that the
    // annihilation rule has both kinds of device to tell apart.
    Outpost::ChassisDesc light{};
    light.id = "LightI";
    light.chassisClass = Outpost::ChassisClass::Light;
    light.hitPoints = 100;
    light.sightSubunits = 20 * Neuron::SUBUNITS_PER_CELL;
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
    Outpost::ModuleDesc builder{};
    builder.id = "Builder";
    builder.systemKind = Outpost::SystemKind::Builder;
    builder.costHundredths = 5000;
    builder.systemRangeSubunits = 8 * Neuron::SUBUNITS_PER_CELL;
    builder.buildPowerHundredthsPerTick = 50;
    Outpost::ModuleDesc cannon{};
    cannon.id = "Cannon";
    cannon.systemKind = Outpost::SystemKind::None;
    cannon.costHundredths = 5000;
    tree.components.modules = {builder, cannon};
    return tree;
  }();
  return TREE;
}

/// Design 0 carries the builder module, design 1 carries a weapon and no builder.
constexpr std::uint32_t BUILDER_DESIGN = 0;
constexpr std::uint32_t FIGHTER_DESIGN = 1;

Outpost::MatchSettings Seats(std::uint8_t _count, std::vector<std::uint8_t> _alliances,
                             Outpost::VictoryCondition _victory = Outpost::VictoryCondition::Annihilation, std::uint32_t _survivalTicks = 0)
{
  Outpost::MatchSettings settings{};
  settings.seed = 5;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = _count;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::Medium;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = _victory;
  settings.survivalTicks = _survivalTicks;
  for (std::uint8_t index = 0; index < _count; ++index)
  {
    settings.seats[index] = {Outpost::SeatKind::Human, _alliances[index]};
  }
  return settings;
}

Outpost::LandscapeDefinition LandscapeWith(std::vector<Outpost::CellPosition> _deposits = {})
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
  structure.hitPoints = 100;
  structure.buildEffortHundredths = 10000;
  structure.working = Outpost::NO_OBJECT;
  return _sim.Objects().Create(structure);
}

/// Gives the seat both designs the first time it is asked, so that a test names a design rather
/// than building one.
void Designs(Outpost::Sim& _sim, std::uint8_t _seat)
{
  Outpost::Seat& seat = _sim.SeatAt(_seat);
  if (!seat.designs.empty())
  {
    return;
  }
  Outpost::DeviceDesign withBuilder{};
  withBuilder.chassis = 0;
  withBuilder.drive = 0;
  withBuilder.modules[0] = 0; // the builder module
  withBuilder.moduleCount = 1;
  Outpost::DeviceDesign withWeapon{};
  withWeapon.chassis = 0;
  withWeapon.drive = 0;
  withWeapon.modules[0] = 1; // the cannon
  withWeapon.moduleCount = 1;
  seat.designs = {withBuilder, withWeapon};
}

Outpost::ObjectId DeviceOf(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _design, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Designs(_sim, _seat);
  Outpost::Device device{};
  device.seat = _seat;
  device.design = _design;
  device.x = static_cast<std::int32_t>(_cellX * CELL + CELL / 2);
  device.z = static_cast<std::int32_t>(_cellY * CELL + CELL / 2);
  device.hitPoints = 100;
  device.target = Outpost::NO_OBJECT;
  return _sim.Objects().Create(device);
}

} // namespace

TEST_CLASS(VictoryTests)
{
public:
  TEST_METHOD(ACommanderWithABuilderLeftIsNotAnnihilated)
  {
    // GameDesign.md §2: "every enemy structure and every enemy builder is destroyed". A commander
    // reduced to one truck is still in the match, and a commander reduced to tanks is not.
    Outpost::Sim sim(Seats(2, {0, 1}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    const Outpost::ObjectId post = Standing(sim, 1, Row::CommandPost, 100, 100);
    const Outpost::ObjectId truck = DeviceOf(sim, 1, BUILDER_DESIGN, 104, 100);
    DeviceOf(sim, 1, FIGHTER_DESIGN, 105, 100);
    sim.Advance();
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Playing);
    Assert::IsTrue(sim.Seats()[1].everHeldBase);

    // The base goes; the truck is what keeps him in.
    Assert::IsTrue(sim.Objects().Remove(post));
    sim.Advance();
    Assert::IsFalse(sim.Finished(), L"a builder is a base");
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Playing);

    // The truck goes; the tank is not a builder and does not keep him in.
    Assert::IsTrue(sim.Objects().Remove(truck));
    Assert::IsTrue(Outpost::Annihilated(sim, 1));
    sim.Advance();
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Eliminated);
    Assert::IsFalse(sim.Seats()[1].surrendered, L"annihilated, not conceded");
    Assert::IsTrue(sim.Finished());
    Assert::AreEqual(0, static_cast<int>(sim.WinningAlliance()));
    Assert::IsTrue(sim.Seats()[0].victory == Outpost::VictoryState::Won);
  }

  TEST_METHOD(AStructureUnderConstructionCountsAndAPlanDoesNot)
  {
    Outpost::Sim sim(Seats(2, {0, 1}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    const Outpost::ObjectId building = Standing(sim, 1, Row::CommandPost, 100, 100);
    Outpost::Structure* raising = sim.Objects().FindStructure(building);
    raising->state = Outpost::StructurePhase::UnderConstruction;
    sim.Advance();
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Playing, L"a half-built base is a base");

    // A plan occupies nothing and nothing has been built: a commander left with one has nothing.
    sim.Objects().FindStructure(building)->state = Outpost::StructurePhase::Plan;
    Assert::IsTrue(Outpost::Annihilated(sim, 1));
    sim.Advance();
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Eliminated);
  }

  TEST_METHOD(TheLastAllianceStandingWinsAndAlliesShareIt)
  {
    // Two commanders allied against one. Taking the lone commander out ends it for both allies at
    // once, which is what "allied commanders share vision and victory" means (GameDesign.md §2).
    Outpost::Sim sim(Seats(3, {0, 0, 1}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    Standing(sim, 1, Row::CommandPost, 40, 40);
    const Outpost::ObjectId lone = Standing(sim, 2, Row::CommandPost, 100, 100);
    sim.Advance();
    Assert::IsFalse(sim.Finished(), L"two alliances are standing");

    Assert::IsTrue(sim.Objects().Remove(lone));
    sim.Advance();
    Assert::IsTrue(sim.Finished());
    Assert::AreEqual(0, static_cast<int>(sim.WinningAlliance()));
    Assert::IsTrue(sim.Seats()[0].victory == Outpost::VictoryState::Won);
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Won, L"an ally who fired no shot still won");
    Assert::IsTrue(sim.Seats()[2].victory == Outpost::VictoryState::Eliminated);
  }

  TEST_METHOD(ASurrenderedAllyStaysEliminatedWhenHisSideWins)
  {
    // Won is for commanders who were there at the end. A seat that conceded is out from that tick
    // and the match's outcome is not his, however his alliance finishes.
    Outpost::Sim sim(Seats(3, {0, 0, 1}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    Standing(sim, 1, Row::CommandPost, 40, 40);
    const Outpost::ObjectId lone = Standing(sim, 2, Row::CommandPost, 100, 100);
    sim.Advance();

    Outpost::Order surrender{};
    surrender.tick = sim.Tick() + 1;
    surrender.seat = 1;
    surrender.kind = Outpost::OrderKind::Surrender;
    sim.Submit(surrender);
    sim.Advance();
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Eliminated);
    Assert::IsTrue(sim.Seats()[1].surrendered);
    Assert::IsFalse(sim.Finished(), L"his ally is still playing");

    Assert::IsTrue(sim.Objects().Remove(lone));
    sim.Advance();
    Assert::IsTrue(sim.Finished());
    Assert::IsTrue(sim.Seats()[0].victory == Outpost::VictoryState::Won);
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Eliminated, L"he was not there for it");
  }

  TEST_METHOD(TheSurvivalClockGoesToTheSideThatExtractedTheMost)
  {
    // Not the side holding the most: what a commander spent, he still dug up. Both sides here end
    // with the same stockpile and the match is decided on the difference in what they extracted.
    Outpost::Sim sim(Seats(2, {0, 1}, Outpost::VictoryCondition::Survival, 3), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    Standing(sim, 1, Row::CommandPost, 100, 100);
    sim.SeatAt(0).extractedHundredths = 10000;
    sim.SeatAt(1).extractedHundredths = 25000;
    sim.SeatAt(0).powerHundredths = 90000;
    sim.SeatAt(1).powerHundredths = 90000;
    for (std::uint32_t tick = 0; tick < 3; ++tick)
    {
      sim.Advance();
    }
    Assert::IsTrue(sim.Finished());
    Assert::AreEqual(1, static_cast<int>(sim.WinningAlliance()), L"the poorer commander dug more");
    Assert::IsTrue(sim.Seats()[0].victory == Outpost::VictoryState::Lost);
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Won);
    Assert::AreEqual(sim.Seats()[0].powerHundredths, sim.Seats()[1].powerHundredths, L"the banks were level");
  }

  TEST_METHOD(TheSurvivalClockIsADrawWhenNeitherSideDugMore)
  {
    Outpost::Sim sim(Seats(2, {0, 1}, Outpost::VictoryCondition::Survival, 4), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    Standing(sim, 1, Row::CommandPost, 100, 100);
    sim.SeatAt(0).extractedHundredths = 7777;
    sim.SeatAt(1).extractedHundredths = 7777;
    for (std::uint32_t tick = 0; tick < 4; ++tick)
    {
      sim.Advance();
    }
    Assert::IsTrue(sim.Finished());
    Assert::AreEqual(static_cast<int>(Outpost::NO_ALLIANCE), static_cast<int>(sim.WinningAlliance()));
    Assert::IsTrue(sim.Seats()[0].victory == Outpost::VictoryState::Lost, L"nobody won, so nobody is Won");
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Lost);
  }

  TEST_METHOD(PowerLostToAFullStockpileWasStillExtracted)
  {
    // Stage 2 banks what it can and drops the rest at the cap. The Survival total is taken before
    // that, because the alternative punishes the commander whose economy outran his spending.
    Outpost::Sim sim(Seats(2, {0, 1}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith({{20, 20}})));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    Standing(sim, 0, Row::Extractor, 20, 20);
    Standing(sim, 0, Row::Generator, 22, 20);
    Standing(sim, 1, Row::CommandPost, 100, 100);
    sim.SeatAt(0).powerHundredths = 1000000; // far over any cap the generators give
    const std::int32_t banked = sim.Seats()[0].powerHundredths;
    sim.Advance();
    Assert::AreEqual(static_cast<std::int64_t>(25), sim.Seats()[0].extractedHundredths, L"one tick of one served extractor");
    Assert::IsTrue(sim.Seats()[0].powerHundredths <= banked, L"the stockpile was already full");
    sim.Advance();
    Assert::AreEqual(static_cast<std::int64_t>(50), sim.Seats()[0].extractedHundredths);
    Assert::AreEqual(static_cast<std::int64_t>(0), sim.Seats()[1].extractedHundredths, L"a command post does not extract");
  }

  TEST_METHOD(AMatchNobodyHasBeenPlacedInDoesNotEndOnItsFirstTick)
  {
    // The annihilation rule cannot fire before a commander has held anything: a lobby hands the
    // Sim its seats and the base level is placed over the ticks that follow, and a rule that read
    // "holds nothing" alone would end the match in between.
    Outpost::Sim sim(Seats(2, {0, 1}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    for (std::uint32_t tick = 0; tick < 10; ++tick)
    {
      sim.Advance();
      Assert::IsFalse(sim.Finished());
    }
    Assert::IsFalse(sim.Seats()[0].everHeldBase);

    Standing(sim, 0, Row::CommandPost, 16, 16);
    Standing(sim, 1, Row::CommandPost, 100, 100);
    sim.Advance();
    Assert::IsTrue(sim.Seats()[0].everHeldBase, L"and now he has");
    Assert::IsTrue(sim.Seats()[1].everHeldBase);
    Assert::IsFalse(sim.Finished());
  }

  TEST_METHOD(AnEliminatedSeatIsOutOfTheTickThatEliminatedIt)
  {
    // A seat that goes out at stage 12 must not be counted standing by the same stage, and its
    // orders must be refused from the next tick on rather than judged.
    Outpost::Sim sim(Seats(3, {0, 1, 2}), Tables());
    Assert::IsTrue(sim.CreateLandscape(LandscapeWith()));
    Standing(sim, 0, Row::CommandPost, 16, 16);
    const Outpost::ObjectId second = Standing(sim, 1, Row::CommandPost, 60, 60);
    Standing(sim, 2, Row::CommandPost, 100, 100);
    sim.Advance();
    Assert::IsTrue(sim.Objects().Remove(second));
    sim.Advance();
    Assert::IsTrue(sim.Seats()[1].victory == Outpost::VictoryState::Eliminated);
    Assert::IsFalse(sim.Finished(), L"two alliances are still playing");

    Outpost::Order chat{};
    chat.tick = sim.Tick() + 1;
    chat.seat = 1;
    chat.kind = Outpost::OrderKind::Chat;
    sim.Submit(chat);
    const std::uint32_t dropped = sim.DroppedOrders();
    sim.Advance();
    Assert::AreEqual(dropped + 1, sim.DroppedOrders(), L"an eliminated commander's orders are dropped");
  }
};

} // namespace SimTests
