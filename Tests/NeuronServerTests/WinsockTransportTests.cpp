#include "pch.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{

namespace
{
/// Comfortably larger than anything this suite sends, and deliberately not any figure the design
/// states -- ADR-003's payload is a decision about the wire and this is a scratch buffer.
inline constexpr std::size_t SCRATCH_BYTES = 2048;

/// Loopback delivery is fast but it is not synchronous with sendto, so a receive that ran exactly
/// once would flake on a loaded runner. Poll to a deadline and give up rather than block: a suite
/// that hangs is worse than one that fails, because CI reports the second and waits on the first.
inline constexpr std::uint32_t POLL_ATTEMPTS = 2000;
inline constexpr std::chrono::milliseconds POLL_INTERVAL{1};

[[nodiscard]] std::uint8_t Code(Neuron::ReceiveOutcome _outcome) noexcept
{
  return static_cast<std::uint8_t>(_outcome);
}

[[nodiscard]] Neuron::ReceiveOutcome ReceiveWithin(Neuron::WinsockTransport& _transport, std::span<std::byte> _buffer,
                                                   std::size_t& _outBytes, Neuron::Endpoint& _outSender)
{
  for (std::uint32_t attempt = 0; attempt < POLL_ATTEMPTS; ++attempt)
  {
    const Neuron::ReceiveOutcome outcome = _transport.Receive(_buffer, _outBytes, _outSender);
    if (outcome != Neuron::ReceiveOutcome::Empty)
    {
      return outcome;
    }
    std::this_thread::sleep_for(POLL_INTERVAL);
  }
  return Neuron::ReceiveOutcome::Empty;
}

/// An endpoint on this machine, which is where every peer in this suite lives.
[[nodiscard]] Neuron::Endpoint Loopback(std::uint16_t _port) noexcept
{
  return Neuron::Endpoint{.addressV4 = Neuron::LOOPBACK_ADDRESS_V4, .port = _port};
}
} // namespace

TEST_CLASS(WinsockTransportLifetime)
{
public:
  TEST_METHOD(OpeningOnPortZeroBindsAnEphemeralPortAndReportsIt)
  {
    Neuron::WinsockTransport transport;

    Assert::IsTrue(transport.Open(0), L"Open(0) failed");
    Assert::IsTrue(transport.IsOpen());
    Assert::AreNotEqual(std::uint16_t{0}, transport.BoundEndpoint().port, L"an ephemeral port was not reported back");
    Assert::AreEqual(0, transport.LastFault());
  }

  TEST_METHOD(ASecondOpenOnAnOpenTransportIsRefused)
  {
    Neuron::WinsockTransport transport;
    Assert::IsTrue(transport.Open(0));

    Assert::IsFalse(transport.Open(0), L"a second Open would leak the first socket and Winsock's refcount");
  }

  TEST_METHOD(CloseIsSafeTwiceAndOnATransportThatNeverOpened)
  {
    Neuron::WinsockTransport never;
    never.Close();
    never.Close();
    Assert::IsFalse(never.IsOpen());

    Neuron::WinsockTransport opened;
    Assert::IsTrue(opened.Open(0));
    opened.Close();
    opened.Close();
    Assert::IsFalse(opened.IsOpen());
    Assert::AreEqual(std::uint16_t{0}, opened.BoundEndpoint().port);
  }

  TEST_METHOD(AClosedTransportCanBeOpenedAgain)
  {
    Neuron::WinsockTransport transport;
    Assert::IsTrue(transport.Open(0));
    transport.Close();

    Assert::IsTrue(transport.Open(0), L"WSAStartup and WSACleanup are not balanced across a reopen");
  }

  /// The destructor is what releases the port, and the proof is that the port can be taken again.
  /// UDP has no TIME_WAIT, so an immediate rebind is the honest test of it.
  TEST_METHOD(TheDestructorReleasesThePort)
  {
    std::uint16_t port = 0;
    {
      Neuron::WinsockTransport transport;
      Assert::IsTrue(transport.Open(0));
      port = transport.BoundEndpoint().port;
    }

    Neuron::WinsockTransport again;
    Assert::IsTrue(again.Open(port), L"the port was still held after the owner was destroyed");
    Assert::AreEqual(port, again.BoundEndpoint().port);
  }

  TEST_METHOD(AClosedTransportNeitherSendsNorReceives)
  {
    constexpr std::array<std::byte, 4> PAYLOAD{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    Neuron::WinsockTransport transport;

    Assert::IsFalse(transport.Send(Loopback(9), PAYLOAD));

    std::array<std::byte, SCRATCH_BYTES> buffer{};
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Failed), Code(transport.Receive(buffer, byteCount, sender)));
  }
};

