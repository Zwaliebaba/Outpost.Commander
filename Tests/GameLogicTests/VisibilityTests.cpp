#include "pch.h"

#include "Sim.h"
#include "Snapshot.h"
#include "Visibility.h"

#include "ContentLoader.h"
#include "FixedPoint.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Fog of war (TechnicalDesign.md §4.6): computed on the host, and the replication filter, so what
// it says is exactly what a commander is allowed to be told. The properties under test are the
// ones that make it safe to send: a cell is visible only while something holds it, explored is
// history and never comes back off, a ridge blocks, height reaches over, and the refresh is
// bounded so the tick's cost does not depend on how many units are on the field.
namespace SimTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;

enum class Row : std::uint32_t
{
  CommandPost = 0,
  Tower = 1
};

/// Two structures with sight and one chassis with sight: the least a visibility test needs.
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
    post.sightSubunits = 12 * Neuron::SUBUNITS_PER_CELL; // GameDesign.md §8: structures see 12 cells
    Outpost::StructureDesc tower{};
    tower.id = "Tower";
    tower.role = Outpost::StructureRole::Tower;
    tower.footprintCellsX = 1;
    tower.footprintCellsY = 1;
    tower.costHundredths = 20000;
    tower.sightSubunits = 20 * Neuron::SUBUNITS_PER_CELL;
    tree.structures.structures = {post, tower};

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
    Outpost::ModuleDesc gun{};
    gun.id = "MachineGun";
    gun.systemKind = Outpost::SystemKind::None;
    gun.costHundredths = 4000;
    tree.components.modules = {gun};
    return tree;
  }();
  return TREE;
}

Outpost::MatchSettings Seats(std::uint8_t _alliance0 = 0, std::uint8_t _alliance1 = 1)
{
  Outpost::MatchSettings settings{};
  settings.seed = 3;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::Medium;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = Outpost::VictoryCondition::Annihilation;
  settings.seats[0] = {Outpost::SeatKind::Human, _alliance0};
  settings.seats[1] = {Outpost::SeatKind::Ai, _alliance1};
  return settings;
}

/// A landscape of one flat tile. The generator's own output, so the heights are whatever it makes
/// of the seed; the tests that care about a ridge raise one themselves with a height delta.
Outpost::LandscapeDefinition Flat()
{
  Outpost::LandscapeDefinition definition{};
  definition.version = Outpost::LANDSCAPE_DEFINITION_VERSION;
  definition.sizeClass = Outpost::SizeClass::Small;
  definition.cellsPerSide = Outpost::SIZE_CLASS_CELLS[0];
  definition.seed = 1;
  definition.palette = "Default";
  definition.tiles = {{0, 0, 512, 170, 0, 0, 0, 70, 1, 0, ""}};
  definition.starts = {{16, 16}, {100, 100}};
  return definition;
}

/// Raises a rectangle of samples to _height, which is how these tests build a ridge or a hill.
[[nodiscard]] bool Raise(Outpost::Sim& _sim, std::uint32_t _sampleX0, std::uint32_t _sampleY0, std::uint32_t _sampleX1,
                         std::uint32_t _sampleY1, std::int16_t _height)
{
  Outpost::HeightDelta delta{};
  delta.x = _sampleX0;
  delta.y = _sampleY0;
  delta.width = _sampleX1 - _sampleX0 + 1;
  delta.height = _sampleY1 - _sampleY0 + 1;
  delta.heights.assign(static_cast<std::size_t>(delta.width) * delta.height, _height);
  return _sim.FlattenTerrain(delta);
}

Outpost::ObjectId Watcher(Outpost::Sim& _sim, std::uint8_t _seat, Row _row, std::uint32_t _cellX, std::uint32_t _cellY)
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

Outpost::ObjectId Scout(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::DeviceDesign design{};
  design.chassis = 0;
  design.drive = 0;
  design.modules = {};
  design.modules[0] = 0;
  design.moduleCount = 1;
  Outpost::Seat& seat = _sim.SeatAt(_seat);
  if (seat.designs.empty())
  {
    seat.designs.push_back(design);
  }
  Outpost::Device device{};
  device.seat = _seat;
  device.design = 0;
  device.x = static_cast<std::int32_t>(_cellX) * CELL + CELL / 2;
  device.z = static_cast<std::int32_t>(_cellY) * CELL + CELL / 2;
  device.target = Outpost::NO_OBJECT;
  return _sim.Objects().Create(device);
}

