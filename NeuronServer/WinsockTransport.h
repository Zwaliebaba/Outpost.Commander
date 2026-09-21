#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Neuron
{

/// An IPv4 address and a port, BOTH IN HOST BYTE ORDER. The transport converts to network order
/// at the socket and nowhere else, so nothing above this file ever calls htons and nothing above
/// it can forget to. R8: a plain aggregate, so brace initialization reads naturally.
struct Endpoint
{
  std::uint32_t addressV4 = 0;
  std::uint16_t port = 0;

  [[nodiscard]] friend bool operator==(const Endpoint&, const Endpoint&) noexcept = default;
};

/// 127.0.0.1, in host byte order. This is what the address IS, not where the host is -- ADR-008
/// owns that, reads it from a file, and is M0.22's.
inline constexpr std::uint32_t LOOPBACK_ADDRESS_V4 = 0x7F000001u;

/// What a drain of the socket produced.
enum class ReceiveOutcome : std::uint8_t
{
  /// A datagram was copied out, and it is _outBytes long -- which is the datagram's length and
  /// not the buffer's.
  Received,
  /// Nothing was waiting. The ordinary answer on a non-blocking socket, and not a fault.
  Empty,
  /// A datagram arrived that did not fit the buffer. IT IS GONE: Windows discards the remainder
  /// of an oversized datagram, so there is nothing to retry and nothing partial worth keeping.
  Oversized,
  /// The socket is in trouble. LastFault() carries the WSAGetLastError code.
  Failed
};

/// The host's UDP endpoint: one non-blocking socket, drained at the top of a tick and written at
/// the end of it, on one thread. At four clients and 20 Hz there is no reason for a second
/// (TechnicalDesign.md section 5).
///
/// THERE IS DELIBERATELY NO SHARED INTERFACE WITH THE CLIENT'S DatagramTransport. The two never
/// link into one binary -- this side is a desktop build and that one is compiled for the Windows
/// Store app family (AGENTS.md section 2) -- and R2 is explicit that a base class for one derived
/// class is ceremony. Two concrete types with no relationship is the right shape, and inventing
/// the abstraction would buy a seam nothing can ever cross.
///
/// Winsock's process-wide startup is owned by this object's lifetime rather than by a pair of
/// calls somebody has to remember: Open starts it, Close and the destructor end it.
class WinsockTransport
{
public:
  WinsockTransport() noexcept = default;
  ~WinsockTransport() noexcept;

  // One socket, owned by one object. Nothing in the host needs to copy or move it, and a copy
  // would close a handle twice.
  WinsockTransport(const WinsockTransport&) = delete;
  WinsockTransport& operator=(const WinsockTransport&) = delete;
  WinsockTransport(WinsockTransport&&) = delete;
  WinsockTransport& operator=(WinsockTransport&&) = delete;

  /// Starts Winsock, creates the socket, puts it in non-blocking mode and binds it to every
  /// interface on _port. Pass 0 for an ephemeral port and read it back from BoundEndpoint().
  /// False if the transport is already open or any step failed, in which case nothing is left
  /// open and LastFault() carries the reason.
  [[nodiscard]] bool Open(std::uint16_t _port) noexcept;

  /// Closes the socket and ends Winsock. Safe on a transport that was never opened.
  void Close() noexcept;

  [[nodiscard]] bool IsOpen() const noexcept
  {
    return m_socket != NO_SOCKET;
  }

  /// What the socket is actually bound to, which is how the caller learns the ephemeral port it
  /// asked for with 0. Zeroed on a transport that is not open.
  [[nodiscard]] Endpoint BoundEndpoint() const noexcept
  {
    return m_bound;
  }

  /// One datagram to one peer. False on any failure including a full send buffer, which on this
  /// socket is indistinguishable from the datagram being lost in the network -- and ADR-003 says
  /// a client that misses a snapshot is fully correct on the next one, so neither is retried.
  [[nodiscard]] bool Send(const Endpoint& _to, std::span<const std::byte> _bytes) noexcept;

  /// One datagram out of the receive queue, without blocking. See ReceiveOutcome for the four
  /// answers; _outBytes and _outSender are written only on Received.
  [[nodiscard]] ReceiveOutcome Receive(std::span<std::byte> _buffer, std::size_t& _outBytes, Endpoint& _outSender) noexcept;

  /// The WSAGetLastError code from the last call that failed, or 0. A diagnostic, not control
  /// flow: every method above already says whether it worked.
  [[nodiscard]] int LastFault() const noexcept
  {
    return m_lastFault;
  }

private:
  /// INVALID_SOCKET, spelled without <winsock2.h> so that this header pulls in nothing. SOCKET is
  /// a UINT_PTR and INVALID_SOCKET is ~0; WinsockTransport.cpp static_asserts both.
  static constexpr std::uintptr_t NO_SOCKET = static_cast<std::uintptr_t>(-1);

  std::uintptr_t m_socket = NO_SOCKET;
  Endpoint m_bound{};
  int m_lastFault = 0;
  bool m_winsockStarted = false;
};

} // namespace Neuron
