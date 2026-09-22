#pragma once

#include "GraphicsDevice.h"

#include <array>
#include <cstdint>
#include <memory>

struct IUnknown;

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason GraphicsDevice's binding is: `<d3d12.h>`
/// in this library's master include would reach the package's precompiled header and both desktop
/// suites below it.
struct SceneTargetBinding;

/// ADR-016: THE WORLD'S RESOLUTION IS A SCALE OF THE PANEL, not a constant, and its default is 1:1
/// -- 2880 x 1920 on the target device. Two settled points are expected and nothing between them:
/// 1:1, and 0.5. **Moving between them is these two numbers and nothing else**, which is the
/// property ADR-016 exists to buy and which M0.16 settles by measuring rather than by argument.
///
/// IT IS A RATIONAL RATHER THAN A FLOAT, for the reason FitTransform gives at more length: R13's
/// whole arrangement is worth nothing if a conversion error lands the world fit at 1.99 rather than
/// 2, and an exact numerator over an exact denominator keeps that decision in integers from the
/// panel's size all the way to the filter.
inline constexpr std::int32_t WORLD_SCALE_NUMERATOR = 1;
inline constexpr std::int32_t WORLD_SCALE_DENOMINATOR = 1;

/// One, which is what the MVP ships (`TechnicalDesign.md` section 6), and the first number the
/// design expects to move -- four is what it asks for, and 1440 x 960 at four samples and
/// 2880 x 1920 at one are the same 5,529,600 samples.
///
/// A FLIP-MODEL BACK BUFFER CANNOT BE MULTISAMPLED, so this count belongs to the scene target and
/// can belong nowhere else. That is most of why the target exists at all: at one sample the present
/// step is a pure copy, and what it buys is that raising this constant is a change to a resolve
/// rather than a rewrite of every pass.
inline constexpr std::uint32_t WORLD_SAMPLE_COUNT = 1;

/// A panel extent at a scale, in physical pixels, in integers.
///
/// NEVER ZERO FOR A POSITIVE PANEL, for the reason `WindowMetrics` gives about its own rounding: a
/// window dragged between monitors can report a size small enough to round to nothing for a single
/// frame, and a render target of zero width does not fail gracefully. A non-positive panel is zero,
/// because that is genuinely no window.
[[nodiscard]] constexpr std::int32_t ScaledExtentPixels(std::int32_t _panelExtentPixels, std::int32_t _numerator,
                                                        std::int32_t _denominator) noexcept
{
  if ((_panelExtentPixels <= 0) || (_numerator <= 0) || (_denominator <= 0))
  {
    return 0;
  }

  const std::int64_t scaled = (static_cast<std::int64_t>(_panelExtentPixels) * _numerator) / _denominator;
  return (scaled < 1) ? 1 : static_cast<std::int32_t>(scaled);
}

/// The world's target size on a panel of this size, at the scale that ships.
[[nodiscard]] constexpr std::int32_t WorldTargetWidthPixels(std::int32_t _panelWidthPixels) noexcept
{
  return ScaledExtentPixels(_panelWidthPixels, WORLD_SCALE_NUMERATOR, WORLD_SCALE_DENOMINATOR);
}

[[nodiscard]] constexpr std::int32_t WorldTargetHeightPixels(std::int32_t _panelHeightPixels) noexcept
{
  return ScaledExtentPixels(_panelHeightPixels, WORLD_SCALE_NUMERATOR, WORLD_SCALE_DENOMINATOR);
}

/// One strip of M0.16's calibration pattern, in scene-target pixels, half-open on the right and the
/// bottom as `D3D12_RECT` is -- so a strip one pixel thick has `right == left + 1`.
///
/// R8: a public aggregate. IT NAMES NO DIRECT3D TYPE, for the reason the binding below is declared
/// here and defined in the .cpp: `<d3d12.h>` in this library's master include reaches the package's
/// precompiled header and both desktop suites under it. The .cpp copies these into `D3D12_RECT`.
struct CalibrationStrip
{
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;

  /// Zero for the empty strip a degenerate target produces, which is how "nothing to draw" is said
  /// without a second return value.
  [[nodiscard]] constexpr std::int32_t AreaPixels() const noexcept
  {
    return ((right <= left) || (bottom <= top)) ? 0 : ((right - left) * (bottom - top));
  }

  [[nodiscard]] friend constexpr bool operator==(const CalibrationStrip&, const CalibrationStrip&) noexcept = default;
};

/// Four edges and a two-armed cross.
inline constexpr std::size_t CALIBRATION_STRIP_COUNT = 6;

/// Where M0.16's hard one-pixel edges go, at a given scene-target size.
///
/// IT IS ARITHMETIC AND IT IS THEREFORE HERE RATHER THAN INSIDE THE DEVICE CALL (R20). **The gate
/// is the claim that these are one pixel**: a strip two pixels thick still looks like a line on the
/// glass and tells a human nothing about whether the scale landed at 2 or at 1.99, so the one part
/// of the pattern that can be wrong invisibly is the part a suite can reach. What is left in the
/// .cpp is a clear per strip, which cannot be.
///
/// THE OUTERMOST ROWS AND COLUMNS, because the edge of the fitted rectangle is where an offset
/// error has the least to hide behind; the center cross catches a scale error instead, which
/// accumulates toward the middle.
///
/// A non-positive extent yields six empty strips rather than negative rectangles -- the same answer
/// `ScaledExtentPixels` gives above, for the same reason: a window dragged between monitors can
/// report a size that rounds to nothing for a single frame, and a negative `D3D12_RECT` is a
/// debug-layer error rather than a blank frame.
[[nodiscard]] constexpr std::array<CalibrationStrip, CALIBRATION_STRIP_COUNT> CalibrationStrips(std::int32_t _widthPixels,
                                                                                                std::int32_t _heightPixels) noexcept
{
  if ((_widthPixels <= 0) || (_heightPixels <= 0))
  {
    return {};
  }

  const std::int32_t width = _widthPixels;
  const std::int32_t height = _heightPixels;
  const std::int32_t centerColumn = width / 2;
  const std::int32_t centerRow = height / 2;

  return {CalibrationStrip{.left = 0, .top = 0, .right = width, .bottom = 1},
          CalibrationStrip{.left = 0, .top = height - 1, .right = width, .bottom = height},
          CalibrationStrip{.left = 0, .top = 0, .right = 1, .bottom = height},
          CalibrationStrip{.left = width - 1, .top = 0, .right = width, .bottom = height},
          CalibrationStrip{.left = 0, .top = centerRow, .right = width, .bottom = centerRow + 1},
          CalibrationStrip{.left = centerColumn, .top = 0, .right = centerColumn + 1, .bottom = height}};
}