/// The repository's own slice landscape, read out of the source tree the way AiTests reads the
/// tables. The disc measurement needs REAL ground: the fault it pins does not exist on the flat
/// tile the rest of this suite generates, because there is no relief there to occlude anything.
[[nodiscard]] const Outpost::LandscapeDefinition& Slice()
{
  static const Outpost::LandscapeDefinition DEFINITION = []
  {
    const std::filesystem::path gameData = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "GameData";
    Outpost::ContentTree tree;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    if (!Outpost::LoadContent(gameData, tree, diagnostics))
    {
      const std::string what = diagnostics.empty() ? "no diagnostic" : diagnostics.front().ToString();
      Assert::Fail(std::wstring(what.begin(), what.end()).c_str());
    }
    Outpost::LandscapeDefinition definition{};
    for (const Outpost::LandscapeDefinition& landscape : tree.landscapes)
    {
      if (landscape.cellsPerSide == Outpost::SIZE_CLASS_CELLS[0] && landscape.starts.size() >= 2 && !landscape.deposits.empty())
      {
        definition = landscape;
      }
    }
    return definition;
  }();
  return DEFINITION;
}

/// The six positions the eye height was chosen against, spread over the slice landscape so that
/// the figure is not one lucky hilltop. Quarters and halves of the side, which is a rule rather
/// than a list of cells nobody can check.
[[nodiscard]] std::array<std::pair<std::uint32_t, std::uint32_t>, 6> DiscPositions(std::uint32_t _side)
{
  return {{{_side / 4, _side / 4},
           {_side / 2, _side / 4},
           {3 * _side / 4, _side / 4},
           {_side / 4, _side / 2},
           {_side / 2, _side / 2},
           {3 * _side / 4, 3 * _side / 4}}};
}

/// How many cells of its own sight disc a viewer at each of those positions can actually see, and
/// how many there were to see.
void MeasureDisc(const Outpost::Landscape& _landscape, std::uint32_t _radiusCells, std::uint64_t& _seen, std::uint64_t& _total)
{
  _seen = 0;
  _total = 0;
  const std::uint32_t side = _landscape.CellsPerSide();
  for (const std::pair<std::uint32_t, std::uint32_t>& position : DiscPositions(side))
  {
    const std::int64_t radius = _radiusCells;
    for (std::int64_t dy = -radius; dy <= radius; ++dy)
    {
      for (std::int64_t dx = -radius; dx <= radius; ++dx)
      {
        if (dx * dx + dy * dy > radius * radius)
        {
          continue;
        }
        const std::int64_t x = static_cast<std::int64_t>(position.first) + dx;
        const std::int64_t y = static_cast<std::int64_t>(position.second) + dy;
        if (x < 0 || y < 0 || x >= static_cast<std::int64_t>(side) || y >= static_cast<std::int64_t>(side))
        {
          continue;
        }
        ++_total;
        std::uint64_t reads = 0;
        _seen += Outpost::Visibility::LineOfSight(_landscape, position.first, position.second, static_cast<std::uint32_t>(x),
                                                  static_cast<std::uint32_t>(y), reads)
                   ? 1
                   : 0;
      }
    }
  }
}

} // namespace

