#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// A snapshot carrying one entity at a position and heading, which is all any test here needs.
/// The wire fields it does not set are zero and stay that way.
[[nodiscard]] Outpost::Snapshot MakeSnapshot(std::uint16_t _sequence, std::int16_t _positionX, std::uint8_t _heading)
{
  Outpost::Snapshot snapshot;
  snapshot.sequence = _sequence;
  snapshot.tick = _sequence;

  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(1, 0);
  record.positionX = _positionX;
  record.heading = _heading;
  snapshot.entities.push_back(record);

  return snapshot;
}
} // namespace

/// `TechnicalDesign.md` section 6 and ADR-003: the frame is drawn 75 milliseconds behind, between
/// whichever pair of snapshots straddles that moment.
TEST_CLASS(InterpolationArithmetic)
{
public:
  TEST_METHOD(TheDelayIsOneNamedConstant)
  {
    // The plan's step asks for this in as many words: "the 75 is one named constant rather than a
    // literal in three places". Asserting the value is how the constant stops being renamed by
    // accident; ADR-003 and TechnicalDesign section 4 are where it comes from.
    Assert::AreEqual(75u, Outpost::INTERPOLATION_DELAY_MILLISECONDS);
    Assert::AreEqual(50u, Outpost::SNAPSHOT_INTERVAL_MILLISECONDS);
  }

  TEST_METHOD(TheFractionRunsFromZeroToOneAcrossThePair)
  {
    const Outpost::Playout atOlder = Outpost::ComputePlayout(1000, 1050, 1000);
    Assert::IsTrue(atOlder.state == Outpost::PlayoutState::Interpolating);
    Assert::AreEqual(0, atOlder.fraction);

    const Outpost::Playout atMiddle = Outpost::ComputePlayout(1000, 1050, 1025);
    Assert::IsTrue(atMiddle.state == Outpost::PlayoutState::Interpolating);
    Assert::AreEqual(Neuron::FIXED_ONE / 2, atMiddle.fraction);

    const Outpost::Playout atNewer = Outpost::ComputePlayout(1000, 1050, 1050);
    Assert::IsTrue(atNewer.state == Outpost::PlayoutState::Interpolating);
    Assert::AreEqual(Neuron::FIXED_ONE, atNewer.fraction);
  }

  TEST_METHOD(APairThatStraddlesNothingIsStarved)
  {
    const Outpost::Playout same = Outpost::ComputePlayout(1000, 1000, 1000);
    Assert::IsTrue(same.state == Outpost::PlayoutState::Starved);

    const Outpost::Playout backwards = Outpost::ComputePlayout(1050, 1000, 1025);
    Assert::IsTrue(backwards.state == Outpost::PlayoutState::Starved);
  }

  TEST_METHOD(APositionInterpolatesLinearly)
  {
    const Neuron::Fixed from = Neuron::FixedFromWholeUnits(100);
    const Neuron::Fixed to = Neuron::FixedFromWholeUnits(200);

    Assert::AreEqual(from, Outpost::InterpolatePosition(from, to, 0));
    Assert::AreEqual(to, Outpost::InterpolatePosition(from, to, Neuron::FIXED_ONE));
    Assert::AreEqual(Neuron::FixedFromWholeUnits(150), Outpost::InterpolatePosition(from, to, Neuron::FIXED_ONE / 2));
  }

  TEST_METHOD(APositionExtrapolatesAlongTheSameLine)
  {
    // A fraction past one is the extrapolating frame, and it must extend the line rather than
    // clamp -- that is the whole difference between guessing and freezing.
    const Neuron::Fixed from = Neuron::FixedFromWholeUnits(100);
    const Neuron::Fixed to = Neuron::FixedFromWholeUnits(200);
    Assert::AreEqual(Neuron::FixedFromWholeUnits(250), Outpost::InterpolatePosition(from, to, Neuron::FIXED_ONE + (Neuron::FIXED_ONE / 2)));
  }

  /// THE ONE THE BINARY ANGLE EXISTS FOR. A heading near the top of the range and one near the
  /// bottom are a short turn apart, and the interpolation has to take that turn rather than
  /// sweeping the long way back around.
  TEST_METHOD(AHeadingTakesTheShortWayRound)
  {
    // A sixteenth of a turn either side of zero.
    const Neuron::Angle from = static_cast<Neuron::Angle>(65536 - 4096);
    const Neuron::Angle to = 4096;

    const Neuron::Angle halfway = Outpost::InterpolateHeading(from, to, Neuron::FIXED_ONE / 2);
    // Straight through zero, not through the half turn on the other side.
    Assert::AreEqual(0, static_cast<int>(halfway));

    Assert::AreEqual(static_cast<int>(from), static_cast<int>(Outpost::InterpolateHeading(from, to, 0)));
    Assert::AreEqual(static_cast<int>(to), static_cast<int>(Outpost::InterpolateHeading(from, to, Neuron::FIXED_ONE)));
  }

  TEST_METHOD(AHeadingGoesTheOtherShortWayToo)
  {
    // The mirror of the test above, so that a sign error in AngleDifference cannot pass both.
    const Neuron::Angle from = 4096;
    const Neuron::Angle to = static_cast<Neuron::Angle>(65536 - 4096);

    const Neuron::Angle halfway = Outpost::InterpolateHeading(from, to, Neuron::FIXED_ONE / 2);
    Assert::AreEqual(0, static_cast<int>(halfway));
  }

  TEST_METHOD(AWireHeadingWidensExactly)
  {
    // 256 steps to 65,536: a shift, and the two ends have to land on the ends.
    Assert::AreEqual(0, static_cast<int>(Outpost::DequantizeWireHeading(0)));
    Assert::AreEqual(16384, static_cast<int>(Outpost::DequantizeWireHeading(64)));
    Assert::AreEqual(65280, static_cast<int>(Outpost::DequantizeWireHeading(255)));
  }
};

