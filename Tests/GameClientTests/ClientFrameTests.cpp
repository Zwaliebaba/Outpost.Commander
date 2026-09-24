#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// An update encoded onto the wire, which is what a frame actually receives -- these tests drive the
/// drain through the real decoder rather than handing it a struct, because the decode is half of what
/// the drain does. One entity, at a tick that doubles as the sequence, and this client's own block.
[[nodiscard]] std::vector<std::byte> EncodedUpdate(std::uint16_t _tick, std::uint16_t _lastCommandApplied, std::int16_t _positionX)
{
  Outpost::Update update;
  update.sequence = _tick;
  update.tick = _tick;
  update.liveEntityCount = 1;
  update.own.lastCommandSequenceApplied = _lastCommandApplied;

  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(1, 1);
  record.positionX = _positionX;
  update.records.push_back(record);

  std::vector<std::byte> bytes(Outpost::EncodedSize(update));
  Neuron::ByteWriter writer{bytes};
  Assert::IsTrue(Outpost::Encode(update, writer));
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

/// A station order, which carries no selection -- the smallest command there is, for the resend tests.
[[nodiscard]] Outpost::Command StationOrder(std::uint16_t _sequence, Outpost::CommandType _type)
{
  return Outpost::Command{.sequence = _sequence, .type = _type, .targetX = 0, .targetY = 0, .selection = {}};
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
    const std::vector<std::byte> first = EncodedUpdate(1, 0, 10);
    const std::vector<std::byte> second = EncodedUpdate(2, 0, 20);
    const std::vector<std::byte> third = EncodedUpdate(3, 0, 30);
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
    const std::vector<std::byte> good = EncodedUpdate(1, 0, 10);
    queue.Push(good);

    Outpost::ClientFrame frame;
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    Assert::AreEqual(2u, result.datagrams);
    Assert::AreEqual(1u, result.faulted);
    // And the good one behind it still arrived, which is the point of counting rather than
    // stopping.
    Assert::AreEqual(1u, result.accepted);
  }

  /// **AN OUT-OF-ORDER UPDATE IS STILL HEARD; ITS STALE RECORD IS NOT** (ADR-024). Ordering is per entity,
  /// by tick, so the late update is folded in and the record it carries is refused.
  TEST_METHOD(AnOutOfOrderUpdateIsFoldedButItsStaleRecordIsRefused)
  {
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> newer = EncodedUpdate(5, 0, 50);
    const std::vector<std::byte> older = EncodedUpdate(4, 0, 40);
    queue.Push(newer);
    queue.Push(older);

    Outpost::ClientFrame frame;
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    Assert::AreEqual(2u, result.accepted);
    Assert::AreEqual(1u, result.refused);
    Assert::AreEqual(0u, result.faulted);
    Assert::AreEqual(std::int16_t{50}, frame.Replicas().Entities()[0].positionX, L"the stale record moved the entity");
  }

  /// Q20: the marker is cleared by the update whose acknowledgment covers it, and the frame reads that
  /// from the newest update rather than from each one it drained.
  TEST_METHOD(TheAcknowledgmentInAnUpdateClearsTheMarkers)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    frame.Markers().Add(MarkerFor(1));
    frame.Markers().Add(MarkerFor(2));
    frame.Markers().Add(MarkerFor(3));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> update = EncodedUpdate(1, 2, 10);
    queue.Push(update);

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);

    Assert::AreEqual(2u, result.markersCleared);
    Assert::AreEqual(static_cast<std::size_t>(1), frame.Markers().Count());
  }

  /// **THE ACKNOWLEDGMENT FOLLOWS THE NEWEST TICK.** An update carries this client's own block and nobody
  /// else's (ADR-024), so the old danger -- reading another player's block -- is gone with the list. The
  /// one that remains is a late update winding the acknowledgment back, and this is it: the late one
  /// acknowledges nothing and must not stop the newer one's clearing.
  TEST_METHOD(TheAcknowledgmentIsTheNewestTicksAndNotTheLatestArrivals)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    frame.Markers().Add(MarkerFor(3));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(9, 5, 0));
    queue.Push(EncodedUpdate(8, 0, 0));

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000);
    Assert::AreEqual(1u, result.markersCleared);
    Assert::AreEqual(static_cast<std::size_t>(0), frame.Markers().Count());
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
  /// difference the join makes to this class. An unseated frame still folds updates in.
  TEST_METHOD(AnUnseatedFrameFoldsUpdatesAndClearsNothing)
  {
    Outpost::ClientFrame frame;
    frame.Markers().Add(MarkerFor(3));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 5, 0));

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
      const std::vector<std::byte> bytes = EncodedUpdate(sequence, 0, static_cast<std::int16_t>(sequence * 10));
      queue.Push(bytes);
      static_cast<void>(frame.DrainPackets(queue, 1000 + (static_cast<std::uint64_t>(sequence) * 50)));
    }

    // Arrivals at 1050, 1100, 1150; the frame at 1150 draws 75 behind, which is 1075 -- halfway between
    // the entity's first two samples. The case two retained samples could not have served.
    std::vector<Outpost::EntityRecord> drawn;
    const Outpost::DrawnSummary summary = frame.Advance(1150, drawn);
    Assert::AreEqual(1u, summary.interpolating);
    Assert::AreEqual(std::int16_t{15}, drawn[0].positionX);
  }

  /// **A RECONNECTING CLIENT MUST NOT BE MUTE.** `CommandIntake` refuses a command whose sequence is
  /// not newer than the last it applied for that player and remembers that for the whole match, so
  /// a client that relaunches and counts from one again has every order discarded until it catches
  /// up. This is the fix, and it was found by tapping a ship on the device for thirty-eight seconds
  /// and watching it not move.
  TEST_METHOD(TheSequenceIsAdoptedFromWhatTheHostHasApplied)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 40, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    // One past what the host has applied, so the very first order this client sends is accepted.
    Assert::AreEqual(41, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(42, static_cast<int>(frame.TakeCommandSequence()));
  }

  /// ADOPTED ONCE AND ONLY FORWARDS. An acknowledgment lags the orders in flight; adopting it again
  /// would wind the counter back over commands already sent and have them refused as duplicates.
  TEST_METHOD(TheSequenceIsNotWoundBackwardsByALaterUpdate)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};

    queue.Push(EncodedUpdate(1, 40, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    // The client sends three orders; the host has acknowledged none of them yet.
    Assert::AreEqual(41, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(42, static_cast<int>(frame.TakeCommandSequence()));
    Assert::AreEqual(43, static_cast<int>(frame.TakeCommandSequence()));

    queue.Push(EncodedUpdate(2, 41, 0));
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

    // And there is still a frame to draw, with nothing in it rather than an error.
    std::vector<Outpost::EntityRecord> drawn;
    const Outpost::DrawnSummary summary = frame.Advance(1000, drawn);
    Assert::AreEqual(std::size_t{0}, drawn.size());
    Assert::AreEqual(0u, summary.interpolating + summary.holding + summary.starved);
  }
};

/// `Interface.md` section 7: a client that loses the link shows the reconnecting overlay and rejoins as
/// itself, and it comes down with the first update after the host seats it again.
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

  /// An update a second, less a millisecond, ago is a link that is still there.
  TEST_METHOD(ASilenceShorterThanTheThresholdIsNotALoss)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 0, 0));
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
    queue.Push(EncodedUpdate(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS);
    Assert::IsTrue(result.linkLost);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);
    Assert::IsTrue(frame.CurrentJoin().Phase() == Outpost::JoinPhase::Joining);
    Assert::AreEqual(2, static_cast<int>(frame.Player()));
    Assert::IsTrue(frame.MutableJoin().ShouldSend(2000), L"the rejoin goes out on the next frame");
  }

  /// **A RESUME IS A SILENCE**, and the stale updates a suspended socket was holding land on the same
  /// frame the loss is seen. They must not take the overlay down: only an update after the host has
  /// seated this client again can.
  TEST_METHOD(UpdatesBeforeTheRejoinIsAnsweredDoNotEndIt)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));

    // Suspended for a minute; two stale updates were waiting in the socket.
    queue.Push(EncodedUpdate(2, 0, 0));
    queue.Push(EncodedUpdate(3, 0, 0));
    const Outpost::ClientFrame::DrainResult resumed = frame.DrainPackets(queue, 61000);
    Assert::IsTrue(resumed.linkLost);
    Assert::AreEqual(2u, resumed.accepted);
    Assert::IsFalse(resumed.linkRestored);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);
  }

  /// `Interface.md` section 7: until the first update lands, **then straight back to play** -- and the
  /// reply and the update commonly arrive in one drain.
  TEST_METHOD(TheFirstUpdateAfterTheReplyRestoresTheLink)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));
    static_cast<void>(frame.DrainPackets(queue, 61000));
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);

    // The reply alone seats the client and does not end the overlay; there is still nothing new to draw.
    static_cast<void>(frame.MutableJoin().Accept(
      Outpost::JoinReply{.result = Outpost::JoinResult::Rejoined, .player = 1, .token = 0xABCDEF0123456789ull, .matchSeed = 99}));
    Assert::IsFalse(frame.DrainPackets(queue, 61050).linkRestored);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Reconnecting);

    queue.Push(EncodedUpdate(2, 0, 0));
    const Outpost::ClientFrame::DrainResult restored = frame.DrainPackets(queue, 61100);
    Assert::IsTrue(restored.linkRestored);
    Assert::IsTrue(frame.Link() == Outpost::LinkState::Linked);
  }

  /// **A REJOIN THAT IS ANSWERED BUT NEVER FOLLOWED BY AN UPDATE ASKS ONCE A SECOND**, not once a frame.
  /// The silence is measured again from the rejoin.
  TEST_METHOD(AnAnsweredRejoinWithNoUpdateRetriesAtTheThreshold)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));
    Assert::IsTrue(frame.DrainPackets(queue, 5000).linkLost);

    Seat(frame, 1);
    Assert::IsFalse(frame.DrainPackets(queue, 5000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS - 1).linkLost);
    Assert::IsTrue(frame.DrainPackets(queue, 5000 + Outpost::ClientFrame::LINK_SILENCE_MILLISECONDS).linkLost);
  }

  /// A client that has never had an update has no link to lose, however long the host takes to send one.
  TEST_METHOD(NoUpdateYetIsNotALoss)
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
    queue.Push(EncodedUpdate(1, 0, 0));
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

  /// **A LOST LINK EMPTIES THE STORE** (ADR-024). The host resets this client's accumulator when it answers
  /// the rejoin, so everything comes again within one sweep -- and whatever died meanwhile had its
  /// removals stop repeating long ago, so anything kept could be a ghost.
  TEST_METHOD(ALostLinkEmptiesTheStore)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    queue.Push(EncodedUpdate(1, 0, 0));
    static_cast<void>(frame.DrainPackets(queue, 1000));
    Assert::AreEqual(std::size_t{1}, frame.Replicas().HeldCount());

    Assert::IsTrue(frame.DrainPackets(queue, 61000).linkLost);
    Assert::AreEqual(std::size_t{0}, frame.Replicas().HeldCount());
  }
};

