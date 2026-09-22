#pragma once

#include "AtlasPacker.h"
#include "TextLayout.h"
#include "FitTransform.h"
#include "GraphicsDevice.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct GlyphAtlasBinding;

/// **THE TWO AUTHORED TYPE SIZES**, and there are two because `Interface.md` §6 says so: a body size for
/// readouts and a larger one for the build buttons. Both are authored; the atlas rasterizes each at the
/// physical size the INTERFACE fit produces (ADR-011, ADR-016), so a glyph is neither doubled nor
/// resampled.
enum class TextSize : std::uint8_t
{
  /// 20 authored pixels -- 40 physical at the target device's exact 2x. Design names, build item names,
  /// panel labels, link state, percent, overlay sublabels.
  Body = 0,

  /// 32 authored -- 64 physical. The credit balance, build costs, the selection group count, the alert
  /// count, overlay headlines.
  Display = 1,
};

inline constexpr std::size_t TEXT_SIZE_COUNT = 2;

/// Authored heights, from `design_handoff_hud/README.md`'s type table. **Both are even**, which is what
/// makes the exact 2x fit land them on 40 and 64 with nothing left over.
inline constexpr std::int32_t BODY_AUTHORED_PIXELS = 20;
inline constexpr std::int32_t DISPLAY_AUTHORED_PIXELS = 32;

/// Segoe UI **Semibold**, at both sizes. The handoff's reason is worth keeping next to the constant: the
/// interface pass has no antialiasing of its own, and a Regular stem at 40 physical pixels on a
/// single-channel coverage atlas breaks up against a bright silhouette.
inline constexpr std::wstring_view FONT_FAMILY = L"Segoe UI";

/// **THE CLEARTYPE AVERAGE, TAKEN IN LINEAR SPACE** -- ADR-009 names this as the second thing a naive
/// implementation gets wrong, after the fringing that makes averaging necessary at all.
///
/// `CreateAlphaTexture` offers bi-level and ClearType and nothing else, so grayscale coverage is
/// obtained by averaging ClearType's three subpixel values. **Those values are gamma-encoded.** Averaging
/// them where they lie is an average of encoded numbers, which is not the encoding of their average: a
/// stem covering two subpixels of three comes out at 170 of 255 rather than 213, and every stem in the
/// interface is systematically thin. The three are therefore decoded, averaged, and re-encoded.
///
/// **THE TRANSFER FUNCTION IS sRGB's, AND THAT IS A CHOICE THIS FUNCTION TAKES** rather than one ADR-009
/// handed down -- it says "linear space" and stops. sRGB is what the rest of this renderer means by
/// encoded, and a pure 2.2 power would differ by less than one part in 255 over most of the range.
///
/// It is a free function and not a method because it is the one part of this header a suite can reach:
/// everything else needs DirectWrite and a graphics device.
[[nodiscard]] std::uint8_t AverageClearTypeCoverage(std::uint8_t _red, std::uint8_t _green, std::uint8_t _blue) noexcept;

/// **DESIGN UNITS TO PHYSICAL PIXELS**, which is the round trip every advance width in the interface
/// makes and the one place a unit error could live.
///
/// A font states its metrics in design units against an em of `_designUnitsPerEm` -- 2,048 for Segoe UI.
/// An advance in pixels is therefore the design advance scaled by the em size over that. Getting it
/// inverted, or dividing by the authored size rather than the em, gives strings that are individually
/// plausible and cumulatively wrong, which is why it is a named function with a test rather than an
/// expression inside a loop.
///
/// A zero em returns zero rather than dividing by it.
[[nodiscard]] float AdvancePixels(std::uint32_t _designAdvance, std::uint16_t _designUnitsPerEm, float _emSizePixels) noexcept;

