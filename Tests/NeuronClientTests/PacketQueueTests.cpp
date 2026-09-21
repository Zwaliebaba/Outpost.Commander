#include "pch.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
inline constexpr std::size_t MARKED_BYTES = 24;
inline constexpr std::uint8_t PRODUCER_COUNT = 4;
inline constexpr std::uint16_t PER_PRODUCER_COUNT = 500;
inline constexpr std::chrono::seconds DRAIN_DEADLINE{5};

/// Stamps a datagram with its own identity and then DERIVES EVERY REMAINING BYTE FROM IT, so that
/// a half-written datagram is detectable rather than merely unlikely. A memcpy the mutex failed
/// to protect leaves a body that does not match its own header, and Matches is what sees that.
void Mark(std::span<std::byte> _bytes, std::uint8_t _producer, std::uint16_t _sequence) noexcept
{
  _bytes[0] = static_cast<std::byte>(_producer);
  _bytes[1] = static_cast<std::byte>(_sequence & 0xFFu);
  _bytes[2] = static_cast<std::byte>((_sequence >> 8) & 0xFFu);
  for (std::size_t index = 3; index < _bytes.size(); ++index)
  {
    _bytes[index] = static_cast<std::byte>((_producer * 31u + _sequence * 7u + index * 13u) & 0xFFu);
  }
}

[[nodiscard]] std::uint8_t ProducerOf(std::span<const std::byte> _bytes) noexcept
{
  return std::to_integer<std::uint8_t>(_bytes[0]);
}

[[nodiscard]] std::uint16_t SequenceOf(std::span<const std::byte> _bytes) noexcept
{
  return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(_bytes[1]) |
                                    (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(_bytes[2])) << 8));
}

[[nodiscard]] bool Matches(std::span<const std::byte> _bytes) noexcept
{
  if (_bytes.size() != MARKED_BYTES)
  {
    return false;
  }
  std::array<std::byte, MARKED_BYTES> expected{};
  Mark(expected, ProducerOf(_bytes), SequenceOf(_bytes));
  return std::equal(expected.begin(), expected.end(), _bytes.begin(), _bytes.end());
}

/// One numbered byte, for the tests that only care which datagram came out.
void PushNumbered(Neuron::PacketQueue& _queue, std::uint8_t _number) noexcept
{
  const std::array<std::byte, 1> datagram{static_cast<std::byte>(_number)};
  _queue.Push(datagram);
}
} // namespace

