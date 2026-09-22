#pragma once

#include "FitTransform.h"
#include "GraphicsDevice.h"
#include "SwapChain.h"

#include <cstdint>
#include <memory>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason GraphicsDevice's binding is.
struct InterfacePassBinding;

/// A rectangle in the interface's AUTHORED coordinates -- 1440 x 960, unconditional, the space
/// every number in `Interface.md` is stated in. Half-open on the right and the bottom, as
/// `SceneTarget`'s calibration strip is and as `D3D12_RECT` is, so a 48-pixel touch target spans
/// `right == left + 48`.
///
/// R8: a public aggregate. IT IS A DIFFERENT TYPE FROM THE ONE BELOW ON PURPOSE. ADR-011's stated
/// cost is that "two coordinate spaces exist where there was one, and a reader has to know which
/// one they are in"; two types is the cheapest thing that answers that, because the compiler then
/// knows too.
struct AuthoredRect
{
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;

  [[nodiscard]] constexpr std::int32_t WidthPixels() const noexcept
  {
    return (right > left) ? (right - left) : 0;
  }

  [[nodiscard]] constexpr std::int32_t HeightPixels() const noexcept
  {
    return (bottom > top) ? (bottom - top) : 0;
  }

  [[nodiscard]] friend constexpr bool operator==(const AuthoredRect&, const AuthoredRect&) noexcept = default;
};

/// The same rectangle after the interface fit: BACK BUFFER pixels, physical, half-open.
struct PhysicalRect
{
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;

  [[nodiscard]] constexpr std::int32_t WidthPixels() const noexcept
  {
    return (right > left) ? (right - left) : 0;
  }

  [[nodiscard]] constexpr std::int32_t HeightPixels() const noexcept
  {
    return (bottom > top) ? (bottom - top) : 0;
  }

  [[nodiscard]] friend constexpr bool operator==(const PhysicalRect&, const PhysicalRect&) noexcept = default;
};

/// The same rectangle again, in normalized device coordinates, which is the only form the vertex
/// stage understands. `top` is the LARGER value: clip space has y increasing upwards and a back
/// buffer has it increasing downwards, and this is where that flip happens.
struct ClipRect
{
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  [[nodiscard]] friend constexpr bool operator==(const ClipRect&, const ClipRect&) noexcept = default;
};

/// Authored layout space to back-buffer pixels, through the INTERFACE fit (ADR-016) and never the
/// world's. This is the whole of ADR-011 as arithmetic.
///
/// EVERY EDGE IS MAPPED, rather than an origin plus a size being scaled. Mapping the origin and
/// then the extent rounds twice and leaves a seam between rectangles that were authored flush;
/// mapping both edges makes an authored 1440 land exactly on the fitted rectangle's right edge,
/// whatever the window, which is the property the test over this asserts at several sizes.
///
/// IT DIVIDES BY THE AUTHORED EXTENT RATHER THAN MULTIPLYING BY `FitTransform::scale`, for the
/// reason `FitTransform` itself gives about 1.99 and 0.999: the fitted width is an integer that
/// M0.12 already proved exact, and an integer ratio against it cannot drift where a float can.
[[nodiscard]] PhysicalRect MapAuthoredRect(const FitTransform& _interfaceFit, const AuthoredRect& _rect) noexcept;

/// Back-buffer pixels to clip space. The back buffer's size is the ONLY thing this takes from the
/// window, and it takes it as a number rather than asking -- R13's "exactly one place asks the
/// window how big it is" is `WindowMetrics`, three steps up the call chain.
///
/// A degenerate back buffer gives an empty rectangle rather than a division by zero, which is the
/// same answer `FitTransform` gives to the same question.
[[nodiscard]] ClipRect ToClipRect(const PhysicalRect& _rect, std::int32_t _bufferWidthPixels, std::int32_t _bufferHeightPixels) noexcept;

/// One filled rectangle, authored. R8: a public aggregate.
struct InterfaceQuad
{
  AuthoredRect rect{};
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  float alpha = 1.0f;
};

/// ADR-011's second pass: after the present blit, before `Present`, STRAIGHT INTO THE BACK BUFFER
/// at physical resolution, laid out in authored coordinates carried through the interface fit.
///
/// IT IS THE ONE DEPARTURE FROM R13's LETTER AND NOT A VIOLATION OF IT. R13 says every pass draws
/// into the scene target; this one does not, so that a glyph is rasterized at the size it is drawn
/// at rather than resampled by the world's scale. R13's intent -- no layout number conditional, no
/// pass branching on the window size -- holds, and `AGENTS.md` R13 records the departure itself.
///
/// NOTHING HERE BRANCHES ON THE WINDOW SIZE, which is the thing this step exists to keep true. The
/// window reaches this class as two numbers inside a `FitTransform` and a back-buffer extent, and
/// every position is the same authored constant at every size.
///
/// AT M0 IT DRAWS ONE RECTANGLE. The pass is what M0 proves -- the render target bind, the pipeline
/// state, and ADR-011's ordering constraint -- and there is nothing to put in it: the glyph atlas
/// is M1.12 and the panels are M1.14. `Record` therefore binds its state per rectangle, which is
/// the right shape for one and the wrong shape for thirty; splitting it into a bind and a draw
/// belongs to the step that first draws thirty.
class InterfacePass
{
public:
  /// Two four-float root constants: the clip rectangle for the vertex stage and the color for the
  /// pixel stage. NO VERTEX BUFFER, NO DESCRIPTOR HEAP AND NO CONSTANT BUFFER -- the quad is
  /// generated from `SV_VertexID` and eight root constants, so a rectangle costs one draw call and
  /// no allocation anywhere.
  static constexpr std::uint32_t CLIP_RECT_CONSTANT_COUNT = 4;
  static constexpr std::uint32_t COLOR_CONSTANT_COUNT = 4;

  InterfacePass() noexcept;
  ~InterfacePass() noexcept;

  InterfacePass(const InterfacePass&) = delete;
  InterfacePass& operator=(const InterfacePass&) = delete;
  InterfacePass(InterfacePass&&) = delete;
  InterfacePass& operator=(InterfacePass&&) = delete;

  /// The root signature and the pipeline state, built from ADR-012's checked-in DXIL. The back
  /// buffer's format is read off the swap chain rather than restated, so one place decides it.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SwapChain& _swapChain) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;

  /// The HRESULT of the last call that failed, or zero.
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Records one rectangle into WHATEVER RENDER TARGET IS ALREADY BOUND, which is the same
  /// arrangement `PresentStep::Record` has and for the same reason: the swap chain binds and clears
  /// the back buffer, the present step blits into it, this draws over that, and the swap chain
  /// closes it. ADR-011's ordering constraint is that sequence, and it lives in the caller.
  ///
  /// _interfaceFit is THE INTERFACE FIT (ADR-016) -- `ComputeInterfaceFit`, never `ComputeFit` over
  /// the scene target. Passing the world's here is the defect ADR-016 exists to prevent: at the 1:1
  /// world default it is identity, and the whole interface lands at half size in one corner.
  ///
  /// IT RESETS THE VIEWPORT AND THE SCISSOR to the whole back buffer, because the present step left
  /// them on the letterboxed world rectangle and an interface clipped to that would lose its own
  /// letterbox bars -- which is where an alert indicator at the screen edge lives (ADR-020).
  [[nodiscard]] bool Record(const GraphicsDevice& _device, const SwapChain& _swapChain, const FitTransform& _interfaceFit,
                            const InterfaceQuad& _quad) noexcept;

private:
  std::shared_ptr<InterfacePassBinding> m_binding;
};

} // namespace Neuron