/// ADR-024's view report: the empty command packet a client sends on a cadence so the host's accumulator
/// knows what it is looking at when it has nothing to order.
TEST_CLASS(ClientFrameView)
{
public:
  /// Never before the host has seated this client -- the host refuses a packet from an endpoint it does
  /// not know, and a report it refuses reports nothing.
  TEST_METHOD(NoReportBeforeTheJoinIsAnswered)
  {
    Outpost::ClientFrame frame;
    Assert::IsFalse(frame.ShouldReportView(1000));
  }

  TEST_METHOD(AReportGoesOutAtTheCadenceAndNoFaster)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    Assert::IsTrue(frame.ShouldReportView(1000), L"the first report goes at once");
    Assert::IsFalse(frame.ShouldReportView(1000 + Outpost::ClientFrame::VIEW_REPORT_INTERVAL_MILLISECONDS - 1));
    Assert::IsTrue(frame.ShouldReportView(1000 + Outpost::ClientFrame::VIEW_REPORT_INTERVAL_MILLISECONDS));
  }

  /// The camera's focus on the wire's own grid, and its distance as the radius in whole world units.
  TEST_METHOD(TheViewIsTheCamerasFocusAndDistance)
  {
    Outpost::ClientFrame frame;
    frame.Camera().focusX = 1000.0f;
    frame.Camera().focusY = -250.0f;
    frame.Camera().distance = 2400.0f;

    Outpost::CommandPacket packet{};
    frame.StampView(packet);
    Assert::AreEqual(std::int16_t{4000}, packet.viewX, L"a world unit is four wire steps");
    Assert::AreEqual(std::int16_t{-1000}, packet.viewY);
    Assert::AreEqual(std::uint16_t{2400}, packet.viewRadiusUnits);
  }
};

