#include "pch.h"
#include "OverseerLink.h"

#include <thread>

namespace
{
// How often SendClosing looks whether the overseer has had all that was sent.
constexpr std::chrono::milliseconds CLOSING_POLL{10};

// Whether two tokens are the same, looking at every byte whatever the first difference, so that how long a refusal takes
// says nothing of the token.
bool SameToken(const Outpost::SeatToken& _a, const Outpost::SeatToken& _b) noexcept
{
  std::uint8_t differs = 0;
  for (std::size_t i = 0; i < _a.size(); ++i)
    differs = static_cast<std::uint8_t>(differs | (_a[i] ^ _b[i]));
  return differs == 0;
}
} // namespace

Outpost::OverseerLink::OverseerLink(const OverseerDesc& _desc, const std::filesystem::path& _identity, OverseerWelcomeMessage _welcome,
                                    std::deque<std::string> _lanes, std::vector<std::string> _records)
  : m_token(_desc.token),
    m_welcome(std::move(_welcome)),
    m_lanes(std::move(_lanes))
{
  m_welcome.laneRecords.clear();
  for (const OverseerSeat& seat : m_welcome.seats)
    m_seats.push_back(seat.player);
  // The day the lanes cover ends at the newest record written, among them and the log's last records.
  for (const std::string& lane : m_lanes)
    m_newestTick = std::max(m_newestTick, HeadOf(lane).value_or(RecordHead{}).tick);
  for (const std::string& record : _records)
    m_newestTick = std::max(m_newestTick, HeadOf(record).value_or(RecordHead{}).tick);
  for (std::string& record : _records)
    m_records.push_back(std::move(record));
  while (m_records.size() > OVERSEER_RECORDS_KEPT)
    m_records.pop_front();
  // Last, since an overseer may connect as soon as it listens.
  m_listener =
    std::make_unique<Neuron::QuicListener>(Neuron::QuicListener::Desc{.applicationProtocol = std::string(OVERSEER_APPLICATION_PROTOCOL),
                                                                      .address = _desc.address,
                                                                      .port = _desc.port,
                                                                      .identity = _identity,
                                                                      .messageLimitBytes = OVERSEER_MESSAGE_LIMIT_BYTES},
                                           [this](const std::shared_ptr<Neuron::QuicChannel>& _channel) { return Admit(_channel); });
}

Outpost::OverseerLink::~OverseerLink() = default;

std::uint16_t Outpost::OverseerLink::Port() const noexcept
{
  return m_listener->Port();
}

const Neuron::CertificateHash& Outpost::OverseerLink::Certificate() const noexcept
{
  return m_listener->Certificate();
}

