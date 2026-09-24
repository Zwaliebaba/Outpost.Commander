#include "pch.h"

#include <cstdint>
#include <limits>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
[[nodiscard]] Neuron::Vec2 At(Neuron::Fixed _x, Neuron::Fixed _y) noexcept
{
  return Neuron::Vec2{.x = _x, .y = _y};
}

/// A ship. **M1.17 MADE THE TICK READ IT**: a design with no drive is a structure the others route
/// around (Q60), so a mover built as a Station, as this was until then, would be in its own way.
inline constexpr Outpost::DesignId SOME_DESIGN = Outpost::DesignId::Miner;

/// Half a turn a tick reaches any bearing at once, which keeps the tests written before Q59 about
/// straight lines rather than about turning.
inline constexpr std::uint16_t INSTANT_TURN = 32768;
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
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(first.IsValid());
    Assert::AreEqual(std::uint16_t{0}, first.index);
    Assert::AreEqual(std::uint16_t{1}, first.generation);
  }

  TEST_METHOD(AFreedAndReusedIndexCarriesANewGeneration)
  {
    // M0.8's first exit criterion, and the reason an identity is two numbers rather than one.
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.Destroy(first));

    const Outpost::EntityId second = world.Create(At(0, 0), 0, SOME_DESIGN);
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
    const Outpost::EntityId doomed = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.Destroy(doomed));
    Assert::IsFalse(world.IsAlive(doomed));
    Assert::AreEqual(std::size_t{0}, world.AliveCount());
  }

  TEST_METHOD(DestroyingTwiceIsNotAnError)
  {
    Outpost::World world;
    const Outpost::EntityId victim = world.Create(At(0, 0), 0, SOME_DESIGN);
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
    const Outpost::EntityId zero = world.Create(At(0, 0), 0, SOME_DESIGN);
    const Outpost::EntityId one = world.Create(At(0, 0), 0, SOME_DESIGN);
    const Outpost::EntityId two = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::AreEqual(std::uint16_t{0}, zero.index);
    Assert::AreEqual(std::uint16_t{1}, one.index);
    Assert::AreEqual(std::uint16_t{2}, two.index);

    Assert::IsTrue(world.Destroy(zero));
    Assert::IsTrue(world.Destroy(one));

    // Pushed 0 then 1; taken from the back, so 1 comes first.
    Assert::AreEqual(std::uint16_t{1}, world.Create(At(0, 0), 0, SOME_DESIGN).index);
    Assert::AreEqual(std::uint16_t{0}, world.Create(At(0, 0), 0, SOME_DESIGN).index);

    // And once the free list is empty the store grows again rather than reusing a live slot.
    Assert::AreEqual(std::uint16_t{3}, world.Create(At(0, 0), 0, SOME_DESIGN).index);
  }

  TEST_METHOD(TheStoreNeverShrinks)
  {
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_DESIGN);
    static_cast<void>(world.Create(At(0, 0), 0, SOME_DESIGN));
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
    const Outpost::EntityId mover = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.OrderMoveTo(mover, At(1000, 0), 7, INSTANT_TURN));

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
    const Outpost::EntityId mover = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.OrderMoveTo(mover, At(1000, 0), 7, INSTANT_TURN));

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
    const Outpost::EntityId mover = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.OrderMoveTo(mover, At(3, 3), 1, INSTANT_TURN));

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
        const Outpost::EntityId mover = world.Create(At(x, y), 0, SOME_DESIGN);
        Assert::IsTrue(world.OrderMoveTo(mover, At(0, 0), 7, INSTANT_TURN));

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
    const Outpost::EntityId stuck = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.OrderMoveTo(stuck, At(1000, 0), 0, INSTANT_TURN));

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
    const Outpost::EntityId idle = world.Create(At(42, -42), 0, SOME_DESIGN);
    for (int tick = 0; tick < 20; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(idle)->position == At(42, -42));
  }

  TEST_METHOD(ADeadEntityIsNotTicked)
  {
    Outpost::World world;
    const Outpost::EntityId first = world.Create(At(0, 0), 0, SOME_DESIGN);
    const Outpost::EntityId second = world.Create(At(0, 0), 0, SOME_DESIGN);
    Assert::IsTrue(world.OrderMoveTo(first, At(1000, 0), 7, INSTANT_TURN));
    Assert::IsTrue(world.OrderMoveTo(second, At(0, 1000), 7, INSTANT_TURN));
    Assert::IsTrue(world.Destroy(first));

    for (int tick = 0; tick < 500; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(second)->position == At(0, 1000));
    Assert::AreEqual(std::size_t{1}, world.AliveCount());
  }
};

