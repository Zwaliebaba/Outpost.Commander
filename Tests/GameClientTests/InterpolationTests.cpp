#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// An update carrying one entity at a position and heading, at a tick -- which is all any test here
/// needs. The wire fields it does not set are zero and stay that way.
[[nodiscard]] Outpost::Update MakeUpdate(std::uint32_t _tick, std::int16_t _positionX, std::uint8_t _heading,
                                         Outpost::WireIdentity _identity = Outpost::PackIdentity(1, 1))
{
  Outpost::Update update;
  update.sequence = static_cast<std::uint16_t>(_tick);
  update.tick = _tick;
  update.liveEntityCount = 1;

  Outpost::EntityRecord record;
  record.identity = _identity;
  record.positionX = _positionX;
  record.heading = _heading;
  update.records.push_back(record);

  return update;
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
/// ADR-024's store: per entity, ordered by tick, holding rather than extrapolating, forgetting after
/// three sweeps.
TEST_CLASS(ReplicaStoreBehavior)
{
public:
  TEST_METHOD(ItRetainsEnoughToCoverTheDelay)
  {
    // Three samples an entity at 75 over 50: two intervals reach back 100 milliseconds, which is past
    // the delay, and the third is the one on the near side of it.
    Assert::AreEqual(static_cast<std::size_t>(3), Outpost::ReplicaStore::RETAINED_COUNT);
    const std::uint64_t spanned =
      static_cast<std::uint64_t>(Outpost::ReplicaStore::RETAINED_COUNT - 1) * Outpost::SNAPSHOT_INTERVAL_MILLISECONDS;
    Assert::IsTrue(spanned >= Outpost::INTERPOLATION_DELAY_MILLISECONDS);
  }

  TEST_METHOD(TheClockNeverRunsAheadOfTheDelay)
  {
    Assert::AreEqual(static_cast<std::uint64_t>(0), Outpost::ReplicaStore::RenderMilliseconds(0));
    Assert::AreEqual(static_cast<std::uint64_t>(0), Outpost::ReplicaStore::RenderMilliseconds(75));
    Assert::AreEqual(static_cast<std::uint64_t>(25), Outpost::ReplicaStore::RenderMilliseconds(100));
  }

  /// **ORDERING IS A TICK COMPARISON AND NOTHING ELSE.** A record no newer than the entity's newest
  /// sample is refused, whatever order the updates arrived in.
  TEST_METHOD(ARecordNoNewerThanWhatIsHeldIsRefused)
  {
    Outpost::ReplicaStore store;
    Assert::AreEqual(1u, store.Accept(MakeUpdate(10, 0, 0), 1000).applied);
    Assert::AreEqual(1u, store.Accept(MakeUpdate(11, 100, 0), 1050).applied);

    Assert::AreEqual(1u, store.Accept(MakeUpdate(10, 999, 0), 1060).refused, L"an older tick was admitted");
    Assert::AreEqual(1u, store.Accept(MakeUpdate(11, 999, 0), 1060).refused, L"the same tick was admitted twice");
    Assert::AreEqual(static_cast<std::uint64_t>(2), store.RefusedCount());
    Assert::AreEqual(std::int16_t{100}, store.Entities()[0].positionX, L"a refused record moved the entity");
  }

  TEST_METHOD(TheSequenceComparisonSurvivesTheWrap)
  {
    Assert::IsTrue(Outpost::SequenceIsNewer(1, 65535));
    Assert::IsFalse(Outpost::SequenceIsNewer(65535, 1));
    Assert::IsFalse(Outpost::SequenceIsNewer(7, 7));
  }

  /// The sequence orders nothing; it counts loss. Two updates that never came are two lost.
  TEST_METHOD(AGapInTheSequenceIsCountedAsLoss)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 0, 0), 1000));
    static_cast<void>(store.Accept(MakeUpdate(4, 0, 0), 1150));
    Assert::AreEqual(static_cast<std::uint64_t>(2), store.LostCount());
  }

  TEST_METHOD(ItInterpolatesBetweenTheEntitysOwnSamples)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 0, 0), 1000));
    static_cast<void>(store.Accept(MakeUpdate(2, 100, 0), 1050));
    static_cast<void>(store.Accept(MakeUpdate(3, 200, 0), 1100));

    // 75 behind 1,100 is 1,025, which is halfway between the first two samples.
    std::vector<Outpost::EntityRecord> drawn;
    const Outpost::DrawnSummary summary = store.Drawn(Outpost::ReplicaStore::RenderMilliseconds(1100), drawn);
    Assert::AreEqual(1u, summary.interpolating);
    Assert::AreEqual(std::size_t{1}, drawn.size());
    Assert::AreEqual(std::int16_t{50}, drawn[0].positionX);
  }

  /// **PAST THE NEWEST SAMPLE IT HOLDS, AND NEVER EXTRAPOLATES** (ADR-024). A distant entity the
  /// accumulator has not refreshed stays where the host last said it was.
  TEST_METHOD(PastTheNewestSampleItHolds)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 0, 0), 1000));
    static_cast<void>(store.Accept(MakeUpdate(2, 100, 0), 1050));

    std::vector<Outpost::EntityRecord> drawn;
    const Outpost::DrawnSummary summary = store.Drawn(5000, drawn);
    Assert::AreEqual(1u, summary.holding);
    Assert::AreEqual(std::int16_t{100}, drawn[0].positionX, L"a held entity moved past its newest sample");
  }

  TEST_METHOD(OneSampleIsStarvedRatherThanAnError)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 40, 0), 1000));
    std::vector<Outpost::EntityRecord> drawn;
    Assert::AreEqual(1u, store.Drawn(1000, drawn).starved);
    Assert::AreEqual(std::int16_t{40}, drawn[0].positionX);
  }

  TEST_METHOD(AnEmptyStoreHandsBackNothingToDraw)
  {
    const Outpost::ReplicaStore store;
    std::vector<Outpost::EntityRecord> drawn{Outpost::EntityRecord{}};
    const Outpost::DrawnSummary summary = store.Drawn(1000, drawn);
    Assert::AreEqual(std::size_t{0}, drawn.size(), L"the output was not cleared");
    Assert::AreEqual(0u, summary.interpolating + summary.holding + summary.starved);
    Assert::IsNull(store.Own());
  }

  /// **AN ENTITY ABSENT FROM AN UPDATE IS NOT DEAD.** It was not due; it stays.
  TEST_METHOD(AnEntityAbsentFromAnUpdateIsKept)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 10, 0, Outpost::PackIdentity(1, 1)), 1000));
    static_cast<void>(store.Accept(MakeUpdate(2, 20, 0, Outpost::PackIdentity(2, 1)), 1050));
    Assert::AreEqual(std::size_t{2}, store.HeldCount());
  }

  /// **DEATH IS A REMOVAL**, matched on the whole identity, and a repeat of one already applied takes
  /// nothing -- in particular not a new occupant of the same slot.
  TEST_METHOD(ARemovalTakesOnlyTheEntityItNames)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 10, 0, Outpost::PackIdentity(1, 1)), 1000));

    Outpost::Update death = MakeUpdate(2, 20, 0, Outpost::PackIdentity(1, 2));
    death.records.clear();
    death.removals.push_back(Outpost::PackIdentity(1, 2));
    Assert::AreEqual(0u, store.Accept(death, 1050).removed, L"a removal naming another generation took the entity");

    death.tick = 3;
    death.removals = {Outpost::PackIdentity(1, 1)};
    Assert::AreEqual(1u, store.Accept(death, 1100).removed);
    Assert::AreEqual(std::size_t{0}, store.HeldCount());

    death.tick = 4;
    Assert::AreEqual(0u, store.Accept(death, 1150).removed, L"a repeated removal counted twice");
  }

  /// A reused slot is a new entity: it starts from its own first sample, never from the old occupant's.
  TEST_METHOD(AReusedSlotStartsAgainFromNothing)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 0, 0, Outpost::PackIdentity(1, 1)), 1000));
    static_cast<void>(store.Accept(MakeUpdate(2, 500, 0, Outpost::PackIdentity(1, 2)), 1050));

    std::vector<Outpost::EntityRecord> drawn;
    Assert::AreEqual(1u, store.Drawn(1025, drawn).starved, L"the new occupant was interpolated from the old one");
    Assert::AreEqual(std::int16_t{500}, drawn[0].positionX);
  }

  /// **FORGETTING IS THE SWEEP.** One live entity is a sweep of one tick, so an entity three ticks
  /// behind the newest update stays and one further behind is gone.
  TEST_METHOD(AnEntityThreeSweepsSilentIsForgotten)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 0, 0, Outpost::PackIdentity(7, 1)), 1000));

    Outpost::Update quiet = MakeUpdate(4, 0, 0, Outpost::PackIdentity(8, 1));
    Assert::AreEqual(0u, store.Accept(quiet, 1150).forgotten, L"forgotten inside three sweeps");
    Assert::AreEqual(std::size_t{2}, store.HeldCount());

    quiet.tick = 5;
    Assert::AreEqual(1u, store.Accept(quiet, 1200).forgotten);
    Assert::AreEqual(std::size_t{1}, store.HeldCount());
  }

  /// The own block follows the newest tick, so a late update cannot wind the credits back.
  TEST_METHOD(TheOwnBlockFollowsTheNewestTick)
  {
    Outpost::ReplicaStore store;
    Outpost::Update newer = MakeUpdate(9, 0, 0);
    newer.own.credits = 900;
    Outpost::Update older = MakeUpdate(8, 0, 0);
    older.own.credits = 100;

    static_cast<void>(store.Accept(newer, 1000));
    static_cast<void>(store.Accept(older, 1010));
    Assert::AreEqual(900u, store.Own()->credits);
  }

  TEST_METHOD(ClearForgetsEverything)
  {
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(MakeUpdate(1, 0, 0), 1000));
    store.Clear();
    Assert::AreEqual(std::size_t{0}, store.HeldCount());
    Assert::IsNull(store.Own());
  }
};

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
