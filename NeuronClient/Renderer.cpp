#include "pch.h"

// pix3.h disables C4548 and C4555 for the rest of the translation unit when markers are compiled out. The push and pop
// keep that inside the header (ADR-005).
#pragma warning(push)
#include <pix3.h>
#pragma warning(pop)

#include "Renderer.h"

#include <algorithm>
#include <cstring>

// dxgi.lib and dxguid.lib are named by DirectXHelper.h.
#pragma comment(lib, "d3d12.lib")

namespace
{
constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
// The scene target is cleared to black; a frame's clear color other than this still works, at some cost on some GPUs.
constexpr std::array<float, 4> SCENE_CLEAR{0.0f, 0.0f, 0.0f, 1.0f};
// The depth buffer is cleared to the far plane; nearer is smaller (ADR-011).
constexpr float DEPTH_CLEAR = 1.0f;
constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

// A frame never waits longer than this for the swap chain, so a lost device cannot hang the loop.
constexpr DWORD FRAME_WAIT_TIMEOUT_MILLISECONDS = 1000;
// A frame's two timestamps: when its command list starts and when it ends.
constexpr UINT TIMESTAMPS_PER_FRAME = 2;
constexpr UINT64 NANOSECONDS_PER_SECOND = 1'000'000'000;

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
    .NumDescriptors = 1,
    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    .NodeMask = 0,
  };
  winrt::check_hresult(m_device->CreateDescriptorHeap(&heapDescription, IID_GRAPHICS_PPV_ARGS(m_renderTargetHeap)));

  const D3D12_DESCRIPTOR_HEAP_DESC depthHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    .NumDescriptors = 1,
    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    .NodeMask = 0,
  };
  winrt::check_hresult(m_device->CreateDescriptorHeap(&depthHeapDescription, IID_GRAPHICS_PPV_ARGS(m_depthStencilHeap)));

  for (auto& allocator : m_commandAllocators)
    winrt::check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(allocator)));
  winrt::check_hresult(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].get(), nullptr,
                                                   IID_GRAPHICS_PPV_ARGS(m_commandList)));
  winrt::check_hresult(m_commandList->Close());

  winrt::check_hresult(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(m_fence)));
  m_fenceEvent.attach(winrt::check_pointer(CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS)));

  // Q4 measures a frame's GPU work, not the present interval (ADR-006).
  const D3D12_QUERY_HEAP_DESC timestampHeapDescription{
    .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
    .Count = TIMESTAMPS_PER_FRAME * FRAME_COUNT,
    .NodeMask = 0,
  };
  winrt::check_hresult(m_device->CreateQueryHeap(&timestampHeapDescription, IID_GRAPHICS_PPV_ARGS(m_timestampHeap)));
  const CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
  const CD3DX12_RESOURCE_DESC readbackDescription = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * TIMESTAMPS_PER_FRAME * FRAME_COUNT);
  winrt::check_hresult(m_device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDescription,
                                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                         IID_GRAPHICS_PPV_ARGS(m_timestampReadback)));
  winrt::check_hresult(m_queue->GetTimestampFrequency(&m_timestampFrequency));

  CreateRenderTargets();
  CreateDepthBuffer();
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
  m_sceneTarget = nullptr;
  m_resolvedScene = nullptr;
  CheckDeviceResult(m_swapChain->ResizeBuffers(FRAME_COUNT, _widthPixels, _heightPixels, BACK_BUFFER_FORMAT, m_swapChainFlags));
  m_widthPixels = _widthPixels;
  m_heightPixels = _heightPixels;
  CreateRenderTargets();
  CreateDepthBuffer();
}

