#include "pch.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] Neuron::Vec2 At(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE};
}

[[nodiscard]] Outpost::Placement RockAt(std::int32_t _x, std::int32_t _y) noexcept
{
  return Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid, .field = Outpost::FieldKind::Home, .position = At(_x, _y)};
}

[[nodiscard]] std::int64_t UnitsBetween(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  return Neuron::Sqrt(Outpost::UniformGrid::DistanceSquared(_a, _b)) / Neuron::FIXED_ONE;
}

/// **A RAID AT THE HOME FIELD.** A station at the origin, a rock 2,000 east, a Miner working it, and whatever the
/// test adds. One tick is the host's order: movement, weapons, mining with what the weapons struck, deaths.
struct Raid
{
  Outpost::World world;
  Outpost::WeaponSystem weapons;
  Outpost::MiningSystem mining;
  Outpost::DeathSystem deaths;
  Outpost::EntityId station{};
  std::uint32_t tick = 0;

  /// False runs the same tick with flight switched off, by telling the miners nothing was fired on.
  bool flight = true;

  explicit Raid(std::vector<Outpost::Placement> _field = {RockAt(2000, 0)})
  {
    world.SetField(std::move(_field));
    station = world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE);
  }

  void Step()
  {
    Outpost::Tick(world);
    weapons.Advance(world, tick);
    mining.Advance(world, flight ? weapons.Struck() : std::span<const Outpost::EntityId>{});
    deaths.Advance(world);
    ++tick;
  }

  [[nodiscard]] const Outpost::MineOrder& MineOf(Outpost::EntityId _miner) const
  {
    return *world.FindMine(_miner);
  }
};
} // namespace

