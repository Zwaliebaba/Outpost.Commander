#include "pch.h"

#include "SwapChain.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>

namespace Neuron
{

struct SwapChainBinding
{
  winrt::com_ptr<IDXGISwapChain3> swapChain;
  winrt::com_ptr<ID3D12DescriptorHeap> renderTargetHeap;
  std::array<winrt::com_ptr<ID3D12Resource>, SwapChain::BUFFER_COUNT> backBuffers;

  std::uint32_t descriptorSize = 0;
  std::int32_t widthPixels = 0;
  std::int32_t heightPixels = 0;
  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
/// Eight bits a channel, no gamma conversion on present. R13 puts every pass in a scene target and
/// the present step is a scale rather than a color conversion, so the swap chain is the plain
/// format and nothing surprises anybody at the end of the frame.
inline constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;

[[nodiscard]] bool IsDeviceLost(HRESULT _result) noexcept
{
  return (_result == DXGI_ERROR_DEVICE_REMOVED) || (_result == DXGI_ERROR_DEVICE_RESET);
}

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

SwapChain::SwapChain() noexcept
  : m_binding{std::make_shared<SwapChainBinding>()}
{
}

SwapChain::~SwapChain() noexcept
{
  Destroy();
}

bool SwapChain::Create(const GraphicsDevice& _device, ::IUnknown* _coreWindow, std::int32_t _widthPixels,
                       std::int32_t _heightPixels) noexcept
{
  SwapChainBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  if ((_coreWindow == nullptr) || (_widthPixels <= 0) || (_heightPixels <= 0) || (_device.State() != DeviceState::Ready))
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  winrt::com_ptr<ID3D12Device> device;
  winrt::com_ptr<ID3D12CommandQueue> queue;
  if (FAILED(_device.DeviceUnknown()->QueryInterface(winrt::guid_of<ID3D12Device>(), device.put_void())) ||
      FAILED(_device.CommandQueueUnknown()->QueryInterface(winrt::guid_of<ID3D12CommandQueue>(), queue.put_void())))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  winrt::com_ptr<IDXGIFactory4> factory;
  binding.lastHresult = CreateDXGIFactory2(0, winrt::guid_of<IDXGIFactory4>(), factory.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  const DXGI_SWAP_CHAIN_DESC1 description{.Width = static_cast<UINT>(_widthPixels),
                                          .Height = static_cast<UINT>(_heightPixels),
                                          .Format = BACK_BUFFER_FORMAT,
                                          .Stereo = FALSE,
                                          .SampleDesc = {.Count = 1, .Quality = 0},
                                          .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                                          .BufferCount = BUFFER_COUNT,
                                          .Scaling = DXGI_SCALING_STRETCH,
                                          .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                                          .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
                                          .Flags = 0};

  winrt::com_ptr<IDXGISwapChain1> created;
  binding.lastHresult = factory->CreateSwapChainForCoreWindow(queue.get(), _coreWindow, &description, nullptr, created.put());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  if (FAILED(created->QueryInterface(winrt::guid_of<IDXGISwapChain3>(), binding.swapChain.put_void())))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .NumDescriptors = BUFFER_COUNT, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE, .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&heapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.renderTargetHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  D3D12_CPU_DESCRIPTOR_HANDLE handle = binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart();
  for (std::uint32_t buffer = 0; buffer < BUFFER_COUNT; ++buffer)
  {
    binding.lastHresult = binding.swapChain->GetBuffer(buffer, winrt::guid_of<ID3D12Resource>(), binding.backBuffers[buffer].put_void());
    if (FAILED(binding.lastHresult))
    {
      return false;
    }
    device->CreateRenderTargetView(binding.backBuffers[buffer].get(), nullptr, handle);
    handle.ptr += binding.descriptorSize;
  }

  binding.widthPixels = _widthPixels;
  binding.heightPixels = _heightPixels;
  binding.ready = true;
  return true;
}

void SwapChain::Destroy() noexcept
{
  SwapChainBinding& binding = *m_binding;
  for (winrt::com_ptr<ID3D12Resource>& buffer : binding.backBuffers)
  {
    buffer = nullptr;
  }
  binding.renderTargetHeap = nullptr;
  binding.swapChain = nullptr;
  binding.ready = false;
  binding.widthPixels = 0;
  binding.heightPixels = 0;
}

bool SwapChain::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t SwapChain::WidthPixels() const noexcept
{
  return m_binding->widthPixels;
}

std::int32_t SwapChain::HeightPixels() const noexcept
{
  return m_binding->heightPixels;
}

std::int32_t SwapChain::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

std::uint32_t SwapChain::CurrentBackBufferIndex() const noexcept
{
  return m_binding->ready ? m_binding->swapChain->GetCurrentBackBufferIndex() : 0;
}

std::uint32_t SwapChain::BackBufferFormatCode() const noexcept
{
  return static_cast<std::uint32_t>(BACK_BUFFER_FORMAT);
}

bool SwapChain::RecordBindAndClear(const GraphicsDevice& _device, float _red, float _green, float _blue) noexcept
{
  SwapChainBinding& binding = *m_binding;
  if (!binding.ready)
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const std::uint32_t index = binding.swapChain->GetCurrentBackBufferIndex();

  const D3D12_RESOURCE_BARRIER barrier{.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                                       .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                                       .Transition = {.pResource = binding.backBuffers[index].get(),
                                                      .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                                      .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                                                      .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET}};
  commandList->ResourceBarrier(1, &barrier);

  D3D12_CPU_DESCRIPTOR_HANDLE handle = binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart();
  handle.ptr += static_cast<SIZE_T>(index) * binding.descriptorSize;

  const std::array<float, 4> color{_red, _green, _blue, 1.0f};
  commandList->OMSetRenderTargets(1, &handle, FALSE, nullptr);
  commandList->ClearRenderTargetView(handle, color.data(), 0, nullptr);
  return true;
}

bool SwapChain::RecordReadyToPresent(const GraphicsDevice& _device) noexcept
{
  SwapChainBinding& binding = *m_binding;
  if (!binding.ready)
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const std::uint32_t index = binding.swapChain->GetCurrentBackBufferIndex();

  const D3D12_RESOURCE_BARRIER barrier{.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                                       .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                                       .Transition = {.pResource = binding.backBuffers[index].get(),
                                                      .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                                      .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                      .StateAfter = D3D12_RESOURCE_STATE_PRESENT}};
  commandList->ResourceBarrier(1, &barrier);
  return true;
}

bool SwapChain::Present() noexcept
{
  SwapChainBinding& binding = *m_binding;
  if (!binding.ready)
  {
    return false;
  }

  // One vertical blank, no tearing. The frame-time measurement M0.16 owes is taken with this in
  // place, because it is what ships.
  const HRESULT result = binding.swapChain->Present(1, 0);
  if (FAILED(result))
  {
    binding.lastHresult = result;
    if (IsDeviceLost(result))
    {
      // A PATH, NOT A CRASH (R12, M0.13). Everything here is released so the caller's rebuild
      // starts from nothing rather than from half a swap chain.
      Destroy();
    }
    return false;
  }
  return true;
}

} // namespace Neuron