/// The off-screen color target the world draws into, and the depth buffer that goes with it (R13).
///
/// The depth buffer is here rather than deferred because the passes that need it are already named:
/// the simulation is two-dimensional (R22) but the camera has three dimensions, and the sky draws
/// last with the depth test on so it shades no pixel the fleet already covers
/// (`TechnicalDesign.md` section 6).
///
/// COM LIFETIMES ARE RAII FROM THE FIRST LINE (R12), as in every class beside this one.
class SceneTarget
{
public:
  static constexpr std::uint32_t SAMPLE_COUNT = WORLD_SAMPLE_COUNT;

  /// R8: a public aggregate, so plain fields and brace initialization.
  struct Desc
  {
    /// THE WORLD'S SIZE, which is the panel's through `WorldTargetWidthPixels` -- not the window's
    /// in device-independent pixels, and not the interface's authored layout space.
    std::int32_t widthPixels = 0;
    std::int32_t heightPixels = 0;

    /// THE CLEAR COLOR IS FIXED AT CREATION, and that is not an API preference. A clear whose color
    /// differs from the resource's optimized clear value takes a slower path and the debug layer
    /// says so; one number used for both cannot drift apart from itself.
    float clearRed = 0.0f;
    float clearGreen = 0.0f;
    float clearBlue = 0.0f;
  };

  SceneTarget() noexcept;
  ~SceneTarget() noexcept;

  SceneTarget(const SceneTarget&) = delete;
  SceneTarget& operator=(const SceneTarget&) = delete;
  SceneTarget(SceneTarget&&) = delete;
  SceneTarget& operator=(SceneTarget&&) = delete;

  [[nodiscard]] bool Create(const GraphicsDevice& _device, const Desc& _desc) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;

  [[nodiscard]] std::int32_t WidthPixels() const noexcept;
  [[nodiscard]] std::int32_t HeightPixels() const noexcept;

  /// The HRESULT of the last call that failed, or zero.
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// The `DXGI_FORMAT` of the color target and of the depth buffer, as plain numbers so that this
  /// header pulls in no Direct3D.
  ///
  /// **A PASS THAT DRAWS INTO THIS TARGET READS THEM RATHER THAN RESTATING THEM**, which is the
  /// arrangement `SwapChain::BackBufferFormatCode` already has: a pipeline state has to agree with
  /// the target it renders into, and two places stating a format is two places to change it.
  [[nodiscard]] std::uint32_t ColorFormatCode() const noexcept;
  [[nodiscard]] std::uint32_t DepthFormatCode() const noexcept;

  /// Binds the color target and the depth buffer and clears both, on the device's open command
  /// list. THE WHOLE OF WHAT M0.15 DRAWS INTO IT -- the world passes that will fill it arrive with
  /// the renderer, and until they do this is one color whose only job is to be visibly different
  /// from the back buffer behind it, so that a present step which drew nothing is obvious.
  [[nodiscard]] bool RecordClear(const GraphicsDevice& _device) noexcept;

  /// Draws M0.16's calibration pattern into this target: single-pixel white lines on the first and
  /// last row and column, and a single-pixel cross through the center.
  ///
  /// **R13's whole arrangement is worth nothing if a conversion error lands the scale at 1.99
  /// rather than 2, or at 0.999 rather than 1**, and a one-pixel edge is the only thing that shows
  /// the difference. At 1:1 each line must reach the glass one physical pixel wide and fully white;
  /// at a 0.5 scale, two physical pixels wide, fully white, with no gray either side. **Gray is the
  /// failure** -- it means the sample landed between texels and the scale is not what it says.
  ///
  /// IT COSTS NO SHADER AND NO GEOMETRY. `ClearRenderTargetView` takes rectangles, so the pattern
  /// is four clears of a one-pixel strip; nothing here is a pipeline state that could itself be the
  /// thing under test.
  [[nodiscard]] bool RecordCalibrationPattern(const GraphicsDevice& _device) noexcept;

  /// The color texture, and the shader-visible heap holding its one shader resource view, as
  /// `IUnknown` -- so PresentStep can sample this target without either header naming a Direct3D
  /// type. Null before Create succeeds.
  [[nodiscard]] ::IUnknown* ColorResourceUnknown() const noexcept;
  [[nodiscard]] ::IUnknown* ShaderResourceHeapUnknown() const noexcept;

  /// That view's GPU-side descriptor handle, as the integer it already is.
  [[nodiscard]] std::uint64_t ShaderResourceHandle() const noexcept;

private:
  std::shared_ptr<SceneTargetBinding> m_binding;
};

} // namespace Neuron
