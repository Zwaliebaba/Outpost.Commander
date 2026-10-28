#include "pch.h"
#include "OverseerClient.h"

Outpost::OverseerClient::OverseerClient(OverseerTicket _ticket)
  : m_ticket(std::move(_ticket))
{
}

Outpost::OverseerClient::~OverseerClient() = default;

void Outpost::OverseerClient::Update(std::chrono::steady_clock::time_point _now)
{
  // A connection being made: once it is, it says hello, and the sight the overseer watches.
  if (m_connecting.valid() && m_connecting.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
  {
    try
    {
      m_channel = m_connecting.get();
      m_channel->Send(EncodeOverseerMessage(OverseerHelloMessage{.token = m_ticket.address.token}));
      if (m_sight.IsValid())
        m_channel->Send(EncodeOverseerMessage(WatchSightMessage{.seat = m_sight}));
      m_nextStatistics = _now;
    }
    catch (const Neuron::Exception& error)
    {
      Drop(error.what(), _now);
    }
  }
  else if (!m_connecting.valid() && !m_channel && _now >= m_nextJoin)
  {
    const Neuron::QuicChannel::Desc desc{.host = m_ticket.address.host,
                                         .port = m_ticket.address.port,
                                         .applicationProtocol = std::string(OVERSEER_APPLICATION_PROTOCOL),
                                         .serverCertificate = m_ticket.address.certificate,
                                         .connectTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(REJOIN_INTERVAL)};
    // The description goes to the thread as an argument, which std::async copies, rather than in the lambda's captures, as the
    // game's joins do.
    m_connecting =
      std::async(std::launch::async, [](const Neuron::QuicChannel::Desc& _desc) { return Neuron::QuicChannel::Connect(_desc); }, desc);
  }
  if (!m_channel)
    return;
  try
  {
    // The overseer's own stream first, so that a welcome is in before the views put together with it.
    for (const std::vector<std::byte>& message : m_channel->Receive())
      m_model.Take(message, _now);
    for (const std::vector<std::byte>& view : m_channel->ReceiveNewest())
    {
      if (const std::optional<std::uint64_t> held = m_model.TakeView(view))
        m_channel->Send(EncodeOverseerMessage(ViewHeldMessage{.tick = *held}));
    }
    if (_now >= m_nextStatistics)
    {
      m_roundTrip = std::chrono::microseconds{m_channel->Statistics().roundTripMicroseconds};
      m_nextStatistics = _now + STATISTICS_INTERVAL;
    }
  }
  catch (const Neuron::Exception& error)
  {
    // Gone, or broken: the server's close says why when it gave a reason of its own.
    const std::optional<std::uint64_t> code = m_channel->PeerErrorCode();
    const std::optional<std::string_view> described = code.has_value() ? DescribeOverseerClose(*code) : std::nullopt;
    Drop(described.has_value() ? std::string(*described) : std::string(error.what()), _now);
  }
}

void Outpost::OverseerClient::Watch(PlayerId _seat)
{
  m_sight = _seat;
  if (m_channel)
    m_channel->Send(EncodeOverseerMessage(WatchSightMessage{.seat = m_sight}));
}

void Outpost::OverseerClient::Drop(std::string _why, std::chrono::steady_clock::time_point _now)
{
  m_channel.reset();
  m_roundTrip.reset();
  m_model.Lost(std::move(_why), _now);
  m_nextJoin = _now + REJOIN_INTERVAL;
}