namespace
{
[[nodiscard]] Neuron::Vec2 InUnits(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(_x), .y = Neuron::FixedFromWholeUnits(_y)};
}

/// A Miner ordered somewhere at its own derived speed and turn rate, which is what the host does.
bool OrderAsMiner(Outpost::World& _world, Outpost::EntityId _id, const Neuron::Vec2& _destination)
{
  return _world.OrderMoveTo(_id, _destination, Outpost::SpeedPerTick(Outpost::DesignId::Miner),
                            Outpost::TurnAnglePerTick(Outpost::DesignId::Miner));
}

/// Q60's keep-out, from the catalog: half the station's size plus half the Miner's.
[[nodiscard]] std::int64_t KeepOutUnits() noexcept
{
  return (static_cast<std::int64_t>(Outpost::Hull(Outpost::Design(Outpost::DesignId::Station).hull).sizeUnits) / 2) +
         (static_cast<std::int64_t>(Outpost::Hull(Outpost::Design(Outpost::DesignId::Miner).hull).sizeUnits) / 2);
}

/// Q61's keep-out between two Miners: half one plus half the other.
[[nodiscard]] std::int64_t ShipKeepOutUnits() noexcept
{
  return static_cast<std::int64_t>(Outpost::Hull(Outpost::Design(Outpost::DesignId::Miner).hull).sizeUnits);
}

[[nodiscard]] std::int64_t DistanceSquaredUnits(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  const std::int64_t x = Neuron::FixedToWholeUnitsFloor(_a.x - _b.x);
  const std::int64_t y = Neuron::FixedToWholeUnitsFloor(_a.y - _b.y);
  return (x * x) + (y * y);
}
} // namespace