TEST_CLASS(PacketQueueBasics)
{
public:
  TEST_METHOD(ZeroSizesAreClampedRatherThanLeftToDivideByZero)
  {
    const Neuron::PacketQueue queue{0, 0};

    Assert::AreEqual(static_cast<std::size_t>(1), queue.SlotCount());
    Assert::AreEqual(static_cast<std::size_t>(1), queue.SlotBytes());
  }

  TEST_METHOD(ADatagramGoesInAndComesOutUnchanged)
  {
    Neuron::PacketQueue queue{4, MARKED_BYTES};
    std::array<std::byte, MARKED_BYTES> sent{};
    Mark(sent, std::uint8_t{7}, std::uint16_t{1234});

    queue.Push(sent);

    std::array<std::byte, MARKED_BYTES> received{};
    std::size_t byteCount = 0;
    Assert::IsTrue(queue.Drain(received, byteCount));
    Assert::AreEqual(MARKED_BYTES, byteCount);
    Assert::IsTrue(sent == received);
  }

  TEST_METHOD(DatagramsComeOutOldestFirst)
  {
    Neuron::PacketQueue queue{4, 1};
    for (std::uint8_t number = 1; number <= 3; ++number)
    {
      PushNumbered(queue, number);
    }

    std::array<std::byte, 1> received{};
    std::size_t byteCount = 0;
    for (std::uint8_t number = 1; number <= 3; ++number)
    {
      Assert::IsTrue(queue.Drain(received, byteCount));
      Assert::AreEqual(number, std::to_integer<std::uint8_t>(received[0]));
    }
    Assert::IsFalse(queue.Drain(received, byteCount));
  }

  TEST_METHOD(DrainOnAnEmptyQueueIsFalseAndIsHowADrainLoopEnds)
  {
    Neuron::PacketQueue queue{4, 8};

    std::array<std::byte, 8> received{};
    std::size_t byteCount = 99;
    Assert::IsFalse(queue.Drain(received, byteCount));
    Assert::AreEqual(static_cast<std::size_t>(99), byteCount, L"a failed drain must not touch the out-parameter");
  }

  TEST_METHOD(PendingCountTracksPushesAndDrains)
  {
    Neuron::PacketQueue queue{4, 1};
    Assert::AreEqual(static_cast<std::size_t>(0), queue.PendingCount());

    PushNumbered(queue, std::uint8_t{1});
    PushNumbered(queue, std::uint8_t{2});
    Assert::AreEqual(static_cast<std::size_t>(2), queue.PendingCount());

    std::array<std::byte, 1> received{};
    std::size_t byteCount = 0;
    Assert::IsTrue(queue.Drain(received, byteCount));
    Assert::AreEqual(static_cast<std::size_t>(1), queue.PendingCount());
  }

  /// Each datagram reports ITS OWN length. A queue that reported the slot's would hand every
  /// decoder a slot full of whatever the previous datagram left behind.
  TEST_METHOD(EachDatagramKeepsItsOwnLength)
  {
    Neuron::PacketQueue queue{4, 16};
    const std::array<std::byte, 3> shortOne{std::byte{1}, std::byte{2}, std::byte{3}};
    const std::array<std::byte, 16> longOne{};

    queue.Push(shortOne);
    queue.Push(longOne);

    std::array<std::byte, 16> received{};
    std::size_t byteCount = 0;
    Assert::IsTrue(queue.Drain(received, byteCount));
    Assert::AreEqual(static_cast<std::size_t>(3), byteCount);
    Assert::IsTrue(queue.Drain(received, byteCount));
    Assert::AreEqual(static_cast<std::size_t>(16), byteCount);
  }

  TEST_METHOD(AnEmptyDatagramIsRejectedAndCounted)
  {
    Neuron::PacketQueue queue{4, 8};

    queue.Push(std::span<const std::byte>{});

    Assert::AreEqual(static_cast<std::size_t>(0), queue.PendingCount());
    Assert::AreEqual(std::uint64_t{1}, queue.RejectedCount());
    Assert::AreEqual(std::uint64_t{0}, queue.DroppedCount());
  }

  TEST_METHOD(ADatagramLargerThanASlotIsRejectedAndCounted)
  {
    Neuron::PacketQueue queue{4, 8};
    const std::array<std::byte, 9> tooBig{};

    queue.Push(tooBig);

    Assert::AreEqual(static_cast<std::size_t>(0), queue.PendingCount());
    Assert::AreEqual(std::uint64_t{1}, queue.RejectedCount());
  }

  /// A drain buffer smaller than a slot is the caller's bug. It consumes the datagram anyway,
  /// because leaving it in place would turn that bug into a queue that fills once and never
  /// empties again -- and a wedged queue looks like a dead network.
  TEST_METHOD(ADrainBufferTooSmallConsumesAndCountsRatherThanWedging)
  {
    Neuron::PacketQueue queue{4, 8};
    const std::array<std::byte, 8> datagram{};
    queue.Push(datagram);

    std::array<std::byte, 4> tooSmall{};
    std::size_t byteCount = 0;
    Assert::IsFalse(queue.Drain(tooSmall, byteCount));

    Assert::AreEqual(static_cast<std::size_t>(0), queue.PendingCount());
    Assert::AreEqual(std::uint64_t{1}, queue.RejectedCount());
  }
};

TEST_CLASS(PacketQueueOverflow)
{
public:
  TEST_METHOD(NothingIsDroppedUntilTheQueueIsFull)
  {
    Neuron::PacketQueue queue{3, 1};
    for (std::uint8_t number = 1; number <= 3; ++number)
    {
      PushNumbered(queue, number);
    }

    Assert::AreEqual(static_cast<std::size_t>(3), queue.PendingCount());
    Assert::AreEqual(std::uint64_t{0}, queue.DroppedCount());
  }

  /// The oldest goes, not the newest, and not the pool thread's time. Every snapshot is
  /// self-contained (ADR-003), so of two the newer is strictly the more useful.
  TEST_METHOD(AFullQueueDropsTheOldestAndCountsIt)
  {
    Neuron::PacketQueue queue{3, 1};
    for (std::uint8_t number = 1; number <= 5; ++number)
    {
      PushNumbered(queue, number);
    }

    Assert::AreEqual(static_cast<std::size_t>(3), queue.PendingCount());
    Assert::AreEqual(std::uint64_t{2}, queue.DroppedCount());

    std::array<std::byte, 1> received{};
    std::size_t byteCount = 0;
    for (std::uint8_t number = 3; number <= 5; ++number)
    {
      Assert::IsTrue(queue.Drain(received, byteCount));
      Assert::AreEqual(number, std::to_integer<std::uint8_t>(received[0]), L"the queue kept the wrong end");
    }
  }

  TEST_METHOD(AQueueThatWrappedStillDrainsInOrder)
  {
    Neuron::PacketQueue queue{3, 1};
    std::array<std::byte, 1> received{};
    std::size_t byteCount = 0;

    // Push and drain enough to carry the head past the end of the ring twice over.
    for (std::uint8_t number = 1; number <= 10; ++number)
    {
      PushNumbered(queue, number);
      Assert::IsTrue(queue.Drain(received, byteCount));
      Assert::AreEqual(number, std::to_integer<std::uint8_t>(received[0]));
    }
    Assert::AreEqual(std::uint64_t{0}, queue.DroppedCount());
  }
};

