#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::DesignId DESIGN{1};
constexpr int TICKS_PER_SECOND = 20;
constexpr float SECONDS_PER_TICK = 1.0f / TICKS_PER_SECOND;
// How far two settled ships' footprints may still overlap (plan task 2.4).
constexpr float OVERLAP_TOLERANCE_METERS = 0.5f;

// The movements the repository's tuning data gives the fastest and the slowest ship.
Outpost::ShipMovement Movement(std::uint32_t _hull, std::uint32_t _drive)
{
  return Outpost::MovementFor(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::HullId{_hull}, Outpost::DriveId{_drive});
}

// A map of open space, with the obstacles given.
Outpost::Map OpenMap(std::vector<Outpost::AsteroidFieldPlacement> _fields)
{
  return {.sizeMeters = 4000.0f, .minimumGapMeters = 60.0f, .starts = {}, .oreAsteroids = {}, .asteroidFields = std::move(_fields)};
}

Outpost::Command Move(std::vector<Outpost::EntityId> _ships, Outpost::PlanePosition _destination)
{
  return {.player = BLUE, .order = Outpost::MoveCommand{.ships = std::move(_ships), .destination = _destination}};
}

bool Arrived(const Outpost::Simulation& _simulation, Outpost::EntityId _ship)
{
  return !_simulation.FindEntity(_ship)->destination.has_value();
}

// How long a group is along its ships' mean heading for each meter it is wide across it.
float LengthPerWidth(const Outpost::Simulation& _simulation, std::span<const Outpost::EntityId> _ships)
{
  Outpost::PlaneVector heading{};
  Outpost::PlaneVector sum{};
  for (const Outpost::EntityId id : _ships)
  {
    const Outpost::Entity& ship = *_simulation.FindEntity(id);
    heading = heading + Outpost::PlaneVector{std::cos(ship.headingRadians), std::sin(ship.headingRadians)};
    sum = sum + (ship.position - Outpost::PlanePosition{});
  }
  heading = Outpost::Normalized(heading, {1.0f, 0.0f});
  const Outpost::PlanePosition center = Outpost::PlanePosition{} + sum * (1.0f / static_cast<float>(_ships.size()));
  float alongLeast = std::numeric_limits<float>::infinity();
  float alongMost = -alongLeast;
  float acrossLeast = alongLeast;
  float acrossMost = -alongLeast;
  for (const Outpost::EntityId id : _ships)
  {
    const Outpost::PlaneVector offset = _simulation.FindEntity(id)->position - center;
    alongLeast = std::min(alongLeast, Outpost::Dot(offset, heading));
    alongMost = std::max(alongMost, Outpost::Dot(offset, heading));
    acrossLeast = std::min(acrossLeast, Outpost::Dot(offset, Outpost::Perpendicular(heading)));
    acrossMost = std::max(acrossMost, Outpost::Dot(offset, Outpost::Perpendicular(heading)));
  }
  return (alongMost - alongLeast) / std::max(acrossMost - acrossLeast, 1.0f);
}

// Twenty-five ships five to a row, 30 m apart, at _x and square on z = 0.
std::vector<Outpost::EntityId> Block(Outpost::Simulation& _simulation, float _x)
{
  std::vector<Outpost::EntityId> ships;
  for (int i = 0; i < 25; ++i)
  {
    const int column = i % 5;
    const int row = i / 5;
    ships.push_back(_simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1),
                                          {_x + (static_cast<float>(column) * 30.0f), -60.0f + (static_cast<float>(row) * 30.0f)}));
  }
  return ships;
}

// Writes down every part a tick tells it of, where it begins and where it ends (task 8.1). An observer may not throw, so
// it keeps them in room set aside beforehand, and Take spells them "+name" and "-name".
class PartRecorder final : public Outpost::TickObserver
{
public:
  void Begin(Outpost::TickPart _part) noexcept override
  {
    Record(true, _part);
  }

  void End(Outpost::TickPart _part) noexcept override
  {
    Record(false, _part);
  }

  [[nodiscard]] std::string Take()
  {
    std::string parts;
    for (std::size_t i = 0; i < m_count; ++i)
      parts += std::format("{}{} ", m_marks[i].begins ? '+' : '-', Outpost::TickPartName(m_marks[i].part));
    m_count = 0;
    return parts;
  }

private:
  struct Mark
  {
    bool begins = false;
    Outpost::TickPart part = Outpost::TickPart::Commands;
  };