/// The extrapolation bound, which the plan asks to be asserted rather than implied.
TEST_CLASS(ExtrapolationBound)
{
public:
  TEST_METHOD(PastTheNewestItExtrapolates)
  {
    const Outpost::Playout justPast = Outpost::ComputePlayout(1000, 1050, 1051);
    Assert::IsTrue(justPast.state == Outpost::PlayoutState::Extrapolating);
    Assert::IsTrue(justPast.fraction > Neuron::FIXED_ONE);
  }

  TEST_METHOD(AtTheBoundItStillExtrapolates)
  {
    const std::uint64_t atBound = 1050 + Outpost::EXTRAPOLATION_BOUND_MILLISECONDS;
    const Outpost::Playout playout = Outpost::ComputePlayout(1000, 1050, atBound);
    Assert::IsTrue(playout.state == Outpost::PlayoutState::Extrapolating);
  }

  TEST_METHOD(PastTheBoundItHolds)
  {
    const std::uint64_t pastBound = 1050 + Outpost::EXTRAPOLATION_BOUND_MILLISECONDS + 1;
    const Outpost::Playout playout = Outpost::ComputePlayout(1000, 1050, pastBound);
    Assert::IsTrue(playout.state == Outpost::PlayoutState::Holding);
  }

  /// HOLDING FREEZES WHERE THE GUESS REACHED, and does not snap back to the last snapshot. A
  /// frame that crossed the bound must not move the ship backwards on screen.
  TEST_METHOD(HoldingFreezesTheFractionAndStopsMoving)
  {
    const std::uint64_t pastBound = 1050 + Outpost::EXTRAPOLATION_BOUND_MILLISECONDS + 1;
    const Outpost::Playout first = Outpost::ComputePlayout(1000, 1050, pastBound);
    const Outpost::Playout muchLater = Outpost::ComputePlayout(1000, 1050, pastBound + 5000);

    Assert::IsTrue(first.state == Outpost::PlayoutState::Holding);
    Assert::IsTrue(muchLater.state == Outpost::PlayoutState::Holding);
    Assert::AreEqual(first.fraction, muchLater.fraction);

    // And it is the bound it froze at, not the newest snapshot: one interval of extrapolation on
    // top of one interval of span is a fraction of two.
    Assert::AreEqual(Neuron::FIXED_ONE * 2, first.fraction);
  }
};

