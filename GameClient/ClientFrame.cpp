#include "pch.h"

#include "ClientFrame.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace Outpost
{

ClientFrame::DrainResult ClientFrame::DrainPackets(Neuron::PacketQueue& _queue, std::uint64_t _nowMilliseconds) noexcept
{
  DrainResult result;

  // **THE SILENCE IS JUDGED BEFORE ANYTHING IS DRAINED**, and that order is what makes a resume show the
  // overlay. A suspended client's socket can hold datagrams that land on the first frame back; judged
  // after the drain they would be stamped now and the loss would never be seen. Judged first, the gap is
  // the suspension, the rejoin starts, and those stale updates cannot clear it -- only one that arrives
  // after the host has seated this client again can.
  if (m_join.IsJoined() && m_heardUpdate && ((_nowMilliseconds - m_lastHeardMilliseconds) >= LINK_SILENCE_MILLISECONDS))
  {
    m_join.Rejoin();

    // **THE STORE STARTS AGAIN FROM NOTHING** (ADR-024). The host resets this client's accumulator when
    // it answers the rejoin, so everything is sent within one sweep -- and the removals of whatever died
    // meanwhile have long stopped repeating, so anything kept here could be a ghost.
    m_replicas.Clear();
    m_reconnecting = true;
    m_lastHeardMilliseconds = _nowMilliseconds;
    result.linkLost = true;
  }

  // On the stack and sized once. A frame must not allocate to read its own input -- the queue went
  // to the trouble of allocating every slot up front for exactly this reason.
  std::array<std::byte, DATAGRAM_BUFFER_BYTES> buffer{};

  std::size_t byteCount = 0;
  while (_queue.Drain(buffer, byteCount))
  {
    ++result.datagrams;

    const std::span<const std::byte> datagram{buffer.data(), byteCount};

    // THE TYPE IS READ BEFORE THE RECORD IS. Two records arrive on this socket since ADR-013, and
    // an update decoder handed a join reply reports a fault -- which would count a perfectly good
    // reply as a corrupt datagram and hide the one thing the client is waiting for.
    Neuron::ByteReader probe{datagram};
    Neuron::PacketHeader header{};
    if (Neuron::PacketHeader::Read(probe, header) != Neuron::PacketFault::None)
    {
      ++result.faulted;
      continue;
    }

    if (header.type == Neuron::PacketType::JoinReply)
    {
      Neuron::ByteReader replyReader{datagram};
      JoinReply reply{};
      if (Decode(replyReader, reply) != JoinFault::None)
      {
        ++result.faulted;
        continue;
      }

      ++result.joinReplies;

      // OR, NOT ASSIGN. The host answers every retry, so several replies can land in one drain and
      // only the one that moved the token has anything to persist.
      result.tokenChanged = m_join.Accept(reply) || result.tokenChanged;

      // R23: THE FIELD FOLLOWS THE JOIN, AND ONLY THE JOIN. A refusal zeroes the count and clears it; a
      // repeat of the same seat is the same pair and costs nothing. A late refusal to a seated client is
      // ignored by `Accept`, so it leaves the pair -- and the field -- where they were.
      static_cast<void>(m_field.Derive(m_join.MatchSeed(), m_join.PlayerCount()));
      continue;
    }

    Neuron::ByteReader reader{datagram};
    Update update;
    if (Decode(reader, update) != UpdateFault::None)
    {
      ++result.faulted;
      continue;
    }

    // EVERY UPDATE IS HEARD, WHATEVER BECAME OF ITS RECORDS. An update whose records were all refused as
    // reordered still says the link is alive and still carries the tick and this client's block.
    const ReplicaStore::AcceptResult accepted = m_replicas.Accept(update, _nowMilliseconds);
    ++result.accepted;
    result.refused += accepted.refused;
    result.refreshed += accepted.refreshed;
    result.refreshTicksTotal += accepted.refreshTicksTotal;
    result.refreshTicksMax = std::max(result.refreshTicksMax, accepted.refreshTicksMax);
    m_lastHeardMilliseconds = _nowMilliseconds;
    m_heardUpdate = true;
  }

  // THE ACKNOWLEDGMENT IS READ FROM THE NEWEST UPDATE AND NOT FROM EACH ONE DRAINED. It is a high-water
  // mark, so the newest covers every marker the older ones would have, and clearing once per frame is
  // both cheaper and easier to reason about (Q20).
  //
  // **THE BLOCK IS THIS CLIENT'S OWN** (ADR-024): an update carries its recipient's block and nobody
  // else's, so there is no longer an index to get wrong -- the one-based player and zero-based block that
  // this comment used to warn about are gone with the per-player list.
  const PlayerBlock* own = m_replicas.Own();
  if ((own != nullptr) && (m_join.Player() != NO_PLAYER))
  {
    const std::uint16_t applied = own->lastCommandSequenceApplied;
    result.markersCleared = static_cast<std::uint32_t>(m_markers.ClearAcknowledged(applied));

    // THE SAME HIGH-WATER MARK RETIRES WHAT IS BEING RESENT (M5). Every command at or before it has been
    // decided on by the host -- applied or refused, both acknowledged -- so sending it again would only be
    // discarded as already applied.
    std::size_t kept = 0;
    for (std::size_t index = 0; index < m_outstanding.size(); ++index)
    {
      if (!SequenceIsNewer(m_outstanding[index].sequence, applied))
      {
        ++result.commandsRetired;
        continue;
      }
      if (kept != index)
      {
        m_outstanding[kept] = std::move(m_outstanding[index]);
        m_issuedMilliseconds[kept] = m_issuedMilliseconds[index];
      }
      ++kept;
    }
    m_outstanding.resize(kept);
    m_issuedMilliseconds.resize(kept);

    // ONCE, FROM THE FIRST UPDATE THAT CARRIES THIS PLAYER'S BLOCK. See the field's comment: a client that
    // always starts counting at one is refused by the host for as long as it takes to count back past the
    // previous session, which is the difference between reconnecting and resuming.
    //
    // ONLY FORWARDS. An acknowledgment lags the orders in flight, so adopting it more than once would wind
    // the counter back over commands the client has already sent and get them refused as duplicates.
    if (!m_adoptedSequence)
    {
      m_adoptedSequence = true;
      m_nextCommandSequence = static_cast<std::uint16_t>(applied + 1);
    }
  }

  // **AND WHAT HAS WAITED TOO LONG IS GIVEN UP, WITH ITS MARKER** (M5), oldest first -- so a command that is
  // never going to be acknowledged cannot ride every packet forever, and the interface stops drawing an order
  // that may never have arrived.
  std::size_t expired = 0;
  while ((expired < m_outstanding.size()) && ((_nowMilliseconds - m_issuedMilliseconds[expired]) >= COMMAND_RESEND_WINDOW_MILLISECONDS))
  {
    static_cast<void>(m_markers.ClearCommand(m_outstanding[expired].sequence));
    ++expired;
  }
  if (expired > 0)
  {
    m_outstanding.erase(m_outstanding.begin(), m_outstanding.begin() + static_cast<std::ptrdiff_t>(expired));
    m_issuedMilliseconds.erase(m_issuedMilliseconds.begin(), m_issuedMilliseconds.begin() + static_cast<std::ptrdiff_t>(expired));
    result.commandsExpired = static_cast<std::uint32_t>(expired);
  }

  // **SEATED AGAIN AND AN UPDATE IN HAND**, in either order within the drain -- the host answers the join
  // and goes on sending, so both commonly arrive together. `Interface.md` section 7: the overlay stays
  // until the first update lands, and then play resumes straight away.
  if (m_reconnecting && m_join.IsJoined() && (result.accepted > 0))
  {
    m_reconnecting = false;
    result.linkRestored = true;
  }

  return result;
}

LinkState ClientFrame::Link() const noexcept
{
  // REFUSED FIRST: a rejoin the host answers with `MatchFull` is terminal, and showing it as a reconnect
  // would promise the player something that retrying will not deliver.
  switch (m_join.Phase())
  {
  case JoinPhase::Refused:
    return LinkState::Refused;
  case JoinPhase::Joined:
  case JoinPhase::Joining:
    break;
  }

  if (m_reconnecting)
  {
    return LinkState::Reconnecting;
  }
  return m_join.IsJoined() ? LinkState::Linked : LinkState::Joining;
}

DrawnSummary ClientFrame::Advance(std::uint64_t _nowMilliseconds, std::vector<EntityRecord>& _outRecords) const
{
  return m_replicas.Drawn(ReplicaStore::RenderMilliseconds(_nowMilliseconds), _outRecords);
}

void ClientFrame::IssueCommand(Command _command, std::uint64_t _nowMilliseconds)
{
  m_outstanding.push_back(std::move(_command));
  m_issuedMilliseconds.push_back(_nowMilliseconds);
}

std::size_t ClientFrame::FillOutstanding(CommandPacket& _packet) const
{
  // The update's payload bounds a command packet too, as the Bot has it: one whole datagram, never a
  // fragment (`TechnicalDesign.md` section 4). What does not fit waits for the next packet, in order.
  return FillOldestFirst(m_outstanding, UPDATE_PAYLOAD_BYTES, _packet);
}

bool ClientFrame::ShouldReportView(std::uint64_t _nowMilliseconds) noexcept
{
  if (!m_join.IsJoined())
  {
    return false;
  }
  if (m_reportedView && ((_nowMilliseconds - m_lastViewReportMilliseconds) < VIEW_REPORT_INTERVAL_MILLISECONDS))
  {
    return false;
  }
  m_reportedView = true;
  m_lastViewReportMilliseconds = _nowMilliseconds;
  return true;
}

void ClientFrame::StampView(CommandPacket& _packet) const noexcept
{
  // Through `Fixed` and the wire's own quantizer, as a move order's target is, so the rounding and the
  // saturation at the play area's corner are the wire's rules rather than this file's.
  const auto fixedX = static_cast<Neuron::Fixed>(m_camera.focusX * static_cast<float>(Neuron::FIXED_ONE));
  const auto fixedY = static_cast<Neuron::Fixed>(m_camera.focusY * static_cast<float>(Neuron::FIXED_ONE));
  _packet.viewX = QuantizePosition(fixedX);
  _packet.viewY = QuantizePosition(fixedY);

  // Saturated rather than wrapped: the camera's far end is 22,500 units, which a sixteen-bit radius holds,
  // and anything past 65,535 means "all of it" either way.
  const float radius = (m_camera.distance < 0.0f) ? 0.0f : m_camera.distance;
  _packet.viewRadiusUnits = (radius >= 65535.0f) ? std::uint16_t{65535} : static_cast<std::uint16_t>(radius);
}

} // namespace Outpost
