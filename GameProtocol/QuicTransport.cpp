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
    m_early.push_back(ToSnapshot(DecodeMessage(messages[i])));
}

void Outpost::QuicTransport::Send(Command _command)
{
  m_channel->Send(EncodeMessage(_command));
}

std::vector<Outpost::Snapshot> Outpost::QuicTransport::Receive()
{
  std::vector<Snapshot> snapshots = std::exchange(m_early, {});
  for (const std::vector<std::byte>& message : ReceiveFromChannel())
    snapshots.push_back(ToSnapshot(DecodeMessage(message)));
  return snapshots;
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
    if (!reason.has_value())
      throw;
    throw Neuron::Exception(std::string(*reason));
  }
}
