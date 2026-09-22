#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
[[nodiscard]] Outpost::OrderMarker MarkerAt(std::uint16_t _sequence, std::int16_t _targetX)
{
  Outpost::OrderMarker marker;
  marker.commandSequence = _sequence;
  marker.targetX = _targetX;
  marker.selection.push_back(Outpost::PackIdentity(1, 0));
  return marker;
}

[[nodiscard]] Outpost::PickCandidate Candidate(std::uint16_t _index, Outpost::PickTier _tier, float _distance)
{
  Outpost::PickCandidate candidate;
  candidate.identity = Outpost::PackIdentity(_index, 0);
  candidate.tier = _tier;
  candidate.screenDistanceAuthoredPixels = _distance;
  return candidate;
}

constexpr float TARGET_ASPECT = 1440.0f / 960.0f;
} // namespace

/// Q20 and `TechnicalDesign.md` section 6: the marker is drawn the instant the gesture resolves and
/// cleared when the acknowledgment covers it.
TEST_CLASS(OrderMarkers)
{
public:
  TEST_METHOD(AMarkerAppearsOnTheResolvingGesture)
  {
    Outpost::OrderMarkerSet markers;
    Assert::AreEqual(static_cast<std::size_t>(0), markers.Count());

    markers.Add(MarkerAt(1, 400));
    Assert::AreEqual(static_cast<std::size_t>(1), markers.Count());
    Assert::AreEqual(400, static_cast<int>(markers.Markers()[0].targetX));
  }

  /// The whole point of the marker: it has to survive the round trip it exists to cover.
  TEST_METHOD(ItSurvivesAnUnacknowledgedRoundTrip)
  {
    Outpost::OrderMarkerSet markers;
    markers.Add(MarkerAt(7, 400));

    // Snapshots keep arriving, and none of them has applied this command yet.
    for (std::uint16_t applied = 0; applied < 7; ++applied)
    {
      Assert::AreEqual(static_cast<std::size_t>(0), markers.ClearAcknowledged(applied));
      Assert::AreEqual(static_cast<std::size_t>(1), markers.Count());
    }
  }

  TEST_METHOD(TheAcknowledgmentThatCoversItClearsIt)
  {
    Outpost::OrderMarkerSet markers;
    markers.Add(MarkerAt(7, 400));

    Assert::AreEqual(static_cast<std::size_t>(1), markers.ClearAcknowledged(7));
    Assert::AreEqual(static_cast<std::size_t>(0), markers.Count());
  }

  /// AT OR BEFORE, NOT EQUAL TO. Three taps in a second are acknowledged by one number, and
  /// matching exactly would leave the first two drawn forever.
  TEST_METHOD(OneLaterSequenceClearsSeveralMarkersAtOnce)
  {
    Outpost::OrderMarkerSet markers;
    markers.Add(MarkerAt(4, 100));
    markers.Add(MarkerAt(5, 200));
    markers.Add(MarkerAt(6, 300));

    Assert::AreEqual(static_cast<std::size_t>(2), markers.ClearAcknowledged(5));
    Assert::AreEqual(static_cast<std::size_t>(1), markers.Count());
    Assert::AreEqual(300, static_cast<int>(markers.Markers()[0].targetX));
  }

  TEST_METHOD(ClearingSurvivesTheSequenceWrap)
  {
    Outpost::OrderMarkerSet markers;
    markers.Add(MarkerAt(65534, 100));
    markers.Add(MarkerAt(65535, 200));
    markers.Add(MarkerAt(0, 300));
    markers.Add(MarkerAt(1, 400));

    // An acknowledgment just past the wrap covers the three before it and not the one after.
    Assert::AreEqual(static_cast<std::size_t>(3), markers.ClearAcknowledged(0));
    Assert::AreEqual(static_cast<std::size_t>(1), markers.Count());
    Assert::AreEqual(400, static_cast<int>(markers.Markers()[0].targetX));
  }

  TEST_METHOD(TheLineIsDrawnFromTheSelectionTheOrderHad)
  {
    // The selection can change under the player's finger while an order is outstanding, and the
    // line belongs to the order rather than to whatever is selected now.
    Outpost::OrderMarker marker;
    marker.commandSequence = 3;
    marker.selection = {Outpost::PackIdentity(4, 0), Outpost::PackIdentity(5, 0)};

    Outpost::OrderMarkerSet markers;
    markers.Add(marker);

    Assert::AreEqual(static_cast<std::size_t>(2), markers.Markers()[0].selection.size());
  }
};

