#include "pch.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr std::uint64_t MATCH_SEED = 20260922;

/// `GameDesign.md` section 10's fleet: fifty ships a player, and a base of four modules. With the station
/// that is 55 entities a player, so two players are the MVP's 110 and four are 220.
constexpr std::int32_t MINERS_PER_PLAYER = 25;
constexpr std::int32_t FIGHTERS_PER_PLAYER = 25;
constexpr std::size_t ENTITIES_PER_PLAYER = MINERS_PER_PLAYER + FIGHTERS_PER_PLAYER + 1 + 4;

/// Long enough that the miners are out, extracting and back, and every fighter fleet has crossed another;
/// short enough that the suite stays a few seconds.
constexpr int WARMUP_TICKS = 400;
constexpr int MEASURED_TICKS = 1200;

/// **A FLEET ORDER EVERY FIFTY TICKS, FOR EVERY PLAYER, INSIDE THE TIMED TICK.** A real host applies an
/// order in `DrainAndApply`, so ring slot assignment is part of the tick it lands on. Two and a half seconds
/// is far more often than a player orders a whole fleet, which is the point of a worst case.
constexpr int REORDER_EVERY_TICKS = 50;

/// ADR-002's tick.
constexpr double BUDGET_MILLISECONDS = 50.0;

/// **THE BUDGET IS ASSERTED ONLY IN AN OPTIMIZED BUILD.** A Debug tick at 220 entities peaked at 19 ms under
/// x64 emulation on the device, and CI runs `Debug|x64` on a runner nobody has timed. A Debug overrun
/// would be a fact about unoptimized code and not about the game, so it is logged there and not failed.
#if defined(NDEBUG)
constexpr bool ASSERT_BUDGET = true;
#else
constexpr bool ASSERT_BUDGET = false;
#endif

[[nodiscard]] bool WithinBudget(double _milliseconds) noexcept
{
  return !ASSERT_BUDGET || (_milliseconds < BUDGET_MILLISECONDS);
}

[[nodiscard]] Neuron::Vec2 Units(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(_x), .y = Neuron::FixedFromWholeUnits(_y)};
}

[[nodiscard]] Neuron::Vec2 Offset(const Neuron::Vec2& _from, std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _from.x + Neuron::FixedFromWholeUnits(_x), .y = _from.y + Neuron::FixedFromWholeUnits(_y)};
}

[[nodiscard]] Neuron::Vec2 StationOf(const Outpost::World& _world, Outpost::PlayerId _player)
{
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    const Outpost::Entity& entity = _world.EntityInSlot(slot);
    if (_world.IsSlotAlive(slot) && (entity.owner == _player) && (entity.design == Outpost::DesignId::Station))
    {
      return entity.position;
    }
  }
  return Neuron::Vec2{};
}

[[nodiscard]] std::uint16_t NearestHomeRock(const Outpost::World& _world, const Neuron::Vec2& _from)
{
  std::uint16_t best = 0;
  std::int64_t bestSquared = -1;
  const std::span<const Outpost::Placement> field = _world.Field();
  for (std::size_t index = 0; index < field.size(); ++index)
  {
    if (field[index].field != Outpost::FieldKind::Home)
    {
      continue;
    }
    const std::int64_t squared = Outpost::UniformGrid::DistanceSquared(field[index].position, _from);
    if ((bestSquared < 0) || (squared < bestSquared))
    {
      best = static_cast<std::uint16_t>(index);
      bestSquared = squared;
    }
  }
  return best;
}

/// One player's fleet at full stretch: four modules around the station, twenty-five miners on the nearest
/// home rock, and twenty-five fighters whose fleet order is re-issued by `Reorder`.
void Populate(Outpost::World& _world, Outpost::PlayerId _player, std::vector<Outpost::EntityId>& _outFighters)
{
  const Neuron::Vec2 station = StationOf(_world, _player);
  static_cast<void>(_world.Create(Offset(station, 300, 0), 0, Outpost::DesignId::ModuleShipyardL1, _player));
  static_cast<void>(_world.Create(Offset(station, -300, 0), 0, Outpost::DesignId::ModuleOreProcessorL1, _player));
  static_cast<void>(_world.Create(Offset(station, 0, 300), 0, Outpost::DesignId::ModuleShipyardL2, _player));
  static_cast<void>(_world.Create(Offset(station, 0, -300), 0, Outpost::DesignId::ModuleOreProcessorL2, _player));

  const std::uint16_t rock = NearestHomeRock(_world, station);
  for (std::int32_t index = 0; index < MINERS_PER_PLAYER; ++index)
  {
    const Outpost::EntityId miner =
      _world.Create(Offset(station, 450 + ((index % 5) * 70), -350 + ((index / 5) * 70)), 0, Outpost::DesignId::Miner, _player);
    static_cast<void>(_world.OrderMine(miner, rock));
  }
  for (std::int32_t index = 0; index < FIGHTERS_PER_PLAYER; ++index)
  {
    _outFighters.push_back(
      _world.Create(Offset(station, -450 - ((index % 5) * 70), -350 + ((index / 5) * 70)), 0, Outpost::DesignId::Fighter, _player));
  }
}

