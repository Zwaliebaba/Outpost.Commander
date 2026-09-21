#include "pch.h"

#include "DatagramTransport.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace Neuron
{

namespace
{
// R10 permits a local alias where a using-directive is not allowed, which is everywhere but a
// unit-test framework.
namespace Foundation = winrt::Windows::Foundation;
namespace Networking = winrt::Windows::Networking;
namespace Sockets = winrt::Windows::Networking::Sockets;
namespace Streams = winrt::Windows::Storage::Streams;
} // namespace

/// Everything the pool thread and the frame thread share, held by one shared_ptr so that neither
/// can outlive the other's view of it.
///
/// THE HANDLER HOLDS A weak_ptr AND NOT A shared_ptr, and that is not caution -- the socket owns
/// the delegate, the delegate would own this, and this owns the socket. A strong reference there
/// is a cycle that never releases the socket.
struct SocketBinding
{
  Sockets::DatagramSocket socket{nullptr};
  Streams::DataWriter writer{nullptr};
  winrt::event_token messageToken{};

  /// Held across the whole of a delivery, and taken by Close, so that Close cannot return while a
  /// datagram is still being copied into a queue the caller is about to destroy.
  std::mutex deliveryMutex;

  /// Read under deliveryMutex. Null once Close has run.
  PacketQueue* queue = nullptr;

  /// Sized to the queue's slot when the transport opened, because the pool thread may not
  /// allocate. deliveryMutex is what makes one buffer enough.
  std::vector<std::byte> scratch;

  std::atomic<TransportState> state{TransportState::Closed};
  std::atomic<bool> storeInFlight{false};
  std::atomic<std::uint64_t> skippedSendCount{0};
  std::atomic<std::uint64_t> oversizedCount{0};
};

namespace
{
/// The whole of what runs on the thread pool thread: lock, copy, return.
void DeliverDatagram(const std::weak_ptr<SocketBinding>& _weak, const Sockets::DatagramSocketMessageReceivedEventArgs& _args) noexcept
{
  const std::shared_ptr<SocketBinding> binding = _weak.lock();
  if (!binding)
  {
    return;
  }

  const std::lock_guard<std::mutex> guard(binding->deliveryMutex);
  if (binding->queue == nullptr)
  {
    return;
  }

  try
  {
    const Streams::DataReader reader = _args.GetDataReader();
    const std::uint32_t availableBytes = reader.UnconsumedBufferLength();
    if (availableBytes == 0)
    {
      return;
    }
    if (availableBytes > binding->scratch.size())
    {
      // Larger than any slot can hold, so there is nowhere to put it and nothing to truncate it
      // into that would still decode. Counted here rather than swallowed.
      binding->oversizedCount.fetch_add(1);
      return;
    }

    auto* const first = reinterpret_cast<std::uint8_t*>(binding->scratch.data());
    reader.ReadBytes(winrt::array_view<std::uint8_t>(first, first + availableBytes));
    binding->queue->Push(std::span<const std::byte>{binding->scratch.data(), availableBytes});
  }
  catch (const winrt::hresult_error&)
  {
    // A datagram that cannot be read is a datagram that did not arrive. There is nothing to
    // report to from here and nothing to retry: the next snapshot is fifty milliseconds away.
  }
}
} // namespace

DatagramTransport::DatagramTransport(PacketQueue& _queue) noexcept
  : m_queue(_queue)
{
}

DatagramTransport::~DatagramTransport() noexcept
{
  Close();
}

bool DatagramTransport::Open(std::string_view _hostAddress, std::uint16_t _port) noexcept
{
  if (m_binding)
  {
    return false;
  }

  try
  {
    auto binding = std::make_shared<SocketBinding>();
    binding->queue = &m_queue;
    binding->scratch.resize(m_queue.SlotBytes());
    binding->state.store(TransportState::Opening);
    binding->socket = Sockets::DatagramSocket();

    const std::weak_ptr<SocketBinding> weak = binding;
    binding->messageToken =
      binding->socket.MessageReceived([weak](const Sockets::DatagramSocket&, const Sockets::DatagramSocketMessageReceivedEventArgs& _args)
                                      { DeliverDatagram(weak, _args); });

    const Networking::HostName host{winrt::to_hstring(_hostAddress)};
    const winrt::hstring serviceName{std::to_wstring(_port)};
    binding->socket.ConnectAsync(host, serviceName)
      .Completed(
        [weak](auto&&, auto&& _status)
        {
          const std::shared_ptr<SocketBinding> held = weak.lock();
          if (!held)
          {
            return;
          }
          if (_status != Foundation::AsyncStatus::Completed)
          {
            held->state.store(TransportState::Failed);
            return;
          }
          try
          {
            // Written before the state is published, and read only after Send has seen Ready: the
            // two atomic accesses are what order this store against that load.
            held->writer = Streams::DataWriter{held->socket.OutputStream()};
            held->state.store(TransportState::Ready);
          }
          catch (const winrt::hresult_error&)
          {
            held->state.store(TransportState::Failed);
          }
        });

    m_binding = std::move(binding);
    return true;
  }
  catch (const winrt::hresult_error&)
  {
    m_binding.reset();
    return false;
  }
}

void DatagramTransport::Close() noexcept
{
  const std::shared_ptr<SocketBinding> binding = std::move(m_binding);
  if (!binding)
  {
    return;
  }

  {
    // A delivery already inside DeliverDatagram finishes here, so the queue is not written to
    // after this scope ends.
    const std::lock_guard<std::mutex> guard(binding->deliveryMutex);
    binding->queue = nullptr;
  }

  try
  {
    if (binding->messageToken)
    {
      binding->socket.MessageReceived(binding->messageToken);
      binding->messageToken = {};
    }
    binding->socket.Close();
  }
  catch (const winrt::hresult_error&)
  {
    // Closing a socket that is already gone is not a failure worth propagating out of a
    // destructor path.
  }

  binding->state.store(TransportState::Closed);
}

TransportState DatagramTransport::State() const noexcept
{
  return m_binding ? m_binding->state.load() : TransportState::Closed;
}

bool DatagramTransport::Send(std::span<const std::byte> _bytes) noexcept
{
  if (!m_binding || _bytes.empty() || m_binding->state.load() != TransportState::Ready)
  {
    return false;
  }

  // The DataWriter's buffer belongs to the store that is running, so the flag is taken BEFORE
  // anything is written into it rather than after.
  bool idle = false;
  if (!m_binding->storeInFlight.compare_exchange_strong(idle, true))
  {
    m_binding->skippedSendCount.fetch_add(1);
    return false;
  }

  try
  {
    const auto* const first = reinterpret_cast<const std::uint8_t*>(_bytes.data());
    m_binding->writer.WriteBytes(winrt::array_view<const std::uint8_t>(first, first + _bytes.size()));

    const std::weak_ptr<SocketBinding> weak = m_binding;
    m_binding->writer.StoreAsync().Completed(
      [weak](auto&&, auto&&)
      {
        if (const std::shared_ptr<SocketBinding> held = weak.lock())
        {
          held->storeInFlight.store(false);
        }
      });
    return true;
  }
  catch (const winrt::hresult_error&)
  {
    m_binding->storeInFlight.store(false);
    return false;
  }
}

std::uint64_t DatagramTransport::SkippedSendCount() const noexcept
{
  return m_binding ? m_binding->skippedSendCount.load() : 0;
}

std::uint64_t DatagramTransport::OversizedCount() const noexcept
{
  return m_binding ? m_binding->oversizedCount.load() : 0;
}

} // namespace Neuron
