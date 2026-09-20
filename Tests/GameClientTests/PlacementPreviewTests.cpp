#include "pch.h"

#include "PlacementPreview.h"

#include "Plan.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Whether a structure may stand where the commander is pointing, answered from the replica
// (Design/Interface.md §7.1; m1-vertical-slice/K4 and G1b's footprint ghost). What is under test is
// that the ghost agrees with the simulation wherever it CAN - the ground's rules are literally the
// same function - and that where it cannot, it errs the way §5.2 requires.
namespace ReplicaTests
{

namespace
{

constexpr std::uint32_t CELLS = Outpost::SIZE_CLASS_CELLS[0]; ///< A Small landscape, 128 cells a side

enum class Row : std::uint32_t
{
  CommandPost = 0, ///< Three by three
  Extractor = 1,   ///< One cell, and only on a deposit
  Tower = 2        ///< One cell, and anywhere
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
    Outpost::StructureDesc extractor{};
    extractor.id = "Extractor";
    extractor.role = Outpost::StructureRole::Extractor;
    extractor.footprintCellsX = 1;
    extractor.footprintCellsY = 1;
    extractor.hitPoints = 400;
    Outpost::StructureDesc tower{};
    tower.id = "Tower";
    tower.role = Outpost::StructureRole::Tower;
    tower.footprintCellsX = 1;
    tower.footprintCellsY = 1;
    tower.hitPoints = 600;
    tree.structures.structures = {post, extractor, tower};
    return tree;
  }();
  return TREE;
}

Outpost::LandscapeDefinition Ground()
{
  Outpost::LandscapeDefinition definition{};
  definition.version = Outpost::LANDSCAPE_DEFINITION_VERSION;
  definition.sizeClass = Outpost::SizeClass::Small;
  definition.cellsPerSide = CELLS;
  definition.seed = 5;
  definition.palette = "Default";
  // ONE TILE WITH NO NOISE IN IT, so that every rule but the one under test is quiet: an
  // amplitude of zero is a plain, and a height shift of a hundred puts the whole of it well clear
  // of the water, which is the rule that otherwise refuses every site on a landscape at zero. The
  // cases that want relief build their own definition.
  definition.tiles = {{0, 0, 512, 170, 0, 100, 100, 70, 1, 8, ""}};
  definition.starts = {{8, 8}, {48, 48}};
  definition.deposits = {{20, 20}};
  return definition;
}

/// What a commander holds, filled by hand: the preview names the two things it reads, so a test
/// can hand it exactly those without standing up a host, a wire and a replica to carry them.
struct Watcher
{
  std::map<std::uint32_t, Outpost::ReplicaStructure> structures;
  std::vector<Outpost::FogState> fog;
  std::uint32_t fogCellsPerSide = 0;

  void Standing(std::uint32_t _id, Row _row, std::uint16_t _cellX, std::uint16_t _cellY, Outpost::StructurePhase _phase,
                bool _ghost = false)
  {
    Outpost::StructureState state{};
    state.id = _id;
    state.design = static_cast<std::uint32_t>(_row);
    state.cellX = _cellX;
    state.cellY = _cellY;
    state.phase = _phase;
    state.hitPoints = _ghost ? Outpost::GHOST_HIT_POINTS : static_cast<std::uint16_t>(100);
    structures[_id] = {state, _ghost};
  }

  void Explored(std::uint32_t _fromX, std::uint32_t _fromY, std::uint32_t _cells)
  {
    fog.assign(static_cast<std::size_t>(CELLS) * CELLS, Outpost::FogState::Unexplored);
    fogCellsPerSide = CELLS;
    for (std::uint32_t y = _fromY; y < _fromY + _cells; ++y)
    {
      for (std::uint32_t x = _fromX; x < _fromX + _cells; ++x)
      {
        fog[static_cast<std::size_t>(y) * CELLS + x] = Outpost::FogState::Explored;
      }
    }
  }

  [[nodiscard]] Outpost::PreviewQuery Query(const Outpost::Landscape& _landscape, const Outpost::DepositField* _deposits = nullptr) const
  {
    return {&structures, fog, fogCellsPerSide, &_landscape, &Tables(), _deposits};
  }
};

} // namespace