/// **ADR-003's RELIABILITY, FOR A PERSON** (the 2026-09-23 review, M5). Until this only the Bot resent; the
/// packaged client sent each command once, so a lost datagram was a lost order.
TEST_CLASS(ClientFrameResend)
{
public:
  /// A command is held until an update's own block acknowledges it, and rides every packet until then --
  /// the view report included, which is what resends it when the player does not tap again.
  TEST_METHOD(ACommandRidesEveryPacketUntilAcknowledged)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    frame.IssueCommand(StationOrder(5, Outpost::CommandType::Build), 1000);
    frame.IssueCommand(StationOrder(6, Outpost::CommandType::CancelBuild), 1010);

    Outpost::CommandPacket report{};
    Assert::AreEqual(std::size_t{2}, frame.FillOutstanding(report));
    Assert::AreEqual(std::uint16_t{5}, report.commands[0].sequence, L"oldest first");
    Assert::AreEqual(std::uint16_t{6}, report.commands[1].sequence);

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> update = EncodedUpdate(1, 5, 0);
    queue.Push(update);
    const Outpost::ClientFrame::DrainResult result = frame.DrainPackets(queue, 1100);
    Assert::AreEqual(1u, result.commandsRetired);

    Outpost::CommandPacket next{};
    Assert::AreEqual(std::size_t{1}, frame.FillOutstanding(next));
    Assert::AreEqual(std::uint16_t{6}, next.commands[0].sequence, L"the acknowledged command was sent again");
  }

  /// **A HIGH-WATER MARK RETIRES EVERYTHING BELOW IT**, across the sixteen-bit wrap.
  TEST_METHOD(OneAcknowledgmentRetiresEveryCommandAtOrBeforeIt)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    for (const std::uint16_t sequence : {std::uint16_t{65534}, std::uint16_t{65535}, std::uint16_t{0}, std::uint16_t{1}})
    {
      frame.IssueCommand(StationOrder(sequence, Outpost::CommandType::CancelBuild), 1000);
    }

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    const std::vector<std::byte> update = EncodedUpdate(1, 0, 0);
    queue.Push(update);
    Assert::AreEqual(3u, frame.DrainPackets(queue, 1050).commandsRetired);
    Assert::AreEqual(std::size_t{1}, frame.OutstandingCommands().size());
    Assert::AreEqual(std::uint16_t{1}, frame.OutstandingCommands().front().sequence);
  }

  /// **GIVEN UP AFTER THE RESEND WINDOW, AND ITS MARKER WITH IT**, so a command that will never be acknowledged
  /// does not ride every packet forever and the interface stops drawing an order that may never have arrived.
  TEST_METHOD(ACommandUnacknowledgedPastTheWindowIsDroppedWithItsMarker)
  {
    Outpost::ClientFrame frame;
    Seat(frame, 1);
    frame.IssueCommand(StationOrder(9, Outpost::CommandType::CancelBuild), 1000);
    frame.Markers().Add(MarkerFor(9));

    Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
    Assert::AreEqual(0u, frame.DrainPackets(queue, 1000 + Outpost::ClientFrame::COMMAND_RESEND_WINDOW_MILLISECONDS - 1).commandsExpired);
    Assert::AreEqual(std::size_t{1}, frame.Markers().Count());

    Assert::AreEqual(1u, frame.DrainPackets(queue, 1000 + Outpost::ClientFrame::COMMAND_RESEND_WINDOW_MILLISECONDS).commandsExpired);
    Assert::AreEqual(std::size_t{0}, frame.OutstandingCommands().size());
    Assert::AreEqual(std::size_t{0}, frame.Markers().Count(), L"the marker of a command given up on is still drawn");
  }

  /// Nothing outstanding is an empty report, which is what a view report was before commands rode it.
  TEST_METHOD(NothingOutstandingFillsNothing)
  {
    Outpost::ClientFrame frame;
    Outpost::CommandPacket report{};
    Assert::AreEqual(std::size_t{0}, frame.FillOutstanding(report));
    Assert::IsTrue(report.commands.empty());
  }
};

} // namespace GameClientTests