/// M1.17: `OpenQuestions.md` Q59, a ship turns while it flies; Q60, it routes around structures; and
/// Q61, it routes around other ships too.
TEST_CLASS(Steering)
{
public:
  TEST_METHOD(ATurnIsBoundedByTheRate)
  {
    // Ordered straight behind itself: the first tick swings the heading by exactly the rate and, a
    // quarter turn or more still off, does not move forward at all.
    Outpost::World world;
    const Outpost::EntityId ship = world.Create(InUnits(0, 0), 0, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, ship, InUnits(-1000, 0)));

    Outpost::Tick(world);

    const Outpost::Entity* entity = world.Find(ship);
    const std::int16_t swung = Neuron::AngleDifference(0, entity->heading);
    Assert::AreEqual(static_cast<std::int32_t>(Outpost::TurnAnglePerTick(Outpost::DesignId::Miner)),
                     static_cast<std::int32_t>((swung < 0) ? -swung : swung), L"the heading did not swing by exactly the turn rate");
    Assert::IsTrue(entity->position == InUnits(0, 0), L"a ship facing away from its target moved forward");
  }

  TEST_METHOD(ItFliesAlongItsHeadingOnceItHasTurned)
  {
    Outpost::World world;
    const Outpost::EntityId ship = world.Create(InUnits(0, 0), 0, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, ship, InUnits(0, 2000)));

    for (int tick = 0; tick < 60; ++tick)
    {
      Outpost::Tick(world);
    }

    // Three seconds in, the turn is done and it faces the point it is flying to. Not the y axis:
    // the arc has carried it about one turning radius to the side of the line it started on.
    const Outpost::Entity* entity = world.Find(ship);
    const Neuron::Vec2 toGo = InUnits(0, 2000) - entity->position;
    const std::int16_t error = Neuron::AngleDifference(entity->heading, Neuron::BearingOf(toGo.x, toGo.y));
    Assert::IsTrue((error >= -64) && (error <= 64), L"the heading is not the direction of travel");
    Assert::IsTrue(entity->position.y > Neuron::FixedFromWholeUnits(100));
  }

  TEST_METHOD(ItArrivesExactlyFromEveryDirectionAndHeading)
  {
    // The throttle is what makes this hold: a point inside the turning circle cannot be orbited,
    // because any forward step has a component toward it.
    const std::int32_t offsets[] = {-900, -40, -3, 3, 40, 900};
    const Neuron::Angle headings[] = {0, 16384, 32768, 49152, 12345};
    for (const std::int32_t x : offsets)
    {
      for (const std::int32_t y : offsets)
      {
        for (const Neuron::Angle heading : headings)
        {
          Outpost::World world;
          const Outpost::EntityId ship = world.Create(InUnits(x, y), heading, Outpost::DesignId::Miner);
          Assert::IsTrue(OrderAsMiner(world, ship, InUnits(0, 0)));

          for (int tick = 0; tick < 600; ++tick)
          {
            Outpost::Tick(world);
          }

          Assert::IsTrue(world.Find(ship)->position == InUnits(0, 0), L"a ship failed to arrive");
          Assert::IsFalse(world.FindOrder(ship)->active);
        }
      }
    }
  }

  TEST_METHOD(AShipRoutesAroundAStationItsLineCrosses)
  {
    // Straight through the middle, the case the M1.16 session saw. Every tick it stays outside the
    // keep-out, and it still lands exactly.
    for (const std::int32_t offset : {0, 60, -60, 150})
    {
      Outpost::World world;
      static_cast<void>(world.Create(InUnits(0, 0), 0, Outpost::DesignId::Station));
      const Outpost::EntityId ship = world.Create(InUnits(-1000, offset), 0, Outpost::DesignId::Miner);
      Assert::IsTrue(OrderAsMiner(world, ship, InUnits(1000, -offset)));

      const std::int64_t keepOut = KeepOutUnits();
      for (int tick = 0; tick < 1200; ++tick)
      {
        Outpost::Tick(world);
        Assert::IsTrue(DistanceSquaredUnits(world.Find(ship)->position, InUnits(0, 0)) >= (keepOut * keepOut),
                       L"the ship entered the station's keep-out circle");
      }

      Assert::IsTrue(world.Find(ship)->position == InUnits(1000, -offset), L"a routed ship did not arrive");
    }
  }

  TEST_METHOD(AShipAlreadyInsideTheKeepOutLeavesIt)
  {
    // Where a new ship appears, in front of its station: the station is not in its way.
    Outpost::World world;
    static_cast<void>(world.Create(InUnits(0, 0), 0, Outpost::DesignId::Station));
    const Outpost::EntityId ship = world.Create(InUnits(60, 0), 0, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, ship, InUnits(-1000, 0)));

    for (int tick = 0; tick < 1200; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(ship)->position == InUnits(-1000, 0));
  }

  TEST_METHOD(AnOrderOntoAStationArrives)
  {
    Outpost::World world;
    static_cast<void>(world.Create(InUnits(0, 0), 0, Outpost::DesignId::Station));
    const Outpost::EntityId ship = world.Create(InUnits(-1000, 0), 0, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, ship, InUnits(40, 10)));

    for (int tick = 0; tick < 1200; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(ship)->position == InUnits(40, 10));
  }

  TEST_METHOD(TwoShipsMeetingHeadOnPassEachOther)
  {
    // Q61: what the M1.16 device check saw, two ships flying through each other. Both keep right, and
    // neither enters the other's keep-out on the way past.
    Outpost::World world;
    const Outpost::EntityId east = world.Create(InUnits(-1000, 0), 0, Outpost::DesignId::Miner);
    const Outpost::EntityId west = world.Create(InUnits(1000, 0), 32768, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, east, InUnits(1000, 0)));
    Assert::IsTrue(OrderAsMiner(world, west, InUnits(-1000, 0)));

    const std::int64_t keepOut = ShipKeepOutUnits();
    for (int tick = 0; tick < 800; ++tick)
    {
      Outpost::Tick(world);
      Assert::IsTrue(DistanceSquaredUnits(world.Find(east)->position, world.Find(west)->position) >= (keepOut * keepOut),
                     L"two ships met head on and flew through each other");
    }
    Assert::IsTrue(world.Find(east)->position == InUnits(1000, 0));
    Assert::IsTrue(world.Find(west)->position == InUnits(-1000, 0));
  }

  TEST_METHOD(AShipRoutesAroundAParkedShip)
  {
    Outpost::World world;
    const Outpost::EntityId parked = world.Create(InUnits(0, 0), 0, Outpost::DesignId::Miner);
    const Outpost::EntityId ship = world.Create(InUnits(-1000, 0), 0, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, ship, InUnits(1000, 0)));

    const std::int64_t keepOut = ShipKeepOutUnits();
    for (int tick = 0; tick < 800; ++tick)
    {
      Outpost::Tick(world);
      Assert::IsTrue(DistanceSquaredUnits(world.Find(ship)->position, world.Find(parked)->position) >= (keepOut * keepOut),
                     L"a ship flew through a parked one");
    }
    Assert::IsTrue(world.Find(ship)->position == InUnits(1000, 0));
    Assert::IsTrue(world.Find(parked)->position == InUnits(0, 0), L"the parked ship was moved");
  }

  TEST_METHOD(AShipGoesAroundAShipParkedBesideItsDestination)
  {
    // THE CASE THE DEVICE FOUND, from the log of 2026-09-23 and relative to the parked Miner: a Fighter
    // sent from in front of its station to a point 98 units past the Miner, on a line straight through
    // it. A "final approach" exception let it fly through, and Q61's order groups replaced it.
    Outpost::World world;
    const Outpost::EntityId parked = world.Create(InUnits(0, 0), 0, Outpost::DesignId::Miner);
    const Outpost::EntityId fighter = world.Create(InUnits(716, 175), 32768, Outpost::DesignId::Fighter);
    Assert::IsTrue(world.OrderMoveTo(fighter, InUnits(-96, -22), Outpost::SpeedPerTick(Outpost::DesignId::Fighter),
                                     Outpost::TurnAnglePerTick(Outpost::DesignId::Fighter)));

    const std::int64_t keepOut =
      (static_cast<std::int64_t>(Outpost::Hull(Outpost::Design(Outpost::DesignId::Fighter).hull).sizeUnits) / 2) + (ShipKeepOutUnits() / 2);
    for (int tick = 0; tick < 800; ++tick)
    {
      Outpost::Tick(world);
      Assert::IsTrue(DistanceSquaredUnits(world.Find(fighter)->position, world.Find(parked)->position) >= (keepOut * keepOut),
                     L"the Fighter flew through the parked Miner");
    }
    Assert::IsTrue(world.Find(fighter)->position == InUnits(-96, -22), L"the Fighter did not arrive");
  }

  TEST_METHOD(ShipsFlyingTheSameWayDoNotSwerve)
  {
    // Q61's first exception: two abreast, one hull apart as the ring places them, flying the same way.
    // Without it each would swerve around the other all the way there.
    Outpost::World world;
    const Outpost::EntityId left = world.Create(InUnits(0, 0), 0, Outpost::DesignId::Miner);
    const Outpost::EntityId right = world.Create(InUnits(10, 60), 0, Outpost::DesignId::Miner);
    Assert::IsTrue(OrderAsMiner(world, left, InUnits(1500, 0)));
    Assert::IsTrue(OrderAsMiner(world, right, InUnits(1510, 60)));

    for (int tick = 0; tick < 400; ++tick)
    {
      Outpost::Tick(world);
      Assert::AreEqual(Neuron::Fixed{0}, world.Find(left)->position.y, L"a ship swerved around its own stream");
    }
    Assert::IsTrue(world.Find(left)->position == InUnits(1500, 0));
    Assert::IsTrue(world.Find(right)->position == InUnits(1510, 60));
  }

  TEST_METHOD(AFleetOrderedToOnePointFillsItsRingWithoutJamming)
  {
    // Q61's order group is what lets this hold: the ships of one order do not avoid each other, so the
    // inner slots are reached through the ones already parked around them. Twelve Miners, the first
    // two rings and five of the third.
    Outpost::World world;
    std::vector<Outpost::EntityId> fleet;
    for (std::int32_t index = 0; index < 12; ++index)
    {
      fleet.push_back(world.Create(InUnits(-1500 + ((index % 4) * 70), (index / 4) * 70), 0, Outpost::DesignId::Miner));
    }
    Assert::AreEqual(fleet.size(), Outpost::OrderFleetTo(world, fleet, InUnits(1500, 0)));

    std::vector<Neuron::Vec2> slots;
    for (const Outpost::EntityId id : fleet)
    {
      slots.push_back(world.FindOrder(id)->destination);
    }

    for (int tick = 0; tick < 1600; ++tick)
    {
      Outpost::Tick(world);
    }

    for (std::size_t index = 0; index < fleet.size(); ++index)
    {
      Assert::IsFalse(world.FindOrder(fleet[index])->active, L"a ship never reached its ring slot");
      Assert::IsTrue(world.Find(fleet[index])->position == slots[index]);
    }
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
      const Outpost::EntityId a = _world.Create(At(100, 200), 1000, Outpost::DesignId::Fighter);
      const Outpost::EntityId b = _world.Create(At(-50, 75), 40000, Outpost::DesignId::Miner);
      static_cast<void>(_world.OrderMoveTo(a, At(900, 200), 7, INSTANT_TURN));
      static_cast<void>(_world.OrderMoveTo(b, At(-50, -900), 13, INSTANT_TURN));
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
    const Outpost::EntityId subject = world.Create(At(10, 20), 30, Outpost::DesignId::Station);
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
    static_cast<void>(left.Create(At(1, 2), 3, Outpost::DesignId::Miner));

    Outpost::World right;
    const Outpost::EntityId filler = right.Create(At(1, 2), 3, Outpost::DesignId::Miner);
    static_cast<void>(right.Create(At(1, 2), 3, Outpost::DesignId::Miner));
    static_cast<void>(right.Destroy(filler));

    Assert::AreNotEqual(Outpost::StateHash(left), Outpost::StateHash(right));
  }

  TEST_METHOD(ADeadSlotIsNotHashed)
  {
    // A dead slot still holds its last occupant's record. Folding it in would report a divergence
    // between two hosts that merely reused slots in a different sequence.
    Outpost::World world;
    const Outpost::EntityId survivor = world.Create(At(7, 7), 7, Outpost::DesignId::Miner);
    const std::uint64_t alone = Outpost::StateHash(world);

    const Outpost::EntityId doomed = world.Create(At(9, 9), 9, Outpost::DesignId::Miner);
    Assert::AreNotEqual(alone, Outpost::StateHash(world));
    Assert::IsTrue(world.Destroy(doomed));
    Assert::AreEqual(alone, Outpost::StateHash(world), L"a destroyed entity is still reaching the hash");

    Assert::IsTrue(world.IsAlive(survivor));
  }

  /// **THE 2026-09-23 REVIEW'S M6**: each of these left the hash unchanged before it widened -- hull 450 to 1,
  /// cargo 0 to 99,000, a mine phase and an owner -- so two builds that disagreed about damage or cargo agreed
  /// here until a death or a delivery happened to land a tick apart.
  TEST_METHOD(HullPointsOwnerAndTheMineOrderReachTheHash)
  {
    Outpost::World world;
    const Outpost::EntityId miner = world.Create(At(7, 7), 7, Outpost::DesignId::Miner, 1);
    const std::uint64_t before = Outpost::StateHash(world);

    world.Find(miner)->hullRemaining = 1;
    const std::uint64_t damaged = Outpost::StateHash(world);
    Assert::AreNotEqual(before, damaged, L"hull points do not reach the hash");

    world.Find(miner)->owner = 2;
    const std::uint64_t reowned = Outpost::StateHash(world);
    Assert::AreNotEqual(damaged, reowned, L"the owner does not reach the hash");

    world.MineInSlot(miner.index).cargoMilliOre = 99000;
    const std::uint64_t laden = Outpost::StateHash(world);
    Assert::AreNotEqual(reowned, laden, L"cargo does not reach the hash");

    world.MineInSlot(miner.index).phase = Outpost::MiningPhase::Unloading;
    Assert::AreNotEqual(laden, Outpost::StateHash(world), L"the mine phase does not reach the hash");
  }

  /// And the match hash sees what a player can spend, which the world does not hold.
  TEST_METHOD(TheMatchHashSeesCreditsAndTheBuildItem)
  {
    Outpost::World world;
    Outpost::BuildSystem build;
    Outpost::Economy economy;
    build.Begin(2);
    const std::uint64_t before = Outpost::MatchHash(world, build, economy);
    Assert::AreNotEqual(Outpost::StateHash(world), before, L"the match hash is the world hash alone");

    build.Grant(2, 1);
    Assert::AreNotEqual(before, Outpost::MatchHash(world, build, economy), L"credits do not reach the match hash");
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
    // THREE DIFFERENT DESIGNS, because the hash folds the hull and a run where they were all equal
    // would not notice a hull that stopped being hashed.
    //
    // **THE LITERAL HAS MOVED TWICE IN ONE DAY AND BOTH TIMES WERE FORCED.** At M1.1 one of these
    // was the number 9, which was never a hull -- it was a loose byte, which is what `HullId` was
    // before there was a catalog to index into. At M1.3 `World::Create` stopped taking a hull at
    // all: an entity is built from a DESIGN and the hull is derived, so the three hulls this run
    // produces are now whatever the Miner, the Fighter and the Station are built on. Neither change
    // was a choice about the hash.
    const Outpost::EntityId first = world.Create(At(-1237, 400), 12345, Outpost::DesignId::Miner);
    const Outpost::EntityId second = world.Create(At(900, -900), 54321, Outpost::DesignId::Fighter);
    const Outpost::EntityId doomed = world.Create(At(0, 0), 7, Outpost::DesignId::Station);

    static_cast<void>(world.OrderMoveTo(first, At(1000, -250), 7, INSTANT_TURN));
    static_cast<void>(world.OrderMoveTo(second, At(-33, 33), 13, INSTANT_TURN));
    static_cast<void>(world.OrderMoveTo(doomed, At(500, 500), 3, INSTANT_TURN));

    for (int tick = 0; tick < 40; ++tick)
    {
      Outpost::Tick(world);
      if (tick == 17)
      {
        // A death mid-run, so the free list and the skipped slot are both inside what is hashed.
        static_cast<void>(world.Destroy(doomed));
      }
    }

    // A CHANGE HERE IS EITHER DELIBERATE OR IT IS A DESYNCHRONISATION. If this literal starts
    // disagreeing without anybody editing the run above it, the tick has stopped being
    // deterministic and that is what this test exists to say (R16, ADR-002).
    //
    // **IT MOVED A THIRD TIME AT M1.17, AND THAT WAS FORCED TOO**: the tick now steers a heading and
    // routes around the Station in the run, and the value was the same on all four pairs before it
    // was pinned.
    //
    // **A FOURTH TIME AFTER THE 2026-09-23 REVIEW (M6)**, because the hash itself widened to hull points,
    // owner and the mine order; the run is unchanged and was `0xa0141c81045fc0bc` under the old fields.
    // Computed under g++ and clang only; the four MSVC pairs are owed with the scripted match's (ADR-002).
    // They passed at M3.1.
    //
    // **A FIFTH TIME AT M3.2, BECAUSE THE HASH WIDENED AGAIN** to each ship's weapon remainders and attack
    // order. Nothing in this run is armed against anything, so the run is unchanged; the same on all four MSVC
    // pairs before it was pinned.
    //
    // **A SIXTH TIME AT M3.6**: the hash folds a mine order's calm count (Q64). The run is unchanged; the same on
    // all four MSVC pairs before it was pinned.
    Assert::AreEqual(0xfdbb9e54bd86ec0dull, Outpost::StateHash(world));
  }
};