TEST_CLASS(WinsockTransportDatagrams)
{
public:
  /// The first two of TechnicalDesign.md section 8's four, against a peer this test stands up
  /// itself: a datagram goes out of one socket and comes out of another, unchanged.
  TEST_METHOD(ADatagramCrossesLoopbackUnchanged)
  {
    constexpr std::array<std::byte, 5> PAYLOAD{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}, std::byte{0x01}};

    Neuron::WinsockTransport host;
    Neuron::WinsockTransport peer;
    Assert::IsTrue(host.Open(0));
    Assert::IsTrue(peer.Open(0));

    Assert::IsTrue(peer.Send(Loopback(host.BoundEndpoint().port), PAYLOAD));

    std::array<std::byte, SCRATCH_BYTES> buffer{};
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(host, buffer, byteCount, sender)));

    Assert::AreEqual(PAYLOAD.size(), byteCount);
    for (std::size_t index = 0; index < PAYLOAD.size(); ++index)
    {
      Assert::AreEqual(std::to_integer<std::uint8_t>(PAYLOAD[index]), std::to_integer<std::uint8_t>(buffer[index]));
    }
  }

  /// The host answers whoever sent to it, so the reported sender has to be an address it can send
  /// back to. This is the property the whole host loop rests on, and it is one conversion away
  /// from being silently wrong in network byte order.
  TEST_METHOD(TheReportedSenderCanBeRepliedTo)
  {
    constexpr std::array<std::byte, 1> QUESTION{std::byte{0x11}};
    constexpr std::array<std::byte, 1> ANSWER{std::byte{0x22}};

    Neuron::WinsockTransport host;
    Neuron::WinsockTransport peer;
    Assert::IsTrue(host.Open(0));
    Assert::IsTrue(peer.Open(0));
    Assert::IsTrue(peer.Send(Loopback(host.BoundEndpoint().port), QUESTION));

    std::array<std::byte, SCRATCH_BYTES> buffer{};
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(host, buffer, byteCount, sender)));
    Assert::AreEqual(Neuron::LOOPBACK_ADDRESS_V4, sender.addressV4);
    Assert::AreEqual(peer.BoundEndpoint().port, sender.port);

    Assert::IsTrue(host.Send(sender, ANSWER));

    std::size_t replyBytes = 0;
    Neuron::Endpoint replySender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(peer, buffer, replyBytes, replySender)));
    Assert::AreEqual(static_cast<std::size_t>(1), replyBytes);
    Assert::AreEqual(std::uint8_t{0x22}, std::to_integer<std::uint8_t>(buffer[0]));
    Assert::AreEqual(host.BoundEndpoint().port, replySender.port);
  }

  /// The third of section 8's four. recvfrom returns the DATAGRAM's length; reporting the
  /// buffer's would make every snapshot decode read 2,048 bytes of whatever was there last.
  TEST_METHOD(AShortReadReportsTheDatagramLengthNotTheBufferLength)
  {
    constexpr std::array<std::byte, 4> PAYLOAD{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};

    Neuron::WinsockTransport host;
    Neuron::WinsockTransport peer;
    Assert::IsTrue(host.Open(0));
    Assert::IsTrue(peer.Open(0));
    Assert::IsTrue(peer.Send(Loopback(host.BoundEndpoint().port), PAYLOAD));

    std::array<std::byte, SCRATCH_BYTES> buffer{};
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(host, buffer, byteCount, sender)));

    Assert::AreEqual(static_cast<std::size_t>(4), byteCount);
    Assert::AreNotEqual(SCRATCH_BYTES, byteCount);
  }

  /// The fourth. An oversized datagram must be REPORTED rather than handed back truncated -- and
  /// it must be gone, so that the next drain gets the next datagram. A receive that left it
  /// queued would wedge the host's loop on it forever, twenty times a second.
  TEST_METHOD(ADatagramLargerThanTheBufferIsReportedAndConsumed)
  {
    std::array<std::byte, 64> oversized{};
    oversized.fill(std::byte{0xAB});
    constexpr std::array<std::byte, 2> NEXT_ONE{std::byte{0x5A}, std::byte{0x5B}};

    Neuron::WinsockTransport host;
    Neuron::WinsockTransport peer;
    Assert::IsTrue(host.Open(0));
    Assert::IsTrue(peer.Open(0));
    const Neuron::Endpoint hostEndpoint = Loopback(host.BoundEndpoint().port);

    Assert::IsTrue(peer.Send(hostEndpoint, oversized));

    std::array<std::byte, 16> narrowBuffer{};
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Oversized), Code(ReceiveWithin(host, narrowBuffer, byteCount, sender)));

    Assert::IsTrue(peer.Send(hostEndpoint, NEXT_ONE));
    std::array<std::byte, SCRATCH_BYTES> buffer{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(host, buffer, byteCount, sender)));
    Assert::AreEqual(static_cast<std::size_t>(2), byteCount);
    Assert::AreEqual(std::uint8_t{0x5A}, std::to_integer<std::uint8_t>(buffer[0]));
  }

  /// A zero-length datagram is legal UDP and cannot carry even a PacketHeader, so it is refused
  /// here rather than sent and dropped at the far end.
  TEST_METHOD(AnEmptyDatagramIsRefused)
  {
    Neuron::WinsockTransport transport;
    Assert::IsTrue(transport.Open(0));

    Assert::IsFalse(transport.Send(Loopback(transport.BoundEndpoint().port), std::span<const std::byte>{}));
  }

  /// The socket is non-blocking, which is what lets one thread drain it at the top of a tick and
  /// carry on. A BLOCKING socket does not fail this test -- it hangs it, and vstest reports the
  /// timeout. That is the observable difference and it is why this is the only honest assertion.
  TEST_METHOD(AnEmptySocketReturnsEmptyRatherThanBlocking)
  {
    Neuron::WinsockTransport transport;
    Assert::IsTrue(transport.Open(0));

    std::array<std::byte, SCRATCH_BYTES> buffer{};
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Empty), Code(transport.Receive(buffer, byteCount, sender)));
    Assert::AreEqual(0, transport.LastFault(), L"an empty queue is not a fault and must not be recorded as one");
  }

  /// Two datagrams in flight before either is drained: the drain has to be a loop, and each pass
  /// has to produce one whole datagram rather than a concatenation. UDP preserves boundaries and
  /// this is what proves the transport does not undo that.
  TEST_METHOD(DatagramBoundariesSurviveADrain)
  {
    constexpr std::array<std::byte, 3> FIRST{std::byte{1}, std::byte{1}, std::byte{1}};
    constexpr std::array<std::byte, 7> SECOND{std::byte{2}, std::byte{2}, std::byte{2}, std::byte{2},
                                              std::byte{2}, std::byte{2}, std::byte{2}};

    Neuron::WinsockTransport host;
    Neuron::WinsockTransport peer;
    Assert::IsTrue(host.Open(0));
    Assert::IsTrue(peer.Open(0));
    const Neuron::Endpoint hostEndpoint = Loopback(host.BoundEndpoint().port);
    Assert::IsTrue(peer.Send(hostEndpoint, FIRST));
    Assert::IsTrue(peer.Send(hostEndpoint, SECOND));

    std::array<std::byte, SCRATCH_BYTES> buffer{};
    std::size_t firstBytes = 0;
    std::size_t secondBytes = 0;
    Neuron::Endpoint sender{};
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(host, buffer, firstBytes, sender)));
    Assert::AreEqual(Code(Neuron::ReceiveOutcome::Received), Code(ReceiveWithin(host, buffer, secondBytes, sender)));

    Assert::AreEqual(static_cast<std::size_t>(3), firstBytes);
    Assert::AreEqual(static_cast<std::size_t>(7), secondBytes);
  }
};

} // namespace NeuronServerTests
