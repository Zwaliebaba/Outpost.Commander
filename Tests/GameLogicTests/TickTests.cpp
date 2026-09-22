#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
[[nodiscard]] Neuron::Vec2 At(Neuron::Fixed _x, Neuron::Fixed _y) noexcept
{
  return Neuron::Vec2{.x = _x, .y = _y};
}

/// A hull number with no meaning -- R24's catalog does not exist yet and the state hash only needs
/// the field to be there and to be covered.
// Any hull; the tick does not read it. M1.1 made this an identity rather than a loose 3.
inline constexpr Outpost::HullId SOME_HULL = Outpost::HullId::Station;
} // namespace

TEST_CLASS(EntityIdentity)
{
public:
  TEST_METHOD(ADefaultIdentityIsNobody)
  {
    Assert::IsFalse(Outpost::NO_ENTITY.IsValid());

    Outpost::World world;
    Assert::IsFalse(world.IsAlive(Outpost::NO_ENTITY));
    Assert::IsNull(world.Find(Outpost::NO_ENTITY));
  }

  TEST_METHOD(TheFirstAllocationOfASlotIsGenerationOne)
  {
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(first.IsValid());
    Assert::AreEqual(std::uint16_t{0}, first.index);
    Assert::AreEqual(std::uint16_t{1}, first.generation);
  }

  TEST_METHOD(AFreedAndReusedIndexCarriesANewGeneration)
  {
    // M0.8's first exit criterion, and the reason an identity is two numbers rather than one.
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.Destroy(first));

    const Outpost::EntityId second = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::AreEqual(first.index, second.index, L"the freed index should have been reused");
    Assert::AreNotEqual(first.generation, second.generation, L"a reused slot must not reissue a generation");

    // And the stale one does not resolve, which is the property the generation exists for: an
    // index alone would have found the new occupant and reported it as the old entity.
    Assert::IsFalse(world.IsAlive(first));
    Assert::IsNull(world.Find(first));
    Assert::IsTrue(world.IsAlive(second));
    Assert::IsNotNull(world.Find(second));
  }

  TEST_METHOD(AnIdentityGoesStaleAtTheDeathRatherThanAtTheReuse)
  {
    // Nothing has taken the slot yet, and the old identity is already gone. That matters because
    // a system holding a reference across a death must not see it come back to life.
    Outpost::World world;
    const Outpost::EntityId doomed = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.Destroy(doomed));
    Assert::IsFalse(world.IsAlive(doomed));
    Assert::AreEqual(std::size_t{0}, world.AliveCount());
  }

  TEST_METHOD(DestroyingTwiceIsNotAnError)
  {
    Outpost::World world;
    const Outpost::EntityId victim = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.Destroy(victim));
    Assert::IsFalse(world.Destroy(victim), L"a thing killed twice in one tick is ordinary");
  }

  TEST_METHOD(TheFreeListIsTakenFromTheEndItWasPushedOn)
  {
    // NOT A PERFORMANCE TEST. The tick iterates in index order, so the sequence in which recycled
    // indices come back out decides the iteration order of everything after a death. Pinning it
    // here is what stops a well-meaning change to the free list becoming a desynchronization that
    // only two machines can see.
    Outpost::World world;
    const Outpost::EntityId zero = world.Create(At(0, 0), 0, SOME_HULL);
    const Outpost::EntityId one = world.Create(At(0, 0), 0, SOME_HULL);
    const Outpost::EntityId two = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::AreEqual(std::uint16_t{0}, zero.index);
    Assert::AreEqual(std::uint16_t{1}, one.index);
    Assert::AreEqual(std::uint16_t{2}, two.index);

    Assert::IsTrue(world.Destroy(zero));
    Assert::IsTrue(world.Destroy(one));

    // Pushed 0 then 1; taken from the back, so 1 comes first.
    Assert::AreEqual(std::uint16_t{1}, world.Create(At(0, 0), 0, SOME_HULL).index);
    Assert::AreEqual(std::uint16_t{0}, world.Create(At(0, 0), 0, SOME_HULL).index);

    // And once the free list is empty the store grows again rather than reusing a live slot.
    Assert::AreEqual(std::uint16_t{3}, world.Create(At(0, 0), 0, SOME_HULL).index);
  }

  TEST_METHOD(TheStoreNeverShrinks)
  {
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_HULL);
    static_cast<void>(world.Create(At(0, 0), 0, SOME_HULL));
    Assert::AreEqual(std::size_t{2}, world.SlotCount());
    Assert::IsTrue(world.Destroy(first));
    Assert::AreEqual(std::size_t{2}, world.SlotCount(), L"removing a slot would renumber every index above it");
    Assert::AreEqual(std::size_t{1}, world.AliveCount());
  }

  TEST_METHOD(IdentitiesHaveATotalOrder)
  {
    // ADR-002 requires ties to break on identity. That needs an ordering to exist at all.
    const Outpost::EntityId low{.index = 1, .generation = 5};
    const Outpost::EntityId high{.index = 2, .generation = 1};
    Assert::IsTrue(low < high, L"index orders before generation");
    Assert::IsTrue(Outpost::EntityId{.index = 1, .generation = 4} < low);
    Assert::IsTrue(low == (Outpost::EntityId{.index = 1, .generation = 5}));
  }
};

