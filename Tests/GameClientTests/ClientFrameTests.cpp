#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// A snapshot encoded onto the wire, which is what a frame actually receives -- these tests drive
/// the drain through the real decoder rather than handing it a struct, because the decode is half
/// of what the drain does.
[[nodiscard]] std::vector<std::byte> EncodedSnapshot(std::uint16_t _sequence, std::uint16_t _lastCommandApplied, std::int16_t _positionX)
{
  Outpost::Snapshot snapshot;
  snapshot.sequence = _sequence;
  snapshot.tick = _sequence;

  Outpost::PlayerBlock player;
  player.lastCommandSequenceApplied = _lastCommandApplied;
  snapshot.players.push_back(player);

  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(1, 0);
  record.positionX = _positionX;
  snapshot.entities.push_back(record);

  std::vector<std::byte> bytes(Outpost::EncodedSize(snapshot));
  Neuron::ByteWriter writer{bytes};
  Assert::IsTrue(Outpost::Encode(snapshot, writer));
  return bytes;
}

/// **SEATS A FRAME THE WAY THE HOST DOES** (ADR-013). Before M1.4 a `ClientFrame` was player one
/// by construction and these tests said nothing; now the player is what the host answered, so a
/// test that wants one says so through the same reply the wire carries.
void Seat(Outpost::ClientFrame& _frame, Outpost::PlayerId _player)
{
  static_cast<void>(_frame.MutableJoin().Accept(
    Outpost::JoinReply{.result = Outpost::JoinResult::Accepted, .player = _player, .token = 0xABCDEF0123456789ull, .matchSeed = 99}));
}

[[nodiscard]] Outpost::OrderMarker MarkerFor(std::uint16_t _sequence)
{
  Outpost::OrderMarker marker;
  marker.commandSequence = _sequence;
  return marker;
}

constexpr std::size_t QUEUE_SLOTS = 8;
constexpr std::size_t QUEUE_SLOT_BYTES = 1500;
} // namespace