TEST_CLASS(PacketQueueThreading)
{
public:
  /// The property the seam rests on: several threads pushing at once lose nothing, tear nothing,
  /// and each one's datagrams come out in the order that thread sent them. The queue makes no
  /// promise about the order BETWEEN threads and this asserts none -- there is no such order to
  /// promise, and a test that invented one would be pinning the scheduler.
  TEST_METHOD(ConcurrentPushesLoseNothingAndTearNothing)
  {
    const std::size_t total = static_cast<std::size_t>(PRODUCER_COUNT) * PER_PRODUCER_COUNT;
    Neuron::PacketQueue queue{total, MARKED_BYTES};

    std::vector<std::thread> producers;
    producers.reserve(PRODUCER_COUNT);
    for (std::uint8_t producer = 0; producer < PRODUCER_COUNT; ++producer)
    {
      producers.emplace_back(
        [&queue, producer]
        {
          std::array<std::byte, MARKED_BYTES> datagram{};
          for (std::uint16_t sequence = 0; sequence < PER_PRODUCER_COUNT; ++sequence)
          {
            Mark(datagram, producer, sequence);
            queue.Push(datagram);
          }
        });
    }
    for (std::thread& producer : producers)
    {
      producer.join();
    }

    Assert::AreEqual(std::uint64_t{0}, queue.DroppedCount(), L"the queue was sized to hold every datagram");
    Assert::AreEqual(std::uint64_t{0}, queue.RejectedCount());
    Assert::AreEqual(total, queue.PendingCount());

    std::vector<std::vector<bool>> seen(PRODUCER_COUNT, std::vector<bool>(PER_PRODUCER_COUNT, false));
    std::vector<int> lastSequence(PRODUCER_COUNT, -1);
    std::array<std::byte, MARKED_BYTES> received{};
    std::size_t byteCount = 0;
    std::size_t drained = 0;

    while (queue.Drain(received, byteCount))
    {
      const std::span<const std::byte> datagram{received.data(), byteCount};
      Assert::IsTrue(Matches(datagram), L"a datagram came out torn");

      const std::uint8_t producer = ProducerOf(datagram);
      const std::uint16_t sequence = SequenceOf(datagram);
      Assert::IsTrue(producer < PRODUCER_COUNT);
      Assert::IsTrue(sequence < PER_PRODUCER_COUNT);
      Assert::IsFalse(seen[producer][sequence], L"a datagram came out twice");
      Assert::IsTrue(static_cast<int>(sequence) > lastSequence[producer], L"one producer's datagrams came out of order");

      seen[producer][sequence] = true;
      lastSequence[producer] = static_cast<int>(sequence);
      ++drained;
    }

    Assert::AreEqual(total, drained, L"a datagram was lost");
  }

  /// The arrangement as it will actually run: something pushing from a pool thread while the
  /// frame drains. The deadline is what turns a lost datagram into a failure rather than a hang.
  TEST_METHOD(APushingThreadAndADrainingThreadAgree)
  {
    Neuron::PacketQueue queue{256, MARKED_BYTES};

    std::thread producer(
      [&queue]
      {
        std::array<std::byte, MARKED_BYTES> datagram{};
        for (std::uint16_t sequence = 0; sequence < PER_PRODUCER_COUNT; ++sequence)
        {
          Mark(datagram, std::uint8_t{1}, sequence);
          queue.Push(datagram);
          std::this_thread::yield();
        }
      });

    std::array<std::byte, MARKED_BYTES> received{};
    std::size_t byteCount = 0;
    std::size_t drained = 0;
    int lastSequence = -1;
    const auto deadline = std::chrono::steady_clock::now() + DRAIN_DEADLINE;
    while (drained < PER_PRODUCER_COUNT && std::chrono::steady_clock::now() < deadline)
    {
      if (!queue.Drain(received, byteCount))
      {
        std::this_thread::yield();
        continue;
      }
      const std::span<const std::byte> datagram{received.data(), byteCount};
      Assert::IsTrue(Matches(datagram), L"a datagram came out torn");
      const int sequence = static_cast<int>(SequenceOf(datagram));
      Assert::IsTrue(sequence > lastSequence, L"the producer's datagrams came out of order");
      lastSequence = sequence;
      ++drained;
    }
    producer.join();

    Assert::AreEqual(static_cast<std::size_t>(PER_PRODUCER_COUNT), drained);
    Assert::AreEqual(std::uint64_t{0}, queue.DroppedCount(), L"the drain could not keep up with 256 slots of slack");
  }
};

} // namespace NeuronClientTests
