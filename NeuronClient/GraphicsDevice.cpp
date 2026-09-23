#include "pch.h"

#include "GraphicsDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>

// The link dependencies travel with the code that needs them rather than with each project that
// happens to link this library -- the same reason WinsockTransport carries ws2_32 itself.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace Neuron
{

/// Everything Direct3D, in one translation unit. R12: every interface is a `winrt::com_ptr` and
/// there is not an `AddRef` or a `Release` anywhere below.
struct DeviceBinding
{
  winrt::com_ptr<IDXGIFactory4> factory;
  winrt::com_ptr<ID3D12Device> device;
  winrt::com_ptr<ID3D12CommandQueue> queue;

  /// ONE PER FRAME IN FLIGHT, and that is not an optimization. An allocator's memory cannot be
  /// reused while the GPU is still reading commands out of it, so a single allocator would force
  /// a full wait every frame and there would be no frames in flight at all.
  std::array<winrt::com_ptr<ID3D12CommandAllocator>, GraphicsDevice::FRAMES_IN_FLIGHT> allocators;

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::com_ptr<ID3D12Fence> fence;

  /// The value each frame slot signaled, so a slot waits only for its OWN previous work.
  std::array<std::uint64_t, GraphicsDevice::FRAMES_IN_FLIGHT> fenceValues{};

  winrt::handle fenceEvent;

  /// FOUR TIMESTAMPS PER FRAME IN FLIGHT -- one either side of the frame's command list, and the two
  /// marks between them that split it into world, present and interface. A slot's
  /// set is only safe to read once the GPU has passed that slot's fence, which is exactly the wait
  /// PresentedFrame already performs, so reading them costs no extra synchronization.
  winrt::com_ptr<ID3D12QueryHeap> timestampHeap;

  /// Persistently mapped. A readback buffer is host-visible and mapping it once is cheaper and no
  /// less correct than mapping around each read.
  winrt::com_ptr<ID3D12Resource> timestampReadback;
  const std::uint64_t* timestampSamples = nullptr;

  /// Ticks a second, from the queue. Zero if the device refused to report it, which is the case
  /// where the measurement is simply unavailable rather than wrong.
  std::uint64_t timestampFrequency = 0;
  std::uint64_t lastFrameGpuMicroseconds = 0;
  GpuFrameSplit lastFrameSplit{};
  bool lastFrameSplitValid = false;

  std::uint32_t frameIndex = 0;
  std::uint32_t highestShaderModel = 0;
  DeviceState state = DeviceState::Absent;
  HRESULT lastHresult = S_OK;
  bool recording = false;
};

namespace
{
/// The floor this game targets. 11_0 is every part that runs Windows 10, and nothing in this
/// renderer asks for more.
inline constexpr D3D_FEATURE_LEVEL FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

/// One either side of the frame's command list, so the difference of the outer two is the whole
/// frame's GPU time, and two marks between them (`MarkWorldDrawn`, `MarkPresentScaled`).
inline constexpr std::uint32_t TIMESTAMPS_PER_FRAME = static_cast<std::uint32_t>(GPU_FRAME_MARKS);

/// Where each timestamp sits in a slot's set, in the order the frame writes them.
inline constexpr std::uint32_t MARK_BEGUN = 0;
inline constexpr std::uint32_t MARK_WORLD_DRAWN = 1;
inline constexpr std::uint32_t MARK_PRESENT_SCALED = 2;
inline constexpr std::uint32_t MARK_ENDED = 3;

[[nodiscard]] bool IsDeviceLost(HRESULT _result) noexcept
{
  return (_result == DXGI_ERROR_DEVICE_REMOVED) || (_result == DXGI_ERROR_DEVICE_RESET);
}
} // namespace

GraphicsDevice::GraphicsDevice() noexcept
  : m_binding{std::make_shared<DeviceBinding>()}
{
}

GraphicsDevice::~GraphicsDevice() noexcept
{
  Destroy();
}

bool GraphicsDevice::Create() noexcept
{
  DeviceBinding& binding = *m_binding;
  binding.state = DeviceState::Absent;
  binding.lastHresult = S_OK;

  UINT factoryFlags = 0;
#if defined(_DEBUG)
  // The debug layer is Debug-only and its absence is not a failure: a machine without the Graphics
  // Tools optional feature installed simply does not have it, and refusing to start there would
  // make the Debug build the one that runs in fewer places than Release.
  {
    winrt::com_ptr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(winrt::guid_of<ID3D12Debug>(), debugController.put_void())))
    {
      debugController->EnableDebugLayer();
      factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
  }
#endif

  binding.lastHresult = CreateDXGIFactory2(factoryFlags, winrt::guid_of<IDXGIFactory4>(), binding.factory.put_void());
  if (FAILED(binding.lastHresult))
  {
    binding.state = DeviceState::Failed;
    return false;
  }

  // Hardware first. D3D12CreateDevice with a null adapter takes the default one.
  binding.lastHresult = D3D12CreateDevice(nullptr, FEATURE_LEVEL, winrt::guid_of<ID3D12Device>(), binding.device.put_void());
  if (FAILED(binding.lastHresult))
  {
    // Then WARP. See the header: a virtual machine or a remote session has no hardware adapter,
    // and a client that will not start there is one nobody can debug.
    winrt::com_ptr<IDXGIAdapter> warpAdapter;
    binding.lastHresult = binding.factory->EnumWarpAdapter(winrt::guid_of<IDXGIAdapter>(), warpAdapter.put_void());
    if (SUCCEEDED(binding.lastHresult))
    {
      binding.lastHresult = D3D12CreateDevice(warpAdapter.get(), FEATURE_LEVEL, winrt::guid_of<ID3D12Device>(), binding.device.put_void());
    }
    if (FAILED(binding.lastHresult))
    {
      binding.state = DeviceState::Failed;
      return false;
    }
  }

  const D3D12_COMMAND_QUEUE_DESC queueDescription{
    .Type = D3D12_COMMAND_LIST_TYPE_DIRECT, .Priority = 0, .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE, .NodeMask = 0};
  binding.lastHresult =
    binding.device->CreateCommandQueue(&queueDescription, winrt::guid_of<ID3D12CommandQueue>(), binding.queue.put_void());
  if (FAILED(binding.lastHresult))
  {
    binding.state = DeviceState::Failed;
    return false;
  }

  for (winrt::com_ptr<ID3D12CommandAllocator>& allocator : binding.allocators)
  {
    binding.lastHresult = binding.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, winrt::guid_of<ID3D12CommandAllocator>(),
                                                                 allocator.put_void());
    if (FAILED(binding.lastHresult))
    {
      binding.state = DeviceState::Failed;
      return false;
    }
  }

  binding.lastHresult = binding.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, binding.allocators[0].get(), nullptr,
                                                          winrt::guid_of<ID3D12GraphicsCommandList>(), binding.commandList.put_void());
  if (FAILED(binding.lastHresult))
  {
    binding.state = DeviceState::Failed;
    return false;
  }

  // A command list is created OPEN. Closing it here means BeginFrame's Reset is symmetric with
  // EndFrameAndSubmit's Close on the very first frame as well as every one after.
  binding.lastHresult = binding.commandList->Close();
  if (FAILED(binding.lastHresult))
  {
    binding.state = DeviceState::Failed;
    return false;
  }

  binding.fenceValues.fill(0);
  binding.lastHresult = binding.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, winrt::guid_of<ID3D12Fence>(), binding.fence.put_void());
  if (FAILED(binding.lastHresult))
  {
    binding.state = DeviceState::Failed;
    return false;
  }
  binding.fenceValues[0] = 1;

  binding.fenceEvent.attach(CreateEventW(nullptr, FALSE, FALSE, nullptr));
  if (!binding.fenceEvent)
  {
    binding.lastHresult = HRESULT_FROM_WIN32(GetLastError());
    binding.state = DeviceState::Failed;
    return false;
  }

  // Asked once, at creation. CheckFeatureSupport with SHADER_MODEL is an in-out call: it is given
  // the highest model to ask about and lowers HighestShaderModel to what the device actually has.
  {
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{.HighestShaderModel = D3D_SHADER_MODEL_6_7};
    if (SUCCEEDED(binding.device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
    {
      binding.highestShaderModel = static_cast<std::uint32_t>(shaderModel.HighestShaderModel);
    }
  }

  // The timestamp facility. A FAILURE HERE IS NOT A FAILURE TO CREATE A DEVICE: an adapter that
  // will not serve timestamps still renders, and the only thing lost is a measurement. Every path
  // below leaves timestampFrequency at zero, which is how LastFrameGpuMicroseconds reports "no
  // figure" rather than reporting a wrong one.
  {
    const D3D12_QUERY_HEAP_DESC heapDescription{
      .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = TIMESTAMPS_PER_FRAME * FRAMES_IN_FLIGHT, .NodeMask = 0};
    if (SUCCEEDED(binding.device->CreateQueryHeap(&heapDescription, winrt::guid_of<ID3D12QueryHeap>(), binding.timestampHeap.put_void())))
    {
      const D3D12_HEAP_PROPERTIES readbackProperties{.Type = D3D12_HEAP_TYPE_READBACK,
                                                     .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                     .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                                     .CreationNodeMask = 1,
                                                     .VisibleNodeMask = 1};
      const D3D12_RESOURCE_DESC readbackDescription{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                                    .Alignment = 0,
                                                    .Width = sizeof(std::uint64_t) * TIMESTAMPS_PER_FRAME * FRAMES_IN_FLIGHT,
                                                    .Height = 1,
                                                    .DepthOrArraySize = 1,
                                                    .MipLevels = 1,
                                                    .Format = DXGI_FORMAT_UNKNOWN,
                                                    .SampleDesc = {.Count = 1, .Quality = 0},
                                                    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                                    .Flags = D3D12_RESOURCE_FLAG_NONE};
      if (SUCCEEDED(binding.device->CreateCommittedResource(&readbackProperties, D3D12_HEAP_FLAG_NONE, &readbackDescription,
                                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, winrt::guid_of<ID3D12Resource>(),
                                                            binding.timestampReadback.put_void())))
      {
        void* mapped = nullptr;
        if (SUCCEEDED(binding.timestampReadback->Map(0, nullptr, &mapped)))
        {
          binding.timestampSamples = static_cast<const std::uint64_t*>(mapped);

          UINT64 frequency = 0;
          if (SUCCEEDED(binding.queue->GetTimestampFrequency(&frequency)) && (frequency != 0))
          {
            binding.timestampFrequency = frequency;
          }
        }
      }
    }
  }

  binding.frameIndex = 0;
  binding.recording = false;
  binding.state = DeviceState::Ready;
  return true;
}

void GraphicsDevice::Destroy() noexcept
{
  DeviceBinding& binding = *m_binding;
  if (binding.state == DeviceState::Ready)
  {
    // Before anything is released. A device torn down with work in flight is a use-after-free
    // with a driver in the middle of it.
    WaitForGpu();
  }

  binding.commandList = nullptr;
  for (winrt::com_ptr<ID3D12CommandAllocator>& allocator : binding.allocators)
  {
    allocator = nullptr;
  }

  // The pointer goes first: it aliases the buffer's mapped range and must not outlive it. A
  // readback buffer mapped for its whole life is unmapped by its own release, so there is no
  // matching Unmap call to forget.
  binding.timestampSamples = nullptr;
  binding.timestampReadback = nullptr;
  binding.timestampHeap = nullptr;
  binding.timestampFrequency = 0;
  binding.lastFrameGpuMicroseconds = 0;
  binding.lastFrameSplitValid = false;
  binding.fence = nullptr;
  binding.fenceEvent.close();
  binding.queue = nullptr;
  binding.device = nullptr;
  binding.factory = nullptr;
  binding.recording = false;
  binding.state = DeviceState::Absent;
}

DeviceState GraphicsDevice::State() const noexcept
{
  return m_binding->state;
}

std::int32_t GraphicsDevice::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

std::uint32_t GraphicsDevice::HighestShaderModel() const noexcept
{
  return m_binding->highestShaderModel;
}

std::uint32_t GraphicsDevice::FrameIndex() const noexcept
{
  return m_binding->frameIndex;
}

void GraphicsDevice::WaitForGpu() noexcept
{
  DeviceBinding& binding = *m_binding;
  if ((binding.state != DeviceState::Ready) || !binding.fence || !binding.queue)
  {
    return;
  }

  const std::uint64_t target = binding.fenceValues[binding.frameIndex];
  if (FAILED(binding.queue->Signal(binding.fence.get(), target)))
  {
    return;
  }

  if (binding.fence->GetCompletedValue() < target)
  {
    if (SUCCEEDED(binding.fence->SetEventOnCompletion(target, binding.fenceEvent.get())))
    {
      static_cast<void>(WaitForSingleObjectEx(binding.fenceEvent.get(), INFINITE, FALSE));
    }
  }

  ++binding.fenceValues[binding.frameIndex];
}

void GraphicsDevice::PresentedFrame() noexcept
{
  DeviceBinding& binding = *m_binding;
  if (binding.state != DeviceState::Ready)
  {
    return;
  }

  const std::uint64_t signaled = binding.fenceValues[binding.frameIndex];
  if (FAILED(binding.queue->Signal(binding.fence.get(), signaled)))
  {
    return;
  }

  binding.frameIndex = (binding.frameIndex + 1) % FRAMES_IN_FLIGHT;

  // THIS SLOT'S OWN EARLIER WORK, and nothing else. Waiting for the frame just submitted would
  // make the two allocators pointless; waiting for this slot's previous frame is exactly what
  // keeps the CPU one frame ahead and never two.
  if (binding.fence->GetCompletedValue() < binding.fenceValues[binding.frameIndex])
  {
    if (SUCCEEDED(binding.fence->SetEventOnCompletion(binding.fenceValues[binding.frameIndex], binding.fenceEvent.get())))
    {
      static_cast<void>(WaitForSingleObjectEx(binding.fenceEvent.get(), INFINITE, FALSE));
    }
  }

  // THE SLOT JUST WAITED FOR IS THE ONE WHOSE TIMESTAMPS ARE NOW SAFE. Its fence has passed, so the
  // resolve that wrote them has completed; the pair belongs to that slot's PREVIOUS frame, which is
  // the most recent frame this device can honestly report on.
  if ((binding.timestampSamples != nullptr) && (binding.timestampFrequency != 0))
  {
    const std::uint32_t first = binding.frameIndex * TIMESTAMPS_PER_FRAME;
    const std::uint64_t begun = binding.timestampSamples[first + MARK_BEGUN];
    const std::uint64_t ended = binding.timestampSamples[first + MARK_ENDED];
    if (ended > begun)
    {
      constexpr std::uint64_t MICROSECONDS_PER_SECOND = 1000000;
      binding.lastFrameGpuMicroseconds = ((ended - begun) * MICROSECONDS_PER_SECOND) / binding.timestampFrequency;
    }

    // A FRAME THAT SKIPPED A MARK leaves that slot holding an older frame's value, which is earlier
    // than this frame's begin -- so SplitGpuFrame refuses it on order, and no stale split is reported.
    std::uint64_t ticks[GPU_FRAME_MARKS]{};
    for (std::uint32_t mark = 0; mark < TIMESTAMPS_PER_FRAME; ++mark)
    {
      ticks[mark] = binding.timestampSamples[first + mark];
    }
    binding.lastFrameSplitValid = SplitGpuFrame(ticks, binding.timestampFrequency, binding.lastFrameSplit);
  }

  binding.fenceValues[binding.frameIndex] = signaled + 1;
}

std::uint64_t GraphicsDevice::LastFrameGpuMicroseconds() const noexcept
{
  return m_binding->lastFrameGpuMicroseconds;
}

bool GraphicsDevice::LastFrameGpuSplit(GpuFrameSplit& _outSplit) const noexcept
{
  if (!m_binding->lastFrameSplitValid)
  {
    return false;
  }
  _outSplit = m_binding->lastFrameSplit;
  return true;
}

void GraphicsDevice::MarkWorldDrawn() noexcept
{
  WriteMark(MARK_WORLD_DRAWN);
}

void GraphicsDevice::MarkPresentScaled() noexcept
{
  WriteMark(MARK_PRESENT_SCALED);
}

void GraphicsDevice::WriteMark(std::uint32_t _mark) noexcept
{
  DeviceBinding& binding = *m_binding;
  if (binding.recording && binding.timestampHeap)
  {
    binding.commandList->EndQuery(binding.timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                  (binding.frameIndex * TIMESTAMPS_PER_FRAME) + _mark);
  }
}

::IUnknown* GraphicsDevice::CommandQueueUnknown() const noexcept
{
  return m_binding->queue.get();
}

::IUnknown* GraphicsDevice::DeviceUnknown() const noexcept
{
  return m_binding->device.get();
}

::IUnknown* GraphicsDevice::CommandListUnknown() const noexcept
{
  return m_binding->commandList.get();
}

bool GraphicsDevice::BeginFrame() noexcept
{
  DeviceBinding& binding = *m_binding;
  if ((binding.state != DeviceState::Ready) || binding.recording)
  {
    return false;
  }

  HRESULT result = binding.allocators[binding.frameIndex]->Reset();
  if (SUCCEEDED(result))
  {
    result = binding.commandList->Reset(binding.allocators[binding.frameIndex].get(), nullptr);
  }

  if (FAILED(result))
  {
    binding.lastHresult = result;

    // A PATH, NOT A CRASH. The frame after a device loss is meant to rebuild.
    binding.state = IsDeviceLost(result) ? DeviceState::Removed : DeviceState::Failed;
    return false;
  }

  if (binding.timestampHeap)
  {
    binding.commandList->EndQuery(binding.timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                  (binding.frameIndex * TIMESTAMPS_PER_FRAME) + MARK_BEGUN);
  }

  binding.recording = true;
  return true;
}

bool GraphicsDevice::EndFrameAndSubmit() noexcept
{
  DeviceBinding& binding = *m_binding;
  if (!binding.recording)
  {
    return false;
  }
  binding.recording = false;

  if (binding.timestampHeap)
  {
    const std::uint32_t first = binding.frameIndex * TIMESTAMPS_PER_FRAME;
    binding.commandList->EndQuery(binding.timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, first + MARK_ENDED);

    // Resolved on the GPU into the readback buffer, in the same command list. Doing it here rather
    // than in a later frame is what makes the pair readable the moment this slot's fence passes.
    binding.commandList->ResolveQueryData(binding.timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, first, TIMESTAMPS_PER_FRAME,
                                          binding.timestampReadback.get(), sizeof(std::uint64_t) * first);
  }

  const HRESULT result = binding.commandList->Close();
  if (FAILED(result))
  {
    binding.lastHresult = result;
    binding.state = IsDeviceLost(result) ? DeviceState::Removed : DeviceState::Failed;
    return false;
  }

  ID3D12CommandList* lists[] = {binding.commandList.get()};
  binding.queue->ExecuteCommandLists(1, lists);
  return true;
}

} // namespace Neuron
