#include "pch.h"
#include "ImpairedLink.h"

#include <mswsock.h>

#include <deque>

namespace
{
using Clock = std::chrono::steady_clock;
using HundredNanoseconds = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

// The longest datagram UDP carries.
constexpr int DATAGRAM_LIMIT_BYTES = 65'535;
// What IPv4 and UDP add to a datagram on the wire, which a way's limit carries too.
constexpr std::uint64_t HEADER_BYTES = 28;
// Each socket's buffers, deep enough that a burst on the loopback is the profile's to drop and not the socket's.
constexpr int SOCKET_BUFFER_BYTES = 4 * 1024 * 1024;

// Winsock, started for as long as it lives.
class WinsockSession : Neuron::NonCopyable
{
public:
  WinsockSession()
  {
    WSADATA data{};
    if (const int error = WSAStartup(MAKEWORD(2, 2), &data); error != 0)
      throw Neuron::Exception(std::format("Winsock did not start: error {}.", error));
  }

  ~WinsockSession()
  {
    (void)WSACleanup();
  }
};

// A socket, closed with it.
class OwnedSocket : Neuron::NonCopyable
{
public:
  OwnedSocket()
    : m_handle(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
  {
    if (m_handle == INVALID_SOCKET)
      throw Neuron::Exception(std::format("A UDP socket did not open: error {}.", WSAGetLastError()));
  }

  ~OwnedSocket()
  {
    (void)closesocket(m_handle);
  }

  [[nodiscard]] SOCKET Handle() const noexcept
  {
    return m_handle;
  }

private:
  SOCKET m_handle = INVALID_SOCKET;
};

sockaddr_in Loopback(std::uint16_t _port) noexcept
{
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(_port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return address;
}

void Check(int _result, std::string_view _what)
{
  if (_result == SOCKET_ERROR)
    throw Neuron::Exception(std::format("The impaired link could not {}: error {}.", _what, WSAGetLastError()));
}

// Opens _socket on the loopback, with deep buffers, and has it signal _readable when a datagram has come. A UDP socket on
// Windows otherwise fails its next read once a datagram it sent was refused, as a client gone or a server not yet listening
// refuses one; the link reads past that.
void Prepare(const OwnedSocket& _socket, HANDLE _readable)
{
  const sockaddr_in any = Loopback(0);
  Check(bind(_socket.Handle(), reinterpret_cast<const sockaddr*>(&any), sizeof(any)), "bind a socket");
  for (const int option : {SO_RCVBUF, SO_SNDBUF})
  {
    Check(
      setsockopt(_socket.Handle(), SOL_SOCKET, option, reinterpret_cast<const char*>(&SOCKET_BUFFER_BYTES), sizeof(SOCKET_BUFFER_BYTES)),
      "size a socket's buffers");
  }
  BOOL report = FALSE;
  DWORD returned = 0;
  Check(WSAIoctl(_socket.Handle(), SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &returned, nullptr, nullptr),
        "stop a socket reporting a refused datagram");
  // This also makes the socket's reads return at once when nothing has come.
  Check(WSAEventSelect(_socket.Handle(), _readable, FD_READ), "wait on a socket");
}

winrt::handle ManualResetEvent()
{
  winrt::handle event;
  event.attach(winrt::check_pointer(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS)));
  return event;
}
} // namespace

struct GameLogicTests::ImpairedLink::State
{
  struct Datagram
  {
    Clock::time_point due;
    std::vector<char> bytes;
  };

  // One way's datagrams on their way, touched by the link's thread only, and its counts, which any thread reads.
  struct Way
  {
    LinkWay profile;
    std::deque<Datagram> waiting;
    // When the last datagram given to the way is due, which no later one comes before.
    Clock::time_point lastDue{};
    // When the way's limit has carried every datagram it was given.
    Clock::time_point freeAt{};
    std::atomic<std::uint64_t> carried = 0;
    std::atomic<std::uint64_t> lost = 0;
    std::atomic<std::uint64_t> dropped = 0;
    std::atomic<std::uint64_t> cut = 0;
  };

  State(const LinkProfile& _profile, std::uint64_t _seed)
    : random(_seed)
  {
    toServer.profile = _profile.toServer;
    toClient.profile = _profile.toClient;
  }

