#include "pch.h"
#include "QuicChannel.h"
#include "ServerCertificate.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <optional>
#include <utility>

// After pch.h, which sets the Windows macro family before msquic.h includes <windows.h> (ADR-004).
#include <msquic.h>

namespace
{
// Each message on the stream is preceded by its length in bytes, in four bytes, little-endian (ADR-060).
constexpr std::size_t LENGTH_BYTES = 4;
// How long a client's handshake with the listener may take.
constexpr std::uint64_t SERVER_HANDSHAKE_TIMEOUT_MILLISECONDS = 10'000;
// How much longer Connect waits than the handshake may take, so that MsQuic's own timeout is the one that ends it.
constexpr std::chrono::milliseconds CONNECT_GRACE{1'000};
// What a client is told when the server had no listener for its connection.
constexpr std::string_view NO_LISTENER_HINT =
  " The server has no listener for this connection: it listens on another address family than the host was reached on, such "
  "as 0.0.0.0 for a host that resolved to IPv6, or speaks another application protocol.";

[[nodiscard]] std::string StatusText(QUIC_STATUS _status)
{
  return std::format("0x{:08X}", static_cast<std::uint32_t>(_status));
}

void Check(QUIC_STATUS _status, std::string_view _call)
{
  if (QUIC_FAILED(_status))
    throw Neuron::Exception(std::format("QUIC: {} failed with status {}.", _call, StatusText(_status)));
}

// MsQuic's function table and one registration. Everything made from them holds a reference, so it outlives them all.
class QuicRuntime : Neuron::NonCopyable
{
public:
  QuicRuntime()
  {
    Check(MsQuicOpen2(&m_api), "MsQuicOpen2");
    const QUIC_REGISTRATION_CONFIG config{.AppName = "Neuron", .ExecutionProfile = QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    const QUIC_STATUS status = m_api->RegistrationOpen(&config, &m_registration);
    if (QUIC_FAILED(status))
    {
      MsQuicClose(m_api);
      Check(status, "RegistrationOpen");
    }
  }

  // Waits until every connection and listener made in the registration is closed, which their holders have done.
  ~QuicRuntime()
  {
    m_api->RegistrationClose(m_registration);
    MsQuicClose(m_api);
  }

  [[nodiscard]] const QUIC_API_TABLE& Api() const noexcept
  {
    return *m_api;
  }

  [[nodiscard]] HQUIC Registration() const noexcept
  {
    return m_registration;
  }

private:
  const QUIC_API_TABLE* m_api = nullptr;
  HQUIC m_registration = nullptr;
};

// MsQuic reads the application protocol while the call that takes it runs, and copies it.
[[nodiscard]] QUIC_BUFFER ProtocolBuffer(std::string& _protocol) noexcept
{
  return {.Length = static_cast<std::uint32_t>(_protocol.size()), .Buffer = reinterpret_cast<std::uint8_t*>(_protocol.data())};
}

[[nodiscard]] HQUIC OpenConfiguration(const QuicRuntime& _runtime, std::string _protocol, const QUIC_SETTINGS& _settings,
                                      const QUIC_CREDENTIAL_CONFIG& _credential)
{
  const QUIC_API_TABLE& api = _runtime.Api();
  const QUIC_BUFFER protocol = ProtocolBuffer(_protocol);
  HQUIC configuration = nullptr;
  Check(api.ConfigurationOpen(_runtime.Registration(), &protocol, 1, &_settings, static_cast<std::uint32_t>(sizeof(_settings)), nullptr,
                              &configuration),
        "ConfigurationOpen");
  const QUIC_STATUS status = api.ConfigurationLoadCredential(configuration, &_credential);
  if (QUIC_FAILED(status))
  {
    api.ConfigurationClose(configuration);
    Check(status, "ConfigurationLoadCredential");
  }
  return configuration;
}

// A message on its way. MsQuic reads the buffer until it reports the send complete, which with its send buffering off is once
// the peer has acknowledged it (ADR-004 decision 6), and the stream's callback deletes it then. Until then its bytes count in
// the channel's backlog.
struct PendingSend
{
  std::vector<std::byte> bytes;
  QUIC_BUFFER buffer{};
  std::atomic<std::uint64_t>* backlog = nullptr;
};

// The bidirectional stream's messages go ahead of the newest messages' streams, which keep MsQuic's default (ADR-096).
constexpr std::uint16_t ORDERED_STREAM_PRIORITY = 0xFFFF;
// How many newest messages are on their way at most: enough that a link whose round trip, and the peer's delay of its
// acknowledgement, is longer than the time between two of them still carries each. A message sent while as many are is
// dropped, rather than any of them abandoned, so that a link too slow for every message carries as many whole as it can.
// MsQuic sends streams of one priority first come, first served, so the older goes first (ADR-096).
constexpr std::size_t NEWEST_ON_THEIR_WAY = 4;
} // namespace

struct Neuron::QuicChannel::State
{
  std::shared_ptr<QuicRuntime> runtime;
  QuicChannel* owner = nullptr;
  // The server's end only: where each message goes.
  Receiver receiver;
  // The client's end only: the server's certificate, which is the only one it accepts.
  std::optional<CertificateHash> pinned;
  // The bytes of a message not yet whole, whether the peer broke the framing, after which nothing more is read, and the
  // longest message the peer may send. Only the connection's MsQuic thread touches them, and it delivers one callback at a
  // time: the listener sets the limit before the connection's first, and a Receiver within its own (LimitMessages).
  std::vector<std::byte> partial;
  bool broken = false;
  std::uint32_t messageLimit = MESSAGE_LIMIT_BYTES;
  // Whether the peer has sent a whole message, which the listener's Sweep reads.
  std::atomic<bool> spoke = false;
  // The bytes handed to MsQuic that it has not reported sent and acknowledged: Send adds them and the stream's callback takes
  // them away, without the lock, which a callback inside a send would otherwise wait for.
  std::atomic<std::uint64_t> backlogBytes = 0;

  // One unidirectional stream of a message of which only the newest matters (ADR-096): the server's end sends on it, and the
  // client's end reads it. The stream's callbacks have it as their context.
  struct NewestStream
  {
    State* state = nullptr;
    HQUIC handle = nullptr;
    // The server's end: whether the message's send has completed, the peer having acknowledged it whole or the stream having
    // failed, so that it is no longer on its way.
    bool settled = false;
    // The client's end: the stream's id, which orders the messages, and the message's bytes so far.
    std::uint64_t id = 0;
    std::vector<std::byte> bytes;
  };

  // Guards everything below. Nothing holds it while it calls a Receiver or waits for MsQuic.
  std::mutex mutex;
  std::condition_variable changed;
  // The client's own configuration; a server's end uses its listener's.
  HQUIC configuration = nullptr;
  HQUIC connection = nullptr;
  HQUIC stream = nullptr;
  std::vector<std::vector<std::byte>> messages;
  // The newest messages' streams MsQuic has not finished with, each closed by whichever takes it out of here: its own last
  // callback, or Close. The client's end also keeps the newest messages that have arrived whole, and the id of the newest.
  std::vector<std::unique_ptr<NewestStream>> newestStreams;
  std::vector<std::vector<std::byte>> newest;
  std::optional<std::uint64_t> newestId;
  bool connected = false;
  bool closedHere = false;
  // When the handshake completed, whether this end has started closing the connection, and whether MsQuic has finished with
  // it, after which no callback of it is to come but the one that says so.
  std::optional<std::chrono::steady_clock::time_point> connectedAt;
  bool closing = false;
  bool shutDown = false;
  // Why the connection went, once it has, and the code the peer closed it with, when it was the peer.
  std::string failure;
  std::optional<std::uint64_t> peerErrorCode;

  [[nodiscard]] bool HasShutDown()
  {
    const std::scoped_lock lock(mutex);
    return shutDown;
  }

  // Whether the handshake completed more than _silence before _now, and the peer has sent nothing since and is still there.
  [[nodiscard]] bool IsSilent(std::chrono::steady_clock::time_point _now, std::chrono::milliseconds _silence)
  {
    const std::scoped_lock lock(mutex);
    return connectedAt.has_value() && !spoke && !closing && failure.empty() && _now - *connectedAt > _silence;
  }

  // Keeps the first reason the connection went, which is the one that explains the rest.
  void Fail(std::string _why)
  {
    {
      const std::scoped_lock lock(mutex);
      if (failure.empty())
        failure = std::move(_why);
    }
    changed.notify_all();
  }

  void ShutdownConnection(std::uint64_t _errorCode) noexcept
  {
    const std::scoped_lock lock(mutex);
    closing = true;
    if (connection != nullptr)
      runtime->Api().ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, _errorCode);
  }

  // The peer broke the framing, or the Receiver could not take a message.
  void Break(std::string_view _why)
  {
    broken = true;
    Fail(std::string(_why));
    ShutdownConnection(FRAMING_ERROR_CODE);
  }

  [[nodiscard]] QUIC_STATUS CheckCertificate(const QUIC_CONNECTION_EVENT& _event)
  {
    if (!pinned)
      return QUIC_STATUS_SUCCESS;
    // QUIC_CREDENTIAL_FLAG_USE_PORTABLE_CERTIFICATES makes it the certificate's DER encoding.
    const auto* der = static_cast<const QUIC_BUFFER*>(_event.PEER_CERTIFICATE_RECEIVED.Certificate);
    if (der != nullptr && HashCertificate({der->Buffer, der->Length}) == *pinned)
      return QUIC_STATUS_SUCCESS;
    Fail("The server presented another certificate than the one the client was given.");
    return QUIC_STATUS_BAD_CERTIFICATE;
  }

  // The client opens the connection's one bidirectional stream, and the server's end adopts it here (ADR-060); the server
  // opens a unidirectional stream for each newest message, and the client's end reads each (ADR-096).
  void AdoptStream(const QUIC_CONNECTION_EVENT& _event)
  {
    const HQUIC peerStream = _event.PEER_STREAM_STARTED.Stream;
    const QUIC_API_TABLE& api = runtime->Api();
    const bool unidirectional = (_event.PEER_STREAM_STARTED.Flags & QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL) != 0;
    // Only the client's end has a pinned certificate, and only it lets its peer open unidirectional streams.
    bool adopted = false;
    if (unidirectional && pinned.has_value())
    {
      // Its events come on this thread, after this returns, so its handler is set before Close can see it. One that starts
      // once Close has taken the channel's streams is refused, as Close would not close it.
      auto newestStream = std::make_unique<NewestStream>(NewestStream{.state = this, .handle = peerStream});
      QUIC_UINT62 id = 0;
      auto idBytes = static_cast<std::uint32_t>(sizeof(id));
      if (QUIC_SUCCEEDED(api.GetParam(peerStream, QUIC_PARAM_STREAM_ID, &idBytes, &id)))
        newestStream->id = id;
      api.SetCallbackHandler(peerStream, reinterpret_cast<void*>(&OnNewestStream), newestStream.get());
      {
        const std::scoped_lock lock(mutex);
        if (!closedHere)
        {
          newestStreams.push_back(std::move(newestStream));
          adopted = true;
        }
      }
      if (adopted)
        return;
    }
    else if (!unidirectional)
    {
      const std::scoped_lock lock(mutex);
      if (stream == nullptr)
      {
        stream = peerStream;
        adopted = true;
      }
    }
    if (adopted)
    {
      api.SetCallbackHandler(peerStream, reinterpret_cast<void*>(&OnStream), this);
      // What it carries goes ahead of the newest messages (ADR-096).
      (void)api.SetParam(peerStream, QUIC_PARAM_STREAM_PRIORITY, static_cast<std::uint32_t>(sizeof(ORDERED_STREAM_PRIORITY)),
                         &ORDERED_STREAM_PRIORITY);
      return;
    }
    // Any other stream is refused: one that starts as the channel closes, or one more than the listener lets a client open,
    // which does not happen.
    api.SetCallbackHandler(peerStream, reinterpret_cast<void*>(&OnRefusedStream), this);
    (void)api.StreamShutdown(peerStream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, FRAMING_ERROR_CODE);
  }

  // Adds what arrived to the message being read, and passes on every message that is now whole. A Receiver is handed each
  // before the next one's length is read, so that a limit it sets holds from the next.
  void Unpack(const QUIC_STREAM_EVENT& _event)
  {
    if (broken)
      return;
    for (std::uint32_t i = 0; i < _event.RECEIVE.BufferCount; ++i)
    {
      const QUIC_BUFFER& buffer = _event.RECEIVE.Buffers[i];
      const auto* bytes = reinterpret_cast<const std::byte*>(buffer.Buffer);
      partial.insert(partial.end(), bytes, bytes + buffer.Length);
    }

    std::vector<std::vector<std::byte>> whole;
    std::size_t offset = 0;
    while (partial.size() - offset >= LENGTH_BYTES)
    {
      std::uint32_t length = 0;
      for (std::size_t i = 0; i < LENGTH_BYTES; ++i)
        length |= static_cast<std::uint32_t>(partial[offset + i]) << (8 * i);
      if (length > messageLimit)
        throw Exception(std::format("The peer sent a message of {} bytes, over the limit of {}.", length, messageLimit));
      if (partial.size() - offset - LENGTH_BYTES < length)
        break;
      const auto first = partial.begin() + static_cast<std::ptrdiff_t>(offset + LENGTH_BYTES);
      std::vector<std::byte> message(first, first + length);
      offset += LENGTH_BYTES + length;
      spoke = true;
      if (receiver)
        receiver(*owner, std::move(message));
      else
        whole.push_back(std::move(message));
    }
    partial.erase(partial.begin(), partial.begin() + static_cast<std::ptrdiff_t>(offset));

    if (whole.empty())
      return;
    {
      const std::scoped_lock lock(mutex);
      for (std::vector<std::byte>& message : whole)
        messages.push_back(std::move(message));
    }
    changed.notify_all();
  }

  // Takes a newest message's stream out of the channel's, when its last callback comes before Close takes it, so that this
  // callback closes it.
  [[nodiscard]] std::unique_ptr<NewestStream> Release(const NewestStream* _stream)
  {
    const std::scoped_lock lock(mutex);
    const auto found = std::ranges::find(newestStreams, _stream, &std::unique_ptr<NewestStream>::get);
    if (found == newestStreams.end())
      return nullptr;
    std::unique_ptr<NewestStream> released = std::move(*found);
    newestStreams.erase(found);
    return released;
  }

  // A newest message has arrived whole: kept if it is newer than any before it.
  void Arrived(NewestStream& _stream)
  {
    {
      const std::scoped_lock lock(mutex);
      if (newestId.has_value() && _stream.id <= *newestId)
        return;
      newestId = _stream.id;
      newest.push_back(std::exchange(_stream.bytes, {}));
    }
    changed.notify_all();
  }

  static QUIC_STATUS QUIC_API OnConnection(HQUIC _connection, void* _context, QUIC_CONNECTION_EVENT* _event) noexcept;
  static QUIC_STATUS QUIC_API OnStream(HQUIC _stream, void* _context, QUIC_STREAM_EVENT* _event) noexcept;
  static QUIC_STATUS QUIC_API OnNewestStream(HQUIC _stream, void* _context, QUIC_STREAM_EVENT* _event) noexcept;
  static QUIC_STATUS QUIC_API OnRefusedStream(HQUIC _stream, void* _context, QUIC_STREAM_EVENT* _event) noexcept;
};

QUIC_STATUS QUIC_API Neuron::QuicChannel::State::OnConnection([[maybe_unused]] HQUIC _connection, void* _context,
                                                              QUIC_CONNECTION_EVENT* _event) noexcept
{
  State& state = *static_cast<State*>(_context);
  try
  {
    switch (_event->Type)
    {
    case QUIC_CONNECTION_EVENT_CONNECTED:
    {
      const std::scoped_lock lock(state.mutex);
      state.connected = true;
      state.connectedAt = std::chrono::steady_clock::now();
    }
      state.changed.notify_all();
      break;
    case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED:
      return state.CheckCertificate(*_event);
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      state.AdoptStream(*_event);
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
    {
      const QUIC_STATUS status = _event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status;
      // A server refuses a connection no listener of its matches as it refuses an application protocol it does not
      // speak, and the likelier cause is a listener of one address family reached on the other.
      const std::string_view hint = status == QUIC_STATUS_ALPN_NEG_FAILURE ? NO_LISTENER_HINT : std::string_view{};
      state.Fail(std::format("The QUIC connection failed with status {}.{}", StatusText(status), hint));
      break;
    }
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
    {
      const std::scoped_lock lock(state.mutex);
      state.peerErrorCode = _event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode;
    }
      state.Fail(std::format("The peer closed the QUIC connection with error code {}.", _event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode));
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
    {
      const std::scoped_lock lock(state.mutex);
      state.shutDown = true;
    }
      state.Fail("The QUIC connection closed.");
      break;
    default:
      break;
    }
    return QUIC_STATUS_SUCCESS;
  }
  catch (...)
  {
    return QUIC_STATUS_INTERNAL_ERROR;
  }
}

QUIC_STATUS QUIC_API Neuron::QuicChannel::State::OnStream([[maybe_unused]] HQUIC _stream, void* _context,
                                                          QUIC_STREAM_EVENT* _event) noexcept
{
  State& state = *static_cast<State*>(_context);
  try
  {
    switch (_event->Type)
    {
    case QUIC_STREAM_EVENT_RECEIVE:
      try
      {
        state.Unpack(*_event);
      }
      catch (const std::exception& error)
      {
        state.Break(error.what());
      }
      break;
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
    {
      const std::unique_ptr<PendingSend> sent(static_cast<PendingSend*>(_event->SEND_COMPLETE.ClientContext));
      *sent->backlog -= sent->bytes.size();
    }
    break;
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
      state.Fail("The peer closed its stream.");
      break;
    default:
      break;
    }
    return QUIC_STATUS_SUCCESS;
  }
  catch (...)
  {
    return QUIC_STATUS_INTERNAL_ERROR;
  }
}

QUIC_STATUS QUIC_API Neuron::QuicChannel::State::OnNewestStream(HQUIC _stream, void* _context, QUIC_STREAM_EVENT* _event) noexcept
{
  auto& newestStream = *static_cast<NewestStream*>(_context);
  State& state = *newestStream.state;
  try
  {
    switch (_event->Type)
    {
    case QUIC_STREAM_EVENT_RECEIVE:
      for (std::uint32_t i = 0; i < _event->RECEIVE.BufferCount; ++i)
      {
        const QUIC_BUFFER& buffer = _event->RECEIVE.Buffers[i];
        const auto* bytes = reinterpret_cast<const std::byte*>(buffer.Buffer);
        newestStream.bytes.insert(newestStream.bytes.end(), bytes, bytes + buffer.Length);
      }
      if (newestStream.bytes.size() > state.messageLimit)
        state.Break(std::format("The peer sent a message of more than {} bytes, over the limit.", state.messageLimit));
      break;
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
      // The stream's end is the message's.
      if (!state.broken)
        state.Arrived(newestStream);
      break;
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
    {
      const std::unique_ptr<PendingSend> sent(static_cast<PendingSend*>(_event->SEND_COMPLETE.ClientContext));
      *sent->backlog -= sent->bytes.size();
      const std::scoped_lock lock(state.mutex);
      newestStream.settled = true;
    }
    break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      // Closed here, unless Close has taken it to close it; either way nothing touches the stream after.
      if (const std::unique_ptr<NewestStream> released = state.Release(&newestStream))
        state.runtime->Api().StreamClose(_stream);
      break;
    default:
      break;
    }
    return QUIC_STATUS_SUCCESS;
  }
  catch (...)
  {
    return QUIC_STATUS_INTERNAL_ERROR;
  }
}

QUIC_STATUS QUIC_API Neuron::QuicChannel::State::OnRefusedStream(HQUIC _stream, void* _context, QUIC_STREAM_EVENT* _event) noexcept
{
  // Nothing is sent on a refused stream, so its only event that matters is the one that lets its handle go. The
  // connection's close waits for it, so the state is still there.
  if (_event->Type == QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE)
    static_cast<State*>(_context)->runtime->Api().StreamClose(_stream);
  return QUIC_STATUS_SUCCESS;
}

std::unique_ptr<Neuron::QuicChannel> Neuron::QuicChannel::Connect(const Desc& _desc)
{
  auto state = std::make_unique<State>();
  state->runtime = std::make_shared<QuicRuntime>();
  state->pinned = _desc.serverCertificate;

  QUIC_SETTINGS settings{};
  settings.HandshakeIdleTimeoutMs = static_cast<std::uint64_t>(_desc.connectTimeout.count());
  settings.IsSet.HandshakeIdleTimeoutMs = TRUE;
  // The server opens a unidirectional stream for each newest message (ADR-096).
  settings.PeerUnidiStreamCount = NEWEST_STREAMS;
  settings.IsSet.PeerUnidiStreamCount = TRUE;
  // A send completes once the peer has acknowledged it, so the backlog counts what has not reached it (ADR-004 decision 6).
  settings.SendBufferingEnabled = FALSE;
  settings.IsSet.SendBufferingEnabled = TRUE;
  // The server's certificate is self-signed, so it is pinned rather than validated against a root (ADR-060).
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_NONE;
  credential.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION |
                     QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED | QUIC_CREDENTIAL_FLAG_USE_PORTABLE_CERTIFICATES;
  state->configuration = OpenConfiguration(*state->runtime, _desc.applicationProtocol, settings, credential);

  // From here the channel owns every handle, and its destructor closes them if this throws.
  auto channel = std::make_unique<QuicChannel>(std::move(state));
  State& own = *channel->m_state;
  const QUIC_API_TABLE& api = own.runtime->Api();
  HQUIC connection = nullptr;
  Check(api.ConnectionOpen(own.runtime->Registration(), &State::OnConnection, &own, &connection), "ConnectionOpen");
  {
    const std::scoped_lock lock(own.mutex);
    own.connection = connection;
  }
  Check(api.ConnectionStart(connection, own.configuration, QUIC_ADDRESS_FAMILY_UNSPEC, _desc.host.c_str(), _desc.port), "ConnectionStart");
  {
    std::unique_lock lock(own.mutex);
    if (!own.changed.wait_for(lock, _desc.connectTimeout + CONNECT_GRACE, [&own] { return own.connected || !own.failure.empty(); }))
      throw Exception(
        std::format("Could not connect to {}:{} over QUIC within {} ms.", _desc.host, _desc.port, _desc.connectTimeout.count()));
    if (!own.failure.empty())
      throw Exception(std::format("Could not connect to {}:{} over QUIC. {}", _desc.host, _desc.port, own.failure));
  }

  HQUIC stream = nullptr;
  Check(api.StreamOpen(connection, QUIC_STREAM_OPEN_FLAG_NONE, &State::OnStream, &own, &stream), "StreamOpen");
  {
    const std::scoped_lock lock(own.mutex);
    own.stream = stream;
  }
  Check(api.StreamStart(stream, QUIC_STREAM_START_FLAG_IMMEDIATE), "StreamStart");
  return channel;
}

Neuron::QuicChannel::QuicChannel(std::unique_ptr<State> _state) noexcept
  : m_state(std::move(_state))
{
  m_state->owner = this;
}

Neuron::QuicChannel::~QuicChannel()
{
  Close();
}

void Neuron::QuicChannel::Send(std::span<const std::byte> _message)
{
  if (_message.size() > MESSAGE_LIMIT_BYTES)
    throw Exception(std::format("A message of {} bytes is over the limit of {}.", _message.size(), MESSAGE_LIMIT_BYTES));
  auto pending = std::make_unique<PendingSend>();
  pending->bytes.resize(LENGTH_BYTES + _message.size());
  const auto length = static_cast<std::uint32_t>(_message.size());
  for (std::size_t i = 0; i < LENGTH_BYTES; ++i)
    pending->bytes[i] = static_cast<std::byte>(length >> (8 * i));
  std::ranges::copy(_message, pending->bytes.begin() + static_cast<std::ptrdiff_t>(LENGTH_BYTES));
  pending->buffer = {.Length = static_cast<std::uint32_t>(pending->bytes.size()),
                     .Buffer = reinterpret_cast<std::uint8_t*>(pending->bytes.data())};

  State& state = *m_state;
  pending->backlog = &state.backlogBytes;
  const std::scoped_lock lock(state.mutex);
  if (state.stream == nullptr)
    return;
  // MsQuic holds the send from here until it reports it complete, and the stream's callback deletes it then. A send it
  // refuses is never reported.
  PendingSend* sending = pending.release();
  state.backlogBytes += sending->bytes.size();
  if (QUIC_FAILED(state.runtime->Api().StreamSend(state.stream, &sending->buffer, 1, QUIC_SEND_FLAG_NONE, sending)))
  {
    state.backlogBytes -= sending->bytes.size();
    delete sending;
  }
}

bool Neuron::QuicChannel::SendNewest(std::span<const std::byte> _message)
{
  if (_message.size() > MESSAGE_LIMIT_BYTES)
    throw Exception(std::format("A message of {} bytes is over the limit of {}.", _message.size(), MESSAGE_LIMIT_BYTES));
  // The stream's end marks the message's, so it crosses without a length.
  auto pending = std::make_unique<PendingSend>();
  pending->bytes.assign(_message.begin(), _message.end());
  pending->buffer = {.Length = static_cast<std::uint32_t>(pending->bytes.size()),
                     .Buffer = reinterpret_cast<std::uint8_t*>(pending->bytes.data())};
  State& state = *m_state;
  pending->backlog = &state.backlogBytes;
  const QUIC_API_TABLE& api = state.runtime->Api();

  // A stream in the list is not closed while the lock is held, since only its own last callback and Close close one, and each
  // takes it out of the list first. Calls from this thread, which is not MsQuic's, run on MsQuic's own and call back there,
  // so none waits for a callback while the lock is held, but a close does: a stream that could not start is closed after.
  auto newestStream = std::make_unique<State::NewestStream>(State::NewestStream{.state = &state});
  HQUIC unstarted = nullptr;
  bool sent = false;
  {
    const std::scoped_lock lock(state.mutex);
    if (state.connection == nullptr || !state.failure.empty() ||
        std::ranges::count_if(state.newestStreams, [](const std::unique_ptr<State::NewestStream>& _stream) { return !_stream->settled; }) >=
          static_cast<std::ptrdiff_t>(NEWEST_ON_THEIR_WAY))
      return false;
    if (QUIC_FAILED(api.StreamOpen(state.connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL, &State::OnNewestStream, newestStream.get(),
                                   &newestStream->handle)))
      return false;
    // A stream the peer allows no more of fails to start, after this returns, and is shut down then, which drops its message;
    // the next goes as usual.
    if (QUIC_FAILED(api.StreamStart(newestStream->handle, QUIC_STREAM_START_FLAG_FAIL_BLOCKED | QUIC_STREAM_START_FLAG_SHUTDOWN_ON_FAIL)))
      unstarted = newestStream->handle;
    else
    {
      PendingSend* sending = pending.release();
      state.backlogBytes += sending->bytes.size();
      if (QUIC_FAILED(api.StreamSend(newestStream->handle, &sending->buffer, 1, QUIC_SEND_FLAG_FIN, sending)))
      {
        state.backlogBytes -= sending->bytes.size();
        delete sending;
        (void)api.StreamShutdown(newestStream->handle, QUIC_STREAM_SHUTDOWN_FLAG_ABORT_SEND, 0);
      }
      else
        sent = true;
      state.newestStreams.push_back(std::move(newestStream));
    }
  }
  // Not in the list, so its last callback leaves it to this.
  if (unstarted != nullptr)
    api.StreamClose(unstarted);
  return sent;
}

std::vector<std::vector<std::byte>> Neuron::QuicChannel::ReceiveNewest()
{
  const std::scoped_lock lock(m_state->mutex);
  return std::exchange(m_state->newest, {});
}

std::vector<std::vector<std::byte>> Neuron::QuicChannel::Receive(std::chrono::milliseconds _wait)
{
  State& state = *m_state;
  std::unique_lock lock(state.mutex);
  if (_wait.count() > 0)
  {
    state.changed.wait_for(lock, _wait, [&state]
                           { return !state.messages.empty() || !state.newest.empty() || !state.failure.empty() || state.closedHere; });
  }
  if (!state.messages.empty())
    return std::exchange(state.messages, {});
  if (state.closedHere)
    throw Exception("The QUIC connection is closed.");
  if (!state.failure.empty())
    throw Exception(state.failure);
  return {};
}

void Neuron::QuicChannel::Shutdown(std::uint64_t _errorCode) noexcept
{
  m_state->ShutdownConnection(_errorCode);
}

void Neuron::QuicChannel::LimitMessages(std::uint32_t _bytes) noexcept
{
  m_state->messageLimit = _bytes;
}

std::optional<std::uint64_t> Neuron::QuicChannel::PeerErrorCode() const
{
  const std::scoped_lock lock(m_state->mutex);
  return m_state->peerErrorCode;
}

bool Neuron::QuicChannel::HasGone() const
{
  const std::scoped_lock lock(m_state->mutex);
  return !m_state->failure.empty();
}

Neuron::QuicStatistics Neuron::QuicChannel::Statistics() const
{
  State& state = *m_state;
  QuicStatistics statistics{.backlogBytes = state.backlogBytes};
  HQUIC connection = nullptr;
  {
    const std::scoped_lock lock(state.mutex);
    connection = state.connection;
  }
  if (connection == nullptr)
    return statistics;
  // Asked without the channel's lock, since MsQuic may wait for the connection's thread, whose callbacks take it.
  QUIC_STATISTICS_V2 quic{};
  auto bytes = static_cast<std::uint32_t>(sizeof(quic));
  if (QUIC_FAILED(state.runtime->Api().GetParam(connection, QUIC_PARAM_CONN_STATISTICS_V2, &bytes, &quic)))
    return statistics;
  statistics.roundTripMicroseconds = quic.Rtt;
  statistics.leastRoundTripMicroseconds = quic.MinRtt;
  statistics.packetsSent = quic.SendTotalPackets;
  statistics.packetsLost = quic.SendSuspectedLostPackets - std::min(quic.SendSpuriousLostPackets, quic.SendSuspectedLostPackets);
  statistics.congestionWindowBytes = quic.SendCongestionWindow;
  return statistics;
}

void Neuron::QuicChannel::Close() noexcept
{
  State& state = *m_state;
  HQUIC stream = nullptr;
  HQUIC connection = nullptr;
  HQUIC configuration = nullptr;
  std::vector<std::unique_ptr<State::NewestStream>> newestStreams;
  {
    const std::scoped_lock lock(state.mutex);
    stream = std::exchange(state.stream, nullptr);
    connection = std::exchange(state.connection, nullptr);
    configuration = std::exchange(state.configuration, nullptr);
    newestStreams = std::exchange(state.newestStreams, {});
    state.closedHere = true;
  }
  state.changed.notify_all();
  if (state.runtime == nullptr)
    return;
  const QUIC_API_TABLE& api = state.runtime->Api();
  // Each close waits for MsQuic's last callback for its handle, so the lock is not held for them. A newest message's stream
  // taken out of the channel's here is closed here, and its own last callback leaves it be.
  for (const std::unique_ptr<State::NewestStream>& newestStream : newestStreams)
    api.StreamClose(newestStream->handle);
  if (stream != nullptr)
    api.StreamClose(stream);
  if (connection != nullptr)
  {
    api.ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    api.ConnectionClose(connection);
  }
  if (configuration != nullptr)
    api.ConfigurationClose(configuration);
}

struct Neuron::QuicListener::State
{
  explicit State(const std::filesystem::path& _identity)
    : certificate(_identity)
  {
  }

