#include "pch.h"
#include "QuicChannel.h"
#include "ServerCertificate.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

// After pch.h, which sets the Windows macro family before msquic.h includes <windows.h> (ADR-004).
#include <msquic.h>

namespace
{
// The longest message either end accepts. The stress scene's snapshot is far shorter, so a longer length is a broken
// stream, not a message.
constexpr std::uint32_t MESSAGE_LIMIT_BYTES = 64u * 1024u * 1024u;
// Each message on the stream is preceded by its length in bytes, in four bytes, little-endian (ADR-060).
constexpr std::size_t LENGTH_BYTES = 4;
// How long a client's handshake with the listener may take.
constexpr std::uint64_t SERVER_HANDSHAKE_TIMEOUT_MILLISECONDS = 10'000;
// How much longer Connect waits than the handshake may take, so that MsQuic's own timeout is the one that ends it.
constexpr std::chrono::milliseconds CONNECT_GRACE{1'000};

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

// A message on its way. MsQuic reads the buffer until it reports the send complete, and the stream's callback deletes it
// then.
struct PendingSend
{
  std::vector<std::byte> bytes;
  QUIC_BUFFER buffer{};
};
} // namespace

struct Neuron::QuicChannel::State
{
  std::shared_ptr<QuicRuntime> runtime;
  QuicChannel* owner = nullptr;
  // The server's end only: where each message goes.
  Receiver receiver;
  // The client's end only: the server's certificate, which is the only one it accepts.
  std::optional<CertificateHash> pinned;
  // The bytes of a message not yet whole, and whether the peer broke the framing, after which nothing more is read. Only
  // the connection's MsQuic thread touches them, and it delivers one callback at a time.
  std::vector<std::byte> partial;
  bool broken = false;

  // Guards everything below. Nothing holds it while it calls a Receiver or waits for MsQuic.
  std::mutex mutex;
  std::condition_variable changed;
  // The client's own configuration; a server's end uses its listener's.
  HQUIC configuration = nullptr;
  HQUIC connection = nullptr;
  HQUIC stream = nullptr;
  std::vector<std::vector<std::byte>> messages;
  bool connected = false;
  bool closedHere = false;
  // Why the connection went, once it has.
  std::string failure;

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

  // The client opens the connection's one stream, and the server's end adopts it here (ADR-060).
  void AdoptStream(const QUIC_CONNECTION_EVENT& _event)
  {
    const HQUIC peerStream = _event.PEER_STREAM_STARTED.Stream;
    bool adopted = false;
    {
      const std::scoped_lock lock(mutex);
      if (stream == nullptr)
      {
        stream = peerStream;
        adopted = true;
      }
    }
    const QUIC_API_TABLE& api = runtime->Api();
    if (adopted)
    {
      api.SetCallbackHandler(peerStream, reinterpret_cast<void*>(&OnStream), this);
      return;
    }
    // The listener lets a client open one stream only, so this does not happen; if it did, the stream is refused.
    api.SetCallbackHandler(peerStream, reinterpret_cast<void*>(&OnRefusedStream), this);
    (void)api.StreamShutdown(peerStream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, FRAMING_ERROR_CODE);
  }

  // Adds what arrived to the message being read, and passes on every message that is now whole.
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
      if (length > MESSAGE_LIMIT_BYTES)
        throw Exception(std::format("The peer sent a message of {} bytes, over the limit of {}.", length, MESSAGE_LIMIT_BYTES));
      if (partial.size() - offset - LENGTH_BYTES < length)
        break;
      const auto first = partial.begin() + static_cast<std::ptrdiff_t>(offset + LENGTH_BYTES);
      whole.emplace_back(first, first + length);
      offset += LENGTH_BYTES + length;
    }
    partial.erase(partial.begin(), partial.begin() + static_cast<std::ptrdiff_t>(offset));

