#include "pch.h"
#include "QuicTransport.h"

#include <utility>

namespace
{
// How long the server may take to seat a client once it has connected.
constexpr std::chrono::milliseconds WELCOME_TIMEOUT{10'000};
} // namespace

bool Outpost::MayJoinAgain(const std::exception& _error) noexcept
{
  const auto* closed = dynamic_cast<const ServerClosed*>(&_error);
  if (closed == nullptr)
    return true;
  const CloseReason reason = closed->Reason();
  return reason != CloseReason::Finished && reason != CloseReason::WrongVersion && reason != CloseReason::SeatRefused &&
         reason != CloseReason::SeatTaken && reason != CloseReason::DroppedByOwner;
}

Outpost::QuicTransport::QuicTransport(const ServerAddress& _address, PlayerId _player, std::chrono::milliseconds _connectTimeout)
  : m_channel(Neuron::QuicChannel::Connect({.host = _address.host,
                                            .port = _address.port,
                                            .applicationProtocol = std::string(QUIC_APPLICATION_PROTOCOL),
                                            .serverCertificate = _address.certificate,
                                            .connectTimeout = _connectTimeout}))
{
  m_channel->Send(EncodeMessage(HelloMessage{.player = _player, .token = _address.token}));

  // A server that refuses the seat closes the connection, and Receive throws saying so.
  const auto deadline = std::chrono::steady_clock::now() + WELCOME_TIMEOUT;
  std::vector<std::vector<std::byte>> messages;
  while (messages.empty())
  {
    const auto left = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if (left.count() <= 0)
      throw Neuron::Exception(std::format("The server did not seat player {} within {} ms.", _player.value, WELCOME_TIMEOUT.count()));
    messages = ReceiveFromChannel(left);
  }

  const Message first = DecodeMessage(messages.front());
  const auto* welcome = std::get_if<WelcomeMessage>(&first);
  if (welcome == nullptr || welcome->player != _player)
    throw Neuron::Exception(std::format("The server did not welcome player {} to its seat.", _player.value));
  for (std::size_t i = 1; i < messages.size(); ++i)
    TakeOrdered(messages[i]);
}

void Outpost::QuicTransport::Send(Command _command)
{
  m_channel->Send(EncodeMessage(_command));
}

std::vector<Outpost::Snapshot> Outpost::QuicTransport::Receive()
{
  for (const std::vector<std::byte>& message : ReceiveFromChannel())
    TakeOrdered(message);
  std::vector<Snapshot> snapshots;
  if (m_waiting.has_value() && m_waiting->orderedSent <= m_orderedTaken)
  {
    StateMessage waiting = std::move(*m_waiting);
    m_waiting.reset();
    TakeState(std::move(waiting), snapshots);
  }
  for (const std::vector<std::byte>& message : m_channel->ReceiveNewest())
  {
    StateMessage state = m_held.Take(DecodeMessage(message));
    // The server sends the next states as changes of this one (ADR-098).
    m_channel->Send(EncodeMessage(HeldMessage{.tick = state.snapshot.tick}));
    TakeState(std::move(state), snapshots);
  }
  return snapshots;
}

void Outpost::QuicTransport::TakeOrdered(std::span<const std::byte> _message)
{
  Message decoded = DecodeMessage(_message);
  if (auto* catalog = std::get_if<CatalogMessage>(&decoded))
    m_catalog = std::move(*catalog);
  else if (auto* events = std::get_if<EventsMessage>(&decoded))
    m_events.push_back(std::move(*events));
  else
    throw Neuron::Exception("The server sent a message that is neither a catalogue nor a tick's events.");
  ++m_orderedTaken;
}

void Outpost::QuicTransport::TakeState(StateMessage _state, std::vector<Snapshot>& _snapshots)
{
  // A state waits for what was sent before it: the catalogue it was built against, and the events of its tick. One built
  // against an older catalogue than the newest is put together with the newest, which is newer by no more ticks than the
  // state is late.
  if (_state.orderedSent > m_orderedTaken)
  {
    m_waiting = std::move(_state);
    return;
  }
  if (!m_catalog.has_value())
    throw Neuron::Exception("The server sent a state before its catalogue.");
  Snapshot snapshot = std::move(_state.snapshot);
  PutCatalog(*m_catalog, snapshot);
  const auto handedOn = std::ranges::find_if(m_events, [&snapshot](const EventsMessage& _events) { return _events.tick > snapshot.tick; });
  for (auto events = m_events.begin(); events != handedOn; ++events)
    PutEvents(std::move(*events), snapshot);
  m_events.erase(m_events.begin(), handedOn);
  _snapshots.push_back(std::move(snapshot));
}

Neuron::QuicStatistics Outpost::QuicTransport::Statistics() const
{
  return m_channel->Statistics();
}

std::vector<std::vector<std::byte>> Outpost::QuicTransport::ReceiveFromChannel(std::chrono::milliseconds _wait)
{
  try
  {
    return m_channel->Receive(_wait);
  }
  catch (const Neuron::Exception&)
  {
    // The server's own reason, when it closed the connection with one.
    const std::optional<std::uint64_t> code = m_channel->PeerErrorCode();
    const std::optional<std::string_view> reason = code.has_value() ? DescribeClose(*code) : std::nullopt;
    if (!code.has_value() || !reason.has_value())
      throw;
    throw ServerClosed(static_cast<CloseReason>(*code), std::string(*reason));
  }
}
