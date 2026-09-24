#include "pch.h"

#include <algorithm>
#include <set>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;

[[nodiscard]] Neuron::Vec2 At(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE};
}

/// A fleet of one design, spread along a line so no two share a distance to the origin unless a test
/// asks for it.
[[nodiscard]] std::vector<Outpost::EntityId> Fleet(Outpost::World& _world, std::size_t _count, Outpost::DesignId _design)
{
  std::vector<Outpost::EntityId> ids;
  ids.reserve(_count);
  for (std::size_t index = 0; index < _count; ++index)
  {
    ids.push_back(_world.Create(At(1000 + static_cast<std::int32_t>(index) * 37, 2000), 0, _design, MINE));
  }
  return ids;
}

[[nodiscard]] std::vector<Neuron::Vec2> DestinationsOf(const Outpost::World& _world, const std::vector<Outpost::EntityId>& _ids)
{
  std::vector<Neuron::Vec2> out;
  out.reserve(_ids.size());
  for (const Outpost::EntityId id : _ids)
  {
    for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
    {
      if (_world.IsSlotAlive(slot) && (_world.EntityInSlot(slot).id == id))
      {
        out.push_back(_world.OrderInSlot(slot).destination);
        break;
      }
    }
  }
  return out;
}
} // namespace

/// Q19's packing. **The layout is a function of an index and nothing else**, which is what makes a
/// formation system later the same assignment with a different slot layout.
TEST_CLASS(TheRingLayout)
{
public:
  /// Hexagonal: slot 0 is the point, ring 1 holds six, ring 2 holds twelve, ring `k` holds `6k`.
  TEST_METHOD(RingKHoldsSixKSlots)
  {
    Assert::AreEqual(static_cast<std::size_t>(0), Outpost::RingOfSlot(0));

    std::size_t index = 1;
    for (std::size_t ring = 1; ring <= 6; ++ring)
    {
      for (std::size_t position = 0; position < (6 * ring); ++position)
      {
        Assert::AreEqual(ring, Outpost::RingOfSlot(index), L"a slot landed in the wrong ring");
        ++index;
      }
    }
  }

  /// **FIFTY SHIPS TAKE FOUR RINGS**, which is the figure the design's fleet scale makes matter: at one
  /// spacing a ring, a fifty-ship order is four spacings of radius rather than a sprawl.
  TEST_METHOD(FiftyShipsFitInFourRings)
  {
    Assert::AreEqual(static_cast<std::size_t>(4), Outpost::RingOfSlot(49));
    Assert::AreEqual(static_cast<std::size_t>(3), Outpost::RingOfSlot(36));
    Assert::AreEqual(static_cast<std::size_t>(4), Outpost::RingOfSlot(37));
  }

  /// Slot zero is the point itself, so a single ship ordered somewhere goes exactly there.
  TEST_METHOD(SlotZeroIsThePoint)
  {
    Assert::IsTrue(Outpost::RingSlotOffset(0, 100 * Neuron::FIXED_ONE) == Neuron::Vec2{});
  }

  /// Every slot in a ring sits at that ring's radius, within the rounding the sine table carries.
  TEST_METHOD(EverySlotInARingIsAtItsRadius)
  {
    const Neuron::Fixed spacing = 90 * Neuron::FIXED_ONE;
    for (std::size_t index = 1; index < 91; ++index)
    {
      const std::size_t ring = Outpost::RingOfSlot(index);
      const std::int64_t expected = static_cast<std::int64_t>(ring) * spacing;
      const std::int64_t actual = Neuron::Sqrt(Neuron::LengthSquared(Outpost::RingSlotOffset(index, spacing)));

      // One `Fixed` step of slack: the table is Q1.15 and the radius is a product of it.
      const std::int64_t error = (actual > expected) ? (actual - expected) : (expected - actual);
      Assert::IsTrue(error <= Neuron::FIXED_ONE, L"a slot left its ring");
    }
  }

  /// **NO TWO SLOTS COINCIDE**, which is the entire point of the exercise: on a plane, ships ordered to
  /// one point stack (ADR-001).
  TEST_METHOD(NoTwoSlotsAreTheSamePlace)
  {
    const Neuron::Fixed spacing = 90 * Neuron::FIXED_ONE;
    std::set<std::pair<Neuron::Fixed, Neuron::Fixed>> seen;
    for (std::size_t index = 0; index < 60; ++index)
    {
      const Neuron::Vec2 offset = Outpost::RingSlotOffset(index, spacing);
      Assert::IsTrue(seen.insert({offset.x, offset.y}).second, L"two ships were sent to one place");
    }
  }

  /// Q37's answer, used: a fleet spaces to its widest member, so nothing overlaps.
  TEST_METHOD(SpacingIsTheWidestHullInTheSelection)
  {
    Outpost::World world;
    const std::vector<Outpost::EntityId> miners = Fleet(world, 3, Outpost::DesignId::Miner);
    Assert::AreEqual(60 * Neuron::FIXED_ONE, Outpost::RingSpacingFor(world, miners));

    std::vector<Outpost::EntityId> mixed = miners;
    mixed.push_back(world.Create(At(0, 0), 0, Outpost::DesignId::Fighter, MINE));
    Assert::AreEqual(90 * Neuron::FIXED_ONE, Outpost::RingSpacingFor(world, mixed), L"a mixed fleet spaces to its largest");

    Assert::AreEqual(Neuron::Fixed{0}, Outpost::RingSpacingFor(world, {}));
  }
};

