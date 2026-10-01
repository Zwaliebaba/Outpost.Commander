#pragma once

namespace Neuron
{
// The Direct3D 12 device, its direct queue and a flip-model swap chain on one window, with a depth buffer the size of the
// back buffer (ADR-006, ADR-011). It uses d3dx12's helpers for barriers and descriptors (ADR-007). It knows no game
// concept: a frame is cleared, whoever holds the command list draws into it, and it is presented.
class Renderer : NonCopyable
{
public:
  // Back buffers, and so the frames the CPU may record ahead of the GPU (ADR-006).
  static constexpr UINT FRAME_COUNT = 2;
  static constexpr DXGI_FORMAT RENDER_TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  static constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;

  // Creates the device on the high-performance hardware adapter, and the swap chain on the window at the given size.
  // Throws winrt::hresult_error on failure.
  Renderer(HWND _window, UINT _widthPixels, UINT _heightPixels);
  ~Renderer();

  // Blocks until the swap chain can take another frame. The caller reads input right after it, so that what a frame
  // shows is as fresh as the frame (ADR-006).
  void WaitForNextFrame() noexcept;

  // Matches the back buffers and the depth buffer to the window's client area. Nothing happens if the size has not
  // changed, or is zero.
  void Resize(UINT _widthPixels, UINT _heightPixels);

  // Starts a frame: clears the back buffer to a linear color and the depth buffer to the far plane, and binds both with
  // a viewport over the whole back buffer. The command list it returns is open until EndFrame.
  [[nodiscard]] ID3D12GraphicsCommandList* BeginFrame(const std::array<float, 4>& _clearColor);

  // Submits the frame BeginFrame started and presents it.
  void EndFrame();

  // A buffer in video memory holding _bytes, for vertices or indices that never change. It is uploaded before this
  // returns, so it is only for loading, not for use while frames are being recorded.
  [[nodiscard]] winrt::com_ptr<ID3D12Resource> CreateStaticBuffer(std::span<const std::byte> _bytes);

  // A 2D texture in video memory holding _texels, one mip level, rows of _width texels of _format packed with no gap,
  // ready to be read by pixel shaders. Like CreateStaticBuffer, it waits for the upload and is only for loading; it may
  // also be called between frames, since it waits for every frame in flight too.
  [[nodiscard]] winrt::com_ptr<ID3D12Resource> CreateStaticTexture(UINT _width, UINT _height, DXGI_FORMAT _format,
                                                                   std::span<const std::byte> _texels);

  // The GPU time of every frame whose work has finished since the last call, from the first command of its command list
  // to the last, oldest first (ADR-006). A frame's time arrives FRAME_COUNT frames after it was submitted.
  [[nodiscard]] std::vector<std::chrono::nanoseconds> TakeGpuFrameTimes();

  [[nodiscard]] ID3D12Device* Device() const noexcept
  {
    return m_device.get();
  }
  // Which of the FRAME_COUNT frames is being recorded, for resources kept once per frame in flight.
  [[nodiscard]] UINT FrameIndex() const noexcept
  {
    return m_frameIndex;
  }
  [[nodiscard]] UINT WidthPixels() const noexcept
  {
    return m_widthPixels;
  }
  [[nodiscard]] UINT HeightPixels() const noexcept
  {
    return m_heightPixels;
  }

private:
  void CreateRenderTargets();
  void CreateDepthBuffer();
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView(UINT _index) const noexcept;
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const noexcept;
  void WaitForGpu();

  // Throws for a failed HRESULT. A lost device is thrown with the reason the device gives for it (ADR-006).
  void CheckDeviceResult(HRESULT _result) const;

  winrt::com_ptr<IDXGIFactory6> m_factory;
  winrt::com_ptr<ID3D12Device> m_device;
  winrt::com_ptr<ID3D12CommandQueue> m_queue;
  winrt::com_ptr<IDXGISwapChain4> m_swapChain;
  winrt::com_ptr<ID3D12DescriptorHeap> m_renderTargetHeap;
  winrt::com_ptr<ID3D12DescriptorHeap> m_depthStencilHeap;
  std::array<winrt::com_ptr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
  winrt::com_ptr<ID3D12Resource> m_depthBuffer;
  std::array<winrt::com_ptr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
  winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList;
  winrt::com_ptr<ID3D12Fence> m_fence;
  // Two timestamps per frame in flight, at the start and the end of its command list, resolved into a readback buffer.
  winrt::com_ptr<ID3D12QueryHeap> m_timestampHeap;
  winrt::com_ptr<ID3D12Resource> m_timestampReadback;
  winrt::handle m_fenceEvent;
  winrt::handle m_frameLatencyWaitable;

  // The fence value each back buffer's last frame signaled; its allocator is free again once the fence reaches it.
  std::array<UINT64, FRAME_COUNT> m_frameFenceValues{};
  UINT64 m_fenceValue = 0;
  // Ticks per second of the queue's timestamps.
  UINT64 m_timestampFrequency = 0;
  // Which back buffers' last frames have timestamps not read yet.
  std::array<bool, FRAME_COUNT> m_timestampsPending{};
  std::vector<std::chrono::nanoseconds> m_gpuFrameTimes;
  UINT m_renderTargetDescriptorSize = 0;
  UINT m_swapChainFlags = 0;
  UINT m_widthPixels = 0;
  UINT m_heightPixels = 0;
  // The back buffer of the frame being recorded, set by BeginFrame.
  UINT m_frameIndex = 0;
  bool m_tearingSupported = false;
  bool m_vsync = true;
  // True once a frame has been presented and not yet waited for; the waitable object is signaled once per present.
  bool m_frameWaitPending = true;
};
} // namespace Neuron