/// The store: what it keeps, what it refuses, and that its clock only ever goes forwards.
TEST_CLASS(ReplicaStoreBehavior)
{
public:
  TEST_METHOD(ItRetainsEnoughToCoverTheDelay)
  {
    // The depth is arithmetic on the delay and the interval rather than a chosen number -- at 75
    // over 50 that is three. Two would not reach: a pair spans one interval and the render time
    // is an interval and a half behind the newest.
    Assert::AreEqual(static_cast<std::size_t>(3), Outpost::ReplicaStore::RETAINED_COUNT);

    const std::uint64_t deepestReach =
      static_cast<std::uint64_t>(Outpost::ReplicaStore::RETAINED_COUNT - 1) * Outpost::SNAPSHOT_INTERVAL_MILLISECONDS;
    Assert::IsTrue(deepestReach >= Outpost::INTERPOLATION_DELAY_MILLISECONDS);
  }

  TEST_METHOD(TheClockNeverRunsAheadOfTheDelay)
  {
    Assert::AreEqual(static_cast<std::uint64_t>(0), Outpost::ReplicaStore::RenderMilliseconds(0));
    Assert::AreEqual(static_cast<std::uint64_t>(0), Outpost::ReplicaStore::RenderMilliseconds(75));
    Assert::AreEqual(static_cast<std::uint64_t>(25), Outpost::ReplicaStore::RenderMilliseconds(100));
  }

  TEST_METHOD(AnOutOfOrderArrivalIsRefused)
  {
    Outpost::ReplicaStore store;
    Assert::IsTrue(store.Accept(MakeSnapshot(10, 0, 0), 1000));
    Assert::IsTrue(store.Accept(MakeSnapshot(11, 100, 0), 1050));

    // The straggler, and the duplicate.
    Assert::IsFalse(store.Accept(MakeSnapshot(10, 999, 0), 1060));
    Assert::IsFalse(store.Accept(MakeSnapshot(11, 999, 0), 1060));
    Assert::AreEqual(static_cast<std::uint64_t>(2), store.RefusedCount());

    // And it did not become the newest, which is the thing that would move the clock backwards.
    Assert::AreEqual(11, static_cast<int>(store.Newest()->sequence));
  }

  TEST_METHOD(TheSequenceComparisonSurvivesTheWrap)
  {
    Assert::IsTrue(Outpost::SequenceIsNewer(0, 65535));
    Assert::IsTrue(Outpost::SequenceIsNewer(1, 65535));
    Assert::IsFalse(Outpost::SequenceIsNewer(65535, 0));
    Assert::IsFalse(Outpost::SequenceIsNewer(10, 10));
  }

  TEST_METHOD(ItFindsThePairThatStraddlesTheRenderTime)
  {
    Outpost::ReplicaStore store;
    Assert::IsTrue(store.Accept(MakeSnapshot(1, 0, 0), 1000));
    Assert::IsTrue(store.Accept(MakeSnapshot(2, 100, 0), 1050));
    Assert::IsTrue(store.Accept(MakeSnapshot(3, 200, 0), 1100));

    // Now is 1100 -- the newest has just arrived -- so the frame is drawn at 1025, which is
    // between the first two. THIS IS THE CASE TWO SNAPSHOTS CANNOT SERVE.
    const std::uint64_t renderTime = Outpost::ReplicaStore::RenderMilliseconds(1100);
    Assert::AreEqual(static_cast<std::uint64_t>(1025), renderTime);

    const Outpost::ReplicaStore::Frame frame = store.FrameAt(renderTime);
    Assert::IsTrue(frame.playout.state == Outpost::PlayoutState::Interpolating);
    Assert::AreEqual(1, static_cast<int>(frame.older->sequence));
    Assert::AreEqual(2, static_cast<int>(frame.newer->sequence));
    Assert::AreEqual(Neuron::FIXED_ONE / 2, frame.playout.fraction);
  }

  TEST_METHOD(AMissingSnapshotExtrapolatesAndThenHolds)
  {
    Outpost::ReplicaStore store;
    Assert::IsTrue(store.Accept(MakeSnapshot(1, 0, 0), 1000));
    Assert::IsTrue(store.Accept(MakeSnapshot(2, 100, 0), 1050));

    // Nothing arrives after 1050. The render clock walks past the newest and the frame goes from
    // interpolating, to guessing, to still.
    Assert::IsTrue(store.FrameAt(1040).playout.state == Outpost::PlayoutState::Interpolating);
    Assert::IsTrue(store.FrameAt(1060).playout.state == Outpost::PlayoutState::Extrapolating);
    Assert::IsTrue(store.FrameAt(1200).playout.state == Outpost::PlayoutState::Holding);
  }

  TEST_METHOD(OneSnapshotIsStarvedRatherThanAnError)
  {
    Outpost::ReplicaStore store;
    Assert::IsTrue(store.Accept(MakeSnapshot(1, 0, 0), 1000));

    const Outpost::ReplicaStore::Frame frame = store.FrameAt(1000);
    Assert::IsTrue(frame.playout.state == Outpost::PlayoutState::Starved);
    // Still something to draw, and both ends are the same snapshot.
    Assert::IsNotNull(frame.older);
    Assert::IsTrue(frame.older == frame.newer);
  }

  TEST_METHOD(AnEmptyStoreHandsBackNothingToDraw)
  {
    const Outpost::ReplicaStore store;
    const Outpost::ReplicaStore::Frame frame = store.FrameAt(1000);
    Assert::IsNull(frame.older);
    Assert::IsNull(frame.newer);
    Assert::IsTrue(frame.playout.state == Outpost::PlayoutState::Starved);
  }

  TEST_METHOD(TheRingKeepsTheNewestWhenItOverflows)
  {
    Outpost::ReplicaStore store;
    for (std::uint16_t sequence = 1; sequence <= 6; ++sequence)
    {
      const std::uint64_t arrival = 1000 + (static_cast<std::uint64_t>(sequence) * 50);
      Assert::IsTrue(store.Accept(MakeSnapshot(sequence, static_cast<std::int16_t>(sequence * 10), 0), arrival));
    }

    Assert::AreEqual(Outpost::ReplicaStore::RETAINED_COUNT, store.HeldCount());
    Assert::AreEqual(6, static_cast<int>(store.Newest()->sequence));

    // The pair around the render time is still found after the ring has wrapped several times,
    // which is what proves the indexing rather than the first three arrivals doing so.
    const Outpost::ReplicaStore::Frame frame = store.FrameAt(1275);
    Assert::IsTrue(frame.playout.state == Outpost::PlayoutState::Interpolating);
    Assert::AreEqual(5, static_cast<int>(frame.older->sequence));
    Assert::AreEqual(6, static_cast<int>(frame.newer->sequence));
  }
};