/// M3.6, `OpenQuestions.md` Q64. **A miner at work that is fired on runs for home**; the player's move order never does.
TEST_CLASS(MinerFlight)
{
public:
  /// A Miner extracting at the rock, a raider arriving 400 east of it: it flees with its cargo and ends nearer home.
  TEST_METHOD(AWorkingMinerUnderFireFleesHome)
  {
    Raid raid;
    const Outpost::EntityId miner = raid.world.Create(At(1850, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(raid.world.OrderMine(miner, 0));
    for (int tick = 0; tick < 40; ++tick)
    {
      raid.Step();
    }
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::Extracting);
    const std::uint32_t cargo = raid.MineOf(miner).cargoMilliOre;
    Assert::IsTrue(cargo > 0);

    static_cast<void>(raid.world.Create(At(2250, 0), Neuron::Angle{32768}, Outpost::DesignId::Fighter, THEIRS));
    for (int tick = 0; (tick < 40) && (raid.MineOf(miner).phase != Outpost::MiningPhase::Fleeing); ++tick)
    {
      raid.Step();
    }
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::Fleeing, L"it kept working under fire");
    Assert::AreEqual(cargo, raid.MineOf(miner).cargoMilliOre, L"it dropped or kept adding to its cargo");
    Assert::AreEqual(std::uint16_t{0}, raid.MineOf(miner).rock, L"it forgot its rock");

    // IT RUNS UNTIL IT IS OUT OF REACH AND THREE SECONDS CALM, and then turns back for its rock (Q64): a flight is
    // a retreat out of the raider's 600, not a trip all the way home.
    std::int64_t nearestHome = UnitsBetween(raid.world.Find(miner)->position, At(0, 0));
    for (int tick = 0; tick < 200; ++tick)
    {
      raid.Step();
      if (raid.world.IsAlive(miner))
      {
        nearestHome = std::min(nearestHome, UnitsBetween(raid.world.Find(miner)->position, At(0, 0)));
      }
    }
    Assert::IsTrue(raid.world.IsAlive(miner), L"a Scout at 100 units a second should outrun one Fighter's reach");
    Assert::IsTrue(nearestHome < 1550, L"it did not run for home");
  }

  /// **A MOVE ORDER IS THE PLAYER'S OVERRIDE**: it ends the mine order, and a miner under one keeps its course.
  TEST_METHOD(AMinerUnderAMoveOrderDoesNotFlee)
  {
    Raid raid;
    const Outpost::EntityId miner = raid.world.Create(At(1850, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(raid.world.OrderMoveTo(miner, At(1850, 3000), Outpost::SpeedPerTick(Outpost::DesignId::Miner),
                                          Outpost::TurnAnglePerTick(Outpost::DesignId::Miner)));
    static_cast<void>(raid.world.Create(At(2250, 0), Neuron::Angle{32768}, Outpost::DesignId::Fighter, THEIRS));

    for (int tick = 0; tick < 40; ++tick)
    {
      raid.Step();
    }
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::None, L"a moved miner took up flight");
    Assert::IsTrue(raid.world.FindOrder(miner)->destination == At(1850, 3000), L"its course was changed");
  }

  /// Three seconds without being fired on and it goes back to its rock; two seconds and fifty-nine ticks, it does not.
  TEST_METHOD(ItGoesBackAfterThreeCalmSeconds)
  {
    Raid raid;
    const Outpost::EntityId miner = raid.world.Create(At(1000, 0), 0, Outpost::DesignId::Miner, MINE);
    Assert::IsTrue(raid.world.OrderMine(miner, 0));

    const std::vector<Outpost::EntityId> struck{miner};
    raid.mining.Advance(raid.world, struck);
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::Fleeing);

    for (std::uint16_t tick = 1; tick < Outpost::FLEE_CALM_TICKS; ++tick)
    {
      raid.mining.Advance(raid.world);
    }
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::Fleeing, L"it went back early");

    // Fired on again resets the three seconds.
    raid.mining.Advance(raid.world, struck);
    for (std::uint16_t tick = 1; tick < Outpost::FLEE_CALM_TICKS; ++tick)
    {
      raid.mining.Advance(raid.world);
    }
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::Fleeing, L"being fired on again did not reset it");

    raid.mining.Advance(raid.world);
    Assert::IsTrue(raid.MineOf(miner).phase == Outpost::MiningPhase::ToOre, L"it never went back to its rock");
  }

  /// The flee path is the same on every run: two raids from one start end in one state.
  TEST_METHOD(TheFleePathIsDeterministic)
  {
    std::uint64_t hashes[2]{};
    for (std::uint64_t& hash : hashes)
    {
      Raid raid;
      for (std::int32_t index = 0; index < 3; ++index)
      {
        const Outpost::EntityId miner = raid.world.Create(At(1850, (index - 1) * 80), 0, Outpost::DesignId::Miner, MINE);
        static_cast<void>(raid.world.OrderMine(miner, 0));
      }
      for (int tick = 0; tick < 40; ++tick)
      {
        raid.Step();
      }
      static_cast<void>(raid.world.Create(At(2300, 0), Neuron::Angle{32768}, Outpost::DesignId::Fighter, THEIRS));
      for (int tick = 0; tick < 300; ++tick)
      {
        raid.Step();
      }
      hash = Outpost::StateHash(raid.world);
    }
    Assert::AreEqual(hashes[0], hashes[1]);
  }

  /// **`GameDesign.md` SECTION 7'S FLIGHT ROW, RE-MEASURED ON THE SHIPPED FIELD.** Six miners at a rock 1,500 from
  /// their station, three Fighters arriving 600 past them and holding there. The figure is logged for §7; the test
  /// asserts only that flight saves miners that staying would lose.
  TEST_METHOD(FlightSavesMinersInARaid)
  {
    std::size_t lost[2]{};
    for (int run = 0; run < 2; ++run)
    {
      Raid raid({RockAt(1500, 0)});
      raid.flight = (run == 0);
      std::vector<Outpost::EntityId> miners;
      for (std::int32_t index = 0; index < 6; ++index)
      {
        miners.push_back(raid.world.Create(At(1400 + ((index % 2) * 70), ((index / 2) - 1) * 80), 0, Outpost::DesignId::Miner, MINE));
        static_cast<void>(raid.world.OrderMine(miners.back(), 0));
      }
      for (int tick = 0; tick < 20; ++tick)
      {
        raid.Step();
      }
      for (std::int32_t index = 0; index < 3; ++index)
      {
        static_cast<void>(raid.world.Create(At(2000, (index - 1) * 90), Neuron::Angle{32768}, Outpost::DesignId::Fighter, THEIRS));
      }
      for (int tick = 0; tick < 60 * 20; ++tick)
      {
        raid.Step();
      }
      for (const Outpost::EntityId miner : miners)
      {
        lost[run] += raid.world.IsAlive(miner) ? 0 : 1;
      }
    }
    Logger::WriteMessage((std::wstring{L"MINER FLIGHT: of six miners 1,500 out, three holding raiders kill "} + std::to_wstring(lost[0]) +
                          L" with flight and " + std::to_wstring(lost[1]) + L" without, in a minute\n")
                           .c_str());
    Assert::IsTrue(lost[0] < lost[1], L"flight saved nobody");
  }
};

} // namespace GameLogicTests
