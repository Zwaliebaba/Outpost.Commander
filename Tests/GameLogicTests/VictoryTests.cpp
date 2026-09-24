#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
[[nodiscard]] Neuron::Vec2 At(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE};
}

/// Two or more seats, a station each, and a Fighter and a Miner each.
struct Match
{
  Outpost::World world;
  Outpost::BuildSystem build;
  Outpost::DeathSystem deaths;
  Outpost::Victory victory;
  std::vector<Outpost::EntityId> stations;

  explicit Match(std::size_t _players)
  {
    build.Begin(_players);
    victory.Begin(_players, 0);
    for (std::size_t player = 1; player <= _players; ++player)
    {
      const auto owner = static_cast<Outpost::PlayerId>(player);
      const std::int32_t x = static_cast<std::int32_t>(player) * 5000;
      stations.push_back(world.Create(At(x, 0), 0, Outpost::DesignId::Station, owner));
      static_cast<void>(world.Create(At(x, 1000), 0, Outpost::DesignId::Fighter, owner));
      static_cast<void>(world.Create(At(x, -1000), 0, Outpost::DesignId::Miner, owner));
    }
  }

  /// Deaths and then victory, as the host ends its tick.
  void End(std::uint32_t _tick)
  {
    deaths.Advance(world);
    victory.Advance(world, build, _tick);
  }

  [[nodiscard]] std::size_t Owned(Outpost::PlayerId _player) const
  {
    std::size_t count = 0;
    for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
    {
      count += (world.IsSlotAlive(slot) && (world.EntityInSlot(slot).owner == _player)) ? 1 : 0;
    }
    return count;
  }
};
} // namespace

/// M3.7, `GameDesign.md` section 2, `OpenQuestions.md` Q65. **The last station standing wins.**
TEST_CLASS(TheVictory)
{
public:
  /// Losing the station eliminates the player and removes everything they own on the same tick.
  TEST_METHOD(LosingTheStationRemovesEverythingOnOneTick)
  {
    Match match(3);
    match.world.Find(match.stations[1])->hullRemaining = 0;
    match.End(100);

    Assert::AreEqual(std::size_t{0}, match.Owned(2), L"an eliminated player kept something");
    Assert::AreEqual(std::size_t{2}, match.victory.Removed().size(), L"the Fighter and the Miner went with it");
    Assert::AreEqual(std::size_t{3}, match.Owned(1));
    Assert::IsFalse(match.victory.Outcome().over, L"two stations still stand");
  }

  /// The last station standing wins.
  TEST_METHOD(TheLastStationStandingWins)
  {
    Match match(2);
    match.world.Find(match.stations[0])->hullRemaining = 0;
    match.End(100);
    Assert::IsTrue(match.victory.Outcome().over);
    Assert::AreEqual(Outpost::PlayerId{2}, match.victory.Outcome().winner);
    Assert::IsFalse(match.victory.Outcome().onClock);

    // And nothing moves once it is over.
    match.world.Find(match.stations[1])->hullRemaining = 0;
    match.End(101);
    Assert::AreEqual(Outpost::PlayerId{2}, match.victory.Outcome().winner);
  }

  /// **EVERY REMAINING STATION DYING ON ONE TICK IS A DRAW**, and the same draw on every run, whatever the order.
  TEST_METHOD(TheLastTwoStationsDyingTogetherIsADraw)
  {
    for (int run = 0; run < 2; ++run)
    {
      Match match(2);
      match.world.Find(match.stations[run])->hullRemaining = 0;
      match.world.Find(match.stations[1 - run])->hullRemaining = 0;
      match.End(100);
      Assert::IsTrue(match.victory.Outcome().over);
      Assert::AreEqual(Outpost::NO_PLAYER, match.victory.Outcome().winner, L"a simultaneous end picked a winner");
      Assert::AreEqual(std::size_t{0}, match.world.AliveCount(), L"the eliminated kept something");
    }
  }

  /// **AT SIX MINUTES THE CLOCK DECIDES**, on station hull first.
  TEST_METHOD(TheClockGoesToTheMostStationHull)
  {
    Match match(2);
    match.world.Find(match.stations[0])->hullRemaining = 7000;
    match.world.Find(match.stations[1])->hullRemaining = 6000;
    match.End(Outpost::MATCH_CLOCK_TICKS - 2);
    Assert::IsFalse(match.victory.Outcome().over, L"it ended before six minutes");
    match.End(Outpost::MATCH_CLOCK_TICKS - 1);
    Assert::IsTrue(match.victory.Outcome().over);
    Assert::IsTrue(match.victory.Outcome().onClock);
    Assert::AreEqual(Outpost::PlayerId{1}, match.victory.Outcome().winner);
  }

  /// A tie on hull goes to credits plus the catalog cost of what is still standing; a tie on that is a draw.
  TEST_METHOD(ATieOnHullGoesToWorthAndThenToADraw)
  {
    Match even(2);
    even.End(Outpost::MATCH_CLOCK_TICKS - 1);
    Assert::IsTrue(even.victory.Outcome().over);
    Assert::AreEqual(Outpost::NO_PLAYER, even.victory.Outcome().winner, L"two identical sides did not draw");

    Match richer(2);
    richer.build.Grant(2, 100);
    richer.End(Outpost::MATCH_CLOCK_TICKS - 1);
    Assert::AreEqual(Outpost::PlayerId{2}, richer.victory.Outcome().winner, L"credits did not break the tie");
  }

  /// A match of one seat has nobody to outlast: it runs to the clock.
  TEST_METHOD(ASoloMatchEndsOnlyOnTheClock)
  {
    Match match(1);
    match.End(100);
    Assert::IsFalse(match.victory.Outcome().over, L"one seat won by outlasting nobody");
    match.End(Outpost::MATCH_CLOCK_TICKS - 1);
    Assert::IsTrue(match.victory.Outcome().over);
    Assert::AreEqual(Outpost::PlayerId{1}, match.victory.Outcome().winner);
  }
};

/// M3.8b. **Elimination takes a player's modules with their ships**, in the same tick and so the same update.
TEST_CLASS(TheEliminatedModules)
{
public:
  TEST_METHOD(ModulesGoWithTheShips)
  {
    Match match(2);
    static_cast<void>(match.world.Create(At(5000, 400), 0, Outpost::DesignId::ModuleShipyardL1, 1));
    static_cast<void>(match.world.Create(At(5000, -400), 0, Outpost::DesignId::ModuleOreProcessorL2, 1));
    match.world.Find(match.stations[0])->hullRemaining = 0;
    match.End(100);
    Assert::AreEqual(std::size_t{0}, match.Owned(1), L"a module outlived its station");
    Assert::AreEqual(std::size_t{4}, match.victory.Removed().size(), L"the Fighter, the Miner and both modules");
  }
};

} // namespace GameLogicTests
