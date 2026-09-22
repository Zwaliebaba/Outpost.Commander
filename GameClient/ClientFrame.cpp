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
  const Snapshot* newest = m_replicas.Newest();
  if ((newest != nullptr) && (m_player < newest->players.size()))
  {
    const std::uint16_t applied = newest->players[m_player].lastCommandSequenceApplied;
    result.markersCleared = static_cast<std::uint32_t>(m_markers.ClearAcknowledged(applied));
  }

  return result;
}

ReplicaStore::Frame ClientFrame::Advance(std::uint64_t _nowMilliseconds) const noexcept
{
  return m_replicas.FrameAt(ReplicaStore::RenderMilliseconds(_nowMilliseconds));
}

} // namespace Outpost
