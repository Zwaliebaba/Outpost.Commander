#include "pch.h"

#include "AiSeat.h"
#include "Construction.h"
#include "ContentLoader.h"
#include "Json.h"
#include "OrderQueue.h"
#include "Placement.h"
#include "Plan.h"
#include "Sim.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The scripted commander (TechnicalDesign.md §7; m1-vertical-slice/S12). This suite is the one that
// exercises every system at once: two scripted commanders play the slice landscape from a seed,
// with the real GameData tables, and everything from the economy to the damage matrix runs because
// they use it. What it asserts is not that the AI plays WELL - that is M2's, against a measured
// match - but that it plays at all, and that it does so as a pure function of the state and the
// tick.
namespace SimTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;

/// The repository's own tables and its slice landscape, read from the source tree the way the
/// landscape fixtures already are (Tests/SimTests/Fixtures/Landscape/README.md).
[[nodiscard]] std::filesystem::path GameData()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "GameData";
}

const Outpost::ContentTree& Tables()
{
  static const Outpost::ContentTree TREE = []
  {
    Outpost::ContentTree tree;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    if (!Outpost::LoadContent(GameData(), tree, diagnostics))
    {
      // A suite that silently ran against an empty tree would assert nothing at all.
      const std::string what = diagnostics.empty() ? "no diagnostic" : diagnostics.front().ToString();
      Assert::Fail(std::wstring(what.begin(), what.end()).c_str());
    }
    return tree;
  }();
  return TREE;
}