/// `Interface.md` section 1: nearest inside a tier, the tier across one.
TEST_CLASS(TapPicking)
{
public:
  TEST_METHOD(NothingInsideTheRadiusIsEmptySpace)
  {
    const Outpost::PickCandidate candidates[] = {Candidate(1, Outpost::PickTier::OwnShip, 25.0f),
                                                 Candidate(2, Outpost::PickTier::Hostile, 100.0f)};

    Outpost::PickCandidate hit;
    Assert::IsFalse(Outpost::ResolvePick(candidates, hit));
  }

  /// Exactly on the radius is still a hit -- exceeding rejects, the same edge rule the seam's palm
  /// rejection uses.
  TEST_METHOD(ExactlyOnTheRadiusIsStillAHit)
  {
    const Outpost::PickCandidate candidates[] = {Candidate(1, Outpost::PickTier::OwnShip, Neuron::PICK_RADIUS_AUTHORED_PIXELS)};

    Outpost::PickCandidate hit;
    Assert::IsTrue(Outpost::ResolvePick(candidates, hit));
  }

  /// THE TIER WINS ACROSS. A hostile at four pixels loses to your own ship at twenty, because the
  /// expensive failure is ordering your fleet somewhere by accident.
  TEST_METHOD(TheTierBeatsTheDistance)
  {
    const Outpost::PickCandidate candidates[] = {Candidate(1, Outpost::PickTier::Hostile, 4.0f),
                                                 Candidate(2, Outpost::PickTier::OwnShip, 20.0f)};

    Outpost::PickCandidate hit;
    Assert::IsTrue(Outpost::ResolvePick(candidates, hit));
    Assert::IsTrue(hit.tier == Outpost::PickTier::OwnShip);
  }

  TEST_METHOD(TheDistanceWinsInsideATier)
  {
    const Outpost::PickCandidate candidates[] = {Candidate(1, Outpost::PickTier::Hostile, 18.0f),
                                                 Candidate(2, Outpost::PickTier::Hostile, 6.0f),
                                                 Candidate(3, Outpost::PickTier::Hostile, 11.0f)};

    Outpost::PickCandidate hit;
    Assert::IsTrue(Outpost::ResolvePick(candidates, hit));
    Assert::AreEqual(static_cast<int>(Outpost::PackIdentity(2, 0)), static_cast<int>(hit.identity));
  }

  TEST_METHOD(TheWholeTierOrderHolds)
  {
    // Every tier present, each one further away than the last, so only the order can decide.
    const Outpost::PickCandidate candidates[] = {
      Candidate(1, Outpost::PickTier::Asteroid, 2.0f), Candidate(2, Outpost::PickTier::Hostile, 6.0f),
      Candidate(3, Outpost::PickTier::OwnStructure, 12.0f), Candidate(4, Outpost::PickTier::OwnShip, 20.0f)};

    Outpost::PickCandidate hit;
    Assert::IsTrue(Outpost::ResolvePick(candidates, hit));
    Assert::IsTrue(hit.tier == Outpost::PickTier::OwnShip);
  }
};

