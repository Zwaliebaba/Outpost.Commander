#include "pch.h"

#include "ClientFrame.h"

#include <array>

namespace Outpost
{

ClientFrame::DrainResult ClientFrame::DrainPackets(Neuron::PacketQueue& _queue, std::uint64_t _nowMilliseconds) noexcept
{
  DrainResult result;

  // **THE SILENCE IS JUDGED BEFORE ANYTHING IS DRAINED**, and that order is what makes a resume show the
  // overlay. A suspended client's socket can hold datagrams that land on the first frame back; judged
  // after the drain they would be stamped now and the loss would never be seen. Judged first, the gap is
  // the suspension, the rejoin starts, and those stale snapshots cannot clear it -- only one that arrives
  // after the host has seated this client again can.
  if (m_join.IsJoined() && m_heardSnapshot && ((_nowMilliseconds - m_lastHeardMilliseconds) >= LINK_SILENCE_MILLISECONDS))
  {
    m_join.Rejoin();
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
    // a snapshot decoder handed a join reply reports a fault -- which would count a perfectly good
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
      continue;
    }

    Neuron::ByteReader reader{datagram};
    Snapshot snapshot;
    if (Decode(reader, snapshot) != SnapshotFault::None)
    {
      ++result.faulted;
      continue;
    }

    if (m_replicas.Accept(snapshot, _nowMilliseconds))
    {
      ++result.accepted;
      m_lastHeardMilliseconds = _nowMilliseconds;
      m_heardSnapshot = true;
    }
    else
    {
      ++result.refused;
    }
  }

  // THE ACKNOWLEDGMENT IS READ FROM THE NEWEST SNAPSHOT AND NOT FROM EACH ONE DRAINED. They are a
  // high-water mark, so the newest covers every marker the older ones would have, and clearing once
  // per frame is both cheaper and easier to reason about (Q20).
  // A PLAYER IDENTITY IS ONE-BASED AND A BLOCK INDEX IS NOT. `GameCore/Entity.h` reserves zero for
  // `NO_PLAYER`, so the host numbers players from one (`GameLogic/Host.cpp`), while the snapshot's
  // per-player blocks are an array starting at zero -- player one's block is index zero.
  //
  // Reading `players[m_player]` looks obviously right and is off by one in a way nothing complains
  // about: with one player it indexes past the end and the bounds check silently skips every
  // acknowledgment, so markers would stay on screen forever; with two it reads the OTHER player's
  // sequence and clears markers on somebody else's progress.
  const Snapshot* newest = m_replicas.Newest();
  const PlayerId player = m_join.Player();
  if ((newest != nullptr) && (player != NO_PLAYER))
  {
    const std::size_t block = static_cast<std::size_t>(player) - 1;
    if (block < newest->players.size())
    {
      const std::uint16_t applied = newest->players[block].lastCommandSequenceApplied;
      result.markersCleared = static_cast<std::uint32_t>(m_markers.ClearAcknowledged(applied));

      // ONCE, FROM THE FIRST SNAPSHOT THAT NAMES THIS PLAYER. See the field's comment: a client
      // that always starts counting at one is refused by the host for as long as it takes to count
      // back past the previous session, which is the difference between reconnecting and resuming.
      //
      // ONLY FORWARDS. An acknowledgment lags the orders in flight, so adopting it more than once
      // would wind the counter back over commands the client has already sent and get them refused
      // as duplicates.
      if (!m_adoptedSequence)
      {
        m_adoptedSequence = true;
        m_nextCommandSequence = static_cast<std::uint16_t>(applied + 1);
      }
    }
  }

  // **SEATED AGAIN AND A SNAPSHOT IN HAND**, in either order within the drain -- the host answers the join
  // and goes on sending, so both commonly arrive together. `Interface.md` section 7: the overlay stays
  // until the first snapshot lands, and then play resumes straight away.
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

ReplicaStore::Frame ClientFrame::Advance(std::uint64_t _nowMilliseconds) const noexcept
{
  return m_replicas.FrameAt(ReplicaStore::RenderMilliseconds(_nowMilliseconds));
}

} // namespace Outpost
