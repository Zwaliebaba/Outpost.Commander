#pragma once

#include "GraphicsDevice.h"

#include <cstdint>
#include <memory>

struct IUnknown;

namespace Neuron
{

/// Defined in the .cpp, for the reason GraphicsDevice's is.
struct SwapChainBinding;

/// A flip-model swap chain over a `CoreWindow`, CREATED AT THE PANEL'S PHYSICAL PIXELS.
///
/// ADR-007 is explicit about that and it is the trap this class exists to avoid: the `CoreWindow`
/// reports its size in DEVICE-INDEPENDENT pixels, the Surface Pro ships at 200%, so a swap chain
/// made from the reported size is 1440 x 960 against a 2880 x 1920 panel. Nothing fails. The game
/// is simply half resolution on the one device it is for. `WindowMetrics` is the conversion and
/// R18 requires a suite over it; this is its only caller that matters.
///
/// FLIP MODEL, which is not a preference either: `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` is the only
/// effect a `CoreWindow` swap chain accepts, and the bit-block transfer models are gone from the
/// application family entirely.
class SwapChain
{
public:
  /// Two, matching the device's frames in flight. A third buffer buys smoothness at the cost of a
  /// frame of latency, and ADR-003's tap-to-visible arithmetic has none to give.
  static constexpr std::uint32_t BUFFER_COUNT = GraphicsDevice::FRAMES_IN_FLIGHT;

  SwapChain() noexcept;
  ~SwapChain() noexcept;

  SwapChain(const SwapChain&) = delete;
  SwapChain& operator=(const SwapChain&) = delete;
  SwapChain(SwapChain&&) = delete;
  SwapChain& operator=(SwapChain&&) = delete;

  /// _coreWindow is the `CoreWindow` as an `IUnknown`, so this header need not name a Windows
  /// Runtime type. _widthPixels and _heightPixels are PHYSICAL pixels -- see the class comment.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, ::IUnknown* _coreWindow, std::int32_t _widthPixels,
                            std::int32_t _heightPixels) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;

  [[nodiscard]] std::int32_t WidthPixels() const noexcept;
  [[nodiscard]] std::int32_t HeightPixels() const noexcept;

  /// The HRESULT of the last call that failed, or zero.
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Records a transition to render target, a clear, and a transition back to present, on the
  /// device's open command list. THE WHOLE OF WHAT M0.13 DRAWS -- the passes that replace it come
  /// with the renderer, and R13 puts them in a scene target rather than straight into this buffer.
  [[nodiscard]] bool RecordClear(const GraphicsDevice& _device, float _red, float _green, float _blue) noexcept;

  /// False on a device loss, which the caller answers by rebuilding rather than by dying.
  [[nodiscard]] bool Present() noexcept;

  [[nodiscard]] std::uint32_t CurrentBackBufferIndex() const noexcept;

private:
  std::shared_ptr<SwapChainBinding> m_binding;
};

} // namespace Neuron
