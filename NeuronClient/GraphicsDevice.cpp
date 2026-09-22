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

  binding.fenceValues[binding.frameIndex] = signaled + 1;
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