Neuron::QuicChannel::Receiver Outpost::OverseerLink::Admit(const std::shared_ptr<Neuron::QuicChannel>& _channel)
{
  // MsQuic hands the receiver one message at a time, so what it keeps between them needs no lock.
  return [this, connection = std::weak_ptr(_channel), welcomed = false](Neuron::QuicChannel& _peer, std::vector<std::byte> _message) mutable
  {
    try
    {
      if (!welcomed)
      {
        // A hello of another build may not decode at all, so its version and its messages' layout are read first.
        const std::optional<std::uint32_t> version = PeekOverseerHelloVersion(_message);
        const std::optional<std::uint64_t> layout = PeekOverseerHelloLayout(_message);
        if (version.has_value() && (*version != OVERSEER_PROTOCOL_VERSION || layout != OverseerLayoutHash()))
        {
          Refuse(_peer, OverseerOutcome::WrongVersion, OverseerCloseReason::WrongVersion);
          return;
        }
        const OverseerMessage message = DecodeOverseerMessage(_message);
        const auto* hello = std::get_if<OverseerHelloMessage>(&message);
        if (hello == nullptr)
        {
          Refuse(_peer, OverseerOutcome::Malformed, OverseerCloseReason::MalformedMessage);
          return;
        }
        if (!SameToken(hello->token, m_token))
        {
          Refuse(_peer, OverseerOutcome::TokenRefused, OverseerCloseReason::TokenRefused);
          return;
        }
        Welcome(connection.lock());
        welcomed = true;
        return;
      }
      // An overseer the channel was taken from says nothing more to the server.
      if (!Holds(&_peer))
        return;
      const OverseerMessage message = DecodeOverseerMessage(_message);
      if (const auto* sight = std::get_if<WatchSightMessage>(&message))
      {
        if (sight->seat.IsValid() && !std::ranges::contains(m_seats, sight->seat))
        {
          Refuse(_peer, OverseerOutcome::Malformed, OverseerCloseReason::MalformedMessage);
          return;
        }
        const std::scoped_lock lock(m_mutex);
        m_sight = sight->seat;
        return;
      }
      if (const auto* held = std::get_if<ViewHeldMessage>(&message))
      {
        const std::scoped_lock lock(m_mutex);
        m_held = std::max(m_held.value_or(0), held->tick);
        return;
      }
      Refuse(_peer, OverseerOutcome::Malformed, OverseerCloseReason::MalformedMessage);
    }
    catch (const Neuron::Exception&)
    {
      Refuse(_peer, OverseerOutcome::Malformed, OverseerCloseReason::MalformedMessage);
    }
  };
}

void Outpost::OverseerLink::Welcome(std::shared_ptr<Neuron::QuicChannel> _channel)
{
  if (!_channel)
    return;
  std::vector<std::byte> records;
  std::shared_ptr<Neuron::QuicChannel> replaced;
  {
    const std::scoped_lock lock(m_mutex);
    // The lanes' records as of the newest record written, made into the welcome here, on MsQuic's thread.
    KeepLaneRecords(m_lanes, LaneDayStart());
    OverseerWelcomeMessage welcome = m_welcome;
    welcome.laneRecords.assign(m_lanes.begin(), m_lanes.end());
    _channel->Send(EncodeOverseerMessage(welcome));
    if (!m_records.empty())
      _channel->Send(EncodeOverseerMessage(LogRecordsMessage{.lines = std::vector<std::string>(m_records.begin(), m_records.end())}));
    m_comings.push_back({.outcome = OverseerOutcome::Opened, .address = _channel->PeerAddress()});
    // One overseer at a time, the newest: the one before it is closed, and its sight and views go with it.
    replaced = std::exchange(m_overseer, std::move(_channel));
    ++m_takings;
    m_sight = {};
    m_held.reset();
    if (replaced)
      m_comings.push_back({.outcome = OverseerOutcome::Closed, .address = replaced->PeerAddress()});
  }
  // Outside the lock, which the replaced connection's own callbacks take.
  if (replaced)
    replaced->Shutdown(static_cast<std::uint64_t>(OverseerCloseReason::Taken));
}

bool Outpost::OverseerLink::Holds(const Neuron::QuicChannel* _channel) const
{
  const std::scoped_lock lock(m_mutex);
  return m_overseer.get() == _channel;
}

void Outpost::OverseerLink::Refuse(Neuron::QuicChannel& _channel, OverseerOutcome _outcome, OverseerCloseReason _reason)
{
  {
    const std::scoped_lock lock(m_mutex);
    m_comings.push_back({.outcome = _outcome, .address = _channel.PeerAddress()});
  }
  _channel.Shutdown(static_cast<std::uint64_t>(_reason));
}

std::vector<Outpost::OverseerRecord> Outpost::OverseerLink::Sweep()
{
  const std::vector<std::string> silent =
    m_listener->Sweep(HELLO_TIMEOUT, static_cast<std::uint64_t>(OverseerCloseReason::MalformedMessage));
  const std::scoped_lock lock(m_mutex);
  for (const std::string& address : silent)
    m_comings.push_back({.outcome = OverseerOutcome::Silent, .address = address});
  // The overseer's connection gone: closed by the overseer, or lost.
  if (m_overseer && m_overseer->HasGone())
  {
    m_comings.push_back({.outcome = OverseerOutcome::Closed, .address = m_overseer->PeerAddress()});
    m_overseer.reset();
  }
  return std::exchange(m_comings, {});
}