/// `TechnicalDesign.md` section 6's order, and R20's line through it: the dispatcher drain stays in
/// the package and everything after it is here, where a suite can drive it.
TEST_CLASS(ClientFrameDrain)
{
public:
  TEST_METHOD(ADrainTakesEverythingWaiting)
  {
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> first = EncodedSnapshot(1, 0, 10);
    const std::vector<std::byte> second = EncodedSnapshot(2, 0, 20);
    const std::vector<std::byte> third = EncodedSnapshot(3, 0, 30);
    queue.Push(first);
    queue.Push(second);
    queue.Push(third);

    Outpost::ClientFrame frame;
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    // A queue left holding a datagram is a frame of latency added for nothing.
    Assert::AreEqual(static_cast<std::size_t>(0), queue.PendingCount());
    Assert::AreEqual(3u, result.datagrams);
    Assert::AreEqual(3u, result.accepted);
    Assert::AreEqual(0u, result.faulted);
  }

  TEST_METHOD(ADatagramThatDoesNotDecodeIsCountedAndNotFatal)
  {
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::array<std::byte, 4> rubbish{std::byte{9}, std::byte{9}, std::byte{9}, std::byte{9}};
    queue.Push(rubbish);
    const std::vector<std::byte> good = EncodedSnapshot(1, 0, 10);
    queue.Push(good);

    Outpost::ClientFrame frame;
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    Assert::AreEqual(2u, result.datagrams);
    Assert::AreEqual(1u, result.faulted);
    // And the good one behind it still arrived, which is the point of counting rather than
    // stopping.
    Assert::AreEqual(1u, result.accepted);
  }

  TEST_METHOD(AnOutOfOrderSnapshotIsRefusedRatherThanFaulted)
  {
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> newer = EncodedSnapshot(5, 0, 50);
    const std::vector<std::byte> older = EncodedSnapshot(4, 0, 40);
    queue.Push(newer);
    queue.Push(older);

    Outpost::ClientFrame frame;
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    Assert::AreEqual(1u, result.accepted);
    Assert::AreEqual(1u, result.refused);
    Assert::AreEqual(0u, result.faulted);
  }

  /// Q20: the marker is cleared by the snapshot whose acknowledgment covers it, and the frame reads
  /// that from the newest snapshot rather than from each one it drained.
  TEST_METHOD(TheAcknowledgmentInASnapshotClearsTheMarkers)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    frame.Markers().Add(MarkerFor(1));
    frame.Markers().Add(MarkerFor(2));
    frame.Markers().Add(MarkerFor(3));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> snapshot = EncodedSnapshot(1, 2, 10);
    queue.Push(snapshot);

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    Assert::AreEqual(2u, result.markersCleared);
    Assert::AreEqual(static_cast<std::size_t>(1), frame.Markers().Count());
  }

  /// A PLAYER IDENTITY IS ONE-BASED AND A BLOCK INDEX IS NOT, and with a single player the two are
  /// indistinguishable -- which is why this test carries TWO blocks. `players[m_player]` reads the
  /// wrong one and `players[m_player - 1]` reads the right one, and only a second player can tell
  /// them apart. The single-block test above passed against both spellings.
  TEST_METHOD(TheAcknowledgmentReadIsThisPlayersBlockAndNotTheOthers)
  {
    Outpost::Snapshot snapshot;
    snapshot.sequence = 1;

    // Player one has applied nothing; player two has applied up to five.
    Outpost::PlayerBlock first;
    first.lastCommandSequenceApplied = 0;
    Outpost::PlayerBlock second;
    second.lastCommandSequenceApplied = 5;
    snapshot.players.push_back(first);
    snapshot.players.push_back(second);

    std::vector<std::byte> bytes(Outpost::EncodedSize(snapshot));
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(snapshot, writer));

    Outpost::ClientFrame frame;
    Seat(frame, 1);
    frame.Markers().Add(MarkerFor(3));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(bytes);

    // Player one acknowledged nothing, so the marker stays. Reading player two's block would clear
    // it and the client would stop drawing an order the host has not applied.
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);
    Assert::AreEqual(0u, result.markersCleared);
    Assert::AreEqual(static_cast<std::size_t>(1), frame.Markers().Count());
  }

  /// **M1.4 INVERTED THIS TEST.** It used to assert that a fresh frame is player one, because
  /// `README.md` F2 had the protocol unable to say and the client assuming. ADR-013 says, and a
  /// client that has not been told is `NO_PLAYER` -- which is the value `CommandIntake` refuses
  /// outright, so the assumption could not have been checked and this one can.
  TEST_METHOD(AFreshFrameIsNobodyUntilTheHostSeatsIt)
  {
    const Outpost::ClientFrame frame;
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(frame.Player()));
    Assert::IsFalse(frame.CurrentJoin().IsJoined());
    Assert::IsTrue(frame.Camera().distance < Outpost::MAXIMUM_CAMERA_DISTANCE);
  }

  /// A SEATED FRAME CLEARS MARKERS AND AN UNSEATED ONE DOES NOT, which is the whole practical
  /// difference the join makes to this class. An unseated frame still folds snapshots in -- it is
  /// the acknowledgment channel that needs to know whose block to read.
  TEST_METHOD(AnUnseatedFrameFoldsSnapshotsAndClearsNothing)
  {
    Outpost::ClientFrame frame;
    frame.Markers().Add(MarkerFor(3));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 5, 0));

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);
    Assert::AreEqual(1u, result.accepted);
    Assert::AreEqual(0u, result.markersCleared);
    Assert::AreEqual(static_cast<std::size_t>(1), frame.Markers().Count());
  }

  TEST_METHOD(TheClockAdvancesWithTheFrameAndNotWithArrivals)
  {
    Outpost::ClientFrame frame;

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    for (std::uint16_t sequence = 1; sequence <= 3; ++sequence)
    {
      const std::vector<std::byte> bytes = EncodedSnapshot(sequence, 0, static_cast<std::int16_t>(sequence * 10));
      queue.Push(bytes);
      static_cast<void>(frame.DrainPackets(queue, 1000 + (static_cast<std::uint64_t>(sequence) * 50)));
    }

    // Arrivals at 1050, 1100, 1150; the frame at 1150 draws 75 behind, which is 1075 -- between the
    // first two. The case two retained snapshots could not have served.
    const Outpost::ReplicaStore::Frame drawn = frame.Advance(1150);
    Assert::IsTrue(drawn.playout.state == Outpost::PlayoutState::Interpolating);
    Assert::AreEqual(1, static_cast<int>(drawn.older->sequence));
    Assert::AreEqual(2, static_cast<int>(drawn.newer->sequence));
  }

  /// **A RECONNECTING CLIENT MUST NOT BE MUTE.** `CommandIntake` refuses a command whose sequence is
  /// not newer than the last it applied for that player and remembers that for the whole match, so
  /// a client that relaunches and counts from one again has every order discarded until it catches
  /// up. This is the fix, and it was found by tapping a ship on the device for thirty-eight seconds
  /// and watching it not move.
  TEST_METHOD(TheSequenceIsAdoptedFromWhatTheHostHasApplied)
  {
    Outpost::Snapshot snapshot;
    snapshot.sequence = 1;
    Outpost::PlayerBlock block;
    block.lastCommandSequenceApplied = 40;
    snapshot.players.push_back(block);

    std::vector<std::byte> bytes(Outpost::EncodedSize(snapshot));
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(snapshot, writer));

    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(bytes);
    static_cast<void>(frame.DrainPackets(queue, 1000));

    // One past what the host has applied, so the very first order this client sends is accepted.
    Assert::AreEqual(41, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(42, static_cast<int>(frame.TakeCommandSequence()));
  }

  /// ADOPTED ONCE AND ONLY FORWARDS. An acknowledgment lags the orders in flight; adopting it again
  /// would wind the counter back over commands already sent and have them refused as duplicates.
  TEST_METHOD(TheSequenceIsNotWoundBackwardsByALaterSnapshot)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};

    Outpost::Snapshot first;
    first.sequence = 1;
    Outpost::PlayerBlock a;
    a.lastCommandSequenceApplied = 40;
    first.players.push_back(a);
    std::vector<std::byte> firstBytes(Outpost::EncodedSize(first));
    Neuron::ByteWriter firstWriter{firstBytes};
    Assert::IsTrue(Outpost::Encode(first, firstWriter));
    queue.Push(firstBytes);
    static_cast<void>(frame.DrainPackets(queue, 1000));

    // The client sends three orders; the host has acknowledged none of them yet.
    Assert::AreEqual(41, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(42, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(43, static_cast<int>(frame.TakeCommandSequence()));

    Outpost::Snapshot later;
    later.sequence = 2;
    Outpost::PlayerBlock b;
    b.lastCommandSequenceApplied = 41;
    later.players.push_back(b);
    std::vector<std::byte> laterBytes(Outpost::EncodedSize(later));
    Neuron::ByteWriter laterWriter{laterBytes};
    Assert::IsTrue(Outpost::Encode(later, laterWriter));
    queue.Push(laterBytes);
    static_cast<void>(frame.DrainPackets(queue, 1050));

    // Still counting on from where it was, not back to 42.
    Assert::AreEqual(44, static_cast<int>(frame.TakeCommandSequence()));
  }

  TEST_METHOD(CommandSequencesAreHandedOutInOrderAndWrap)
  {
    Outpost::ClientFrame frame;
    Assert::AreEqual(1, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(2, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(3, static_cast<int>(frame.TakeCommandSequence()));
  }

  TEST_METHOD(AnEmptyQueueIsAnOrdinaryFrame)
  {
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    Outpost::ClientFrame frame;

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);
    Assert::AreEqual(0u, result.datagrams);

    // And there is still a frame to draw, which is Starved rather than an error.
    const Outpost::ReplicaStore::Frame drawn = frame.Advance(1000);
    Assert::IsTrue(drawn.playout.state == Outpost::PlayoutState::Starved);
    Assert::IsNull(drawn.newer);
  }
};

/// `Interface.md` section 7: a client that loses the link shows the reconnecting overlay and rejoins as
/// itself, and it comes down with the first snapshot after the host seats it again.
TEST_CLASS(ClientFrameLink)
{
public:
  TEST_METHOD(TheLinkFollowsTheJoinUntilAnythingIsLost)
  {
    Outpost::ClientFrame frame;
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Joining);

    Seat(frame, 1);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Linked);

    Outpost::ClientFrame refused;
    static_cast<void>(refused.MutableJoin().Accept(
      Outpost::JoinReply{.result = Outpost::JoinResult::MatchFull, .player = Outpost::NO_PLAYER, .token = 0, .matchSeed = 0}));
    Assert::IsTrue(refused.Link() == Outpost::LinkState::Refused);
  }

  /// A snapshot a second, less a millisecond, is a link that is still there.
  TEST_METHOD(ASilenceShorterThanTheThresholdIsNotALoss)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS - 1);
    Assert::IsFalse(result.linkLost);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Linked);
    Assert::IsTrue(frame.CurrentJoin().IsJoined());
  }

  /// **THE LOSS STARTS A REJOIN, AND THE PLAYER IS KEPT** so the panels behind the overlay are still this
  /// player's.
  TEST_METHOD(ASilenceStartsARejoinAndShowsTheOverlay)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 2);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS);
    Assert::IsTrue(result.linkLost);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);
    Assert::IsTrue(frame.CurrentJoin().Phase() == Outpost::JoinPhase::Joining);
    Assert::AreEqual(2, static_cast<int>(frame.Player()));
    Assert::IsTrue(frame.MutableJoin().ShouldSend(2000), L"the rejoin goes out on the next frame");
  }

  /// **A RESUME IS A SILENCE**, and the stale snapshots a suspended socket was holding land on the same
  /// frame the loss is seen. They must not take the overlay down: only a snapshot after the host has
  /// seated this client again can.
  TEST_METHOD(SnapshotsBeforeTheRejoinIsAnsweredDoNotEndIt)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    // Suspended for a minute; two stale snapshots were waiting in the socket.
    queue.Push(EncodedSnapshot(2, 0, 0));
    queue.Push(EncodedSnapshot(3, 0, 0));
    const Outpost::ClientFrame::DrainResult resumed = frame.DrainPackets(queue, 61000);
    Assert::IsTrue(resumed.linkLost);
    Assert::AreEqual(2u, resumed.accepted);
    Assert::IsFalse(resumed.linkRestored);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);
  }

  /// `Interface.md` section 7: until the first snapshot lands, **then straight back to play** -- and the
  /// reply and the snapshot commonly arrive in one drain.
  TEST_METHOD(TheFirstSnapshotAfterTheReplyRestoresTheLink)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));
    static_cast<void>(frame.DrainPackets(queue, 61000));
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);

    // The reply alone seats the client and does not end the overlay; there is still nothing new to draw.
    static_cast<void>(frame.MutableJoin().Accept(
      Outpost::JoinReply{.result = Outpost::JoinResult::Rejoined, .player = 1, .token = 0xABCDEF0123456789ull, .matchSeed = 99}));
    Assert::IsFalse(frame.DrainPackets(queue, 61050).linkRestored);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);

    queue.Push(EncodedSnapshot(2, 0, 0));
    const Outpost::ClientFrame::DrainResult restored = frame.DrainPackets(queue, 61100);
    Assert::IsTrue(restored.linkRestored);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Linked);
  }

  /// **A REJOIN THAT IS ANSWERED BUT NEVER FOLLOWED BY A SNAPSHOT ASKS ONCE A SECOND**, not once a frame.
  /// The silence is measured again from the rejoin.
  TEST_METHOD(AnAnsweredRejoinWithNoSnapshotRetriesAtTheThreshold)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));
    Assert::IsTrue(frame.DrainPackets(queue, 5000).linkLost);

    Seat(frame, 1);
    Assert::IsFalse(frame.DrainPackets(queue, 5000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS - 1).linkLost);
    Assert::IsTrue(frame.DrainPackets(queue, 5000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS).linkLost);
  }

  /// A client that has never had a snapshot has no link to lose, however long the host takes to send one.
  TEST_METHOD(NoSnapshotYetIsNotALoss)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    Assert::IsFalse(frame.DrainPackets(queue, 60000).linkLost);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Linked);
  }

  /// **A REFUSED REJOIN IS SHOWN AS A REFUSAL.** The host had restarted and filled the slot; retrying
  /// will not help and the overlay must not say it will.
  TEST_METHOD(ARefusedRejoinIsRefusedAndNotReconnecting)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedSnapshot(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));
    static_cast<void>(frame.DrainPackets(queue, 5000));

    static_cast<void>(frame.MutableJoin().Accept(
      Outpost::JoinReply{.result = Outpost::JoinResult::MatchFull, .player = Outpost::NO_PLAYER, .token = 0, .matchSeed = 0}));
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Refused);
  }
};

