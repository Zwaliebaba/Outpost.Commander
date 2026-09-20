#include "pch.h"

#include "Placement.h"
#include "Sim.h"

#include "FixedPoint.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Where a structure may stand (GameDesign.md §5; m1-vertical-slice/S4). Every rule the design
// states is one test, and the two that had a choice in them - which reading of "the slope across
// the footprint", and whether a plan blocks - are pinned by a case that would pass under the other
// reading, so that changing the rule fails here rather than in a capture nobody reads.
namespace SimTests
{

namespace
{

constexpr std::uint32_t SAMPLES = Outpost::SAMPLES_PER_CELL_EDGE;

enum class Row : std::uint32_t
{
  CommandPost = 0,
  Extractor = 1,
  Factory = 2
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
    post.buildTimeTicks = 1200;
    Outpost::StructureDesc extractor{};
    extractor.id = "Extractor";
    extractor.role = Outpost::StructureRole::Extractor;
    extractor.footprintCellsX = 1;
    extractor.footprintCellsY = 1;
    extractor.hitPoints = 200;
    extractor.costHundredths = 5000;
    extractor.buildTimeTicks = 300;
    Outpost::StructureDesc factory{};
    factory.id = "Factory";
    factory.role = Outpost::StructureRole::Factory;
    factory.footprintCellsX = 3;
    factory.footprintCellsY = 3;
    factory.hitPoints = 800;
    factory.costHundredths = 40000;
    factory.buildTimeTicks = 1200;
    tree.structures.structures = {post, extractor, factory};
    return tree;
  }();
  return TREE;
}

Outpost::MatchSettings TwoSeats()
{
  Outpost::MatchSettings settings{};
  settings.seed = 7;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::Medium;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = Outpost::VictoryCondition::Annihilation;
  settings.seats[0] = {Outpost::SeatKind::Human, 0};
  settings.seats[1] = {Outpost::SeatKind::Ai, 1};
  return settings;
}

/// The built-in Small landscape of the other suites: real ground, gentle, dry where these tests
/// build. What a rule needs to bite is sculpted with a height delta rather than hoped for.
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

/// Sets every sample of a rectangle of CELLS to one height, which is what makes a test's ground say
/// exactly what the test is about.
[[nodiscard]] bool Level(Outpost::Sim& _sim, std::uint32_t _cellX, std::uint32_t _cellY, std::uint32_t _cellsX, std::uint32_t _cellsY,
                         std::int16_t _height)
{
  Outpost::HeightDelta delta{};
  delta.x = _cellX * SAMPLES;
  delta.y = _cellY * SAMPLES;
  delta.width = _cellsX * SAMPLES + 1;
  delta.height = _cellsY * SAMPLES + 1;
  delta.heights.assign(static_cast<std::size_t>(delta.width) * delta.height, _height);
  return _sim.FlattenTerrain(delta);
}

/// One sample, for the cases that are about a single step between two of them.
[[nodiscard]] bool RaiseSample(Outpost::Sim& _sim, std::uint32_t _sampleX, std::uint32_t _sampleY, std::int16_t _height)
{
  Outpost::HeightDelta delta{};
  delta.x = _sampleX;
  delta.y = _sampleY;
  delta.width = 1;
  delta.height = 1;
  delta.heights = {_height};
  return _sim.FlattenTerrain(delta);
}

void Reveal(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY, std::uint32_t _cellsX,
            std::uint32_t _cellsY)
{
  for (std::uint32_t y = _cellY; y < _cellY + _cellsY; ++y)
  {
    for (std::uint32_t x = _cellX; x < _cellX + _cellsX; ++x)
    {
      _sim.SeatAt(_seat).fog.AddViewer(x, y);
    }
  }
}

Outpost::ObjectId Placed(Outpost::Sim& _sim, std::uint8_t _seat, Row _row, std::uint32_t _cellX, std::uint32_t _cellY,
                         Outpost::StructurePhase _state)
{
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = static_cast<std::uint32_t>(_row);
  structure.cellX = _cellX;
  structure.cellY = _cellY;
  structure.state = _state;
  structure.hitPoints = 100;
  structure.working = Outpost::NO_OBJECT;
  return _sim.Objects().Create(structure);
}

/// The fault CheckPlacement gives a footprint of _row at a cell, for seat 0.
[[nodiscard]] Outpost::PlacementFault FaultAt(Outpost::Sim& _sim, Row _row, std::uint32_t _cellX, std::uint32_t _cellY)
{
  const Outpost::StructureDesc& row = Tables().structures.structures[static_cast<std::uint32_t>(_row)];
  const Outpost::Footprint footprint = Outpost::FootprintAt(row, _cellX, _cellY);
  const Outpost::PlacementQuery query{&_sim.Terrain(), &_sim.Objects(), &_sim.Seats()[0], &row, &Tables(), &_sim.Power().Deposits()};
  return Outpost::CheckPlacement(footprint, query);
}

/// The square of a distance given in CELLS, in subunits. A helper rather than
/// `static_cast<std::int64_t>(n * CELL) * (n * CELL)` because that spelling does the multiplication
/// in int and widens the result afterwards, which clang-tidy's
/// bugprone-implicit-widening-of-multiplication-result refuses - rightly, since the same shape over
/// a Frontier landscape's subunits would overflow.
[[nodiscard]] constexpr std::int64_t CellsSquared(std::int64_t _cells) noexcept
{
  return _cells * Neuron::SUBUNITS_PER_CELL * _cells * Neuron::SUBUNITS_PER_CELL;
}

} // namespace