/// **DIRECTWRITE RASTERIZES SEGOE UI INTO A TEXTURE WE OWN, AT STARTUP** (ADR-009).
///
/// `DWriteCreateFactory`, `GetSystemFontCollection`, `CreateGlyphRunAnalysis`, then
/// `GetAlphaTextureBounds` and `CreateAlphaTexture` for coverage bytes uploaded by hand.
/// **No Direct2D and no `ID3D11On12Device`** -- R12 bans both by name, which closes the route every
/// D3D12 text sample takes, and is why this class exists rather than forty lines of interop.
///
/// **A MISSING FAMILY IS A STARTUP FAILURE AND NOT A SUBSTITUTION.** R13 requires every layout number to
/// be unconditional, and a substituted font has different advance widths -- so a silent fallback would
/// quietly move every string in the interface on some machine nobody tests on. `Create` returns false.
///
/// **THE ATLAS IS SIZED AGAINST THE WINDOW, SO A RESIZE INVALIDATES IT**, and so does device removal.
/// Both are `Create` again; neither may stall a frame visibly, which is why the character set is the 95
/// printable ASCII and not a cache that grows.
class GlyphAtlas
{
public:
  /// Enough for both sizes of 95 characters with room over. 1024 x 512 at one byte a texel is 512 KiB,
  /// which is a rounding error against the 6.3 MB cubemap this renderer briefly carried.
  static constexpr std::uint32_t ATLAS_WIDTH_PIXELS = 1024;
  static constexpr std::uint32_t ATLAS_HEIGHT_PIXELS = 512;

  /// One texel around every glyph. The sampler is point (a glyph is drawn at exactly the size it was
  /// rasterized, so there is nothing to filter), but a padding of one costs 95 texels a row and removes
  /// a whole class of question about rounding at a slot's edge.
  static constexpr std::uint32_t PADDING_PIXELS = 1;

  GlyphAtlas() noexcept;
  ~GlyphAtlas() noexcept;

  GlyphAtlas(const GlyphAtlas&) = delete;
  GlyphAtlas& operator=(const GlyphAtlas&) = delete;
  GlyphAtlas(GlyphAtlas&&) = delete;
  GlyphAtlas& operator=(GlyphAtlas&&) = delete;

  /// Rasterizes both sizes and uploads them. _interfaceFit is **the interface fit** (ADR-016) -- the
  /// world's would rasterize the type at the world's scale, which at the 1:1 default is half size.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const FitTransform& _interfaceFit) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// The physical height this size was rasterized at, which is the authored height times the fit's
  /// scale. Exposed because it is the number a suite checks the "neither doubled nor resampled" claim
  /// against.
  [[nodiscard]] float EmSizePixels(TextSize _size) const noexcept;

  /// Baseline to baseline, in physical pixels.
  [[nodiscard]] float LineHeightPixels(TextSize _size) const noexcept;

  /// The em size, ascent and descent together, which is what `LayoutText` needs to put a baseline in a
  /// line box the way the handoff's reference does.
  [[nodiscard]] FaceMetrics Face(TextSize _size) const noexcept;

  /// **A BLOCK OF FULL COVERAGE, PACKED FIRST.** A solid rectangle is a glyph quad whose texels are all
  /// this block, which is what lets a panel's plates and its text go out in one instanced draw in the
  /// order they were emitted -- a plate, then the text on it, then an overlay's scrim over both.
  [[nodiscard]] AtlasSlot SolidSlot() const noexcept;

  /// Every character at one size, for `LayoutText`.
  [[nodiscard]] const GlyphTable& Table(TextSize _size) const noexcept;

  /// One character. A character outside the range returns an entry with `present` false and a zero
  /// advance, so a caller that draws it moves the pen nowhere and emits nothing.
  [[nodiscard]] const GlyphEntry& Glyph(TextSize _size, wchar_t _character) const noexcept;

  /// The laid-out width of a string in physical pixels -- the sum of the advances, which is what a
  /// non-wrapping fixed rect is measured against. **No kerning**: `GetDesignGlyphMetrics` gives advances
  /// and this design has no string where a kern pair is visible at 40 pixels.
  [[nodiscard]] float MeasureTextPixels(TextSize _size, std::wstring_view _text) const noexcept;

  [[nodiscard]] std::uint32_t WidthPixels() const noexcept;
  [[nodiscard]] std::uint32_t HeightPixels() const noexcept;

  /// How much of the atlas the packing actually used, for the same reason `AtlasPacker` exposes it.
  [[nodiscard]] std::uint32_t UsedHeightPixels() const noexcept;

  /// The shader-visible heap holding this atlas's view, and the handle into it -- the same arrangement
  /// `SceneTarget` offers `PresentStep`, and for the same reason: the pass binds what the resource's
  /// owner created rather than creating a second view of it.
  [[nodiscard]] void* ShaderResourceHeapUnknown() const noexcept;
  [[nodiscard]] std::uint64_t ShaderResourceHandle() const noexcept;

private:
  std::shared_ptr<GlyphAtlasBinding> m_binding;
};

} // namespace Neuron