/// M1.3: what `World::Create` derives, and the one piece of redundant state on `Entity`.
TEST_CLASS(CreationFromADesign)
{
public:
  /// **THE HULL AND THE DESIGN CANNOT DISAGREE**, because `Create` is the only writer of either
  /// and it derives the first from the second (ADR-006, R24). `Entity` carries both on purpose --
  /// ADR-002's hash covers the hull and the wire carries the design, so both are read on a hot path
  /// -- and this is what pays for that redundancy.
  TEST_METHOD(TheHullIsDerivedFromTheDesign)
  {
    Outpost::World world;
    for (const Outpost::DesignEntry& design : Outpost::Designs())
    {
      const Outpost::EntityId id = world.Create(Neuron::Vec2{}, 0, design.id, 1);
      const Outpost::Entity* entity = world.Find(id);
      Assert::IsNotNull(entity);
      Assert::AreEqual(static_cast<int>(design.id), static_cast<int>(entity->design));
      Assert::AreEqual(static_cast<int>(design.hull), static_cast<int>(entity->hull));
      Assert::AreEqual(static_cast<int>(Outpost::Design(design.id).hull), static_cast<int>(entity->hull));
    }
  }

  /// A new entity starts undamaged, and **the figure is the derivation's rather than a literal**.
  /// A `Station` is 8,000 points because `GameDesign.md` section 6 says so and `Derive` sums it,
  /// not because anything here typed it.
  TEST_METHOD(ANewEntityStartsAtFullHull)
  {
    Outpost::World world;
    for (const Outpost::DesignEntry& design : Outpost::Designs())
    {
      const Outpost::EntityId id = world.Create(Neuron::Vec2{}, 0, design.id, 1);
      const Outpost::Entity* entity = world.Find(id);
      Assert::IsNotNull(entity);

      const Outpost::DerivedStats stats = Outpost::Derive(design.id);
      Assert::IsTrue(stats.hullPoints > 0, L"a design with no hull points would quantize as undamaged forever");
      Assert::AreEqual(static_cast<std::uint32_t>(entity->hullRemaining), stats.hullPoints);
      Assert::AreEqual(std::uint8_t{100}, Outpost::QuantizeHullPercent(entity->hullRemaining, stats.hullPoints));
    }
  }

  /// **THE STORE HOLDS HULL POINTS AND THE WIRE HOLDS A PERCENTAGE**, and the widths have to be
  /// able to carry the catalog's largest hull. A `Cruiser`'s 3,000 and a `Station`'s 8,000 both fit
  /// sixteen bits with room; if a hull ever passes 65,535 this is the assertion that says so.
  TEST_METHOD(EveryHullFitsTheRemainingField)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      Assert::IsTrue(hull.hullPoints <= std::numeric_limits<std::uint16_t>::max());
    }
  }
};
} // namespace GameLogicTests