TEST_CLASS(VisibilityTests)
{
public:
  TEST_METHOD(ACellIsVisibleWhileAViewerHoldsItAndExploredForEverAfter)
  {
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    const Outpost::ObjectId scout = Scout(sim, 0, 40, 40);
    sim.Advance();

    const Outpost::FogGrid& fog = sim.Seats()[0].fog;
    Assert::IsTrue(fog.Visible(40, 40), L"the cell it stands in");
    Assert::IsTrue(fog.Visible(45, 40), L"five cells away, inside a twenty-cell radius");
    Assert::IsFalse(fog.Explored(80, 40), L"forty cells away, outside it");

    // The viewer goes away: what it held falls back to explored, never to unexplored.
    Assert::IsTrue(sim.Objects().Remove(scout));
    sim.Advance();
    Assert::IsFalse(fog.Visible(40, 40));
    Assert::IsTrue(fog.Explored(40, 40), L"explored is history and does not come off");
    Assert::AreEqual(std::uint16_t{0}, fog.ViewersAt(40, 40));
  }

  TEST_METHOD(AMovedViewerUnseesWhatItLeftBeforeItSeesWhatItReached)
  {
    // The count is what makes a cell visible, so a move that counted the new disc without
    // un-counting the old one would leave a trail of lit cells behind every unit in the game.
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    const Outpost::ObjectId scout = Scout(sim, 0, 20, 20);
    sim.Advance();
    const Outpost::FogGrid& fog = sim.Seats()[0].fog;
    Assert::IsTrue(fog.Visible(20, 20));

    sim.Objects().FindDevice(scout)->x = 60 * CELL + CELL / 2;
    sim.Objects().FindDevice(scout)->z = 60 * CELL + CELL / 2;
    sim.Advance();
    Assert::IsFalse(fog.Visible(20, 20), L"the cell it left");
    Assert::IsTrue(fog.Explored(20, 20));
    Assert::IsTrue(fog.Visible(60, 60), L"the cell it reached");

    // Two viewers on one cell, one of which leaves: the cell stays visible, which is the whole
    // reason the grid counts rather than flags.
    const Outpost::ObjectId first = Scout(sim, 0, 80, 80);
    const Outpost::ObjectId second = Scout(sim, 0, 80, 80);
    sim.Advance();
    Assert::AreEqual(std::uint16_t{2}, fog.ViewersAt(80, 80));
    Assert::IsTrue(sim.Objects().Remove(first));
    sim.Advance();
    Assert::IsTrue(fog.Visible(80, 80), L"the second viewer still holds it");
    Assert::IsTrue(sim.Objects().Remove(second));
    sim.Advance();
    Assert::IsFalse(fog.Visible(80, 80));
  }

  TEST_METHOD(ARidgeBetweenTheViewerAndTheTargetBlocks)
  {
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    // A wall four cells wide across the whole line, tall enough that no line of sight clears it.
    Assert::IsTrue(Raise(sim, 0, 40 * Outpost::SAMPLES_PER_CELL_EDGE, 512, 41 * Outpost::SAMPLES_PER_CELL_EDGE, 400));
    Scout(sim, 0, 35, 35);
    sim.Advance();

    const Outpost::FogGrid& fog = sim.Seats()[0].fog;
    Assert::IsTrue(fog.Visible(35, 38), L"this side of the ridge");
    Assert::IsFalse(fog.Visible(35, 45), L"beyond it, and inside the radius");
    std::uint64_t reads = 0;
    Assert::IsFalse(Outpost::Visibility::LineOfSight(sim.Terrain(), 35, 35, 35, 45, reads));
    Assert::IsTrue(Outpost::Visibility::LineOfSight(sim.Terrain(), 35, 35, 35, 38, reads));
  }

  TEST_METHOD(AHillExtendsTheRadiusByACellForEveryThirtyTwoUnitsOfHeight)
  {
    // GameDesign.md §8: height adds a cell of sight for every 32 world units a viewer stands
    // above its target. A tower on flat ground sees 20 cells; the same tower on a hill sees past
    // that, and the ground it sees is the ground below it.
    Outpost::Sim flat(Seats(), Tables());
    Assert::IsTrue(flat.CreateLandscape(Flat()));
    Watcher(flat, 0, Row::Tower, 60, 60);
    flat.Advance();
    Assert::IsFalse(flat.Seats()[0].fog.Visible(60, 85), L"25 cells from a 20-cell tower");

    Outpost::Sim hill(Seats(), Tables());
    Assert::IsTrue(hill.CreateLandscape(Flat()));
    // A pillar under the tower, 320 units up: ten cells of bonus, which reaches 25 comfortably.
    // Only the samples STRICTLY INSIDE cell 60 are raised. A cell's height is the tallest of its
    // five-by-five samples and neighbouring cells share their edge samples, so raising the whole
    // cell would raise its neighbours too - and a plateau's own edge blocks the view out of its
    // middle, which is correct geometry and not what this test is about.
    Assert::IsTrue(Raise(hill, 60 * Outpost::SAMPLES_PER_CELL_EDGE + 1, 60 * Outpost::SAMPLES_PER_CELL_EDGE + 1,
                         60 * Outpost::SAMPLES_PER_CELL_EDGE + 3, 60 * Outpost::SAMPLES_PER_CELL_EDGE + 3, 320));
    Watcher(hill, 0, Row::Tower, 60, 60);
    hill.Advance();
    Assert::IsTrue(hill.Seats()[0].fog.Visible(60, 85), L"the same 25 cells, from 320 units up");
  }

  TEST_METHOD(TheRefreshIsBoundedByTheBudgetInViewersAndNotByTheFieldSize)
  {
    // The point of a budget in viewers is that the tick's cost is a property of the design and not
    // of how many units a commander has built (TechnicalDesign.md §4.6).
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    const std::uint32_t many = Outpost::REFRESH_BUDGET_VIEWERS + 57;
    for (std::uint32_t index = 0; index < many; ++index)
    {
      Scout(sim, 0, 10 + (index % 100), 10 + (index / 100));
    }
    sim.Advance();
    Assert::AreEqual(Outpost::REFRESH_BUDGET_VIEWERS, sim.Sight().LastRefreshedViewers(), L"never more than the budget");
    Assert::AreEqual(std::size_t{Outpost::REFRESH_BUDGET_VIEWERS}, sim.Sight().Stamps().size());

    // The rest arrive on the next tick, because the ones already stamped are no longer "moved" and
    // sort behind those that have never been counted.
    sim.Advance();
    Assert::AreEqual(std::size_t{many}, sim.Sight().Stamps().size(), L"every viewer counted within two ticks");
    Assert::IsTrue(sim.Sight().LastHeightReads() > 0, L"the reads are counted for the tick-cost measurement");
  }

  TEST_METHOD(AlliedSeatsShareVisionAndEnemiesDoNot)
  {
    Outpost::Sim allied(Seats(0, 0), Tables());
    Assert::IsTrue(allied.CreateLandscape(Flat()));
    Scout(allied, 0, 50, 50);
    allied.Advance();
    Assert::IsTrue(allied.Seats()[0].fog.Visible(50, 50));
    Assert::IsTrue(allied.Seats()[1].fog.Visible(50, 50), L"an ally sees what its partner sees");

    Outpost::Sim enemies(Seats(0, 1), Tables());
    Assert::IsTrue(enemies.CreateLandscape(Flat()));
    Scout(enemies, 0, 50, 50);
    enemies.Advance();
    Assert::IsTrue(enemies.Seats()[0].fog.Visible(50, 50));
    Assert::IsFalse(enemies.Seats()[1].fog.Explored(50, 50), L"an enemy learns nothing");
  }

  TEST_METHOD(AGhostIsRecordedWhileSeenAndKeptAfterTheViewerLeaves)
  {
    Outpost::Sim sim(Seats(0, 1), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    const Outpost::ObjectId theirs = Watcher(sim, 1, Row::CommandPost, 50, 50);
    const Outpost::ObjectId scout = Scout(sim, 0, 50, 52);
    sim.Advance();

    const Outpost::Ghost* ghost = sim.Seats()[0].ghosts.Find(theirs);
    Assert::IsNotNull(ghost);
    Assert::AreEqual(std::uint32_t{50}, ghost->cellX);
    Assert::AreEqual(std::uint8_t{1}, ghost->seat, L"who owned it when it was seen");
    const std::uint32_t seenAt = ghost->seenTick;

    // The scout dies. The record stays, which is what an explored map shows and what an attack on
    // an unseen target is redirected to (GameDesign.md §8).
    Assert::IsTrue(sim.Objects().Remove(scout));
    sim.Advance();
    sim.Advance();
    Assert::IsFalse(sim.Seats()[0].fog.Visible(50, 50));
    const Outpost::Ghost* remembered = sim.Seats()[0].ghosts.Find(theirs);
    Assert::IsNotNull(remembered);
    Assert::AreEqual(seenAt, remembered->seenTick, L"the record is of when it was last seen, not of now");
  }

  TEST_METHOD(AGhostOfSomethingGoneIsClearedWhenTheCommanderLooksAgain)
  {
    Outpost::Sim sim(Seats(0, 1), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    const Outpost::ObjectId theirs = Watcher(sim, 1, Row::CommandPost, 50, 50);
    Scout(sim, 0, 50, 52);
    sim.Advance();
    Assert::IsNotNull(sim.Seats()[0].ghosts.Find(theirs));

    // Demolished while the commander is still watching: a base that is gone stops being drawn.
    Assert::IsTrue(sim.Objects().Remove(theirs));
    sim.Advance();
    Assert::IsNull(sim.Seats()[0].ghosts.Find(theirs));
  }

  TEST_METHOD(TwoSimsFedTheSameThingAgreeCellForCellAndAcrossASnapshot)
  {
    // The fog is the replication filter, so a divergence here is a divergence in what two hosts
    // would tell their clients. The stamps are state and travel with the grids.
    Outpost::Sim left(Seats(), Tables());
    Outpost::Sim right(Seats(), Tables());
    Assert::IsTrue(left.CreateLandscape(Flat()));
    Assert::IsTrue(right.CreateLandscape(Flat()));
    for (std::uint32_t index = 0; index < 5; ++index)
    {
      Scout(left, 0, 30 + index * 7, 30);
      Scout(right, 0, 30 + index * 7, 30);
      Watcher(left, 1, Row::Tower, 70, 30 + index);
      Watcher(right, 1, Row::Tower, 70, 30 + index);
    }
    for (std::uint32_t tick = 0; tick < 4; ++tick)
    {
      left.Advance();
      right.Advance();
    }
    Assert::AreEqual(left.Hash(), right.Hash());

    std::optional<Outpost::Sim> read = Outpost::Snapshot::Read(Outpost::Snapshot::Write(left), Tables());
    if (!read.has_value())
    {
      Assert::Fail(L"the snapshot did not read back"); // noreturn, which is what the move below relies on
    }
    Outpost::Sim reloaded = *read;
    Assert::AreEqual(left.Hash(), reloaded.Hash());
    Assert::AreEqual(left.Sight().Stamps().size(), reloaded.Sight().Stamps().size(), L"the stamps are on the wire");
    left.Advance();
    reloaded.Advance();
    Assert::AreEqual(left.Hash(), reloaded.Hash(), L"and the next refresh agrees, which is what they are for");
  }

  TEST_METHOD(TheRefreshsCostIsMeasuredForTheTickAdr)
  {
    // m1-vertical-slice/G3 adds a measurement section to the tick ADR, and the visibility
    // refresh's share of a tick is the first number it asks for. Logged rather than asserted on a
    // figure, because a figure measured on this machine is not one measured on the owner's; what
    // IS asserted is the shape - the reads are bounded by the budget and not by the field.
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    for (std::uint32_t index = 0; index < Outpost::REFRESH_BUDGET_VIEWERS; ++index)
    {
      Scout(sim, 0, 20 + (index % 80), 20 + (index / 80));
    }
    const auto start = std::chrono::steady_clock::now();
    sim.Advance();
    const auto end = std::chrono::steady_clock::now();
    const std::int64_t microseconds = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const std::uint64_t reads = sim.Sight().LastHeightReads();
    const std::wstring message = L"measured: a full visibility refresh of " + std::to_wstring(Outpost::REFRESH_BUDGET_VIEWERS) +
                                 L" viewers at a 20-cell radius took " + std::to_wstring(microseconds) + L" us and " +
                                 std::to_wstring(reads) + L" cell-height reads (" +
                                 std::to_wstring(reads / Outpost::REFRESH_BUDGET_VIEWERS) + L" a viewer)";
    Logger::WriteMessage(message.c_str());
    Assert::AreEqual(Outpost::REFRESH_BUDGET_VIEWERS, sim.Sight().LastRefreshedViewers());

    // Twice the viewers is the same work, because the budget is what bounds it. This is the
    // property the design asks for and the one a later optimisation must not lose.
    Outpost::Sim crowded(Seats(), Tables());
    Assert::IsTrue(crowded.CreateLandscape(Flat()));
    for (std::uint32_t index = 0; index < Outpost::REFRESH_BUDGET_VIEWERS * 2; ++index)
    {
      Scout(crowded, 0, 20 + (index % 80), 20 + (index / 80));
    }
    crowded.Advance();
    Assert::AreEqual(Outpost::REFRESH_BUDGET_VIEWERS, crowded.Sight().LastRefreshedViewers(), L"twice the viewers, the same budget");
  }

  TEST_METHOD(AViewerSeesMostOfItsOwnDiscOverTheSliceLandscape)
  {
    // WHAT THIS NUMBER IS FOR (m1-vertical-slice/S13). Landscape::Cell's height is a cell's
    // TALLEST sample, so a sight line drawn from that height to that height grazes the ground the
    // whole way and every swell between two cells occludes. The owner saw the result on 2026-09-19
    // and described it exactly: cyan terrain riddled with rectangular black holes, through ground
    // the commander was standing next to. GameLogic/Visibility.h's VIEWER_EYE_WORLD_UNITS is the fix and
    // this is the measurement that chose it, kept as a test so that a later change to CellHeight,
    // to the radius or to the eye cannot take the fog back to moth-eaten without this number
    // moving and somebody having to say why.
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Slice()), L"the slice landscape did not generate");

    std::uint64_t seen = 0;
    std::uint64_t total = 0;
    MeasureDisc(sim.Terrain(), 12, seen, total); // 12 cells: a structure's sight (GameDesign.md §8)

    Logger::WriteMessage(("    measured: a 12-cell viewer sees " + std::to_string(seen) + " of the " + std::to_string(total) +
                          " cells of its own disc over six positions of the slice landscape, at an eye of " +
                          std::to_string(Outpost::VIEWER_EYE_WORLD_UNITS) + " world units\n")
                           .c_str());

    // The figure as measured. It is pinned exactly, like the recorded landscape hashes next door:
    // if GameData\Landscapes\Slice.json is edited the number moves, and updating it is a decision
    // somebody takes rather than a drift nobody sees.
    Assert::AreEqual(std::uint64_t{2646}, total, L"the six discs are this many cells of landscape");
    Assert::AreEqual(std::uint64_t{2359}, seen, L"and this many of them are visible; re-measure if Slice.json changed");

    // And the point of the number, stated so that a reader who does not know the history still
    // knows what is being defended. At an eye of zero the same six discs come back at 722.
    Assert::IsTrue(seen * 100 / total >= 80, L"a viewer sees most of its own disc");
  }

  TEST_METHOD(AFlattenDropsOnlyTheStampsThatReachTheGroundItMoved)
  {
    // Un-counting a disc against heights that have changed would take viewers off cells that were
    // never counted, so the discs that REACH the changed ground are un-counted first and the
    // budget counts them again. m1-vertical-slice/S9 dropped the whole fog instead and left the
    // narrowing to S4, which is the task that actually flattens: a structure going up must not
    // black out a commander's explored map, and S4 puts one up every few seconds.
    Outpost::Sim sim(Seats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Flat()));
    Scout(sim, 0, 40, 40);
    sim.Advance();
    Assert::AreEqual(std::size_t{1}, sim.Sight().Stamps().size());
    Assert::IsTrue(sim.Seats()[0].fog.Explored(40, 40));

    // Sixty cells away is past any radius these tables give, so nothing about this viewer's disc
    // can have changed and its stamp is still exact.
    Assert::IsTrue(Raise(sim, 100 * Outpost::SAMPLES_PER_CELL_EDGE, 100 * Outpost::SAMPLES_PER_CELL_EDGE,
                         102 * Outpost::SAMPLES_PER_CELL_EDGE, 102 * Outpost::SAMPLES_PER_CELL_EDGE, 200));
    Assert::AreEqual(std::size_t{1}, sim.Sight().Stamps().size(), L"a disc that does not reach the ground that moved is untouched");
    Assert::IsTrue(sim.Seats()[0].fog.Visible(40, 40));

    // Under its own feet, it does reach.
    Assert::IsTrue(Raise(sim, 40 * Outpost::SAMPLES_PER_CELL_EDGE, 40 * Outpost::SAMPLES_PER_CELL_EDGE, 42 * Outpost::SAMPLES_PER_CELL_EDGE,
                         42 * Outpost::SAMPLES_PER_CELL_EDGE, 200));
    Assert::AreEqual(std::size_t{0}, sim.Sight().Stamps().size(), L"the stamp goes with the heights it was worked out against");
    Assert::IsFalse(sim.Seats()[0].fog.Visible(40, 40), L"un-counted until the budget counts it again");
    Assert::IsTrue(sim.Seats()[0].fog.Explored(40, 40), L"but the history stays, which the whole-fog reset threw away");
    sim.Advance();
    Assert::IsTrue(sim.Seats()[0].fog.Visible(40, 40), L"the next tick puts it back");
  }
};

} // namespace SimTests
