#pragma once

namespace Neuron
{
// A SHA-256 digest of a certificate's DER encoding. A client knows the server it means by it: the certificate is pinned,
// not validated against a root (ADR-060).
using CertificateHash = std::array<std::uint8_t, 32>;

// The error code a channel closes its connection with when the peer breaks the framing below, such as with a message over
// the limit. Codes below it are the caller's.
inline constexpr std::uint64_t FRAMING_ERROR_CODE = 0x100;

// How one connection does, as MsQuic reports it, and what its channel has handed MsQuic that the peer has not yet
// acknowledged (ADR-004 decision 6).
struct QuicStatistics
{
  // The smoothed round trip, and the least of it since the connection began.
  std::uint32_t roundTripMicroseconds = 0;
  std::uint32_t leastRoundTripMicroseconds = 0;
  // The QUIC packets sent since the connection began, and those of them that were lost: suspected lost and not found again.
  std::uint64_t packetsSent = 0;
  std::uint64_t packetsLost = 0;
  // How many bytes the connection may have in flight before it waits for acknowledgements.
  std::uint32_t congestionWindowBytes = 0;
  // The bytes of messages the channel has handed MsQuic that the peer has not yet acknowledged, those still waiting to be sent
  // and those on their way.
  std::uint64_t backlogBytes = 0;
};

// One QUIC connection that carries messages both ways, reliably and in order, on one bidirectional stream (ADR-004,
// ADR-060), and from the server to the client messages of which only the newest matters, each on a unidirectional stream of
// its own (ADR-096). A message is a byte string, and what it means is the caller's: this knows no game concept (R9). MsQuic
// runs on threads of its own, so every member may be called from any thread, apart from where it says otherwise.
//
// MsQuic is included only from the .cpp (ADR-004), so a channel's state is defined there.
class QuicChannel : NonCopyable
{
public:
  // How long a client's handshake may take before it gives up, unless its Desc says otherwise.
  static constexpr std::chrono::milliseconds CONNECT_TIMEOUT{10'000};
  // The longest message a channel takes from its peer unless it is told a shorter one (LimitMessages,
  // QuicListener::Desc). The stress scene's snapshot is far shorter, so a longer length is a broken stream, not a message.
  static constexpr std::uint32_t MESSAGE_LIMIT_BYTES = 64u * 1024u * 1024u;
  // How many messages of which only the newest matters a client lets its server have on their way at once (SendNewest).
  static constexpr std::uint16_t NEWEST_STREAMS = 16;

  struct Desc
  {
    // The server's address and port.
    std::string host;
    std::uint16_t port = 0;
    // The application protocol both ends negotiate.
    std::string applicationProtocol;
    // The only certificate the client accepts from the server.
    CertificateHash serverCertificate{};
    // How long the handshake may take before the client gives up.
    std::chrono::milliseconds connectTimeout = CONNECT_TIMEOUT;
  };

  // What the server's end of a connection does with each message, on MsQuic's thread, in the order the client sent them:
  // it is given the channel and the message.
  // An exception it throws shuts the connection down with FRAMING_ERROR_CODE. It may call Send and Shutdown, but not Close.
  using Receiver = std::function<void(QuicChannel&, std::vector<std::byte>)>;

  struct State;

  // Connects a client and returns once the handshake is complete and its stream is open. Throws Neuron::Exception when it
  // cannot connect, or when the server presents another certificate than _desc's.
  [[nodiscard]] static std::unique_ptr<QuicChannel> Connect(const Desc& _desc);

  // For QuicListener and Connect: a channel over the connection _state holds.
  explicit QuicChannel(std::unique_ptr<State> _state) noexcept;
  ~QuicChannel();

  // Queues a message for the peer and returns at once. A message on a connection that has gone is dropped: Receive says
  // why. Throws Neuron::Exception for a message over the limit.
  void Send(std::span<const std::byte> _message);

  // The messages that arrived since the last call, oldest first. With a wait, it waits up to that long for the first one,
  // or for one of ReceiveNewest's. Once the connection has gone, whichever end closed it, it throws Neuron::Exception saying
  // why, after the messages that arrived before then. A server's end hands its messages to its Receiver instead, so this
  // returns none there.
  [[nodiscard]] std::vector<std::vector<std::byte>> Receive(std::chrono::milliseconds _wait = {});

  // Queues a message of which only the newest matters to the peer, on a unidirectional stream of its own, and returns at once
  // whether it did. While four such messages are on their way, the peer having acknowledged none of them whole, it is
  // dropped instead, so that nothing sent is abandoned; and the bidirectional stream's messages go first when both have something to
  // send. A message on a connection that has gone is dropped too, as is one the peer allows no stream for, NEWEST_STREAMS
  // being open, though that one is found only once this has returned that it queued it. Only a server's end sends them,
  // since only a client lets its peer open such streams. Throws Neuron::Exception for a message over the limit. Never from a
  // Receiver.
  [[nodiscard]] bool SendNewest(std::span<const std::byte> _message);

  // The messages of which only the newest matters that arrived whole since the last call, oldest first, each newer than the
  // one before it: one that arrives after a newer one is dropped. A client's end receives them, and a server's none.
  [[nodiscard]] std::vector<std::vector<std::byte>> ReceiveNewest();

  // Starts closing the connection, with an error code the peer sees, and returns at once. A Receiver may call it.
  void Shutdown(std::uint64_t _errorCode) noexcept;

  // The longest message the peer may send from here on; a longer one breaks the framing. Only from the channel's Receiver,
  // which is handed the peer's messages one at a time, so that the limit holds from the message after the one it was given.
  void LimitMessages(std::uint32_t _bytes) noexcept;

  // The error code the peer closed the connection with, once it has; none while it is open, or when it went otherwise.
  [[nodiscard]] std::optional<std::uint64_t> PeerErrorCode() const;

  // Whether the connection has gone, whichever end closed it and however: a peer that vanished is gone once MsQuic's idle
  // timeout says so.
  [[nodiscard]] bool HasGone() const;

  // How the connection does now; all but the backlog are zero once the channel is closed. It asks MsQuic, which may wait for
  // the connection's own thread, so never from a Receiver, and not while another thread closes the channel.
  [[nodiscard]] QuicStatistics Statistics() const;

  // Closes the connection and waits for MsQuic's last callback for it. Never from a Receiver. The destructor closes a
  // channel that is still open.
  void Close() noexcept;

private:
  // The listener makes the channels of the connections it is given, and lets them go (QuicListener::Sweep).
  friend class QuicListener;

  std::unique_ptr<State> m_state;
};

// A QUIC server: it listens on an address and makes a QuicChannel of every client that connects. It presents a self-signed
// certificate, which a client pins (ADR-060): one of its own, or one it keeps in a folder from one run to the next (ADR-078).
class QuicListener : NonCopyable
{
public:
  // The address that listens on every interface of both IPv4 and IPv6 (ADR-078 decision 6).
  static constexpr std::string_view EVERY_ADDRESS = "*";

