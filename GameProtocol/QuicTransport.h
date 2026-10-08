#pragma once

namespace Outpost
{
// Where a client finds a server that takes its players over QUIC, the certificate the server presents there, which is the
// only one the client accepts (ADR-060), and the token its seat takes (ADR-078).
struct ServerAddress
{
  std::string host;
  std::uint16_t port = 0;
  Neuron::CertificateHash certificate{};
  SeatToken token{};
};

// A client's connection to the server over QUIC (ADR-004, ADR-060): its commands go out and its snapshots come in as
// WireFormat's messages, reliably and in order.
class QuicTransport final : public Transport
{
public:
  // Connects to the server at _address, takes _player's seat there with the address's token, and returns once the server
  // has welcomed it. Throws Neuron::Exception when it cannot connect, or when the server refuses it, saying why.
  QuicTransport(const ServerAddress& _address, PlayerId _player);

  void Send(Command _command) override;
  // Throws Neuron::Exception once the connection has gone, saying why in the server's words when the server closed it
  // (DescribeClose), or when the server sends anything but a snapshot.
  [[nodiscard]] std::vector<Snapshot> Receive() override;

private:
  // The channel's messages, or, once the connection has gone, an exception that says why.
  [[nodiscard]] std::vector<std::vector<std::byte>> ReceiveFromChannel(std::chrono::milliseconds _wait = {});

  std::unique_ptr<Neuron::QuicChannel> m_channel;
  // Snapshots that arrived along with the welcome.
  std::vector<Snapshot> m_early;
};
} // namespace Outpost