/// M0's one verb, and R19's line.
TEST_CLASS(TapOrders)
{
public:
  TEST_METHOD(EmptySpaceWithASelectionIsAMove)
  {
    Outpost::CameraPose pose;
    pose.distance = 8000.0f;

    const Outpost::TapOutcome outcome = Outpost::ResolveTap(pose, TARGET_ASPECT, 720.0f, 480.0f, 1440.0f, 960.0f, {}, true);

    Assert::IsTrue(outcome.action == Outpost::TapAction::MoveTo);
    // The center of the frame is the focus.
    Assert::AreEqual(pose.focusX, outcome.worldX, 1.0f);
    Assert::AreEqual(pose.focusY, outcome.worldY, 1.0f);
  }

  /// `Interface.md` section 4, stated explicitly because "nothing happens" is a decision here.
  TEST_METHOD(EmptySpaceWithNothingSelectedDoesNothing)
  {
    Outpost::CameraPose pose;
    pose.distance = 8000.0f;

    const Outpost::TapOutcome outcome = Outpost::ResolveTap(pose, TARGET_ASPECT, 720.0f, 480.0f, 1440.0f, 960.0f, {}, false);

    Assert::IsTrue(outcome.action == Outpost::TapAction::Nothing);
  }

  TEST_METHOD(ATapOnSomethingIsAboutThatThing)
  {
    Outpost::CameraPose pose;
    pose.distance = 8000.0f;

    const Outpost::PickCandidate candidates[] = {Candidate(9, Outpost::PickTier::Hostile, 5.0f)};
    const Outpost::TapOutcome outcome = Outpost::ResolveTap(pose, TARGET_ASPECT, 720.0f, 480.0f, 1440.0f, 960.0f, candidates, true);

    Assert::IsTrue(outcome.action == Outpost::TapAction::Occupied);
    Assert::AreEqual(static_cast<int>(Outpost::PackIdentity(9, 0)), static_cast<int>(outcome.hit.identity));
  }

  TEST_METHOD(AMoveCommandCarriesTheQuantizedPointAndTheSelection)
  {
    const std::uint16_t selection[] = {Outpost::PackIdentity(3, 1), Outpost::PackIdentity(4, 0)};
    const Outpost::Command command = Outpost::BuildMoveCommand(12, 1024.0f, -2048.0f, selection);

    Assert::AreEqual(12, static_cast<int>(command.sequence));
    Assert::IsTrue(command.type == Outpost::CommandType::MoveTo);
    Assert::AreEqual(static_cast<std::size_t>(2), command.selection.size());

    // Four wire steps to the world unit, so 1,024 units is 4,096 steps.
    Assert::AreEqual(4096, static_cast<int>(command.targetX));
    Assert::AreEqual(-8192, static_cast<int>(command.targetY));
  }

  /// R19, asserted rather than asserted-about. The whole tap path runs -- pick, order, marker --
  /// and the replica the client holds is byte-for-byte what the snapshot put there.
  TEST_METHOD(NothingInGameClientMovesAnEntity)
  {
    Outpost::Snapshot snapshot;
    snapshot.sequence = 1;
    Outpost::EntityRecord record;
    record.identity = Outpost::PackIdentity(3, 0);
    record.positionX = 100;
    record.positionY = 200;
    record.heading = 64;
    snapshot.entities.push_back(record);

    Outpost::ReplicaStore store;
    Assert::IsTrue(store.Accept(snapshot, 1000));

    const Outpost::EntityRecord before = store.Newest()->entities[0];

    // The player taps empty space with that ship selected, and the client does everything it is
    // allowed to do about it.
    Outpost::CameraPose pose;
    pose.distance = 8000.0f;
    const Outpost::TapOutcome outcome = Outpost::ResolveTap(pose, TARGET_ASPECT, 200.0f, 300.0f, 1440.0f, 960.0f, {}, true);
    Assert::IsTrue(outcome.action == Outpost::TapAction::MoveTo);

    const std::uint16_t selection[] = {record.identity};
    const Outpost::Command command = Outpost::BuildMoveCommand(1, outcome.worldX, outcome.worldY, selection);

    Outpost::OrderMarker marker;
    marker.targetX = command.targetX;
    marker.targetY = command.targetY;
    marker.commandSequence = command.sequence;
    marker.selection.assign(selection, selection + 1);

    Outpost::OrderMarkerSet markers;
    markers.Add(marker);

    // The order was sent, the marker is drawn -- and the ship has not moved a step.
    Assert::AreEqual(static_cast<std::size_t>(1), markers.Count());
    const Outpost::EntityRecord after = store.Newest()->entities[0];
    Assert::IsTrue(before == after);
  }
};

} // namespace GameClientTests