TEST_CLASS(PlacementTests)
{
public:
  TEST_METHOD(AFootprintMustLieWhollyOnTheLandscape)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    // The border of a generated landscape falls to the outside plain, which is under the sea
    // (Content/LandscapeDefinition.h), so the ground is levelled first: this test is about the
    // bounds and nothing else.
    Assert::IsTrue(Level(sim, 123, 123, 5, 5, 40));
    Reveal(sim, 0, 120, 120, 8, 8);
    // 125 plus three is 128, which is one past the last cell of a Small landscape.
    Assert::IsTrue(Outpost::PlacementFault::OffLandscape == FaultAt(sim, Row::Factory, 126, 120), L"the far edge");
    Assert::IsTrue(Outpost::PlacementFault::OffLandscape == FaultAt(sim, Row::Factory, 120, 126));
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 125, 125), L"and 125 is the last that fits");
  }

  TEST_METHOD(AStandingStructureBlocksAndAPlanDoesNot)
  {
    // "A placed plan costs nothing and obstructs nothing until a builder begins it"
    // (GameDesign.md §5), which is the whole reason a plan is a state and not a separate record.
    Outpost::Sim sim(TwoSeats(), Tables());
    // Deposits under the two cells the one-by-one probe uses, so that what it measures is the
    // occupancy rule and not the extractor's own.
    Assert::IsTrue(sim.CreateLandscape(Ground({{32, 32}, {33, 33}})));
    Assert::IsTrue(Level(sim, 28, 28, 10, 10, 40));
    Reveal(sim, 0, 28, 28, 10, 10);
    const Outpost::ObjectId plan = Placed(sim, 0, Row::Factory, 30, 30, Outpost::StructurePhase::Plan);
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 30, 30), L"two plans may share ground");

    sim.Objects().FindStructure(plan)->state = Outpost::StructurePhase::UnderConstruction;
    Assert::IsTrue(Outpost::PlacementFault::Occupied == FaultAt(sim, Row::Factory, 30, 30), L"the moment a builder begins it");
    Assert::IsTrue(Outpost::PlacementFault::Occupied == FaultAt(sim, Row::Extractor, 32, 32), L"the far corner of a three by three");
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Extractor, 33, 33), L"and one cell past it is free");
  }

  TEST_METHOD(AFeatureBlocksTheGroundItStandsOn)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Assert::IsTrue(Level(sim, 28, 28, 10, 10, 40));
    Reveal(sim, 0, 28, 28, 10, 10);
    Outpost::Feature rock{};
    rock.cellX = 31;
    rock.cellY = 31;
    (void)sim.Objects().Create(rock);
    Assert::IsTrue(Outpost::PlacementFault::Occupied == FaultAt(sim, Row::Factory, 30, 30));
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 32, 32), L"clear of it");
  }

  TEST_METHOD(WaterUnderAnyCellRefusesIt)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Reveal(sim, 0, 28, 28, 10, 10);
    Assert::IsTrue(Level(sim, 30, 30, 3, 3, 40));
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 30, 30));
    // Sea level is zero (Content/LandscapeDefinition.h), so one cell put under it is enough.
    Assert::IsTrue(Level(sim, 32, 32, 1, 1, -10));
    Assert::IsTrue(Outpost::PlacementFault::Water == FaultAt(sim, Row::Factory, 30, 30), L"one corner in the sea");
  }

  TEST_METHOD(TheSteepestCellDecidesTheSlopeAndNotTheAverageAcrossIt)
  {
    // This is the whole of the reading ADR-less choice in Sim/Placement.h, pinned by a case the
    // other reading would accept: a three-by-three of level ground with ONE eight-unit step in the
    // middle of it. Across the footprint that is 8 world units over 192, which is 4%; under the
    // one cell that carries it, it is 8 over the 16-unit sample spacing, which is 50%.
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Reveal(sim, 0, 48, 48, 6, 6);
    Assert::IsTrue(Level(sim, 49, 49, 3, 3, 40));
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 49, 49), L"level ground");
    Assert::AreEqual(0u, Outpost::FootprintSlopePercent(sim.Terrain(), {49, 49, 3, 3}));

    Assert::IsTrue(RaiseSample(sim, 50 * SAMPLES + 2, 50 * SAMPLES + 2, 48));
    Assert::AreEqual(50u, Outpost::FootprintSlopePercent(sim.Terrain(), {49, 49, 3, 3}), L"eight world units over the 16 between samples");
    Assert::IsTrue(Outpost::PlacementFault::TooSteep == FaultAt(sim, Row::Factory, 49, 49));
    Assert::IsTrue(Outpost::MAX_PLACEMENT_SLOPE_PERCENT == 25u, L"the same number the drive table gives wheels");
  }

  TEST_METHOD(GroundTheCommanderHasNotSeenIsRefused)
  {
    // "A structure may be placed anywhere the commander has explored" (GameDesign.md §5): explored
    // and not visible, which is what makes a forward base possible after the scout has left.
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Assert::IsTrue(Level(sim, 60, 60, 3, 3, 40));
    Assert::IsTrue(Outpost::PlacementFault::Unexplored == FaultAt(sim, Row::Factory, 60, 60));
    Reveal(sim, 0, 60, 60, 2, 3);
    Assert::IsTrue(Outpost::PlacementFault::Unexplored == FaultAt(sim, Row::Factory, 60, 60), L"two of the three columns is not all of it");
    Reveal(sim, 0, 62, 60, 1, 3);
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 60, 60));

    // The viewers go away; the history does not, and the placement is still legal.
    for (std::uint32_t y = 60; y < 63; ++y)
    {
      for (std::uint32_t x = 60; x < 63; ++x)
      {
        sim.SeatAt(0).fog.RemoveViewer(x, y);
      }
    }
    Assert::IsFalse(sim.Seats()[0].fog.Visible(61, 61));
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 60, 60), L"explored, not visible");
  }

  TEST_METHOD(AnExtractorStandsOnADepositAndNowhereElse)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground({{70, 70}})));
    Assert::IsTrue(Level(sim, 69, 69, 5, 5, 40));
    Reveal(sim, 0, 69, 69, 5, 5);
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Extractor, 70, 70));
    Assert::IsTrue(Outpost::PlacementFault::NotOnDeposit == FaultAt(sim, Row::Extractor, 71, 70));
    // The rule is the extractor's alone; every other role may stand where the landscape allows.
    Assert::IsTrue(Outpost::PlacementFault::Accepted == FaultAt(sim, Row::Factory, 70, 71));
  }

  TEST_METHOD(TheFlattenLevelsEverySampleTheFootprintOwnsToTheirMean)
  {
    Outpost::Sim sim(TwoSeats(), Tables());
    Assert::IsTrue(sim.CreateLandscape(Ground()));
    Assert::IsTrue(Level(sim, 80, 80, 3, 3, 40));
    Assert::IsTrue(RaiseSample(sim, 81 * SAMPLES, 81 * SAMPLES, 53));

    const Outpost::Footprint footprint{80, 80, 3, 3};
    // Thirteen by thirteen samples: four a cell plus the sample that closes the far side, which is
    // the same window the landscape derives a cell from.
    const Outpost::HeightDelta delta = Outpost::FlattenDelta(sim.Terrain(), footprint);
    Assert::AreEqual(80u * SAMPLES, delta.x);
    Assert::AreEqual(80u * SAMPLES, delta.y);
    Assert::AreEqual(13u, delta.width);
    Assert::AreEqual(13u, delta.height);
    Assert::AreEqual(std::size_t{169}, delta.heights.size());
    // 168 samples at 40 and one at 53 is 6,733 over 169, which is 39 and a bit: integers round
    // toward zero, and the fraction is a quarter of a subunit of drawing.
    Assert::AreEqual(40, Outpost::FootprintMeanHeightWorldUnits(sim.Terrain(), footprint));
    Assert::AreEqual(std::int16_t{40}, delta.heights.front());

    Assert::IsTrue(sim.FlattenTerrain(delta));
    Assert::AreEqual(std::int16_t{40}, sim.Terrain().HeightAt(81 * SAMPLES, 81 * SAMPLES), L"the lump is gone");
    Assert::AreEqual(0u, Outpost::FootprintSlopePercent(sim.Terrain(), footprint));
  }

  TEST_METHOD(AFootprintComesFromItsRowAndWithoutTablesItIsOneCell)
  {
    Outpost::Structure structure{};
    structure.design = static_cast<std::uint32_t>(Row::Factory);
    structure.cellX = 10;
    structure.cellY = 20;
    Assert::IsTrue(Outpost::Footprint{10, 20, 3, 3} == Outpost::FootprintOf(structure, &Tables()));
    // No tables: one cell, which is where the structure certainly is and never ground it may not
    // hold. Guessing a size would block placements that are legal.
    Assert::IsTrue(Outpost::Footprint{10, 20, 1, 1} == Outpost::FootprintOf(structure, nullptr));

    Assert::IsTrue(Outpost::Overlaps({10, 20, 3, 3}, {12, 22, 1, 1}), L"the far corner is inside");
    Assert::IsFalse(Outpost::Overlaps({10, 20, 3, 3}, {13, 20, 1, 1}), L"and one past it is not");
    Assert::IsFalse(Outpost::Overlaps({10, 20, 3, 3}, {10, 23, 3, 3}));
  }

  TEST_METHOD(TheDistanceToAFootprintIsZeroInsideItAndToTheNearestEdgeOutside)
  {
    // ONE FUNCTION, TWO CALLERS, AND THAT IS THE POINT OF IT (m1-vertical-slice/S13). Stage 5
    // counts a builder's effort toward a plan when this is inside the builder module's range
    // (Sim/Construction.cpp), and the scripted commander decides where to walk a builder so that
    // stage 5 will count it (Sim/AiSeat.cpp). It lived in Construction.cpp's anonymous namespace,
    // so the AI measured to the site's CORNER against half the reach as a correction, and the two
    // could not be checked against each other. They are the same call now.
    constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;
    const Outpost::Footprint footprint{10, 20, 3, 3}; // Cells 10..12 by 20..22

    // Inside is zero, at every corner of it and in the middle.
    Assert::AreEqual(std::int64_t{0}, Outpost::DistanceSquaredTo(footprint, 10 * CELL, 20 * CELL));
    Assert::AreEqual(std::int64_t{0}, Outpost::DistanceSquaredTo(footprint, 11 * CELL + CELL / 2, 21 * CELL + CELL / 2));
    Assert::AreEqual(std::int64_t{0}, Outpost::DistanceSquaredTo(footprint, 13 * CELL, 23 * CELL), L"the far edge is inside");

    // Outside on one axis is that axis alone; the rectangle's edge and not its centre, which is
    // what makes a 3-by-3 site reachable from beside it rather than only from a truck's length
    // away.
    Assert::AreEqual(static_cast<std::int64_t>(CELL) * CELL, Outpost::DistanceSquaredTo(footprint, 9 * CELL, 21 * CELL));
    Assert::AreEqual(static_cast<std::int64_t>(CELL) * CELL, Outpost::DistanceSquaredTo(footprint, 14 * CELL, 21 * CELL));
    Assert::AreEqual(static_cast<std::int64_t>(CELL) * CELL, Outpost::DistanceSquaredTo(footprint, 11 * CELL, 19 * CELL));

    // Diagonally out, it is the corner: the two axes squared and summed, never the larger of them.
    Assert::AreEqual(2 * static_cast<std::int64_t>(CELL) * CELL, Outpost::DistanceSquaredTo(footprint, 9 * CELL, 19 * CELL));

    // A builder standing on the cell diagonally touching the corner of a site is three quarters of
    // a cell from it, which is the case S13 turns on: it is inside a two-cell builder's reach, so
    // stage 5 counts it, so the commander has nothing left to order.
    const std::int64_t touching = Outpost::DistanceSquaredTo(footprint, 9 * CELL + CELL / 2, 19 * CELL + CELL / 2);
    Assert::AreEqual(2 * static_cast<std::int64_t>(CELL / 2) * (CELL / 2), touching);
    Assert::IsTrue(touching <= CellsSquared(2), L"inside a two-cell reach");
  }
};

} // namespace SimTests
