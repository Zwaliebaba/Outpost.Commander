#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace Neuron
{

/// The seam between the thread pool and the frame.
///
/// DatagramSocket delivers MessageReceived ON A THREAD POOL THREAD, not on the frame's thread
/// (TechnicalDesign.md section 5). The handler does exactly one thing: copy the datagram's bytes
/// in here and return. Nothing parses a packet there, nothing allocates there, and nothing
/// touches renderer or replica state there. This class is what makes that instruction followable:
/// the critical section is a memcpy and an index, and every slot was allocated at construction.
///
/// A FULL QUEUE DROPS THE OLDEST. Blocking the pool thread is the one thing this must never do,
/// and of the two datagrams the newest is worth more: ADR-003 makes every snapshot
/// self-contained, so a client that misses one is fully correct on the next. The drop is counted
/// rather than silent, because a queue that is quietly discarding the frame's input looks exactly
/// like a network that is quietly dropping it, and those want different fixes.
///
/// Sizes are the caller's: the slot count is how much jitter the frame will absorb and the slot
/// size is how large a datagram can be, and both belong with the frame loop that knows the wire
/// format rather than with the queue that does not.
class PacketQueue
{
public:
  /// Allocates every slot up front -- _slotCount times _slotBytes, in one block -- so that Push
  /// never allocates. Both are clamped to at least one.
  PacketQueue(std::size_t _slotCount, std::size_t _slotBytes);

  PacketQueue(const PacketQueue&) = delete;
  PacketQueue& operator=(const PacketQueue&) = delete;
  PacketQueue(PacketQueue&&) = delete;
  PacketQueue& operator=(PacketQueue&&) = delete;

  /// Copies one datagram in. Called from the thread pool. Never allocates, never throws, and
  /// never waits for anything but the mutex. An empty datagram, or one larger than a slot, is
  /// refused and counted under RejectedCount.
  void Push(std::span<const std::byte> _datagram) noexcept;

  /// Copies the oldest datagram out. Called from the frame. False when nothing is waiting, which
  /// is the ordinary answer and is how a drain loop ends.
  ///
  /// _outBuffer should be at least SlotBytes(); a smaller one cannot hold every datagram a slot
  /// can, and a datagram that will not fit it is consumed and counted under RejectedCount rather
  /// than left to wedge the queue forever.
  [[nodiscard]] bool Drain(std::span<std::byte> _outBuffer, std::size_t& _outBytes) noexcept;

  [[nodiscard]] std::size_t PendingCount() const noexcept;

  /// Datagrams discarded because the queue was full when a newer one arrived.
  [[nodiscard]] std::uint64_t DroppedCount() const noexcept;

  /// Datagrams that never entered the queue: empty, larger than a slot, or larger than the
  /// buffer a drain offered.
  [[nodiscard]] std::uint64_t RejectedCount() const noexcept;

  [[nodiscard]] std::size_t SlotCount() const noexcept
  {
    return m_slotCount;
  }

  [[nodiscard]] std::size_t SlotBytes() const noexcept
  {
    return m_slotBytes;
  }

private:
  /// The slot a datagram would be written into next. Callers hold m_mutex.
  [[nodiscard]] std::size_t TailSlot() const noexcept;

  mutable std::mutex m_mutex;
  std::vector<std::byte> m_storage;
  std::vector<std::size_t> m_lengths;
  std::size_t m_slotCount = 1;
  std::size_t m_slotBytes = 1;
  std::size_t m_headSlot = 0;
  std::size_t m_pendingCount = 0;
  std::uint64_t m_droppedCount = 0;
  std::uint64_t m_rejectedCount = 0;
};

} // namespace Neuron