  std::shared_ptr<QuicRuntime> runtime;
  ServerCertificate certificate;
  HQUIC configuration = nullptr;
  HQUIC listener = nullptr;
  std::uint16_t port = 0;
  std::uint32_t messageLimit = QuicChannel::MESSAGE_LIMIT_BYTES;
  Accept accept;
  // Guards the channels, which MsQuic's threads add to.
  std::mutex mutex;
  std::vector<std::shared_ptr<QuicChannel>> channels;

  ~State()
  {
    if (runtime == nullptr)
      return;
    const QUIC_API_TABLE& api = runtime->Api();
    // Waits until the listener has stopped, so that no channel is added after it.
    if (listener != nullptr)
      api.ListenerClose(listener);
    for (const std::shared_ptr<QuicChannel>& channel : channels)
      channel->Close();
    if (configuration != nullptr)
      api.ConfigurationClose(configuration);
  }

  static QUIC_STATUS QUIC_API OnListener(HQUIC _listener, void* _context, QUIC_LISTENER_EVENT* _event) noexcept;
};

QUIC_STATUS QUIC_API Neuron::QuicListener::State::OnListener([[maybe_unused]] HQUIC _listener, void* _context,
                                                             QUIC_LISTENER_EVENT* _event) noexcept
{
  if (_event->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION)
    return QUIC_STATUS_SUCCESS;
  State& state = *static_cast<State*>(_context);
  const QUIC_API_TABLE& api = state.runtime->Api();
  const HQUIC connection = _event->NEW_CONNECTION.Connection;
  try
  {
    auto channelState = std::make_unique<QuicChannel::State>();
    channelState->runtime = state.runtime;
    channelState->messageLimit = state.messageLimit;
    QuicChannel::State& own = *channelState;
    auto channel = std::make_shared<QuicChannel>(std::move(channelState));
    own.receiver = state.accept(channel);
    {
      const std::scoped_lock lock(state.mutex);
      state.channels.push_back(channel);
    }
    {
      // The channel owns the connection from here, and closes it. Until here a failure is returned, and MsQuic closes
      // the connection it refused.
      const std::scoped_lock lock(own.mutex);
      own.connection = connection;
    }
    api.SetCallbackHandler(connection, reinterpret_cast<void*>(&QuicChannel::State::OnConnection), &own);
    const QUIC_STATUS status = api.ConnectionSetConfiguration(connection, state.configuration);
    if (QUIC_FAILED(status))
      api.ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    return QUIC_STATUS_SUCCESS;
  }
  catch (...)
  {
    return QUIC_STATUS_INTERNAL_ERROR;
  }
}

Neuron::QuicListener::QuicListener(const Desc& _desc, Accept _accept)
  : m_state(std::make_unique<State>(_desc.identity))
{
  State& state = *m_state;
  state.runtime = std::make_shared<QuicRuntime>();
  state.accept = std::move(_accept);
  state.messageLimit = _desc.messageLimitBytes;
  const QUIC_API_TABLE& api = state.runtime->Api();

  QUIC_SETTINGS settings{};
  settings.HandshakeIdleTimeoutMs = SERVER_HANDSHAKE_TIMEOUT_MILLISECONDS;
  settings.IsSet.HandshakeIdleTimeoutMs = TRUE;
  // A client opens the connection's one stream, and the server opens none (ADR-060).
  settings.PeerBidiStreamCount = 1;
  settings.IsSet.PeerBidiStreamCount = TRUE;
  // As the client's: a send completes once the peer has acknowledged it (ADR-004 decision 6).
  settings.SendBufferingEnabled = FALSE;
  settings.IsSet.SendBufferingEnabled = TRUE;
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT;
  credential.Flags = QUIC_CREDENTIAL_FLAG_NONE;
  credential.CertificateContext = state.certificate.Context();
  state.configuration = OpenConfiguration(*state.runtime, _desc.applicationProtocol, settings, credential);

  Check(api.ListenerOpen(state.runtime->Registration(), &State::OnListener, &state, &state.listener), "ListenerOpen");
  // The loopback address, unless _desc names another, as an IPv4 or IPv6 literal or as every address. MsQuic matches a
  // connection to a listener of its own family, so a listener on 0.0.0.0 has none for a client that reaches it on IPv6, as
  // one naming localhost does; a listener of no family matches both.
  QUIC_ADDR address{};
  if (_desc.address.empty())
  {
    QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET);
    address.Ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  }
  else if (_desc.address == EVERY_ADDRESS)
    QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_UNSPEC);
  else if (inet_pton(AF_INET, _desc.address.c_str(), &address.Ipv4.sin_addr) == 1)
    QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET);
  else if (inet_pton(AF_INET6, _desc.address.c_str(), &address.Ipv6.sin6_addr) == 1)
    QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET6);
  else
    throw Exception(std::format("QuicListener: '{}' is not an IPv4 or IPv6 address to listen on, nor {}.", _desc.address, EVERY_ADDRESS));
  QuicAddrSetPort(&address, _desc.port);
  std::string protocolName = _desc.applicationProtocol;
  const QUIC_BUFFER protocol = ProtocolBuffer(protocolName);
  Check(api.ListenerStart(state.listener, &protocol, 1, &address), "ListenerStart");

  QUIC_ADDR bound{};
  auto boundBytes = static_cast<std::uint32_t>(sizeof(bound));
  Check(api.GetParam(state.listener, QUIC_PARAM_LISTENER_LOCAL_ADDRESS, &boundBytes, &bound), "GetParam");
  state.port = QuicAddrGetPort(&bound);
}

