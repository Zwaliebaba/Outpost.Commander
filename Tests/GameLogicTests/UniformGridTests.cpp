#include "pch.h"

#include <algorithm>
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

[[nodiscard]] Neuron::Fixed Units(std::int32_t _units) noexcept
{
  return _units * Neuron::FIXED_ONE;
}

/// THE ANSWER WITHOUT THE GRID: every live entity, tested one by one, in index order -- which for a world
/// that has never freed a slot is identity order. What the grid returns must be exactly this.
[[nodiscard]] std::vector<Outpost::EntityId> BruteForce(const Outpost::World& _world, const Neuron::Vec2& _center, Neuron::Fixed _radius)
{
  std::vector<Outpost::EntityId> out;
  const std::int64_t radiusSquared = static_cast<std::int64_t>(_radius) * _radius;
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (_world.IsSlotAlive(slot) && (Outpost::UniformGrid::DistanceSquared(_world.EntityInSlot(slot).position, _center) <= radiusSquared))
    {
      out.push_back(_world.EntityInSlot(slot).id);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

[[nodiscard]] bool TakeAnything(const Outpost::Entity&) noexcept
{
  return true;
}

/// A deterministic scatter over the whole square and a little past it, from the tree's own PRNG.
void Scatter(Outpost::World& _world, std::size_t _count, std::uint64_t _seed)
{
  Neuron::Pcg32 random{_seed};
  for (std::size_t index = 0; index < _count; ++index)
  {
    const std::int32_t x = random.NextInRange(-8400, 8400);
    const std::int32_t y = random.NextInRange(-8400, 8400);
    static_cast<void>(_world.Create(At(x, y), 0, Outpost::DesignId::Miner));
  }
}
} // namespace

/// M2.5. **The grid is allowed to be wrong about nothing**: every query is compared with the answer a
/// brute-force walk gives, so the grid can only ever be a faster way of saying the same thing.
TEST_CLASS(TheUniformGrid)
{
public:
  /// `TechnicalDesign.md` section 2's figures, and that they cover ADR-001's square exactly.
  TEST_METHOD(ThirtyTwoCellsOfFiveHundredTwelveCoverTheSquare)
  {
    Assert::AreEqual(512, Outpost::UniformGrid::CELL_UNITS);
    Assert::AreEqual(32, Outpost::UniformGrid::CELLS_PER_SIDE);
    Assert::AreEqual(std::size_t{1024}, Outpost::UniformGrid::CELL_COUNT);
    Assert::AreEqual(std::size_t{0}, Outpost::UniformGrid::CellOf(At(-8192, -8192)));
    Assert::AreEqual(std::size_t{1023}, Outpost::UniformGrid::CellOf(At(8191, 8191)));
  }

  /// **THE EDGES OF A CELL.** A cell owns its low edge and not its high one, and a position past the square
  /// clamps to the edge cell rather than falling off the grid.
  TEST_METHOD(ACellOwnsItsLowEdgeAndThePastSquareClamps)
  {
    Assert::AreEqual(std::size_t{16}, Outpost::UniformGrid::CellOf(At(0, -8192)), L"x = 0 is the first unit of column 16");
    Assert::AreEqual(std::size_t{15}, Outpost::UniformGrid::CellOf(Neuron::Vec2{.x = -1, .y = Units(-8192)}),
                     L"one step below zero is column 15");
    Assert::AreEqual(std::size_t{1}, Outpost::UniformGrid::CellOf(At(-7680, -8192)));
    Assert::AreEqual(std::size_t{31}, Outpost::UniformGrid::CellOf(At(8192, -8192)), L"the far edge clamps in");
    Assert::AreEqual(std::size_t{0}, Outpost::UniformGrid::CellOf(At(-20000, -20000)), L"far outside clamps to the corner");
    Assert::AreEqual(std::size_t{1023}, Outpost::UniformGrid::CellOf(At(20000, 20000)));
  }

  /// **EVERY ENTITY WITHIN THE RADIUS AND NOTHING OUTSIDE IT**, against the brute force, over a scatter
  /// that reaches past the square and query circles of every size from inside one cell to wider than the
  /// map -- centered in cells, on cell corners, and at the edges and corners of the square.
  TEST_METHOD(AQueryIsExactlyTheBruteForceAnswer)
  {
    Outpost::World world;
    Scatter(world, 600, 20260922);
    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    Assert::AreEqual(world.AliveCount(), grid.EntryCount(), L"the rebuild dropped something");

    const std::vector<Neuron::Vec2> centers{At(0, 0),       At(512, 512),    At(-8192, -8192), At(8192, 8192), At(8191, -8192),
                                            At(-8192, 300), At(3000, -4700), At(-9000, 9000),  At(255, -257)};
    const std::vector<std::int32_t> radii{0, 1, 100, 511, 512, 513, 1500, 4000, 20000};

    std::vector<Outpost::EntityId> found;
    for (const Neuron::Vec2& center : centers)
    {
      for (const std::int32_t radius : radii)
      {
        grid.Query(world, center, Units(radius), found);
        Assert::IsTrue(found == BruteForce(world, center, Units(radius)), L"the grid disagreed with the brute force");
      }
    }
  }

  /// **ACROSS A CELL BOUNDARY, BOTH SIDES.** Two entities a unit either side of the line between columns 15
  /// and 16, and a circle centered on neither cell's middle.
  TEST_METHOD(AQueryReachesAcrossCellBoundaries)
  {
    Outpost::World world;
    const Outpost::EntityId left = world.Create(At(-1, 0), 0, Outpost::DesignId::Miner);
    const Outpost::EntityId right = world.Create(At(1, 0), 0, Outpost::DesignId::Miner);
    Outpost::UniformGrid grid;
    grid.Rebuild(world);

    std::vector<Outpost::EntityId> found;
    grid.Query(world, At(0, 0), Units(1), found);
    Assert::AreEqual(std::size_t{2}, found.size());
    Assert::IsTrue((found[0] == left) && (found[1] == right));
  }

  /// **ON THE CIRCLE IS INSIDE IT**, and one fixed-point step past it is not.
  TEST_METHOD(TheRadiusIsInclusive)
  {
    Outpost::World world;
    static_cast<void>(world.Create(At(300, 400), 0, Outpost::DesignId::Miner));
    Outpost::UniformGrid grid;
    grid.Rebuild(world);

    std::vector<Outpost::EntityId> found;
    grid.Query(world, At(0, 0), Units(500), found);
    Assert::AreEqual(std::size_t{1}, found.size(), L"a 3-4-5 triangle's corner is on the circle");
    grid.Query(world, At(0, 0), Units(500) - 1, found);
    Assert::AreEqual(std::size_t{0}, found.size());
    grid.Query(world, At(0, 0), -1, found);
    Assert::AreEqual(std::size_t{0}, found.size(), L"a negative radius finds nothing");
  }

  /// **THE RETURNED ORDER IS BY IDENTITY, AND ASSERTED TO BE** -- including after slots are freed and reused,
  /// when index order and creation order stop being the same thing and a newer entity can sit in a lower
  /// cell than an older one.
  TEST_METHOD(ResultsAreInIdentityOrderAfterReuse)
  {
    Outpost::World world;
    Scatter(world, 300, 7);
    for (std::size_t slot = 0; slot < world.SlotCount(); slot += 3)
    {
      static_cast<void>(world.Destroy(world.EntityInSlot(slot).id));
    }
    Scatter(world, 120, 8);

    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    std::vector<Outpost::EntityId> found;
    grid.Query(world, At(0, 0), Units(20000), found);
    Assert::AreEqual(world.AliveCount(), found.size());
    Assert::IsTrue(std::is_sorted(found.begin(), found.end()), L"a query left in cell order");
    Assert::IsTrue(std::adjacent_find(found.begin(), found.end()) == found.end(), L"an entity returned twice");
  }

  /// An entity destroyed after the rebuild is not returned, because its identity no longer resolves.
  TEST_METHOD(AStaleIdentityIsSkipped)
  {
    Outpost::World world;
    const Outpost::EntityId gone = world.Create(At(10, 10), 0, Outpost::DesignId::Miner);
    const Outpost::EntityId kept = world.Create(At(20, 20), 0, Outpost::DesignId::Miner);
    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    static_cast<void>(world.Destroy(gone));

    std::vector<Outpost::EntityId> found;
    grid.Query(world, At(0, 0), Units(100), found);
    Assert::AreEqual(std::size_t{1}, found.size());
    Assert::IsTrue(found[0] == kept);
  }
};

/// The query that reaches an outcome: which one is nearest.
TEST_CLASS(TheNearestQuery)
{
public:
  /// **TWO AT EXACTLY EQUAL DISTANCE RESOLVE TO THE SAME ONE, EVERY RUN: THE LOWER IDENTITY.** Four
  /// entities on the axes around a point, in four different cells, created so that the lowest identity is
  /// NOT in the first cell a row-major walk visits -- so a tie broken on cell order would pick another.
  TEST_METHOD(AnExactTieResolvesToTheLowerIdentity)
  {
    Outpost::World world;
    const Outpost::EntityId east = world.Create(At(300, 0), 0, Outpost::DesignId::Miner);
    static_cast<void>(world.Create(At(-300, 0), 0, Outpost::DesignId::Miner));
    static_cast<void>(world.Create(At(0, -300), 0, Outpost::DesignId::Miner));
    static_cast<void>(world.Create(At(0, 300), 0, Outpost::DesignId::Miner));

    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    std::vector<Outpost::EntityId> scratch;
    for (int run = 0; run < 10; ++run)
    {
      Assert::IsTrue(grid.Nearest(world, At(0, 0), Units(1000), TakeAnything, scratch) == east, L"a tie broke on something but identity");
    }
  }

  /// Strictly nearer wins whatever its identity, and the filter is applied before distance.
  TEST_METHOD(TheNearestAcceptedEntityWins)
  {
    Outpost::World world;
    static_cast<void>(world.Create(At(100, 0), 0, Outpost::DesignId::Miner, 2));
    const Outpost::EntityId near1 = world.Create(At(200, 0), 0, Outpost::DesignId::Miner, 1);
    static_cast<void>(world.Create(At(50, 0), 0, Outpost::DesignId::Miner, 2));
    static_cast<void>(world.Create(At(300, 0), 0, Outpost::DesignId::Miner, 1));

    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    std::vector<Outpost::EntityId> scratch;
    const Outpost::EntityId found =
      grid.Nearest(world, At(0, 0), Units(1000), [](const Outpost::Entity& _entity) noexcept { return _entity.owner == 1; }, scratch);
    Assert::IsTrue(found == near1);
  }

  TEST_METHOD(NothingInRangeIsNoEntity)
  {
    Outpost::World world;
    static_cast<void>(world.Create(At(5000, 0), 0, Outpost::DesignId::Miner));
    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    std::vector<Outpost::EntityId> scratch;
    Assert::IsFalse(grid.Nearest(world, At(0, 0), Units(1000), TakeAnything, scratch).IsValid());
  }

  /// **THE BRUTE FORCE AGAIN**, for nearest over the scatter: the lowest identity among those at the least
  /// distance, from many centers.
  TEST_METHOD(NearestIsTheBruteForceAnswer)
  {
    Outpost::World world;
    Scatter(world, 400, 99);
    Outpost::UniformGrid grid;
    grid.Rebuild(world);
    std::vector<Outpost::EntityId> scratch;

    Neuron::Pcg32 random{123};
    for (int probe = 0; probe < 200; ++probe)
    {
      const Neuron::Vec2 center = At(random.NextInRange(-8192, 8192), random.NextInRange(-8192, 8192));
      const Neuron::Fixed radius = Units(random.NextInRange(0, 3000));

      Outpost::EntityId expected = Outpost::NO_ENTITY;
      std::int64_t expectedSquared = 0;
      for (const Outpost::EntityId id : BruteForce(world, center, radius))
      {
        const std::int64_t squared = Outpost::UniformGrid::DistanceSquared(world.Find(id)->position, center);
        if (!expected.IsValid() || (squared < expectedSquared))
        {
          expected = id;
          expectedSquared = squared;
        }
      }
      Assert::IsTrue(grid.Nearest(world, center, radius, TakeAnything, scratch) == expected);
    }
  }
};

} // namespace GameLogicTests