ID3D12GraphicsCommandList* Neuron::Renderer::BeginFrame(const std::array<float, 4>& _clearColor)
{
  PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame");
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // This back buffer's allocator was last used FRAME_COUNT frames ago; the GPU must be done with it before it is reset.
  if (m_fence->GetCompletedValue() < m_frameFenceValues[m_frameIndex])
  {
    winrt::check_hresult(m_fence->SetEventOnCompletion(m_frameFenceValues[m_frameIndex], m_fenceEvent.get()));
    WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, FALSE);
  }
  // That frame's timestamps are resolved now too.
  if (m_timestampsPending[m_frameIndex])
  {
    const UINT first = m_frameIndex * TIMESTAMPS_PER_FRAME;
    const D3D12_RANGE readRange{.Begin = sizeof(UINT64) * first, .End = sizeof(UINT64) * (first + TIMESTAMPS_PER_FRAME)};
    void* mapped = nullptr;
    winrt::check_hresult(m_timestampReadback->Map(0, &readRange, &mapped));
    std::array<UINT64, TIMESTAMPS_PER_FRAME> timestamps{};
    std::memcpy(timestamps.data(), static_cast<const std::byte*>(mapped) + readRange.Begin, sizeof(timestamps));
    const D3D12_RANGE nothingWritten{.Begin = 0, .End = 0};
    m_timestampReadback->Unmap(0, &nothingWritten);
    m_timestampsPending[m_frameIndex] = false;
    if (timestamps[1] >= timestamps[0] && m_timestampFrequency > 0)
    {
      // In two parts, so that the product cannot overflow whatever the frequency.
      const UINT64 ticks = timestamps[1] - timestamps[0];
      const UINT64 nanoseconds = ((ticks / m_timestampFrequency) * NANOSECONDS_PER_SECOND) +
                                 ((ticks % m_timestampFrequency) * NANOSECONDS_PER_SECOND / m_timestampFrequency);
      m_gpuFrameTimes.emplace_back(static_cast<std::int64_t>(nanoseconds));
    }
  }

  ID3D12CommandAllocator* allocator = m_commandAllocators[m_frameIndex].get();
  winrt::check_hresult(allocator->Reset());
  winrt::check_hresult(m_commandList->Reset(allocator, nullptr));
  m_commandList->EndQuery(m_timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, m_frameIndex * TIMESTAMPS_PER_FRAME);
  PIXBeginEvent(m_commandList.get(), PIX_COLOR_DEFAULT, L"Scene");

  // The scene target is left a render target by the frame before, and the back buffer is not touched until EndFrame.
  const D3D12_CPU_DESCRIPTOR_HANDLE renderTarget = SceneTargetView();
  const D3D12_CPU_DESCRIPTOR_HANDLE depthStencil = DepthStencilView();
  m_commandList->ClearRenderTargetView(renderTarget, _clearColor.data(), 0, nullptr);
  m_commandList->ClearDepthStencilView(depthStencil, D3D12_CLEAR_FLAG_DEPTH, DEPTH_CLEAR, 0, 0, nullptr);
  m_commandList->OMSetRenderTargets(1, &renderTarget, FALSE, &depthStencil);

  const D3D12_VIEWPORT viewport{
    .TopLeftX = 0.0f,
    .TopLeftY = 0.0f,
    .Width = static_cast<float>(m_widthPixels),
    .Height = static_cast<float>(m_heightPixels),
    .MinDepth = D3D12_MIN_DEPTH,
    .MaxDepth = D3D12_MAX_DEPTH,
  };
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(m_widthPixels), .bottom = static_cast<LONG>(m_heightPixels)};
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissor);
  return m_commandList.get();
}