bool Outpost::OverseerLink::Held() const
{
  const std::scoped_lock lock(m_mutex);
  return m_overseer && !m_overseer->HasGone();
}

Outpost::PlayerId Outpost::OverseerLink::Sight() const
{
  const std::scoped_lock lock(m_mutex);
  return m_sight;
}

void Outpost::OverseerLink::SendView(Snapshot _view)
{
  std::shared_ptr<Neuron::QuicChannel> overseer;
  std::uint32_t takings = 0;
  std::optional<std::uint64_t> held;
  {
    const std::scoped_lock lock(m_mutex);
    overseer = m_overseer;
    takings = m_takings;
    held = m_held;
  }
  if (!overseer || overseer->HasGone())
    return;
  // A newer overseer holds none of the views sent the one before it.
  if (takings != m_viewsTaking)
  {
    m_viewsSent.Clear();
    m_viewsTaking = takings;
  }
  // The sectors' shapes and the fields are the welcome's, and the view says nothing of what happened between two of them.
  (void)TakeCatalog(_view);
  (void)TakeEvents(_view);
  StateMessage state{.snapshot = std::move(_view)};
  if (overseer->SendNewest(m_viewsSent.Encode(state, held)))
    m_viewsSent.Keep(state.snapshot);
}

std::uint64_t Outpost::OverseerLink::LaneDayStart() const noexcept
{
  const std::uint64_t day = LANE_SECONDS * m_welcome.ticksPerSecond;
  return m_newestTick > day ? m_newestTick - day : 0;
}

void Outpost::OverseerLink::SendRecords(std::vector<std::string> _lines)
{
  if (_lines.empty())
    return;
  const std::scoped_lock lock(m_mutex);
  bool lanes = false;
  for (const std::string& line : _lines)
  {
    m_records.push_back(line);
    if (const std::optional<RecordHead> head = HeadOf(line))
    {
      m_newestTick = std::max(m_newestTick, head->tick);
      if (IsLaneRecord(head->kind))
      {
        m_lanes.push_back(line);
        lanes = true;
      }
    }
  }
  if (lanes)
    KeepLaneRecords(m_lanes, LaneDayStart());
  while (m_records.size() > OVERSEER_RECORDS_KEPT)
    m_records.pop_front();
  if (m_overseer)
    m_overseer->Send(EncodeOverseerMessage(LogRecordsMessage{.lines = std::move(_lines)}));
}

void Outpost::OverseerLink::SendHealth(const HealthMessage& _health)
{
  const std::vector<std::byte> encoded = EncodeOverseerMessage(_health);
  const std::scoped_lock lock(m_mutex);
  if (m_overseer)
    m_overseer->Send(encoded);
}

void Outpost::OverseerLink::SendClosing(const ClosingMessage& _closing, std::chrono::milliseconds _wait)
{
  const std::vector<std::byte> encoded = EncodeOverseerMessage(_closing);
  std::shared_ptr<Neuron::QuicChannel> overseer;
  {
    const std::scoped_lock lock(m_mutex);
    if (!m_overseer)
      return;
    m_overseer->Send(encoded);
    overseer = m_overseer;
  }
  // Closing a connection drops what it has yet to deliver, so the close waits until the overseer has acknowledged it all.
  const auto deadline = std::chrono::steady_clock::now() + _wait;
  while (!overseer->HasGone() && overseer->Statistics().backlogBytes > 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(CLOSING_POLL);
  overseer->Shutdown(static_cast<std::uint64_t>(OverseerCloseReason::Finished));
}