/// M1.7's exit criteria, which are all about reproducibility.
TEST_CLASS(OrderingAFleet)
{
public:
  /// **THE SAME SELECTION TO THE SAME POINT YIELDS THE SAME SLOTS IN THE SAME ORDER.**
  TEST_METHOD(TheSameOrderTwiceIsTheSameAssignment)
  {
    Outpost::World first;
    Outpost::World second;
    const std::vector<Outpost::EntityId> a = Fleet(first, 12, Outpost::DesignId::Fighter);
    const std::vector<Outpost::EntityId> b = Fleet(second, 12, Outpost::DesignId::Fighter);

    Assert::AreEqual(static_cast<std::size_t>(12), Outpost::OrderFleetTo(first, a, At(0, 0)));
    Assert::AreEqual(static_cast<std::size_t>(12), Outpost::OrderFleetTo(second, b, At(0, 0)));

    Assert::IsTrue(DestinationsOf(first, a) == DestinationsOf(second, b));
  }

  /// **AND IT IS INDEPENDENT OF THE ORDER THE SELECTION ARRIVED IN**, which a client has no reason to
  /// keep stable -- a hit test walks the replica store, and that order is the store's rather than the
  /// player's.
  TEST_METHOD(TheArrivalOrderOfTheSelectionDoesNotMatter)
  {
    Outpost::World world;
    const std::vector<Outpost::EntityId> ids = Fleet(world, 9, Outpost::DesignId::Fighter);

    Assert::AreEqual(static_cast<std::size_t>(9), Outpost::OrderFleetTo(world, ids, At(-500, 700)));
    const std::vector<Neuron::Vec2> forwards = DestinationsOf(world, ids);

    std::vector<Outpost::EntityId> shuffled = ids;
    std::reverse(shuffled.begin(), shuffled.end());
    Assert::AreEqual(static_cast<std::size_t>(9), Outpost::OrderFleetTo(world, shuffled, At(-500, 700)));

    Assert::IsTrue(forwards == DestinationsOf(world, ids), L"reversing the selection moved the slots");
  }

  /// **A TIE BREAKS ON IDENTITY**, and this builds the tie deliberately: four ships at the four corners
  /// of a square around the target are at exactly equal distances, which on integer positions is common
  /// rather than exotic. Distance alone is not a total order and `std::sort` may return either
  /// arrangement for one that is not.
  TEST_METHOD(ATieAtEqualDistanceBreaksOnIdentity)
  {
    Outpost::World world;
    std::vector<Outpost::EntityId> ring;
    ring.push_back(world.Create(At(1000, 0), 0, Outpost::DesignId::Fighter, MINE));
    ring.push_back(world.Create(At(0, 1000), 0, Outpost::DesignId::Fighter, MINE));
    ring.push_back(world.Create(At(-1000, 0), 0, Outpost::DesignId::Fighter, MINE));
    ring.push_back(world.Create(At(0, -1000), 0, Outpost::DesignId::Fighter, MINE));

    Assert::AreEqual(static_cast<std::size_t>(4), Outpost::OrderFleetTo(world, ring, At(0, 0)));

    // The lowest index took slot zero, which is the target itself.
    Assert::IsTrue(DestinationsOf(world, ring)[0] == At(0, 0), L"the tie did not break on identity");

    // And it does so again from a reversed selection.
    std::vector<Outpost::EntityId> reversed = ring;
    std::reverse(reversed.begin(), reversed.end());
    Assert::AreEqual(static_cast<std::size_t>(4), Outpost::OrderFleetTo(world, reversed, At(0, 0)));
    Assert::IsTrue(DestinationsOf(world, ring)[0] == At(0, 0));
  }

  /// **THE NEAREST SHIP TAKES THE CENTER SLOT**, so a fleet does not cross itself to reach a formation.
  TEST_METHOD(TheNearestShipTakesTheCenter)
  {
    Outpost::World world;
    // **NOT `near` AND `far`.** `<windows.h>` defines both as empty macros -- the same trap
    // `TechnicalDesign.md` section 1 names for `small`, and it reaches a test file the moment one
    // includes the SDK through a precompiled header.
    const Outpost::EntityId furthest = world.Create(At(5000, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId nearest = world.Create(At(100, 0), 0, Outpost::DesignId::Fighter, MINE);

    // Presented furthest-first, so an implementation that used arrival order would fail this.
    const std::vector<Outpost::EntityId> selection{furthest, nearest};
    Assert::AreEqual(static_cast<std::size_t>(2), Outpost::OrderFleetTo(world, selection, At(0, 0)));

    const std::vector<Neuron::Vec2> destinations = DestinationsOf(world, selection);
    Assert::IsTrue(destinations[1] == At(0, 0), L"the near ship did not take the center");
    Assert::IsFalse(destinations[0] == At(0, 0));
  }

  /// Fifty ships, none of them in the same place.
  TEST_METHOD(FiftyShipsGetFiftyDistinctDestinations)
  {
    Outpost::World world;
    const std::vector<Outpost::EntityId> fleet = Fleet(world, 50, Outpost::DesignId::Fighter);
    Assert::AreEqual(static_cast<std::size_t>(50), Outpost::OrderFleetTo(world, fleet, At(0, 0)));

    std::set<std::pair<Neuron::Fixed, Neuron::Fixed>> seen;
    for (const Neuron::Vec2& destination : DestinationsOf(world, fleet))
    {
      Assert::IsTrue(seen.insert({destination.x, destination.y}).second, L"two of fifty ships were sent to one point");
    }
  }

  /// A destination outside the play area is clamped, the way a target already is.
  TEST_METHOD(ARingAtTheCornerStaysInsideThePlayArea)
  {
    Outpost::World world;
    const std::vector<Outpost::EntityId> fleet = Fleet(world, 20, Outpost::DesignId::Fighter);
    static_cast<void>(Outpost::OrderFleetTo(world, fleet, At(8192, 8192)));

    for (const Neuron::Vec2& destination : DestinationsOf(world, fleet))
    {
      Assert::IsTrue(Outpost::ClampToPlayArea(destination) == destination);
    }
  }

  /// **AND IT STAYS A RING** (the 2026-09-23 review, m4): fifty fighters ordered into a corner or against an
  /// edge get fifty distinct points, where clamping each slot on its own folded them onto the edge line.
  TEST_METHOD(ARingAgainstTheWallKeepsEverySlotDistinct)
  {
    const Neuron::Fixed edge = Outpost::PLAY_AREA_HALF_EXTENT;
    for (const Neuron::Vec2 target :
         {Neuron::Vec2{.x = edge, .y = edge}, Neuron::Vec2{.x = edge, .y = 0}, Neuron::Vec2{.x = -edge, .y = -edge}})
    {
      Outpost::World world;
      const std::vector<Outpost::EntityId> fleet = Fleet(world, 50, Outpost::DesignId::Fighter);
      static_cast<void>(Outpost::OrderFleetTo(world, fleet, target));

      std::set<std::pair<Neuron::Fixed, Neuron::Fixed>> seen;
      for (const Neuron::Vec2& destination : DestinationsOf(world, fleet))
      {
        Assert::IsTrue(Outpost::ClampToPlayArea(destination) == destination);
        Assert::IsTrue(seen.insert({destination.x, destination.y}).second, L"the wall stacked two ships on one point");
      }
    }
  }

  /// An identity that does not resolve is skipped rather than refusing the order -- a ship that died
  /// between validation and here is ordinary.
  TEST_METHOD(ADeadIdentityIsSkipped)
  {
    Outpost::World world;
    std::vector<Outpost::EntityId> fleet = Fleet(world, 3, Outpost::DesignId::Fighter);
    Assert::IsTrue(world.Destroy(fleet[1]));

    Assert::AreEqual(static_cast<std::size_t>(2), Outpost::OrderFleetTo(world, fleet, At(0, 0)));
  }
};

/// R24's speed, which M0 could not derive because an entity had no design.
TEST_CLASS(DerivedMovementSpeed)
{
public:
  /// A Miner moves 5 units a tick and a Fighter 7 -- 100 and 140 units a second over twenty ticks, and
  /// both divide exactly.
  TEST_METHOD(EachDesignMovesAtItsOwnSpeed)
  {
    Assert::AreEqual(5 * Neuron::FIXED_ONE, Outpost::SpeedPerTick(Outpost::DesignId::Miner));
    Assert::AreEqual(7 * Neuron::FIXED_ONE, Outpost::SpeedPerTick(Outpost::DesignId::Fighter));
  }

  /// **A STATION TOLD TO MOVE STAYS WHERE IT IS** rather than being a special case somewhere. It has no
  /// drive, so its derived speed is zero, and `World::OrderMoveTo` reads that as an order that moves
  /// nothing.
  TEST_METHOD(AStationDerivesNoSpeed)
  {
    Assert::AreEqual(Neuron::Fixed{0}, Outpost::SpeedPerTick(Outpost::DesignId::Station));

    Outpost::World world;
    const Outpost::EntityId station = world.Create(At(0, 0), 0, Outpost::DesignId::Station, MINE);
    static_cast<void>(Outpost::OrderFleetTo(world, std::vector<Outpost::EntityId>{station}, At(4000, 4000)));

    for (int tick = 0; tick < 50; ++tick)
    {
      Outpost::Tick(world);
    }
    Assert::IsTrue(world.Find(station)->position == At(0, 0), L"a station moved");
  }

  /// **THE MINER WAS MOVING AT THE FIGHTER'S SPEED UNTIL NOW**, which is why nothing looked wrong: the
  /// constant it replaced was `7 * 256`, exactly the Fighter's, so only one of the two designs was
  /// right. Asserted as a relation rather than as two numbers.
  TEST_METHOD(TheFighterIsFasterThanTheMiner)
  {
    Assert::IsTrue(Outpost::SpeedPerTick(Outpost::DesignId::Fighter) > Outpost::SpeedPerTick(Outpost::DesignId::Miner));
  }
};

} // namespace GameLogicTests