void Neuron::Renderer::EndFrame()
{
  // The samples are averaged in the sRGB format, so in linear color, into the resolved scene. A flip-model back buffer
  // cannot be sRGB, and a resolve cannot change the format, so the resolved scene is copied into it, which a copy within
  // one format group may do (ADR-040).
  ID3D12Resource* backBuffer = m_backBuffers[m_frameIndex].get();
  const std::array toResolve{
    CD3DX12_RESOURCE_BARRIER::Transition(m_sceneTarget.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
    CD3DX12_RESOURCE_BARRIER::Transition(m_resolvedScene.get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST),
  };
  m_commandList->ResourceBarrier(static_cast<UINT>(toResolve.size()), toResolve.data());
  m_commandList->ResolveSubresource(m_resolvedScene.get(), 0, m_sceneTarget.get(), 0, RENDER_TARGET_FORMAT);

  const std::array toCopy{
    CD3DX12_RESOURCE_BARRIER::Transition(m_resolvedScene.get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE),
    CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST),
    CD3DX12_RESOURCE_BARRIER::Transition(m_sceneTarget.get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
  };
  m_commandList->ResourceBarrier(static_cast<UINT>(toCopy.size()), toCopy.data());
  m_commandList->CopyResource(backBuffer, m_resolvedScene.get());

  const auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
  m_commandList->ResourceBarrier(1, &toPresent);

  PIXEndEvent(m_commandList.get());
  const UINT first = m_frameIndex * TIMESTAMPS_PER_FRAME;
  m_commandList->EndQuery(m_timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
  m_commandList->ResolveQueryData(m_timestampHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, first, TIMESTAMPS_PER_FRAME, m_timestampReadback.get(),
                                  sizeof(UINT64) * first);
  m_timestampsPending[m_frameIndex] = true;
  winrt::check_hresult(m_commandList->Close());

  ID3D12CommandList* commandList = m_commandList.get();
  PIXBeginEvent(m_queue.get(), PIX_COLOR_DEFAULT, L"Submit");
  m_queue->ExecuteCommandLists(1, &commandList);
  PIXEndEvent(m_queue.get());

  const UINT syncInterval = m_vsync ? 1 : 0;
  const UINT presentFlags = (!m_vsync && m_tearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
  CheckDeviceResult(m_swapChain->Present(syncInterval, presentFlags));
  m_frameWaitPending = true;

  m_frameFenceValues[m_frameIndex] = ++m_fenceValue;
  CheckDeviceResult(m_queue->Signal(m_fence.get(), m_frameFenceValues[m_frameIndex]));
  PIXEndEvent();
}

std::vector<std::chrono::nanoseconds> Neuron::Renderer::TakeGpuFrameTimes()
{
  std::vector<std::chrono::nanoseconds> times;
  times.swap(m_gpuFrameTimes);
  return times;
}

winrt::com_ptr<ID3D12Resource> Neuron::Renderer::CreateStaticBuffer(std::span<const std::byte> _bytes)
{
  const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Buffer(_bytes.size());

  // A buffer is created in the common state and promoted to whatever its first use needs, the copy here and reading
  // vertices or indices after it, so no barrier is recorded.
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  winrt::com_ptr<ID3D12Resource> buffer;
  winrt::check_hresult(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COMMON,
                                                         nullptr, IID_GRAPHICS_PPV_ARGS(buffer)));

  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  winrt::com_ptr<ID3D12Resource> upload;
  winrt::check_hresult(m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr, IID_GRAPHICS_PPV_ARGS(upload)));
  void* mapped = nullptr;
  const D3D12_RANGE nothingRead{.Begin = 0, .End = 0};
  winrt::check_hresult(upload->Map(0, &nothingRead, &mapped));
  std::memcpy(mapped, _bytes.data(), _bytes.size());
  upload->Unmap(0, nullptr);

  winrt::com_ptr<ID3D12CommandAllocator> allocator;
  winrt::check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(allocator)));
  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::check_hresult(
    m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.get(), nullptr, IID_GRAPHICS_PPV_ARGS(commandList)));
  commandList->CopyBufferRegion(buffer.get(), 0, upload.get(), 0, _bytes.size());
  winrt::check_hresult(commandList->Close());
  ID3D12CommandList* lists[] = {commandList.get()};
  m_queue->ExecuteCommandLists(1, lists);

  // The upload buffer and the allocator must outlive the copy.
  WaitForGpu();
  return buffer;
}

winrt::com_ptr<ID3D12Resource> Neuron::Renderer::CreateStaticTexture(UINT _width, UINT _height, DXGI_FORMAT _format,
                                                                     std::span<const std::byte> _texels)
{
  const std::array<std::span<const std::byte>, 1> levels{_texels};
  return CreateStaticTexture(_width, _height, _format, levels);
}