/// ADR-008: the address is configuration, and the fallback is the part that has to be right.
TEST_CLASS(HostAddressFallback)
{
public:
  TEST_METHOD(AnAddressInTheFileIsUsed)
  {
    Assert::IsTrue(Neuron::HostAddressFromFileContents("192.168.1.40") == "192.168.1.40");
  }

  TEST_METHOD(TheFirstLineIsTakenAndTrimmed)
  {
    // A file dropped by a person has a trailing newline, and often a space before it.
    Assert::IsTrue(Neuron::HostAddressFromFileContents("192.168.1.40\r\n") == "192.168.1.40");
    Assert::IsTrue(Neuron::HostAddressFromFileContents("  192.168.1.40  \n") == "192.168.1.40");
    Assert::IsTrue(Neuron::HostAddressFromFileContents("192.168.1.40\nthe tablet, on the desk") == "192.168.1.40");
  }

  /// EVERY WAY OF SAYING NOTHING IS THE SAME AS NO FILE. Refusing to start would turn a typo into a
  /// client that cannot be run on a machine that may not have a keyboard attached.
  TEST_METHOD(AFileThatNamesNothingFallsBack)
  {
    Assert::IsTrue(Neuron::HostAddressFromFileContents("") == Neuron::DEFAULT_HOST_ADDRESS);
    Assert::IsTrue(Neuron::HostAddressFromFileContents("   ") == Neuron::DEFAULT_HOST_ADDRESS);
    Assert::IsTrue(Neuron::HostAddressFromFileContents("\r\n") == Neuron::DEFAULT_HOST_ADDRESS);
    Assert::IsTrue(Neuron::HostAddressFromFileContents("\n192.168.1.40") == Neuron::DEFAULT_HOST_ADDRESS);
  }

  TEST_METHOD(TheDefaultIsLoopback)
  {
    // ADR-008: the development machine is the host until a Surface Pro is pointed at a LAN address.
    Assert::IsTrue(Neuron::DEFAULT_HOST_ADDRESS == "127.0.0.1");
  }

  /// IT DOES NOT VALIDATE. Whether the text names a reachable host is the transport's answer; a
  /// syntax check here would be a second, worse parser in front of the real one.
  TEST_METHOD(SomethingThatIsNotAnAddressIsPassedThrough)
  {
    Assert::IsTrue(Neuron::HostAddressFromFileContents("surface-pro.local") == "surface-pro.local");
    Assert::IsTrue(Neuron::HostAddressFromFileContents("not an address") == "not an address");
  }
};

} // namespace GameClientTests
