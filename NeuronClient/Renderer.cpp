#include "pch.h"

// pix3.h disables C4548 and C4555 for the rest of the translation unit when markers are compiled out. The push and pop
// keep that inside the header (ADR-005).
#pragma warning(push)
#include <pix3.h>
#pragma warning(pop)

#include "Renderer.h"

// dxgi.lib and dxguid.lib are named by DirectXHelper.h.
#pragma comment(lib, "d3d12.lib")

namespace
{
constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT RENDER_TARGET_VIEW_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

// A frame never waits longer than this for the swap chain, so a lost device cannot hang the loop.
constexpr DWORD FRAME_WAIT_TIMEOUT_MILLISECONDS = 1000;

#if defined(_DEBUG)
// The debug layer and DRED both need the Graphics Tools optional feature. Without it the game runs undiagnosed rather
// than not at all (ADR-006).
void EnableDebugLayers() noexcept
{
  winrt::com_ptr<ID3D12Debug> debug;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_GRAPHICS_PPV_ARGS(debug))))
    debug->EnableDebugLayer();

  winrt::com_ptr<ID3D12DeviceRemovedExtendedDataSettings> removedDeviceData;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_GRAPHICS_PPV_ARGS(removedDeviceData))))
  {
    removedDeviceData->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    removedDeviceData->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
  }
}
#endif

winrt::com_ptr<IDXGIFactory6> CreateFactory()
{
  winrt::com_ptr<IDXGIFactory6> factory;
#if defined(_DEBUG)
  if (SUCCEEDED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_GRAPHICS_PPV_ARGS(factory))))
    return factory;
#endif
  winrt::check_hresult(CreateDXGIFactory2(0, IID_GRAPHICS_PPV_ARGS(factory)));
  return factory;
}

// The first high-performance hardware adapter that runs Direct3D 12 at the minimum feature level (ADR-006).
winrt::com_ptr<ID3D12Device> CreateDevice(IDXGIFactory6* _factory)
{
  for (UINT index = 0;; ++index)
  {
    winrt::com_ptr<IDXGIAdapter1> adapter;
    const HRESULT result =
      _factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_GRAPHICS_PPV_ARGS(adapter));
    if (result == DXGI_ERROR_NOT_FOUND)
      break;
    winrt::check_hresult(result);

    DXGI_ADAPTER_DESC1 description{};
    winrt::check_hresult(adapter->GetDesc1(&description));
    if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
      continue;

    winrt::com_ptr<ID3D12Device> device;
    if (SUCCEEDED(D3D12CreateDevice(adapter.get(), MINIMUM_FEATURE_LEVEL, IID_GRAPHICS_PPV_ARGS(device))))
      return device;
  }
  throw winrt::hresult_error(DXGI_ERROR_UNSUPPORTED, L"No graphics adapter in this PC supports Direct3D 12.");
}
} // namespace

Neuron::Renderer::Renderer(HWND _window, UINT _widthPixels, UINT _heightPixels)
  : m_widthPixels(_widthPixels),
    m_heightPixels(_heightPixels)
{
#if defined(_DEBUG)
  EnableDebugLayers();
#endif
  m_factory = CreateFactory();
  m_device = CreateDevice(m_factory.get());

  const D3D12_COMMAND_QUEUE_DESC queueDescription{
    .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
    .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
    .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    .NodeMask = 0,
  };
  winrt::check_hresult(m_device->CreateCommandQueue(&queueDescription, IID_GRAPHICS_PPV_ARGS(m_queue)));

  // Tearing can only be allowed when the swap chain is created, so it is asked for whenever the system supports it and
  // used only when vsync is off (ADR-006).
  BOOL allowTearing = FALSE;
  m_tearingSupported = SUCCEEDED(m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))) &&
                       allowTearing != FALSE;
  m_swapChainFlags = static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
  if (m_tearingSupported)
    m_swapChainFlags |= static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

  const DXGI_SWAP_CHAIN_DESC1 swapChainDescription{
    .Width = m_widthPixels,
    .Height = m_heightPixels,
    .Format = BACK_BUFFER_FORMAT,
    .Stereo = FALSE,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = FRAME_COUNT,
    .Scaling = DXGI_SCALING_STRETCH,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    .Flags = m_swapChainFlags,
  };
  winrt::com_ptr<IDXGISwapChain1> swapChain;
  winrt::check_hresult(m_factory->CreateSwapChainForHwnd(m_queue.get(), _window, &swapChainDescription, nullptr, nullptr, swapChain.put()));
  m_swapChain = swapChain.as<IDXGISwapChain4>();

  // The game is always borderless full screen; DXGI's own Alt+Enter would switch to exclusive full screen (ADR-006).
  winrt::check_hresult(m_factory->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER));
  winrt::check_hresult(m_swapChain->SetMaximumFrameLatency(1));
  m_frameLatencyWaitable.attach(winrt::check_pointer(m_swapChain->GetFrameLatencyWaitableObject()));

  const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    .NumDescriptors = FRAME_COUNT,
    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    .NodeMask = 0,
  };
  winrt::check_hresult(m_device->CreateDescriptorHeap(&heapDescription, IID_GRAPHICS_PPV_ARGS(m_renderTargetHeap)));
  m_renderTargetDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  for (auto& allocator : m_commandAllocators)
    winrt::check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(allocator)));
  winrt::check_hresult(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].get(), nullptr,
                                                   IID_GRAPHICS_PPV_ARGS(m_commandList)));
  winrt::check_hresult(m_commandList->Close());

  winrt::check_hresult(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(m_fence)));
  m_fenceEvent.attach(winrt::check_pointer(CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS)));

  CreateRenderTargets();
}

