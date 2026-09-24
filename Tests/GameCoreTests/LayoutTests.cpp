#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

/// M1.5. **The one property that matters is that both sides get the same answer** (R23), and it is
/// asserted by running the function twice rather than by trusting that it is pure.
TEST_CLASS(TheStartingLayout)
{
public:
  /// **RUN TWICE, COMPARED WHOLE.** The host runs this to populate the match and the client runs it to
  /// draw the same field; a generator that disagreed with itself would put a station somewhere the
  /// player could not shoot at.
  TEST_METHOD(TheSameSeedProducesTheSameLayout)
  {
    for (const std::size_t players : {std::size_t{1}, std::size_t{2}, std::size_t{4}})
    {
      for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{20260922}, ~std::uint64_t{0}})
      {
        const std::vector<Outpost::Placement> first = Outpost::GenerateLayout(seed, players);
        const std::vector<Outpost::Placement> second = Outpost::GenerateLayout(seed, players);
        Assert::IsTrue(first == second, L"the generator disagreed with itself");
      }
    }
  }

  /// **THE SEED IS IGNORED AT M0 AND M1 AND THAT IS THE DESIGN** (`GameDesign.md` section 3), not an
  /// oversight. Pinned so that the day M2 makes it matter, this test is the one that fails and says so.
  TEST_METHOD(TheSeedChangesNothingYet)
  {
    Assert::IsTrue(Outpost::GenerateLayout(1, 2) == Outpost::GenerateLayout(999999, 2));
  }

  TEST_METHOD(EveryPlayerGetsOneStation)
  {
    const std::vector<Outpost::Placement> layout = Outpost::GenerateLayout(0, 2);
    Assert::AreEqual(static_cast<std::size_t>(2), layout.size());
    for (const Outpost::Placement& placed : layout)
    {
      Assert::IsTrue(placed.design == Outpost::DesignId::Station);
      Assert::IsTrue(placed.owner != Outpost::NO_PLAYER);
    }
    Assert::AreEqual(1, static_cast<int>(layout[0].owner));
    Assert::AreEqual(2, static_cast<int>(layout[1].owner));
  }

  /// **AT TWO PLAYERS THE COPY IS A NEGATION AND IS THEREFORE EXACT** (`TechnicalDesign.md` section 3).
  /// This is the fairness guarantee the whole generator rests on: every player's start is the same
  /// start, so no balance analysis is needed and no seed can be unlucky.
  TEST_METHOD(TwoPlayersAreExactlyOpposed)
  {
    const std::vector<Outpost::Placement> layout = Outpost::GenerateLayout(0, 2);
    Assert::AreEqual(layout[0].position.x, -layout[1].position.x);
    Assert::AreEqual(layout[0].position.y, -layout[1].position.y);
  }

  /// Q26's answer, and the arithmetic behind it. The radius is exact on both axes because the anchors
  /// sit on them.
  TEST_METHOD(TheAnchorRadiusIsSixThousandUnits)
  {
    Assert::AreEqual(6000, Outpost::ANCHOR_RADIUS_UNITS);

    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::size_t index = 1; index <= players; ++index)
      {
        const Neuron::Vec2 anchor = Outpost::StartAnchor(players, static_cast<Outpost::PlayerId>(index));
        Assert::AreEqual(static_cast<std::int64_t>(Outpost::ANCHOR_RADIUS) * Outpost::ANCHOR_RADIUS, Neuron::LengthSquared(anchor),
                         L"an anchor left the circle");
      }
    }
  }

  /// **TWELVE THOUSAND UNITS APART, WHICH IS 85.7 SECONDS AT THE FIGHTER'S SPEED** -- `GameDesign.md`
  /// section 7 puts a crossing at 80 to 100 seconds and that is the arithmetic the raid balance rests
  /// on, so the WINDOW is what is asserted and the figure is pinned in tenths beside it. Q26 rounds it
  /// to 86 in prose; rounding is not the claim.
  ///
  /// A radius that moved without the crossing time being looked at again is what this catches.
  TEST_METHOD(TwoOpposedStationsAreOneCrossingApart)
  {
    const std::vector<Outpost::Placement> layout = Outpost::GenerateLayout(0, 2);
    const std::int64_t separation = Neuron::Sqrt(Neuron::LengthSquared(layout[1].position - layout[0].position));
    const std::int64_t units = separation / Neuron::FIXED_ONE;
    Assert::AreEqual(static_cast<std::int64_t>(12000), units);

    const std::int64_t fighterSpeed = static_cast<std::int64_t>(Outpost::Derive(Outpost::DesignId::Fighter).speedUnitsPerSecond);
    const std::int64_t tenths = (units * 10) / fighterSpeed;
    Assert::AreEqual(static_cast<std::int64_t>(857), tenths);
    Assert::IsTrue((tenths >= 800) && (tenths <= 1000), L"the crossing left GameDesign.md section 7's window");
  }

  /// **AND AT FOUR PLAYERS THE ADJACENT CROSSING FALLS OUT OF THAT WINDOW ENTIRELY** -- 8,485 units is
  /// 60.6 seconds against section 7's floor of eighty. The register names it as a consequence to measure
  /// at M4 rather than a reason to move the radius now, and it is pinned here because it is a number
  /// nobody would look at again otherwise.
  TEST_METHOD(FourPlayersPutAdjacentStationsInsideTheRaidWindow)
  {
    const std::vector<Outpost::Placement> layout = Outpost::GenerateLayout(0, 4);
    Assert::AreEqual(static_cast<std::size_t>(4), layout.size());

    const std::int64_t adjacent = Neuron::Sqrt(Neuron::LengthSquared(layout[1].position - layout[0].position)) / Neuron::FIXED_ONE;
    const std::int64_t opposed = Neuron::Sqrt(Neuron::LengthSquared(layout[2].position - layout[0].position)) / Neuron::FIXED_ONE;

    Assert::AreEqual(static_cast<std::int64_t>(8485), adjacent);
    Assert::AreEqual(static_cast<std::int64_t>(12000), opposed);

    const std::int64_t fighterSpeed = static_cast<std::int64_t>(Outpost::Derive(Outpost::DesignId::Fighter).speedUnitsPerSecond);
    const std::int64_t tenths = (adjacent * 10) / fighterSpeed;
    Assert::AreEqual(static_cast<std::int64_t>(606), tenths);
    Assert::IsTrue(tenths < 800, L"the four-player consequence Q26 names has gone away and nobody said so");
  }

  /// Every anchor is inside the play area with room for the camera clamp -- 2,192 units of it, which is
  /// what the radius was chosen to leave.
  TEST_METHOD(EveryAnchorIsInsideThePlayArea)
  {
    for (std::size_t index = 1; index <= Outpost::ANCHOR_COUNT; ++index)
    {
      const Neuron::Vec2 anchor = Outpost::StartAnchor(4, static_cast<Outpost::PlayerId>(index));
      Assert::IsTrue(anchor.x > -Outpost::PLAY_AREA_HALF_EXTENT);
      Assert::IsTrue(anchor.x < Outpost::PLAY_AREA_HALF_EXTENT);
      Assert::IsTrue(anchor.y > -Outpost::PLAY_AREA_HALF_EXTENT);
      Assert::IsTrue(anchor.y < Outpost::PLAY_AREA_HALF_EXTENT);
      Assert::IsTrue(Outpost::ClampToPlayArea(anchor) == anchor, L"an anchor had to be clamped");
    }
  }

  /// **EVERY ANCHOR IS A QUARTER TURN OF THE ONE BEFORE, IN POSITION AND IN HEADING TOGETHER.** That is
  /// what makes the heading exact: nothing computes an angle toward the center, it is the same count of
  /// quarter turns.
  TEST_METHOD(EachAnchorIsAQuarterTurnOfTheLast)
  {
    for (std::size_t index = 1; index < Outpost::ANCHOR_COUNT; ++index)
    {
      const Neuron::Vec2 previous = Outpost::StartAnchor(4, static_cast<Outpost::PlayerId>(index));
      const Neuron::Vec2 next = Outpost::StartAnchor(4, static_cast<Outpost::PlayerId>(index + 1));

      Assert::AreEqual(-previous.y, next.x);
      Assert::AreEqual(previous.x, next.y);

      const Neuron::Angle before = Outpost::StartHeading(4, static_cast<Outpost::PlayerId>(index));
      const Neuron::Angle after = Outpost::StartHeading(4, static_cast<Outpost::PlayerId>(index + 1));
      Assert::AreEqual(static_cast<int>(Neuron::ANGLE_QUARTER_TURN), static_cast<int>(static_cast<Neuron::Angle>(after - before)));
    }
  }

  /// **A STATION FACES THE CENTER.** Asserted through the trigonometry rather than against the angle
  /// literal, because the claim is about where it looks and not about which number that is: the heading's
  /// direction vector, scaled by the radius, has to land on the origin.
  TEST_METHOD(EveryStationFacesTheCenter)
  {
    for (std::size_t index = 1; index <= Outpost::ANCHOR_COUNT; ++index)
    {
      const Neuron::Vec2 anchor = Outpost::StartAnchor(4, static_cast<Outpost::PlayerId>(index));
      const Neuron::Angle heading = Outpost::StartHeading(4, static_cast<Outpost::PlayerId>(index));

      // The anchors sit on the axes, so the sine and cosine are exactly zero and plus or minus one and
      // there is no rounding to allow for.
      const std::int64_t towardX = (static_cast<std::int64_t>(Neuron::Cosine(heading)) * Outpost::ANCHOR_RADIUS) / Neuron::SINE_ONE;
      const std::int64_t towardY = (static_cast<std::int64_t>(Neuron::Sine(heading)) * Outpost::ANCHOR_RADIUS) / Neuron::SINE_ONE;

      Assert::AreEqual(static_cast<std::int64_t>(0), static_cast<std::int64_t>(anchor.x) + towardX);
      Assert::AreEqual(static_cast<std::int64_t>(0), static_cast<std::int64_t>(anchor.y) + towardY);
    }
  }

  /// A player who has no start gets the origin, which no anchor is -- so the caller can tell.
  TEST_METHOD(APlayerWithNoStartGetsTheOrigin)
  {
    Assert::IsTrue(Outpost::StartAnchor(2, Outpost::NO_PLAYER) == Neuron::Vec2{});
    Assert::IsTrue(Outpost::StartAnchor(2, 3) == Neuron::Vec2{});
    Assert::IsTrue(Outpost::StartAnchor(0, 1) == Neuron::Vec2{});
    Assert::AreEqual(static_cast<std::size_t>(0), Outpost::GenerateLayout(0, 0).size());
  }

  /// **ABOVE FOUR, EVERY PLAYER GETS A STATION** (ADR-023's stress layout), up to every `PlayerId` there
  /// is -- and a count past that is clamped rather than refused, the way `Sessions` clamps it.
  TEST_METHOD(AStressCountSeatsEveryPlayerUpToTheCapacity)
  {
    Assert::AreEqual(std::size_t{8}, Outpost::GenerateLayout(0, 8).size());
    Assert::AreEqual(std::size_t{100}, Outpost::GenerateLayout(0, 100).size());
    Assert::AreEqual(Outpost::MAX_PLAYERS, Outpost::GenerateLayout(0, 300).size());
  }

  /// Integer arithmetic on a pinned table, so the same count gives the same starts twice (R16, R23) -- and
  /// every start is on the anchor circle, inside the play area, and not on top of another.
  TEST_METHOD(TheStressLayoutIsDeterministicDistinctAndOnTheCircle)
  {
    for (const std::size_t players : {std::size_t{5}, std::size_t{8}, std::size_t{16}, std::size_t{100}})
    {
      const std::vector<Outpost::Placement> first = Outpost::GenerateLayout(0, players);
      Assert::IsTrue(first == Outpost::GenerateLayout(0, players), L"the stress layout moved between two calls");

      for (std::size_t index = 0; index < first.size(); ++index)
      {
        const Neuron::Vec2 anchor = first[index].position;
        Assert::IsTrue(Outpost::ClampToPlayArea(anchor) == anchor, L"a stress start had to be clamped");

        // On the circle to within the table's one part in 32,768 -- a fraction of a world unit.
        const std::int64_t xUnits = anchor.x / Neuron::FIXED_ONE;
        const std::int64_t yUnits = anchor.y / Neuron::FIXED_ONE;
        const std::int64_t radiusSquared = (xUnits * xUnits) + (yUnits * yUnits);
        const std::int64_t expected = static_cast<std::int64_t>(Outpost::ANCHOR_RADIUS_UNITS) * Outpost::ANCHOR_RADIUS_UNITS;
        Assert::IsTrue((radiusSquared > expected - 30000) && (radiusSquared < expected + 30000), L"a stress start is off the circle");

        for (std::size_t other = index + 1; other < first.size(); ++other)
        {
          Assert::IsFalse(first[other].position == anchor, L"two stress starts share a place");
        }
      }
    }
  }

  /// **FACING THE CENTER**, as the quarter-turn anchors do -- within the table's rounding, since an eighth of
  /// a turn is not exact.
  TEST_METHOD(EveryStressStationFacesTheCenter)
  {
    constexpr std::size_t PLAYERS = 12;
    for (std::size_t index = 1; index <= PLAYERS; ++index)
    {
      const Neuron::Vec2 anchor = Outpost::StartAnchor(PLAYERS, static_cast<Outpost::PlayerId>(index));
      const Neuron::Angle heading = Outpost::StartHeading(PLAYERS, static_cast<Outpost::PlayerId>(index));

      const std::int64_t towardX = (static_cast<std::int64_t>(Neuron::Cosine(heading)) * Outpost::ANCHOR_RADIUS) / Neuron::SINE_ONE;
      const std::int64_t towardY = (static_cast<std::int64_t>(Neuron::Sine(heading)) * Outpost::ANCHOR_RADIUS) / Neuron::SINE_ONE;
      const std::int64_t missX = static_cast<std::int64_t>(anchor.x) + towardX;
      const std::int64_t missY = static_cast<std::int64_t>(anchor.y) + towardY;

      // Within two world units of the origin, which is what the table's resolution leaves at six thousand.
      Assert::IsTrue((missX * missX) + (missY * missY) < (4 * Neuron::FIXED_ONE * Neuron::FIXED_ONE), L"a stress station looks away");
    }
  }

  /// **TWO AND FOUR ARE UNTOUCHED BY IT.** The stress path starts above four, so the fair layouts every other
  /// test here pins are the same bit for bit.
  TEST_METHOD(TheFairLayoutsDoNotTakeTheStressPath)
  {
    Assert::IsTrue(Outpost::StartAnchor(2, 1) == Neuron::Vec2{.x = -Outpost::ANCHOR_RADIUS, .y = 0});
    Assert::IsTrue(Outpost::StartAnchor(4, 1) == Neuron::Vec2{.x = -Outpost::ANCHOR_RADIUS, .y = 0});
    Assert::AreEqual(0, static_cast<int>(Outpost::StartHeading(4, 1)));
  }
};