Neuron::QuicListener::~QuicListener() = default;

std::uint16_t Neuron::QuicListener::Port() const noexcept
{
  return m_state->port;
}

const Neuron::CertificateHash& Neuron::QuicListener::Certificate() const noexcept
{
  return m_state->certificate.Hash();
}

void Neuron::QuicListener::Sweep(std::chrono::milliseconds _silence, std::uint64_t _errorCode)
{
  State& state = *m_state;
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::shared_ptr<QuicChannel>> gone;
  std::vector<std::shared_ptr<QuicChannel>> silent;
  {
    const std::scoped_lock lock(state.mutex);
    // Nothing more arrives on a connection MsQuic has finished with, so no Receiver takes a new hold of its channel.
    const auto kept = std::stable_partition(state.channels.begin(), state.channels.end(), [](const std::shared_ptr<QuicChannel>& _channel)
                                            { return !_channel->m_state->HasShutDown() || _channel.use_count() > 1; });
    gone.assign(std::make_move_iterator(kept), std::make_move_iterator(state.channels.end()));
    state.channels.erase(kept, state.channels.end());
    for (const std::shared_ptr<QuicChannel>& channel : state.channels)
    {
      if (channel->m_state->IsSilent(now, _silence))
        silent.push_back(channel);
    }
  }
  // Outside the listener's lock, which MsQuic's threads take for each new connection: a shutdown takes the channel's own
  // lock, and the channels let go, as they are destroyed on the way out, wait for MsQuic's last callback for each.
  for (const std::shared_ptr<QuicChannel>& channel : silent)
    channel->Shutdown(_errorCode);
}

std::size_t Neuron::QuicListener::ChannelCount() const
{
  const std::scoped_lock lock(m_state->mutex);
  return m_state->channels.size();
}
