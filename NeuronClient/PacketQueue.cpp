#include "pch.h"

#include "PacketQueue.h"

#include <algorithm>
#include <cstring>

namespace Neuron
{

PacketQueue::PacketQueue(std::size_t _slotCount, std::size_t _slotBytes)
  : m_slotCount(std::max<std::size_t>(_slotCount, 1)),
    m_slotBytes(std::max<std::size_t>(_slotBytes, 1))
{
  // Clamped rather than asserted because a zero slot count is a modulo by zero in Push, and this
  // runs on a thread that has nowhere to report anything to.
  m_storage.resize(m_slotCount * m_slotBytes);
  m_lengths.resize(m_slotCount, 0);
}

void PacketQueue::Push(std::span<const std::byte> _datagram) noexcept
{
  const std::lock_guard<std::mutex> guard(m_mutex);

  if (_datagram.empty() || _datagram.size() > m_slotBytes)
  {
    ++m_rejectedCount;
    return;
  }

  if (m_pendingCount == m_slotCount)
  {
    // Drop the oldest, not the newest, and not the pool thread's time.
    m_headSlot = (m_headSlot + 1) % m_slotCount;
    --m_pendingCount;
    ++m_droppedCount;
  }

  const std::size_t slot = TailSlot();
  std::memcpy(m_storage.data() + (slot * m_slotBytes), _datagram.data(), _datagram.size());
  m_lengths[slot] = _datagram.size();
  ++m_pendingCount;
}

bool PacketQueue::Drain(std::span<std::byte> _outBuffer, std::size_t& _outBytes) noexcept
{
  const std::lock_guard<std::mutex> guard(m_mutex);

  if (m_pendingCount == 0)
  {
    return false;
  }

  const std::size_t length = m_lengths[m_headSlot];
  const std::size_t slot = m_headSlot;
  m_headSlot = (m_headSlot + 1) % m_slotCount;
  --m_pendingCount;

  // Consumed either way. A buffer too small for a slot is the caller's bug, and leaving the
  // datagram in place would turn that bug into a queue that fills once and never empties again.
  if (length > _outBuffer.size())
  {
    ++m_rejectedCount;
    return false;
  }

  std::memcpy(_outBuffer.data(), m_storage.data() + (slot * m_slotBytes), length);
  _outBytes = length;
  return true;
}

std::size_t PacketQueue::PendingCount() const noexcept
{
  const std::lock_guard<std::mutex> guard(m_mutex);
  return m_pendingCount;
}

std::uint64_t PacketQueue::DroppedCount() const noexcept
{
  const std::lock_guard<std::mutex> guard(m_mutex);
  return m_droppedCount;
}

std::uint64_t PacketQueue::RejectedCount() const noexcept
{
  const std::lock_guard<std::mutex> guard(m_mutex);
  return m_rejectedCount;
}

std::size_t PacketQueue::TailSlot() const noexcept
{
  return (m_headSlot + m_pendingCount) % m_slotCount;
}

} // namespace Neuron