const Outpost::LandscapeDefinition& Slice()
{
  static const Outpost::LandscapeDefinition DEFINITION = []
  {
    Outpost::LandscapeDefinition definition{};
    Outpost::ContentTree tree;
    std::vector<Outpost::ContentDiagnostic> diagnostics;
    if (!Outpost::LoadContent(GameData(), tree, diagnostics))
    {
      Assert::Fail(L"the content did not load");
    }
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

Outpost::MatchSettings TwoScriptedSeats(std::uint32_t _survivalTicks = 0)
{
  Outpost::MatchSettings settings{};
  settings.seed = 20260919;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::High;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Low;
  settings.victory = _survivalTicks == 0 ? Outpost::VictoryCondition::Annihilation : Outpost::VictoryCondition::Survival;
  settings.survivalTicks = _survivalTicks;
  // Research is the seat's own auto-research (m1-vertical-slice/S6), which is why there is no
  // research behaviour in the list: a scripted seat is set up with it on.
  settings.seats[0] = {Outpost::SeatKind::Ai, 0, true};
  settings.seats[1] = {Outpost::SeatKind::Ai, 1, true};
  return settings;
}

/// The row of the first structure with a role, in the real tables.
[[nodiscard]] std::uint32_t RowOf(Outpost::StructureRole _role)
{
  const std::vector<Outpost::StructureDesc>& rows = Tables().structures.structures;
  for (std::uint32_t row = 0; row < rows.size(); ++row)
  {
    if (rows[row].role == _role)
    {
      return row;
    }
  }
  Assert::Fail(L"the tables have no structure with that role");
}

/// The base level of GameDesign.md §2: nothing but a builder and a command post. Placed by the
/// test because NOTHING IN THE TREE OWNS BASE-LEVEL PLACEMENT YET - it is the application's
/// (m1-vertical-slice/G1), and until G1 lands a match is set up by whoever starts one.
void PlaceStartingBase(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY)
{
  const std::uint32_t postRow = RowOf(Outpost::StructureRole::CommandPost);
  Outpost::Structure post{};
  post.seat = _seat;
  post.design = postRow;
  post.cellX = _cellX;
  post.cellY = _cellY;
  post.state = Outpost::StructurePhase::Standing;
  post.hitPoints = Tables().structures.structures[postRow].hitPoints;
  post.buildEffortHundredths = Outpost::RequiredEffortHundredths(Tables().structures.structures[postRow].buildTimeTicks);
  post.working = Outpost::NO_OBJECT;
  const Outpost::ObjectId placed = _sim.Objects().Create(post);
  const Outpost::StructureDesc& row = Tables().structures.structures[postRow];
  for (std::uint32_t y = 0; y < row.footprintCellsY; ++y)
  {
    for (std::uint32_t x = 0; x < row.footprintCellsX; ++x)
    {
      _sim.SetObstruction(_cellX + x, _cellY + y, 1);
    }
  }
  Assert::IsTrue(placed.Valid());

  // One builder, from a design the seat saves for itself: the AI's own designer picks it on its
  // first decision, so the starting truck is built from whatever the designer would have chosen.
  Outpost::AiBlackboard blackboard;
  Outpost::Observe(_sim, _seat, blackboard);
  Outpost::DeviceDesign design{};
  Assert::IsTrue(Outpost::BestDesign(_sim, _seat, Outpost::AiRole::Builder, blackboard, design), L"the tables offer no builder");
  _sim.SeatAt(_seat).designs.push_back(design);
  // On ground the truck's drive can actually stand on. A device dropped on a cliff has no component
  // in the cluster graph, so every route it is ever given comes back unreachable and it stands
  // where it was put for the whole match - which is exactly what the first run of this suite did
  // to the second commander, and it looked like an AI fault rather than a placement one.
  const Outpost::DriveClass drive = Tables().components.drives[_sim.Seats()[_seat].designs[0].drive].driveClass;
  std::uint32_t truckX = _cellX;
  std::uint32_t truckY = _cellY;
  bool standable = false;
  for (std::uint32_t ring = 3; ring < 16 && !standable; ++ring)
  {
    for (std::uint32_t y = _cellY > ring ? _cellY - ring : 0; y <= _cellY + ring && !standable; ++y)
    {
      for (std::uint32_t x = _cellX > ring ? _cellX - ring : 0; x <= _cellX + ring && !standable; ++x)
      {
        if (x < _sim.Terrain().CellsPerSide() && y < _sim.Terrain().CellsPerSide() && _sim.Clusters().Passable(x, y, drive) &&
            _sim.Clusters().ComponentAt(x, y, drive) != Outpost::NO_COMPONENT)
        {
          truckX = x;
          truckY = y;
          standable = true;
        }
      }
    }
  }
  Assert::IsTrue(standable, L"nowhere near the start that this drive can stand");

  Outpost::Device truck{};
  truck.seat = _seat;
  truck.design = 0;
  truck.x = static_cast<std::int32_t>(truckX) * CELL + CELL / 2;
  truck.z = static_cast<std::int32_t>(truckY) * CELL + CELL / 2;
  truck.hitPoints = 100;
  truck.target = Outpost::NO_OBJECT;
  Assert::IsTrue(_sim.Objects().Create(truck).Valid());
}

/// A match on the slice landscape with both seats set up as the lobby's "nothing" base level.
Outpost::Sim SliceMatch(std::uint32_t _survivalTicks = 0)
{
  Outpost::Sim sim(TwoScriptedSeats(_survivalTicks), Tables());
  Assert::IsTrue(sim.CreateLandscape(Slice()), L"the slice landscape did not generate");
  const std::vector<Outpost::CellPosition>& starts = Slice().starts;
  PlaceStartingBase(sim, 0, starts[0].x, starts[0].y);
  PlaceStartingBase(sim, 1, starts[1].x, starts[1].y);
  return sim;
}

[[nodiscard]] std::uint32_t CountStructures(const Outpost::Sim& _sim, std::uint8_t _seat, Outpost::StructureRole _role)
{
  std::uint32_t count = 0;
  _sim.Objects().ForEachStructure(
    [&](Outpost::ObjectId, const Outpost::Structure& _structure)
    {
      if (_structure.seat != _seat || _structure.state != Outpost::StructurePhase::Standing)
      {
        return;
      }
      if (_structure.design < _sim.Content().structures.structures.size() &&
          _sim.Content().structures.structures[_structure.design].role == _role)
      {
        ++count;
      }
    });
  return count;
}

[[nodiscard]] std::uint32_t CountDevices(const Outpost::Sim& _sim, std::uint8_t _seat)
{
  std::uint32_t count = 0;
  _sim.Objects().ForEachDevice([&](Outpost::ObjectId, const Outpost::Device& _device) { count += _device.seat == _seat ? 1 : 0; });
  return count;
}

/// Places a plan for a seat: a structure in StructurePhase::Plan, which is what a PlaceStructure
/// order leaves behind and what the FINISH behaviour walks a builder to.
Outpost::ObjectId PlacePlan(Outpost::Sim& _sim, std::uint8_t _seat, Outpost::StructureRole _role, std::uint32_t _cellX,
                            std::uint32_t _cellY)
{
  const std::uint32_t row = RowOf(_role);
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = row;
  structure.cellX = _cellX;
  structure.cellY = _cellY;
  structure.state = Outpost::StructurePhase::Plan; // A plan occupies nothing and has paid nothing
  structure.working = Outpost::NO_OBJECT;
  return _sim.Objects().Create(structure);
}

/// Gives a seat the fighter design it would otherwise stop and make, so that a decision reaches the
/// behaviours past DESIGN. Index 0 is the builder and index 1 the fighter, always (GameLogic/AiSeat.cpp).
void GiveFighterDesign(Outpost::Sim& _sim, std::uint8_t _seat)
{
  Outpost::AiBlackboard blackboard;
  Outpost::Observe(_sim, _seat, blackboard);
  Outpost::DeviceDesign fighter{};
  Assert::IsTrue(Outpost::BestDesign(_sim, _seat, Outpost::AiRole::Fighter, blackboard, fighter), L"the tables offer no fighter");
  _sim.SeatAt(_seat).designs.push_back(fighter);
}

/// Walls a footprint in: every cell of the ring _ring cells out from it is obstructed, which is
/// what a structure's own cells do to the cluster graph and what makes a site unreachable.
void WallIn(Outpost::Sim& _sim, const Outpost::Footprint& _footprint, std::uint32_t _rings)
{
  for (std::uint32_t ring = 1; ring <= _rings; ++ring)
  {
    const std::int64_t x0 = static_cast<std::int64_t>(_footprint.cellX) - ring;
    const std::int64_t y0 = static_cast<std::int64_t>(_footprint.cellY) - ring;
    const std::int64_t x1 = static_cast<std::int64_t>(_footprint.cellX) + _footprint.cellsX - 1 + ring;
    const std::int64_t y1 = static_cast<std::int64_t>(_footprint.cellY) + _footprint.cellsY - 1 + ring;
    for (std::int64_t y = y0; y <= y1; ++y)
    {
      for (std::int64_t x = x0; x <= x1; ++x)
      {
        const bool onTheRing = x == x0 || x == x1 || y == y0 || y == y1;
        if (onTheRing && x >= 0 && y >= 0 && x < static_cast<std::int64_t>(_sim.Terrain().CellsPerSide()) &&
            y < static_cast<std::int64_t>(_sim.Terrain().CellsPerSide()))
        {
          _sim.SetObstruction(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1);
        }
      }
    }
  }
}

/// The cell a Move order names, from its operands (GameShared/Order.h: the device, then x and z in
/// subunits).
[[nodiscard]] Outpost::CellPosition MoveTargetCell(const Outpost::Order& _order)
{
  return {static_cast<std::uint32_t>(_order.operands[1] / CELL), static_cast<std::uint32_t>(_order.operands[2] / CELL)};
}

/// Whether a cell is inside a footprint.
[[nodiscard]] bool Inside(const Outpost::Footprint& _footprint, const Outpost::CellPosition& _cell)
{
  return _cell.x >= _footprint.cellX && _cell.x < _footprint.cellX + _footprint.cellsX && _cell.y >= _footprint.cellY &&
         _cell.y < _footprint.cellY + _footprint.cellsY;
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

TEST_CLASS(AiTests)
{
public:
  TEST_METHOD(TheDesignerPicksTheBestDamagePerPowerAgainstWhatItHasSeen)
  {
    Outpost::Sim sim = SliceMatch();
    Outpost::AiBlackboard blackboard;
    Outpost::Observe(sim, 0, blackboard);

    Outpost::DeviceDesign fighter{};
    Assert::IsTrue(Outpost::BestDesign(sim, 0, Outpost::AiRole::Fighter, blackboard, fighter));
    Outpost::DeviceDesign builder{};
    Assert::IsTrue(Outpost::BestDesign(sim, 0, Outpost::AiRole::Builder, blackboard, builder));
    Assert::IsTrue(fighter.modules[0] != builder.modules[0], L"a gun is not a builder");

    // With nothing seen, the score is the mean over every column; with a composition seen, it is
    // that composition's. What must be true is that the matrix makes a DIFFERENCE - that some
    // weapon in the tables is worth a different amount against tracks than against legs - because
    // otherwise the designer is an expensive way to pick the cheapest gun.
    Outpost::AiBlackboard againstTracks = blackboard;
    againstTracks.enemyColumns = {};
    againstTracks.enemyColumns[Outpost::TargetColumnOf(Outpost::DriveClass::Tracks)] = 10;
    Outpost::AiBlackboard againstLegs = blackboard;
    againstLegs.enemyColumns = {};
    againstLegs.enemyColumns[Outpost::TargetColumnOf(Outpost::DriveClass::Legs)] = 10;
    bool anyDifference = false;
    for (const Outpost::ModuleDesc& module : sim.Content().components.modules)
    {
      if (module.systemKind != Outpost::SystemKind::None)
      {
        continue;
      }
      anyDifference = anyDifference || Outpost::ExpectedDamage(sim.Content(), module, againstTracks) !=
                                         Outpost::ExpectedDamage(sim.Content(), module, againstLegs);
    }
    Assert::IsTrue(anyDifference, L"the matrix says something different about tracks than about legs");
  }

  TEST_METHOD(AScriptedCommanderBuildsAnEconomyAnArmyAndMeetsTheOther)
  {
    // The acceptance's first run: within 12,000 ticks - ten minutes - both commanders must have
    // had a served extractor, a factory, a lab and a device of their own, and must have met.
    //
    // "MUST HAVE HAD" AND NOT "MUST STILL HAVE AT TICK 12,000", which is a correction and not a
    // weakening (m1-vertical-slice/S14). This case read the counts once, at the end, and that was
    // a true reading of the acceptance only while the two commanders could not hurt each other:
    // measured 2026-09-19, they fired no shot in twenty thousand ticks, so nothing was ever
    // destroyed and a count at the end was a count of everything ever built. With combat working,
    // the commander who loses the fight has lost its factory and its generator well before tick
    // 12,000 - which is the game working, not the AI failing to build one. So each condition is
    // latched when it is first true, exactly as the seed sweep below already does it.
    Outpost::Sim sim = SliceMatch();
    const auto started = std::chrono::steady_clock::now();
    std::uint32_t firstContactTick = 0;
    std::uint32_t firstShotTick = 0;
    std::uint32_t shots = 0;
    std::array<bool, 2> hadEverything{};
    for (std::uint32_t tick = 0; tick < 12000 && !sim.Finished(); ++tick)
    {
      sim.Advance();
      // AND THAT THEY FIGHT, which is counted here because the tick loop is already paid for
      // (m1-vertical-slice/S14). Nothing asserted it until 2026-09-19 and nothing could: a shot
      // left no trace anybody could count until C9 gave the simulation a record of one. Measured
      // before that task's fix, these two commanders built 115 devices between them, spent 99,734
      // device-ticks holding an ACQUIRED target, and fired nothing in twenty thousand ticks.
      shots += static_cast<std::uint32_t>(sim.Shots().size());
      if (firstShotTick == 0 && !sim.Shots().empty())
      {
        firstShotTick = sim.Tick();
      }
      if (firstContactTick == 0)
      {
        Outpost::AiBlackboard blackboard;
        Outpost::Observe(sim, 0, blackboard);
        firstContactTick = blackboard.contact ? sim.Tick() : 0;
      }
      for (std::uint8_t seat = 0; seat < 2; ++seat)
      {
        hadEverything[seat] =
          hadEverything[seat] || (CountStructures(sim, seat, Outpost::StructureRole::Extractor) > 0 &&
                                  CountStructures(sim, seat, Outpost::StructureRole::Generator) > 0 &&
                                  CountStructures(sim, seat, Outpost::StructureRole::Factory) > 0 &&
                                  CountStructures(sim, seat, Outpost::StructureRole::ResearchLab) > 0 && CountDevices(sim, seat) > 1);
      }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

    for (std::uint8_t seat = 0; seat < 2; ++seat)
    {
      Assert::IsTrue(hadEverything[seat],
                     L"a served extractor, a factory, a lab and a device of its own - every one of them, at some point");
      Assert::IsFalse(sim.Seats()[seat].researchComplete.empty(), L"auto-research ran");
    }
    Assert::IsTrue(firstContactTick > 0, L"the two commanders met");
    // FOUGHT, AND IN EARNEST. "A shot was fired at all" is too weak to pin this: with the attack
    // group's destination left broken, the defenders still stumble into each other eventually -
    // measured, first shot on tick 11,758 and 56 shots in the whole match, against tick 6,980 and
    // 1,751 with the groups arriving. Both numbers are asserted, with wide margins on each side of
    // the gap, because the landscape decides them far more than the seed does: over five seeds the
    // first shot fell on tick 6,980 in every one.
    Assert::IsTrue(firstShotTick > 0 && firstShotTick < 9000, L"and fought early: a match where neither can hurt the other has no end");
    Assert::IsTrue(shots > 500, L"and went on fighting, rather than brushing past each other once");
    Logger::WriteMessage(("    measured: two scripted commanders reached first contact on tick " + std::to_string(firstContactTick) +
                          ", the first shot on tick " + std::to_string(firstShotTick) + ", " + std::to_string(shots) +
                          " shots in all; 12,000 ticks took " + std::to_string(elapsed.count()) + " ms\n")
                           .c_str());
  }

  TEST_METHOD(AnAttackGroupIsNeverSentToGroundNothingCanStandOn)
  {
    // THE OTHER DEADLOCK OF THE SAME SHAPE (m1-vertical-slice/S14), in the half of the AI that
    // fights rather than the half that builds. An attack group is sent APPROACH_CELLS short of
    // the enemy's start cell, and a base GROWS: by the time a commander has four idle fighters,
    // four cells back is inside the enemy's buildings, whose cells are impassable. The planner
    // answers Unreachable one tick later, GameLogic/Movement.cpp falls the device back to Stop, and the
    // next decision orders the same four devices at the same cell - for the rest of the match.
    // Measured before the fix: the group was ordered on tick 5,623 to cell (38,90), gave up on
    // 5,624 having moved nothing, and the same order went out every ten ticks thereafter.
    //
    // WHAT IS ASSERTED IS THE INVARIANT AND NOT THE SYMPTOM: every AttackMove this commander
    // issues names a cell its own drive class can stand on. A destination nothing can stand on is
    // a route nobody can search, whatever else is true of the map.
    Outpost::Sim sim = SliceMatch();
    std::uint32_t attacks = 0;
    std::uint32_t arrivalsSeen = 0;
    for (std::uint32_t tick = 0; tick < 8000; ++tick)
    {
      sim.Advance();
      for (const Outpost::OrderQueue::Entry& entry : sim.Orders().Entries())
      {
        if (entry.arrival < arrivalsSeen || entry.order.kind != Outpost::OrderKind::AttackMove)
        {
          continue;
        }
        arrivalsSeen = entry.arrival + 1;
        const Outpost::CellPosition target = MoveTargetCell(entry.order);
        ++attacks;
        const Outpost::Device* device =
          sim.Objects().FindDevice({static_cast<std::uint32_t>(entry.order.operands[0]), Outpost::ObjectKind::Device});
        if (device == nullptr)
        {
          continue; // It died between the order being queued and this tick; not this rule's case.
        }
        const Outpost::DriveClass drive = Outpost::DriveOf(sim, entry.order.seat, *device);
        if (!sim.Clusters().Passable(target.x, target.y, drive) ||
            sim.Clusters().ComponentAt(target.x, target.y, drive) == Outpost::NO_COMPONENT)
        {
          Assert::Fail((L"an attack group was sent to cell (" + std::to_wstring(target.x) + L"," + std::to_wstring(target.y) +
                        L"), which its own drive class cannot stand on")
                         .c_str());
        }
      }
    }
    Logger::WriteMessage(
      ("    measured: " + std::to_string(attacks) + " attack orders, every one of them at ground the group can stand on\n").c_str());
    Assert::IsTrue(attacks > 0, L"the commander attacked at all, or this asserts nothing");
  }

  TEST_METHOD(AScriptedCommanderNeverWalksABuilderOntoASitesOwnFootprint)
  {
    // THE DEADLOCK THIS PINS (m1-vertical-slice/S13). A structure occupies its cells from the tick
    // construction begins (GameShared/Plan.h), so those cells leave the cluster graph; the planner refuses
    // a route to one, GameLogic/Movement.cpp falls the device back to Stop, and FINISH - seeing an idle
    // builder and an unfinished site - orders it at the same cell on the next decision, and the
    // next. It answers before the generator, the factory and the lab, so the commander stops
    // building anything at all: measured at 11,500 of 12,000 ticks spent ordering one truck at a
    // cell it could never reach. The invariant is cheap to state and it is the whole cure.
    Outpost::Sim sim = SliceMatch();
    std::uint32_t moves = 0;
    std::uint32_t arrivalsSeen = 0;
    for (std::uint32_t tick = 0; tick < 4000; ++tick)
    {
      sim.Advance();
      for (const Outpost::OrderQueue::Entry& entry : sim.Orders().Entries())
      {
        if (entry.arrival < arrivalsSeen || entry.order.kind != Outpost::OrderKind::Move)
        {
          continue;
        }
        arrivalsSeen = entry.arrival + 1;
        const Outpost::CellPosition target = MoveTargetCell(entry.order);
        ++moves;
        sim.Objects().ForEachStructure(
          [&](Outpost::ObjectId, const Outpost::Structure& _structure)
          {
            if (_structure.seat != entry.order.seat || !Outpost::Occupies(_structure.state))
            {
              return;
            }
            const Outpost::Footprint footprint = Outpost::FootprintOf(_structure, &sim.Content());
            if (Inside(footprint, target))
            {
              Assert::Fail((L"a builder was sent onto cell (" + std::to_wstring(target.x) + L"," + std::to_wstring(target.y) +
                            L"), which is inside its own commander's structure at (" + std::to_wstring(footprint.cellX) + L"," +
                            std::to_wstring(footprint.cellY) + L")")
                             .c_str());
            }
          });
      }
    }
    Assert::IsTrue(moves > 0, L"and the commanders really did order some walking");
    Logger::WriteMessage(
      ("    measured: " + std::to_string(moves) + " Move orders over 4,000 ticks, none onto a seat's own structure\n").c_str());
  }

  TEST_METHOD(ASiteNoBuilderCanBeGotToIsPassedOverAndTheNextOneIsWorked)
  {
    // The other half of the same defect. FINISH used to look at unfinishedPlaces.front() alone and
    // answer true for having ordered a truck at it, so ONE site nobody could reach stopped every
    // other site from being finished and stopped the behaviours below it from running at all - and
    // the slack DecideForSeat carries for exactly this case ("a deposit on a summit no drive can
    // climb") could never be used, because Finish answered first.
    Outpost::Sim sim = SliceMatch();
    // One tick, so that stage 7 has run and the ground round the base is EXPLORED. CheckPlacement
    // refuses an unexplored footprint, and a plan refused for that reason is cancelled by the
    // ABANDON behaviour before FINISH ever sees it - which would pass this test for the wrong
    // reason. Found by running it.
    sim.Advance();
    GiveFighterDesign(sim, 0);
    const Outpost::CellPosition start = Slice().starts[0];

    // The older site is walled in - every cell around it obstructed, as a structure's own cells
    // are - and the newer one is open ground beside the base.
    const Outpost::ObjectId sealed = PlacePlan(sim, 0, Outpost::StructureRole::Generator, start.x, start.y + 7);
    const Outpost::Structure* sealedStructure = sim.Objects().FindStructure(sealed);
    Assert::IsTrue(sealedStructure != nullptr);
    const Outpost::Footprint sealedFootprint = Outpost::FootprintOf(*sealedStructure, &sim.Content());
    WallIn(sim, sealedFootprint, 3);
    const Outpost::ObjectId open = PlacePlan(sim, 0, Outpost::StructureRole::Generator, start.x + 5, start.y);
    const Outpost::Structure* openStructure = sim.Objects().FindStructure(open);
    Assert::IsTrue(openStructure != nullptr);
    const Outpost::Footprint openFootprint = Outpost::FootprintOf(*openStructure, &sim.Content());

    // The seats already decided once on the tick above, so the queue is not empty; the orders of
    // THIS decision are the ones arriving from here on.
    const std::uint32_t watermark = sim.Orders().NextArrival();
    Outpost::DecideForSeat(sim, 0);

    bool orderedAtTheOpenSite = false;
    for (const Outpost::OrderQueue::Entry& entry : sim.Orders().Entries())
    {
      if (entry.arrival < watermark || entry.order.kind != Outpost::OrderKind::Move)
      {
        continue;
      }
      const Outpost::CellPosition target = MoveTargetCell(entry.order);
      Assert::IsFalse(Inside(sealedFootprint, target), L"never onto the sealed site itself");
      const std::int32_t x = static_cast<std::int32_t>(target.x) * CELL + CELL / 2;
      const std::int32_t z = static_cast<std::int32_t>(target.y) * CELL + CELL / 2;
      Assert::IsTrue(Outpost::DistanceSquaredTo(sealedFootprint, x, z) > CellsSquared(4), L"and not at the ring of wall around it either");
      orderedAtTheOpenSite = orderedAtTheOpenSite || Outpost::DistanceSquaredTo(openFootprint, x, z) <= CellsSquared(2);
    }
    Assert::IsTrue(orderedAtTheOpenSite, L"the older site was passed over and the next one was worked");
  }

  TEST_METHOD(APlanTheGroundWillNoLongerTakeIsCanceledRatherThanCountedForEver)
  {
    // THE THIRD DEFECT, and the one the plan's notes guessed wrong about (m1-vertical-slice/S13).
    // A plan does NOT occupy its cells (GameShared/Plan.h), so two plans may overlap; the first to begin
    // construction flattens and marks its footprint, and the other is refused by CheckPlacement
    // for as long as it exists. GameLogic/Construction.cpp is explicit that "a plan that can no longer be
    // built stays a plan until it is cancelled", which is right for a human who can see it and
    // press the button - and nothing was doing the cancelling for a scripted commander.
    //
    // IT STARVED ITSELF ON ITS OWN BOOKKEEPING: AiBlackboard counts what a commander holds "built
    // or building", so a dead plan still counted as the lab it was going to be, INDUSTRY never
    // placed another, and two builders stood beside it in reach with the power to build it.
    Outpost::Sim sim = SliceMatch();
    sim.Advance(); // Stage 7, so the ground is explored and the plan below starts out LEGAL
    GiveFighterDesign(sim, 0);
    const Outpost::CellPosition start = Slice().starts[0];

    const Outpost::ObjectId plan = PlacePlan(sim, 0, Outpost::StructureRole::ResearchLab, start.x + 5, start.y + 5);
    const Outpost::Structure* planned = sim.Objects().FindStructure(plan);
    Assert::IsTrue(planned != nullptr);
    const Outpost::Footprint footprint = Outpost::FootprintOf(*planned, &sim.Content());
    const Outpost::PlacementQuery query{&sim.Terrain(),  &sim.Objects(),
                                        &sim.Seats()[0], &sim.Content().structures.structures[planned->design],
                                        &sim.Content(),  &sim.Power().Deposits()};
    Assert::IsTrue(Outpost::CheckPlacement(footprint, query) == Outpost::PlacementFault::Accepted,
                   L"the site starts out legal, or this test proves nothing about why it is canceled");

    Outpost::AiBlackboard before;
    Outpost::Observe(sim, 0, before);
    Assert::AreEqual(1u, before.standing[static_cast<std::size_t>(Outpost::AiNeed::Lab)], L"the plan counts as the lab it will be");

    // The ground goes out from under it, by the one mechanism that does this in a real match: a
    // neighbour that OVERLAPPED the plan began construction, which is the moment it starts
    // occupying its cells (GameShared/Plan.h). Both were legal to place, because a plan occupies nothing,
    // and only one of them can ever be built. Obstructing a cell directly does NOT reproduce it -
    // CheckPlacement's Occupied test reads the world's structures and not the obstruction byte, so
    // the first version of this test set the byte, saw no refusal, and proved nothing.
    const Outpost::ObjectId neighbor = PlacePlan(sim, 0, Outpost::StructureRole::Generator, footprint.cellX + 1, footprint.cellY + 1);
    Outpost::Structure* begun = sim.Objects().FindStructure(neighbor);
    Assert::IsTrue(begun != nullptr);
    begun->state = Outpost::StructurePhase::UnderConstruction;
    Assert::IsTrue(Outpost::CheckPlacement(footprint, query) != Outpost::PlacementFault::Accepted, L"the site is refused now");

    const std::uint32_t watermark = sim.Orders().NextArrival();
    Outpost::DecideForSeat(sim, 0);
    bool canceled = false;
    for (const Outpost::OrderQueue::Entry& entry : sim.Orders().Entries())
    {
      canceled = canceled || (entry.arrival >= watermark && entry.order.kind == Outpost::OrderKind::CancelStructure &&
                              static_cast<std::uint32_t>(entry.order.operands[0]) == plan.value);
    }
    Assert::IsTrue(canceled, L"the commander canceled the plan it can no longer build");

    // And it really goes, so the count it was inflating comes back down and INDUSTRY can place
    // another. Two ticks, because an AI order is queued for tick t plus AI_ORDER_DELAY_TICKS.
    for (std::uint32_t tick = 0; tick <= Outpost::AI_ORDER_DELAY_TICKS; ++tick)
    {
      sim.Advance();
    }
    Assert::IsTrue(sim.Objects().FindStructure(plan) == nullptr, L"the plan is gone");
    Outpost::AiBlackboard after;
    Outpost::Observe(sim, 0, after);
    Assert::AreEqual(0u, after.standing[static_cast<std::size_t>(Outpost::AiNeed::Lab)], L"and is no longer counted as a lab");
  }

  TEST_METHOD(TheAcceptanceMatchHoldsOnTwoMoreSeeds)
  {
    // m1-vertical-slice/S13. The eye change broke the scripted commander on 0 of 8 seeds, so a fix
    // that worked on one seed would prove nothing; the plan's seed is the case above and these are
    // the others.
    //
    // TWO AND NOT EIGHT, RULED BY THE OWNER ON 2026-09-19 once the cost was measured against what
    // it bought. Eight came to about 143 seconds on the Windows runner - roughly 45% on top of a
    // 321-second test step, on every push, for the rest of the project. What it bought is small,
    // because the landscape is fixed and the seed perturbs the opening very little: over sixteen
    // seeds, first contact fell on tick 8,047 in EVERY one and the second commander's base was
    // identical in every one, with only the first commander's device count moving between 16 and
    // 20. They are nearly the same match, repeated. What this still defends against is a fix that
    // happened to suit one opening, and what actually caught the three regressions is cheap and
    // sits above: the never-onto-a-footprint invariant, the passed-over site and the cancelled
    // plan. The loop stops the moment a match has met every condition, which is exactly equivalent
    // to the assertion - they are asserted WITHIN 12,000 ticks - and is what keeps even this
    // affordable.
    const auto started = std::chrono::steady_clock::now();
    std::uint32_t decided = 0;
    for (std::uint32_t index = 1; index <= 2; ++index)
    {
      Outpost::MatchSettings settings = TwoScriptedSeats();
      settings.seed += index;
      Outpost::Sim sim(settings, Tables());
      Assert::IsTrue(sim.CreateLandscape(Slice()), L"the slice landscape did not generate");
      PlaceStartingBase(sim, 0, Slice().starts[0].x, Slice().starts[0].y);
      PlaceStartingBase(sim, 1, Slice().starts[1].x, Slice().starts[1].y);

      bool met = false;
      for (std::uint32_t tick = 0; tick < 12000 && !sim.Finished() && !met; ++tick)
      {
        sim.Advance();
        Outpost::AiBlackboard blackboard;
        Outpost::Observe(sim, 0, blackboard);
        met = blackboard.contact;
        for (std::uint8_t seat = 0; seat < 2 && met; ++seat)
        {
          met = CountStructures(sim, seat, Outpost::StructureRole::Extractor) > 0 &&
                CountStructures(sim, seat, Outpost::StructureRole::Generator) > 0 &&
                CountStructures(sim, seat, Outpost::StructureRole::Factory) > 0 &&
                CountStructures(sim, seat, Outpost::StructureRole::ResearchLab) > 0 && CountDevices(sim, seat) > 1;
        }
      }
      Assert::IsTrue(
        met,
        (L"seed " + std::to_wstring(settings.seed) + L": both commanders built an economy, an army and met, within 12,000 ticks").c_str());
      decided += met ? 1 : 0;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    Logger::WriteMessage(("    measured: " + std::to_string(decided) + " of 2 further seeds met the acceptance, in " +
                          std::to_string(elapsed.count()) + " ms\n")
                           .c_str());
  }

  TEST_METHOD(TwoSimsFromOneSeedAgreeHashForHash)
  {
    // The determinism the whole design rests on, over the one system that decides for itself. If a
    // scripted commander read wall time, a float or an unordered container, this is where it shows.
    Outpost::Sim first = SliceMatch();
    Outpost::Sim second = SliceMatch();
    Assert::AreEqual(first.ComputeHash(), second.ComputeHash(), L"the same setup is the same state");
    for (std::uint32_t tick = 0; tick < 3000; ++tick)
    {
      first.Advance();
      second.Advance();
      if (first.Hash() != second.Hash())
      {
        Assert::Fail((L"they diverged on tick " + std::to_wstring(first.Tick())).c_str());
      }
    }
    Assert::IsTrue(first.Objects().NextId() > 4, L"and the match really did happen");
  }

  TEST_METHOD(AScriptedMatchReachesADecidedVictoryState)
  {
    // The acceptance's second run. The survival clock is what makes it affordable in CI: a
    // 36,000-tick annihilation between two commanders who both rebuild can run for ever, and what
    // the acceptance asks for is that the match REACHES a decided state, not that one of them wins
    // by conquest. The duration is recorded in the task's notes.
    Outpost::Sim sim = SliceMatch(36000);
    const auto started = std::chrono::steady_clock::now();
    std::uint32_t ticks = 0;
    while (!sim.Finished() && ticks < 40000)
    {
      sim.Advance();
      ++ticks;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

    Assert::IsTrue(sim.Finished(), L"the match was decided");
    for (std::uint8_t seat = 0; seat < 2; ++seat)
    {
      Assert::IsTrue(sim.Seats()[seat].victory != Outpost::VictoryState::Playing, L"and every commander knows how it went");
    }
    Logger::WriteMessage(("    measured: a decided match took " + std::to_string(ticks) + " ticks and " + std::to_string(elapsed.count()) +
                          " ms, extracted " + std::to_string(sim.Seats()[0].extractedHundredths / 100) + " and " +
                          std::to_string(sim.Seats()[1].extractedHundredths / 100) + " power\n")
                           .c_str());
  }

  TEST_METHOD(AScriptedSeatEmitsItsOrdersThroughTheOrdinaryQueueForALaterTick)
  {
    // §7: "emits orders for tick t plus its delay through the same queue as a client". Nothing in
    // the AI reaches into the simulation directly, which is what makes an AI seat and a human seat
    // the same thing below stage 1.
    Outpost::Sim sim = SliceMatch();
    Assert::IsTrue(sim.Orders().Empty());
    Outpost::DecideForSeat(sim, 0);
    Assert::IsFalse(sim.Orders().Empty(), L"it decided something");
    const Outpost::Order& first = sim.Orders().Entries()[0].order;
    Assert::AreEqual(sim.Tick() + Outpost::AI_ORDER_DELAY_TICKS, first.tick, L"for a later tick");
    Assert::AreEqual(static_cast<int>(0), static_cast<int>(first.seat));
  }
};

} // namespace SimTests