Neuron::Renderer::~Renderer()
{
  // The GPU may still be reading the back buffers and the allocators, so it is drained before they go. Nothing here
  // throws: a failed signal means the device is gone, and there is nothing left to wait for.
  const UINT64 value = ++m_fenceValue;
  if (SUCCEEDED(m_queue->Signal(m_fence.get(), value)) && SUCCEEDED(m_fence->SetEventOnCompletion(value, m_fenceEvent.get())))
    WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, FALSE);
}

void Neuron::Renderer::WaitForNextFrame() noexcept
{
  if (!m_frameWaitPending)
    return;
  WaitForSingleObjectEx(m_frameLatencyWaitable.get(), FRAME_WAIT_TIMEOUT_MILLISECONDS, TRUE);
  m_frameWaitPending = false;
}

void Neuron::Renderer::Resize(UINT _widthPixels, UINT _heightPixels)
{
  if (_widthPixels == 0 || _heightPixels == 0 || (_widthPixels == m_widthPixels && _heightPixels == m_heightPixels))
    return;

  WaitForGpu();
  for (auto& backBuffer : m_backBuffers)
    backBuffer = nullptr;
  CheckDeviceResult(m_swapChain->ResizeBuffers(FRAME_COUNT, _widthPixels, _heightPixels, BACK_BUFFER_FORMAT, m_swapChainFlags));
  m_widthPixels = _widthPixels;
  m_heightPixels = _heightPixels;
  CreateRenderTargets();
}

void Neuron::Renderer::RenderFrame(const std::array<float, 4>& _clearColor)
{
  PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame");
  const UINT frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // This back buffer's allocator was last used FRAME_COUNT frames ago; the GPU must be done with it before it is reset.
  if (m_fence->GetCompletedValue() < m_frameFenceValues[frameIndex])
  {
    winrt::check_hresult(m_fence->SetEventOnCompletion(m_frameFenceValues[frameIndex], m_fenceEvent.get()));
    WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, FALSE);
  }

  ID3D12CommandAllocator* allocator = m_commandAllocators[frameIndex].get();
  winrt::check_hresult(allocator->Reset());
  winrt::check_hresult(m_commandList->Reset(allocator, nullptr));
  PIXBeginEvent(m_commandList.get(), PIX_COLOR_DEFAULT, L"Clear");

  ID3D12Resource* backBuffer = m_backBuffers[frameIndex].get();
  const auto toRenderTarget =
    CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_commandList->ResourceBarrier(1, &toRenderTarget);
  m_commandList->ClearRenderTargetView(RenderTargetView(frameIndex), _clearColor.data(), 0, nullptr);
  const auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  m_commandList->ResourceBarrier(1, &toPresent);

  PIXEndEvent(m_commandList.get());
  winrt::check_hresult(m_commandList->Close());

  ID3D12CommandList* commandList = m_commandList.get();
  PIXBeginEvent(m_queue.get(), PIX_COLOR_DEFAULT, L"Submit");
  m_queue->ExecuteCommandLists(1, &commandList);
  PIXEndEvent(m_queue.get());

  const UINT syncInterval = m_vsync ? 1 : 0;
  const UINT presentFlags = (!m_vsync && m_tearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
  CheckDeviceResult(m_swapChain->Present(syncInterval, presentFlags));
  m_frameWaitPending = true;

  m_frameFenceValues[frameIndex] = ++m_fenceValue;
  CheckDeviceResult(m_queue->Signal(m_fence.get(), m_frameFenceValues[frameIndex]));
  PIXEndEvent();
}

void Neuron::Renderer::CreateRenderTargets()
{
  D3D12_RENDER_TARGET_VIEW_DESC viewDescription{};
  viewDescription.Format = RENDER_TARGET_VIEW_FORMAT;
  viewDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  for (UINT index = 0; index < FRAME_COUNT; ++index)
  {
    winrt::check_hresult(m_swapChain->GetBuffer(index, IID_GRAPHICS_PPV_ARGS(m_backBuffers[index])));
    m_device->CreateRenderTargetView(m_backBuffers[index].get(), &viewDescription, RenderTargetView(index));
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE Neuron::Renderer::RenderTargetView(UINT _index) const noexcept
{
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_renderTargetHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(_index),
                                       m_renderTargetDescriptorSize);
}

void Neuron::Renderer::WaitForGpu()
{
  const UINT64 value = ++m_fenceValue;
  CheckDeviceResult(m_queue->Signal(m_fence.get(), value));
  winrt::check_hresult(m_fence->SetEventOnCompletion(value, m_fenceEvent.get()));
  WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, FALSE);
}

void Neuron::Renderer::CheckDeviceResult(HRESULT _result) const
{
  if (_result == DXGI_ERROR_DEVICE_REMOVED || _result == DXGI_ERROR_DEVICE_RESET || _result == DXGI_ERROR_DEVICE_HUNG)
  {
    const HRESULT reason = m_device->GetDeviceRemovedReason();
    throw winrt::hresult_error(FAILED(reason) ? reason : _result, L"The graphics device stopped working, so the game has to close.");
  }
  winrt::check_hresult(_result);
}
