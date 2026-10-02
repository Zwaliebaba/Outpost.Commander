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

  TEST_METHOD(AShipInsideAnObstacleIsPushedOut)
  {
    Outpost::Simulation simulation(1, TICKS_PER_SECOND);
    simulation.PlaceMap(OpenMap({{.position = {}, .radiusMeters = 100.0f}}));
    const Outpost::EntityId ship = simulation.SpawnShip(BLUE, DESIGN, Movement(1, 1), {10.0f, 0.0f});
    (void)simulation.Tick({});
    Assert::IsTrue(Outpost::Distance(simulation.FindEntity(ship)->position, {}) >= 108.0f - 0.01f);
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
};
} // namespace GameLogicTests