/// One entity across the pair, and the trap the generation exists to close.
TEST_CLASS(RecordInterpolation)
{
public:
  TEST_METHOD(APositionAndHeadingMoveTogether)
  {
    Outpost::EntityRecord older;
    older.identity = Outpost::PackIdentity(7, 1);
    older.positionX = 0;
    older.positionY = 0;
    older.heading = 0;
    older.hullPercentRemaining = 100;

    Outpost::EntityRecord newer = older;
    newer.positionX = 400;
    newer.positionY = 800;
    newer.heading = 64;
    newer.hullPercentRemaining = 50;

    Outpost::EntityRecord result;
    Assert::IsTrue(Outpost::InterpolateRecord(older, newer, Neuron::FIXED_ONE / 2, result));

    Assert::AreEqual(200, static_cast<int>(result.positionX));
    Assert::AreEqual(400, static_cast<int>(result.positionY));
    Assert::AreEqual(32, static_cast<int>(result.heading));

    // Hull does not interpolate: a percentage between two integers is a number the host never
    // held, so the newer truth is what comes out.
    Assert::AreEqual(50, static_cast<int>(result.hullPercentRemaining));
  }

  /// A SLOT REUSED BETWEEN TWO SNAPSHOTS IS NOT ONE SHIP MOVING. Same index, different
  /// generation, and interpolating across it would draw the dead ship sliding into the new one.
  TEST_METHOD(AReusedSlotIsRefusedRatherThanInterpolated)
  {
    Outpost::EntityRecord older;
    older.identity = Outpost::PackIdentity(7, 1);
    older.positionX = 0;

    Outpost::EntityRecord newer;
    newer.identity = Outpost::PackIdentity(7, 2);
    newer.positionX = 4000;

    Assert::AreEqual(static_cast<int>(Outpost::IndexOf(older.identity)), static_cast<int>(Outpost::IndexOf(newer.identity)));

    Outpost::EntityRecord result;
    Assert::IsFalse(Outpost::InterpolateRecord(older, newer, Neuron::FIXED_ONE / 2, result));
  }

  TEST_METHOD(TheEndsAreTheRecordsThemselves)
  {
    Outpost::EntityRecord older;
    older.identity = Outpost::PackIdentity(3, 0);
    older.positionX = -1000;
    older.heading = 200;

    Outpost::EntityRecord newer = older;
    newer.positionX = 1000;
    newer.heading = 40;

    Outpost::EntityRecord atOlder;
    Assert::IsTrue(Outpost::InterpolateRecord(older, newer, 0, atOlder));
    Assert::AreEqual(static_cast<int>(older.positionX), static_cast<int>(atOlder.positionX));
    Assert::AreEqual(static_cast<int>(older.heading), static_cast<int>(atOlder.heading));

    Outpost::EntityRecord atNewer;
    Assert::IsTrue(Outpost::InterpolateRecord(older, newer, Neuron::FIXED_ONE, atNewer));
    Assert::AreEqual(static_cast<int>(newer.positionX), static_cast<int>(atNewer.positionX));
    Assert::AreEqual(static_cast<int>(newer.heading), static_cast<int>(atNewer.heading));
  }
};

} // namespace GameClientTests