TEST_CLASS(Movement)
{
public:
  TEST_METHOD(ItArrivesExactlyAndStops)
  {
    Outpost::World world;
    const Outpost::EntityId mover = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.OrderMoveTo(mover, At(1000, 0), 7));

    for (int tick = 0; tick < 500; ++tick)
    {
      Outpost::Tick(world);
    }

    const Outpost::Entity* entity = world.Find(mover);
    Assert::IsNotNull(entity);
    Assert::AreEqual(Neuron::Fixed{1000}, entity->position.x, L"it did not land exactly on the destination");
    Assert::AreEqual(Neuron::Fixed{0}, entity->position.y);
    Assert::IsFalse(world.FindOrder(mover)->active, L"an arrived order should have cleared itself");
  }

  TEST_METHOD(ItDoesNotOscillateAcrossTheDestination)
  {
    // THE CLASSIC FIXED-POINT FAILURE M0.8 NAMES. A step taken without an arrival guard carries
    // the entity past the point, the next tick carries it back, and it shivers there forever.
    // Seven does not divide a thousand, so the last step is a partial one and this is exactly the
    // arrangement that would oscillate.
    Outpost::World world;
    const Outpost::EntityId mover = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.OrderMoveTo(mover, At(1000, 0), 7));

    for (int tick = 0; tick < 200; ++tick)
    {
      Outpost::Tick(world);
    }
    const Neuron::Vec2 settled = world.Find(mover)->position;

    for (int tick = 0; tick < 200; ++tick)
    {
      Outpost::Tick(world);
      Assert::IsTrue(world.Find(mover)->position == settled, L"the entity moved after it had arrived");
    }
  }

  TEST_METHOD(AShortDiagonalDoesNotStall)
  {
    // The mirror image of oscillation, and the one a truncating divide produces: at a delta of
    // (3, 3) with a step of 1 the floored distance is 4, both components truncate 3/4 to zero,
    // and the entity never moves again. Rounding is what makes this arrive.
    Outpost::World world;
    const Outpost::EntityId mover = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.OrderMoveTo(mover, At(3, 3), 1));

    for (int tick = 0; tick < 32; ++tick)
    {
      Outpost::Tick(world);
    }

    Assert::IsTrue(world.Find(mover)->position == At(3, 3), L"a sub-unit diagonal stalled short of its destination");
    Assert::IsFalse(world.FindOrder(mover)->active);
  }

  TEST_METHOD(ItArrivesFromEveryDirection)
  {
    // Rounding that is not symmetric about zero shows up here as one quadrant arriving and
    // another stalling or overshooting.
    const Neuron::Fixed offsets[] = {-1237, -256, -3, 3, 256, 1237};
    for (const Neuron::Fixed x : offsets)
    {
      for (const Neuron::Fixed y : offsets)
      {
        Outpost::World world;
        const Outpost::EntityId mover = world.Create(At(x, y), 0, SOME_HULL);
        Assert::IsTrue(world.OrderMoveTo(mover, At(0, 0), 7));

        for (int tick = 0; tick < 1000; ++tick)
        {
          Outpost::Tick(world);
        }

        Assert::IsTrue(world.Find(mover)->position == At(0, 0), L"an approach direction failed to arrive");
        Assert::IsFalse(world.FindOrder(mover)->active);
      }
    }
  }

  TEST_METHOD(ASpeedOfZeroNeverArrives)
  {
    // Legal, and the honest outcome: somewhere to be and no way to get there beats a teleport.
    Outpost::World world;
    const Outpost::EntityId stuck = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.OrderMoveTo(stuck, At(1000, 0), 0));

    for (int tick = 0; tick < 100; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(stuck)->position == At(0, 0));
    Assert::IsTrue(world.FindOrder(stuck)->active, L"an unreachable order should still be outstanding");
  }

  TEST_METHOD(AnEntityWithNoOrderDoesNotMove)
  {
    Outpost::World world;
    const Outpost::EntityId idle = world.Create(At(42, -42), 0, SOME_HULL);
    for (int tick = 0; tick < 20; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(idle)->position == At(42, -42));
  }

  TEST_METHOD(ADeadEntityIsNotTicked)
  {
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_HULL);
    const Outpost::EntityId second = world.Create(At(0, 0), 0, SOME_HULL);
    Assert::IsTrue(world.OrderMoveTo(first, At(1000, 0), 7));
    Assert::IsTrue(world.OrderMoveTo(second, At(0, 1000), 7));
    Assert::IsTrue(world.Destroy(first));

    for (int tick = 0; tick < 500; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(second)->position == At(0, 1000));
    Assert::AreEqual(std::size_t{1}, world.AliveCount());
  }
};

