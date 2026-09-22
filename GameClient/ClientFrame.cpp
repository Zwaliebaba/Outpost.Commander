#include "pch.h"

#include "ClientFrame.h"

#include <array>

namespace Outpost
{

ClientFrame::DrainResult ClientFrame::DrainPackets(Neuron::PacketQueue& _queue, std::uint64_t _nowMilliseconds) noexcept
{
  DrainResult result;

  // On the stack and sized once. A frame must not allocate to read its own input -- the queue went
  // to the trouble of allocating every slot up front for exactly this reason.
  std::array<std::byte, DATAGRAM_BUFFER_BYTES> buffer{};

  std::size_t byteCount = 0;
  while (_queue.Drain(buffer, byteCount))
  {
    ++result.datagrams;

    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), byteCount}};
    Snapshot snapshot;
    if (Decode(reader, snapshot) != SnapshotFault::None)
    {
      ++result.faulted;
      continue;
    }

    if (m_replicas.Accept(snapshot, _nowMilliseconds))
    {
      ++result.accepted;
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
  if ((newest != nullptr) && (m_player != NO_PLAYER))
  {
    const std::size_t block = static_cast<std::size_t>(m_player) - 1;
    if (block < newest->players.size())
    {
      const std::uint16_t applied = newest->players[block].lastCommandSequenceApplied;
      result.markersCleared = static_cast<std::uint32_t>(m_markers.ClearAcknowledged(applied));
    }
  }

  return result;
}

ReplicaStore::Frame ClientFrame::Advance(std::uint64_t _nowMilliseconds) const noexcept
{
  return m_replicas.FrameAt(ReplicaStore::RenderMilliseconds(_nowMilliseconds));
}

} // namespace Outpost