TEST_CLASS(PlacementPreviewTests)
{
public:
  TEST_METHOD(AFootprintIsTheRowsOwnSizeAndOneCellForARowThatIsNotThere)
  {
    const Outpost::Footprint post = Outpost::PreviewFootprint(Tables(), static_cast<std::uint32_t>(Row::CommandPost), 10, 20);
    Assert::AreEqual(std::uint32_t{10}, post.cellX);
    Assert::AreEqual(std::uint32_t{20}, post.cellY);
    Assert::AreEqual(std::uint32_t{3}, post.cellsX, L"three by three");
    Assert::AreEqual(std::uint32_t{3}, post.cellsY);

    const Outpost::Footprint unknown = Outpost::PreviewFootprint(Tables(), 99, 10, 20);
    Assert::AreEqual(std::uint32_t{1}, unknown.cellsX, L"a row this build does not have claims one cell and no more");
    Assert::AreEqual(std::uint32_t{1}, unknown.cellsY);
  }

  TEST_METHOD(APreviewWithNothingBehindItRefusesRatherThanReadingIt)
  {
    Outpost::Landscape landscape;
    Watcher watcher;
    Assert::IsTrue(Outpost::PreviewPlacement(0, 10, 10, {nullptr, {}, 0, &landscape, &Tables()}) == Outpost::PlacementFault::OffLandscape,
                   L"nothing to read");
    Assert::IsTrue(Outpost::PreviewPlacement(0, 10, 10, watcher.Query(landscape)) == Outpost::PlacementFault::OffLandscape,
                   L"a landscape the join has not delivered yet");
    Assert::IsTrue(landscape.Create(Ground()));
    Assert::IsTrue(Outpost::PreviewPlacement(99, 10, 10, watcher.Query(landscape)) == Outpost::PlacementFault::OffLandscape,
                   L"and a row the tables do not carry");
  }

  TEST_METHOD(GroundOffTheEdgeIsRefusedAndTheMiddleIsTaken)
  {
    Outpost::Landscape landscape;
    Assert::IsTrue(landscape.Create(Ground()));
    Watcher watcher;
    const Outpost::PreviewQuery query = watcher.Query(landscape);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 30, 30, query) == Outpost::PlacementFault::Accepted,
                   L"flat ground in the middle takes anything");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::CommandPost), CELLS - 1, 30, query) ==
                     Outpost::PlacementFault::OffLandscape,
                   L"a three-by-three whose far corner is off the edge");
  }

  TEST_METHOD(AStandingStructureOccupiesItsGroundAndAPlanDoesNot)
  {
    // GameDesign.md §5's rule, which is the simulation's: two commanders may plan the same ground
    // and the first to begin it gets it, so a plan the commander can see does not stop him.
    Outpost::Landscape landscape;
    Assert::IsTrue(landscape.Create(Ground()));
    Watcher watcher;
    watcher.Standing(1, Row::CommandPost, 30, 30, Outpost::StructurePhase::Standing);
    const Outpost::PreviewQuery query = watcher.Query(landscape);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 32, 32, query) == Outpost::PlacementFault::Occupied,
                   L"the far corner of its three by three");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 33, 33, query) == Outpost::PlacementFault::Accepted,
                   L"and one cell past it is free");

    watcher.Standing(2, Row::CommandPost, 40, 40, Outpost::StructurePhase::Plan);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 41, 41, query) == Outpost::PlacementFault::Accepted,
                   L"a plan blocks nobody until a builder begins it");
  }

  TEST_METHOD(AGHOSTBlocksToo)
  {
    // A building he has seen and walked away from is still there as far as he knows. One he could
    // build through would put his factory inside somebody's base the moment he scouted it again.
    Outpost::Landscape landscape;
    Assert::IsTrue(landscape.Create(Ground()));
    Watcher watcher;
    watcher.Standing(7, Row::CommandPost, 30, 30, Outpost::StructurePhase::Standing, true);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 31, 31, watcher.Query(landscape)) ==
                   Outpost::PlacementFault::Occupied);
  }

  TEST_METHOD(UnexploredGroundIsRefusedAndExploredGroundIsEnough)
  {
    // Explored, not visible: a base built where a scout once walked is the whole point of the rule.
    Outpost::Landscape landscape;
    Assert::IsTrue(landscape.Create(Ground()));
    Watcher watcher;
    watcher.Explored(28, 28, 6);
    const Outpost::PreviewQuery query = watcher.Query(landscape);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::CommandPost), 30, 30, query) ==
                     Outpost::PlacementFault::Accepted,
                   L"inside what he has explored");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::CommandPost), 33, 30, query) ==
                     Outpost::PlacementFault::Unexplored,
                   L"a footprint whose far column he has never seen");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 10, 10, query) == Outpost::PlacementFault::Unexplored,
                   L"and ground nowhere near him");
  }

  TEST_METHOD(AnExtractorNeedsADepositAndNothingElseDoes)
  {
    Outpost::Landscape landscape;
    Assert::IsTrue(landscape.Create(Ground()));
    Outpost::DepositField deposits;
    deposits.Build(Ground());
    Watcher watcher;
    const Outpost::PreviewQuery query = watcher.Query(landscape, &deposits);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Extractor), 20, 20, query) ==
                     Outpost::PlacementFault::Accepted,
                   L"on the deposit the definition names");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Extractor), 21, 20, query) ==
                     Outpost::PlacementFault::NotOnDeposit,
                   L"and one cell off it");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Tower), 21, 20, query) == Outpost::PlacementFault::Accepted,
                   L"while a tower of the same one-cell size stands anywhere");

    // No deposit field is an absence and not permission withheld, which is how CheckPlacement
    // reads a null one: the extractor is refused nowhere rather than everywhere.
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::Extractor), 21, 20, watcher.Query(landscape)) ==
                   Outpost::PlacementFault::Accepted);
  }

  TEST_METHOD(GroundThatIsOccupiedIsReportedAsOccupiedAndNotAsTheGroundsOwnFault)
  {
    // The fault ORDER, which Sim/Placement.h calls "the order a commander would want to be told
    // about them": a site that is both built on and too steep is reported as built on, because
    // that is the thing he can do something about. A preview that tested the ground first would
    // tell him a hillside was too steep when what is actually in his way is his own factory.
    Outpost::Landscape landscape;
    Outpost::LandscapeDefinition rough = Ground();
    rough.tiles = {{0, 0, 512, 170, 160, 300, 48, 70, 1, 32, ""}};
    Assert::IsTrue(landscape.Create(rough));

    Watcher watcher;
    std::uint32_t steepX = 0;
    std::uint32_t steepY = 0;
    for (std::uint32_t y = 3; y + 6 < CELLS && steepY == 0; y += 3)
    {
      for (std::uint32_t x = 3; x + 6 < CELLS; x += 3)
      {
        if (Outpost::CheckFootprintGround(landscape, {x, y, 3, 3}) == Outpost::PlacementFault::TooSteep)
        {
          steepX = x;
          steepY = y;
          break;
        }
      }
    }
    Assert::IsTrue(steepY != 0, L"a rough landscape has a steep three-by-three somewhere");
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::CommandPost), steepX, steepY, watcher.Query(landscape)) ==
                     Outpost::PlacementFault::TooSteep,
                   L"empty, it is the ground's fault");

    watcher.Standing(1, Row::CommandPost, static_cast<std::uint16_t>(steepX), static_cast<std::uint16_t>(steepY),
                     Outpost::StructurePhase::Standing);
    Assert::IsTrue(Outpost::PreviewPlacement(static_cast<std::uint32_t>(Row::CommandPost), steepX, steepY, watcher.Query(landscape)) ==
                     Outpost::PlacementFault::Occupied,
                   L"and built on, it is the building");
  }

  TEST_METHOD(TheGhostAndTheSimulationAgreeOnTheGROUNDCellForCell)
  {
    // THE CLAIM THIS FILE EXISTS FOR. The ghost is drawn from the client's own landscape and the
    // order is judged against the host's, and those two are generated from the same definition -
    // so on the ground's own three rules they have to agree everywhere, not merely usually. The
    // steepest-cell reading of the slope is where a second implementation would have drifted, and
    // it is shared literally: the loop below calls Sim's function on both landscapes.
    Outpost::Landscape hostSide;
    Outpost::Landscape clientSide;
    Outpost::LandscapeDefinition rough = Ground();
    rough.tiles = {{0, 0, 512, 170, 160, 300, 48, 70, 1, 32, ""}};
    Assert::IsTrue(hostSide.Create(rough));
    Assert::IsTrue(clientSide.Create(rough));

    std::uint32_t accepted = 0;
    std::uint32_t water = 0;
    std::uint32_t steep = 0;
    std::uint32_t sites = 0;
    for (std::uint32_t y = 0; y + 3 <= CELLS; y += 3)
    {
      for (std::uint32_t x = 0; x + 3 <= CELLS; x += 3)
      {
        const Outpost::Footprint site{x, y, 3, 3};
        const Outpost::PlacementFault host = Outpost::CheckFootprintGround(hostSide, site);
        Assert::IsTrue(host == Outpost::CheckFootprintGround(clientSide, site), L"site for site, on every one of them");
        ++sites;
        accepted += host == Outpost::PlacementFault::Accepted ? 1u : 0u;
        water += host == Outpost::PlacementFault::Water ? 1u : 0u;
        steep += host == Outpost::PlacementFault::TooSteep ? 1u : 0u;
      }
    }
    Logger::WriteMessage(("    measured: of " + std::to_string(sites) + " three-by-three sites of a rough landscape the ground takes " +
                          std::to_string(accepted) + ", refuses " + std::to_string(water) + " for water and " + std::to_string(steep) +
                          " for slope\n")
                           .c_str());
    // ALL THREE ANSWERS HAVE TO APPEAR, or this compares two functions that never said anything
    // but Accepted and would agree whatever either of them did.
    Assert::AreEqual(std::uint32_t{1764}, sites);
    Assert::IsTrue(accepted > 0 && water > 0 && steep > 0, L"a landscape rough enough to give every answer");
  }
};

} // namespace ReplicaTests