/// **EVERY PLAYER'S FIGHTERS TO THE MIDDLE OF THE MAP**, re-ordered every fifty ticks so ring assignment stays inside
/// the timed tick. Fleets from every side converge, cross and, since M3.2, fight there: the stations are about
/// 6,000 units out, so the fleets meet partway through the timed ticks and fight for the rest of them. Until M3.2
/// this alternated with a point near home every fifty ticks, which is 350 units of flying, so nobody ever met.
void Reorder(Outpost::World& _world, const std::vector<std::vector<Outpost::EntityId>>& _fighters)
{
  for (const std::vector<Outpost::EntityId>& fleet : _fighters)
  {
    static_cast<void>(Outpost::OrderFleetTo(_world, fleet, Units(0, 0)));
  }
}

struct TickCost
{
  std::size_t entities;
  double meanMilliseconds;
  double percentile99Milliseconds;
  double maximumMilliseconds;

  /// **PROOF THE WORLD WAS WORKING**, so a small figure cannot be an idle one: ships under a move order,
  /// averaged over the timed ticks, and the timed ticks on which ore was delivered.
  double movingShipsPerTick;
  std::size_t deliveringTicks;

  /// **ADR-014's OWED FIGURE**: the most fire events any one tick sent, which must stay under the accumulator's
  /// cap of 40. The fighters converge on the middle of the map, so the two sides fight there.
  std::size_t mostFireEventsInATick;
};

[[nodiscard]] std::size_t MovingShips(const Outpost::World& _world)
{
  std::size_t moving = 0;
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (_world.IsSlotAlive(slot) && _world.OrderInSlot(slot).active)
    {
      ++moving;
    }
  }
  return moving;
}

/// **EVERY HULL BACK TO FULL**, before each tick. Since M3.4 the fleets that meet in the middle kill each other,
/// and a cost measured on a world that is dying is a smaller world's: the figure is the design's 110 and 220, so
/// the count is held there. The weapons still fire and still settle their damage every tick; only the deaths
/// they would cause are kept out of the measurement.
void Mend(Outpost::World& _world)
{
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (_world.IsSlotAlive(slot))
    {
      Outpost::Entity& entity = _world.EntityInSlot(slot);
      entity.hullRemaining = static_cast<std::uint16_t>(Outpost::Derive(entity.design).hullPoints);
    }
  }
}

[[nodiscard]] TickCost Measure(std::size_t _players, bool _populated)
{
  Outpost::Host host;
  host.BeginMatch(MATCH_SEED, _players);

  std::vector<std::vector<Outpost::EntityId>> fighters(_populated ? _players : 0);
  for (std::size_t player = 0; player < fighters.size(); ++player)
  {
    Populate(host.MutableWorld(), static_cast<Outpost::PlayerId>(player + 1), fighters[player]);
  }

  std::vector<double> milliseconds;
  milliseconds.reserve(MEASURED_TICKS);
  std::size_t movingTotal = 0;
  std::size_t deliveringTicks = 0;
  std::size_t mostFireEvents = 0;
  for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick)
  {
    Mend(host.MutableWorld());
    const auto begun = std::chrono::steady_clock::now();
    if ((tick % REORDER_EVERY_TICKS) == 0)
    {
      Reorder(host.MutableWorld(), fighters);
    }
    host.RunOneTick();
    const auto ended = std::chrono::steady_clock::now();

    if (tick >= WARMUP_TICKS)
    {
      milliseconds.push_back(std::chrono::duration<double, std::milli>(ended - begun).count());
      movingTotal += MovingShips(host.CurrentWorld());
      deliveringTicks += host.CurrentMining().Deliveries().empty() ? 0 : 1;
      mostFireEvents = std::max(mostFireEvents, host.CurrentWeapons().Fired().size());
    }
  }

  const std::size_t entities = host.CurrentWorld().AliveCount();
  double total = 0.0;
  for (const double sample : milliseconds)
  {
    total += sample;
  }
  std::sort(milliseconds.begin(), milliseconds.end());
  return TickCost{.entities = entities,
                  .meanMilliseconds = total / static_cast<double>(milliseconds.size()),
                  .percentile99Milliseconds = milliseconds[(milliseconds.size() * 99) / 100],
                  .maximumMilliseconds = milliseconds.back(),
                  .movingShipsPerTick = static_cast<double>(movingTotal) / static_cast<double>(MEASURED_TICKS),
                  .deliveringTicks = deliveringTicks,
                  .mostFireEventsInATick = mostFireEvents};
}

