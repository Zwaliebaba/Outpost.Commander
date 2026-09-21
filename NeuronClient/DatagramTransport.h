#pragma once

#include "PacketQueue.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace Neuron
{

/// The socket, its event registration and the buffer the pool thread copies through. It is
/// declared here and DEFINED IN THE .cpp, so that no C++/WinRT projection header reaches this
/// library's master include -- and therefore none reaches GameClient, the package, or the two
/// DESKTOP test DLLs that link this library's Windows Store build (AGENTS.md section 3).
struct SocketBinding;

enum class TransportState : std::uint8_t
{
  Closed,
  /// ConnectAsync is in flight. The frame carries on drawing; there is nothing to draw yet.
  Opening,
  Ready,
  Failed
};

/// The client's UDP endpoint, over Windows::Networking::Sockets::DatagramSocket -- the Windows
/// SDK's own projection, and therefore inside R14's closed list.
///
/// THE THREADING SEAM IS THE POINT OF THIS CLASS. DatagramSocket delivers MessageReceived on a
/// thread pool thread, not on the frame's thread (TechnicalDesign.md section 5). The handler here
/// does exactly one thing: copy the datagram's bytes into the PacketQueue and return. It parses
/// nothing, allocates nothing and touches no renderer or replica state, and the buffer it copies
/// through was allocated when the transport opened.
///
/// Open, Close, Send and State belong to the frame thread and are not synchronized against each
/// other. Only the delivery path crosses threads, and it is the only thing that takes a lock.
///
/// The PacketQueue is the caller's and must outlive this transport.
///
/// NONE OF THIS IS UNIT-TESTED AND IT CANNOT HONESTLY BE: nothing in this tree can stand up a
/// DatagramSocket inside a desktop test host and prove the pool thread did the right thing.
/// M0.5's gate is what proves it, on two machines and then on one, and saying so here is better
/// than a test that pretends. PacketQueue -- which is where the thread-crossing actually lives --
/// is pinned by NeuronClientTests in full.
class DatagramTransport
{
public:
  explicit DatagramTransport(PacketQueue& _queue) noexcept;
  ~DatagramTransport() noexcept;

  DatagramTransport(const DatagramTransport&) = delete;
  DatagramTransport& operator=(const DatagramTransport&) = delete;
  DatagramTransport(DatagramTransport&&) = delete;
  DatagramTransport& operator=(DatagramTransport&&) = delete;

  /// Starts connecting to the host and RETURNS IMMEDIATELY, before the socket is usable. A UWP
  /// socket's connect is asynchronous and the frame thread is an ASTA, where blocking on an
  /// asynchronous operation is a deadlock rather than a delay. Poll State() instead.
  ///
  /// False only when the transport is already open or the socket could not be created at all.
  [[nodiscard]] bool Open(std::string_view _hostAddress, std::uint16_t _port) noexcept;

  /// Revokes the handler, closes the socket and stops delivery. A datagram already inside the
  /// handler finishes before this returns; one arriving after it is dropped. Safe to call twice.
  void Close() noexcept;

  [[nodiscard]] TransportState State() const noexcept;

  /// One datagram to the host. False when the transport is not Ready, when _bytes is empty, or
  /// when the previous send has not completed -- commands are repeated in every outgoing packet
  /// until the snapshot acknowledges them (ADR-003), so a skipped send costs one repeat and is
  /// counted rather than queued.
  [[nodiscard]] bool Send(std::span<const std::byte> _bytes) noexcept;

  /// Sends refused because the previous one was still in flight.
  [[nodiscard]] std::uint64_t SkippedSendCount() const noexcept;

  /// Datagrams that arrived larger than a queue slot and could not be copied anywhere.
  [[nodiscard]] std::uint64_t OversizedCount() const noexcept;

private:
  PacketQueue& m_queue;
  std::shared_ptr<SocketBinding> m_binding;
};

} // namespace Neuron
