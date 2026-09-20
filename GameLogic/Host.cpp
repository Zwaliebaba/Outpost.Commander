#include "pch.h"

#include "Host.h"

#include "OrderValidation.h"

#include <algorithm>

namespace Outpost
{

namespace
{

/// A seat that has gone quiet is suspect after this long and under AI control after the grace
/// period on top (§5.4, GameDesign.md §10). In the simulation's ticks, which is the host's clock.
constexpr std::uint32_t QUIET_TICKS = 5 * static_cast<std::uint32_t>(Neuron::TICKS_PER_SECOND);

} // namespace

Host::Host(Sim& _sim, Neuron::Transport& _transport, std::uint64_t _contentHash, std::uint32_t _tick)
  : m_sim(&_sim),
    m_transport(&_transport),
    m_contentHash(_contentHash)
{
  m_lastFrameBytes.assign(MAX_SEATS, 0);
  (void)_tick;
}

SeatConnection Host::SeatState(std::uint8_t _seat) const noexcept
{
  for (const HostClient& client : m_clients)
  {
    // AN OBSERVER IS NOT THE SEAT IT WATCHES, here and in the two functions below. It holds a view
    // of that seat, so the plain search by view.seat would find it and report an AI seat as one a
    // player is sitting in - which would hand the next joiner a refusal and would tell the
    // interface that a scripted commander is human.
    if (client.view.seat == _seat && !client.observer)
    {
      return client.state;
    }
  }
  return SeatConnection::Open;
}

const HostClient* Host::ClientForSeat(std::uint8_t _seat) const noexcept
{
  for (const HostClient& client : m_clients)
  {
    if (client.view.seat == _seat && !client.observer)
    {
      return &client;
    }
  }
  return nullptr;
}

std::uint32_t Host::LastFrameBytes(std::uint8_t _seat) const noexcept
{
  return _seat < m_lastFrameBytes.size() ? m_lastFrameBytes[_seat] : 0;
}

HostClient* Host::Find(Neuron::ConnectionId _connection) noexcept
{
  for (HostClient& client : m_clients)
  {
    if (client.connection == _connection && client.state != SeatConnection::Open)
    {
      return &client;
    }
  }
  return nullptr;
}

std::uint8_t Host::FreeSeat() const noexcept
{
  const std::span<const Seat> seats = m_sim->Seats();
  for (std::size_t seat = 0; seat < seats.size(); ++seat)
  {
    const auto index = static_cast<std::uint8_t>(seat);
    if (seats[index].kind != SeatKind::Human)
    {
      continue;
    }
    bool taken = false;
    for (const HostClient& client : m_clients)
    {
      // NO OBSERVER TEST HERE, AND THAT IS AN INVARIANT RATHER THAN AN OVERSIGHT. This loop only
      // ever looks at seats whose kind is Human, and OnJoin refuses to let anybody watch one of
      // those - so an observer's view.seat is never an index this loop reaches. A guard would be
      // unreachable, which means untestable, which means it would rot. If watching a human's seat
      // is ever allowed, this is the second place that has to change.
      taken = taken || (client.view.seat == index && client.state != SeatConnection::Open);
    }
    if (!taken)
    {
      return index;
    }
  }
  return MAX_SEATS;
}

void Host::Advance(std::uint32_t _tick)
{
  Neuron::ConnectionId connection = Neuron::NO_CONNECTION;
  while (m_transport->Accept(connection))
  {
    // Nothing is held for a connection until it has joined: a connection that never sends a Join
    // costs a socket and no seat.
  }

  Receive(_tick);

  for (HostClient& client : m_clients)
  {
    if (client.state == SeatConnection::Open)
    {
      continue;
    }
    const std::uint32_t quiet = _tick - client.lastHeardTick;
    const std::uint32_t grace = m_sim->Settings().rejoinGraceTicks;
    if (quiet >= QUIET_TICKS + grace)
    {
      if (client.observer)
      {
        // NOTHING WAITS FOR AN OBSERVER. The grace period exists so a commander's seat is held
        // while he reconnects; a client that was only watching holds no seat, so what its silence
        // means is that it is gone, and the seat it was watching never knew it was there. It is
        // dropped below rather than here, because a client cannot be erased while this loop holds
        // a reference into the vector.
        client.state = SeatConnection::Open;
        continue;
      }
      if (client.state != SeatConnection::UnderAi)
      {
        // The grace period of GameDesign.md §10 has run out. The seat plays on as a scripted one,
        // and the player may still rejoin with the same token and take it back.
        client.state = SeatConnection::UnderAi;
        ++m_counters.droppedToAi;
      }
    }
    else if (quiet >= QUIET_TICKS)
    {
      client.state = SeatConnection::Suspect;
    }
    else
    {
      client.state = SeatConnection::Playing;
    }
  }

  std::erase_if(m_clients, [](const HostClient& _client) { return _client.observer && _client.state == SeatConnection::Open; });

  LatchRejections();

  if (m_sim->PublishDue())
  {
    Publish(_tick);
  }
}

/// Puts one refusal into a client's latch, with the sequence step that says it is a new one.
///
/// FREE, BECAUSE TWO PLACES NOW SET IT: the simulation's refusals for a seat, folded every tick
/// below, and an observer's own orders, refused by the host before they reach the simulation at
/// all (m1-vertical-slice/G2). Both have to step the same counter the same way or a client cannot
/// tell a second refusal from the first.
void Latch(RejectionLatch& _latch, OrderKind _kind, RejectReason _reason) noexcept
{
  _latch.sequence = static_cast<std::uint16_t>(_latch.sequence + 1);
  if (_latch.sequence == 0)
  {
    // 0 is how a seat that has had no refusal says so, so the counter steps over it on the wrap
    // rather than claiming a commander's 65,536th refusal never happened.
    _latch.sequence = 1;
  }
  _latch.kind = _kind;
  _latch.reason = _reason;
}

void Host::LatchRejections() noexcept
{
  // EVERY TICK, NOT EVERY PUBLISH. Sim clears a seat's rejections at the start of every stage 1 and
  // a publish is due on even ticks only, so reading the vector from PublishTo would drop every
  // refusal an odd tick judged - which, since a commander's clicks do not land on even ticks by
  // arrangement, is about half of them.
  const std::span<const Seat> seats = m_sim->Seats();
  for (HostClient& client : m_clients)
  {
    // AN OBSERVER IS TOLD NOTHING ABOUT THE COMMANDER'S OWN CLICKS. A refusal answers the client
    // that sent the order; a client watching somebody else's seat sent none of them, and its latch
    // is the one place its OWN refused orders are reported. Folding the seat's refusals into it
    // would overwrite that with an answer to a question it never asked.
    if (client.state == SeatConnection::Open || client.observer || client.view.seat >= seats.size())
    {
      continue;
    }
    for (const OrderRejection& rejection : seats[client.view.seat].rejections)
    {
      Latch(client.view.rejection, rejection.kind, rejection.reason);
    }
  }
}

void Host::Receive(std::uint32_t _tick)
{
  Neuron::ConnectionId connection = Neuron::NO_CONNECTION;
  std::span<const std::byte> datagram;
  while (m_transport->Receive(connection, datagram))
  {
    std::span<const std::byte> payload;
    if (!Neuron::UnframeDatagram(datagram, payload, m_framing))
    {
      continue;
    }
    Neuron::ByteReader reader(payload);
    MessageKind kind{};
    if (!ReadMessageKind(reader, kind))
    {
      continue;
    }
    switch (kind)
    {
    case MessageKind::Join:
    {
      Join join{};
      if (Read(reader, join))
      {
        OnJoin(connection, join, _tick);
      }
      break;
    }
    case MessageKind::Orders:
    {
      HostClient* client = Find(connection);
      Orders orders{};
      if (client != nullptr && Read(reader, orders))
      {
        OnOrders(*client, orders, _tick);
      }
      break;
    }
    case MessageKind::Heartbeat:
    {
      HostClient* client = Find(connection);
      Heartbeat heartbeat{};
      if (client != nullptr && Read(reader, heartbeat))
      {
        client->lastHeardTick = _tick;
        client->view.acknowledgedSequence = std::max(client->view.acknowledgedSequence, heartbeat.ack.frameSequence);
      }
      break;
    }
    case MessageKind::Ack:
    case MessageKind::JoinAccepted:
    case MessageKind::JoinRefused:
    case MessageKind::Frame:
    case MessageKind::Fragment:
    default:
      // Everything else travels host to client. A host that receives one is being talked to by
      // something that is not a client of this protocol.
      break;
    }
  }
}

void Host::OnJoin(Neuron::ConnectionId _connection, const Join& _join, std::uint32_t _tick)
{
  if (_join.protocolVersion != NET_PROTOCOL_VERSION)
  {
    ++m_counters.refusals;
    Refuse(_connection, RefusalReason::ProtocolVersion);
    return;
  }
  if (_join.contentHash != m_contentHash)
  {
    // ADR-009's binding, at the join rather than at a snapshot: a client whose tables differ is
    // playing a different game and would diverge in silence.
    ++m_counters.refusals;
    Refuse(_connection, RefusalReason::ContentHash);
    return;
  }

  // A rejoin is a join with the same token (§5.4): the seat comes back from AI control and the
  // client gets a full frame, because nothing it held can be trusted after an absence.
  for (HostClient& client : m_clients)
  {
    if (client.token == _join.token && client.state != SeatConnection::Open)
    {
      client.connection = _connection;
      client.state = SeatConnection::Playing;
      client.lastHeardTick = _tick;
      client.view.history.Clear();
      client.view.acknowledgedSequence = NO_BASELINE;
      // The history is gone, so no baseline can be folded into the fog again; the next frame is a
      // full one and encodes the whole grid against an empty one (Net/FrameEncoder.cpp).
      client.view.foggedThrough = NO_BASELINE;
      // AND THE EVENTS OF THE ABSENCE GO WITH IT (m1-vertical-slice/C9). A state list is caught up
      // by the full frame that follows, which is exactly what a rejoin is for; an event cannot be,
      // because the tick it happened in is over. Sending the queue to a commander who has just
      // come back would open his match with a burst of muzzle flashes from shots fired while he
      // was gone, at objects that have since died. The running total goes back to nothing with it,
      // which is safe precisely because the history that could name an older total is cleared in
      // the same breath.
      client.view.pendingEvents.clear();
      client.view.eventsForgotten = 0;
      client.orders = {};
      ++m_counters.rejoins;
      JoinAccepted accepted{};
      accepted.seat = client.view.seat;
      accepted.tick = m_sim->Tick();
      accepted.settings = m_sim->Settings();
      accepted.landscape = m_sim->Terrain().Definition();
      Neuron::ByteWriter payload;
      Write(payload, accepted);
      SendPayload(_connection, payload);
      return;
    }
  }

  // A JOIN THAT ASKS TO WATCH (m1-vertical-slice/G2). The seat has to exist in this match and it
  // has to be a seat no human owns: a scripted commander has nobody to be displaced, and a human's
  // seat is still a human's while his rejoin grace runs, so the rule is on the seat's KIND rather
  // than on whether a client is connected to it now. NoSeat is the refusal either way, because
  // from the joiner's side "there is no such seat for you" is the same answer.
  const bool observing = _join.observeSeat != NO_OBSERVED_SEAT;
  const std::span<const Seat> seats = m_sim->Seats();
  if (observing && (_join.observeSeat >= seats.size() || seats[_join.observeSeat].kind == SeatKind::Human))
  {
    ++m_counters.refusals;
    Refuse(_connection, RefusalReason::NoSeat);
    return;
  }

  const std::uint8_t seat = observing ? _join.observeSeat : FreeSeat();
  if (seat >= MAX_SEATS)
  {
    ++m_counters.refusals;
    Refuse(_connection, RefusalReason::NoSeat);
    return;
  }

  HostClient client{};
  client.connection = _connection;
  client.token = _join.token;
  client.view.seat = seat;
  client.lastHeardTick = _tick;
  client.state = SeatConnection::Playing;
  client.observer = observing;
  m_clients.push_back(std::move(client));
  if (observing)
  {
    ++m_counters.observers;
  }
  else
  {
    ++m_counters.joins;
  }

  JoinAccepted accepted{};
  accepted.seat = seat;
  accepted.tick = m_sim->Tick();
  accepted.settings = m_sim->Settings();
  accepted.landscape = m_sim->Terrain().Definition();
  Neuron::ByteWriter payload;
  Write(payload, accepted);
  SendPayload(_connection, payload);
}

void Host::OnOrders(HostClient& _client, const Orders& _orders, std::uint32_t _tick)
{
  _client.lastHeardTick = _tick;
  _client.view.acknowledgedSequence = std::max(_client.view.acknowledgedSequence, _orders.ack.frameSequence);

  std::vector<Order> delivered;
  _client.orders.Receive(_orders.orders, delivered);
  for (Order order : delivered)
  {
    // AN OBSERVER OWNS NOTHING, so every order it sends is refused before the shape is even looked
    // at (m1-vertical-slice/G2). NotOwned is the reason rather than a new one: RejectReason says
    // "the order names an object the seat does not own, or none at all", which is exactly true of
    // a client watching somebody else's commander, and it reaches the client through the latch
    // every refusal uses rather than through a second path.
    if (_client.observer)
    {
      ++m_counters.ordersRefusedObserver;
      Latch(_client.view.rejection, order.kind, RejectReason::NotOwned);
      continue;
    }
    // The SHAPE is checked here and the rules are the simulation's: a seat a client does not sit in
    // is a client lying about who it is, which Net refuses, while "he cannot afford it" is stage
    // 1's judgement and belongs in the rejection list the seat carries.
    if (order.seat != _client.view.seat || static_cast<std::uint8_t>(order.kind) >= ORDER_KIND_COUNT)
    {
      ++m_counters.ordersRefusedShape;
      continue;
    }
    // Always for the next tick: an order for a tick already run would be moved anyway, and one for
    // a tick far ahead would let a client schedule the future.
    order.tick = m_sim->Tick() + 1;
    m_sim->Submit(order);
    ++m_counters.ordersApplied;
  }
}

void Host::Publish(std::uint32_t _tick)
{
  for (HostClient& client : m_clients)
  {
    if (client.state == SeatConnection::Open)
    {
      continue;
    }
    PublishTo(client, _tick);
  }
  // The events of the interval have now gone to everyone who was entitled to them.
  m_events.clear();
}

/// Drops from the client's pending queue the events the frame it has acknowledged carried
/// (m1-vertical-slice/C9). Called before the next frame is encoded, so that what goes out is what
/// the client is still owed.
void Host::ForgetAcknowledgedEvents(HostClient& _client)
{
  if (_client.view.acknowledgedSequence == NO_BASELINE)
  {
    return;
  }
  const FrameRecord* acknowledged = _client.view.history.Find(_client.view.acknowledgedSequence);
  if (acknowledged == nullptr)
  {
    return; // Aged out of the history; the client is about to be sent a full frame.
  }
  // WHAT IS OWED IS THE DIFFERENCE BETWEEN TWO RUNNING TOTALS, and never the record's own count.
  // This runs on every publish and the acknowledged sequence moves on rather fewer of them, so the
  // same record is folded several times over; a fold that dropped the record's count each time
  // would take events off the front that the client has never been sent. Nothing owed is the
  // ordinary case here, and it has to be free.
  if (acknowledged->eventsSentThrough <= _client.view.eventsForgotten)
  {
    return;
  }
  const std::size_t drop =
    std::min<std::size_t>(acknowledged->eventsSentThrough - _client.view.eventsForgotten, _client.view.pendingEvents.size());
  _client.view.pendingEvents.erase(_client.view.pendingEvents.begin(),
                                   _client.view.pendingEvents.begin() + static_cast<std::ptrdiff_t>(drop));
  _client.view.eventsForgotten += static_cast<std::uint32_t>(drop);
}

void Host::PublishTo(HostClient& _client, std::uint32_t _tick)
{
  (void)_tick;
  ForgetAcknowledgedEvents(_client);
  GatherInterest(*m_sim, _client.view.seat, m_interest);

  // Only the events this commander could see. An event names objects, and an object he cannot see
  // is one he is not told died - which is what makes an explosion fog-correct for free (§5.3).
  std::vector<Event> visible;
  for (const Event& event : m_events)
  {
    const bool known = std::binary_search(m_interest.devices.begin(), m_interest.devices.end(), event.source) ||
                       std::binary_search(m_interest.structures.begin(), m_interest.structures.end(), event.source);
    if (known)
    {
      visible.push_back(event);
    }
  }

  const FrameRecord* baseline =
    _client.view.acknowledgedSequence == NO_BASELINE ? nullptr : _client.view.history.Find(_client.view.acknowledgedSequence);
  if (baseline == nullptr)
  {
    ++m_counters.fullFrames;
  }
  EncodeFrame(*m_sim, m_interest, _client.view, baseline, visible, m_frame, m_record);

  Neuron::ByteWriter payload;
  Write(payload, m_frame);
  const std::size_t bytes = payload.Bytes().size();
  m_lastFrameBytes[_client.view.seat] = static_cast<std::uint32_t>(bytes);
  m_counters.largestFrameBytes = std::max(m_counters.largestFrameBytes, static_cast<std::uint32_t>(bytes));

  if (bytes <= Neuron::MAX_DATAGRAM_PAYLOAD_BYTES)
  {
    SendPayload(_client.connection, payload);
  }
  else if (SplitIntoFragments(m_frame.sequence, payload.Bytes(), m_fragments))
  {
    for (const Fragment& piece : m_fragments)
    {
      Neuron::ByteWriter one;
      Write(one, piece);
      SendPayload(_client.connection, one);
      ++m_counters.fragments;
    }
  }
  else
  {
    // Past what MAX_FRAGMENTS can carry. Nothing is sent, the client's acknowledgement does not
    // move, and the next publish tries again - which is the same recovery a lost frame has.
    return;
  }

  _client.view.history.Push(m_record);
  ++_client.view.nextSequence;
  ++m_counters.framesPublished;
}

void Host::SendPayload(Neuron::ConnectionId _connection, const Neuron::ByteWriter& _payload)
{
  Neuron::ByteWriter datagram;
  if (Neuron::FrameDatagram(_payload.Bytes(), datagram))
  {
    (void)m_transport->Send(_connection, datagram.Bytes());
  }
}

void Host::Refuse(Neuron::ConnectionId _connection, RefusalReason _reason)
{
  JoinRefused refused{};
  refused.reason = _reason;
  refused.hostProtocolVersion = NET_PROTOCOL_VERSION;
  refused.hostContentHash = m_contentHash;
  Neuron::ByteWriter payload;
  Write(payload, refused);
  SendPayload(_connection, payload);
}

} // namespace Outpost