  // First, so that Winsock outlives the sockets.
  WinsockSession winsock;
  OwnedSocket clientSide;
  OwnedSocket serverSide;
  winrt::handle clientReadable = ManualResetEvent();
  winrt::handle serverReadable = ManualResetEvent();
  winrt::handle timer;
  std::uint16_t port = 0;
  // Where the client's last datagram came from; touched by the link's thread only.
  std::optional<sockaddr_in> client;
  Neuron::Random random;
  std::atomic<bool> cut = false;
  Way toServer;
  Way toClient;

  // Gives _way a datagram that came at _now: lost, dropped or queued for when it is due.
  void Admit(Way& _way, std::span<const char> _bytes, Clock::time_point _now)
  {
    const LinkWay& profile = _way.profile;
    if (cut)
    {
      ++_way.cut;
      return;
    }
    if (profile.lossPartsPerMillion > 0 && random.NextBelow(1'000'000) < profile.lossPartsPerMillion)
    {
      ++_way.lost;
      return;
    }
    Clock::time_point departs = _now;
    if (profile.bitsPerSecond > 0)
    {
      const Clock::time_point start = std::max(_now, _way.freeAt);
      if (start - _now > QUEUE_LIMIT)
      {
        ++_way.dropped;
        return;
      }
      const std::uint64_t bits = (_bytes.size() + HEADER_BYTES) * 8;
      _way.freeAt = start + std::chrono::nanoseconds{static_cast<std::int64_t>((bits * 1'000'000'000) / profile.bitsPerSecond)};
      departs = _way.freeAt;
    }
    std::chrono::microseconds delay = profile.delay;
    if (profile.jitter.count() > 0)
    {
      const auto spread = static_cast<std::uint32_t>((2 * profile.jitter.count()) + 1);
      delay += std::chrono::microseconds{static_cast<std::int64_t>(random.NextBelow(spread)) - profile.jitter.count()};
    }
    const Clock::time_point due = std::max(departs + std::max(delay, std::chrono::microseconds{}), _way.lastDue);
    _way.lastDue = due;
    _way.waiting.push_back({.due = due, .bytes = {_bytes.begin(), _bytes.end()}});
  }

  // Reads every datagram that has come to _socket into _way. The client's side learns where the client is.
  void Read(const OwnedSocket& _socket, HANDLE _readable, Way& _way, std::vector<char>& _buffer)
  {
    WSANETWORKEVENTS events{};
    Check(WSAEnumNetworkEvents(_socket.Handle(), _readable, &events), "read a socket's events");
    while (true)
    {
      sockaddr_in from{};
      int fromBytes = sizeof(from);
      const int bytes =
        recvfrom(_socket.Handle(), _buffer.data(), static_cast<int>(_buffer.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromBytes);
      if (bytes == SOCKET_ERROR)
      {
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
          return;
        if (error == WSAECONNRESET || error == WSAEMSGSIZE)
          continue;
        Check(bytes, "read a socket");
      }
      if (&_socket == &clientSide)
        client = from;
      Admit(_way, std::span(_buffer).first(static_cast<std::size_t>(bytes)), Clock::now());
    }
  }

  // Sends every datagram of _way that is due, to _to, or where _socket is connected to without one.
  static void Deliver(Way& _way, const OwnedSocket& _socket, const sockaddr_in* _to)
  {
    const Clock::time_point now = Clock::now();
    while (!_way.waiting.empty() && _way.waiting.front().due <= now)
    {
      const std::vector<char>& bytes = _way.waiting.front().bytes;
      const auto length = static_cast<int>(bytes.size());
      const int sent = _to == nullptr
                         ? send(_socket.Handle(), bytes.data(), length, 0)
                         : sendto(_socket.Handle(), bytes.data(), length, 0, reinterpret_cast<const sockaddr*>(_to), sizeof(*_to));
      if (sent == SOCKET_ERROR)
        ++_way.dropped;
      else
        ++_way.carried;
      _way.waiting.pop_front();
    }
  }

  // When the next datagram either way is due, if any is waiting.
  [[nodiscard]] std::optional<Clock::time_point> NextDue() const
  {
    std::optional<Clock::time_point> next;
    for (const Way* way : {&toServer, &toClient})
    {
      if (!way->waiting.empty())
        next = std::min(next.value_or(Clock::time_point::max()), way->waiting.front().due);
    }
    return next;
  }

  static Counts CountsOf(const Way& _way) noexcept
  {
    return {.carried = _way.carried, .lost = _way.lost, .dropped = _way.dropped, .cut = _way.cut};
  }
};

GameLogicTests::ImpairedLink::ImpairedLink(std::uint16_t _serverPort, const LinkProfile& _profile, std::uint64_t _seed)
  : m_state(std::make_unique<State>(_profile, _seed))
{
  State& state = *m_state;
  Prepare(state.clientSide, state.clientReadable.get());
  Prepare(state.serverSide, state.serverReadable.get());
  sockaddr_in bound{};
  int boundBytes = sizeof(bound);
  Check(getsockname(state.clientSide.Handle(), reinterpret_cast<sockaddr*>(&bound), &boundBytes), "find its port");
  state.port = ntohs(bound.sin_port);
  const sockaddr_in server = Loopback(_serverPort);
  Check(connect(state.serverSide.Handle(), reinterpret_cast<const sockaddr*>(&server), sizeof(server)), "aim at the server");
  // A high-resolution timer sends a datagram when it is due, where a plain one waits for the system timer's next tick, 15.6
  // ms apart by default; as the server's thread does, it takes a plain one where there is no high-resolution one (ADR-055).
  state.timer.attach(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));
  if (!state.timer)
    state.timer.attach(winrt::check_pointer(CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS)));
  m_thread = std::jthread([this](const std::stop_token& _stop) { Run(_stop); });
}

GameLogicTests::ImpairedLink::~ImpairedLink() = default;

std::uint16_t GameLogicTests::ImpairedLink::Port() const noexcept
{
  return m_state->port;
}

Outpost::ServerAddress GameLogicTests::ImpairedLink::Through(Outpost::ServerAddress _seat) const
{
  _seat.host = "127.0.0.1";
  _seat.port = Port();
  return _seat;
}

void GameLogicTests::ImpairedLink::Cut(bool _cut) noexcept
{
  m_state->cut = _cut;
}

GameLogicTests::ImpairedLink::Counts GameLogicTests::ImpairedLink::ToServer() const noexcept
{
  return State::CountsOf(m_state->toServer);
}

GameLogicTests::ImpairedLink::Counts GameLogicTests::ImpairedLink::ToClient() const noexcept
{
  return State::CountsOf(m_state->toClient);
}

void GameLogicTests::ImpairedLink::Run(const std::stop_token& _stop)
{
  using Microsoft::VisualStudio::CppUnitTestFramework::Logger;
  // A failure stops the link, and a test that needs it fails waiting for what it does not carry.
  try
  {
    State& state = *m_state;
    const winrt::handle stopped = ManualResetEvent();
    // Declared after the event, so that it is gone before the event is.
    const std::stop_callback wake(_stop, [&stopped]() noexcept { (void)SetEvent(stopped.get()); });
    const std::array<HANDLE, 4> waits{stopped.get(), state.clientReadable.get(), state.serverReadable.get(), state.timer.get()};
    std::vector<char> buffer(DATAGRAM_LIMIT_BYTES);
    while (!_stop.stop_requested())
    {
      // A wait that wakes for nothing, as a timer that went off while a socket woke the thread does, reads and sends
      // nothing.
      const std::optional<Clock::time_point> next = state.NextDue();
      const auto remaining = next.has_value() ? std::chrono::ceil<HundredNanoseconds>(*next - Clock::now()) : HundredNanoseconds::max();
      if (remaining.count() > 0)
      {
        if (next.has_value())
        {
          // A negative due time is relative to now.
          LARGE_INTEGER due{};
          due.QuadPart = -remaining.count();
          winrt::check_bool(SetWaitableTimer(state.timer.get(), &due, 0, nullptr, nullptr, FALSE));
        }
        if (WaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE, INFINITE) == WAIT_FAILED)
          winrt::throw_last_error();
      }
      if (_stop.stop_requested())
        return;
      state.Read(state.clientSide, state.clientReadable.get(), state.toServer, buffer);
      state.Read(state.serverSide, state.serverReadable.get(), state.toClient, buffer);
      // The server's side is connected to the server.
      State::Deliver(state.toServer, state.serverSide, nullptr);
      if (state.client.has_value())
        State::Deliver(state.toClient, state.clientSide, &*state.client);
    }
  }
  catch (const winrt::hresult_error& error)
  {
    Logger::WriteMessage(std::format(L"The impaired link stopped: {}", std::wstring_view(error.message())).c_str());
  }
  catch (const std::exception& error)
  {
    Logger::WriteMessage(std::format("The impaired link stopped: {}", error.what()).c_str());
  }
}