winrt::com_ptr<ID3D12Resource> Neuron::Renderer::CreateStaticTexture(UINT _width, UINT _height, DXGI_FORMAT _format,
                                                                     std::span<const std::span<const std::byte>> _levels)
{
  const auto levelCount = static_cast<UINT16>(_levels.size());
  const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Tex2D(_format, _width, _height, 1, levelCount);
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  winrt::com_ptr<ID3D12Resource> texture;
  winrt::check_hresult(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST,
                                                         nullptr, IID_GRAPHICS_PPV_ARGS(texture)));

  const UINT64 uploadBytes = GetRequiredIntermediateSize(texture.get(), 0, levelCount);
  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC uploadDescription = CD3DX12_RESOURCE_DESC::Buffer(uploadBytes);
  winrt::com_ptr<ID3D12Resource> upload;
  winrt::check_hresult(m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
                                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_GRAPHICS_PPV_ARGS(upload)));

  winrt::com_ptr<ID3D12CommandAllocator> allocator;
  winrt::check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(allocator)));
  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::check_hresult(
    m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.get(), nullptr, IID_GRAPHICS_PPV_ARGS(commandList)));
  // d3dx12 lays the rows out at the pitch the copy needs.
  std::vector<D3D12_SUBRESOURCE_DATA> sources;
  UINT levelHeight = _height;
  for (const std::span<const std::byte> level : _levels)
  {
    const LONG_PTR rowBytes = static_cast<LONG_PTR>(level.size() / levelHeight);
    sources.push_back({.pData = level.data(), .RowPitch = rowBytes, .SlicePitch = static_cast<LONG_PTR>(level.size())});
    levelHeight = std::max(1u, levelHeight / 2);
  }
  if (UpdateSubresources(commandList.get(), texture.get(), upload.get(), 0, 0, levelCount, sources.data()) == 0)
    throw winrt::hresult_error(E_FAIL, L"A texture could not be uploaded.");
  const CD3DX12_RESOURCE_BARRIER toShader =
    CD3DX12_RESOURCE_BARRIER::Transition(texture.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &toShader);
  winrt::check_hresult(commandList->Close());
  ID3D12CommandList* lists[] = {commandList.get()};
  m_queue->ExecuteCommandLists(1, lists);

  // The upload buffer and the allocator must outlive the copy.
  WaitForGpu();
  return texture;
}

void Neuron::Renderer::CreateRenderTargets()
{
  // The back buffers are only copied into, so they need no view.
  for (UINT index = 0; index < FRAME_COUNT; ++index)
    winrt::check_hresult(m_swapChain->GetBuffer(index, IID_GRAPHICS_PPV_ARGS(m_backBuffers[index])));

  // Resize has already drained the GPU and released the old targets.
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  const CD3DX12_RESOURCE_DESC sceneDescription = CD3DX12_RESOURCE_DESC::Tex2D(RENDER_TARGET_FORMAT, m_widthPixels, m_heightPixels, 1, 1,
                                                                              SAMPLE_COUNT, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
  const CD3DX12_CLEAR_VALUE clearValue(RENDER_TARGET_FORMAT, SCENE_CLEAR.data());
  winrt::check_hresult(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &sceneDescription,
                                                         D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
                                                         IID_GRAPHICS_PPV_ARGS(m_sceneTarget)));

  D3D12_RENDER_TARGET_VIEW_DESC viewDescription{};
  viewDescription.Format = RENDER_TARGET_FORMAT;
  viewDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
  m_device->CreateRenderTargetView(m_sceneTarget.get(), &viewDescription, SceneTargetView());

  const CD3DX12_RESOURCE_DESC resolvedDescription = CD3DX12_RESOURCE_DESC::Tex2D(RENDER_TARGET_FORMAT, m_widthPixels, m_heightPixels, 1, 1);
  winrt::check_hresult(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &resolvedDescription,
                                                         D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                                         IID_GRAPHICS_PPV_ARGS(m_resolvedScene)));
}

void Neuron::Renderer::CreateDepthBuffer()
{
  // The old buffer is released first; Resize has already drained the GPU.
  m_depthBuffer = nullptr;
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Tex2D(DEPTH_FORMAT, m_widthPixels, m_heightPixels, 1, 1, SAMPLE_COUNT, 0,
                                                                         D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  const CD3DX12_CLEAR_VALUE clearValue(DEPTH_FORMAT, DEPTH_CLEAR, 0);
  winrt::check_hresult(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                         &clearValue, IID_GRAPHICS_PPV_ARGS(m_depthBuffer)));

  D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription{};
  viewDescription.Format = DEPTH_FORMAT;
  viewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
  m_device->CreateDepthStencilView(m_depthBuffer.get(), &viewDescription, DepthStencilView());
}

D3D12_CPU_DESCRIPTOR_HANDLE Neuron::Renderer::SceneTargetView() const noexcept
{
  return m_renderTargetHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE Neuron::Renderer::DepthStencilView() const noexcept
{
  return m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart();
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