    if (receiver)
    {
      for (std::vector<std::byte>& message : whole)
        receiver(*owner, std::move(message));
      return;
    }
    if (whole.empty())
      return;
    {
      const std::scoped_lock lock(mutex);
      for (std::vector<std::byte>& message : whole)
        messages.push_back(std::move(message));
    }
    changed.notify_all();
  }

  static QUIC_STATUS QUIC_API OnConnection(HQUIC _connection, void* _context, QUIC_CONNECTION_EVENT* _event) noexcept;
  static QUIC_STATUS QUIC_API OnStream(HQUIC _stream, void* _context, QUIC_STREAM_EVENT* _event) noexcept;
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
    }
      state.changed.notify_all();
      break;
    case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED:
      return state.CheckCertificate(*_event);
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      state.AdoptStream(*_event);
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
      state.Fail(std::format("The QUIC connection failed with status {}.", StatusText(_event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status)));
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
      state.Fail(std::format("The peer closed the QUIC connection with error code {}.", _event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode));
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
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
      delete static_cast<PendingSend*>(_event->SEND_COMPLETE.ClientContext);
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
  const std::scoped_lock lock(state.mutex);
  if (state.stream == nullptr)
    return;
  // MsQuic holds the send from here until it reports it complete, and the stream's callback deletes it then. A send it
  // refuses is never reported.
  PendingSend* sending = pending.release();
  if (QUIC_FAILED(state.runtime->Api().StreamSend(state.stream, &sending->buffer, 1, QUIC_SEND_FLAG_NONE, sending)))
    delete sending;
}

std::vector<std::vector<std::byte>> Neuron::QuicChannel::Receive(std::chrono::milliseconds _wait)
{
  State& state = *m_state;
  std::unique_lock lock(state.mutex);
  if (_wait.count() > 0)
    state.changed.wait_for(lock, _wait, [&state] { return !state.messages.empty() || !state.failure.empty() || state.closedHere; });
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

void Neuron::QuicChannel::Close() noexcept
{
  State& state = *m_state;
  HQUIC stream = nullptr;
  HQUIC connection = nullptr;
  HQUIC configuration = nullptr;
  {
    const std::scoped_lock lock(state.mutex);
    stream = std::exchange(state.stream, nullptr);
    connection = std::exchange(state.connection, nullptr);
    configuration = std::exchange(state.configuration, nullptr);
    state.closedHere = true;
  }
  state.changed.notify_all();
  if (state.runtime == nullptr)
    return;
  const QUIC_API_TABLE& api = state.runtime->Api();
  // Each close waits for MsQuic's last callback for its handle, so the lock is not held for them.
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
  std::shared_ptr<QuicRuntime> runtime;
  ServerCertificate certificate;
  HQUIC configuration = nullptr;
  HQUIC listener = nullptr;
  std::uint16_t port = 0;
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
  : m_state(std::make_unique<State>())
{
  State& state = *m_state;
  state.runtime = std::make_shared<QuicRuntime>();
  state.accept = std::move(_accept);
  const QUIC_API_TABLE& api = state.runtime->Api();

  QUIC_SETTINGS settings{};
  settings.HandshakeIdleTimeoutMs = SERVER_HANDSHAKE_TIMEOUT_MILLISECONDS;
  settings.IsSet.HandshakeIdleTimeoutMs = TRUE;
  // A client opens the connection's one stream, and the server opens none (ADR-060).
  settings.PeerBidiStreamCount = 1;
  settings.IsSet.PeerBidiStreamCount = TRUE;
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT;
  credential.Flags = QUIC_CREDENTIAL_FLAG_NONE;
  credential.CertificateContext = state.certificate.Context();
  state.configuration = OpenConfiguration(*state.runtime, _desc.applicationProtocol, settings, credential);

  Check(api.ListenerOpen(state.runtime->Registration(), &State::OnListener, &state, &state.listener), "ListenerOpen");
  QUIC_ADDR address{};
  QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET);
  address.Ipv4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
