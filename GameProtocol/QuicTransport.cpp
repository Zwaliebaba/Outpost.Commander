#include "pch.h"
#include "QuicTransport.h"

#include <utility>

namespace
{
// How long the server may take to seat a client once it has connected.
constexpr std::chrono::milliseconds WELCOME_TIMEOUT{10'000};

Outpost::Snapshot ToSnapshot(Outpost::Message _message)
{
  if (auto* snapshot = std::get_if<Outpost::Snapshot>(&_message))
    return std::move(*snapshot);
  throw Neuron::Exception("The server sent a message that is not a snapshot.");
}
} // namespace

Outpost::QuicTransport::QuicTransport(const ServerAddress& _address, PlayerId _player)
  : m_channel(Neuron::QuicChannel::Connect({.host = _address.host,
                                            .port = _address.port,
                                            .applicationProtocol = std::string(QUIC_APPLICATION_PROTOCOL),
                                            .serverCertificate = _address.certificate}))
{
  m_channel->Send(EncodeMessage(HelloMessage{.player = _player}));

  // A server that refuses the seat closes the connection, and Receive throws saying so.
  const auto deadline = std::chrono::steady_clock::now() + WELCOME_TIMEOUT;
  std::vector<std::vector<std::byte>> messages;
  while (messages.empty())
  {
    const auto left = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if (left.count() <= 0)
      throw Neuron::Exception(std::format("The server did not seat player {} within {} ms.", _player.value, WELCOME_TIMEOUT.count()));
    messages = m_channel->Receive(left);
  }

  const Message first = DecodeMessage(messages.front());
  const auto* welcome = std::get_if<WelcomeMessage>(&first);
  if (welcome == nullptr || welcome->player != _player)
    throw Neuron::Exception(std::format("The server did not welcome player {} to its seat.", _player.value));
  for (std::size_t i = 1; i < messages.size(); ++i)
    m_early.push_back(ToSnapshot(DecodeMessage(messages[i])));
}

void Outpost::QuicTransport::Send(Command _command)
{
  m_channel->Send(EncodeMessage(_command));
}

std::vector<Outpost::Snapshot> Outpost::QuicTransport::Receive()
{
  std::vector<Snapshot> snapshots = std::exchange(m_early, {});
  for (const std::vector<std::byte>& message : m_channel->Receive())
    snapshots.push_back(ToSnapshot(DecodeMessage(message)));
  return snapshots;
}