/// The station the layout places is a row in the design table and needs no code of its own -- which is
/// ADR-006's claim, checked where it is first relied on.
TEST_CLASS(TheStationTheLayoutPlaces)
{
public:
  TEST_METHOD(ItIsAStationHullWithNoDriveAndTwoMounts)
  {
    const Outpost::DesignEntry& station = Outpost::Design(Outpost::DesignId::Station);
    Assert::IsTrue(station.hull == Outpost::HullId::Station);
    Assert::IsTrue(station.drive == Outpost::DriveId::None);
    Assert::IsTrue(station.slots[0] == Outpost::ComponentId::PointDefense);
    Assert::IsTrue(station.slots[1] == Outpost::ComponentId::PointDefense);

    const Outpost::DerivedStats stats = Outpost::Derive(Outpost::DesignId::Station);
    Assert::AreEqual(0u, stats.speedUnitsPerSecond, L"a station never moves");
    Assert::AreEqual(8000u, stats.hullPoints);
  }

  /// **THE SAFE ZONE STOPS WELL SHORT OF THE HOME FIELD, AND THAT IS THE POINT**
  /// (`GameDesign.md` section 5). Point defense reaches 400 against a home field about 1,500 out, so
  /// miners at the rocks are raidable -- and a `MassDriver` reaches 600, so a fighter can stand off at
  /// 500 and shell the station untouched. Both are deliberate and both read as bugs, so both are pinned
  /// here where the station is first placed on a map.
  TEST_METHOD(ThePointDefenseOutrangesNothing)
  {
    Assert::AreEqual(480, static_cast<int>(Outpost::Component(Outpost::ComponentId::PointDefense).rangeUnits), L"Q63");
    Assert::AreEqual(600, static_cast<int>(Outpost::Component(Outpost::ComponentId::MassDriver).rangeUnits));
    Assert::IsTrue(Outpost::Component(Outpost::ComponentId::PointDefense).rangeUnits <
                     Outpost::Component(Outpost::ComponentId::MassDriver).rangeUnits,
                   L"a station whose defense outranged the fighter would be unkillable");
  }
};

} // namespace GameCoreTests
