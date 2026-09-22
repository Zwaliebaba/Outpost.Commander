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
