#pragma once

namespace Neuron
{
// A SHA-256 digest of a certificate's DER encoding. A client knows the server it means by it: the certificate is pinned,
// not validated against a root (ADR-060).
using CertificateHash = std::array<std::uint8_t, 32>;

// The error code a channel closes its connection with when the peer breaks the framing below, such as with a message over
// the limit. Codes below it are the caller's.
inline constexpr std::uint64_t FRAMING_ERROR_CODE = 0x100;

// One QUIC connection that carries messages both ways, reliably and in order, on one bidirectional stream (ADR-004,
// ADR-060). A message is a byte string, and what it means is the caller's: this knows no game concept (R9). MsQuic runs
// on threads of its own, so every member may be called from any thread, apart from where it says otherwise.
//
// MsQuic is included only from the .cpp (ADR-004), so a channel's state is defined there.
class QuicChannel : NonCopyable
{
public:
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
    std::chrono::milliseconds connectTimeout{10'000};
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

  // The messages that arrived since the last call, oldest first. With a wait, it waits up to that long for the first one.
  // Once the connection has gone, whichever end closed it, it throws Neuron::Exception saying why, after the messages that
  // arrived before then. A server's end hands its messages to its Receiver instead, so this returns none there.
  [[nodiscard]] std::vector<std::vector<std::byte>> Receive(std::chrono::milliseconds _wait = {});

  // Starts closing the connection, with an error code the peer sees, and returns at once. A Receiver may call it.
  void Shutdown(std::uint64_t _errorCode) noexcept;

  // Closes the connection and waits for MsQuic's last callback for it. Never from a Receiver. The destructor closes a
  // channel that is still open.
  void Close() noexcept;

private:
  std::unique_ptr<State> m_state;
};

// A QUIC server: it listens on the loopback address and makes a QuicChannel of every client that connects. It presents a
// self-signed certificate of its own, which a client pins (ADR-060).
class QuicListener : NonCopyable
{
public:
  struct Desc
  {
    std::string applicationProtocol;
    // Zero lets the system choose one; Port says which.
    std::uint16_t port = 0;
  };

  // What the listener does with a client that has connected, on MsQuic's thread: given its channel, it returns the
  // Receiver its messages go to.
  using Accept = std::function<QuicChannel::Receiver(const std::shared_ptr<QuicChannel>&)>;

  struct State;

  // Listens on 127.0.0.1 only. Throws Neuron::Exception when it cannot.
  QuicListener(const Desc& _desc, Accept _accept);
  // Stops listening and closes every channel it made, waiting for their last callbacks.
  ~QuicListener();

  [[nodiscard]] std::uint16_t Port() const noexcept;
  [[nodiscard]] const CertificateHash& Certificate() const noexcept;

private:
  std::unique_ptr<State> m_state;
};
} // namespace Neuron