  void Record(bool _begins, Outpost::TickPart _part) noexcept
  {
    if (m_count < m_marks.size())
      m_marks[m_count++] = {.begins = _begins, .part = _part};
  }

  std::array<Mark, 64> m_marks{};
  std::size_t m_count = 0;
};
} // namespace

TEST_CLASS(MovementTests)
{
public:
  TEST_METHOD(DerivesMovementFromHullAndDrive)
  {
    const Outpost::ShipMovement smallIon = Movement(1, 1);
    Assert::AreEqual(78.0f, smallIon.speedMetersPerSecond);
    Assert::AreEqual(8.0f, smallIon.radiusMeters);
    // 180 degrees a second, times the Ion drive's 1.25.
    Assert::IsTrue(std::abs(smallIon.turnRateRadiansPerSecond - 3.9269908f) < 1e-5f);
    Assert::AreEqual(20.0f, Movement(3, 2).speedMetersPerSecond);
  }

  // Task 8.1: the simulation tells its observer where each part of a tick begins and ends, nested, in the order it runs
  // them; a graph built for an order is inside the order. It reads no clock itself (ADR-009), and it tells only the tick
  // it was given the observer for.
  TEST_METHOD(TellsItsObserverEachPartOfATick)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 150.0f}}));
    const Outpost::ShipMovement movement = Movement(1, 1);
    const Outpost::EntityId first = simulation.SpawnShip(BLUE, DESIGN, movement, {-400.0f, 10.0f});
    const Outpost::EntityId second = simulation.SpawnShip(BLUE, DESIGN, movement, {-400.0f, 40.0f});
    PartRecorder recorder;

    (void)simulation.Tick({Move({first, second}, {400.0f, 0.0f})}, &recorder);
    Assert::AreEqual(std::string("+commands +group_route +graph_build -graph_build -group_route +ship_paths -ship_paths -commands "
                                 "+fight -fight +targets -targets +move -move +separate -separate "),
                     recorder.Take());
    (void)simulation.Tick({}, &recorder);
    Assert::AreEqual(std::string("+commands -commands +fight -fight +targets -targets +move -move +separate -separate "), recorder.Take());

    // Neither a tick without it, nor a copy made after one with it, tells it anything.
    (void)simulation.Tick({});
    Outpost::Simulation copy = simulation;
    (void)copy.Tick({});
    Assert::AreEqual(std::string(), recorder.Take());
  }

  // Plan task 2.4: a ship reaches a target behind an obstacle, and never passes through it on the way.
  TEST_METHOD(AShipReachesATargetBehindAnObstacle)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 150.0f}}));
    const Outpost::ShipMovement movement = Movement(1, 1);
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, movement, {-400.0f, 10.0f});

    (void)simulation.Tick({Move({ship}, {400.0f, -10.0f})});
    int ticks = 1;
    for (; ticks < 30 * TICKS_PER_SECOND && !Arrived(simulation, ship); ++ticks)
    {
      (void)simulation.Tick({});
      Assert::IsTrue(Outpost::Distance(simulation.FindEntity(ship)->position, {}) >= 150.0f + movement.radiusMeters - 0.01f);
    }
    Assert::IsTrue(Arrived(simulation, ship), L"the ship never arrived");
    Assert::IsTrue(Outpost::Distance(simulation.FindEntity(ship)->position, {400.0f, -10.0f}) < 0.01f);
    // Round the obstacle rather than through it: the straight line is 800 m, 10.3 s at 78 m/s.
    Assert::IsTrue(static_cast<float>(ticks) * SECONDS_PER_TICK > 800.0f / movement.speedMetersPerSecond);
  }

  // Plan task 2.4: a mixed group arrives together, at its slowest member's pace.
  TEST_METHOD(AMixedGroupArrivesTogetherAtItsSlowestPace)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({}));
    const Outpost::ShipMovement fast = Movement(1, 1);
    const Outpost::ShipMovement slow = Movement(3, 2);
    const Outpost::EntityId fastShip = simulation.SpawnShip(BLUE, DESIGN, fast, {-300.0f, 40.0f});
    const Outpost::EntityId slowShip = simulation.SpawnShip(BLUE, DESIGN, slow, {-300.0f, -40.0f});
    const Outpost::PlanePosition slowStart = simulation.FindEntity(slowShip)->position;

    (void)simulation.Tick({Move({fastShip, slowShip}, {300.0f, 0.0f})});
    int fastArrival = 0;
    int slowArrival = 0;
    for (int tick = 1; tick < 60 * TICKS_PER_SECOND && (fastArrival == 0 || slowArrival == 0); ++tick)
    {
      (void)simulation.Tick({});
      if (fastArrival == 0 && Arrived(simulation, fastShip))
        fastArrival = tick;
      if (slowArrival == 0 && Arrived(simulation, slowShip))
        slowArrival = tick;
    }
    Assert::IsTrue(fastArrival > 0 && slowArrival > 0, L"a ship never arrived");
    // Together: within half a second of each other.
    Assert::IsTrue(std::abs(fastArrival - slowArrival) <= TICKS_PER_SECOND / 2,
                   (std::to_wstring(fastArrival) + L" and " + std::to_wstring(slowArrival)).c_str());
    // At the slow ship's pace: no sooner than it could cover its own way at its top speed.
    const float slowDistance = Outpost::Distance(slowStart, simulation.FindEntity(slowShip)->position);
    Assert::IsTrue(static_cast<float>(fastArrival) * SECONDS_PER_TICK >= slowDistance / slow.speedMetersPerSecond - 1.0f);
  }

  // Plan task 2.4: ships ordered from a heap end up side by side, their footprints overlapping by no more than the stated
  // tolerance once they have settled.
  TEST_METHOD(ShipsSettleWithoutOverlapping)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {200.0f, 60.0f}, .radiusMeters = 80.0f}}));
    std::vector<Outpost::EntityId> ships;
    for (int i = 0; i < 24; ++i)
    {
      // A heap: five to a row, two meters apart, so that every ship overlaps its neighbors.
      const int column = i % 5;
      const int row = i / 5;
      const Outpost::ShipMovement movement = i % 4 == 0 ? Movement(3, 2) : Movement(1, 1);
      ships.push_back(simulation.SpawnShip(BLUE, DESIGN, movement, {static_cast<float>(column) * 2.0f, static_cast<float>(row) * 2.0f}));
    }

    (void)simulation.Tick({Move(ships, {500.0f, 0.0f})});
    for (int tick = 0; tick < 90 * TICKS_PER_SECOND; ++tick)
      (void)simulation.Tick({});

    for (size_t a = 0; a < ships.size(); ++a)
    {
      const Outpost::Entity& first = *simulation.FindEntity(ships[a]);
      Assert::IsTrue(Arrived(simulation, ships[a]), L"a ship never arrived");
      for (size_t b = a + 1; b < ships.size(); ++b)
      {
        const Outpost::Entity& second = *simulation.FindEntity(ships[b]);
        const float overlap = first.radiusMeters + second.radiusMeters - Outpost::Distance(first.position, second.position);
        Assert::IsTrue(overlap <= OVERLAP_TOLERANCE_METERS, std::to_wstring(overlap).c_str());
      }
    }
  }

  // ADR-010: a group follows one route round an obstacle, and every ship of it arrives.
  TEST_METHOD(AGroupReachesATargetBehindAnObstacle)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 150.0f}}));
    std::vector<Outpost::EntityId> ships;
    for (int i = 0; i < 12; ++i)
    {
      // Four to a row, 40 m apart; every third ship a Medium hull.
      const int column = i % 4;
      const int row = i / 4;
      const Outpost::ShipMovement movement = i % 3 == 0 ? Movement(2, 1) : Movement(1, 1);
      ships.push_back(
        simulation.SpawnShip(BLUE, DESIGN, movement, {-500.0f + static_cast<float>(column) * 40.0f, static_cast<float>(row) * 40.0f}));
    }

    (void)simulation.Tick({Move(ships, {500.0f, 0.0f})});
    for (int tick = 0; tick < 60 * TICKS_PER_SECOND; ++tick)
    {
      (void)simulation.Tick({});
      for (const Outpost::EntityId id : ships)
      {
        const Outpost::Entity& ship = *simulation.FindEntity(id);
        Assert::IsTrue(Outpost::Distance(ship.position, {}) >= 150.0f + ship.radiusMeters - 0.01f);
      }
    }
    for (const Outpost::EntityId id : ships)
    {
      Assert::IsTrue(Arrived(simulation, id), L"a ship never arrived");
      Assert::IsTrue(Outpost::Distance(simulation.FindEntity(id)->position, {500.0f, 0.0f}) < 200.0f);
    }
  }

  // ADR-047: a group passes an obstacle side by side, each ship in its own lane, rather than in file through the one point
  // where the obstacle's edge leaves room. Without lanes, the group below is 4.9 times as long as it is wide as it passes;
  // in open space it is as long as it is wide.
  TEST_METHOD(AGroupPassesAnObstacleSideBySide)
  {
    constexpr float MOST_LENGTH_PER_WIDTH = 2.0f;
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 200.0f}}));
    std::vector<Outpost::EntityId> ships;
    for (int i = 0; i < 25; ++i)
    {
      // Five to a row, 30 m apart, square on the obstacle's middle.
      const int column = i % 5;
      const int row = i / 5;
      ships.push_back(simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1),
                                           {-1500.0f + (static_cast<float>(column) * 30.0f), -60.0f + (static_cast<float>(row) * 30.0f)}));
    }

    (void)simulation.Tick({Move(ships, {1500.0f, 0.0f})});
    float mostLengthPerWidth = 0.0f;
    for (int tick = 0;
         tick < 60 * TICKS_PER_SECOND && !std::ranges::all_of(ships, [&](Outpost::EntityId _id) { return Arrived(simulation, _id); });
         ++tick)
    {
      (void)simulation.Tick({});
      // How long the group is along its ships' mean heading, and how wide across it.
      Outpost::PlaneVector heading{};
      Outpost::PlaneVector sum{};
      for (const Outpost::EntityId id : ships)
      {
        const Outpost::Entity& ship = *simulation.FindEntity(id);
        heading = heading + Outpost::PlaneVector{std::cos(ship.headingRadians), std::sin(ship.headingRadians)};
        sum = sum + (ship.position - Outpost::PlanePosition{});
      }
      heading = Outpost::Normalized(heading, {1.0f, 0.0f});
      const Outpost::PlanePosition center = Outpost::PlanePosition{} + sum * (1.0f / static_cast<float>(ships.size()));
      float alongLeast = std::numeric_limits<float>::infinity();
      float alongMost = -alongLeast;
      float acrossLeast = alongLeast;
      float acrossMost = -alongLeast;
      for (const Outpost::EntityId id : ships)
      {
        const Outpost::PlaneVector offset = simulation.FindEntity(id)->position - center;
        alongLeast = std::min(alongLeast, Outpost::Dot(offset, heading));
        alongMost = std::max(alongMost, Outpost::Dot(offset, heading));
        acrossLeast = std::min(acrossLeast, Outpost::Dot(offset, Outpost::Perpendicular(heading)));
        acrossMost = std::max(acrossMost, Outpost::Dot(offset, Outpost::Perpendicular(heading)));
      }
      mostLengthPerWidth = std::max(mostLengthPerWidth, (alongMost - alongLeast) / std::max(acrossMost - acrossLeast, 1.0f));
    }
    for (const Outpost::EntityId id : ships)
      Assert::IsTrue(Arrived(simulation, id), L"a ship never arrived");
    Assert::IsTrue(mostLengthPerWidth <= MOST_LENGTH_PER_WIDTH, std::to_wstring(mostLengthPerWidth).c_str());
  }

  // Phase 1 plan task 7.3, carried over to Phase 2 (ADR-047): a group given an Attack order on one enemy passes an
  // obstacle side by side too, and keeps its lanes as it paths again after its moving target. Its ships all close on one
  // point, so past the obstacle the group narrows toward it, and the bound is looser than a move's: one shared route made
  // it 6.2 times as long as wide.
  TEST_METHOD(AnAttackingGroupPassesAnObstacleSideBySide)
  {
    constexpr float MOST_LENGTH_PER_WIDTH = 2.5f;
    constexpr Outpost::PlayerId RED{2};
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 200.0f}}));
    const std::vector<Outpost::EntityId> ships = Block(simulation, -1500.0f);
    // A slow target that combat can touch and that fires at nothing, moving away across the far side of the obstacle.
    const Outpost::DesignId targetDesign =
      simulation.SaveDesign(RED, "Target", {},
                            {.movement = {.speedMetersPerSecond = 5.0f, .turnRateRadiansPerSecond = 1.0f, .radiusMeters = 10.0f},
                             .hitPointsHundredths = 100'000'000});
    const Outpost::EntityId target = simulation.SpawnShip(RED, targetDesign, {1500.0f, 0.0f});
    (void)simulation.Tick({{.player = RED, .order = Outpost::MoveCommand{.ships = {target}, .destination = {1500.0f, 1500.0f}}},
                           {.player = BLUE, .order = Outpost::AttackCommand{.ships = ships, .target = target}}});

    float mostLengthPerWidth = 0.0f;
    std::uint32_t repaths = 0;
    const auto centerX = [&]
    {
      float sum = 0.0f;
      for (const Outpost::EntityId id : ships)
        sum += simulation.FindEntity(id)->position.xMeters;
      return sum / static_cast<float>(ships.size());
    };
    for (int tick = 0; tick < 60 * TICKS_PER_SECOND && centerX() < 300.0f; ++tick)
    {
      const Outpost::PlanePosition chased = simulation.FindEntity(ships.front())->chasedPosition;
      (void)simulation.Tick({});
      repaths += simulation.FindEntity(ships.front())->chasedPosition == chased ? 0 : 1;
      mostLengthPerWidth = std::max(mostLengthPerWidth, LengthPerWidth(simulation, ships));
    }
    Assert::IsTrue(centerX() >= 300.0f, L"the group never passed the obstacle");
    Assert::IsTrue(repaths >= 2, L"the target never moved far enough to path after");
    Assert::IsTrue(mostLengthPerWidth <= MOST_LENGTH_PER_WIDTH, std::to_wstring(mostLengthPerWidth).c_str());
  }

  TEST_METHOD(AShipInsideAnObstacleIsPushedOut)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 100.0f}}));
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1), {10.0f, 0.0f});
    (void)simulation.Tick({});
    Assert::IsTrue(Outpost::Distance(simulation.FindEntity(ship)->position, {}) >= 108.0f - 0.01f);
  }

  // Task 7.2, ADR-039: a ship turns in an arc, as an aircraft does. Sent to a point behind it, it never stands to turn:
  // it moves every tick, turns no faster than its turn rate, comes about in a loop to one side at half its cruise speed
  // or less, and arrives on the point.
  TEST_METHOD(AShipComesAboutInALoop)
  {
    for (const Outpost::ShipMovement movement : {Movement(1, 1), Movement(3, 2)})
    {
      Outpost::Simulation simulation(1, TICKS_PER_SECOND);
      simulation.PlaceMap(OpenMap({}));
      const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, movement, {});
      const Outpost::PlanePosition goal{-150.0f, 0.0f};
      (void)simulation.Tick({Move({ship}, goal)});
      float widestMeters = 0.0f;
      for (int tick = 0; tick < 60 * TICKS_PER_SECOND && !Arrived(simulation, ship); ++tick)
      {
        const Outpost::Entity before = *simulation.FindEntity(ship);
        (void)simulation.Tick({});
        const Outpost::Entity& after = *simulation.FindEntity(ship);
        const float stepMeters = Outpost::Distance(before.position, after.position);
        Assert::IsTrue(stepMeters > 0.0f, L"the ship stood to turn");
        const float turned = std::abs(std::remainder(after.headingRadians - before.headingRadians, 2.0f * std::numbers::pi_v<float>));
        Assert::IsTrue(turned <= (movement.turnRateRadiansPerSecond * SECONDS_PER_TICK) + 1e-4f, L"it turned faster than it can");
        if (std::abs(std::remainder(before.headingRadians, 2.0f * std::numbers::pi_v<float>)) < std::numbers::pi_v<float> / 2.0f)
          Assert::IsTrue(stepMeters <= (movement.speedMetersPerSecond * SECONDS_PER_TICK / 2.0f) + 1e-3f,
                         L"it came about faster than half its cruise speed");
        widestMeters = std::max(widestMeters, std::abs(after.position.zMeters));
      }
      Assert::IsTrue(Arrived(simulation, ship), L"the ship never arrived");
      Assert::IsTrue(Outpost::Distance(simulation.FindEntity(ship)->position, goal) < 0.01f);
      // A loop to one side, about twice its turn radius at half speed across.
      const float halfSpeedRadiusMeters = movement.speedMetersPerSecond / 2.0f / movement.turnRateRadiansPerSecond;
      Assert::IsTrue(widestMeters > halfSpeedRadiusMeters && widestMeters < 3.0f * halfSpeedRadiusMeters);
    }
  }

  // Task 7.2, ADR-039: a point inside the circle a ship turns at full speed, 15 m abeam of a ship whose turn at full speed
  // is 20 m across, is reached on a tighter arc, flown slower, and not circled.
  TEST_METHOD(AShipReachesAPointInsideItsTurn)
  {
    const Outpost::ShipMovement movement = Movement(1, 1);
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({}));
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, movement, {});
    const Outpost::PlanePosition goal{0.0f, 15.0f};
    (void)simulation.Tick({Move({ship}, goal)});
    int ticks = 1;
    for (; ticks < 10 * TICKS_PER_SECOND && !Arrived(simulation, ship); ++ticks)
      (void)simulation.Tick({});
    Assert::IsTrue(Arrived(simulation, ship), L"the ship never arrived");
    Assert::IsTrue(Outpost::Distance(simulation.FindEntity(ship)->position, goal) < 0.01f, L"it gave up short of the point");
    // A quarter turn takes 0.4 s at its 225 degrees a second; circling the point would take seconds.
    Assert::IsTrue(ticks <= TICKS_PER_SECOND, L"it circled the point");
  }

  TEST_METHOD(StopHaltsAMovingShip)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({}));
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1), {});
    (void)simulation.Tick({Move({ship}, {1000.0f, 0.0f})});
    (void)simulation.Tick({{.player = BLUE, .order = Outpost::StopCommand{.ships = {ship}}}});
    const Outpost::PlanePosition stopped = simulation.FindEntity(ship)->position;
    for (int tick = 0; tick < TICKS_PER_SECOND; ++tick)
      (void)simulation.Tick({});
    Assert::IsTrue(simulation.FindEntity(ship)->position == stopped);
  }

  TEST_METHOD(RefusesAHullWiderThanTheMapsNarrowestPassage)
  {
    Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    tuning.hulls.back().footprintRadiusMeters = 31.0;
    Assert::ExpectException<Neuron::Exception>(
      [&tuning] { Outpost::InProcessServer server(tuning, Outpost::LoadMap(ReadRepositoryMap()), {.seed = 1}); });
  }

  // ADR-032: an order for more than SPLIT_ORDER_SHIPS ships plans half its paths in its tick and the rest in the next. Its
  // ships hold, with no order, until the group sets off together in the second tick, and then all arrive. An order of
  // SPLIT_ORDER_SHIPS sets off in its own tick, as every order did before.
  TEST_METHOD(PlansALargeOrderOverTwoTicks)
  {
    const auto spawnGroup = [](Outpost::Simulation& _simulation, std::size_t _count)
    {
      std::vector<Outpost::EntityId> ships;
      ships.reserve(_count);
      for (std::size_t i = 0; i < _count; ++i)
      {
        const std::size_t row = i / 8;
        ships.push_back(_simulation.SpawnShip(
          BLUE, DESIGN, Movement(1, 1), {-600.0f + (static_cast<float>(i % 8) * 25.0f), -100.0f + (static_cast<float>(row) * 25.0f)}));
      }
      return ships;
    };
    const Outpost::PlanePosition destination{600.0f, 0.0f};

    Outpost::Simulation atLimit(1, TICKS_PER_SECOND);
    atLimit.PlaceMap(OpenMap({{.position = {0.0f, 0.0f}, .radiusMeters = 150.0f}}));
    const std::vector<Outpost::EntityId> fleet = spawnGroup(atLimit, Outpost::Simulation::SPLIT_ORDER_SHIPS);
    (void)atLimit.Tick({Move(fleet, destination)});
    Assert::IsTrue(std::ranges::all_of(fleet, [&](Outpost::EntityId _id) { return !atLimit.FindEntity(_id)->path.empty(); }),
                   L"a group of the limit sets off at once");

    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {0.0f, 0.0f}, .radiusMeters = 150.0f}}));
    const std::vector<Outpost::EntityId> ships = spawnGroup(simulation, Outpost::Simulation::SPLIT_ORDER_SHIPS + 8);
    (void)simulation.Tick({Move(ships, destination)});
    for (const Outpost::EntityId id : ships)
    {
      const Outpost::Entity& ship = *simulation.FindEntity(id);
      Assert::IsTrue(ship.path.empty() && ship.order == Outpost::ShipOrder::None, L"holding while the group plans");
    }
    // A copy part-way through planning is the same simulation, and plans the same.
    Outpost::Simulation copy = simulation;
    Assert::IsTrue(copy == simulation);

    (void)simulation.Tick({});
    (void)copy.Tick({});
    Assert::IsTrue(copy == simulation);
    float slowestSeconds = 0.0f;
    for (const Outpost::EntityId id : ships)
    {
      const Outpost::Entity& ship = *simulation.FindEntity(id);
      Assert::IsTrue(!ship.path.empty() && ship.order == Outpost::ShipOrder::Move, L"set off together");
      Assert::IsTrue(ship.destination == destination);
      float meters = Outpost::Distance(ship.position, ship.path.front());
      for (std::size_t i = 1; i < ship.path.size(); ++i)
        meters += Outpost::Distance(ship.path[i - 1], ship.path[i]);
      slowestSeconds = std::max(slowestSeconds, meters / ship.cruiseSpeedMetersPerSecond);
    }
    // At one pace: every ship's path takes the slowest's time.
    for (const Outpost::EntityId id : ships)
    {
      const Outpost::Entity& ship = *simulation.FindEntity(id);
      float meters = Outpost::Distance(ship.position, ship.path.front());
      for (std::size_t i = 1; i < ship.path.size(); ++i)
        meters += Outpost::Distance(ship.path[i - 1], ship.path[i]);
      Assert::AreEqual(slowestSeconds, meters / ship.cruiseSpeedMetersPerSecond, slowestSeconds * 1e-3f);
    }

    for (int tick = 0; tick < 60 * TICKS_PER_SECOND; ++tick)
      (void)simulation.Tick({});
    Assert::IsTrue(std::ranges::all_of(ships, [&](Outpost::EntityId _id) { return Arrived(simulation, _id); }));
  }

  // ADR-032: a ship ordered again while its group plans leaves the group; the rest set off without it.
  TEST_METHOD(AShipOrderedAgainLeavesItsPlanningGroup)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({}));
    std::vector<Outpost::EntityId> ships;
    ships.reserve(Outpost::Simulation::SPLIT_ORDER_SHIPS + 4);
    for (std::size_t i = 0; i < Outpost::Simulation::SPLIT_ORDER_SHIPS + 4; ++i)
    {
      const std::size_t row = i / 6;
      ships.push_back(
        simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1), {static_cast<float>(i % 6) * 30.0f, static_cast<float>(row) * 30.0f}));
    }
    const Outpost::EntityId stopped = ships.front();
    const Outpost::EntityId elsewhere = ships.back();
    (void)simulation.Tick({Move(ships, {800.0f, 0.0f}), {.player = BLUE, .order = Outpost::StopCommand{.ships = {stopped}}}});
    (void)simulation.Tick({Move({elsewhere}, {-800.0f, 0.0f})});
    Assert::IsTrue(simulation.FindEntity(stopped)->path.empty() && simulation.FindEntity(stopped)->order == Outpost::ShipOrder::None);
    Assert::IsTrue(simulation.FindEntity(elsewhere)->destination == Outpost::PlanePosition{-800.0f, 0.0f}, L"its own order stands");
    Assert::IsTrue(simulation.FindEntity(ships[1])->destination == Outpost::PlanePosition{800.0f, 0.0f});
  }

  // ADR-032: a structure placed drops the path graphs, and the quiet ticks after it build them again, one a tick, so that
  // the next order finds them built; a tick that plans paths builds none of its own accord.
  TEST_METHOD(RebuildsDroppedGraphsOnQuietTicks)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {0.0f, 0.0f}, .radiusMeters = 150.0f}}));
    simulation.PreparePathfinding(8.0f);
    simulation.PreparePathfinding(14.0f);
    (void)simulation.SpawnStructure(BLUE, Outpost::StructureKind::Shipyard, {400.0f, 400.0f}, 40.0f);
    PartRecorder recorder;
    (void)simulation.Tick({}, &recorder);
    Assert::AreEqual(std::string("+commands -commands +fight -fight +targets -targets +move -move +separate -separate +graph_build "
                                 "-graph_build "),
                     recorder.Take());
    (void)simulation.Tick({}, &recorder);
    Assert::IsTrue(recorder.Take().ends_with("+graph_build -graph_build "), L"the second graph");
    (void)simulation.Tick({}, &recorder);
    Assert::IsFalse(recorder.Take().contains("graph_build"), L"both are built");

    // A ship's order finds its graph built.
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1), {-600.0f, 0.0f});
    (void)simulation.Tick({Move({ship}, {600.0f, 0.0f})}, &recorder);
    Assert::IsFalse(recorder.Take().contains("graph_build"));
  }
};
} // namespace GameLogicTests