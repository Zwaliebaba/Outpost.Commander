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

// A client's connection to the server over QUIC (ADR-004, ADR-060): its commands go out as WireFormat's messages, and the
// server's catalogues and each tick's events come in reliably and in order, and each tick's state on a stream of its own, of
// which only the newest matters (ADR-096). It hands on a snapshot for each state that arrives: the state with the last
// catalogue put back into it (ADR-060 decision 9), and the events of every tick up to its own that have not been handed on.
// A state waits for the catalogues and events sent before it, giving way to a newer state while it does.
class QuicTransport final : public Transport
{
public:
  // Connects to the server at _address, takes _player's seat there with the address's token, and returns once the server
  // has welcomed it, giving the handshake _connectTimeout (ADR-078 decision 9). Throws Neuron::Exception when it cannot
  // connect, or when the server refuses it, saying why.
  QuicTransport(const ServerAddress& _address, PlayerId _player,
                std::chrono::milliseconds _connectTimeout = Neuron::QuicChannel::CONNECT_TIMEOUT);

  void Send(Command _command) override;
  // Throws Neuron::Exception once the connection has gone, saying why in the server's words when the server closed it
  // (DescribeClose), or when the server sends a message that is not the one its stream carries, or a state before a
  // catalogue.
  [[nodiscard]] std::vector<Snapshot> Receive() override;

  // How the connection to the server does (ADR-004 decision 6).
  [[nodiscard]] Neuron::QuicStatistics Statistics() const;

private:
  // The channel's messages, or, once the connection has gone, an exception that says why.
  [[nodiscard]] std::vector<std::vector<std::byte>> ReceiveFromChannel(std::chrono::milliseconds _wait = {});

  // Keeps a catalogue the server sent, or a tick's events until a state takes them.
  void TakeOrdered(std::span<const std::byte> _message);

  // Adds _state to _snapshots as its snapshot, once the catalogues and events sent before it have come; until then it waits.
  void TakeState(StateMessage _state, std::vector<Snapshot>& _snapshots);

  std::unique_ptr<Neuron::QuicChannel> m_channel;
  std::optional<CatalogMessage> m_catalog;
  // How many catalogues and events have come, which a state names; the ticks' events not yet handed on, oldest first; and
  // the newest state waiting for what was sent before it.
  std::uint32_t m_orderedTaken = 0;
  std::vector<EventsMessage> m_events;
  std::optional<StateMessage> m_waiting;
};
} // namespace Outpost