void Report(const wchar_t* _label, const TickCost& _cost)
{
  Logger::WriteMessage((std::wstring{L"TICK COST "} + _label + L": " + std::to_wstring(_cost.entities) + L" entities, mean " +
                        std::to_wstring(_cost.meanMilliseconds) + L" ms, p99 " + std::to_wstring(_cost.percentile99Milliseconds) +
                        L" ms, max " + std::to_wstring(_cost.maximumMilliseconds) + L" ms over " + std::to_wstring(MEASURED_TICKS) +
                        L" ticks; " + std::to_wstring(_cost.movingShipsPerTick) + L" ships moving a tick, ore delivered on " +
                        std::to_wstring(_cost.deliveringTicks) + L" ticks; at most " + std::to_wstring(_cost.mostFireEventsInATick) +
                        L" fire events in a tick\n")
                         .c_str());
}
} // namespace

/// **M2.14, `TechnicalDesign.md` section 9.3 and ADR-002's owed measurement**: an empty tick and a full one,
/// at the MVP's 110 entities and at 220, against the 50 ms budget.
///
/// **IT IS A MEASUREMENT FIRST AND A TEST SECOND.** The figures go to the log, where a Release run on the
/// device reads them; the assertion is only that no tick overran the budget, and only in an optimized build. Wall time lives here in the suite and never in
/// `GameLogic` (R16): the host is timed from outside, exactly as `Server`'s shell would time it.
///
/// **THE HOST HAS NO CLIENTS HERE**, so the per-client accumulator and the sends cost nothing. That is ADR-024's
/// measurement, bounded from outside by ADR-022's harness; this one is the simulation.
TEST_CLASS(TheTickCost)
{
public:
  TEST_METHOD(AnEmptyTickFitsTheBudget)
  {
    const TickCost two = Measure(2, false);
    const TickCost four = Measure(4, false);
    Report(L"empty, 2 players", two);
    Report(L"empty, 4 players", four);
    Assert::IsTrue(WithinBudget(two.maximumMilliseconds));
    Assert::IsTrue(WithinBudget(four.maximumMilliseconds));
  }

  TEST_METHOD(AFullTickAt110EntitiesFitsTheBudget)
  {
    const TickCost full = Measure(2, true);
    Report(L"full, 2 players", full);
    Assert::IsTrue(full.movingShipsPerTick > (2 * 20.0), L"the fleets were not moving, so the figure is an idle world's");
    Assert::IsTrue(full.deliveringTicks > 0, L"no miner delivered, so the mining loop was not in the figure");
    Assert::AreEqual(2 * ENTITIES_PER_PLAYER, full.entities, L"the MVP's 110");
    Assert::IsTrue(full.mostFireEventsInATick > 0, L"the fleets never fought, so combat was not in the figure");
    Assert::IsTrue(full.mostFireEventsInATick < 40, L"ADR-014: fire events a tick at 110 entities must stay under the cap of 40");
    Assert::IsTrue(WithinBudget(full.maximumMilliseconds), L"a tick overran ADR-002's 50 ms");
  }

  TEST_METHOD(AFullTickAt220EntitiesFitsTheBudget)
  {
    const TickCost full = Measure(4, true);
    Report(L"full, 4 players", full);
    Assert::IsTrue(full.movingShipsPerTick > (4 * 20.0), L"the fleets were not moving, so the figure is an idle world's");
    Assert::IsTrue(full.deliveringTicks > 0, L"no miner delivered, so the mining loop was not in the figure");
    Assert::AreEqual(4 * ENTITIES_PER_PLAYER, full.entities, L"four players at the design's fleet");
    Assert::IsTrue(WithinBudget(full.maximumMilliseconds), L"a tick overran ADR-002's 50 ms");
  }
};

} // namespace GameLogicTests