TEST_CLASS(StateHashing)
{
public:
  TEST_METHOD(OneInputHashesTheSameTwice)
  {
    // M0.8's third exit criterion, and the smallest form of the claim ADR-002 actually owes: the
    // same inputs reach the same state. The real one, across four configuration and platform
    // pairs, waits for a determinism test with orders in it.
    const auto build = [](Outpost::World& _world)
    {
      const Outpost::EntityId a = _world.Create(At(100, 200), 1000, Outpost::HullId::Frigate);
      const Outpost::EntityId b = _world.Create(At(-50, 75), 40000, Outpost::HullId::Cruiser);
      static_cast<void>(_world.OrderMoveTo(a, At(900, 200), 7));
      static_cast<void>(_world.OrderMoveTo(b, At(-50, -900), 13));
      for (int tick = 0; tick < 60; ++tick)
      {
        Outpost::Tick(_world);
      }
    };

    Outpost::World first;
    Outpost::World second;
    build(first);
    build(second);
    Assert::AreEqual(Outpost::StateHash(first), Outpost::StateHash(second));
  }

  TEST_METHOD(EveryHashedFieldChangesIt)
  {
    Outpost::World world;
    const Outpost::EntityId subject = world.Create(At(10, 20), 30, Outpost::HullId::Station);
    const std::uint64_t baseline = Outpost::StateHash(world);

    world.Find(subject)->position.x = 11;
    const std::uint64_t movedX = Outpost::StateHash(world);
    Assert::AreNotEqual(baseline, movedX, L"position.x is not covered");

    world.Find(subject)->position.x = 10;
    world.Find(subject)->position.y = 21;
    Assert::AreNotEqual(baseline, Outpost::StateHash(world), L"position.y is not covered");

    world.Find(subject)->position.y = 20;
    world.Find(subject)->heading = 31;
    Assert::AreNotEqual(baseline, Outpost::StateHash(world), L"heading is not covered");

    world.Find(subject)->heading = 30;
    // A DIFFERENT identity, which is all this needs: the assertion is that the hash covers the
    // field at all, and M1.1 made the field an identity rather than a loose number.
    world.Find(subject)->hull = Outpost::HullId::ModuleFrame;
    Assert::AreNotEqual(baseline, Outpost::StateHash(world), L"hull is not covered");

    world.Find(subject)->hull = Outpost::HullId::Station;
    Assert::AreEqual(baseline, Outpost::StateHash(world), L"restoring every field should restore the hash");
  }

  TEST_METHOD(IdentityIsCoveredToo)
  {
    // Two worlds holding identical records at different indices are different states.
    Outpost::World left;
    static_cast<void>(left.Create(At(1, 2), 3, Outpost::HullId::Scout));

    Outpost::World right;
    const Outpost::EntityId filler = right.Create(At(1, 2), 3, Outpost::HullId::Scout);
    static_cast<void>(right.Create(At(1, 2), 3, Outpost::HullId::Scout));
    static_cast<void>(right.Destroy(filler));

    Assert::AreNotEqual(Outpost::StateHash(left), Outpost::StateHash(right));
  }

  TEST_METHOD(ADeadSlotIsNotHashed)
  {
    // A dead slot still holds its last occupant's record. Folding it in would report a divergence
    // between two hosts that merely reused slots in a different sequence.
    Outpost::World world;
    const Outpost::EntityId survivor = world.Create(At(7, 7), 7, Outpost::HullId::Scout);
    const std::uint64_t alone = Outpost::StateHash(world);

    const Outpost::EntityId doomed = world.Create(At(9, 9), 9, Outpost::HullId::Scout);
    Assert::AreNotEqual(alone, Outpost::StateHash(world));
    Assert::IsTrue(world.Destroy(doomed));
    Assert::AreEqual(alone, Outpost::StateHash(world), L"a destroyed entity is still reaching the hash");

    Assert::IsTrue(world.IsAlive(survivor));
  }

  TEST_METHOD(AnEmptyWorldIsStable)
  {
    Outpost::World first;
    Outpost::World second;
    Assert::AreEqual(Outpost::StateHash(first), Outpost::StateHash(second));
  }

  TEST_METHOD(AScriptedRunHashesToTheSameNumberOnEveryPlatform)
  {
    // THE CLAIM ADR-002 OWES, AT THE SCALE M0.8 CAN CARRY IT. Every other test in this class
    // compares a hash against another hash computed in the same process, which proves the hash is
    // a function of the state and proves nothing at all about x64 against ARM64. A LITERAL is what
    // makes this a cross-platform assertion: the same number has to come out of Debug and Release,
    // x64 and ARM64, or one of those four builds fails.
    //
    // It is not the determinism test ADR-002 is owed -- that one needs commands and a scripted
    // order list, and it arrives with M0.10 -- but it is the same property, and it is cheap.
    Outpost::World world;
    // THREE DIFFERENT HULLS, because the hash folds the field and a run where they were all equal
    // would not notice a hull that stopped being hashed. **These were 2, 9 and 1 until M1.1**, and
    // 9 was never a hull -- it was a loose byte, which is what `HullId` was before there was a
    // catalog to index into. Two of the three are unchanged; the 9 became `ModuleFrame`, which is
    // why the literal below moved and is the only reason it moved.
    const Outpost::EntityId first = world.Create(At(-1237, 400), 12345, Outpost::HullId::Cruiser);
    const Outpost::EntityId second = world.Create(At(900, -900), 54321, Outpost::HullId::ModuleFrame);
    const Outpost::EntityId doomed = world.Create(At(0, 0), 7, Outpost::HullId::Frigate);

    static_cast<void>(world.OrderMoveTo(first, At(1000, -250), 7));
    static_cast<void>(world.OrderMoveTo(second, At(-33, 33), 13));
    static_cast<void>(world.OrderMoveTo(doomed, At(500, 500), 3));

    for (int tick = 0; tick < 40; ++tick)
    {
      Outpost::Tick(world);
      if (tick == 17)
      {
        // A death mid-run, so the free list and the skipped slot are both inside what is hashed.
        static_cast<void>(world.Destroy(doomed));
      }
    }

    // **THE LITERAL MOVED AT M1.1 AND THAT IS THE ONLY TIME IT SHOULD.** It was
    // 0xd2a4d77a1900bf40 when one of the three hulls above was the number 9, which the catalog
    // makes unrepresentable -- there are five hulls and 9 was never one of them. The inputs
    // changed by exactly that much and the hash changed with them.
    //
    // A CHANGE HERE IS EITHER DELIBERATE OR IT IS A DESYNCHRONISATION. If this literal starts
    // disagreeing without anybody editing the run above it, the tick has stopped being
    // deterministic and that is what this test exists to say (R16, ADR-002).
    Assert::AreEqual(0xd2a4e47a1900d557ull, Outpost::StateHash(world));
  }
};

} // namespace GameLogicTests
