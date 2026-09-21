#include "pch.h"

#include "WinsockTransport.h"

// <winsock2.h> AFTER <windows.h> is only safe because NeuronCore.h defines WIN32_LEAN_AND_MEAN,
// which is exactly what keeps the older <winsock.h> out of <windows.h>. That macro has one owner
// in this tree (AGENTS.md section 4) and this file is one of the reasons it matters.
#include <winsock2.h>
#include <mstcpip.h>

// The link dependency travels with the code that needs it rather than with each project that
// happens to link NeuronServer. A StaticLibrary's AdditionalDependencies do not reach the
// executables and suites downstream of it; a directive in the .obj does.
#pragma comment(lib, "ws2_32.lib")

namespace Neuron
{

namespace
{
/// Host byte order in, network byte order out. This function and its inverse are the only two
/// places in the tree that call htons or ntohl.
[[nodiscard]] sockaddr_in ToSockAddr(const Endpoint& _endpoint) noexcept
{
  sockaddr_in address{};
  address.sin_family = static_cast<ADDRESS_FAMILY>(AF_INET);
  address.sin_port = htons(_endpoint.port);
  address.sin_addr.s_addr = htonl(_endpoint.addressV4);
  return address;
}

[[nodiscard]] Endpoint FromSockAddr(const sockaddr_in& _address) noexcept
{
  return Endpoint{.addressV4 = static_cast<std::uint32_t>(ntohl(_address.sin_addr.s_addr)),
                  .port = static_cast<std::uint16_t>(ntohs(_address.sin_port))};
}
} // namespace

WinsockTransport::~WinsockTransport() noexcept
{
  Close();
}

bool WinsockTransport::Open(std::uint16_t _port) noexcept
{
  // The two assumptions that let this header hide <winsock2.h> behind a std::uintptr_t.
  static_assert(sizeof(SOCKET) == sizeof(std::uintptr_t), "SOCKET is a UINT_PTR; NO_SOCKET stands in for it");
  static_assert(NO_SOCKET == static_cast<std::uintptr_t>(INVALID_SOCKET), "NO_SOCKET must be INVALID_SOCKET");

  if (IsOpen())
  {
    return false;
  }

  WSADATA winsockData{};
  const int startup = WSAStartup(MAKEWORD(2, 2), &winsockData);
  if (startup != 0)
  {
    // WSAStartup is the one call that returns its error rather than leaving it in WSAGetLastError,
    // because on failure there may be no Winsock to hold one.
    m_lastFault = startup;
    return false;
  }
  m_winsockStarted = true;

  const SOCKET handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (handle == INVALID_SOCKET)
  {
    m_lastFault = WSAGetLastError();
    Close();
    return false;
  }
  m_socket = static_cast<std::uintptr_t>(handle);

  u_long nonBlocking = 1;
  if (ioctlsocket(handle, FIONBIO, &nonBlocking) == SOCKET_ERROR)
  {
    m_lastFault = WSAGetLastError();
    Close();
    return false;
  }

  // SIO_UDP_CONNRESET off, and this one is not optional on Windows. When a datagram this host
  // sent reaches a machine with nothing listening, the ICMP port-unreachable that comes back
  // makes the NEXT recvfrom on this socket fail with WSAECONNRESET -- on a connectionless socket,
  // reporting a peer's state as this socket's fault. The host's drain would read that as a broken
  // socket the first time a client quits. Turning it off restores the behavior UDP is written
  // against: a peer that is gone is silence, not an error.
  BOOL reportConnectionReset = FALSE;
  DWORD returnedBytes = 0;
  if (WSAIoctl(handle, SIO_UDP_CONNRESET, &reportConnectionReset, sizeof(reportConnectionReset), nullptr, 0, &returnedBytes, nullptr,
               nullptr) == SOCKET_ERROR)
  {
    m_lastFault = WSAGetLastError();
    Close();
    return false;
  }

  // Every interface, because a client on the LAN has to reach the host as well as one on this
  // machine. WHERE the host is, is a different question and ADR-008 owns it.
  const sockaddr_in address = ToSockAddr(Endpoint{.addressV4 = static_cast<std::uint32_t>(INADDR_ANY), .port = _port});

  // Qualified, because <functional> anywhere in this translation unit's future would otherwise
  // make this std::bind.
  if (::bind(handle, reinterpret_cast<const sockaddr*>(&address), static_cast<int>(sizeof(address))) == SOCKET_ERROR)
  {
    m_lastFault = WSAGetLastError();
    Close();
    return false;
  }

  // Asking the socket what it got is how the caller learns the port it asked for with 0.
  sockaddr_in boundAddress{};
  int boundLength = static_cast<int>(sizeof(boundAddress));
  if (getsockname(handle, reinterpret_cast<sockaddr*>(&boundAddress), &boundLength) == SOCKET_ERROR)
  {
    m_lastFault = WSAGetLastError();
    Close();
    return false;
  }
  m_bound = FromSockAddr(boundAddress);

  m_lastFault = 0;
  return true;
}

void WinsockTransport::Close() noexcept
{
  if (m_socket != NO_SOCKET)
  {
    closesocket(static_cast<SOCKET>(m_socket));
    m_socket = NO_SOCKET;
  }
  if (m_winsockStarted)
  {
    WSACleanup();
    m_winsockStarted = false;
  }
  m_bound = Endpoint{};

  // m_lastFault survives on purpose: Open calls this on its own failure paths, and the caller is
  // about to ask what went wrong.
}

bool WinsockTransport::Send(const Endpoint& _to, std::span<const std::byte> _bytes) noexcept
{
  if (!IsOpen())
  {
    return false;
  }

  // A zero-length datagram is legal UDP and is never anything this host means to send -- it
  // cannot even carry a PacketHeader. Refusing it here also settles what sendto would be handed
  // for an empty span's data(), which is not required to be a pointer at all.
  if (_bytes.empty())
  {
    return false;
  }

  const sockaddr_in address = ToSockAddr(_to);
  const int sent = sendto(static_cast<SOCKET>(m_socket), reinterpret_cast<const char*>(_bytes.data()), static_cast<int>(_bytes.size()), 0,
                          reinterpret_cast<const sockaddr*>(&address), static_cast<int>(sizeof(address)));
  if (sent == SOCKET_ERROR)
  {
    m_lastFault = WSAGetLastError();
    return false;
  }
  return static_cast<std::size_t>(sent) == _bytes.size();
}

ReceiveOutcome WinsockTransport::Receive(std::span<std::byte> _buffer, std::size_t& _outBytes, Endpoint& _outSender) noexcept
{
  if (!IsOpen() || _buffer.empty())
  {
    return ReceiveOutcome::Failed;
  }

  sockaddr_in from{};
  int fromLength = static_cast<int>(sizeof(from));
  const int received = recvfrom(static_cast<SOCKET>(m_socket), reinterpret_cast<char*>(_buffer.data()), static_cast<int>(_buffer.size()), 0,
                                reinterpret_cast<sockaddr*>(&from), &fromLength);
  if (received == SOCKET_ERROR)
  {
    const int fault = WSAGetLastError();

    // An empty queue is the ordinary answer on a non-blocking socket, twenty times a second for
    // as long as nobody is talking. Recording it would bury every fault worth reading.
    if (fault == WSAEWOULDBLOCK)
    {
      return ReceiveOutcome::Empty;
    }

    m_lastFault = fault;

    // The datagram did not fit and Windows has already discarded what would not: the buffer holds
    // a truncated prefix that is worth nothing, and the NEXT receive gets the next datagram
    // rather than this one again. Saying so is what keeps a drain loop from wedging on it.
    if (fault == WSAEMSGSIZE)
    {
      return ReceiveOutcome::Oversized;
    }
    return ReceiveOutcome::Failed;
  }

  // recvfrom's return is the DATAGRAM's length, not the buffer's. Reporting the buffer's is the
  // bug this out-parameter exists to make impossible to write.
  _outBytes = static_cast<std::size_t>(received);
  _outSender = FromSockAddr(from);
  return ReceiveOutcome::Received;
}

} // namespace Neuron
