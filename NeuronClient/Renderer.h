#pragma once

namespace Neuron
{
// The Direct3D 12 device, its direct queue and a flip-model swap chain on one window (ADR-006). It knows no game
// concept: it clears to the color it is given and presents.
class Renderer : NonCopyable
{
public:
  // Back buffers, and so the frames the CPU may record ahead of the GPU (ADR-006).
  static constexpr UINT FRAME_COUNT = 2;

  // Creates the device on the high-performance hardware adapter, and the swap chain on the window at the given size.
  // Throws winrt::hresult_error on failure.
  Renderer(HWND _window, UINT _widthPixels, UINT _heightPixels);
  ~Renderer();

  // Blocks until the swap chain can take another frame. The caller reads input right after it, so that what a frame
  // shows is as fresh as the frame (ADR-006).
  void WaitForNextFrame() noexcept;

  // Matches the back buffers to the window's client area. Nothing happens if the size has not changed, or is zero.
  void Resize(UINT _widthPixels, UINT _heightPixels);

  // Records a frame that clears the back buffer to a linear color, submits it and presents it.
  void RenderFrame(const std::array<float, 4>& _clearColor);

private:
  void CreateRenderTargets();
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView(UINT _index) const noexcept;
  void WaitForGpu();

  // Throws for a failed HRESULT. A lost device is thrown with the reason the device gives for it (ADR-006).
  void CheckDeviceResult(HRESULT _result) const;

  winrt::com_ptr<IDXGIFactory6> m_factory;
  winrt::com_ptr<ID3D12Device> m_device;
  winrt::com_ptr<ID3D12CommandQueue> m_queue;
  winrt::com_ptr<IDXGISwapChain4> m_swapChain;
  winrt::com_ptr<ID3D12DescriptorHeap> m_renderTargetHeap;
  std::array<winrt::com_ptr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
  std::array<winrt::com_ptr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
  winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList;
  winrt::com_ptr<ID3D12Fence> m_fence;
  winrt::handle m_fenceEvent;
  winrt::handle m_frameLatencyWaitable;

  // The fence value each back buffer's last frame signaled; its allocator is free again once the fence reaches it.
  std::array<UINT64, FRAME_COUNT> m_frameFenceValues{};
  UINT64 m_fenceValue = 0;
  UINT m_renderTargetDescriptorSize = 0;
  UINT m_swapChainFlags = 0;
  UINT m_widthPixels = 0;
  UINT m_heightPixels = 0;
  bool m_tearingSupported = false;
  bool m_vsync = true;
  // True once a frame has been presented and not yet waited for; the waitable object is signaled once per present.
  bool m_frameWaitPending = true;
};
} // namespace Neuron
