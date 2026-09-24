#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] Neuron::Vec2 Units(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(_x), .y = Neuron::FixedFromWholeUnits(_y)};
}

[[nodiscard]] Outpost::EntityId Select(const Outpost::World& _world, Outpost::EntityId _shooter, bool _requireArc)
{
  Outpost::UniformGrid grid;
  grid.Rebuild(_world);
  std::vector<Outpost::EntityId> scratch;
  return Outpost::SelectTarget(_world, grid, *_world.Find(_shooter), _requireArc, scratch);
}
} // namespace

/// M3.2: what a ship picks to shoot at.
TEST_CLASS(TheTargeting)
{
public:
  TEST_METHOD(ReachIsTheCatalogs)
  {
    const Outpost::Reach fighter = Outpost::ReachOf(Outpost::DesignId::Fighter);
    Assert::IsTrue(fighter.armed);
    Assert::AreEqual(600u, fighter.rangeUnits);
    Assert::AreEqual(std::uint16_t{8192}, fighter.arcHalfAngle, L"Q68: 45 degrees either side");

    const Outpost::Reach station = Outpost::ReachOf(Outpost::DesignId::Station);
    Assert::AreEqual(400u, station.rangeUnits);
    Assert::AreEqual(std::uint16_t{32768}, station.arcHalfAngle, L"Q68: point defense is all around");

    Assert::IsFalse(Outpost::ReachOf(Outpost::DesignId::Miner).armed, L"a mining laser does no damage");
  }

  TEST_METHOD(ATargetExactlyOnTheRangeBoundaryIsInRange)
  {
    // Inclusive, and compared squared in Fixed, so the boundary resolves the same way on every run.
    Assert::IsTrue(Outpost::InRange(Units(0, 0), Units(600, 0), 600));
    Assert::IsFalse(Outpost::InRange(Units(0, 0), Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(600) + 1, .y = 0}, 600));
    Assert::IsTrue(Outpost::InRange(Units(0, 0), Units(360, 480), 600), L"a 3-4-5 triangle lands exactly on it");
  }

  TEST_METHOD(TwoAtEqualDistanceSelectTheLowerIdentity)
  {
    for (const bool reversed : {false, true})
    {
      Outpost::World world;
      const Outpost::EntityId shooter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
      const Outpost::EntityId first = world.Create(Units(0, reversed ? -300 : 300), 0, Outpost::DesignId::Miner, THEIRS);
      static_cast<void>(world.Create(Units(0, reversed ? 300 : -300), 0, Outpost::DesignId::Miner, THEIRS));

      Assert::IsTrue(Select(world, shooter, false) == first, L"a tie went to the higher identity");
    }
  }

  TEST_METHOD(ANearerHostileWins)
  {
    Outpost::World world;
    const Outpost::EntityId shooter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    static_cast<void>(world.Create(Units(500, 0), 0, Outpost::DesignId::Miner, THEIRS));
    const Outpost::EntityId nearer = world.Create(Units(200, 100), 0, Outpost::DesignId::Miner, THEIRS);
    Assert::IsTrue(Select(world, shooter, false) == nearer);
  }

  TEST_METHOD(AMoverOnlyTargetsInsideItsArc)
  {
    // Q78: under a move order a ship fires at will, but never turns for it, so a hostile off its arc is not a
    // target however near it is.
    Outpost::World world;
    const Outpost::EntityId shooter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId beside = world.Create(Units(0, 200), 0, Outpost::DesignId::Miner, THEIRS);
    const Outpost::EntityId ahead = world.Create(Units(450, 100), 0, Outpost::DesignId::Miner, THEIRS);

    Assert::IsTrue(Select(world, shooter, false) == beside, L"an idle ship takes the nearest, and turns to it");
    Assert::IsTrue(Select(world, shooter, true) == ahead, L"a mover takes the nearest inside its arc");
  }

  TEST_METHOD(OwnAndNobodysAreNeverTargets)
  {
    Outpost::World world;
    const Outpost::EntityId shooter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    static_cast<void>(world.Create(Units(100, 0), 0, Outpost::DesignId::Miner, MINE));
    static_cast<void>(world.Create(Units(150, 0), 0, Outpost::DesignId::Miner, Outpost::NO_PLAYER));
    Assert::IsFalse(Select(world, shooter, false).IsValid());
  }

  TEST_METHOD(NothingPastTheRangeIsSelected)
  {
    Outpost::World world;
    const Outpost::EntityId shooter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    static_cast<void>(world.Create(Units(601, 0), 0, Outpost::DesignId::Miner, THEIRS));
    Assert::IsFalse(Select(world, shooter, false).IsValid());
  }
};

} // namespace GameLogicTests