  struct Desc
  {
    std::string applicationProtocol;
    // The IPv4 or IPv6 address to listen on, such as "0.0.0.0" for every interface of IPv4 alone, or EVERY_ADDRESS for
    // every interface of both; empty for 127.0.0.1 only (ADR-060).
    std::string address;
    // Zero lets the system choose one; Port says which.
    std::uint16_t port = 0;
    // A folder the listener keeps its certificate in, so that it presents the same one each time it is made there and a
    // client pins it once (ADR-078); empty for a certificate of its own that goes with it.
    std::filesystem::path identity;
    // The longest message a client may send until its Receiver says otherwise (QuicChannel::LimitMessages).
    std::uint32_t messageLimitBytes = QuicChannel::MESSAGE_LIMIT_BYTES;
  };

  // What the listener does with a client that has connected, on MsQuic's thread: given its channel, it returns the
  // Receiver its messages go to.
  using Accept = std::function<QuicChannel::Receiver(const std::shared_ptr<QuicChannel>&)>;

  struct State;

  // Listens on _desc's address. Throws Neuron::Exception when it cannot, or when the address is not one.
  QuicListener(const Desc& _desc, Accept _accept);
  // Stops listening and closes every channel it made, waiting for their last callbacks.
  ~QuicListener();

  [[nodiscard]] std::uint16_t Port() const noexcept;
  [[nodiscard]] const CertificateHash& Certificate() const noexcept;

  // Shuts down, with _errorCode, every connection that completed its handshake more than _silence ago and has sent no
  // message since, and lets go of every channel whose connection MsQuic has finished with and that nothing else holds.
  // Never from a Receiver or any other of MsQuic's threads: letting a channel go waits for MsQuic's last callback for it.
  void Sweep(std::chrono::milliseconds _silence, std::uint64_t _errorCode);

  // How many channels it holds: every connection it was given that it has not let go.
  [[nodiscard]] std::size_t ChannelCount() const;

private:
  std::unique_ptr<State> m_state;
};
} // namespace Neuron
